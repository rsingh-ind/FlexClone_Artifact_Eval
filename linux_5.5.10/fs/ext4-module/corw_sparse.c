
#include <linux/pagemap.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/mbcache.h>
#include <linux/quotaops.h>
#include <linux/iversion.h>
#include <linux/backing-dev.h>
#include "ext4_jbd2.h"
#include "ext4.h"
#include "ext4_extents.h"
#include "xattr.h"
#include "acl.h"


#include "corw_sparse.h"

#include <linux/fdtable.h>
#include <linux/rcupdate.h>
#include <linux/delay.h>
#include <linux/kobject.h> 
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/vmalloc.h>

#include <linux/memcontrol.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/writeback.h>
#include <linux/vmstat.h>

static ssize_t sysfs_async_copy_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t sysfs_async_copy_status_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
void scorw_inc_process_usage_count(struct scorw_inode *scorw_inode);
void scorw_dec_process_usage_count(struct scorw_inode *scorw_inode);


LIST_HEAD(scorw_inodes_list);
DEFINE_RWLOCK(scorw_lock);	//read-write spinlock. Protects list of scorw inodes i.e. search, insertion, deletion from scorw inodes list.

LIST_HEAD(page_copy_llist);	//page copy linked list
DEFINE_HASHTABLE(page_copy_hlist, HASH_TABLE_SIZE_BITS_2);	//page copy hash list
DEFINE_RWLOCK(page_copy_lock);  //read-write spinlock. Protects list, hashtable of page copy structs

DECLARE_WAIT_QUEUE_HEAD(page_copy_thread_wq);	//page copy thread wait queue

//Note: This list + lock combo is used here as well as in jbd2 code. Hence, we have defined both these in 
//	fs/inode.c 
extern struct list_head wait_for_commit_list;	//list containing info about frnd inodes associated with child inodes waiting for commit 
extern spinlock_t commit_lock;			//spinlock that protects above list

LIST_HEAD(pending_frnd_version_cnt_inc_list);	//list of frnd inodes waiting for updation of their version count
DEFINE_MUTEX(frnd_version_cnt_lock);		//lock that protects above list

static unsigned long num_files_recovered = 0;

//global vars
int async_copy_on = 1;
int use_follow_on_read = 1;
int exiting = 0;
struct task_struct *scorw_thread = 0;
struct task_struct *page_copy_thread = 0;
struct inode_policy *inode_policy = 0;
struct extent_policy *extent_policy = 0;
struct kobject *scorw_sysfs_kobject = 0;
struct kobj_attribute async_copy_status_attr = __ATTR(async_copy_status, 0660, sysfs_async_copy_status_show, sysfs_async_copy_status_store);
int stop_page_copy_thread = 0;
int par_creation_in_progress = 0;
int page_copy_thread_running = 0;
struct kmem_cache *page_copy_slab_cache = 0;


//Extended attributes names 
//const int CHILD_NAME_LEN = 16;
//const int CHILD_RANGE_LEN = 16;
char *scorw_child = "c_";	//Fill child num using sprintf. Eg: c_0, c_1, ... , c_255
char *scorw_range_start = "r_start_";	//Fill range using sprintf. Eg: r_0, r_1, ... , r_3
char *scorw_range_end = "r_end_";	//Fill range using sprintf. Eg: r_0, r_1, ... , r_3
char *scorw_child_frnd = "SCORW_CHILD";	//attribute maintained in friend file
char *scorw_parent = "SCORW_PARENT";
char *scorw_friend = "SCORW_FRIEND";	
char *copy_size = "COPY_SIZE";
char *blocks_to_copy = "BLOCKS_TO_COPY";
char *num_ranges = "NUM_RANGES";	//How many ranges have been supplied to child?
char *version = "v";


extern wait_queue_head_t sync_child_wait_queue; 
extern void ext4_es_print_tree(struct inode *inode);
extern void ext4_ext_put_gap_in_cache(struct inode *inode, ext4_lblk_t hole_start, ext4_lblk_t hole_len);
extern unsigned long (*get_child_inode_num)(struct inode *);
extern int (*is_par_inode)(struct inode*, int );
extern int (*is_child_inode)(struct inode*, int );
extern unsigned long (*get_child_i_attr_val)(struct inode *, int );
extern unsigned long (*get_friend_attr_val)(struct inode *);
extern int (*is_block_copied)(struct inode *, unsigned );
extern struct page_copy *(*find_page_copy)(unsigned block_num, unsigned long par_ino_num, int child_index);
extern void (*__scorw_exit)(void);
extern int (*is_child_file)(struct inode* inode, int consult_extended_attributes);
extern int (*is_par_file)(struct inode* inode, int consult_extended_attributes);
extern int (*unlink_child_file)(struct inode *inode);
extern int (*unlink_par_file)(struct inode *inode);

//Friend file rebuilding related sysfs attribute and functions
int enable_recovery = 0;
unsigned long long last_recovery_time_us = 0;
struct kobject *kobj_scorw = 0;

//Enable/Disable recovery of frnd file
static ssize_t  sysfs_show_enable_recovery(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t  sysfs_store_enable_recovery(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count);
struct kobj_attribute frnd_file_enable_recovery_attr = __ATTR(enable_recovery, 0660, sysfs_show_enable_recovery, sysfs_store_enable_recovery);

static ssize_t  sysfs_show_enable_recovery(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", enable_recovery);
}

static ssize_t  sysfs_store_enable_recovery(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count)
{
	sscanf(buf, "%d", &enable_recovery);
	enable_recovery = !!enable_recovery;
	return count;
}

//Fetch time taken (in ms) to recover friend file during last open() of any child file
static ssize_t  sysfs_show_last_recovery_time_us(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t  sysfs_store_last_recovery_time_us(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count);
struct kobj_attribute frnd_file_last_recovery_time_us_attr = __ATTR(last_recovery_time_us, 0440, sysfs_show_last_recovery_time_us, sysfs_store_last_recovery_time_us);

static ssize_t  sysfs_show_last_recovery_time_us(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%llu", last_recovery_time_us);
}

static ssize_t  sysfs_store_last_recovery_time_us(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count)
{
	//nothing to be done.
	//(last recovery time can't be overwritten from userspace)
	return count;
}





static ssize_t sysfs_async_copy_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	//printk(" sysfs_async_copy_status_show: async_copy_on: %d\n", async_copy_on);
        return sprintf(buf, "async_copy_on: %d", async_copy_on);
}

void scorw_thread_exit_cleanup(void)
{
	/*
	struct list_head *curr;
	struct scorw_inode *curr_scorw_inode = 0;
	struct scorw_inode *prev_scorw_inode = 0;

	//printk("Inside scorw_thread_exit_cleanup\n");

	
	//printk("scorw_thread_exit_cleanup: acquiring scorw_lock\n");
	write_lock(&scorw_lock);


	list_for_each(curr, &scorw_inodes_list)
	{
		//printk("scorw_thread_exit_cleanup: Inside list_for_each\n");
		curr_scorw_inode = list_entry(curr, struct scorw_inode, i_list);
		//printk("scorw_thread_exit_cleanup: scorw_inode corresponding inode: %lu is child inode? %d\n", curr_scorw_inode->i_ino_num, scorw_is_child_inode(curr_scorw_inode));
		if((prev_scorw_inode) && (scorw_is_child_inode(prev_scorw_inode)))
		{
			write_unlock(&scorw_lock);
			//printk("scorw_thread_exit_cleanup: child scorw inode: %lu found!! Calling scorw_put_inode on it\n", prev_scorw_inode->i_ino_num);
			mutex_lock(&(prev_scorw_inode->i_lock));
			
			
			//if(prev_scorw_inode->i_pending_copy_pages != 0)
			//{
			//	printk("scorw_thread_exit_cleanup: inode num: %lu, inode ref count value: %u (Before iput)\n", prev_scorw_inode->i_ino_num, prev_scorw_inode->i_vfs_inode->i_count);
			//	iput(prev_scorw_inode->i_vfs_inode);
			//	printk("scorw_thread_exit_cleanup: inode num: %lu, inode ref count value: %u (After iput)\n", prev_scorw_inode->i_ino_num, prev_scorw_inode->i_vfs_inode->i_count);
			//}
			
			mutex_unlock(&(prev_scorw_inode->i_lock));
			//printk("scorw_thread_exit_cleanup: inode num: %lu, inode ref count value: %u (Before scorw_put_inode)\n", prev_scorw_inode->i_ino_num, prev_scorw_inode->i_vfs_inode->i_count);
			scorw_put_inode(prev_scorw_inode->i_vfs_inode, 1, 1);
			//printk("scorw_thread_exit_cleanup: inode num: %lu, inode ref count value: %u (After scorw_put_inode)\n", prev_scorw_inode->i_ino_num, prev_scorw_inode->i_vfs_inode->i_count);
			write_lock(&scorw_lock);
		}
		prev_scorw_inode = curr_scorw_inode;
	}

	if((prev_scorw_inode) && (scorw_is_child_inode(prev_scorw_inode)))
	{
		write_unlock(&scorw_lock);
		//printk("scorw_thread_exit_cleanup: child scorw inode: %lu found!!. Calling scorw_put_inode on it.\n", prev_scorw_inode->i_ino_num);
		mutex_lock(&(prev_scorw_inode->i_lock));
		//if(prev_scorw_inode->i_pending_copy_pages != 0)
		//{
		//	printk("scorw_thread_exit_cleanup: inode num: %lu, inode ref count value: %u (Before iput)\n", prev_scorw_inode->i_ino_num, prev_scorw_inode->i_vfs_inode->i_count);
		//	iput(prev_scorw_inode->i_vfs_inode);
		//	printk("scorw_thread_exit_cleanup: inode num: %lu, inode ref count value: %u (Before iput)\n", prev_scorw_inode->i_ino_num, prev_scorw_inode->i_vfs_inode->i_count);
		//}
		
		mutex_unlock(&(prev_scorw_inode->i_lock));
		//printk("scorw_thread_exit_cleanup: inode num: %lu, inode ref count value: %u (Before scorw_put_inode)\n", prev_scorw_inode->i_ino_num, prev_scorw_inode->i_vfs_inode->i_count);
		scorw_put_inode(prev_scorw_inode->i_vfs_inode, 1, 1);
		//printk("scorw_thread_exit_cleanup: inode num: %lu, inode ref count value: %u (After scorw_put_inode)\n", prev_scorw_inode->i_ino_num, prev_scorw_inode->i_vfs_inode->i_count);
		write_lock(&scorw_lock);
	}
	


	write_unlock(&scorw_lock);
	//printk("scorw_thread_exit_cleanup: released scorw_lock\n");
	*/
}

static ssize_t sysfs_async_copy_status_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) 
{
	int new_async_copy_on_val;
        sscanf(buf, "%d", &new_async_copy_on_val);
	
	//start thread
	if((async_copy_on == 0) && (new_async_copy_on_val))
	{
		scorw_thread_init();
	}
	//terminate thread
	if((async_copy_on) && (new_async_copy_on_val == 0))
	{
		scorw_thread_exit();
	}

	async_copy_on = new_async_copy_on_val;
	//printk("sysfs_async_copy_status_store: async_copy_on: %d\n", async_copy_on);
        return count;
}

static void scorw_es_print_tree(struct inode *inode)
{
	struct ext4_es_tree *tree;
	struct rb_node *node;

	printk(KERN_DEBUG "status extents for inode %lu:", inode->i_ino);
	tree = &EXT4_I(inode)->i_es_tree;
	node = rb_first(&tree->root);
	while (node) {
		struct extent_status *es;
		es = rb_entry(node, struct extent_status, rb_node);
		printk(KERN_DEBUG " [%u/%u) %llu %x",
		       es->es_lblk, es->es_len,
		       ext4_es_pblock(es), ext4_es_status(es));
		node = rb_next(node);
	}
	printk(KERN_DEBUG "\n");
}


//list of scorw inodes present
//
struct scorw_inode *scorw_inode_list(void)
{
	/*
	struct list_head *curr;
	struct scorw_inode *curr_scorw_inode;

	
	//printk("Inside scorw_inode_list\n");
	read_lock(&scorw_lock);
	list_for_each(curr, &scorw_inodes_list)
	{
		curr_scorw_inode = list_entry(curr, struct scorw_inode, i_list);
		//printk("scorw_inode num: %lu exists\n", curr_scorw_inode->i_ino_num);
		//printk("scorw_inode, i_process_usage_count: %lu\n", atomic64_read(&(curr_scorw_inode->i_process_usage_count)));
		//printk("scorw_inode, i_thread_usage_count: %lu\n", atomic64_read(&(curr_scorw_inode->i_thread_usage_count)));
		//printk("scorw_inode, i_pending_copy_pages: %u\n", curr_scorw_inode->i_pending_copy_pages);
	}
	read_unlock(&scorw_lock);
	//printk("Returning from scorw_inode_list\n");
	*/
	return NULL; 
}


//clean scorw inodes present
//
struct scorw_inode *scorw_clean_inode_list(void)
{
	struct list_head *curr;
	struct list_head *temp;
	struct scorw_inode *curr_scorw_inode;

	//printk("Inside scorw_clean_inode_list\n");
	list_for_each_safe(curr, temp, &scorw_inodes_list)
	{
		curr_scorw_inode = list_entry(curr, struct scorw_inode, i_list);
		//printk("scorw_clean_inode num: %lu exists\n", curr_scorw_inode->i_ino_num);
		scorw_put_inode(curr_scorw_inode->i_vfs_inode, 1, 0, 0);
	}
	//printk("Returning from scorw_clean_inode_list\n");
	return NULL; 
}



//given an inode number, search scorw inode corresponding the inode number in scorw inodes list
//
struct scorw_inode *scorw_search_inode_list(unsigned long ino_num)
{
	struct list_head *curr;
	struct scorw_inode *curr_scorw_inode;

	//printk("Inside scorw_search_inode_list\n");

	list_for_each(curr, &scorw_inodes_list)
	{
		curr_scorw_inode = list_entry(curr, struct scorw_inode, i_list);
		//printk("scorw_search_inode_list: Comparing %lu and %lu. Looking for scorw inode corresponding inode: %lu\n", curr_scorw_inode->i_ino_num, ino_num, ino_num);
		if(curr_scorw_inode->i_ino_num == ino_num)
		{
			//printk("scorw_search_inode_list: Matching scorw inode found. Returning.\n");
			return curr_scorw_inode;
		}
	}
	//printk("scorw_search_inode_list: No Matching scorw inode found\n");
	return NULL; 
}




//Given a vfs inode, find scorw inode corresponding this vfs inode
//If scorw inode doesn't exist, it isn't created instead
//null is returned.
//
//should be called by those processes that have incremented usage count of this inode.
//otherwise bug because returned inode might be freed by someone else
struct scorw_inode* scorw_find_inode(struct inode *inode)
{
	//struct scorw_inode *scorw_inode = NULL;

	//printk("Inside scorw_find_inode\n");

	//printk("scorw_find_inode: searching scorw_lock corresponding inode: %lu\n", inode->i_ino);
	//read_lock(&scorw_lock);
	//scorw_inode = scorw_search_inode_list(inode->i_ino);
	//read_unlock(&scorw_lock);

	return inode->i_scorw_inode;
}


//tells whether given scorw inode belongs to a child file or a parent file.
//returns 1 if it belongs to child file else returns 0
int scorw_is_child_inode(struct scorw_inode *scorw_inode)
{
	//printk("Inside scorw_is_child_inode\n");
	if(scorw_inode->i_par_vfs_inode != NULL)
	{
		return 1;
	}
	return 0;
}


//checks whether a file with specified inode is a child file or not.
//If file is a child file, return 1 
//else return 0
int scorw_is_child_file(struct inode* inode, int consult_extended_attributes)
{
	unsigned long p_ino_num;	
	struct scorw_inode* scorw_inode;
	int is_child_inode;

	//printk("Inside scorw_is_child_file\n");

	//check in memory list of scorw inodes
	//printk("scorw_is_child_file: checking in memory list of scorw inode\n");
	scorw_inode = scorw_find_inode(inode);
	//printk("scorw_is_child_file: scorw_inode: %p\n", scorw_inode);
	if(scorw_inode != NULL)
	{
		is_child_inode = scorw_is_child_inode(scorw_inode);
		//printk("scorw_is_child_file: is_child_inode: %d\n", is_child_inode);
		return (is_child_inode != 0);
	}

	//printk("scorw_is_child_file: consult_extended_attributes: %d\n", consult_extended_attributes);
	if(!consult_extended_attributes)
	{
		return 0;
	}

	//check extended attribute
	//printk("scorw_is_child_file: checking in extended attribute\n");
	
	p_ino_num = scorw_get_parent_attr_val(inode);
	//printk("scorw_is_child_file: is child file? : %lu. Returning from this function\n", p_ino_num);
	if(p_ino_num > 0)
	{
		return 1;
	}
	return 0;	//not a child file
}

//checks whether a file with specified inode is a parent file or not.
//If file is a parent file, return 1 
//else return 0
int scorw_is_par_file(struct inode* inode, int consult_extended_attributes)
{
	int i;
	unsigned long c_ino_num;
	struct scorw_inode* scorw_inode;
	int is_child_inode;

	//printk("Inside scorw_is_par_file\n");
	

/*
 * Note: scorw_find_inode acquires a global mutex lock that protects list of scorw inodes.
 * 	This can lead to recursive lock bug when scorw_get_inode is called (scorw_get_inode also acquires this global mutex lock)
 * 	scorw_get_inode creates zero pages for friend file. While creating zero pages balance_dirty_pages is called.
 * 	Inside this balance_dirty_pages fn, we call scorw_is_par_file (which calls scorw_find_inode) resulting in attempt to recursively 
 * 	acquire the mutex lock.
 *
 * 	Even if we consult extended attributes to conclude whether an inode is a parent inode or not, overhead should not be much because
 * 	page containing extended attributes will be cached in memory.
 *
 * 	Update: 
 * 		1) consulting extended attributes drops performance significantly. Ran a fio on baseline file s.t. it consulted extended attributes to confirm whether a file
 * 			is a parent file or not. Throughput decreased from (~1400MBps when scorw_is_par_file concludes that a file is a parent file or not without consulting 
 * 			extended attributes vs ~450MBps when scorw_is_par_file consulted extended attributes.
 * 		2) Have changed the position inside balance_dirty_pages, from where scorw_is_par_file is called. I think, it won't be called now from the scorw_get_inode ---> frnd file zero pages
 * 			path. So, we can use scorw_find_inode here without any problem.
 *
 *
 */

	//check in memory list of scorw inodes
	//printk("scorw_is_par_file: checking in memory list of scorw inode\n");
	scorw_inode = scorw_find_inode(inode);
	//printk("scorw_is_par_file: scorw_inode: %p\n", scorw_inode);
	if(scorw_inode != NULL)
	{
		is_child_inode = scorw_is_child_inode(scorw_inode);
		//printk("scorw_is_par_file: is_child_inode: %d\n", is_child_inode);
		return (is_child_inode == 0);
	}

	//printk("scorw_is_par_file: consult_extended_attributes: %d\n", consult_extended_attributes);
	if(!consult_extended_attributes)
	{
		return 0;
	}


	//check extended attributes
	for(i = 0; i < SCORW_MAX_CHILDS; i++)
	{
		//printk("scorw_is_par_file: Inside for loop\n");
		c_ino_num = scorw_get_child_i_attr_val(inode, i);
		//printk("Inside c_ino_num of child %d: %llu\n", i, c_ino_num);

		if(c_ino_num)
		{
			return 1;
		}
	}
	return 0;
}


//given child attribute number, returns its value if present 
//else returns 0;
unsigned long scorw_get_child_i_attr_val(struct inode *inode, int child_i)
{
	int buf_size = 0;
	unsigned long c_ino_num;
        char scorw_child_name[CHILD_NAME_LEN];

	//printk("Inside scorw_get_child_i_attr_val\n");
	
	if(child_i >= SCORW_MAX_CHILDS)
	{
		printk("Error: scorw_get_child_i_attr_val: Only %d children supported!\n", SCORW_MAX_CHILDS);
		return 0;
	}
	
	memset(scorw_child_name, '\0', CHILD_NAME_LEN);
	sprintf(scorw_child_name, "%s%d", scorw_child, child_i);

	buf_size = ext4_xattr_get(inode, 1, scorw_child_name, &c_ino_num, sizeof(unsigned long));
	if(buf_size > 0)
	{
		return c_ino_num;
	}
	return 0;
}

//given inode, returns its parents inode number (if it exists) 
//returns 0 otherwise
unsigned long scorw_get_parent_attr_val(struct inode *inode)
{
	unsigned long p_ino_num;
	int buf_size;

	//printk("Inside scorw_get_parent_attr_val\n");
	buf_size = ext4_xattr_get(inode, 1, scorw_parent , &p_ino_num, sizeof(unsigned long));
	if(buf_size > 0)
	{
		//printk("Returning from scorw_get_parent_attr_val\n");
		return p_ino_num;
	}
	//printk("Returning from scorw_get_parent_attr_val\n");
	return 0;
}

//given inode, returns its friends inode number (if it exists)
//returns 0 otherwise
unsigned long scorw_get_friend_attr_val(struct inode *inode)
{
	unsigned long f_ino_num;
	int buf_size;

	//printk("Inside scorw_get_friend_attr_val\n");
	buf_size = ext4_xattr_get(inode, 1, scorw_friend , &f_ino_num, sizeof(unsigned long));
	if(buf_size > 0)
	{
		//printk("Returning from scorw_get_friend_attr_val\n");
		return f_ino_num;
	}
	//printk("Returning from scorw_get_friend_attr_val\n");
	return 0;
}


//given inode of a friend, returns inode number of the child file corresponding it(if it exists)
//returns 0 otherwise
unsigned long scorw_get_child_friend_attr_val(struct inode *inode)
{
	unsigned long f_ino_num;
	int buf_size;

	//printk("Inside scorw_get_child_friend_attr_val\n");
	buf_size = ext4_xattr_get(inode, 1, scorw_child_frnd, &f_ino_num, sizeof(unsigned long));
	if(buf_size > 0)
	{
		return f_ino_num;
	}
	return 0;
}

//given child inode, returns its version number 
//returns 0 otherwise
unsigned long scorw_get_child_version_attr_val(struct inode *inode)
{
	unsigned long version_val;
	int buf_size;

	//printk("Inside scorw_get_child_version_attr_val\n");
	buf_size = ext4_xattr_get(inode, 1, version, &version_val, sizeof(unsigned long));
	if(buf_size > 0)
	{
		return version_val;
	}
	return 0;
}

//given child inode, update its version number 
void scorw_set_child_version_attr_val(struct inode *inode, unsigned long version_val)
{
	//printk("Inside scorw_set_child_version_attr_val\n");
	ext4_xattr_set(inode, 1, version, &version_val, sizeof(unsigned long), 0);

}

//given frnd inode, returns its version number 
//returns 0 otherwise
unsigned long scorw_get_frnd_version_attr_val(struct inode *inode)
{
	unsigned long version_val;
	int buf_size;

	//printk("Inside scorw_get_frnd_version_attr_val\n");
	buf_size = ext4_xattr_get(inode, 1, version, &version_val, sizeof(unsigned long));
	if(buf_size > 0)
	{
		return version_val;
	}
	return 0;
}

//given frnd inode, update its version number 
void scorw_set_frnd_version_attr_val(struct inode *inode, unsigned long version_val)
{
	//printk("Inside scorw_set_frnd_version_attr_val\n");
	ext4_xattr_set(inode, 1, version, &version_val, sizeof(unsigned long), 0);

}


//given vfs inode, returns size of parent at the time of creating child file.
//This much data is the total data that needs to be copied from parent to child.
long long scorw_get_copy_size_attr_val(struct inode *inode)
{
	long long c_size;
	int buf_size;

	//printk("Inside scorw_get_copy_size_attr_val\n");
	buf_size = ext4_xattr_get(inode, 1, copy_size, &c_size, sizeof(long long));
	if(buf_size > 0)
	{
		return c_size;
	}
	return 0;
}

//given vfs inode, returns count of blocks yet to be copied from parent to child.
unsigned scorw_get_blocks_to_copy_attr_val(struct inode *inode)
{
	unsigned blocks_count;
	int buf_size;

	//printk("Inside scorw_get_blocks_to_copy_attr_val\n");
	buf_size = ext4_xattr_get(inode, 1, blocks_to_copy, &blocks_count, sizeof(unsigned));
	//printk("scorw_get_blocks_to_copy_attr_val: value: %llu\n", blocks_count);
	if(buf_size > 0)
	{
		return blocks_count;
	}
	return 0;
}

//given child vfs inode, returns number of ranges supplied to child
int scorw_get_num_ranges_attr_val(struct inode *inode)
{
	int n_ranges;
	int buf_size;

	//printk("Inside scorw_get_num_ranges_attr_val\n");
	buf_size = ext4_xattr_get(inode, 1, num_ranges, &n_ranges, sizeof(int));
	if(buf_size > 0)
	{
		return n_ranges;
	}
	return 0;
}

//given range attribute number, returns its start value if present 
//else returns 0;
unsigned scorw_get_range_i_start_attr_val(struct inode *inode, int index)
{
	int buf_size = 0;
	unsigned range_i_start = 0;
        char range_start[CHILD_RANGE_LEN];

	//printk("Inside scorw_get_range_i_start_attr_val\n");
	if(index >= MAX_RANGES_SUPPORTED)
	{
		printk("Error: scorw_get_range_i_start_attr_val: Only %d ranges supported! \n", MAX_RANGES_SUPPORTED);
		return 0;
	}
	
	memset(range_start, '\0', CHILD_RANGE_LEN);
	sprintf(range_start, "%s%d", scorw_range_start, index);

	buf_size = ext4_xattr_get(inode, 1, range_start, &range_i_start, sizeof(unsigned));
	if(buf_size > 0)
	{
		return range_i_start;
	}
	return 0;
}


#ifdef USE_OLD_RANGE
//given range attribute number, returns its end value if present 
//else returns 0;
unsigned scorw_get_range_i_end_attr_val(struct inode *inode, int index)
{
	int buf_size = 0;
	unsigned range_i_end = 0;
        char range_end[CHILD_RANGE_LEN];

	//printk(" Inside scorw_get_range_i_end_attr_val\n");
	if(index >= MAX_RANGES_SUPPORTED)
	{
		printk("Error: scorw_get_range_i_end_attr_val: Only %d ranges supported!\n", MAX_RANGES_SUPPORTED);
		return 0;
	}
	
	memset(range_end, '\0', CHILD_RANGE_LEN);
	sprintf(range_end, "%s%d", scorw_range_end, index);

	buf_size = ext4_xattr_get(inode, 1, range_end, &range_i_end, sizeof(unsigned));
	if(buf_size > 0)
	{
		return range_i_end;
	}
	return 0;
}
#else
int scorw_get_range_attr_val(struct inode *inode, int index, struct child_range_xattr *crx)
{
        char range_start[CHILD_RANGE_LEN];
        int retval = 0;
	//printk("Inside scorw_get_range_i_start_attr_val\n");
	if(index >= MAX_RANGES_SUPPORTED)
	{
		printk("Error: scorw_get_range_i_start_attr_val: Only %d ranges supported! \n", MAX_RANGES_SUPPORTED);
		goto ret;
	}
	
	memset(range_start, '\0', CHILD_RANGE_LEN);
	sprintf(range_start, "%s%d", scorw_range_start, index);

	retval = ext4_xattr_get(inode, 1, range_start, crx, sizeof(struct child_range_xattr));
ret:	
	return retval;
}
#endif

void scorw_set_blocks_to_copy_attr_val(struct inode *inode, unsigned blocks_count)
{
	//printk("Inside scorw_set_blocks_to_copy_attr_val\n");
	//printk("scorw_set_blocks_to_copy_attr_val: new value: %llu\n", blocks_count);
	ext4_xattr_set(inode, 1, blocks_to_copy, &blocks_count, sizeof(unsigned), 0);

}


void scorw_remove_child_i_attr(struct inode *inode, int child_i)
{
        char scorw_child_name[CHILD_NAME_LEN];

	//printk("scorw_remove_child_i_attr called\n");

	if(child_i >= SCORW_MAX_CHILDS)
	{
		printk("Error: scorw_remove_child_i_attr: Only %d children supported!\n", SCORW_MAX_CHILDS);
	}
	
	memset(scorw_child_name, '\0', CHILD_NAME_LEN);
	sprintf(scorw_child_name, "%s%d", scorw_child, child_i);

	ext4_xattr_set(inode, 1, scorw_child_name, NULL, 0, 0);
}

//Given a parent vfs inode and inode number, remove attribute of vfs inode which 
//has the given inode number as its value.
//
//Arguments:
//p_inode : vfs inode of parent file and whose attribute needs to be removed
//c_ino_num: value of attribute that needs to be removed
//
//returns:
//0 on success
//-1 on failure
int scorw_remove_child_attr(struct inode *p_inode, unsigned long c_ino_num)
{
	int i;
	unsigned long inode_num;
	//printk("Inside scorw_remove_child_attr\n");
	//printk("scorw_remove_child_attr: p_inode->i_ino: %lu\n", p_inode->i_ino);
	//printk("scorw_remove_child_attr: c_ino_num: %u\n", c_ino_num);

	for(i = 0; i < SCORW_MAX_CHILDS; i++)
	{
		//printk("scorw_remove_child_attr: Inside for loop\n");
		inode_num = scorw_get_child_i_attr_val(p_inode, i);
		//printk("scorw_remove_child_attr: child %d's val: %u, looking for val: %u \n", i, inode_num, c_ino_num);

		if(inode_num == c_ino_num)
		{
			scorw_remove_child_i_attr(p_inode, i);
			return 0;
		}
	}
	return -1;
}

//given inode of child, remove all attributes related to parent and copying kept by child.
void scorw_remove_par_attr(struct inode *inode)
{

	//printk("scorw_remove_par_attr called for inode->i_ino: %lu\n", inode->i_ino);
	ext4_xattr_set(inode, 1, scorw_parent, NULL, 0, 0);
	ext4_xattr_set(inode, 1, scorw_friend, NULL, 0, 0);
	ext4_xattr_set(inode, 1, copy_size, NULL, 0, 0);
	ext4_xattr_set(inode, 1, blocks_to_copy, NULL, 0, 0);
}


//given inode of friend, remove all attributes related to child kept by friend.
void scorw_remove_child_friend_attr(struct inode *inode)
{

	//printk("scorw_remove_child_friend_attr called for inode->i_ino: %lu\n", inode->i_ino);
	ext4_xattr_set(inode, 1, scorw_child_frnd, NULL, 0, 0);
}



//find info from extent status tree
//currently only info about delayed extents is found from es tree because such info hasn't been
//yet saved to extent tree
//
//Takes as input scorw inode, logical block corresponding which extent is to be found
//flag that tells whether extent of inode corresponding passed scorw inode 
//or parent inodes extent is to be found.
//
//fills ex with extent info it if exists corresponding given blk number and returns 1
//else, returns 0. 
int scorw_find_extent_from_es_tree(struct scorw_inode *scorw_inode, loff_t lblk, int find_extent_of_itself, struct scorw_extent *ex)
{
	struct extent_status es;		//extent status
	int found = 0;


	//printk("Inside scorw_find_extent_from_es_tree\n");
	if(find_extent_of_itself == 1)
	{
		//ext4_es_print_tree(scorw_inode->i_vfs_inode);
		found = ext4_es_lookup_extent(scorw_inode->i_vfs_inode, lblk, NULL, &es);
	}
	else if(find_extent_of_itself == 0)
	{
		//if(scorw_inode->i_par_vfs_inode)
			//ext4_es_print_tree(scorw_inode->i_par_vfs_inode);
		found = ext4_es_lookup_extent(scorw_inode->i_par_vfs_inode, lblk, NULL, &es);
	}
	else if(find_extent_of_itself == 2)
	{
		found = ext4_es_lookup_extent(scorw_inode->i_frnd_vfs_inode, lblk, NULL, &es);
	}

	if(!found)
	{
	//	printk("scorw_find_extent_from_es_tree: extent not found\n");
		return 0;
	}
	//printk("scorw_find_extent_from_es_tree: extent found\n");

	if((ext4_es_is_written(&es) ||  ext4_es_is_unwritten(&es)  || ext4_es_is_delayed(&es)) && ((lblk >= es.es_lblk) && (lblk < (es.es_lblk + es.es_len))))
	{
		//printk("scorw_find_extent_from_es_tree: es.es_lblk: %u, es.es_len: %u\n", es.es_lblk, es.es_len);
		ex->ee_block = es.es_lblk;
		ex->ee_len = es.es_len;
		return 1;
	}
	return 0;
}


//find info from extent tree
//
//Takes as input scorw inode, logical block corresponding which extent is to be found
//flag that tells whether extent of inode corresponding passed scorw inode 
//or parent inodes extent is to be found.
//
//fills ex with extent info it if exists corresponding given blk number and returns 1
//else, returns 0. 
int scorw_find_extent_from_extent_tree(struct scorw_inode *scorw_inode, loff_t lblk, int find_extent_of_itself, struct scorw_extent *ex)
{
	struct ext4_ext_path *path = NULL;	//path to extent from root
	int depth = 0;				//depth of extent
	struct ext4_extent *ex_from_extent_tree = NULL;		//extent prepared from info avialable in extent tree.

	//printk("Inside scorw_find_extent_from_extent_tree\n");

	//find info from extent tree
	if(find_extent_of_itself == 1)
	{
		path = ext4_find_extent(scorw_inode->i_vfs_inode, lblk, NULL, 0);	
		depth = ext_depth(scorw_inode->i_vfs_inode);
	}
	else if(find_extent_of_itself == 0)
	{
		path = ext4_find_extent(scorw_inode->i_par_vfs_inode, lblk, NULL, 0);	
		depth = ext_depth(scorw_inode->i_par_vfs_inode);
	}
	else if(find_extent_of_itself == 2)
	{
		path = ext4_find_extent(scorw_inode->i_frnd_vfs_inode, lblk, NULL, 0);	
		depth = ext_depth(scorw_inode->i_frnd_vfs_inode);
	}
	ex_from_extent_tree = path[depth].p_ext;


	//printk("scorw_find_extent_from_extent_tree: block lblk: %llu, depth: %d\n", lblk, depth);

	if(ex_from_extent_tree)
	{
		//printk("scorw_find_extent_from_extent_tree: ext4_extent ex->ee_block: %u\n", ex_from_extent_tree->ee_block);	
		//printk("scorw_find_extent_from_extent_tree: ext4_extent ex->ee_len: %d\n", ex_from_extent_tree->ee_len);	
		if((lblk >= ex_from_extent_tree->ee_block) && (lblk < (ex_from_extent_tree->ee_block + ex_from_extent_tree->ee_len)))
		{
			//printk("scorw_find_extent_from_extent_tree: DATA BLOCK extent found\n");
			ex->ee_block = ex_from_extent_tree->ee_block;
			ex->ee_len = ex_from_extent_tree->ee_len;

			//release memory occupied by path
			ext4_ext_drop_refs(path);
			kfree(path);

			return 1;
		}
		else
		{
			//printk("scorw_find_extent_from_extent_tree: HOLE. extent not found.\n");
		}
	}

	//release memory occupied by path
	ext4_ext_drop_refs(path);
	kfree(path);

	return 0;
}


//Takes as input scorw inode, logical block corresponding which extent is to be found
//flag that tells whether extent of inode corresponding passed scorw inode 
//or parent inodes extent is to be found.
//
//Returns extent if it exists corresponding given blk number.
//else returns null
//
//It is the responsibility of caller to free the memory occupied by returned extent (if not null).
//
struct scorw_extent* scorw_find_extent(struct scorw_inode *scorw_inode, loff_t lblk, int find_extent_of_itself)
{ 
	struct scorw_extent *ex;		//extent prepared from info avialable in extent tree.
	ex = kzalloc(sizeof(struct scorw_extent), GFP_KERNEL);

	//printk("Inside scorw_find_extent\n");
	//printk("scorw_find_extent: scorw_inode->i_ino_num: %lu\n", scorw_inode->i_ino_num);
	//printk("scorw_find_extent: lblk: %lld\n", lblk);
	//printk("scorw_find_extent: find_extent_of_itself: %d\n", find_extent_of_itself);
	
/*
	if(scorw_find_extent_from_es_tree(scorw_inode, lblk, find_extent_of_itself, ex))
	{
	//	printk("scorw_find_extent: Returning extent from es tree\n");
		return ex;
	}
*/

	//Observed an anomaly: 32 KB parent file. Writing to 4 blocks of parent. extent of size 4 (start block: 0, len: 4) getting created in es tree.
	//upon calling, scorw_find_extent() for block num: 4, lookup in es tree fails. Then, lookup happens in extent tree. extent tree returns extent of size 8 (start block: 0, len: 8)
	//Thus, now there are two entries in list of extents to be processed by a child: 0/4 and 0/8 (notation: start block/length).
	//We can see that blocks 0 to 3 will be processed again by scorw thread unnecessarily.
	//So, considering to check extent tree only for extent lookups now.
	//It will need few disk accesses when child scorw inode is created. However, other children of same parent will find the extent tree of parent cached in page cache.
	if(scorw_find_extent_from_extent_tree(scorw_inode, lblk, find_extent_of_itself, ex))
	{
	//	printk("scorw_find_extent: Returning extent from extent tree\n");
		return ex;
	}

	kfree(ex);
	//printk("scorw_find_extent: Returning NULL\n");
	return NULL;
}


int scorw_get_block_copied_bit(struct scorw_inode *scorw_inode, loff_t lblk)
{
	
	int found;
	struct extent_status es;
        struct scorw_extent *ex = NULL;          //extent
	int bit;


	//printk("scorw_get_block_copied_bit: Start \n");
	found = ext4_es_lookup_extent(scorw_inode->i_vfs_inode, lblk, 0, &es);
	if(!found)
	{
		//printk("scorw_get_block_copied_bit: extent corresponding block: %llu not found in es tree. Need to search in extent tree.\n", lblk);

		//search in extent tree
		ex = scorw_find_extent(scorw_inode, lblk, 1);
		if(ex)
		{
			//printk("scorw_get_block_copied_bit: ext4_extent ex->ee_block: %u\n", ex->ee_block);
			//printk("scorw_get_block_copied_bit: ext4_extent ex->ee_len: %d\n", ex->ee_len);
			bit = 1;

			//It is callers responsibility to free the memory occupied by this extent returned by scorw_find_extent
			kfree(ex);
		}
		else
		{
			//printk("scorw_get_block_copied_bit: Extent doesn't exists. HOLE\n");
			//ext4_ext_put_gap_in_cache(scorw_inode->i_vfs_inode, lblk, 1);
			bit = 0;
		}
	}
	else
	{
		//printk("scorw_get_block_copied_bit: extent corresponding block: %llu found\n", lblk);
		//printk("scorw_get_block_copied_bit: es.es_lblk: %u\n", es.es_lblk);
		//printk("scorw_get_block_copied_bit: es.es_len: %u\n", es.es_len);
		//printk("scorw_get_block_copied_bit: es.es_pblk: %llu\n", es.es_pblk);

		if(ext4_es_is_hole(&es))
		{
			//printk("scorw_get_block_copied_bit: Hole i.e. block is not copied.\n");
			bit = 0;
		}
		else
		{
			//printk("scorw_get_block_copied_bit: Data i.e. block is already copied.\n");
			bit = 1;
		}
	}
	return bit;
}





//assumes lock on scorw_inode is held by caller
//decrements count of blocks yet to be copied by n.
void scorw_dec_yet_to_copy_blocks_count(struct scorw_inode* scorw_inode, unsigned n)
{
	//scorw_inode->i_pending_copy_pages -= n;
	atomic64_sub(n, &(scorw_inode->i_pending_copy_pages)); 
	//printk("scorw_dec_yet_to_copy_blocks_count: After: scorw_inode->i_pending_copy_pages: %u\n", scorw_inode->i_pending_copy_pages);
}



//args:
//inode: vfs inode corresponding friend file
//lblk: block of friend file to be zeroed
int scorw_create_zero_page(struct inode *inode, unsigned lblk)
{
        struct address_space *mapping = NULL;
        struct page *page_w = NULL;
        void *fsdata = 0;
        int error = 0;
	void *kaddr_w; 
	int i;

	if(get_child_inode_num)
	{
		down_read(&(inode->i_scorw_rwsem));
	}
	mapping = inode->i_mapping;
	//printk("scorw_copy_zero_page: Inside scorw_copy_zero_page, lblk: %u\n", lblk);
	
	error = ext4_da_write_begin(NULL, mapping, ((unsigned long)lblk) << PAGE_SHIFT, PAGE_SIZE, 0, &page_w, &fsdata);
	//printk("################# Page allocated but not mapped. page_w->_refcount: %d, page_w->_mapcount: %d\n",  page_ref_count(page_w), atomic_read(&page_w->_mapcount));
	if(page_w != NULL)
	{
		kaddr_w = kmap_atomic(page_w);
		//printk("################# Page allocated and mapped. page_w->_refcount: %d, page_w->_mapcount: %d\n", page_ref_count(page_w),atomic_read(&page_w->_mapcount));

		for(i = 0; i < PAGE_SIZE; i++)
		{
			*((char*)kaddr_w + i) = '\0';
		}
		kunmap_atomic(kaddr_w);
		//printk("################# Page allocated and unmapped mapped. page_w->_refcount: %d, page_w->_mapcount: %d\n",  page_ref_count(page_w),atomic_read(&page_w->_mapcount));

		//ext4_es_print_tree(scorw_inode->i_vfs_inode);
		ext4_da_write_end(NULL, mapping , ((unsigned long)lblk) << PAGE_SHIFT, PAGE_SIZE, PAGE_SIZE, page_w, fsdata);
		//printk("1. scorw_copy_zero_page: size of frnd file as per its inode: %lu\n", inode->i_size);

		//There are only few pages in friend file. So, we can skip ratelimiting it.
		//This is done to avoid acquiring scorw_lock recursively. Read comment inside scorw_is_par_file() to know more.
		//balance_dirty_pages_ratelimited(mapping);
		//printk("2. scorw_copy_zero_page: size of frnd file as per its inode: %lu\n", inode->i_size);

		if(get_child_inode_num)
		{
			up_read(&(inode->i_scorw_rwsem));
		}

	}
	else
	{
		printk("scorw_create_zero_page: Error: %d in ext4_da_write_begin\n", error);
		if(get_child_inode_num)
		{
			up_read(&(inode->i_scorw_rwsem));
		}

		return error;
	}
	return 0;
}




//get page corresponding provided page index in given inode's page cache.
struct page* scorw_get_page(struct inode* inode, loff_t lblk)
{
        struct address_space *mapping = NULL;
        struct page *page = NULL;
	//void *kaddr = NULL;
	int error = 0;

	mapping = inode->i_mapping;

	/*Fast approach (avoids locking of page)*/
	page = find_get_page(mapping, lblk);
	if (page && PageUptodate(page))
	{
		return page;
	}

	/*Fall back approach */

	//returns a locked page
	page = find_or_create_page(mapping, lblk, mapping_gfp_mask(mapping));
	if(page == NULL)
	{
		//current->backing_dev_info = NULL;
		printk(KERN_ERR "Error!! Failed to allocate page. Out of memory!!\n");
		return NULL;
	}

	//If parent's page is already in cache, don't read it from disk
	if(PageUptodate(page))
	{
		unlock_page(page);
	}
	else
	{
		//parent's page is not in cache, read it from disk
		//calling ext4_readpage. ext4_readpage calls ext4_mpage_readpages internally.
		error = mapping->a_ops->readpage(NULL, page);
		if(error)
		{
			printk(KERN_ERR "scorw_get_page: Error while Reading parent's page into pagecache\n");
			put_page(page);
			//current->backing_dev_info = NULL;
			//inode_unlock(scorw_inode->i_vfs_inode);
			return NULL;
		}      

		if(!PageUptodate(page))
		{
			lock_page_killable(page);
			unlock_page(page);
		}
	}
	return page;
}

void scorw_put_page(struct page* page)
{
	put_page(page);
}

//Args:
//vfs inode of frnd file
//block num 
int scorw_is_block_copied(struct inode *inode, unsigned blk_num)
{
	struct page* page = 0;
	char* kaddr = 0;
	int byte_num= 0;
	int bit_num = 0;
	char byte_value= 0;
	int copy_state = 0;

	//printk("scorw_is_block_copied called\n");
	

	//Single 4KB can store 32768 bits (2^15) i.e. copy status of 2^15 blocks
	//i.e. 12 + 3 = 15 i.e. PAGE_SHIFT+3 blocks info
	page = scorw_get_page(inode, (blk_num/PAGE_BLOCKS));
	if(page == NULL)
	{
		printk(KERN_ERR "Failed to get page\n");
		return -1;
	}
	blk_num = blk_num%PAGE_BLOCKS;

	kaddr = kmap_atomic(page);
	byte_num = ((blk_num)/8);
	bit_num = ((blk_num)%8);
	byte_value = *(kaddr+byte_num);
	copy_state = (byte_value & (1<<bit_num));
	kunmap_atomic(kaddr);

	scorw_put_page(page);

	return copy_state;
}

//Note:
//	Block number passed to this function may or maynot be aligned to 8 bytes boundary
//	(8bytes = 8*8bits = 64 bits i.e. 64 blocks information is stored in 8 bytes of frnd file)
//
//Args:
//	inode: frnd vfs inode	
//	lblk: block number
//
//Returns:
//	8 bytes value in which copy bit corresponding blk_num is present
//
unsigned long long scorw_get_block_copied_8bytes(struct inode *inode, loff_t blk_num)
{
        struct page* page = 0;
        char* kaddr = 0;
        unsigned long long byte_num= 0;
        unsigned long long byte_value= 0;
	unsigned long long byte_num_alignedto_8bytes_boundary = 0;	

        //printk("%s() called. blk_num: %llu\n", __func__, blk_num);


        //Single 4KB can store 32768 bits (2^15) i.e. copy status of 2^15 blocks
        //i.e. 12 + 3 = 15 i.e. PAGE_SHIFT+3 blocks info
        page = scorw_get_page(inode, (blk_num/PAGE_BLOCKS));
        if(page == NULL)
        {
                printk(KERN_ERR "Failed to get page\n");
                return -1;
        }
        blk_num = blk_num%PAGE_BLOCKS;
        //printk("%s(): blk_num: %llu\n", __func__, blk_num);

        kaddr = kmap_atomic(page);
        byte_num = ((blk_num)/8);
        //printk("%s(): byte_num: %llu\n", __func__, byte_num);
	byte_num_alignedto_8bytes_boundary = ((byte_num >> 3) << 3);	//align byte_num to 8bytes boundary
        //printk("%s(): byte_num_alignedto_8bytes_boundary: %llu\n", __func__, byte_num_alignedto_8bytes_boundary);
        byte_value = *((unsigned long long*)(kaddr+byte_num_alignedto_8bytes_boundary));
        //printk("%s(): byte_value: %llu\n", __func__, byte_value);
        kunmap_atomic(kaddr);

        scorw_put_page(page);

        return byte_value;	
}

int scorw_is_extent_copied(struct scorw_inode *scorw_inode, unsigned start_block_num, unsigned len)
{
	unsigned i = 0;
	for(i = start_block_num; i < (start_block_num + len); i++)
	{
		if(!scorw_is_block_copied(scorw_inode->i_frnd_vfs_inode, i))
			return 0;
	}	

	return 1;
}


//===================================================================================

struct pending_frnd_version_cnt_inc *scorw_alloc_pending_frnd_version_cnt_inc(void)
{
	return kzalloc(sizeof(struct pending_frnd_version_cnt_inc), GFP_KERNEL);
}

void scorw_free_pending_frnd_version_cnt_inc(struct pending_frnd_version_cnt_inc *pending_frnd_version_cnt_inc)
{
	return kfree(pending_frnd_version_cnt_inc);
}

void scorw_add_pending_frnd_version_cnt_inc_list(struct pending_frnd_version_cnt_inc *pending_frnd_version_cnt_inc)
{
	INIT_LIST_HEAD(&pending_frnd_version_cnt_inc->list);
	list_add_tail(&(pending_frnd_version_cnt_inc->list), &pending_frnd_version_cnt_inc_list);
}

void scorw_remove_pending_frnd_version_cnt_inc_list(struct pending_frnd_version_cnt_inc *pending_frnd_version_cnt_inc)
{
	list_del(&(pending_frnd_version_cnt_inc->list));
}

//===================================================================================


//Taken from 'set_page_dirty(struct page *page)' and 'ext4_set_page_dirty(struct page *page)'
int scorw_set_page_dirty(struct page *page)
{
        //WARN_ON_ONCE(!PageLocked(page) && !PageDirty(page));
        //WARN_ON_ONCE(!page_has_buffers(page));
        return __set_page_dirty_buffers(page);
}


//Directly performing setting of bit at 1 byte granularity
//
//Args:
//	bitnum: which bit in 1 byte to set?
//	p:	address of byte whose bit is to be set
//
//Ref: 
//	1) arch_set_bit() 	
//	2) https://docs.oracle.com/cd/E19120-01/open.solaris/817-5477/eoizp/index.html
//
void scorw_set_bit(u8 bitnum, volatile u8 *p)
{
	BUG_ON(bitnum > 7);
	u8 bitmask = 1 << bitnum;
	//printk("%s(): p: %lx, bitmask: %x\n", __func__, p, bitmask);
	asm volatile(LOCK_PREFIX "orb %b1,%0"
			: "+m" (*(volatile char*)(p))
			: "iq" (bitmask)
			: "memory");
}

void scorw_set_block_copied(struct scorw_inode* scorw_inode, unsigned blk_num)
{
	struct page* page = 0;
	char* kaddr = 0;
	int byte_num= 0;
	int bit_num = 0;
	int ret = 0;
	struct inode *inode = scorw_inode->i_frnd_vfs_inode;
	//struct address_space *mapping = inode->i_mapping;

	//printk("scorw_set_block_copied called\n");
	

	//Single 4KB can store 32768 bits (2^15) i.e. copy status of 2^15 blocks
	//i.e. 12 + 3 = 15 i.e. PAGE_SHIFT+3 blocks info
	page = scorw_get_page(inode, (blk_num/PAGE_BLOCKS));
	if(page == NULL)
	{
		printk(KERN_ERR "Failed to get page\n");
	}

	//printk("%s(): page: %u obtained. Has buffers? %d\n", __func__, (blk_num/PAGE_BLOCKS), page_has_buffers(page));
	//new start
	if(!page_has_buffers(page))
	{
		//printk("%s(): page: %u doesn't have buffers\n", __func__, (blk_num/PAGE_BLOCKS));
		lock_page(page);
		// In case writeback began while the page was unlocked 
		wait_for_stable_page(page);

		//needed to create buffer heads and fill them with physical addresses
		//will be needed during writeback
		//Don't perform read operation, so, passed PAGE_SIZE as length
		ret = __block_write_begin(page, page->index << PAGE_SHIFT, PAGE_SIZE, ext4_da_get_block_prep);
		if(ret < 0)
		{
			unlock_page(page);
			scorw_put_page(page);
			printk("%s(): Error inside __block_write_begin\n", __func__);
			BUG_ON(ret<0);
		}

		unlock_page(page);
	}
	//new end

	blk_num = blk_num%PAGE_BLOCKS;

	kaddr = kmap_atomic(page);
	byte_num = ((blk_num)/8);
	bit_num = ((blk_num)%8);
	//Todo: want to update byte atomically. set_bit() is too expensive. Find an alternative.
	//set_bit(bit_num, (unsigned long *)(kaddr+byte_num));
	//*(kaddr+byte_num) |= (1<<bit_num);
	//printk("%s(): Before: frnd byte: %d, value: %x\n", __func__, byte_num, *(char*)(kaddr+byte_num));
	scorw_set_bit(bit_num, (char*)(kaddr+byte_num));
	//printk("%s(): After: frnd byte: %d, value: %x\n", __func__, byte_num, *(char*)(kaddr+byte_num));
	kunmap_atomic(kaddr);

	if(!PageDirty(page))
	{
		//printk("%s(): page: %u is not dirty. Setting it dirty.\n", __func__, page->index);
		lock_page(page);
		scorw_set_page_dirty(page);
		unlock_page(page);
	}

	scorw_put_page(page);
}

//Args:
//	frnd_inode:	inode of frnd file
//	start_blk:	blk in extent corresponding which 8 bytes value needs to be set
//	value_8bytes:	8 bytes value to be set in friend file's page at apt position
//
void scorw_set_block_copied_8bytes(struct inode* frnd_inode, unsigned long start_blk, unsigned long long value_8bytes)
{
        struct address_space *mapping = NULL;
	struct page* page = 0;
	void *fsdata = 0;
	char* kaddr = 0;
	int which_8bytes = 0;

	//printk("scorw_set_block_copied_8bytes called\n");
	mapping = frnd_inode->i_mapping;

	ext4_da_write_begin(NULL, mapping, ((unsigned long)start_blk/PAGE_BLOCKS)<< PAGE_SHIFT, PAGE_SIZE, 0, &page, &fsdata);
	if(page != NULL)
	{
		start_blk = start_blk % PAGE_BLOCKS;

		kaddr = kmap_atomic(page);
		which_8bytes = start_blk / 64;	//8 bytes == 64 bits. To which 8 bytes, this blk belongs to?
		*((unsigned long long*)(kaddr + (which_8bytes * 8))) |= value_8bytes;
		kunmap_atomic(kaddr);
	
		ext4_da_write_end(NULL, mapping , ((unsigned long)start_blk/PAGE_BLOCKS)<< PAGE_SHIFT, PAGE_SIZE, PAGE_SIZE, page, fsdata);
		balance_dirty_pages_ratelimited(mapping);
	}
	else
	{
		printk("scorw_set_block_copied_8bytes: ext4_da_write_begin failed to return a page\n");
	}
}

int scorw_ext_precache_depth0(struct inode* child_inode)
{
	int i;
	int depth = ext_depth(child_inode);
	struct ext4_extent_header *eh = ext_inode_hdr(child_inode);
	struct ext4_extent *ex = EXT_FIRST_EXTENT(eh);
	ext4_lblk_t prev = 0;

	for (i = le16_to_cpu(eh->eh_entries); i > 0; i--, ex++) {
		unsigned int status = EXTENT_STATUS_WRITTEN;
		ext4_lblk_t lblk = le32_to_cpu(ex->ee_block);
		int len = ext4_ext_get_actual_len(ex);

		if(prev && (prev != lblk))
		{
			ext4_es_cache_extent(child_inode, prev, lblk - prev, ~0, EXTENT_STATUS_HOLE);
		}

		if(ext4_ext_is_unwritten(ex))
		{
			status = EXTENT_STATUS_UNWRITTEN;
		}
		ext4_es_cache_extent(child_inode, lblk, len, ext4_ext_pblock(ex), status);
		prev = lblk + len;
	}

	return 0;
}

int scorw_rebuild_friend_file(struct inode* child_inode, struct inode* frnd_inode)
{
	struct ext4_es_tree *tree = 0;
	struct rb_node *node = 0;
	unsigned long long value_8bytes = 0;
	unsigned long start_blk = 0;
	unsigned long pending_extent_blks = 0;
	int num_bits_to_process = 0;
	int copy_bit = 0;
	int depth = ext_depth(child_inode);

	//printk("Inside scorw_rebuild_friend_file. child inode: %lu, frnd inode: %lu\n", child_inode->i_ino, frnd_inode->i_ino);

	//precache extents (bring extent info from disk into extent status tree)
	//printk("%s(): Child has depth %d\n", __func__, depth);
	if(depth == 0)
	{
		scorw_ext_precache_depth0(child_inode);
	}
	else
	{
		ext4_ext_precache(child_inode);
	}

	//scorw_es_print_tree(child_inode);	//<------------ Added for debugging purpose

	//process extents in extent status tree
	tree = &EXT4_I(child_inode)->i_es_tree;
	node = rb_first(&tree->root);
	while(node){
		struct extent_status *es;
		es = rb_entry(node, struct extent_status, rb_node);
		if((ext4_es_status(es) & ((ext4_fsblk_t)EXTENT_STATUS_WRITTEN)) ||
			(ext4_es_status(es) & ((ext4_fsblk_t)EXTENT_STATUS_UNWRITTEN)) ||
			(ext4_es_status(es) & ((ext4_fsblk_t)EXTENT_STATUS_DELAYED)))
		{
			start_blk = es->es_lblk;
			pending_extent_blks = es->es_len;
			//printk("%s(): [%u/%u) %llu EXTENT_STATUS_WRITTEN\n", __func__, es->es_lblk, es->es_len, ext4_es_pblock(es));
			while(pending_extent_blks)
			{

				/*
				 * Eg:
				 * 	extent info = 32/200 (extent start blk num = 32, extent length = 200 i.e blks 32 to 231 are part of this extent)
				 *	Initially, start blk = 32, pending extent blks = 200
				 *
				 *	(start blk, num bits to process in current 8 bytes (64 bit) boundary, pending extent blks)
				 *	32, 32, 168
				 *	64, 64, 104
				 *	128, 64, 40
				 *	192, 40, 0
				 */
				num_bits_to_process = ((64 - (start_blk % 64)) < (pending_extent_blks)) ? (64 - (start_blk % 64)) : (pending_extent_blks);

				value_8bytes = 0;	//since we have reset all friend file bits to 0, no need to call scorw_get_block_copied_8bytes() to read 
							//the 8bytes corresponding start blk (they will be 0)
				//printk("1. %s(): start_blk: %lu, pending_extent_blks: %lu, num_bits_to_process: %d, [Before]value_8bytes: %llx\n", __func__, start_blk, pending_extent_blks, num_bits_to_process, value_8bytes);

				//printk("2. %s(): copy_bit range: %lu to %lu\n", __func__, start_blk % 64, ((start_blk % 64) + num_bits_to_process));
				for(copy_bit = start_blk % 64; copy_bit < ((start_blk % 64) + num_bits_to_process); copy_bit++)
				{
					value_8bytes = value_8bytes | ((unsigned long long)1 << copy_bit);
				}
				//printk("3. %s(): [After 1]value_8bytes: %llx\n", __func__, value_8bytes);
				scorw_set_block_copied_8bytes(frnd_inode, start_blk, value_8bytes);

				pending_extent_blks -= num_bits_to_process;
				start_blk += num_bits_to_process;
			}
		}
		//if(ext4_es_status(es) & ((ext4_fsblk_t)EXTENT_STATUS_HOLE))
		//	printk(KERN_DEBUG " [%u/%u) %llu EXTENT_STATUS_HOLE", es->es_lblk, es->es_len, ext4_es_pblock(es));
		node = rb_next(node);
	}

	return 0;
}

int scorw_init_friend_file(struct inode* frnd_inode, long long child_copy_size)
{
	int i = 0;
	unsigned total_pages = (((child_copy_size % (PAGE_BLOCKS << PAGE_SHIFT)) == 0) ? (child_copy_size / (PAGE_BLOCKS << PAGE_SHIFT)) : ((child_copy_size / (PAGE_BLOCKS << PAGE_SHIFT))+1));
	long long frnd_size = frnd_inode->i_size; 

	//Build frnd file
	if(frnd_size == 0)
	{
		for(i = 0; i < total_pages; i++)
		{
			scorw_create_zero_page(frnd_inode, i);
		}
	}
	return 0;
}

int scorw_recover_friend_file(struct inode* child_inode, struct inode* frnd_inode, long long child_copy_size)
{
	unsigned i = 0;
	unsigned total_pages = 0; 	
	unsigned long long rebuilding_time_start = 0;
	unsigned long long rebuilding_time_end = 0;

	total_pages = (((child_copy_size % (PAGE_BLOCKS << PAGE_SHIFT)) == 0) ? (child_copy_size / (PAGE_BLOCKS << PAGE_SHIFT)) : ((child_copy_size / (PAGE_BLOCKS << PAGE_SHIFT))+1));

	//printk("scorw_prepare_friend_file called\n");
	//printk("scorw_prepare_friend_file: frnd_size: %lld\n", frnd_size);
	//printk("scorw_prepare_friend_file: total_pages to allocate in friend file: %lu\n", total_pages);
	//printk("scorw_prepare_friend_file: Rebuilding frnd file corresponding child: %lu\n", child_inode->i_ino);
	rebuilding_time_start = ktime_get_real_ns();
	/*
	 * It is possible that child blks are not present on disk but friend file has copy bits set corresponding these blks.
	 * Eg: Assume that a child file has 1000 dirty blks. In a single page of friend file itself copy bit corresponding all 1000 blks can be present.
	 * Now, it is possible that the sole friend file block gets flushed to disk while not all of the 1000 dirty blks of child get flushed to disk and system crashes.
	 * In this case, on recovery, friend file will contain some bits as set even though child doesn't have data corresponding those blks.
	 * Hence, we are resetting all bits in frnd file to 0 before beginning the rebuilding process.
	 */
	for(i = 0; i < total_pages; i++)
	{
		scorw_create_zero_page(frnd_inode, i);
	}
	scorw_rebuild_friend_file(child_inode, frnd_inode);
	rebuilding_time_end = ktime_get_real_ns();
	//printk("rebuilding_time_end: %llu, rebuilding_time_start: %llu, Rebuilding time (ms): %llu\n", rebuilding_time_end, rebuilding_time_start, (rebuilding_time_end - rebuilding_time_start)/1000000);
	last_recovery_time_us = (rebuilding_time_end - rebuilding_time_start)/1000;

	return 0;
}

int scorw_is_inode_dirty(struct inode* inode)
{
	int dirty_state = 0;
	spin_lock(&inode->i_lock);	
	dirty_state = !!(inode->i_state & I_DIRTY);
	spin_unlock(&inode->i_lock);	
	return dirty_state;
}


//load fields of scorw inode. Memory allocation of fields of scorw is also done here.
//Eg: For bitmap
//
//new_sparse parameter tells whether scorw inode is created corresponding a new sparse file or
//an existing sparse type
//
void scorw_prepare_child_inode(struct scorw_inode *scorw_inode, struct inode *vfs_inode, int new_sparse)
{
	int i = 0;
	unsigned j = 0;
	int first_write_done = 0;
	unsigned long p_ino_num = 0;
	unsigned long f_ino_num = 0;
	unsigned temp_hash_table_size = 0;
	unsigned long child_version_val = 0;
	unsigned long frnd_version_val = 0;

	//printk("1.Inside scorw_prepare_child_inode\n");

	p_ino_num = scorw_get_parent_attr_val(vfs_inode);
	BUG_ON(p_ino_num == 0);

	f_ino_num = scorw_get_friend_attr_val(vfs_inode);
	BUG_ON(f_ino_num == 0);

	scorw_inode->i_ino_num = vfs_inode->i_ino;	
	//atomic64_set((&(scorw_inode->i_process_usage_count)), 0);
	scorw_inode->i_process_usage_count = 0;
	atomic64_set((&(scorw_inode->i_thread_usage_count)), 0);
	scorw_inode->i_ino_unlinked = 0;
	scorw_inode->i_vfs_inode = ext4_iget(vfs_inode->i_sb, vfs_inode->i_ino, EXT4_IGET_NORMAL);
	scorw_inode->i_par_vfs_inode = 	ext4_iget(vfs_inode->i_sb, p_ino_num, EXT4_IGET_NORMAL);
	scorw_inode->i_frnd_vfs_inode =	ext4_iget(vfs_inode->i_sb, f_ino_num, EXT4_IGET_NORMAL);
	scorw_inode->i_copy_size = scorw_get_copy_size_attr_val(vfs_inode);
	atomic64_set(&(scorw_inode->i_pending_copy_pages),  scorw_get_blocks_to_copy_attr_val(scorw_inode->i_vfs_inode)); 
	scorw_inode->i_at_index = -1;
	ext4_inode_attach_jinode(scorw_inode->i_frnd_vfs_inode);
	ext4_inode_attach_jinode(vfs_inode);

	//printk("%s(): i_count of child inode: %u\n", __func__, scorw_inode->i_vfs_inode->i_count);
	//printk("%s(): i_count of par inode: %u\n", __func__, scorw_inode->i_par_vfs_inode->i_count);
	//printk("%s(): i_count of frnd inode: %u\n", __func__, scorw_inode->i_frnd_vfs_inode->i_count);
	
	scorw_inode->i_num_ranges = scorw_get_num_ranges_attr_val(vfs_inode);
	for(i=0; i<scorw_inode->i_num_ranges; i++)
	{
                #ifdef USE_OLD_RANGE
		  scorw_inode->i_range[i].start = scorw_get_range_i_start_attr_val(vfs_inode, i);
		  scorw_inode->i_range[i].end = scorw_get_range_i_end_attr_val(vfs_inode, i);
                #else
		  struct child_range_xattr crx;   
		  BUG_ON(!scorw_get_range_attr_val(vfs_inode, i, &crx));

		  scorw_inode->i_range[i].start = crx.start;
		  
		  scorw_inode->i_range[i].end = crx.end;
		  scorw_inode->i_range[i].snapx_behavior = crx.snap_behavior;
                #endif
		//printk("scorw_prepare_child_inode: range: %d, %ld:%ld:%d\n", i, scorw_inode->i_range[i].start, scorw_inode->i_range[i].end, crx.snap_behavior);
	}
#if 0
	for(i=scorw_inode->i_num_ranges; i<MAX_RANGES_SUPPORTED; i++)
	{
		scorw_inode->i_range[i].start = -1;
		scorw_inode->i_range[i].end = -1;
		//printk("scorw_prepare_child_inode: range: %d, %lld:%lld\n", i, scorw_inode->i_range[i].start, scorw_inode->i_range[i].end);
	}
#endif
	
	
	//printk("scorw_prepare_child_inode: i_ino_num: %lu\n", scorw_inode->i_ino_num);
	//printk("scorw_prepare_child_inode: p_inode_num: %lu\n", p_ino_num);
	//printk("scorw_prepare_child_inode: f_inode_num: %lu\n", f_ino_num);
	//printk("scorw_prepare_child_inode: i_process_usage_count: %u\n", scorw_inode->i_process_usage_count);
	//printk("scorw_prepare_child_inode: i_thread_usage_count: %u\n", scorw_inode->i_thread_usage_count);
	//printk("scorw_prepare_child_inode: i_copy_size: %lld\n", scorw_inode->i_copy_size);
	//printk("scorw_prepare_child_inode: i_pending_copy_size: %u\n", scorw_inode->i_pending_copy_pages);
	
	mutex_init(&(scorw_inode->i_uncopied_extents_list_lock));
	INIT_LIST_HEAD(&(scorw_inode->i_uncopied_extents_list));

	//find the number of blocks occupied by parent file at the time of copying and divide the result by 4.
	//Then, round the number to lower power of 2.
	//
	//Why divide by 4? 
	//Because, we are creating hash lists (size of hash table) proportional to the size of parent file at the time of copying
	//Initially, we created 1 million (2^20) such lists  while testing for 16GB files
	//i.e. 2^20 lists for file with 2^22 blocks.
	//We keep this proportion unchanged i.e. for every 4 blocks in parent file, we create a list in the hash table
	//
	//Why round the number to lower power of 2?
	//Eg: 6MB file => 384 lists.  
	//    hash table api's work with power of 2. Hence, when log2(384) is calculated, hash table api's
	//    will treat hash table as of size 256 i.e. 256 linked lists only i.e. remaining lists will be unused.
	//    Hence, we before hand round of number of linked lists to a power of 2, so that, unecessary
	//    lists are not created.
	//
	//Update:
	//* Number of active map entries at any time = ((write size)/4KB * num threads)
	//* For example, for a 32 threaded experiment with write size <= 4KB, max number of active map entries anytime will be 32
	//* For example, for a 32 threaded experiment with write size = 1MB, max number of active map entries anytime will be (32 * (1MB/4KB)) = 2^13
	//* Above, example is an extreme case and normally, active map with few ten's or hundred's of lists should suffice
	//* I think, size of parent file at the time of copying is irrelevant in deciding the size of active map lists
	
	//temp_hash_table_size = (((scorw_inode->i_copy_size - 1) >> PAGE_SHIFT) + 1) >> 2;
	temp_hash_table_size = 128;
	temp_hash_table_size = temp_hash_table_size > MIN_HASH_TABLE_SIZE ? temp_hash_table_size : MIN_HASH_TABLE_SIZE; 
	temp_hash_table_size = 1 << (ilog2(temp_hash_table_size));
	scorw_inode->i_hash_table_size = temp_hash_table_size;

	scorw_inode->i_uncopied_blocks_list = vmalloc(sizeof(struct hlist_head) * scorw_inode->i_hash_table_size);
	BUG_ON(scorw_inode->i_uncopied_blocks_list == NULL);
	__hash_init(scorw_inode->i_uncopied_blocks_list, scorw_inode->i_hash_table_size);

	scorw_inode->i_uncopied_blocks_lock = vmalloc(sizeof(struct spinlock) * scorw_inode->i_hash_table_size);
	BUG_ON(scorw_inode->i_uncopied_blocks_lock == NULL);
	for(j = 0; j < scorw_inode->i_hash_table_size; j++)
	{
		spin_lock_init(&(scorw_inode->i_uncopied_blocks_lock[j]));
	}

	//printk("%s(): [Before recovery code] child file (inode: %lu) child is dirty (%d) (inode dirty: %lu, data pages dirty: %lu)\n", __func__, scorw_inode->i_vfs_inode->i_ino, scorw_is_inode_dirty(scorw_inode->i_vfs_inode), scorw_inode->i_vfs_inode->i_state & I_DIRTY_INODE, scorw_inode->i_vfs_inode->i_state & I_DIRTY_PAGES);

	//Recovering frnd file
	//Note: Trigger recovery only when child version count hasn't been incremented yet
	//	i.e. we are not in the middle of the updation of child and frnd version counts.
	first_write_done = atomic_read(&(scorw_inode->i_vfs_inode->i_cannot_update_child_version_cnt));
	if(first_write_done == 0)
	{
		child_version_val = scorw_get_child_version_attr_val(scorw_inode->i_vfs_inode);	
		frnd_version_val = scorw_get_frnd_version_attr_val(scorw_inode->i_frnd_vfs_inode);	
		BUG_ON((child_version_val == 0) || (frnd_version_val == 0));

		//printk("%s(): Recover frnd: %d (child version: %lu, frnd version: %lu)\n", __func__, child_version_val != frnd_version_val, child_version_val, frnd_version_val);
		if(child_version_val != frnd_version_val)
		{
			//recover frnd file
			scorw_recover_friend_file(scorw_inode->i_vfs_inode, scorw_inode->i_frnd_vfs_inode, scorw_inode->i_copy_size);

			//sync recovered frnd file blocks
			write_inode_now(scorw_inode->i_frnd_vfs_inode, 1);

			//set frnd version count equal to child version cnt
			frnd_version_val = child_version_val;
			scorw_set_frnd_version_attr_val(scorw_inode->i_frnd_vfs_inode, frnd_version_val);

			//sync frnd inode
			sync_inode_metadata(scorw_inode->i_frnd_vfs_inode, 1);

			++num_files_recovered;
			//printk("%s(): Num files recovered: %lu\n", __func__, num_files_recovered);
		}

	}
	else
	{
		//printk("%s(): Skipping frnd recovery. We are in the middle of version cnt update.\n", __func__);
	}
	//printk("%s(): [After recovery code] child file (inode: %lu) child is dirty (%d) (inode dirty: %lu, data pages dirty: %lu)\n", __func__, scorw_inode->i_vfs_inode->i_ino, scorw_is_inode_dirty(scorw_inode->i_vfs_inode), scorw_inode->i_vfs_inode->i_state & I_DIRTY_INODE, scorw_inode->i_vfs_inode->i_state & I_DIRTY_PAGES);

	//printk("scorw_prepare_child_inode: returning\n");
	//debugging start
	scorw_inode->i_par_vfs_inode->scorw_page_cache_hits = 0;
	scorw_inode->i_par_vfs_inode->scorw_page_cache_misses = 0;
	//debugging end
}


//unload fields of scorw inode. Eg: deallocation of memory occupied by fields, reseting of fields
void scorw_unprepare_child_inode(struct scorw_inode *scorw_inode)
{
	struct pending_frnd_version_cnt_inc *pending_frnd_version_cnt_inc = 0;
	int entry_found = 0;
	struct inode *child_inode = scorw_inode->i_vfs_inode;
	struct inode *par_inode = scorw_inode->i_par_vfs_inode; 
	struct inode *frnd_inode = scorw_inode->i_frnd_vfs_inode; 
	//printk("Inside scorw_unprepare_child_inode\n");
	
	//printk("%s(): [closing file] child file (inode: %lu) child is dirty (%d) (inode dirty: %lu, data pages dirty: %lu)\n", __func__, scorw_inode->i_vfs_inode->i_ino, scorw_is_inode_dirty(scorw_inode->i_vfs_inode), scorw_inode->i_vfs_inode->i_state & I_DIRTY_INODE, scorw_inode->i_vfs_inode->i_state & I_DIRTY_PAGES);
	
	//debugging start
	//printk("Num page cache hits: %lu\n", scorw_inode->i_par_vfs_inode->scorw_page_cache_hits);
	//printk("Num page cache misses: %lu\n", scorw_inode->i_par_vfs_inode->scorw_page_cache_misses);
	//debugging end
	
	vfree(scorw_inode->i_uncopied_blocks_list);
	vfree(scorw_inode->i_uncopied_blocks_lock);

	//printk("%s(): i_count of child inode: %u\n", __func__, atomic_read(&scorw_inode->i_vfs_inode->i_count) - 1);
	//printk("%s(): i_count of par inode: %u\n", __func__, atomic_read(&scorw_inode->i_par_vfs_inode->i_count) - 1);
	//printk("%s(): i_count of frnd inode: %u\n", __func__, atomic_read(&scorw_inode->i_frnd_vfs_inode->i_count) - 1);

	//If no write occurred on child, then skip updation of version cnt of frnd
	//Note:
	//	Recall, once write operation occurs on child, i_cannot_update_child_version_cnt flag is set and it remains set
	//	until version count of frnd inode is updated. So, intermediate open()/close() calls between setting and
	//	unsetting of the i_cannot_update_child_version_cnt, still see i_cannot_update_child_version_cnt flag as set.
	if(atomic_read(&child_inode->i_cannot_update_child_version_cnt) == 0)
	{
		//printk("%s(): No write operation performed on child. Skipping updation of version count of frnd of child inode: %lu\n", __func__, child_inode->i_ino);
		iput(par_inode);	
		iput(child_inode);	
		iput(frnd_inode);	
	}
	else
	{
		//queue relevant information for updation of version cnt of frnd
		//Note:
		//	Relevant locks s.a. inode lock for ordering opening/closing of child file (i_vfs_inode_lock) and 
		//	lock that protects 'pending_frnd_version_cnt_inc_list' list
		//	are taken in scorw_put_inode() beforehand.
		//
		entry_found = 0;
		//check whether information is already queued 
		list_for_each_entry(pending_frnd_version_cnt_inc, &pending_frnd_version_cnt_inc_list, list)
		{
			//printk("%s(): Child inode: %lu waiting in queue for processing\n", __func__, pending_frnd_version_cnt_inc->child->i_ino);
			if(pending_frnd_version_cnt_inc->child->i_ino == child_inode->i_ino)
			{
				entry_found = 1;
				break;
			}
		}
		//information is already queued
		iput(par_inode);	
		if(entry_found)
		{
			//printk("%s(): Child inode: %lu ALREADY waiting in queue for processing\n", __func__, child_inode->i_ino);
			iput(frnd_inode);	
			iput(child_inode);	
		}
		else
		{
			//printk("%s(): Child inode: %lu NOT in queue waiting for processing. Adding it.\n", __func__, child_inode->i_ino);
			//queue information
			//Note: we don't do iput(child, frnd, par) here. It will be done in
			//asynch thread, when it does version count related processing
			pending_frnd_version_cnt_inc = scorw_alloc_pending_frnd_version_cnt_inc();
			BUG_ON(pending_frnd_version_cnt_inc == NULL);

			pending_frnd_version_cnt_inc->child = child_inode;
			pending_frnd_version_cnt_inc->frnd = frnd_inode;
			pending_frnd_version_cnt_inc->iter_id = 0;

			scorw_add_pending_frnd_version_cnt_inc_list(pending_frnd_version_cnt_inc);
		}
	}
	//printk("%s(): Closing child\n", __func__);
}


void scorw_prepare_par_inode(struct scorw_inode *scorw_inode, struct inode *vfs_inode)
{
	int i = 0;
	//unsigned long c_ino_num = 0;
   	//struct scorw_inode *c_inode = 0;

	//printk("Inside scorw_prepare_par_inode\n");

	scorw_inode->i_ino_num = vfs_inode->i_ino;	
	//atomic64_set((&(scorw_inode->i_process_usage_count)), 0);
	scorw_inode->i_process_usage_count = 0;
	atomic64_set((&(scorw_inode->i_thread_usage_count)), 0);
	//printk("scorw_prepare_par_inode: inode num: %lu, inode ref count value: %u (Before iget, is parent)\n", scorw_inode->i_ino_num, vfs_inode->i_count);
	scorw_inode->i_vfs_inode = ext4_iget(vfs_inode->i_sb, vfs_inode->i_ino, EXT4_IGET_NORMAL);
	//printk("scorw_prepare_par_inode: inode num: %lu, inode ref count value: %u (After iget, is parent)\n", scorw_inode->i_ino_num, vfs_inode->i_count);
	scorw_inode->i_par_vfs_inode = NULL;
	scorw_inode->i_frnd_vfs_inode = NULL;
	scorw_inode->i_copy_size = 0;
	scorw_inode->i_ino_unlinked = 0;
	scorw_inode->i_at_index = -1;
	scorw_inode->i_num_ranges = 0;
	for(i=0; i<MAX_RANGES_SUPPORTED; i++)
	{
		scorw_inode->i_range[i].start = -1;
		scorw_inode->i_range[i].end = -1;
	}
}

//unload fields of scorw inode. Eg: deallocation of memory occupied by fields, reset of fields
void scorw_unprepare_par_inode(struct scorw_inode *scorw_inode)
{
	//int i;
	//struct inode *c_inode;
	//printk("Inside scorw_unprepare_par_inode\n");
	//printk("scorw_unprepare_par_inode: inode num: %lu, inode ref count value: %u (Before iput)\n", scorw_inode->i_vfs_inode->i_ino, scorw_inode->i_vfs_inode->i_count);
	iput(scorw_inode->i_vfs_inode);
	//printk("scorw_unprepare_par_inode: inode num: %lu, inode ref count value: %u (After iput)\n", scorw_inode->i_vfs_inode->i_ino, scorw_inode->i_vfs_inode->i_count);
}

//allocate memory for scorw inode
struct scorw_inode *scorw_alloc_inode(void)
{
	//printk("Inside scorw_alloc_inode\n");
	//return kzalloc(sizeof(struct scorw_inode), GFP_KERNEL);
	//return vzalloc(sizeof(struct scorw_inode));
	
	//vzalloc() internally calls __vmalloc() but with GFP_KERNEL flag. Changed the flag to GFP_NOWAIT
	return __vmalloc(sizeof(struct scorw_inode), GFP_NOWAIT | __GFP_HIGHMEM | __GFP_ZERO, PAGE_KERNEL);
}

//free memory occupied by scorw inode
void scorw_free_inode(struct scorw_inode *scorw_inode)
{
	//printk("Inside scorw_free_inode\n");
	return vfree(scorw_inode);
}



//add an scorw inode into list of scorw inode's
void scorw_add_inode_list(struct scorw_inode *scorw_inode)
{
	//struct scorw_inode *tmp;
	//printk("Inside scorw_add_inode_list\n");
	//printk("%s: Adding scrow_inode = %lx, inode_num = %lu, vfs_inode  = %lx \n", __func__, scorw_inode, scorw_inode->i_ino_num, scorw_inode->i_vfs_inode);
	INIT_LIST_HEAD(&scorw_inode->i_list);
	list_add_tail(&(scorw_inode->i_list), &scorw_inodes_list);
        
        /*list_for_each_entry(tmp, &scorw_inodes_list, i_list)
	   printk("%s: Listing scrow_inode = %lx, inode_num = %lu, vfs_inode  = %lx \n", __func__, tmp, tmp->i_ino_num, tmp->i_vfs_inode);
	*/

}

//remove an scorw inode from list of scorw inode's
void scorw_remove_inode_list(struct scorw_inode *scorw_inode)
{
	//printk("%s: Removing scrow_inode = %lx, inode_num = %lu, vfs_inode  = %lx \n", __func__, scorw_inode, scorw_inode->i_ino_num, scorw_inode->i_vfs_inode);
	list_del(&(scorw_inode->i_list));
}

//print scorw inodes list
void scorw_print_inode_list(void)
{
	struct list_head *curr;
	struct scorw_inode *curr_scorw_inode;

	//printk("Inside scorw_print_inode_list\n");
	//printk("scorw_print_inode_list: &scorw_inodes_list: %p\n", &scorw_inodes_list);

	struct list_head *head;
	head = &scorw_inodes_list;

	//list_for_each(curr, &scorw_inodes_list)
	list_for_each(curr, head)
	{
		curr_scorw_inode = list_entry(curr, struct scorw_inode, i_list);
		//printk("\nscorw_print_inode_list: node->i_ino_num: %lu\n", curr_scorw_inode->i_ino_num);
		if(curr_scorw_inode->i_vfs_inode)
		{
			//printk("scorw_print_inode_list: node->i_vfs_inode->i_ino: %lu\n", curr_scorw_inode->i_vfs_inode->i_ino);
		}
		if(curr_scorw_inode->i_par_vfs_inode)
		{
			//printk("scorw_print_inode_list: node->i_par_vfs_inode->i_ino: %lu\n", curr_scorw_inode->i_par_vfs_inode->i_ino);
		}
		//printk("\nscorw_print_inode_list: node->i_pending_copy_pages: %u\n", curr_scorw_inode->i_pending_copy_pages);
		//printk("\nscorw_print_inode_list: node->i_usage_count: %d\n", curr_scorw_inode->i_usage_count);
		//printk("\nscorw_print_inode_list: node->i_thread_usage_count: %d\n", curr_scorw_inode->i_thread_usage_count);
	}

	//printk("Returning from scorw_print_inode_list\n");
}


//===================================================================================
//
struct wait_for_commit *scorw_alloc_wait_for_commit(void)
{
	return kzalloc(sizeof(struct wait_for_commit), GFP_KERNEL);
}

void scorw_free_wait_for_commit(struct wait_for_commit *wait_for_commit)
{
	return kfree(wait_for_commit);
}

void scorw_add_wait_for_commit_list(struct wait_for_commit *wait_for_commit)
{
	INIT_LIST_HEAD(&wait_for_commit->list);
	list_add_tail(&(wait_for_commit->list), &wait_for_commit_list);
}

void scorw_remove_wait_for_commit_list(struct wait_for_commit *wait_for_commit)
{
	list_del(&(wait_for_commit->list));
}

//===================================================================================


//allocate memory for uncopied extent 
struct uncopied_extent *scorw_alloc_uncopied_extent(void)
{
	//printk("Inside scorw_alloc_uncopied_extent\n");
	return kzalloc(sizeof(struct uncopied_extent), GFP_KERNEL);
}

//free memory occupied by uncopied extent 
void scorw_free_uncopied_extent(struct uncopied_extent *uncopied_extent)
{
	//printk("Inside scorw_free_uncopied_extent\n");
	return kfree(uncopied_extent);
}


//add an uncopied extent into list of uncopied extent's
//parameters:
//scorw_inode: scorw inode to which uncopied extent belongs
//uncopied extent: uncopied extent that needs to be added in uncopied extent's list
void scorw_add_uncopied_extent_list(struct scorw_inode *scorw_inode, struct uncopied_extent *uncopied_extent)
{
	//printk("Inside scorw_add_uncopied_extent\n");

	INIT_LIST_HEAD(&(uncopied_extent->list));
	list_add_tail(&(uncopied_extent->list), &(scorw_inode->i_uncopied_extents_list));
}

//remove an uncopied extent from list of uncopied extent's
void scorw_remove_uncopied_extent_list(struct uncopied_extent* uncopied_extent)
{
	if(uncopied_extent != 0)
		list_del(&(uncopied_extent->list));
}

//print uncopied extents list
void scorw_print_uncopied_extent_list(struct scorw_inode *scorw_inode)
{
	struct list_head *curr;
	struct uncopied_extent *curr_uncopied_extent;

	//printk("Inside scorw_print_uncopied_extent_list\n");
	list_for_each(curr, &(scorw_inode->i_uncopied_extents_list))
	{
		curr_uncopied_extent = list_entry(curr, struct uncopied_extent, list);
		//printk("scorw_print_uncopied_extent_list: curr_uncopied_extent->start_block_num: %u, length: %u\n", curr_uncopied_extent->start_block_num, curr_uncopied_extent->len);
	}
}

//empty uncopied extents list i.e. remove all uncopied extents 
void scorw_empty_uncopied_extent_list(struct scorw_inode *scorw_inode)
{
	struct list_head *curr;
	struct uncopied_extent *curr_uncopied_extent = 0;
	struct uncopied_extent *prev_uncopied_extent = 0;

	//printk("Inside scorw_empty_uncopied_extent_list\n");

	list_for_each(curr, &(scorw_inode->i_uncopied_extents_list))
	{
		curr_uncopied_extent = list_entry(curr, struct uncopied_extent, list);
		scorw_remove_uncopied_extent_list(prev_uncopied_extent);
		prev_uncopied_extent = curr_uncopied_extent;
	}
	scorw_remove_uncopied_extent_list(prev_uncopied_extent);
}


//find  an uncopied extent from uncopied extents list
struct uncopied_extent *scorw_find_uncopied_extent_list(struct scorw_inode *scorw_inode, unsigned block, unsigned len)
{
	struct list_head *curr;
	struct uncopied_extent *curr_uncopied_extent;

	//printk("Inside scorw_find_uncopied_extent\n");

	list_for_each(curr, &(scorw_inode->i_uncopied_extents_list))
	{
		curr_uncopied_extent = list_entry(curr, struct uncopied_extent, list);
		if((block == curr_uncopied_extent->start_block_num) && (len == curr_uncopied_extent->len))
		{
			return curr_uncopied_extent;
		}
	}
	return NULL;
}

int scorw_get_uncopied_extent(struct scorw_inode *scorw_inode, unsigned block, unsigned len)
{
	struct uncopied_extent *uncopied_extent;
	//printk("Inside scorw_get_uncopied_extent\n");

	uncopied_extent = scorw_alloc_uncopied_extent();
	if(uncopied_extent)
	{
		uncopied_extent->start_block_num = block;
		uncopied_extent->len = len;
		scorw_add_uncopied_extent_list(scorw_inode, uncopied_extent);
	}
	else
	{
		printk(KERN_ERR "SCORW_OUT_OF_MEMORY: Failed to allocate memory for uncopied extent\n");
		return -1;
	}
	return 0;
}

//An extent that was earlier uncopied has now been copied.
//Arguments:
//	scorw_inode: scorw inode whose extent got copied
//	block: starting block of extent that got copied
//	len: length of extent that got copied
void scorw_put_uncopied_extent(struct scorw_inode *scorw_inode, unsigned block, unsigned len)
{
	struct uncopied_extent *uncopied_extent;
	//printk("Inside scorw_put_uncopied_extent\n");

	//remove info about this extent from uncopied extents list
	uncopied_extent = scorw_find_uncopied_extent_list(scorw_inode, block, len);
	if(uncopied_extent != NULL)
	{
		scorw_remove_uncopied_extent_list(uncopied_extent);
		scorw_free_uncopied_extent(uncopied_extent);
	}
}



//===================================================================================


//allocate memory to maintain info about uncopied block being processed
struct uncopied_block *scorw_alloc_uncopied_block(void)
{
	//printk("Inside scorw_alloc_uncopied_block\n");
	return ((struct uncopied_block *)kzalloc(sizeof(struct uncopied_block), GFP_NOWAIT));
}

//uncopied block has been copied. Free memory occupied by its structure.
void scorw_free_uncopied_block(struct uncopied_block *uncopied_block)
{
	//printk("Inside scorw_free_uncopied_block\n");
	kfree(uncopied_block);
}

//print being processed uncopied blocks list
void scorw_print_uncopied_blocks_list(struct scorw_inode *scorw_inode)
{
	
    	struct uncopied_block *cur = 0;
	unsigned bkt = 0; //bucket

	printk("Inside scorw_print_uncopied_blocks_list, inode: %lu\n", scorw_inode->i_ino_num);
	/*
	hash_for_each(scorw_inode->i_uncopied_blocks_list, bkt, cur, node)
	{
		printk("scorw_print_uncopied_blocks_list: element: start_block_num= %u, bucket= %d, inode: %lu\n" , cur->block_num, bkt, scorw_inode->i_ino_num);
	}
	*/
	
        printk("%s(), Dumping all elements in hash_table", __func__);
        for((bkt) = 0, cur = NULL; cur == NULL && (bkt) < scorw_inode->i_hash_table_size; (bkt)++)
        {
                hlist_for_each_entry(cur, &scorw_inode->i_uncopied_blocks_list[bkt], node)
                {
                        printk("%s(), bkt: %d, blk num: %u\n", __func__, bkt, cur->block_num);
                }
        }
        printk("%s(), ----- End of Dumping -----", __func__);
}

//add uncopied block being processed into list of uncopied blocks being processed
//parameters:
//scorw_inode: scorw inode to which block being processed belongs
//uncopied block: block being processed that needs to be added in block's being processed's list
void scorw_add_uncopied_blocks_list(struct scorw_inode *scorw_inode, struct uncopied_block *uncopied_block)
{
	//printk("Inside scorw_add_uncopied_blocks_list\n");
	//hash_add((scorw_inode->i_uncopied_blocks_list), &(uncopied_block->node), uncopied_block->block_num);
	//hash_add(hashtable, node, key)
        //hlist_add_head(node, &hashtable[hash_min(key, HASH_BITS(hashtable))])
	
	//printk("scorw_add_uncopied_block_list, inode: %lu. Added record with key: %u\n", scorw_inode->i_ino_num, uncopied_block->block_num);
        hlist_add_head(&(uncopied_block->node), &scorw_inode->i_uncopied_blocks_list[hash_min(uncopied_block->block_num, ilog2(scorw_inode->i_hash_table_size))]);
	//scorw_print_uncopied_blocks_list(scorw_inode);
	//printk("Returning from scorw_add_uncopied_blocks_list\n");
}

//remove an being processed uncopied block from list of being processed block's
void scorw_remove_uncopied_blocks_list(struct uncopied_block* uncopied_block)
{
	//printk("Inside scorw_remove_uncopied_blocks_list\n");
	//printk("scorw_remove_uncopied_block_list, Removing record with key: %u\n", uncopied_block->block_num);
	hash_del(&(uncopied_block->node));
	//printk("Returning from scorw_remove_uncopied_blocks_list\n");
}


//find a being processed uncopied block from being processed uncopied blocks list
struct uncopied_block *scorw_find_uncopied_block_list(struct scorw_inode *scorw_inode, unsigned key)
{
    	struct uncopied_block *cur = 0;

	//printk("Inside scorw_find_uncopied_block_list, inode: %lu. Looking for record with key: %u\n", scorw_inode->i_ino_num, key);
	/*
	hash_for_each_possible(scorw_inode->i_uncopied_blocks_list, cur, node, key)
	{
		//printk("scorw_find_uncopied_block_list: cur->block_num: %lu, key: %u\n", cur->block_num, key);
		if (cur->block_num == key)
		{
			return cur;
		}
	}
	*/
	
	hlist_for_each_entry(cur, &scorw_inode->i_uncopied_blocks_list[hash_min(key, ilog2(scorw_inode->i_hash_table_size))], node)
        {
		//printk("scorw_find_uncopied_block_list: cur->block_num: %u, key: %u\n", cur->block_num, key);
		if (cur->block_num == key)
		{
			//printk("scorw_find_uncopied_block_list: match found!\n");
			return cur;
		}
        }
	
	//printk("scorw_find_uncopied_block_list: No match found!\n");

	return NULL;
}




//Which of the operations done on an uncopied extent can be be done in parallel i.e. simultaneously?
int scorw_is_compatible_processing_type(int pt1, int pt2)
{
	//readings are compatible with each other
	//reading and copying is compatible
	//copying is not compatible with copying

	if((pt1 & READING) && (pt2 & COPYING))
		return 1;
	if((pt1 & COPYING) && (pt2 & READING))
		return 1;
	if((pt1 & READING) && (pt2 & READING))
		return 1;
	if(pt1 == NOP)
		return 1;
	if(pt2 == NOP)
		return 1;

	return 0;
}



//Arguments:
//	scorw_inode: scorw inode corresponding which being processed uncopied block needs to be created.
//	block_num: block number of the block that will be processed (acts as key).
//	processing_type: What operation is being done on this block? (read, copy)
struct uncopied_block* scorw_get_uncopied_block(struct scorw_inode *scorw_inode, unsigned block_num, int processing_type)
{
	int ret = 0;
    	struct uncopied_block *uncopied_block = 0;

	//printk("[pid: %lu] %s(): Getting blk: %u of child inode: %lu\n", current->pid, __func__, block_num, scorw_inode->i_ino_num);

	//Note:
	//	* We know hash table is a array of linked lists.
	//	* List used to hash an element is calculated using "hash_min(key, HASH_BITS(hashtable))" function
	//	* We are protecting i'th list with i'th spinlock
	//	* Since, list is protected by this spinlock, we don't need to worry about insertion/deletion/lookup
	//	  happening in parallel because if i'th spinlock is acquired, no other function can do insertion, 
	//	  deletion in this list.
	//	  In parallel, insertion/deletion/lookup can happen in other lists of hash table
	//
	spin_lock(&(scorw_inode->i_uncopied_blocks_lock[hash_min(block_num, ilog2(scorw_inode->i_hash_table_size))]));
	
	//scorw_print_uncopied_blocks_list(scorw_inode);
	uncopied_block = scorw_find_uncopied_block_list(scorw_inode, block_num);
	if(uncopied_block)
	{
		while(!scorw_is_compatible_processing_type(uncopied_block->processing_type, processing_type))
		{
			uncopied_block->num_waiting += 1;
			spin_unlock(&(scorw_inode->i_uncopied_blocks_lock[hash_min(block_num, ilog2(scorw_inode->i_hash_table_size))]));

			//wait_event(uncopied_block->wait_queue, scorw_is_compatible_processing_type(uncopied_block->processing_type, processing_type));
			ret = wait_event_timeout(uncopied_block->wait_queue, scorw_is_compatible_processing_type(uncopied_block->processing_type, processing_type), 10*HZ);
			/*
			if(ret == 0)
			{
				printk("[pid: %d] %s(): waiting to acquire lock for blk: %u\n", current->pid, __func__, block_num);
			}
			*/

			spin_lock(&(scorw_inode->i_uncopied_blocks_lock[hash_min(block_num, ilog2(scorw_inode->i_hash_table_size))]));
			uncopied_block->num_waiting -=1;
		}
		uncopied_block->processing_type |= processing_type;
		if(processing_type == READING)
		{
			++(uncopied_block->num_readers);
		}
	}
	else
	{
		//printk("scorw_get_uncopied_block: New uncopied block allocated\n");
		uncopied_block = scorw_alloc_uncopied_block();
		if(uncopied_block)
		{
			uncopied_block->block_num = block_num;
			uncopied_block->processing_type = processing_type;
			uncopied_block->num_waiting = 0;
			uncopied_block->num_readers = 0;
			if(processing_type == READING)
			{
				uncopied_block->num_readers = 1;
			}
			init_waitqueue_head(&(uncopied_block->wait_queue));

			scorw_add_uncopied_blocks_list(scorw_inode, uncopied_block);
		}
		else
		{
			printk(KERN_ERR "SCORW_OUT_OF_MEMORY: Memory not allocated for uncopied_block\n");
		}
	}
	//scorw_print_uncopied_blocks_list(scorw_inode);
	spin_unlock(&(scorw_inode->i_uncopied_blocks_lock[hash_min(block_num, ilog2(scorw_inode->i_hash_table_size))]));

	return uncopied_block;
}



//An block that was earlier being processed has now been completely processed.
//Arguments:
//	scorw_inode: scorw inode whose block got processed 
//	key: block number of the block that got processed 
int scorw_put_uncopied_block(struct scorw_inode *scorw_inode, unsigned key, int processing_type, struct uncopied_block* ptr_uncopied_block)
{
	int updated_processing_type = 0;
	struct uncopied_block *uncopied_block = 0;

	//printk("[pid: %lu] %s(): Putting blk: %u for child inode: %lu\n", current->pid, __func__, key, scorw_inode->i_ino_num);

	spin_lock(&(scorw_inode->i_uncopied_blocks_lock[hash_min(key, ilog2(scorw_inode->i_hash_table_size))]));

	//Note: This lookup is important because it is possible that
	//	two threads can simultaneously call this function + scorw_remove_uncopied_block function for same block.
	//	As a result, uncopied block can be freed before this function runs resulting
	//	in use after free problem.
	//
	uncopied_block = scorw_find_uncopied_block_list(scorw_inode, key);
	if(uncopied_block == NULL)
	{
		spin_unlock(&(scorw_inode->i_uncopied_blocks_lock[hash_min(key, ilog2(scorw_inode->i_hash_table_size))]));
		return -1;
	}

	if(processing_type == READING)
	{
		--(uncopied_block->num_readers);

		if((uncopied_block->num_readers) > 0)
		{
			spin_unlock(&(scorw_inode->i_uncopied_blocks_lock[hash_min(key, ilog2(scorw_inode->i_hash_table_size))]));
			return 0;
		}
	}
	updated_processing_type = ~(processing_type);
	updated_processing_type &= uncopied_block->processing_type;
	uncopied_block->processing_type = updated_processing_type;

	if((uncopied_block->processing_type == NOP) &&  (uncopied_block->num_waiting != 0))
	{
		wake_up(&(uncopied_block->wait_queue));
	}
	/*
	else
	{
		printk("[pid: %d] %s(): Decided not to call wakeup for blk num: %lu, processing type: %u, num waiters: %u, child inode: %lu\n", current->pid, __func__, key, uncopied_block->processing_type, uncopied_block->num_waiting, scorw_inode->i_ino_num);
	}
	*/

	spin_unlock(&(scorw_inode->i_uncopied_blocks_lock[hash_min(key, ilog2(scorw_inode->i_hash_table_size))]));
	return 0;
}

//Assumption:
//	1) This is called when there is no put pending corresponding any get
//	Update: Even if put is pending, then also no problem. Because uncopied block will get removed due to this remove and 
//	when put gets called, it finds that uncopied block doesn't exists and it does nothing.
int scorw_remove_uncopied_block(struct scorw_inode *scorw_inode, unsigned key, struct uncopied_block *ptr_uncopied_block)
{
    	struct uncopied_block *uncopied_block = 0;

	//printk("Inside %s()\n", __func__);
	spin_lock(&(scorw_inode->i_uncopied_blocks_lock[hash_min(key, ilog2(scorw_inode->i_hash_table_size))]));

	//Note: This lookup is important because it is possible that
	//	two threads can simultaneously call this function for same block.
	//	As a result, double free can occur if we don't perform this lookup
	//	and directly free the uncopied block passed as the argument
	//
	uncopied_block = scorw_find_uncopied_block_list(scorw_inode, key);
	if(uncopied_block == NULL)
	{
		spin_unlock(&(scorw_inode->i_uncopied_blocks_lock[hash_min(key, ilog2(scorw_inode->i_hash_table_size))]));
		return 0;
	}
	else if((uncopied_block->processing_type == NOP) &&  (uncopied_block->num_waiting == 0))
	{
		scorw_remove_uncopied_blocks_list(uncopied_block);
		scorw_free_uncopied_block(uncopied_block);
	}
	spin_unlock(&(scorw_inode->i_uncopied_blocks_lock[hash_min(key, ilog2(scorw_inode->i_hash_table_size))]));

	return 0;
}


//====================================================================================

//allocate memory for page copy
struct page_copy *scorw_alloc_page_copy(void)
{
	//printk("Inside scorw_alloc_page_copy\n");
	//return kzalloc(sizeof(struct page_copy), GFP_KERNEL);
	return kmem_cache_alloc(page_copy_slab_cache, GFP_KERNEL);
}

//free memory occupied by page copy 
void scorw_free_page_copy(struct page_copy *page_copy)
{
	//printk("Inside scorw_free_page_copy\n");
	//return kfree(page_copy);
	return kmem_cache_free(page_copy_slab_cache, page_copy);
}



//add a page copy into linked list of page copy's
void scorw_add_page_copy_llist(struct page_copy *page_copy)
{
	INIT_LIST_HEAD(&(page_copy->ll_node));
	list_add_tail(&(page_copy->ll_node), &page_copy_llist);
}

//remove a page copy from linked list of page copy's
void scorw_remove_page_copy_llist(struct page_copy *page_copy)
{
	list_del(&(page_copy->ll_node));
}

//print page copy linked list
void scorw_print_page_copy_llist(void)
{
	struct list_head *head;
	struct list_head *curr;
	struct page_copy *curr_page_copy;
	head = &page_copy_llist;

	list_for_each(curr, head)
	{
		curr_page_copy = list_entry(curr, struct page_copy, ll_node);
		printk("scorw_print_page_copy_llist: block_num: %u, parent inode num: %lu", curr_page_copy->block_num, curr_page_copy->par->i_ino_num);
	}
}


void scorw_add_page_copy_hlist(struct page_copy *page_copy)
{
	hash_add(page_copy_hlist, &(page_copy->h_node), (page_copy->block_num));
}

void scorw_remove_page_copy_hlist(struct page_copy *page_copy)
{
	hash_del(&(page_copy->h_node));
}

void scorw_print_page_copy_hlist(void)
{
    	struct page_copy *cur;
	int bkt; //bucket

	hash_for_each(page_copy_hlist, bkt, cur, h_node)
	{
		printk("scorw_print_page_copy_hlist: block_num= %u, bucket= %d, parent inode num: %lu\n" , cur->block_num, bkt, cur->par->i_ino_num);
	}
	
}

//We use only block number as hash key. (However, for unique identification of a page copy, we need block number, 
//parent's inode num, target info.)
//
//We have control over how to search the page copy of our interest. hash_for_each_possible() searches for all 
//page copy's mapped to a same linked list.  (Hash table is made up of linked lists.)
//
//When this linked list is scanned, we can put condition that the matching page copy entry 
//should have block number, parent inode number, target child of our interest.
struct page_copy *scorw_find_page_copy(unsigned block_num, unsigned long par_ino_num, int child_index)
{
	struct page_copy *cur;

	//printk("scorw_find_page_copy_hlist: Looking for entry with block_num= %u, parent inode num: %lu, child_index: %d\n" , block_num, par_ino_num, child_index);
	if(child_index == -1)
	{
		//Consider the case when child is open and par isn't open.
		//In this case, child_index of child will be -1.
		//When it tries to do page copy lookup, it should fail.
		//This is because, page copy structs can only exist if a par exists
		return NULL;
	}
	BUG_ON(!(child_index>=0 &&  child_index<SCORW_MAX_CHILDS));
	read_lock(&(page_copy_lock));	
	hash_for_each_possible(page_copy_hlist, cur, h_node, block_num)
	{
		//printk("scorw_find_page_copy_hlist: current page copy, block_num= %u, parent inode num: %lu, child_index: %d, is_target_child: %d\n" , cur->block_num, cur->par->i_ino_num, child_index, cur->is_target_child[child_index]);
		if((block_num == cur->block_num) && (par_ino_num == cur->par->i_ino_num) && (cur->is_target_child[child_index]))
		{
			//printk("scorw_find_page_copy_hlist: match found!\n");
			read_unlock(&(page_copy_lock));	
			return cur;
		}
	}
	read_unlock(&(page_copy_lock));	
	//printk("scorw_find_page_copy_hlist: No match found!\n");

	return NULL;
}

int scorw_copy_page_to_page_copy(struct page_copy *page_copy, int is_4KB_write, struct page* par_data_page)
{
	void *kaddr_r = 0;
	void *kaddr_w = 0;

	//printk("scorw_copy_page_to_page_copy: Inside this function\n");

	//page up to date means hot cache scenario
	//page not up to date and is_4KB_write = 0 means cold cache with <4KB write 
	if(PageUptodate(par_data_page) || (!is_4KB_write))
	{
		//printk("scorw_copy_page_to_page_copy: page corresponding block: %u exists in page cache either due to hot cache or <4KB cold cache scenario\n", page_copy->block_num);
		page_copy->data_page = alloc_pages(GFP_KERNEL, 0);
		BUG_ON(page_copy->data_page == NULL);

		kaddr_r = kmap_atomic(par_data_page);		//par_data_page's get_page is done in ext4_da_write_begin 
								//and its put_page will be done in ext4_da_write_end
		kaddr_w = kmap_atomic(page_copy->data_page);

		copy_page(kaddr_w, kaddr_r);

		kunmap_atomic(kaddr_r);
		kunmap_atomic(kaddr_w);

		page_copy->data_page_loaded = 1;
	}

	return 0;
}

void scorw_unprepare_page_copy(struct page_copy *page_copy)
{
	if(page_copy->data_page != 0)
	{
		__free_pages(page_copy->data_page, 0);
	}

	write_lock(&(page_copy_lock));
	scorw_remove_page_copy_llist(page_copy);
	scorw_remove_page_copy_hlist(page_copy);
	write_unlock(&(page_copy_lock));	

	//Parent file can be closed before all the page structs have been processed.
	//Thus, to trigger the freeing of vfs inodes of par, child, frnd inodes, it is essential that
	//ref count of scorw inode is continously checked to evaluate whether the ref count has become 0.
	//On refcount of scorw inode becoming zero, iput() the vfs inodes.
	//Hence, scorw_dec_process_usage_count is insufficient here and we have to fallback to scorw_put_inode()
	//
	//scorw_dec_process_usage_count(page_copy->par);
	scorw_put_inode(page_copy->par->i_vfs_inode, 0, 1, 0);
}	


int scorw_prepare_page_copy(unsigned block_num, struct scorw_inode *par_scorw_inode, struct page_copy *page_copy, int is_4KB_write, struct page* par_data_page, unsigned char *is_target_child)
{
	int i = 0;

	//printk("Inside scorw_prepare_page_copy\n");
	page_copy->block_num = block_num;
	page_copy->par = par_scorw_inode;
	page_copy->data_page_loaded = 0;
	page_copy->data_page = 0;

	scorw_copy_page_to_page_copy(page_copy, is_4KB_write, par_data_page);

	//parent scorw inode should not get freed until page copy struct exists
	//So, increase reference count of parent scorw inode
	//scorw_inc_process_usage_count(par_scorw_inode);
	++par_scorw_inode->added_to_page_copy;
	//printk("%s(): added page copy structure. Total page copy structures added till now: %lu\n", __func__, par_scorw_inode->added_to_page_copy);
	for(i=0; i<SCORW_MAX_CHILDS; i++)
	{
		if(i <= par_scorw_inode->i_last_child_index)
		{
			page_copy->is_target_child[i] = is_target_child[i];
		}
		else
		{
			page_copy->is_target_child[i] = 0;
		}
		//printk("scorw_prepare_page_copy: page_copy->is_target_child[%d]: %u\n", i, page_copy->is_target_child[i]);
	}

	write_lock(&(page_copy_lock));	
	scorw_add_page_copy_llist(page_copy);
	scorw_add_page_copy_hlist(page_copy);
	write_unlock(&(page_copy_lock));	

	return 0;
}

//==================================================================================





//increment usage count of scorw inode
void scorw_inc_process_usage_count(struct scorw_inode *scorw_inode)
{
	//printk("scorw_inc_process_usage_count called\n");

	//printk("scorw_inc_process_usage_count: Before usage_count: %d\n", scorw_inode->i_usage_count);
	//atomic64_inc(&(scorw_inode->i_process_usage_count));
	//printk("scorw_inc_process_usage_count: After usage_count: %d\n", scorw_inode->i_usage_count);
	spin_lock(&(scorw_inode->i_process_usage_count_lock));
	++(scorw_inode->i_process_usage_count);
	spin_unlock(&(scorw_inode->i_process_usage_count_lock));

}

unsigned long scorw_get_process_usage_count(struct scorw_inode *scorw_inode)
{
	//printk("scorw_get_process_usage_count called\n");
	unsigned long usage_count = 0;

	spin_lock(&(scorw_inode->i_process_usage_count_lock));
	usage_count = scorw_inode->i_process_usage_count;
	spin_unlock(&(scorw_inode->i_process_usage_count_lock));

	return usage_count;
}

//decrement usage count of scorw inode
void scorw_dec_process_usage_count(struct scorw_inode *scorw_inode)
{
	//printk("scorw_dec_process_usage_count called\n");
	//if(atomic64_read(&(scorw_inode->i_process_usage_count)) > 0)
	//	atomic64_dec(&(scorw_inode->i_process_usage_count));
	spin_lock(&(scorw_inode->i_process_usage_count_lock));
	BUG_ON(scorw_inode->i_process_usage_count <= 0);
	--(scorw_inode->i_process_usage_count);
	spin_unlock(&(scorw_inode->i_process_usage_count_lock));

}


//increment thread usage count of scorw inode
void scorw_inc_thread_usage_count(struct scorw_inode *scorw_inode)
{
	//printk("scorw_inc_thread_usage_count called\n");
	atomic64_inc(&(scorw_inode->i_thread_usage_count));

}

//get thread usage count of scorw inode
unsigned long scorw_get_thread_usage_count(struct scorw_inode *scorw_inode)
{
	//printk("scorw_get_thread_usage_count called\n");
	return atomic64_read(&(scorw_inode->i_thread_usage_count));
}

//decrement thread usage count of scorw inode
void scorw_dec_thread_usage_count(struct scorw_inode *scorw_inode)
{
	//printk("scorw_dec_thread_usage_count called\n");
	if(atomic64_read(&(scorw_inode->i_thread_usage_count)) > 0)
		atomic64_dec(&(scorw_inode->i_thread_usage_count));

}



//takes arguments, child scorw inode, logical block corresponding which copy has to happen
//and pointer to address space of parent from which copy has to happen to child
//returns error on failure and 0 on success
int scorw_copy_page(struct scorw_inode *scorw_inode, loff_t lblk, struct address_space *p_mapping)
{
        //struct address_space *par_mapping = NULL;
        struct address_space *child_mapping = NULL;
        struct page *page = NULL;
        struct page *page_w = NULL;
        void *fsdata = 0;
        int error = 0;
	int i = 0;
	unsigned long len;	/* In native ext4, len means bytes to write to page */
	unsigned long copied;	/* In native ext4, copied means bytes copied from user */
				/* In our case, len == copied */


	//parent's page 
	page = scorw_get_page(scorw_inode->i_par_vfs_inode, lblk);
/*
	//printk("scorw_copy_page: Reading 1 page of parent\n");
	par_mapping = p_mapping;
	//printk("scorw_copy_page: par_mapping: %x, page index: %lu\n", par_mapping, lblk);

	//returns a locked page
	page = find_or_create_page(par_mapping, lblk, mapping_gfp_mask(par_mapping));
	if(page == NULL)
	{
		printk("Error!! Failed to allocate page. Out of memory!!\n");
		return SCORW_OUT_OF_MEMORY;
	}


	//If parent's page is already in cache, don't read it from disk
	if(PageUptodate(page))
	{
		unlock_page(page);
	}
	else
	{
		//printk("scorw_copy_page: Reading parent's page into pagecache\n");
		//parent's page is not in cache, read it from disk
		//calling ext4_readpage. ext4_readpage calls ext4_mpage_readpages internally.
		error = par_mapping->a_ops->readpage(NULL, page);
		if(error)
		{
			printk("scorw_copy_page: Error while Reading parent's page into pagecache\n");
			put_page(page);
			return error;
		}      

		//Looks like this is reqd. to make sure that read operation completes
		//i.e. block until read completes. 
		//printk("scorw_copy_page: PageUptodate(read page): %d\n", PageUptodate(page));
		if(!PageUptodate(page))
		{
			//printk("scorw_copy_page: locking read page.\n"); 
			lock_page_killable(page);

			//printk("scorw_copy_page: unlocking read page\n"); 
			unlock_page(page);
		}
	}
*/

	//printk("scorw_copy_page: obtaining child mapping\n"); 
	child_mapping = scorw_inode->i_vfs_inode->i_mapping;


	len = (PAGE_SIZE <= (scorw_inode->i_copy_size - (lblk << PAGE_SHIFT)) ? PAGE_SIZE: (scorw_inode->i_copy_size - (lblk << PAGE_SHIFT)));
	copied = len;
	//printk("scorw_copy_page: Amount of data copied/written to page: %lu\n", copied); 
	
	//printk("scorw_copy_page: calling ext4_da_write_begin\n"); 
	
	error = ext4_da_write_begin(NULL, child_mapping, ((unsigned long)lblk) << PAGE_SHIFT, len, 0, &page_w, &fsdata);
	if(page_w != NULL)
	{
		//printk("scorw_copy_page: Page allocated by ext4_da_write_begin()\n");

		//printk("scorw_copy_page: Mapping parent's page using kmap_atomic\n");
		//printk("scorw_copy_page: Mapping child's page using kmap_atomic\n");
		void *kaddr = kmap_atomic(page);
		void *kaddr_w = kmap_atomic(page_w);

		//printk("page[0]: %d\n", *((char*)kaddr + 0));
		//printk("page[1]: %d\n", *((char*)kaddr + 1));
		//printk("page[2]: %d\n", *((char*)kaddr + 2));
		//printk("page[3]: %d\n", *((char*)kaddr + 3));
		//printk("page[4095]: %d\n", *((char*)kaddr + 4095));
		copy_page(kaddr_w, kaddr);

		for(i = copied; i < PAGE_SIZE; i++)
		{
			*((char*)kaddr_w + i) = '\0';
		}

		//printk("scorw_copy_page: unmapping parent's page using kunmap_atomic\n");
		//printk("scorw_copy_page: unmapping child's page using kunmap_atomic\n");
		kunmap_atomic(kaddr);
		kunmap_atomic(kaddr_w);

		//printk("scorw_copy_page: calling ext4_da_write_end\n"); 
		ext4_da_write_end(NULL, child_mapping , ((unsigned long)lblk) << PAGE_SHIFT, len, copied, page_w, fsdata);
		//printk("scorw_copy_page: returning from ext4_da_write_end\n"); 

		balance_dirty_pages_ratelimited(child_mapping);

	}
	else
	{
		printk("scorw_copy_page: Error in ext4_da_write_begin\n");
		scorw_put_page(page);

		return error;
	}
	//printk("scorw_copy_page: put_page() called\n");
	scorw_put_page(page);


	return 0;

}

//page copy and child scorw inode passed
int scorw_copy_page_from_page_copy(struct page_copy *page_copy, struct scorw_inode *scorw_inode)
{

        struct address_space *child_mapping = NULL;
        struct page *page_w = NULL;
        void *fsdata = 0;
	void *kaddr_w = 0;
	void *kaddr_r = 0;
        int error = 0;
	int i = 0;
	int cold_load = 0;	//Tells whether block_read_full_page() was called or not
	unsigned lblk = 0;
	unsigned long len;	// In native ext4, len means bytes to write to page 
	unsigned long copied;	// In native ext4, copied means bytes copied from user 
				// In our case, len == copied 
	
	//printk("scorw_copy_page_from_page_copy: Inside this fn\n");
	
	child_mapping = scorw_inode->i_vfs_inode->i_mapping;
	lblk = page_copy->block_num;

	len = (PAGE_SIZE <= (scorw_inode->i_copy_size - (lblk << PAGE_SHIFT)) ? PAGE_SIZE: (scorw_inode->i_copy_size - (lblk << PAGE_SHIFT)));
	copied = len;

	if(len <= 0)
	{
		return 0;
	}

	
	//read page of parent from disk 
	//Note: Ideally, need to use lock here to avoid the race condition when page copy thread is processing a page copy struct and another code path (say read from child) 
	//is also calling this function
	//For example: Read of child happens and this child (say child 4 of parent) wants to read from page copy. Simultaneously, page copy thread is processing 
	//child 1 of parent.
	//Both find data_page_loaded to be 0 and both should not start reading from disk.
	//
	//Update: lock_page(data_page) below can help to resolve this problem
	if(page_copy->data_page_loaded == 0)
	{
		page_copy->data_page = alloc_pages(GFP_KERNEL, 0);
		BUG_ON(page_copy->data_page == NULL);

		page_copy->data_page->mapping = scorw_inode->i_par_vfs_inode->i_mapping;
		page_copy->data_page->index = page_copy->block_num;

		/*
		kaddr_r = kmap_atomic(page_copy->data_page);
		*((char*)kaddr_r + 0) = 'x';
		*((char*)kaddr_r + 1) = 'y';
		*((char*)kaddr_r + 2) = 'z';
		*((char*)kaddr_r + 3) = 'A';
		printk("[pid: %u]scorw_copy_page_from_page_copy: block: %u, (writing to data page before reading block contents from disk) kaddr_r: %c%c%c%c\n", current->pid, page_copy->block_num, *((char*)kaddr_r + 0), *((char*) kaddr_r + 1), *((char*)kaddr_r + 2), *((char*)kaddr_r + 3));
		kunmap_atomic(kaddr_r);
		*/
		//printk("scorw_copy_page_from_page_copy: block: %u being read from disk\n", page_copy->block_num);
		
		lock_page(page_copy->data_page);	
		block_read_full_page(page_copy->data_page, ext4_get_block);	//Looks like page is unlocked within this function
		wait_on_page_locked(page_copy->data_page);			//Update: page is unlocked on end-io
		BUG_ON(!PageUptodate(page_copy->data_page));
		page_copy->data_page_loaded = 1;	
		cold_load = 1;
		/*
		kaddr_r = kmap_atomic(page_copy->data_page);
		printk("scorw_copy_page_from_page_copy: block: %u being read from disk. First 4 bytes: %c%c%c%c\n", page_copy->block_num, *((char*)kaddr_r + 0), *((char*)kaddr_r + 1), *((char*)kaddr_r + 2), *((char*)kaddr_r + 3));
		kunmap_atomic(kaddr_r);
		*/
	}

	//write page contents
	//printk("scorw_copy_page_from_page_copy: writing page contents\n");
	error = ext4_da_write_begin(NULL, child_mapping, ((unsigned long)lblk) << PAGE_SHIFT, len, 0, &page_w, &fsdata);
	if(page_w != NULL)
	{
		//printk("scorw_copy_page_from_page_copy: ext4_da_write_begin returned successfully\n");
		kaddr_r = kmap_atomic(page_copy->data_page);
		kaddr_w = kmap_atomic(page_w);

		copy_page(kaddr_w, kaddr_r);
		for(i = copied; i < PAGE_SIZE; i++)
		{
			*((char*)kaddr_w + i) = '\0';
		}

		kunmap_atomic(kaddr_r);
		kunmap_atomic(kaddr_w);
	

		//printk("scorw_copy_page_from_page_copy: Before calling ext4_da_write_end called, copied: %d, PageDirty(page_w): %d\n", copied, PageDirty(page_w));
		ext4_da_write_end(NULL, child_mapping , ((unsigned long)lblk) << PAGE_SHIFT, len, copied, page_w, fsdata);
		//printk("scorw_copy_page_from_page_copy: After calling ext4_da_write_end called, copied: %d, PageDirty(page_w): %d\n", copied, PageDirty(page_w));
		//printk("scorw_copy_page_from_page_copy: balance_dirty_pages_ratelimited called\n");
		balance_dirty_pages_ratelimited(child_mapping);
	}
	else
	{
		printk("scorw_copy_page_from_page_copy: Error in ext4_da_write_begin\n");
		if(cold_load)
		{
			lock_page(page_copy->data_page);	
			try_to_free_buffers(page_copy->data_page);
			unlock_page(page_copy->data_page);	
		}
		page_copy->data_page->mapping = NULL;
		return error;
	}
	if(cold_load)
	{
		lock_page(page_copy->data_page);	
		try_to_free_buffers(page_copy->data_page);
		unlock_page(page_copy->data_page);	
	}
	page_copy->data_page->mapping = NULL;
	//printk("scorw_copy_page_from_page_copy: Returning from this fn\n");

	return 0;
}

//
//* 	This function returns an scorw inode with incremented reference count corresponding a vfs inode. If scorw inode already exists, its ref count is incremented
//	else it is newly created, ref count incremented and returned.
//* 	This function internally holds scorw_lock. Hence, this lock should not be held by calling function
//	while calling this function
//* 	In case of child, its scorw inode is created only.
//*	In cae of parent, its dependent scorw inodes are also implicitly created.
//
//Arguments:
//
//inode: 		vfs inode corresponding which scorw inode needs to be found/created
//is_child_inode: 	flag that tells whether scorw inode of child file or parent file is getting created.
//new_sparse: 		flag used in case child file's scorw inode is getting created. It tells whether scorw inode of a new file or existing file
//			is getting created.	[Update: This flag is deprecated]
//
struct scorw_inode* scorw_get_inode(struct inode *inode, int is_child_inode, int new_sparse)
{
	int i;
	unsigned long c_ino_num;
        struct scorw_inode *scorw_inode;
        struct scorw_inode *c_scorw_inode;
	struct inode *c_inode;


	//printk("1.Inside scorw_get_inode, inode: %lu, is_child_inode: %d\n", inode->i_ino, is_child_inode);
	mutex_lock(&(inode->i_vfs_inode_lock));

	//read_lock(&scorw_lock);
	//scorw_inode = scorw_search_inode_list(inode->i_ino);
	//read_unlock(&scorw_lock);
	scorw_inode = inode->i_scorw_inode;

	if(scorw_inode != NULL)
	{
		//If a child inode has been unlinked, scorw_get_inode can be called on it only when par is being opened. 
		//Since, child is unlinked, don't open it anymore and don't attach the scorw inode of child
		//to par.
		if(is_child_inode && scorw_inode->i_ino_unlinked)
		{
			//printk("scorw_get_inode, child inode: %lu has been unlinked. Won't be attached with par\n", inode->i_ino);
			mutex_unlock(&(inode->i_vfs_inode_lock));
			return 0;
		}
		//printk("2.scorw_get_inode: Already there is a scorw inode corresponding inode: %lu\n", inode->i_ino);
		//increment usage count of already existing scorw inode
		scorw_inc_process_usage_count(scorw_inode);
	}
	else
	{

		//printk("3.scorw_get_inode: No. Already there is not a scorw inode corresponding inode: %lu\n", inode->i_ino);
		//printk("4.scorw_get_inode: Inserting new scorw inode\n");

		scorw_inode = scorw_alloc_inode();
		if(scorw_inode != NULL)
		{
			init_rwsem(&(scorw_inode->i_lock));
			spin_lock_init(&(scorw_inode->i_process_usage_count_lock));
			scorw_inode->added_to_page_copy = 0;
			scorw_inode->removed_from_page_copy = 0;
			if(is_child_inode)
			{
				//printk("5.scorw_get_inode: Calling scorw_prepare_child_inode\n");
				scorw_prepare_child_inode(scorw_inode, inode, new_sparse);
				if(scorw_thread)
				{
					//increment usage count of scorw inode on the behalf of kernel thread 
					//printk("scorw_get_inode: Incrementing usage count\n");
					scorw_inc_thread_usage_count(scorw_inode);

					//Increase ref count of inode. This will be decreased by thread when 
					//file has been deleted. If file has been deleted, scorw inode and vfs inode both should 
					//exist so that if thread had already selected this scorw inode to process, then it can complete
					//that process and then stop using it,
					ext4_iget(inode->i_sb, inode->i_ino, EXT4_IGET_NORMAL);
					
					//do all changes needed for inode policy to work as required when a new scorw inode is added.
					inode_policy->inode_added(inode_policy, scorw_inode);

				}

			}
			else
			{
				//printk("6.scorw_get_inode: Calling scorw_prepare_par_inode\n");
				scorw_prepare_par_inode(scorw_inode, inode);
			}

			//increment usage count of scorw inode created
			//This refers to the usage on behalf of process of which this scorw inode is getting created
			//printk("7.scorw_get_inode: Incrementing usage count\n");
			scorw_inc_process_usage_count(scorw_inode);

			//attach newly created scorw inode to the corresponding vfs inode
			inode->i_scorw_inode = scorw_inode;

			if(!is_child_inode)
			{
				//printk("scorw_get_inode: Parent will create scorw inodes of children now\n");
				for(i = 0; i < SCORW_MAX_CHILDS; i++)
				{
					c_ino_num = scorw_get_child_i_attr_val(inode, i);	
					if(c_ino_num)
					{
						//printk("scorw_get_inode: child %d, inode num: %lu\n", i, c_ino_num);
						c_inode = ext4_iget(inode->i_sb, c_ino_num, EXT4_IGET_NORMAL);
						if(IS_ERR_VALUE(c_inode) || !scorw_is_child_file(c_inode, 1))
						{
							printk("%s(): Possibly parent contains dangling extended attribute about a deleted child!\n", __func__);
							printk("%s(): Deleted this extended attribute..\n", __func__);
							scorw_remove_child_i_attr(inode, i);
							continue;
						}
						c_scorw_inode = scorw_get_inode(c_inode, 1, 0);
						//printk("scorw_get_inode: scorw inode of child %d, inode num: %lu, got created\n", i, c_ino_num);
						scorw_inode->i_child_scorw_inode[i] = c_scorw_inode;
						if(c_scorw_inode)
						{
							c_scorw_inode->i_at_index = i;
							c_inode->i_scorw_inode = c_scorw_inode;
						}
						scorw_inode->i_last_child_index = i;
						iput(c_inode);
					}
					else
					{
						scorw_inode->i_child_scorw_inode[i] = NULL;
					}
				}
			}

			//add scorw inode into list of scorw inodes
			//printk("8.scorw_get_inode: Adding to inode list\n");
			//This lock is required to prevent search operations from occuring in parallel while the insertion of a new
			//node takes place in the list
			//
			//This list is useful when scorw copy thread has to pick an scorw inode to process
			write_lock(&scorw_lock);
			scorw_add_inode_list(scorw_inode);
			write_unlock(&scorw_lock);
		}
		else
		{
			mutex_unlock(&(inode->i_vfs_inode_lock));
			printk(KERN_ERR "scorw_get_inode: SCORW_OUT_OF_MEMORY: Releasing vfs inode lock\n");
			return 0;
		}
	}
	//printk("scorw_get_inode: releasing vfs inode lock\n");
	mutex_unlock(&(inode->i_vfs_inode_lock));

	//printk("9.scorw_get_inode: returning\n");

	return scorw_inode;
}


//This function is called while setting of 
//extended attributes.
//It attaches the new child to parent scorw inode
//if it (par scorw inode) exists. 
//
//This function is called while holding 3 parent vfs inode locks:
//	* i_vfs_inode_lock 
//	* i_vfs_inode_open_close_lock
//	* writeback_sync lock
//
//Args:
//	par_inode: vfs inode of the parent file corresponding which new child is being added
//	child_inode: vfs inode of the child file being created newly 
//	index: index at which child inode num is stored in parent's extended attributes
//
void special_open(struct inode *par_inode, struct inode *child_inode, int index)
{
	int i = 0;
	int n = 0;
	struct scorw_inode *par_scorw_inode = 0;
	struct scorw_inode *child_scorw_inode = 0;

	//printk("Inside %s(). par: %lu, child: %lu, index: %d\n", __func__, par_inode->i_ino, child_inode->i_ino, index);
	//par exists?
	//read_lock(&scorw_lock);
	//par_scorw_inode = scorw_search_inode_list(par_inode->i_ino);
	//read_unlock(&scorw_lock);
	par_scorw_inode = par_inode->i_scorw_inode;
	if(par_scorw_inode)
	{
		//create child scorw inode
		//printk("%s(): par: %lu scorw exists\n", __func__, par_inode->i_ino);
		child_scorw_inode = scorw_get_inode(child_inode, 1, 0);
		if(child_scorw_inode)
		{
			//attach child scorw inode to par
			//printk("%s(): child: %lu is open\n", __func__, child_inode->i_ino);
			down_write(&(par_scorw_inode->i_lock));
			par_scorw_inode->i_child_scorw_inode[index] = child_scorw_inode;
			child_scorw_inode->i_at_index = index;
			if(index > par_scorw_inode->i_last_child_index)
			{
				par_scorw_inode->i_last_child_index = index;
			}
			up_write(&(par_scorw_inode->i_lock));
			//printk("%s(): Attached child: %lu to par: %lu at index: %d\n", __func__, child_inode->i_ino, par_inode->i_ino, index);

			//Our setxattr utility is as following:
			//	open(child)
			//	open(parent)
			//	setxattr()	//create parent child relationship. Inside this we call get_inode to create child scorw inode and attach this scorw inode to parent
			//	close(child)	//However, due to this close(), child scorw inode that we just created, will get closed.
			//			//Hence, we are making an additional get_inode call that will complement this close i.e. put_inode
			//			//and first get_inode will be closed when close(parent) is called.
			//	close(parent)
			scorw_get_inode(child_inode, 1, 0);
		}
		else
		{
			//Par exists but child doesn't exists
			BUG_ON(par_scorw_inode);
		}
	}
	else
	{
		//printk("%s(): par: %lu scorw does not exists. Parent open() count: %d\n", __func__, par_inode->i_ino, atomic_read(&(par_inode->i_vfs_inode_open_count)));
		if(atomic_read(&(par_inode->i_vfs_inode_open_count)) > 1)
		{
			//printk("%s(): par: %lu scorw does not exists. But par file is open.\n", __func__, par_inode->i_ino);

			//This lock is meant to freeze the existance of par scorw inode i.e. prevent opening/closing of par scorw inode in parallel
			//Eventhough we are unlocking it here (because get_inode will need to acquire this lock), par scorw inode creation, removal is still
			//frozen due to i_vfs_inode_open_close_lock lock
			//
			mutex_unlock(&(par_inode->i_vfs_inode_lock));
			n = atomic_read(&(par_inode->i_vfs_inode_open_count));
			//open par scorw inode as many times as par file is open
			for(i=0 ; i < n; i++)
			{
				scorw_get_inode(par_inode, 0, 0);
			}
			mutex_lock(&(par_inode->i_vfs_inode_lock));

			//Our setxattr utility is as following:
			//	open(child)
			//	open(parent)
			//	setxattr()	//create parent child relationship. Inside this we call get_inode to create par scorw inode and its children's scorw inodes 
			//	close(child)	//However, due to this close(), child scorw inode that we just created, will get closed.
			//			//Hence, we are making an additional get_inode call for child that will complement this close i.e. put_inode
			//	close(parent)
			scorw_get_inode(child_inode, 1, 0);
		}
		else
		{
			//Our setxattr utility is the only entity that has opened the par file
			//It will be immediately closed. So, no need to create scorw inodes.
			//
			//printk("%s(): par: %lu scorw does not exists and par file is not open. Doing nothing.\n", __func__, par_inode->i_ino);
		}
	}
}
 


//Checks whether all blocks have been copied from parent to child or not.
//If all blocks have been copied, do cleanup
//Assumes,child scorw_inode's lock is held by calling function
//Argument:
//
//scorw inode whose completion of copying needs to be checked
void scorw_child_copy_completion_cleanup(struct scorw_inode *scorw_inode)
{
	//printk("Inside scorw_child_copy_completion_cleanup, scorw_inode inode num: %lu\n", scorw_inode->i_ino_num);
	//printk("scorw_child_copy_completion_cleanup: scorw_inode->i_pending_copy_pages: %u\n", scorw_inode->i_pending_copy_pages);

	//End parent child relationship
	if((atomic64_read(&(scorw_inode->i_pending_copy_pages)) == 0) && ((scorw_inode->i_frnd_vfs_inode->i_state & I_DIRTY_PAGES) == 0))
	{
		//Note/Todo:
		//Consider a situation where, during write to parent, write operation happens on all parent blocks.
		//When fio has completed, page copy can be still processing the queued blocks to be processed.
		//Now consider the case when page copy also has finished its processing.
		//In this scenario we expect that cleanup of extended attributes of par,child and friend files can be done.
		//
		//However, there's a catch. Eventhough page copy thread has finished, in writeback path, still writeback
		//has to rely on inode numbers of par/child/frnd for writing back data blocks.
		//So, if we cleanup extended attributes + scorw inodes, writeback path's attempt to get inode numbers (from extended attributes)
		//will fail i.e. 0 will be returned by API's when writeback path tries to read extended attributes of par, child, frnd files.
		//
		//Sample bug: EXT4-fs error (device vdb1): writeback_sb_inodes:1966: comm kworker/u64:2: inode #0: comm kworker/u64:2: iget: illegal inode #
		//(This bug came because extended attributes got cleaned up and when writeback path tried to find the inode number of frnd file, it got 0 as
		//return value (i.e. extended attribute doesn't exists).
		//
		//So, temporarily stopping the cleanup of extended attributes until a better way to handle this is found.
		//
	
		/*	
		//printk("scorw_child_copy_completion_cleanup: All data has been copied from parent to child. Doing cleanup\n");
		//remove parent attributes maintained by child inode
		scorw_remove_par_attr(scorw_inode->i_vfs_inode);

		//remove child attributes maintained by parent inode
		scorw_remove_child_attr(scorw_inode->i_par_vfs_inode, scorw_inode->i_ino_num);

		//remove child attributes maintained by friend inode
		scorw_remove_child_friend_attr(scorw_inode->i_frnd_vfs_inode);
		*/
	}
	else if(atomic64_read(&(scorw_inode->i_pending_copy_pages)) > 0)
	{
		//save info about count of blocks yet to be copied to disk
		scorw_set_blocks_to_copy_attr_val(scorw_inode->i_vfs_inode, atomic64_read(&(scorw_inode->i_pending_copy_pages))); 
	}
}


//Given a vfs inode, decrement its reference count. If ref count is 0, free scorw inode.
//In case of child, only its own ref count is decremented and scorw is freed if reqd.
//In case of parent, ref count of parent and its dependent child scorw inodes is decremented.
//
//Locking of scorw inodes list and scorw inode being processed is being internally by this function and calling
//function shouldn't hold these locks.
//
//Argument:
//inode: vfs inode corresponding which scorw inode's ref count needs to be decremented.
//is_child_inode: flag that tells whether parent inode being put or child inode.
//is_thread_putting: who is calling this function? Is it a kernel thread or execution context on the behalf of a process
//is_par_putting: Is parent putting inode or the child itself i.e. parent is getting freed and has recursively tried to free child?
//
int scorw_put_inode(struct inode *inode, int is_child_inode, int is_thread_putting, int is_par_putting)
{
	int i = 0;
	int j = 0;
	int q = 0;
	unsigned long c_ino_num = 0;
        struct scorw_inode *scorw_inode = NULL;
        struct scorw_inode *c_scorw_inode = NULL;
        struct scorw_inode *p_scorw_inode = NULL;
	struct list_head *head;
	struct list_head *curr;
	struct page_copy *curr_page_copy;

        //printk("1.scorw_put_inode called for inode: %lu\n", inode->i_ino);
	//If this is the last reference to child scorw inode, we will be inserting 
	//this child's info into the list to be processed by asynch thread
	//for updation of frnd version count for recovery.
	//
	//This list is protected by the following (frnd_version_cnt_lock) lock
	//
	//Ordering of the locks is as following:
	//	frnd_version_cnt_lock
	//		i_vfs_inode_lock
	//
	if(is_child_inode)
	{
		mutex_lock(&(frnd_version_cnt_lock));
	}
	mutex_lock(&(inode->i_vfs_inode_lock));

        //find scorw inode corresponding vfs inode
	//read_lock(&scorw_lock);
        //scorw_inode = scorw_search_inode_list(inode->i_ino);
	//read_unlock(&scorw_lock);
	scorw_inode = inode->i_scorw_inode;
        if(scorw_inode == NULL)
        {
                //printk("2.scorw_put_inode: scorw_inode is not present corresponding this inode. Releasing vfs inode lock.\n");
		mutex_unlock(&(inode->i_vfs_inode_lock));
		if(is_child_inode)
		{
			mutex_unlock(&(frnd_version_cnt_lock));
		}
		return 0;
        }
	//printk("3.scorw_put_inode: scorw_inode is present corresponding this inode\n");
	//decrease usage count of scorw inode
	if(is_thread_putting)
	{
		++(scorw_inode->removed_from_page_copy);
		//printk("3.scorw_put_inode: thread putting scorw inode. page copy structures processed: %lu\n", scorw_inode->removed_from_page_copy);
	}
	else
	{
		scorw_dec_process_usage_count(scorw_inode);
		//printk("4.scorw_put_inode: process putting scorw inode. updated usage count: %lu\n", scorw_get_process_usage_count(scorw_inode));
	}

	//printk("5.scorw_put_inode: process usage count is: %ld\n", scorw_get_process_usage_count(scorw_inode));
	//printk("6.scorw_put_inode: thread usage count is: %ld\n", scorw_get_thread_usage_count(scorw_inode));
	////printk("scorw_put_inode: uncopied blocks count is: %d\n", scorw_get_uncopied_blocks_count(scorw_inode));
	

	//free scorw inode if its overall usage count is 0
	//Note 1:
	//	* Different cases for blocks in page copy queue:
	//		- Parent hasn't called put inode but page copy thread has already processed all page copy structures related to parent
	//			+ In this case, following if condition will be false because process usage count is not zero
	//
	//		- Parent has called put inode and page copy thread has already processed all page copy structures related to parent
	//			+ In this case, following if condition will be true
	//
	//		- Parent has called put inode but page copy thread hasn't processed all page copy structures related to parent
	//			+ In this case, following if condition will be false because second condition will be false
	//
	//		- Parent has called put inode and page copy thread later processes all page copy structures related to parent
	//			+ In this case, following if condition will be true
	//Note 2:
	//	* process usage count should be checked first and only when process usage count becomes 0, page copy counters should
	//	  be checked because, process usage count being zero suggests that parent has closed
	//	  and now 'added to page copy counter won't change'.
	//
	if((scorw_get_process_usage_count(scorw_inode) == 0) && (scorw_inode->added_to_page_copy == scorw_inode->removed_from_page_copy))
	{
		//printk("scorw_put_inode: inode: %lu, process and thread usage count is 0\n", scorw_inode->i_ino_num);
		if(is_child_inode)
		{
			//child file has been unlinked
			//Perform cleanup of parent-child relationship
			if(scorw_inode->i_ino_unlinked)
			{
				//On unlinking of child file, delete it as target
				//from page copy structs
				if(scorw_inode->i_at_index != -1)
				{
					read_lock(&(page_copy_lock));
					head = &page_copy_llist;
					list_for_each(curr, head)
					{
						curr_page_copy = list_entry(curr, struct page_copy, ll_node);
						curr_page_copy->is_target_child[scorw_inode->i_at_index] = 0;
						//printk("scorw_put_inode: block_num: %u, parent inode num: %lu", curr_page_copy->block_num, curr_page_copy->par->i_ino_num);
					}
					read_unlock(&(page_copy_lock));
				}

				//cleanup extended attributes irrespective of parent being open or not
				for(j = 0; j < SCORW_MAX_CHILDS; j++)
				{
					c_ino_num = scorw_get_child_i_attr_val(scorw_inode->i_par_vfs_inode, j);
					if(c_ino_num == inode->i_ino)
					{
						//printk("scorw_put_inode: child inode: %lu info removed from par %lu extended attributes\n", scorw_inode->i_ino_num, scorw_inode->i_par_vfs_inode->i_ino);
						scorw_remove_child_i_attr(scorw_inode->i_par_vfs_inode, j);
						break;
					}
				}
			}
			else
			{
				//printk("scorw_put_inode: child inode: %lu, child has not been unlinked\n", scorw_inode->i_ino_num);
			}

			//do cleanup on the basis of how much data has been copied from parent to child
			scorw_child_copy_completion_cleanup(scorw_inode);
			
			//printk("8.scorw_put_inode: unpreparing child inode: %lu.\n", scorw_inode->i_ino_num);
			//free fields of scorw inode
			scorw_unprepare_child_inode(scorw_inode);
		}

		//remove scorw inode from scorw inodes list
		//printk("10.scorw_put_inode: remove scorw inode from scorw inodes list: %lu\n", scorw_inode->i_ino_num);
		//This lock is required to prevent search operations from occuring in parallel while the deletion of a 
		//node takes place from the list
		write_lock(&scorw_lock);
		scorw_remove_inode_list(scorw_inode);
		write_unlock(&scorw_lock);

		//detach scorw inode from its vfs inode
		inode->i_scorw_inode = 0;

		//decrease ref count of child scorw inodes of parent also.
		if(!is_child_inode)
		{
			//printk("scorw_put_inode: putting parent scorw inode's children scorw inodes\n");
			//handle each child.
			for(i = 0; i < SCORW_MAX_CHILDS; i++)
			{
				c_scorw_inode = scorw_inode->i_child_scorw_inode[i];
				if(c_scorw_inode == NULL)
				{
					continue;
				}
				//printk("scorw_put_inode: child number: %d has inode num: %lu. Putting this child.\n", i, c_scorw_inode->i_ino_num);
				scorw_put_inode(c_scorw_inode->i_vfs_inode, 1, 0, 1);
			}

			//printk("9.scorw_put_inode: unpreparing parent scorw inode\n");
			//free fields of scorw inode
			scorw_unprepare_par_inode(scorw_inode);
		}

		//free memory occupied by scorw inode
		//printk("scorw_put_inode: freeing memory occupied by scorw inode\n");
		scorw_free_inode(scorw_inode);

	}
	//printk("scorw_put_inode: Releasing vfs inode lock\n");
	mutex_unlock(&(inode->i_vfs_inode_lock));
	if(is_child_inode)
	{
		mutex_unlock(&(frnd_version_cnt_lock));
	}

        //printk("11.scorw_put_inode returning after processing inode: %lu\n", inode->i_ino);

	return 0;
}



//given a block range (start, end), copy these blocks from parent to child
//scorw inode of child is passed in args
int scorw_copy_blocks(struct scorw_inode *scorw_inode, unsigned start_block, unsigned end_block)
{
	unsigned i = 0;
    	struct uncopied_block *uncopied_block = 0;
	struct page_copy *page_copy = 0;
	
	//printk("scorw_copy_blocks called\n");
	//printk("scorw_copy_blocks: child inode: %lu, start_block: %u, end_block: %u\n", scorw_inode->i_ino_num, start_block, end_block); 

	for(i = start_block; i <= end_block; i++)
	{
		//printk("scorw_copy_blocks: child inode: %lu, start_block: %u, i: %u, end_block: %u\n", scorw_inode->i_ino_num, start_block, i, end_block);
		uncopied_block = scorw_get_uncopied_block(scorw_inode, i, COPYING);
		if(!scorw_is_block_copied(scorw_inode->i_frnd_vfs_inode, i))
		{
			//mutex_lock(&page_copy_lock);
			page_copy = scorw_find_page_copy(i, scorw_inode->i_par_vfs_inode->i_ino, scorw_inode->i_at_index);
			if(page_copy != NULL)	//page copy exists
			{
				//Block hasn't been alraedy copied to child. This, implies that it is safe to copy block from page copy.
				//printk("scorw_copy_blocks: block i: %u will be copied from page copy\n", i);
				scorw_copy_page_from_page_copy(page_copy, scorw_inode);	
			}
			else	//page copy does not exist
			{
				//Block has not been already copied. Page copy doesn't exists. Only possible if page copy was never created. So, read from parent.
				//copy page from parent
				//printk("scorw_copy_blocks: block i: %u will be copied from parent\n", i);
				scorw_copy_page(scorw_inode, i, scorw_inode->i_par_vfs_inode->i_mapping);
			}
			//mutex_unlock(&page_copy_lock);

			//printk("(Before)scorw_copy_blocks: scorw_is_block_copied(scorw_inode: %lu, block_num: %u): %d\n", scorw_inode->i_ino_num, i, scorw_is_block_copied(scorw_inode, i));
			scorw_set_block_copied(scorw_inode, i);
			//printk("(After)scorw_copy_blocks: scorw_is_block_copied(scorw_inode: %lu, block_num: %u): %d\n", scorw_inode->i_ino_num, i, scorw_is_block_copied(scorw_inode, i));
			
			//decrement count of remaining blocks to be copied
			//printk("(Before)scorw_copy_blocks: blocks pending to be copied(scorw_inode: %lu): %d\n", scorw_inode->i_ino_num, scorw_inode->i_pending_copy_pages);
			scorw_dec_yet_to_copy_blocks_count(scorw_inode, 1);
			//printk("(After)scorw_copy_blocks: blocks pending to be copied(scorw_inode: %lu): %d\n", scorw_inode->i_ino_num, scorw_inode->i_pending_copy_pages);
		}
		scorw_put_uncopied_block(scorw_inode, i, COPYING, uncopied_block);
		scorw_remove_uncopied_block(scorw_inode, i, uncopied_block);
	}
	//printk("scorw_copy_blocks returning\n");

	return 0;
}

#ifdef USE_OLD_RANGE
int scorw_write_child_blocks_end(struct inode* inode, loff_t offset, size_t len, struct uncopied_block *uncopied_block)
#else
int scorw_write_child_blocks_end(struct inode* inode, loff_t offset, size_t len, struct uncopied_block *uncopied_block, bool shared)
#endif
{
        struct scorw_inode *scorw_inode = NULL;
        unsigned start_block = 0;
        unsigned end_block = 0;
        unsigned last_block_eligible_for_copy = 0;
        unsigned first_blk_num = 0;
        unsigned last_blk_num = 0;
	int i = 0;

        scorw_inode = scorw_find_inode(inode);
	BUG_ON(!scorw_inode || scorw_inode->i_vfs_inode != inode);

	//printk("scorw_write_child_blocks_end called\n");
	first_blk_num = (offset >> PAGE_SHIFT);
	last_blk_num = ((offset+len-1)>>PAGE_SHIFT);
	last_block_eligible_for_copy  = ((scorw_inode->i_copy_size-1) >> PAGE_SHIFT);
	start_block = first_blk_num;
	end_block = (last_blk_num < last_block_eligible_for_copy ?  last_blk_num : last_block_eligible_for_copy);
	//printk("scorw_write_child_blocks_end: start_block: %u, end_block: %u\n", start_block, end_block);


	//This write is purely append operation. Nothing to be done by us.
	if(start_block > last_block_eligible_for_copy)
	{
		//printk("scorw_write_child_blocks_end: This portion of write is purely append operation. Nothing to be done by us.\n");
		return 0;
	}


	for(i = start_block; i <= end_block; )
	{

		//printk("scorw_write_child_blocks_end: start_block: %u, i: %u, end_block: %u\n", start_block, i, end_block);
		if(scorw_is_block_copied(scorw_inode->i_frnd_vfs_inode, i))
		{
			//printk("scorw_write_child_blocks_end: Not copying block: %u because it is already copied\n", i);
			i++;
			continue;
		}

#ifndef USE_OLD_RANGE
		if(!shared)
#endif
		scorw_set_block_copied(scorw_inode, i);
		//printk("scorw_write_child_blocks_end: checking whether block is set as copied: %d\n", (scorw_is_block_copied(scorw_inode, i)));

		//decrement count of remaining blocks to be copied
		//printk("scorw_write_child_blocks_end: Before: scorw_inode->i_pending_copy_pages: %u\n", scorw_inode->i_pending_copy_pages);
		scorw_dec_yet_to_copy_blocks_count(scorw_inode, 1);
		//printk("scorw_write_child_blocks_end: After: scorw_inode->i_pending_copy_pages: %u\n", scorw_inode->i_pending_copy_pages);
		

		scorw_put_uncopied_block(scorw_inode, i, COPYING, uncopied_block);
		scorw_remove_uncopied_block(scorw_inode, i, uncopied_block);

		i++;
	}

	return 0;
}


#ifndef USE_OLD_RANGE
//Returns the snapx write behavior for a child inode
static int snapx_get_range_info(struct child_range **cr, struct scorw_inode *scorw_inode, unsigned blk_num)
{

	//printk("%s(): checking clone behaviour for blk_num: %u\n", __func__, blk_num);
	if(!scorw_inode->i_num_ranges)
	{
		//printk("%s(): clone behaviour: CoW\n", __func__);
		return SNAPX_FLAG_COW;
	}


	if(*cr == NULL || (*cr)->end < blk_num){ //No prev history, or stale history
             int i;
	     for(i=0; i < scorw_inode->i_num_ranges; i++){
		     if((scorw_inode->i_range[i].start <= blk_num) && (blk_num <= scorw_inode->i_range[i].end)){ 
			     *cr = &scorw_inode->i_range[i];
			     break;
		     }
	     }
	}
	if(*cr == NULL) //Its the default behavior
	{
		//printk("%s(): clone behaviour: CoW\n", __func__);
             	return SNAPX_FLAG_COW;
	}
	//printk("%s(): clone behaviour: %d\n", __func__, (*cr)->snapx_behavior);
        return (*cr)->snapx_behavior; 
}
#endif


//Increment child version count
//Args:
//	inode: child inode whose version count has to be incremented
void scorw_child_version_cnt_inc(struct inode *inode)
{
	unsigned long child_version_val = 0;
	struct wait_for_commit *wait_for_commit = 0;

	if(atomic_xchg(&(inode->i_cannot_update_child_version_cnt), 1) == 0)
	{
		//A freshly created frnd inode will have sync frnd flag as 0.
		//We know, this flag in frnd is allowed to have only 2 values: {-1, 1}
		//At this point, frnd is not dirty. But after this step (write to child)
		//frnd will become dirty.
		//We are initializing this flag to -1 to indicate that
		//this frnd inode is currently not allowed to sync
		inode->i_scorw_inode->i_frnd_vfs_inode->i_can_sync_frnd = -1;


		//update child version cnt
		child_version_val = scorw_get_child_version_attr_val(inode);
		BUG_ON(child_version_val == 0);

		++child_version_val;

		scorw_set_child_version_attr_val(inode, child_version_val);

		//queue frnd info for setting of flag that allows frnd inode/blks syncing
		wait_for_commit = scorw_alloc_wait_for_commit();
		BUG_ON(wait_for_commit == NULL);

		wait_for_commit->frnd = inode->i_scorw_inode->i_frnd_vfs_inode;
		wait_for_commit->child_staged_for_commit = 0;

		spin_lock(&commit_lock);
		scorw_add_wait_for_commit_list(wait_for_commit);
		spin_unlock(&commit_lock);
	}	
}

//Takes child inode, offset from where write has to happen and length of write.
//Does copy on write
//cow will happen in atmost 2 blocks. 
//Suppose write is happening at offset 10 to offset 100, then we need to cow only
//block 0
//If write is happening at offfset 10 to 5000, then we need to do cow for
//block 0 and block 1
//If write is happening at offfset 10 to 16000, then we need to do cow for
//block 0 and block 3
//If write is happening at offfset 0 to 16000, then we need to do cow for
//block 3
//If write is happening at offfset 0 to 16383, then we donot need to do cow
//
#ifdef USE_OLD_RANGE
int scorw_write_child_blocks_begin(struct inode* inode, loff_t offset, size_t len, void **ptr_uncopied_block)
#else
int scorw_write_child_blocks_begin(struct inode* inode, loff_t offset, size_t len, void **ptr_uncopied_block, struct sharing_range_info *shr_info)
#endif
{
	struct scorw_inode *scorw_inode = NULL;
	unsigned start_block = 0;
	unsigned end_block = 0;
	unsigned last_block_eligible_for_copy = 0;
	unsigned first_blk_num = 0;
	unsigned last_blk_num = 0;
	int i = 0;
	struct uncopied_block *uncopied_block = NULL;
	struct page_copy *page_copy = 0;
#ifndef USE_OLD_RANGE
	struct child_range *cr = NULL;
#endif

	scorw_inode = scorw_find_inode(inode);
	BUG_ON(!scorw_inode || scorw_inode->i_vfs_inode != inode);

	//first and last block (inclusive) covered by range i.e. offset, len passed to this function
	first_blk_num = (offset >> PAGE_SHIFT);
	last_blk_num = ((offset+len-1)>>PAGE_SHIFT);
	last_block_eligible_for_copy  = ((scorw_inode->i_copy_size-1) >> PAGE_SHIFT);
	//printk("scorw_write_child_blocks_begin: offset: %lld, offset+len: %lld\n", offset, offset+len);
	//printk("scorw_write_child_blocks_begin: first_blk_num: %u, last_blk_num: %u, last_block_eligible_for_copy: %u\n", first_blk_num, last_blk_num, last_block_eligible_for_copy);

	start_block = first_blk_num;
	end_block = (last_blk_num < last_block_eligible_for_copy ?  last_blk_num : last_block_eligible_for_copy);
	//printk("scorw_write_child_blocks_begin: start_block: %u, end_block: %u\n", start_block, end_block);


	//This write is purely append operation. Nothing to be done by us.
	if(start_block > last_block_eligible_for_copy)
	{
		//printk("scorw_write_child_blocks_begin: This portion of write is purely append operation. Nothing to be done by us. \n");
		return 0;
	}

	#ifndef USE_OLD_RANGE
        for(i= start_block; i<=end_block; ++i){
	     WriteCh snxb;	
	     if(!cr && i != start_block) 
	           break;	     
             snxb = snapx_get_range_info(&cr, scorw_inode, i);

	     //Check if we were handling a stream of shared mode write
	     if(snxb != SNAPX_FLAG_SHARED && shr_info->initialized){
		       WARN_ON(1);   //Mixed write is not handled. We will perform a partial write prep and go back
		       end_block = i -1;  
		       //printk("%s(): shared write\n", __func__);
		       break;
	     }

	     if(snxb == SNAPX_FLAG_COW_RO || snxb == SNAPX_FLAG_SEE_TH_RO || snxb == SNAPX_FLAG_SHARED_RO)
	     {
		     //printk("%s(): writing to read-only blk. Returning -1.\n", __func__);
		     return -1; 
	     }
             if(snxb == SNAPX_FLAG_SHARED) { // we need to maintain a shared range and handle it latter
		     if(i != start_block && !shr_info->initialized){
		         WARN_ON(1);   //Mixed write to CoW is not handled. We will perform a partial write prep and go back
			 end_block = i -1;  
			 shr_info->partial_cow = true;
			 shr_info->end_block = end_block;
			 shr_info->start_block = start_block;
		         //printk("%s(): AAAA \n", __func__);
			 break;
		     }else if(shr_info->initialized){
		         //printk("%s(): BBBB \n", __func__);
			 shr_info->end_block++;     
		     }else if(i == start_block){
			  BUG_ON(shr_info->initialized);   
			  shr_info->initialized = true;
		          shr_info->start_block = i;
		          shr_info->end_block = i;
		          //printk("%s(): CCCC \n", __func__);
		     }
	     }   
	}
        #endif


	for(i = start_block; i <= end_block; )
	{
		//printk("scorw_write_child_blocks_begin: calling scorw_get_uncopied_block\n");
		uncopied_block = scorw_get_uncopied_block(scorw_inode, i, COPYING);
		*ptr_uncopied_block = uncopied_block;	//This uncopied block will be passed to write_end fn
		//printk("scorw_write_child_blocks_begin: sizeof(uncopied_block): %d\n", sizeof(struct uncopied_block));
	
	#ifndef USE_OLD_RANGE
		//we don't perform cow for shared blks. We are just interested in acquiring active map locks
	        if(shr_info->initialized){
			i++;
			continue;
		}	
        #endif
		//printk("scorw_write_child_blocks_begin: start_block: %u, i: %u, end_block: %u\n", start_block, i, end_block);
		if(scorw_is_block_copied(scorw_inode->i_frnd_vfs_inode, i))
		{
			//printk("scorw_write_child_blocks_begin: Not copying block: %u because it is already copied\n", i);
			scorw_put_uncopied_block(scorw_inode, i, COPYING, uncopied_block);
			scorw_remove_uncopied_block(scorw_inode, i, uncopied_block);
			i++;
			continue;
		}

		//increment child version count on first write to a blk
		//that is not copied to child yet because only then
		//friend file is also modified
		scorw_child_version_cnt_inc(inode);

		if((start_block == end_block)  && (i == start_block) && ((offset%PAGE_SIZE) == 0) && (((offset+len)%PAGE_SIZE) == 0))
		{
			//Note: For such cases also, i_pending_copy_pages is decreased.
			//printk("scorw_write_child_blocks_begin: first block: %u is equal to end block: %u and it will be fully overwritten\n", start_block, end_block);
			i++;
			continue;
		}
		if((start_block != end_block) && (i == start_block) && ((offset%PAGE_SIZE) == 0))
		{
			//Note: For such cases also, i_pending_copy_pages is decreased.
			//printk("scorw_write_child_blocks_begin: Not copying first block: %u because it will be fully overwritten\n", i);
			i++;
			continue;
		}
		if((start_block != end_block) && (i == end_block) && (((offset+len)%PAGE_SIZE) == 0))
		{
			//Note: For such cases also, i_pending_copy_pages is decreased.
			//printk("scorw_write_child_blocks_begin: Not copying last block: %u because it will be fully overwritten\n", i);
			i++;
			continue;
		}
		if((i==start_block) || (i == end_block))
		{
			//printk("scorw_write_child_blocks_begin: block i: %u will be copied\n", i);
			
			page_copy = scorw_find_page_copy(i, scorw_inode->i_par_vfs_inode->i_ino, scorw_inode->i_at_index);
			if(page_copy != NULL)	//page copy exists
			{
				//Block hasn't been alraedy copied to child. This, implies that it is safe to copy block from page copy.
				//printk("scorw_write_child_blocks_begin: block i: %u will be copied from page copy\n", i);
				scorw_copy_page_from_page_copy(page_copy, scorw_inode);	
			}
			else	//page copy does not exist
			{
				//Block has not been already copied. Page copy doesn't exists. Only possible if page copy was never created. So, read from parent.
				//copy page from parent
				//printk("scorw_write_child_blocks_begin: block i: %u will be copied from parent\n", i);
				scorw_copy_page(scorw_inode, i, scorw_inode->i_par_vfs_inode->i_mapping);
			}
		}
	
		i++;
	}
	return 0;
}


//====================================
//Why read barrier logic is required?
//====================================
//Putting a barrier here so that read requests do not read modified data of parent. Instead they finish before this barrier.
//Consider a scenario when read request comes before the page copy is created (write to parent is happening in parallel).
//It will check that page copy doesn't exist and suppose it get scheduled out. If we don't add this barrier, then the 
//read operation will get scheduled. Assume that write to parent has completed by this time. Thus, it will read the modified 
//data from the parent.
//This barrier makes sure that read operation completes before this barrier itself. READING and COPYING_EXCL conflict with each other.
//So, COPYING_EXCL lock can be obtained only when READING lock is not there.
//
//All reads after the barrier will be read from child or page copy.
//
//=================================================================
//Why read barrier logic is split into two halves (begin and end)?
//=================================================================
//Done to fix following ordering deadlock in our older code when
//read barrier was called from within scorw_write_par, instead of the 
//approach used now:
//Assume one writer and one reader are writing/reading same file.
//If write parent comes first, it acquires lock on page 'p' being written (aops->write_begin), then it acquires
//Active map lock on this page 'p'
//On the other hand, reader acquires Active map lock on page 'p' first (scorw_follow_on_read())and then acquires
//page lock (generic_file_buffered_read()). Thus, this can lead to deadlock.
//
//So, we modified barrier logic
void scorw_read_barrier_begin(struct scorw_inode *p_scorw_inode, unsigned block_num, struct uncopied_block **uncopied_block)
{
	int j = 0;
	struct scorw_inode *c_scorw_inode = 0;

	//Note: we acquire following lock in barrier_begin()
	//	and release it in barrier_end(). This won't cause
	//	issue in case of multiple writers to parent
	//	because vfs inode serializes writers.
	//	Also, this won't cause stalling of open()
	//	of child files because this function is acquired
	//	and then released per page basis (see 
	//	scorw_generic_perform_write())
	down_read(&(p_scorw_inode->i_lock));
	for(j = 0; j <= p_scorw_inode->i_last_child_index; j++)
	{
		c_scorw_inode = p_scorw_inode->i_child_scorw_inode[j];
		if(c_scorw_inode == NULL)
		{
			continue;
		}

		//barrier should be set for a child file only if it is open
		//When a parent file is opened, reference count of child is also increased
		//If write on parent is happening (parent file is open) and ref count of child is 1, this implies
		//that child file is not open
		//printk("child %d, process ref count: %lu\n", j, c_scorw_inode->i_process_usage_count);
		uncopied_block[j] = 0;
		if(scorw_get_process_usage_count(c_scorw_inode) > 1)
		{
			uncopied_block[j] = scorw_get_uncopied_block(c_scorw_inode, block_num, COPYING_EXCL);
		}
	}	
}

//see comment above scorw_read_barrier_begin()
void scorw_read_barrier_end(struct scorw_inode *p_scorw_inode, unsigned block_num, struct uncopied_block **uncopied_block)
{
	int j = 0;
	struct scorw_inode *c_scorw_inode = 0;

	for(j = 0; j <= p_scorw_inode->i_last_child_index; j++)
	{
		c_scorw_inode = p_scorw_inode->i_child_scorw_inode[j];
		if(c_scorw_inode == NULL)
		{
			continue;
		}

		//see comment in scorw_read_barrier_begin()
		if(uncopied_block[j])
		{
			scorw_put_uncopied_block(c_scorw_inode, block_num, COPYING_EXCL, uncopied_block[j]);
			scorw_remove_uncopied_block(c_scorw_inode, block_num, uncopied_block[j]);
		}

	}	
	//This lock is acquired in scorw_read_barrier_begin()
	up_read(&(p_scorw_inode->i_lock));
}


//This function checks whether a given block is in the range of blocks of child
//corresponding which static snapshot of par is taken
//Args:
//	scorw_inode: 	child scorw inode
//	blk_num:	block number to check in range
static int scorw_is_in_range(struct scorw_inode *scorw_inode, unsigned blk_num)
{
	int i = 0;
#ifdef USE_OLD_RANGE 
	int in_range = 0;
#else
	int in_range = 1;
#endif
	for(i=0; i < scorw_inode->i_num_ranges; i++)
	{
		if((scorw_inode->i_range[i].start <= blk_num) && (blk_num <= scorw_inode->i_range[i].end))
		{
                        #ifdef USE_OLD_RANGE 
			     in_range = 1;
                        #else
			    if(scorw_inode->i_range[i].snapx_behavior > SNAPX_FLAG_COW_RO)
			           in_range = 0;	     
			#endif 
			break;
		}
	}
	return in_range;
}


//Assumes parent and child scorw inodes are already created.
//Called for write on every page
//parent inode is passed as arg to this function
//
//
//Regarding page copy:
//	There can be multiple page copy structs corresponding same block
//	in the page copy list.
//	For eg: Following code snippet can create multiple page copy structs
//		1) create child
//		2) write blk 0 of par
//		3) create new child
//		4) goto step 2)
//
//	However, we are making sure that a child will always be the target of only one page copy
//
int scorw_write_par_blocks(struct inode* inode, loff_t offset, size_t len, struct page* par_data_page)
{
	unsigned blk_num = 0;
	struct scorw_inode *p_scorw_inode = NULL;
	struct scorw_inode *c_scorw_inode = NULL;
	unsigned last_block_eligible_for_copy = 0;
	struct page_copy *page_copy = 0;
	struct page_copy *existing_page_copy = 0;
	int j = 0;
	int in_range = 0;
	int is_4KB_write = 0;
	int target_children_exists = 0;
	unsigned char is_target_child[SCORW_MAX_CHILDS];

	//printk("Inside scorw_write_par_blocks \n");
	if(len <= 0)
	{
		printk("scorw_write_par_blocks: length is 0. Thus, skipping write.\n");
		return 0;
	}

	blk_num = (offset >> PAGE_SHIFT);
	last_block_eligible_for_copy  = ((inode->i_size-1) >> PAGE_SHIFT);
	//printk("[pid: %u] Inside scorw_write_par_blocks: processing block: %u, parent inode: %lu.\n", current->pid, blk_num, inode->i_ino);

	//This write is purely append operation. Nothing to be done by us.
	if(blk_num > last_block_eligible_for_copy)
	{
		//printk("scorw_write_par_blocks: This portion of write is purely append operation. Nothing to be done by us.\n");
		return 0;
	}

	//find scorw inode corresponding the parent vfs inode
	p_scorw_inode = scorw_find_inode(inode);
	BUG_ON(!p_scorw_inode || p_scorw_inode->i_vfs_inode != inode);

	//Find target children (to which par contents needs to be copied)
	target_children_exists = 0;
	//printk("scorw_write_par_blocks: Finding target children to which par contents needs to be copied\n");
	for(j = 0; j <= p_scorw_inode->i_last_child_index; j++)
	{
		is_target_child[j] = 0;
		c_scorw_inode = p_scorw_inode->i_child_scorw_inode[j];
		if(c_scorw_inode == NULL)
		{
			continue;
		}
		//printk("**** Child %d, inode num: %lu ****\n", j, c_scorw_inode->i_ino_num);
		//This blk should not be copied to child if child's range doesn't
		//include this blk i.e. child doesn't want static snapshot of
		//par's contents
		in_range = scorw_is_in_range(c_scorw_inode, blk_num);   //Here non-zero in_range means it is a CoW
		if(!in_range)
		{
			//printk("scorw_write_par_blocks: block: %u, not in range of child: %d with inode num: %lu\n", blk_num, j, c_scorw_inode->i_ino_num);
			continue;
		}
		//existing page copy with child as target exists?
		//If existing page copy exists, it implies, static snapshot of this block
		//has already been created for this child.
		existing_page_copy = scorw_find_page_copy(blk_num, inode->i_ino, c_scorw_inode->i_at_index);
		if(existing_page_copy)
		{
			//printk("scorw_write_par_blocks: block: %u, existing page copy exists corressponding child: %d with inode num: %lu\n", blk_num, j, c_scorw_inode->i_ino_num);
			continue;
		}
		//if blk is already copied, nothing to do for this child
		if(scorw_is_block_copied(c_scorw_inode->i_frnd_vfs_inode, blk_num))
		{
			//printk("scorw_write_par_blocks: block: %u, blk is already copied corressponding child: %d with inode num: %lu\n", blk_num, j, c_scorw_inode->i_ino_num);
			continue;
		}
		is_target_child[j] = 1;
		target_children_exists = 1;
		//printk("scorw_write_par_blocks: block: %u, child: %d with inode num: %lu is a target child\n", blk_num, j, c_scorw_inode->i_ino_num);

	}	

	if(!target_children_exists)
	{
		//printk("scorw_write_par_blocks: block: %u, no target children exists\n", blk_num);
		return 0;
	}

	//printk("scorw_write_par_blocks: write on block: %u, offset: %lu, len: %lu\n", blk_num, offset, len);
	//check whether contents of entire block will be overwritten or not (4KB write or not)
	if((offset <= ((unsigned long)blk_num << PAGE_SHIFT)) && ((offset + len) >= (((unsigned long)blk_num << PAGE_SHIFT) + PAGE_SIZE)))
	{
		is_4KB_write = 1;
		//printk("scorw_write_par_blocks: write on block: %u is a 4KB write\n", blk_num);
	}

	//create page copy
	page_copy = scorw_alloc_page_copy();
	BUG_ON(page_copy == NULL);

	scorw_prepare_page_copy(blk_num, p_scorw_inode, page_copy, is_4KB_write, par_data_page, is_target_child);
	//printk("Printing page copy list\n");
	//scorw_print_page_copy_hlist();

	//printk("scorw_write_par_blocks: Waking up page copy thread\n");
	/* Let page copy thread wakeup be triggered due to timeout
	if(!page_copy_thread_running)
	{
		wake_up(&page_copy_thread_wq); 
	}
	*/
	
	return 0;
}


unsigned long long scorw_min(unsigned long long a, unsigned long long b)
{
	//printk("%s(), a: %llu, b: %llu, a < b? %d\n", __func__, a, b, a < b);
	return ((a < b) ? a : b);
}


ssize_t scorw_read_from_child(struct kiocb *iocb, struct iov_iter *to, unsigned batch_start_blk, unsigned batch_end_blk)
{
	ssize_t ret = 0;
	size_t old_to_count = 0;
	loff_t start = 0;
	loff_t end = 0;
	size_t expected_to_count = 0;


	//printk("Inside scorw_read_from_child\n");
	old_to_count = to->count;
	start = iocb->ki_pos;
	end = scorw_min((iocb->ki_pos + to->count) , ((unsigned long long)batch_end_blk << PAGE_SHIFT));
	to->count = end - start;	//read these many bytes
	expected_to_count = to->count;
	//printk("%s(), batch_end_blk: %u, old_to_count: %lu, start: %lu, end: %lu, to->count: %lu\n", __func__, batch_end_blk, old_to_count, start, end, to->count);

	ret = generic_file_read_iter(iocb, to);
	//printk("%s(), read %lu bytes\n", __func__, ret);
	BUG_ON(ret < 0);

	to->count = old_to_count - ret;
	//printk("%s(), updated to->count: %lu after read\n", __func__, to->count);

	//printk("scorw_read_from_child: Total return value: %lu\n", ret);
	return ret;
}

//Serve request from parent's page cache
ssize_t scorw_read_from_parent(struct scorw_inode *scorw_inode, struct kiocb *iocb, struct iov_iter *to, unsigned batch_start_blk, unsigned batch_end_blk)
{
	ssize_t ret = 0;
	size_t old_to_count = 0;
	size_t expected_to_count = 0;
	loff_t start = 0;
	loff_t end = 0;
	struct inode *p_inode;  	//owner inode of inode on which read operation is being done
	struct address_space *mapping;  //original address space mapping of vfs inode on which read is being done.

	//printk("scorw_read_from_parent: reading using parent's page cache\n");
	//save original mapping 
	mapping = iocb->ki_filp->f_mapping;

	//change mapping
	p_inode = scorw_inode->i_par_vfs_inode;
	iocb->ki_filp->f_mapping = p_inode->i_mapping;

	old_to_count = to->count;
	start = iocb->ki_pos;
	end = scorw_min((iocb->ki_pos + to->count), ((unsigned long long)batch_end_blk << PAGE_SHIFT));
	to->count = end - start;	//read these many bytes
	expected_to_count = to->count;
	//printk("[pid: %d] %s(), batch_end_blk: %u, old_to_count: %lu, start: %lu, end: %lu, iocb->ki_pos: %lu, to->count: %lu\n", current->pid, __func__, batch_end_blk, old_to_count, start, end, iocb->ki_pos, to->count);

	//read data from parents page cache and fill buffer
	//Note: internally, in functions s.a. ext4_readpage, ext4_mpage_readpages,
	//inode to which page belongs to is found using page->mapping->host.
	//Hence, if there is miss in page cache of parent, parents extents will be used to read this page.  
	ret = generic_file_read_iter(iocb, to);
	//printk("[pid: %d] %s(), read %ld bytes\n", current->pid, __func__, ret);
	//BUG_ON(ret < 0);
	if(ret > 0)
	{
		to->count = old_to_count - ret;
	}
	//printk("[pid: %d] %s(), updated to->count: %lu after read\n", current->pid, __func__, to->count);

	//restore original page cache mapping 
	iocb->ki_filp->f_mapping = mapping;

	return ret;
}


enum batching_type
{
	NOT_STARTED,
	PRESENT_IN_CHILD,
	PRESENT_IN_PARENT
};

ssize_t submit_read_request(struct scorw_inode* scorw_inode, struct kiocb *iocb, struct iov_iter *to, enum batching_type prev_batching_type, unsigned batch_start_blk, unsigned batch_end_blk)
{
	ssize_t ret = 0;        //return value

	if(prev_batching_type == PRESENT_IN_PARENT)
	{
		ret = scorw_read_from_parent(scorw_inode, iocb, to, batch_start_blk, batch_end_blk);
	}
	else if(prev_batching_type == PRESENT_IN_CHILD)
	{
		ret = scorw_read_from_child(iocb, to, batch_start_blk, batch_end_blk);
	}
	else
	{
		BUG();
	}

	return ret;
}

ssize_t scorw_follow_on_read_child_blocks(struct inode* inode, struct kiocb *iocb, struct iov_iter *to)
{
	int i = 0;
	ssize_t r = 0;        	//return value
	ssize_t ret = 0;        //return value
	unsigned curr_page = 0;
	unsigned first_page = 0;
	unsigned last_page = 0;
	unsigned last_block_eligible_for_copy = 0;
	unsigned scan_pages_till = 0;
	struct scorw_inode* scorw_inode = NULL;
	struct uncopied_block *uncopied_block = 0;
	struct page_copy *page_copy = 0;
	enum batching_type prev_batching_type = NOT_STARTED;
	enum batching_type batching_type = NOT_STARTED;
	unsigned batch_start_blk = 0;			//inclusive
	unsigned batch_end_blk = 0;			//not inclusive

	scorw_inode = scorw_find_inode(inode);
	BUG_ON(!scorw_inode || scorw_inode->i_vfs_inode != inode);

	//copy till page before last page.
	//Eg: ki_pos = 0, to->count = 4097, 
	//last_page = 2 because bytes 0 to 4095 of page 0, 4097th byte i.e. byte 0 of page 1 get written.
	first_page = iocb->ki_pos >> PAGE_SHIFT;
	last_page = (iocb->ki_pos + to->count + PAGE_SIZE-1) >> PAGE_SHIFT;				//not inclusive
	last_block_eligible_for_copy  = ((((long long)scorw_inode->i_copy_size)-1) >> PAGE_SHIFT);	//inclusive
	scan_pages_till = scorw_min(last_page-1, last_block_eligible_for_copy);


	//printk("\n\nInside scorw_follow_on_read_child_blocks\n");
	//printk("scorw_follow_on_read_child_blocks: first_page: %u, last_page: %u\n", first_page, last_page);
	
	//reading outside copy size	
	if(first_page > last_block_eligible_for_copy)
	{
		//printk("scorw_follow_on_read_child_blocks: reading outside copy size. returning SCORW_PERFORM_ORIG_READ\n");
		return SCORW_PERFORM_ORIG_READ;
	}

	//read originating within copy size 
	curr_page = first_page;
	batch_start_blk = curr_page;
	batch_end_blk = curr_page;
	while(curr_page <= scan_pages_till)
	{
		//printk("scorw_follow_on_read_child_blocks: Processing page num: %u. Acquiring READING lock\n", curr_page);
		uncopied_block = scorw_get_uncopied_block(scorw_inode, curr_page, READING);	
		//printk("scorw_follow_on_read_child_blocks: Processing page num: %u. curr_page: %u, scan_pages_till: %u, batch_start_blk: %u, batch_end_blk: %u. Acquiring READING lock\n", curr_page, curr_page, scan_pages_till, batch_start_blk, batch_end_blk);

		if(scorw_is_block_copied(scorw_inode->i_frnd_vfs_inode, curr_page))		
		{
			//read from child
			//printk("scorw_follow_on_read_child_blocks: Block is already copied to child. Will reading block (block num: %u) from child\n", curr_page);
			batching_type = PRESENT_IN_CHILD;

		}
		else	
		{
			page_copy = scorw_find_page_copy(curr_page, scorw_inode->i_par_vfs_inode->i_ino, scorw_inode->i_at_index);	
			if(page_copy != NULL)
			{
				//printk("scorw_follow_on_read_child_blocks: Block is in page copy. Will reading block (block num: %u) from child\n", curr_page);
				//let data in page copy be copied to child's page cache
				//Then, perform read from child's page cache
				//We are handling both subcases of 
				//	- (page_copy->data_page_loaded == 0)	and
				//	- (page_copy->data_page_loaded == 1)	
				//using this approach itself
				//
				//Note: we have acquired READING lock and page copy thread acquires COPY_EXCL lock
				//So, if we found a pagecopy, it is gauaranteed that it is use that copies it to the child
				//due to incompatibility of READING and COPY_EXCL lock 
				scorw_copy_page_from_page_copy(page_copy, scorw_inode);
				scorw_set_block_copied(scorw_inode, curr_page);
			
				//decrement count of remaining blocks to be copied
				scorw_dec_yet_to_copy_blocks_count(scorw_inode, 1);

				//read from child
				batching_type = PRESENT_IN_CHILD;
			}
			else
			{
				//read from parent
				batching_type = PRESENT_IN_PARENT;
				//printk("scorw_follow_on_read_child_blocks: Will be reading block (block num: %u) from parent\n", curr_page);
			}
		}
		++batch_end_blk;
		++curr_page;

		if(batching_type == prev_batching_type)
		{
			//printk("scorw_follow_on_read_child_blocks: batching_type == prev_batching_type == %d\n", batching_type);
			continue;
		}
		if(prev_batching_type == NOT_STARTED)
		{
			prev_batching_type = batching_type;
			//printk("scorw_follow_on_read_child_blocks: batching started from current block itself. Type of batching == %d\n", batching_type);
			continue;
		}
		if(batching_type != prev_batching_type)
		{
			//printk("scorw_follow_on_read_child_blocks: batching_type != prev_batching_type, batching_type: %d, prev_batching_type: %d \n", batching_type, prev_batching_type);

			//submit read request
			r = submit_read_request(scorw_inode, iocb, to, prev_batching_type, batch_start_blk, batch_end_blk - 1);
			if(r > 0)
			{
				ret += r;
			}

			//release blocks from READING state
			for(i = batch_start_blk; i < batch_end_blk - 1; i++)
			{
				scorw_put_uncopied_block(scorw_inode, i, READING, uncopied_block);		
				scorw_remove_uncopied_block(scorw_inode, i, uncopied_block); 			
				//printk("scorw_follow_on_read_child_blocks: Processing page num: %u. Released READING lock\n", i);
			}

			//error encountered, Stop reading further
			if(r <= 0)
			{
				return ret;
			}

			prev_batching_type = batching_type;
			batch_start_blk = batch_end_blk - 1;
		}
	}
	//printk("[pid: %lu]scorw_follow_on_read_child_blocks: batch_start_blk: %u, batch_end_blk: %u\n", current->pid, batch_start_blk, batch_end_blk);
	if(batch_end_blk > batch_start_blk)
	{
		//printk("[pid: %lu]scorw_follow_on_read_child_blocks: Last batch being processed. batching_type: %d, prev_batching_type: %d \n", current->pid, batching_type, prev_batching_type);

		//submit read request
		r = submit_read_request(scorw_inode, iocb, to, batching_type, batch_start_blk, batch_end_blk);
		//printk("[pid: %lu]scorw_follow_on_read_child_blocks: ret: %lu, batch_start_blk: %u, batch_end_blk: %u\n", current->pid, r, batch_start_blk, batch_end_blk);
		if(r > 0)
		{
			ret += r;
		}

		//release blocks from READING state
		for(i = batch_start_blk; i < batch_end_blk; i++)
		{
			//printk("[pid: %lu] scorw_follow_on_read_child_blocks: Processing page num: %u. Putting READING lock\n", current->pid, i);
			scorw_put_uncopied_block(scorw_inode, i, READING, uncopied_block);		
			//printk("[pid: %lu] scorw_follow_on_read_child_blocks: Processing page num: %u. Removing READING lock\n", current->pid, i);
			scorw_remove_uncopied_block(scorw_inode, i, uncopied_block); 			
			//printk("[pid: %lu] scorw_follow_on_read_child_blocks: Processing page num: %u. Removed READING lock\n", current->pid, i);
		}
		//printk("[pid: %lu]scorw_follow_on_read_child_blocks: done, batch_start_blk: %u, batch_end_blk: %u\n", current->pid, batch_start_blk, batch_end_blk);

		//error encountered, Stop reading further
		if(r <= 0)
		{
			return ret;
		}
	}

	//read originating outside copy size 
	//For example:
	//	If read(page 0,1,2,3,4,5) request comes s.t.
	//		first_page = 0
	//		last_page = 6
	//		last_block_eligible_for_copy = 5
	//	Nothing to read after page 5. 
	//	So, don't enter the 'if' condition
	if(last_page > (last_block_eligible_for_copy + 1))
	{
		do
		{
			r = scorw_read_from_child(iocb, to, batch_start_blk, last_page);
			//printk("scorw_follow_on_read_child_blocks: reading data outside copy range. Read: %ld bytes\n", r);
			if(r <= 0)	//Error or no more data in file
			{
				break;
			}
			ret += r;
		}while(1);
	}	

	//printk("scorw_follow_on_read_child_blocks: Read: %lu bytes in total\n", ret);
	return ret;
}




//Todo: Unlink associated frnd file on the deletion of child file
int scorw_unlink_child_file(struct inode *c_inode)
{
        int i = 0;
	struct inode *p_inode = 0;
	struct scorw_inode *c_scorw_inode = 0;
	struct scorw_inode *p_scorw_inode = 0;
        unsigned long c_ino_num = 0;
	unsigned long p_ino_num = 0;

	//printk("Inside %s(). child unlinked: %lu\n", __func__, c_inode->i_ino);

	//Freeze the existance of child and par inode.s i.e. prevent opening/closing of child inode in parallel
	mutex_lock(&(c_inode->i_vfs_inode_open_close_lock));

	p_ino_num = scorw_get_parent_attr_val(c_inode);
	p_inode = ext4_iget(c_inode->i_sb, p_ino_num, EXT4_IGET_NORMAL);
	if(IS_ERR_VALUE(p_inode))
	{
		printk("Error: scorw_unlink_child_file: p_inode: %lu (After iget)\n", (unsigned long)p_inode);
		mutex_unlock(&(c_inode->i_vfs_inode_open_close_lock));
		//iput(p_inode);	//Since, iget() failed, there should be no iput()
		return -1;
	}
	mutex_lock(&(p_inode->i_vfs_inode_open_close_lock));


	c_scorw_inode = c_inode->i_scorw_inode;
	p_scorw_inode = p_inode->i_scorw_inode;

	//child scorw inode exists
	//To keep things simple, let's perform cleanup when child scorw inode is closed
	//Note: This can be further optimized
	if(c_scorw_inode)
	{
		//mark child scorw inode as orphan
		//printk("%s(): marked child: %lu scorw as orphan\n", __func__, c_inode->i_ino);
		c_scorw_inode->i_ino_unlinked = 1;
	}
	else
	{
		//Neither child nor parent is open. Delete extended attributes related to child stored in parent.
		//printk("%s(): child: %lu scorw is not open. This implies neither child nor parent is open\n", __func__, c_inode->i_ino);
		for(i = 0; i < SCORW_MAX_CHILDS; i++)
		{
			c_ino_num = scorw_get_child_i_attr_val(p_inode, i);
			//printk("%s(): child attribute %d has inode num: %lu\n", __func__, i, c_ino_num);
			if(c_ino_num == c_inode->i_ino)
			{
				//printk("%s(): corresponding attribute in parent found. Removing this attribute.\n", __func__);
				scorw_remove_child_i_attr(p_inode, i);
				break;
			}
		}

	}
	mutex_unlock(&(p_inode->i_vfs_inode_open_close_lock));
	iput(p_inode);
	mutex_unlock(&(c_inode->i_vfs_inode_open_close_lock));

	return 0;
}

int scorw_unlink_par_file(struct inode *inode)
{

/*
        int i;
        struct inode *c_inode;
        struct scorw_inode *p_scorw_inode;
        struct scorw_inode *c_scorw_inode;
 
	//printk("scorw_unlink_par_file called\n");

	//printk("scorw_unlink_par_file: finding/creating scorw inode of parent file\n");
	p_scorw_inode = scorw_get_inode(inode, 0, 0);
	mutex_lock(&(p_scorw_inode->i_lock));

	
	//printk("scorw_unlink_par_file: Copying pending data from parent to children.\n");
	//printk("scorw_unlink_par_file: Removing parent's info saved in child inodes.\n");
	for(i = 0; i < SCORW_MAX_CHILDS; i++)
	{
		//printk("scorw_unlink_par_file: Processing child %d.\n", i);
		if((c_inode = p_scorw_inode->i_child_vfs_inode[i]) == NULL)
			continue;

		//printk("scorw_unlink_par_file: inode number of child %d: %lu.\n", i, c_inode->i_ino);
		c_scorw_inode = scorw_find_inode(c_inode);

		//Copy pending data from parent to children.
		//printk("scorw_unlink_par_file: Copying pending data from parent to child %d.\n", i);
		scorw_copy_blocks(c_scorw_inode, 0, ((c_scorw_inode->i_copy_size-1)/PAGE_SIZE));

		//Remove parents info saved in child inode's
		//printk("scorw_unlink_par_file: Removing parent's info saved in child %d's inodes.\n", i);
		scorw_remove_par_attr(c_inode);
	}
	mutex_unlock(&(p_scorw_inode->i_lock));

	//decrease ref count of parent scorw inode (and its childrens)
	scorw_put_inode(inode, 0, 0);
*/


	return 0;
}


//generate list of extents yet to be copied from parent to child inode
int scorw_round_robin_inode_added(struct inode_policy* ip, struct scorw_inode *scorw_inode)
{
	
	long long copy_size;	//total bytes to be copied
	unsigned num_blocks;	//num blocks to be copied
	unsigned i;
	//int lblk_copied;
        struct scorw_extent *ex = NULL;          //extent
	unsigned start_block_num;
	unsigned len;

	//printk("Inside scorw_round_robin_inode_added ############################################\n");

	copy_size = scorw_inode->i_copy_size;
	num_blocks = ((copy_size - 1) >> PAGE_SHIFT) + 1;	//assuming, copy_size > 0

	for(i = 0; i < num_blocks; i++)
	{
		//printk("scorw_round_robin_inode_added: Searching for extent corresponding block num: %u\n", i);
		//corresponding extent exists in parent?
		ex = scorw_find_extent(scorw_inode, i, 0);
		if(ex)
		{
			//printk("scorw_round_robin_inode_added: ext4_extent ex->ee_block: %u \n", ex->ee_block);
			//printk("scorw_round_robin_inode_added: ext4_extent ex->ee_len: %d \n", ex->ee_len);
			if(scorw_is_extent_copied(scorw_inode, ex->ee_block, ex->ee_len))
			{
				//printk("scorw_round_robin_inode_added: extent is already copied\n");
				i = ex->ee_block + ex->ee_len - 1;
				kfree(ex);
				continue;

			}
			//insert into uncopied extents list
			//printk("scorw_round_robin_inode_added: Extent %d/%d is yet to be copied completely\n", ex->ee_block, ex->ee_len);
			start_block_num = ex->ee_block;
			len = ex->ee_len;
			i = ex->ee_block + ex->ee_len - 1;

			//It is callers responsibility to free the memory occupied by this extent
                        kfree(ex);
		}
		else
		{
			//insert into uncopied extents list
			//printk("scorw_round_robin_inode_added: ext4_extent ex->ee_block: %u\n", i);
			//printk("scorw_round_robin_inode_added: ext4_extent ex->ee_len: 1\n");
			//printk("scorw_round_robin_inode_added: Extent %d/1 is yet to be copied completely\n", i);
			start_block_num = i;
			len = 1;	
		}

		mutex_lock(&(scorw_inode->i_uncopied_extents_list_lock));
		scorw_get_uncopied_extent(scorw_inode, start_block_num, len);
		mutex_unlock(&(scorw_inode->i_uncopied_extents_list_lock));
	}
	
	return 0;	
}

//called after scorw thread has been stopped
int scorw_round_robin_inode_removed(struct inode_policy* ip, struct scorw_inode *scorw_inode)
{
	//printk("Inside scorw_round_robin_inode_removed\n");

	//empty uncopied extents list
	//Useful in case when filesystem is unmounted and still some extents of current scorw inode remain to be processed
	mutex_lock(&(scorw_inode->i_uncopied_extents_list_lock));
	scorw_empty_uncopied_extent_list(scorw_inode);
	mutex_unlock(&(scorw_inode->i_uncopied_extents_list_lock));
	
	return 0;
}

struct scorw_inode *scorw_round_robin_inode_policy(struct inode_policy* ip)
{
	struct scorw_inode *head = 0;
	struct scorw_inode *selected_inode = 0;
	struct scorw_inode *last_selected_inode = 0;
	int last_selected_inode_known = 0;

	//printk("1.Inside round_robin_inode_policy\n");

	head = ip->private;	//scorw inode that was prev. processed by scorw thread
	
	/*
	if(head)
		printk("round_robin_inode_policy: prev. processed inode: %lu\n", head->i_ino_num);
	else
		printk("round_robin_inode_policy: prev. processed inode: NULL\n");
	*/
	


	//list is empty
	if(list_empty(&scorw_inodes_list))
	{
		//printk("round_robin_inode_policy: scorw inodes list is empty\n");
		return NULL;
	}

	//list is not empty
	while(1)
	{
		if((head == NULL) || (head == list_last_entry(&(scorw_inodes_list), struct scorw_inode, i_list)))
		{
			selected_inode = list_first_entry(&(scorw_inodes_list), struct scorw_inode, i_list);

			//this will help to know that we have iterated all scorw inodes and none of them needs to be processed by scorw_thread_fn()
			if(last_selected_inode_known == 0)
			{
				last_selected_inode = list_last_entry(&(scorw_inodes_list), struct scorw_inode, i_list);
				last_selected_inode_known = 1;
				//printk("round_robin_inode_policy: last_selected_inode: %lu\n", last_selected_inode->i_ino_num);
			}
		}
		else
		{
			selected_inode = (list_entry((head->i_list.next), struct scorw_inode, i_list));

			//this will help to know that we have iterated all scorw inodes and none of them needs to be processed by scorw_thread_fn()
			if(last_selected_inode_known == 0)
			{
				last_selected_inode = head;
				last_selected_inode_known = 1;
				//printk("round_robin_inode_policy: last_selected_inode: %lu\n", last_selected_inode->i_ino_num);
			}
		}
		head = selected_inode;
		//printk("round_robin_inode_policy: temp. selected inode: %lu\n", head->i_ino_num);


		if((!scorw_is_child_inode(selected_inode)) || (scorw_get_thread_usage_count(selected_inode) == 0))
		{
			//printk("round_robin_inode_policy: selected inode: %lu is either not child or its thread usage count is zero\n", head->i_ino_num);
			//printk("round_robin_inode_policy: selected inode: %lu scorw_is_child_inode(selected_inode): %d\n", head->i_ino_num, scorw_is_child_inode(selected_inode));
			//printk("round_robin_inode_policy: selected inode: %lu scorw_get_thread_usage_count(selected_inode): %d\n", head->i_ino_num, scorw_get_thread_usage_count(selected_inode));

			/*
			if(ip->private)
			{
				printk("round_robin_inode_policy: prev. inode num: %lu\n", ((struct scorw_inode*)ip->private)->i_ino_num);
			}
			*/

			//printk("round_robin_inode_policy: last_selected_inode: %lu\n", last_selected_inode->i_ino_num);
			//printk("round_robin_inode_policy: selected inode: %lu\n", selected_inode->i_ino_num);
			//printk("round_robin_inode_policy: selected inode == last_selected_inode: %lu\n", last_selected_inode == selected_inode);

			//checked all scorw inodes. No child with pending extents to be copied is present.
			if(selected_inode == last_selected_inode)
			{
				//printk("round_robin_inode_policy: No element in current scorw inodes list can be processed by scorw thread\n");
				return NULL;
			}
			continue;
		}
		break;
	}
	ip->private = selected_inode;
	//printk("round_robin_inode_policy: finally selected inode: %lu\n", selected_inode->i_ino_num);

	return selected_inode;
}
	

struct uncopied_extent *scorw_sequential_extent_policy(struct scorw_inode *scorw_inode, struct extent_policy* ep)
{
	struct list_head *curr = NULL;
	struct uncopied_extent *first_uncopied_extent = NULL;
	struct uncopied_extent *prev= NULL;


	//printk("Inside scorw_sequential_extent_policy\n");

	list_for_each(curr, &(scorw_inode->i_uncopied_extents_list))
	{
		first_uncopied_extent = list_entry(curr, struct uncopied_extent, list);
		//printk("scorw_sequential_extent_policy: curr_uncopied_extent->block: %u\n", first_uncopied_extent->start_block_num);
		//printk("scorw_sequential_extent_policy: curr_uncopied_extent->len: %u\n", first_uncopied_extent->len);

		if(prev)
		{
			//printk("scorw_sequential_extent_policy: prev extent is copied\n");
			mutex_lock(&(scorw_inode->i_uncopied_extents_list_lock));
			//printk("scorw_sequential_extent_policy: Before: Putting copied extent\n");
			scorw_put_uncopied_extent(scorw_inode, prev->start_block_num, prev->len);
			//printk("scorw_sequential_extent_policy: After: Putting copied extent\n");
			mutex_unlock(&(scorw_inode->i_uncopied_extents_list_lock));
		}
		if(scorw_is_extent_copied(scorw_inode, first_uncopied_extent->start_block_num, first_uncopied_extent->len))
		{
			//printk("scorw_sequential_extent_policy: extent is copied\n");
			prev = first_uncopied_extent;
			continue;
		}
		break;
	}

	return first_uncopied_extent;
}







int scorw_thread_fn(void *data)
{
	struct scorw_inode *scorw_inode;
	//struct scorw_inode *prev_scorw_inode;
	struct inode *c_inode;
	struct uncopied_extent *uncopied_extent;
	int all_copied;
 	unsigned extent_start_block_num;
 	unsigned extent_end_block_num;
	unsigned extent_len;
	int congested;

	//printk("Inside scorw_thread_fn\n");
	//msleep(60000);

	while (1)	
	{
		//scorw_print_inode_list();

		//printk("scorw_thread_fn: ============================================================\n");
		//printk("scorw_thread_fn: Hello from thread\n");
		//printk("scorw_thread_fn: ============================================================\n");
		if(kthread_should_stop())
		{
			break;
		}

		//find scorw inode corresponding child file to which data has to be copied
		//printk("scorw_thread_fn: selecting inode to process\n");
		read_lock(&scorw_lock);
		scorw_inode = inode_policy->select_inode(inode_policy);
		read_unlock(&scorw_lock);


		if(scorw_inode != NULL)
		{
			//printk("scorw_thread_fn: inode: %lu selected to process\n", scorw_inode->i_ino_num);
			//scorw_print_uncopied_extent_list(scorw_inode);
			/*
			if(scorw_inode->i_ino_unlinked == 1)
			{
				//printk("scorw_thread_fn: inode: %lu has been unlinked\n", scorw_inode->i_ino_num);
				//printk("1. scorw_thread_fn: inode: %lu, inode ref count: %u (Before scorw put inode)\n", scorw_inode->i_ino_num, scorw_inode->i_vfs_inode->i_count);
				scorw_put_inode(scorw_inode->i_vfs_inode, 1, 1);
				//printk("scorw_thread_fn: inode: %lu, inode ref count: %u (After scorw put inode)\n", scorw_inode->i_ino_num, scorw_inode->i_vfs_inode->i_count);
				continue;
			}
			*/

			//printk("scorw_thread_fn: Checking for congestion\n");
			c_inode = scorw_inode->i_vfs_inode;
			while((congested = bdi_rw_congested(inode_to_bdi(c_inode))))
			{
				printk("congested####################\n");
				//https://stackoverflow.com/questions/2731463/converting-jiffies-to-milli-seconds
				congestion_wait(BLK_RW_ASYNC, HZ/50);
			}

			//choose extent which needs to be copied to child file
			//printk("scorw_thread_fn: choosing extent which needs to be copied to child file\n");
			uncopied_extent = extent_policy->select_extent(scorw_inode, extent_policy);
			//printk("scorw_thread_fn: uncopied_extent: %lx\n", uncopied_extent);

			//do copying
			if(uncopied_extent != NULL)
			{
				extent_start_block_num = uncopied_extent->start_block_num;
				extent_len = uncopied_extent->len;
				extent_end_block_num = extent_start_block_num + extent_len - 1;
				//printk("scorw_thread_fn: extent start block num: %u, len: %lu, end block num: %u\n", extent_start_block_num, extent_len, extent_end_block_num);
				scorw_copy_blocks(scorw_inode, extent_start_block_num, extent_end_block_num);
			}
			else
			{
				//printk("scorw_thread_fn: all extents already copied\n");
			}

			//whether all blocks of parent file have been copied to child file or not
			all_copied = 0;
			if(atomic64_read(&(scorw_inode->i_pending_copy_pages)) == 0)
			{
				all_copied = 1;
			}
			//printk("scorw_thread_fn: all extents copied: %d\n", all_copied);

			//if all pages have been copied, decrease ref count of scorw inode. Do cleanup if reqd.
			if(all_copied)
			{
				//If inode getting put/deleted is the same inode that will be used in next round robin inode selection
				//then, it will lead to error because scorw inode would have been freed till then.
				//Hence, reset the scorw inode that will act as the head of the next selection of 
				//scorw inode in round robin manner.
				//Note: One approach is to consider 'prev. inode' as the inode corresponding which we will find the next inode in 
				//inode list. However, it is possible that prev. inode itself might get freed (for example: prev.'s thread count had become zero
				//but it was still in inode list because its process count was non-zero. It can get deleted when its process count becomes zero.
				//
				/*	
				if(list_first_entry(&(scorw_inodes_list), struct scorw_inode, i_list) == list_last_entry(&(scorw_inodes_list), struct scorw_inode, i_list))
				{
					//current scorw inode is the only entry in the linked list
					printk("scorw_thread_fn: current scorw inode is the only entry in the linked list\n");
					inode_policy->private = NULL;
				}
				else if(scorw_inode == list_first_entry(&(scorw_inodes_list), struct scorw_inode, i_list))
				{
					//current scorw inode is at the head of the list. So, prev. element will be at the end of the list
					printk("scorw_thread_fn: current scorw inode is at the head of the list. So, prev. element will be at the end of the list\n");
					inode_policy->private = list_last_entry(&(scorw_inodes_list), struct scorw_inode, i_list);
				}
				else
				{
					//current scorw inode is not at the head of the list.
					printk("scorw_thread_fn: current scorw inode is not at the head of the list\n");
					inode_policy->private = list_entry((scorw_inode->i_list.prev), struct scorw_inode, i_list);
				}
				*/
				inode_policy->private = NULL;	//Let search for scorw inode start from first inode itself (linear search).


				//processing to be done by thread on this scorw inode is complete
				scorw_put_inode(scorw_inode->i_vfs_inode, 1, 1, 0);
			}
		}
		else
		{
			//milli seconds sleep
			msleep(1000);
			//printk("scorw_thread_fn: No inode selected to process\n");
		}
		//msleep(1000);
	}
	printk("scorw_thread_fn: Thread stopping\n");
	//do_exit(0);


	return 0;
}

int scorw_thread_init(void)
{
	struct inode_policy *round_robin = 0;
	struct extent_policy *sequential = 0;

	//printk("Inside scorw_thread_init\n");

	if(scorw_thread)
	{
		printk("scorw_thread_init: Thread already exists\n");
		return -1;
	}

	//set default inode policy
	//printk("scorw_thread_init: Setting inode policy\n");
	round_robin = kzalloc( sizeof(struct inode_policy), GFP_KERNEL);
	round_robin->select_inode = scorw_round_robin_inode_policy;
	round_robin->inode_added = scorw_round_robin_inode_added;
	round_robin->inode_removed = scorw_round_robin_inode_removed;
	round_robin->private = NULL; 
	inode_policy = round_robin;

	//set default extent policy
	//printk("scorw_thread_init: Setting extent policy\n");
	sequential = kzalloc( sizeof(struct extent_policy), GFP_KERNEL);
	sequential->select_extent = scorw_sequential_extent_policy;
	sequential->private = "World!";
	extent_policy = sequential;

	//printk("scorw_thread_init: Creating Thread\n");
	scorw_thread = kthread_run(scorw_thread_fn, NULL, "scorw_thread");
	if (scorw_thread)
	{
		//printk("scorw_thread_init: Thread created successfully\n");
	}
	else
	{
		//printk("scorw_thread_init: Thread creation failed\n");
		kfree(inode_policy);
		kfree(extent_policy);
		return -1;
	}
	return 0;
}

int scorw_thread_exit(void)
{
	//printk("Inside scorw_thread_exit. Terminating Thread\n");
	if(scorw_thread)
	{
		//printk("scorw_thread_exit: Thread about to be terminated\n");
		kthread_stop(scorw_thread);
		//printk("scorw_thread_exit: Thread terminated\n");

		//Reminder: must be done before inode_policy is freed because presence of inode policy is needed in scorw_put_inode.
		//Todo: Where are remaining uncopied_extent structures getting freed?
		//
		scorw_thread_exit_cleanup();

		//printk("scorw_thread_exit: Freeing inode policy\n");
		kfree(inode_policy);
		inode_policy = 0;

		//printk("scorw_thread_exit: Freeing extent policy\n");
		kfree(extent_policy);
		extent_policy = 0;

		scorw_thread = 0;
	}
	else
	{
		//printk("scorw_thread_exit: No thread to terminate\n");
	}
	return 0;
}


struct page_copy *scorw_get_page_copy_to_process(void)
{
	//printk("Inside scorw_get_page_copy_to_process\n");
	if(list_empty(&page_copy_llist))
	{
		//printk("scorw_get_page_copy_to_process: list is empty. returning.\n");
		return NULL;
	}
	return list_first_entry(&(page_copy_llist), struct page_copy, ll_node);
}

void scorw_free_all_page_copy(void)
{
	struct page_copy *page_copy = 0;

	//printk("Inside scorw_free_all_page_copy\n");
	while(!list_empty(&page_copy_llist))
	{
		//Note: FS is unmounting and page copy thread has exited. So, no insertion/deletion will happen from this list anymore.
		page_copy = list_first_entry_or_null(&(page_copy_llist), struct page_copy, ll_node);
		if(!page_copy)
		{
			continue;
		}
		//printk("cleaning page copy corresponding block: %u, parent: %lu\n", page_copy->block_num, page_copy->par->i_ino_num);
		scorw_unprepare_page_copy(page_copy);
		scorw_free_page_copy(page_copy);
	}
}

//This function processes requests related to updation of frnd inodes version counts
//Args:
//	sync_child: How to handle a dirty child? Should it be synced now itself (or) should we
//		wait for it to be synced by writeback thread?
//		When this function is called at the time of unmounting, we sync child ourselves.
//		Otherwise, we wait for child to be synced by writeback thread.
//		
void scorw_process_pending_frnd_version_cnt_inc_list(int sync_child)
{
	struct pending_frnd_version_cnt_inc *request = NULL;
	struct pending_frnd_version_cnt_inc *tmp = NULL;
	unsigned long child_version_val = 0;
	unsigned long frnd_version_val = 0;
	struct inode *child = 0;
	struct inode *frnd = 0;
	static unsigned long iter_id = 1;

	//printk("%s(): Will process requests under id: %lu\n", __func__, iter_id);

	mutex_lock(&frnd_version_cnt_lock);
	list_for_each_entry_safe(request, tmp, &pending_frnd_version_cnt_inc_list, list)
	{
		//printk("%s(): Chosen request corresponding child: %lu, frnd: %lu. sync_child: %d\n", __func__, request->child->i_ino, request->frnd->i_ino, sync_child);
		//iterated through all requests. Some entries have been requeued.
		//Skipping processing them.
		if(request->iter_id == iter_id)
		{
			break;
		}
		child = request->child;
		frnd = request->frnd;
		request->iter_id = iter_id;

		//we want atomicity w.r.t. open()/close() of child file
		mutex_lock(&(child->i_vfs_inode_lock));

		if(sync_child)
		{
			//printk("%s(): will explicitly sync child before updating frnd inode\n", __func__);

			//Expecting that all files are closed before unmounting
			//Recall, otherwise, umount throws 'target busy' error
			BUG_ON(child->i_scorw_inode);

			//remove entry from list
			scorw_remove_pending_frnd_version_cnt_inc_list(request);	
			kfree(request);

			//sync child if dirty
			if(scorw_is_inode_dirty(child))
			{
				write_inode_now(child, 1);
			}

			//update frnd version count
			child_version_val = scorw_get_child_version_attr_val(child);
			BUG_ON(child_version_val == 0);
			frnd_version_val = child_version_val;
			scorw_set_frnd_version_attr_val(frnd, frnd_version_val);

			//mark frnd as dirty and sync it
			mark_inode_dirty(frnd);
			write_inode_now(frnd, 1);

			//reset flags related to version count management
			atomic_set(&child->i_cannot_update_child_version_cnt, 0);	//allow updation of child version count on next write to child 
			frnd->i_can_sync_frnd = -1;	//disallow syncing of frnd file
			mutex_unlock(&(child->i_vfs_inode_lock));
			iput(child);
			iput(frnd);
		}
		else
		{
			//printk("%s(): will NOT explicitly sync child before updating frnd inode\n", __func__);

			//child file is open
			if(child->i_scorw_inode)
			{
				//printk("%s(): child file is open. removing request from list\n", __func__);
				scorw_remove_pending_frnd_version_cnt_inc_list(request);	
				kfree(request);
				mutex_unlock(&(child->i_vfs_inode_lock));
				iput(child);
				iput(frnd);
			}
			//
			//child is dirty or child version count is not yet synced to disk
			else if(scorw_is_inode_dirty(child) || (frnd->i_can_sync_frnd != 1))
			{
				//requeue request at the tail of the list
				scorw_remove_pending_frnd_version_cnt_inc_list(request);	
				scorw_add_pending_frnd_version_cnt_inc_list(request);	
				mutex_unlock(&(child->i_vfs_inode_lock));
			}
			//All set. Frnd version count can be updated.
			else
			{
				//printk("%s(): child file is not open and child is not dirty. Updating and syncing frnd version count\n", __func__);
				//remove entry from list
				scorw_remove_pending_frnd_version_cnt_inc_list(request);	
				kfree(request);

				//update frnd version count
				child_version_val = scorw_get_child_version_attr_val(child);
				BUG_ON(child_version_val == 0);
				frnd_version_val = child_version_val;
				scorw_set_frnd_version_attr_val(frnd, frnd_version_val);

				//mark frnd as dirty and sync it
				mark_inode_dirty(frnd);
				write_inode_now(frnd, 1);

				//reset flags related to version count management
				atomic_set(&child->i_cannot_update_child_version_cnt, 0);	//allow updation of child version count on next write to child 
				frnd->i_can_sync_frnd = -1;	//disallow syncing of frnd file
				mutex_unlock(&(child->i_vfs_inode_lock));
				iput(child);
				iput(frnd);
			}
		}
	}
	mutex_unlock(&frnd_version_cnt_lock);
	//printk("%s(): Reached end of current iteration\n", __func__);
	++iter_id;
}

int scorw_page_copy_thread_fn(void *arg)
{
	struct page_copy *cur_page_copy = 0;
	struct scorw_inode *p_scorw_inode = 0;
	struct scorw_inode *c_scorw_inode = 0;
	struct uncopied_block *uncopied_block = 0;
	unsigned long last_process_time_jiffies =  0;
	unsigned long timeout_jiffies =  0;
	int block_num = 0;
	int j = 0;
	int ret = 0;

	timeout_jiffies = 5*HZ;
	last_process_time_jiffies =  jiffies;
	while(1)
	{
		//printk("Inside scorw_page_copy_thread_fn\n");
		
		//Todo: Ideally, before stopping this thread during unmount, make sure that all pending copy's are processed.
		//Because, when we do unmount, pending writes to parent (waiting for page copies to be applied to children)
		//will be committed to disk. So, parent's data will get overwritten. So, unless this data has been copied to children,
		//we will have correctness issue.
		//
		//update: Done. Check below.
		//update: Commented out the modifications to achieve above. So, that testing can complete faster.
		//
		//printk("scorw_page_copy_thread_fn: calling wait_event, stop_page_copy_thread: %d, list_empty(&page_copy_llist): %d\n", stop_page_copy_thread, list_empty(&page_copy_llist));
		//
		//Note: 
		//	* 1 Hz is equal to number of jiffies in one sec.
		//	* jiffies + x*Hz  means timeout of x sec from now
		wait_event_timeout(page_copy_thread_wq, (!list_empty(&page_copy_llist)) || (stop_page_copy_thread==1), timeout_jiffies);
		//printk("scorw_page_copy_thread_fn: Woke up from waiting, stop_page_copy_thread: %d, list_empty(&page_copy_llist): %d\n", stop_page_copy_thread, list_empty(&page_copy_llist));
		
		//waited long enough for processing requests waiting for 
		//updation of frnd version count
		if(time_after_eq(jiffies, last_process_time_jiffies + timeout_jiffies))
		{
			scorw_process_pending_frnd_version_cnt_inc_list(0);
			last_process_time_jiffies = jiffies;
			continue;
		}

		//If stop_page_copy_thread is set this implies unmount is in progress. So, no more writes will happen.
		//Unmount only when all page copies have been done
		//
		//if((stop_page_copy_thread == 1) && (list_empty(&page_copy_llist)))
		if((stop_page_copy_thread == 1))
		{
			//cleanup of page copy
			//printk("scorw_page_copy_thread_fn: Told to exit. Doing cleanup before unmount.\n");
			scorw_free_all_page_copy();
			printk("scorw_page_copy_thread_fn: Stopping page copy thread\n");
			break;
		}
		//printk("scorw_page_copy_thread_fn: Selecting page copy struct to process\n");
		
		//select page copy to process
		cur_page_copy = scorw_get_page_copy_to_process();
		//BUG_ON(cur_page_copy == NULL);
		if(cur_page_copy == NULL)
		{
			continue;
		}
		page_copy_thread_running = 1;
		
		//process selected page copy
		//printk("scorw_page_copy_thread_fn: copying block %u (parent inode: %lu), to all children\n", cur_page_copy->block_num, cur_page_copy->par->i_ino_num);
	
		//For each child, check if page is copied. If not, then copy it.
		p_scorw_inode = cur_page_copy->par;
		block_num = cur_page_copy->block_num;
		//printk("scorw_page_copy_thread_fn: trying down_read par i_lock. copying block %u (parent inode: %lu), to all children\n", cur_page_copy->block_num, cur_page_copy->par->i_ino_num);
		down_read(&(p_scorw_inode->i_lock));
		//printk("scorw_page_copy_thread_fn: down_read par i_lock succeeded. copying block %u (parent inode: %lu), to all children\n", cur_page_copy->block_num, cur_page_copy->par->i_ino_num);
		for(j = 0; j < SCORW_MAX_CHILDS; j++)
		{
			//pick a child
			c_scorw_inode = p_scorw_inode->i_child_scorw_inode[j];
			if((c_scorw_inode == NULL) || (cur_page_copy->is_target_child[j] == 0))
			{
				//printk("scorw_page_copy_thread_fn: Child: %d doesn't exist (or) child exists but isn't target of this page copy struct\n", j);
				continue;
			}

			//If number of blocks in child file are less than the block being processed, donot copy the content of this block to child
			if(((c_scorw_inode->i_copy_size - 1) >> PAGE_SHIFT) < block_num)
			{
				//printk("scorw_page_copy_thread_fn: Out of range block for child: %d, child inode: %lu. Skipping it!", j, c_scorw_inode->i_ino_num);
				continue;
			}

			//copy block to a child
			//Note: Copying from page copy to child has to happen in COPYING_EXCL mode.
			//Consider the case when read from child acquires READING lock and finds 
			//that page copy struct corresponding a block exists.
			//Now, assume that before it can read from page copy struct, it gets scheduled out. 
			//Now, page copy struct gets processed and deleted in this page copy thread.
			//When read from page copy struct resumes, it will fault because page copy struct doesn't exist any more.
			//Thus, page copying here should not be done with COPYING mode (which is compatible 
			//with READING mode but with COPYING_EXCL mode)	
			uncopied_block = scorw_get_uncopied_block(c_scorw_inode, block_num, COPYING_EXCL);
			if(!scorw_is_block_copied(c_scorw_inode->i_frnd_vfs_inode, block_num))
			{
				//printk("scorw_page_copy_thread_fn: copying block %u (parent inode: %lu) to child: %d with inode num: %lu\n", cur_page_copy->block_num, cur_page_copy->par->i_ino_num, j, c_scorw_inode->i_ino_num);
				//increment child version count on first write to a blk
				//that is not copied to child yet because only then
				//friend file is also modified
				scorw_child_version_cnt_inc(c_scorw_inode->i_vfs_inode);
				ret = scorw_copy_page_from_page_copy(cur_page_copy, c_scorw_inode);
				if(ret == 0)
				{
					scorw_set_block_copied(c_scorw_inode, block_num);
					scorw_dec_yet_to_copy_blocks_count(c_scorw_inode, 1);
					//signal to sync path regarding completion of this copy
					//so that child blk can be flushed to disk
					wake_up(&sync_child_wait_queue);
				}

			}
			scorw_put_uncopied_block(c_scorw_inode, block_num, COPYING_EXCL, uncopied_block);
			scorw_remove_uncopied_block(c_scorw_inode, block_num, uncopied_block);

			//sync the dirty blocks of the child file
			//scorw_inode_write_and_wait_range(c_scorw_inode->i_vfs_inode, 0, c_scorw_inode->i_copy_size);
		}	
		//printk("scorw_page_copy_thread_fn: block: %u is copied to all children\n",  block_num);
		up_read(&(p_scorw_inode->i_lock));
	
		//free processed page copy
		scorw_unprepare_page_copy(cur_page_copy);
		scorw_free_page_copy(cur_page_copy);
		page_copy_thread_running = 0;
	}	
	stop_page_copy_thread = 2;	//To signal that thread is ready to exit

	return 0;
}

int scorw_page_copy_thread_init(void)
{
	//printk("Inside scorw_page_copy_thread_init\n");
	page_copy_slab_cache = kmem_cache_create("page_copy", sizeof(struct page_copy), 0, SLAB_HWCACHE_ALIGN, NULL);
	if(page_copy_slab_cache == NULL)
	{
		printk("scorw_page_copy_thread_init: Failed to create page copy slab allocator cache\n");
		return -1;
	}
	if(page_copy_thread)
	{
		printk("scorw_page_copy_thread_init: Thread already exists\n");
		//return -1;
	}

	//printk("scorw_page_copy_thread_init: Creating Thread\n");
	page_copy_thread = kthread_run(scorw_page_copy_thread_fn, NULL, "page_copy_thread");
	if(page_copy_thread)
	{
		printk("scorw_page_copy_thread_init: Thread created successfully\n");
	}
	else
	{
		printk("scorw_page_copy_thread_init: Thread creation failed\n");
		return -1;
	}
	return 0;
}


int scorw_page_copy_thread_exit(void)
{
	//printk("Inside scorw_page_copy_thread_exit. Terminating Thread\n");
	if(page_copy_thread)
	{
		printk("scorw_page_copy_thread_exit: Thread about to be terminated\n");
		stop_page_copy_thread = 1;
		wake_up(&page_copy_thread_wq);
		while(stop_page_copy_thread != 2)
		{
			msleep(1000);
		}
		printk("scorw_page_copy_thread_exit: Thread terminated\n");
		page_copy_thread = 0;
	}
	else
	{
		printk("scorw_page_copy_thread_exit: No thread to terminate\n");
	}
	kmem_cache_destroy(page_copy_slab_cache);
	return 0;
}


//this is similar to module init function
void scorw_init(void)
{
	int error = 0;
	int ret = 0;
	scorw_thread = NULL;
	inode_policy = NULL;
	extent_policy = NULL;

	//printk("************************ Hello from scorwExt4 ******************************* \n");


	if(kobj_scorw)
	{
		//previous unmount aborted. For eg: fs was in use.
		//So, everything still is as it is.
		printk("Not re-initializing scorw on mount because possibly prev. unmount aborted and scorw state is still  valid!\n");
		return;
	}
	
	hash_init(page_copy_hlist);
	//start threads
	//scorw_thread_init();
	ret = scorw_page_copy_thread_init();
	if(ret < 0)
	{
		printk("Error while initialising page copy thread\n");
	}

	/*Creating a directory in /sys/ */
	kobj_scorw = kobject_create_and_add("corw_sparse", NULL);
	 
	/*Creating sysfs file*/
	if(kobj_scorw)
	{
		printk("scorw_init: created corw_sparse sysfs directory successfully");
		error = sysfs_create_file(kobj_scorw, &frnd_file_enable_recovery_attr.attr);
		if(error)
		{
			printk(KERN_ERR "scorw_init: Error while creating enable_recovery sysfs file\n");
			sysfs_remove_file(kobj_scorw, &frnd_file_enable_recovery_attr.attr);
			kobject_put(kobj_scorw);
		}
		else
		{
			printk("scorw_init: created enable_recovery file within corw_sparse sysfs directory successfully");
		}

		error = sysfs_create_file(kobj_scorw, &frnd_file_last_recovery_time_us_attr.attr);
		if(error)
		{
			printk(KERN_ERR "scorw_init: Error while creating last_recovery_time_ms sysfs file\n");
			sysfs_remove_file(kobj_scorw, &frnd_file_last_recovery_time_us_attr.attr);
			kobject_put(kobj_scorw);
		}
		else
		{
			printk("scorw_init: created last_recovery_time_ms within corw_sparse sysfs directory successfully");
		}
	}
	else
	{
		printk("scorw_init: Failed to create corw_sparse sysfs directory successfully. Does it already exists?");
	}


	////////////////////////////////////////////////////////
	//exported symbols (functions pointers) initialisation
	////////////////////////////////////////////////////////
	
	//If this function pointer(get_child_inode_num) is set, it enables the code in fs/fs-writeback.c that handles ordering of 
	//friend file and child files blocks.
	////
	//get_child_inode_num = scorw_get_child_friend_attr_val;
	get_child_inode_num = NULL;

	//If this function pointer(is_par_inode) is set, it enables the code is fs/fs-writeback.c that handles the ordering of
	//parent file blocks and child files blocks
	is_par_inode = scorw_is_par_file;
	//is_par_inode = NULL;
	
	//If this function pointer(is_par_inode) is set, it enables the code is fs/ext4-module/inode.c that handles the 
	//optimization of the code that handles the sparse files (ext4_da_map_blocks())
	is_child_inode = scorw_is_child_file;

	//For a parent file, find its children's inode numbers (if they exist)
	//Must set this function pointer to an actual function if is_par_inode is set
	get_child_i_attr_val = scorw_get_child_i_attr_val;

	//Find friend file inode num corresponding a child file
	//Must set this function pointer to an actual function if is_par_inode is set
	get_friend_attr_val = scorw_get_friend_attr_val;

	//Find whether a block is copied as per friend file or not
	//Must set this function pointer to an actual function if is_par_inode is set
	is_block_copied = scorw_is_block_copied;

	//Find whether page copy corresponding a block exists or not.
	//Must set this function pointer to an actual function if is_par_inode is set
	find_page_copy = scorw_find_page_copy;

	//unmount
	__scorw_exit = scorw_exit;

	is_child_file = scorw_is_child_file;
	is_par_file = scorw_is_par_file;
	unlink_child_file = scorw_unlink_child_file;
	unlink_par_file = scorw_unlink_par_file;

	init_waitqueue_head(&sync_child_wait_queue);
}

//this is similar to module exit function
//Note: This function is called on unmounting of a filesystem
void scorw_exit(void)
{
	printk("scorw_exit called\n");

	//scorw_inode_list();
	//scorw_clean_inode_list();

	//stop threads
	//scorw_thread_exit();
	scorw_page_copy_thread_exit();
	scorw_free_all_page_copy();
	scorw_process_pending_frnd_version_cnt_inc_list(1);

	/*
	if(scorw_sysfs_kobject)
	{	
		//printk("scorw_sysfs_kobject exists! Removing it and files inside it\n");
		sysfs_remove_file(scorw_sysfs_kobject, &async_copy_status_attr.attr);
		kobject_del(scorw_sysfs_kobject);
	}
	else
	{
		//printk("scorw_sysfs_kobject doesn't exist!\n");
	}
	*/
	if(kobj_scorw)
	{
		printk("scorw_exit: kobj_scorw exists! Removing it and files inside it\n");
		sysfs_remove_file(kobj_scorw, &frnd_file_enable_recovery_attr.attr);
		sysfs_remove_file(kobj_scorw, &frnd_file_last_recovery_time_us_attr.attr);
		kobject_put(kobj_scorw);
	}
	else
	{
		printk("scorw_exit: kobj_scorw doesn't exist!\n");
	}

	///////////////////////////////////////////////////
	//exported symbols (functions pointers) resetting
	///////////////////////////////////////////////////
	//get_child_inode_num = 0;
/*
	get_child_inode_num = 0;
	is_par_inode = 0;
	get_child_i_attr_val = 0;
	get_friend_attr_val = 0;
	is_block_copied = 0;
	__scorw_exit = 0;
*/
	is_child_file = 0;
	is_par_file = 0;
	unlink_child_file = 0;
	unlink_par_file = 0;
	kobj_scorw = 0;

	//printk("************************ Good Bye from scorwExt4 *******************************\n");
}
