
/*
 *  linux/fs/ext4/file.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/file.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  ext4 fs regular file handling primitives
 *
 *  64-bit file support on 64-bit platforms by Jakub Jelinek
 *	(jj@sunsite.ms.mff.cuni.cz)
 */

#include <linux/time.h>
#include <linux/fs.h>
#include <linux/iomap.h>
#include <linux/mount.h>
#include <linux/path.h>
#include <linux/dax.h>
#include <linux/quotaops.h>
#include <linux/pagevec.h>
#include <linux/uio.h>
#include <linux/mman.h>
#include <linux/backing-dev.h>
#include "ext4.h"
#include "ext4_jbd2.h"
#include "xattr.h"
#include "acl.h"
#include "truncate.h"

#include <linux/sched.h>
#include <linux/fs_struct.h>
#include <linux/path.h>
#include <linux/mount.h>
#include <linux/fs.h>

#include "corw_sparse.h"

///////// scorw start //////////
extern void ext4_es_print_tree(struct inode *inode);
///////// scorw end //////////



static bool ext4_dio_supported(struct inode *inode)
{
	//printk("Inside ext4_dio_supported\n");
	if (IS_ENABLED(CONFIG_FS_ENCRYPTION) && IS_ENCRYPTED(inode))
		return false;
	if (fsverity_active(inode))
		return false;
	if (ext4_should_journal_data(inode))
		return false;
	if (ext4_has_inline_data(inode))
		return false;
	return true;
}

static ssize_t ext4_dio_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	ssize_t ret;
	struct inode *inode = file_inode(iocb->ki_filp);

	printk("&&&&&&&&&&&&&&&&&&&&&&&&&Inside ext4_dio_read_iter\n");
	

	if (iocb->ki_flags & IOCB_NOWAIT) {
		if (!inode_trylock_shared(inode))
			return -EAGAIN;
	} else {
		inode_lock_shared(inode);
	}

	if (!ext4_dio_supported(inode)) {
		inode_unlock_shared(inode);
		/*
		 * Fallback to buffered I/O if the operation being performed on
		 * the inode is not supported by direct I/O. The IOCB_DIRECT
		 * flag needs to be cleared here in order to ensure that the
		 * direct I/O path within generic_file_read_iter() is not
		 * taken.
		 */
		iocb->ki_flags &= ~IOCB_DIRECT;
		return generic_file_read_iter(iocb, to);
	}

	ret = iomap_dio_rw(iocb, to, &ext4_iomap_ops, NULL,
			   is_sync_kiocb(iocb));
	inode_unlock_shared(inode);

	file_accessed(iocb->ki_filp);
	return ret;
}

#ifdef CONFIG_FS_DAX
static ssize_t ext4_dax_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	struct inode *inode = file_inode(iocb->ki_filp);
	ssize_t ret;

	//printk("Inside ext4_dax_read_iter\n");

	if (iocb->ki_flags & IOCB_NOWAIT) {
		if (!inode_trylock_shared(inode))
			return -EAGAIN;
	} else {
		inode_lock_shared(inode);
	}
	/*
	 * Recheck under inode lock - at this point we are sure it cannot
	 * change anymore
	 */
	if (!IS_DAX(inode)) {
		inode_unlock_shared(inode);
		/* Fallback to buffered IO in case we cannot support DAX */
		return generic_file_read_iter(iocb, to);
	}
	ret = dax_iomap_rw(iocb, to, &ext4_iomap_ops);
	inode_unlock_shared(inode);

	file_accessed(iocb->ki_filp);
	return ret;
}
#endif

static ssize_t ext4_file_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	ssize_t ret;
	struct inode *inode = file_inode(iocb->ki_filp);
        int is_child_file;

	//printk("\n\n===============================\n");
	//static unsigned ctr = 0;
	//printk("Inside ext4_file_read_iter. Called %u times \n", ++ctr);

	if (unlikely(ext4_forced_shutdown(EXT4_SB(inode->i_sb))))
		return -EIO;

	//reading 0 bytes
	if (!iov_iter_count(to))
		return 0; /* skip atime */

	//printk("ext4_file_read_iter called!\n");
        //printk("ext4_file_read_iter: inode: %p\n", inode);
	//printk("ext4_file_read_iter: inode->i_private: %p $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n", inode->i_private);


	/////////////follow on read///////////////
	///////////// scorw start /////////////
	
	is_child_file = scorw_is_child_file(inode, 0);
	if(is_child_file && (!(iocb->ki_flags & IOCB_DIRECT)))
	{      
		ret = scorw_follow_on_read_child_blocks(inode, iocb, to);
		if(ret != SCORW_PERFORM_ORIG_READ)
		{
			return ret;
		}
	}
	else if(is_child_file && ((iocb->ki_flags & IOCB_DIRECT)))
	{      
		return 0;
	}
	
	///////////// scorw end /////////////

	
	//This config is off or IS_DAX(inode) is false.
	//Not called for normal read
#ifdef CONFIG_FS_DAX
	if (IS_DAX(inode))
		return ext4_dax_read_iter(iocb, to);
#endif
	//direct i/o. Not called for normal read
	if (iocb->ki_flags & IOCB_DIRECT)
		return ext4_dio_read_iter(iocb, to);

	//This function is called
	return generic_file_read_iter(iocb, to);
}

/*
 * Called when an inode is released. Note that this is different
 * from ext4_file_open: open gets called at every open, but release
 * gets called only when /all/ the files are closed.
 */
//Update: 
//Tried to open a file 4 times and then close it 4 times.
//Each time release fn. was called.
//
//Maybe, means called upon say when process exits.
//
static int ext4_release_file(struct inode *inode, struct file *filp)
{
	//printk("Inside ext4_release_file\n");
	//printk("ext4_release_file: inode num: %lu", inode->i_ino);
	//dump_stack();

	////////// scorw start //////////
	//print info about inodes in scorw inodes list
        //scorw_print_inode_list();
	

	//printk("ext4_release_file: Checking whether file is a sparse file\n");
	if(scorw_is_child_file(inode, 0))
	{
		//printk("ext4_release_file: Yes! opened file is a sparse file.\n");
		//This lock makes sure that open(par),close(par) and creation of par scorw inodes inside special_open() happens atomically
		mutex_lock(&(inode->i_vfs_inode_open_close_lock));
		atomic_sub(1, &(inode->i_vfs_inode_open_count));

		scorw_put_inode(inode, 1, 0, 0);

		mutex_unlock(&(inode->i_vfs_inode_open_close_lock));
	}
	else if(scorw_is_par_file(inode, 0))
	{
		//This lock makes sure that open(par),close(par) and creation of par scorw inodes inside special_open() happens atomically
		mutex_lock(&(inode->i_vfs_inode_open_close_lock));
		atomic_sub(1, &(inode->i_vfs_inode_open_count));

		//printk("ext4_release_file: Yes! opened file is a parent file\n");
		scorw_put_inode(inode, 0, 0, 0);

		mutex_unlock(&(inode->i_vfs_inode_open_close_lock));
	}

	
	////////// scorw end //////////

	//do delayed allocation for this inode when this file is closed. 
	//(one any close? or last close?)
	//Todo: Do this test. Also, can it happen before close also? example: sync()
	//Could there be some thread also that schedules this allocation on expiration of some
	//timer? (How would thread know which inode is needs allocation?)
	//
	if (ext4_test_inode_state(inode, EXT4_STATE_DA_ALLOC_CLOSE)) {
		ext4_alloc_da_blocks(inode);
		ext4_clear_inode_state(inode, EXT4_STATE_DA_ALLOC_CLOSE);
	}
	/* if we are the last writer on the inode, drop the block reservation */
	if ((filp->f_mode & FMODE_WRITE) &&
			(atomic_read(&inode->i_writecount) == 1) &&
		        !EXT4_I(inode)->i_reserved_data_blocks)
	{
		down_write(&EXT4_I(inode)->i_data_sem);
		ext4_discard_preallocations(inode);
		up_write(&EXT4_I(inode)->i_data_sem);
	}
	if (is_dx(inode) && filp->private_data)
		ext4_htree_free_dir_info(filp->private_data);

	return 0;
}

/*
 * This tests whether the IO in question is block-aligned or not.
 * Ext4 utilizes unwritten extents when hole-filling during direct IO, and they
 * are converted to written only after the IO is complete.  Until they are
 * mapped, these blocks appear as holes, so dio_zero_block() will assume that
 * it needs to zero out portions of the start and/or end block.  If 2 AIO
 * threads are at work on the same unwritten block, they must be synchronized
 * or one thread will zero the other's data, causing corruption.
 */
static int
ext4_unaligned_aio(struct inode *inode, struct iov_iter *from, loff_t pos)
{
	struct super_block *sb = inode->i_sb;
	int blockmask = sb->s_blocksize - 1;

	//printk("Inside ext4_unaligned_aio\n");

	if (pos >= ALIGN(i_size_read(inode), sb->s_blocksize))
		return 0;

	if ((pos | iov_iter_alignment(from)) & blockmask)
		return 1;

	return 0;
}

/* Is IO overwriting allocated and initialized blocks? */
static bool ext4_overwrite_io(struct inode *inode, loff_t pos, loff_t len)
{
	struct ext4_map_blocks map;
	unsigned int blkbits = inode->i_blkbits;
	int err, blklen;

	//printk("Inside ext4_overwrite_io\n");

	if (pos + len > i_size_read(inode))
		return false;

	map.m_lblk = pos >> blkbits;
	map.m_len = EXT4_MAX_BLOCKS(len, pos, blkbits);
	blklen = map.m_len;

	err = ext4_map_blocks(NULL, inode, &map, 0);
	/*
	 * 'err==len' means that all of the blocks have been preallocated,
	 * regardless of whether they have been initialized or not. To exclude
	 * unwritten extents, we need to check m_flags.
	 */
	return err == blklen && (map.m_flags & EXT4_MAP_MAPPED);
}

//Some generic checks are:
//If inode tells that its a swap file, return error(Maybe, to avoid direct write to swap file)
//
//If trying to write 0 bytes, return 0;
//
//Also, change position of file offset to end of file in case of 
//append operation
//
//If not direct i/o but still no-waiting flag is set, return error
//(because can sleep due to reading/allocating some memory)
//
//If offset is larger than what is the largest supported size, return error
//
//Apart from above generic checks,
//Update the mtime and ctime members of vfs inode and mark the inode for writeback. (Inode is marked dirty)
//(Isn't ondisk inode required to be modified and marked dirty? Is marking vfs inod dirty enough?)
static ssize_t ext4_write_checks(struct kiocb *iocb, struct iov_iter *from)
{
	//obtain vfs inode
	struct inode *inode = file_inode(iocb->ki_filp);
	ssize_t ret;

	//printk("Inside ext4_write_checks\n");

	if (unlikely(IS_IMMUTABLE(inode)))
		return -EPERM;

	ret = generic_write_checks(iocb, from);
	if (ret <= 0)
		return ret;

	/*
	 * If we have encountered a bitmap-format file, the size limit
	 * is smaller than s_maxbytes, which is for extent-mapped files.
	 */
	if (!(ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))) {
		struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);

		if (iocb->ki_pos >= sbi->s_bitmap_maxbytes)
			return -EFBIG;
		iov_iter_truncate(from, sbi->s_bitmap_maxbytes - iocb->ki_pos);
	}

	ret = file_modified(iocb->ki_filp);
	if (ret)
		return ret;

	return iov_iter_count(from);
}

//// scorw start ////
ssize_t scorw_generic_perform_write(struct file *file, struct iov_iter *i, loff_t pos, int write_to_par)
{
	struct address_space *mapping = file->f_mapping;
	const struct address_space_operations *a_ops = mapping->a_ops;
	long status = 0;
	ssize_t written = 0;
	unsigned int flags = 0;
	int error = 0;
	//scorw start//
	struct inode *inode = mapping->host;
	int is_append_op = 0;	//optimization. Skip scorw operations for append to a parent file.
	unsigned blk_num = 0;
	unsigned last_block_eligible_for_copy = 0;
	struct uncopied_block *uncopied_block[SCORW_MAX_CHILDS] = {0};
	//scorw end//

	do {
		struct page *page;
		unsigned long offset;	/* Offset into pagecache page */
		unsigned long bytes;	/* Bytes to write to page */
		size_t copied;		/* Bytes copied from user */
		void *fsdata;
		is_append_op = 0;	//optimization. Skip scorw operations for append to a parent file.
		blk_num = 0;
		last_block_eligible_for_copy = 0;

		offset = (pos & (PAGE_SIZE - 1));
		bytes = min_t(unsigned long, PAGE_SIZE - offset,
						iov_iter_count(i));

again:
		/*
		 * Bring in the user page that we will copy from _first_.
		 * Otherwise there's a nasty deadlock on copying from the
		 * same page as we're writing to, without it being marked
		 * up-to-date.
		 *
		 * Not only is this an optimisation, but it is also required
		 * to check that the address is actually valid, when atomic
		 * usercopies are used, below.
		 */
		if (unlikely(iov_iter_fault_in_readable(i, bytes))) {
			status = -EFAULT;
			break;
		}

		if (fatal_signal_pending(current)) {
			status = -EINTR;
			break;
		}

		//////////////// scorw start ///////////////
		//printk("scorw_generic_perform_write: Writing to a file. write_to_par: %d\n", write_to_par);
		if(write_to_par)
		{
			blk_num = (offset >> PAGE_SHIFT);
			last_block_eligible_for_copy  = ((inode->i_size-1) >> PAGE_SHIFT);

			//This write is purely append operation. Nothing to be done by us.
			if(blk_num > last_block_eligible_for_copy)
			{
				is_append_op = 1;
			}
			else
			{
				scorw_read_barrier_begin(scorw_find_inode(inode), blk_num, uncopied_block); 
			}
		}
		//////////////// scorw end ///////////////


		status = a_ops->write_begin(file, mapping, pos, bytes, flags, &page, &fsdata);
		if (unlikely(status < 0))
			break;

		//////////////// scorw start ///////////////
		if(write_to_par && !is_append_op)
		{
			//printk("[pid: %u] scorw_generic_perform_write: Writing to parent file. start offset: %lu, len: %d\n", current->pid, pos, bytes);
			error = scorw_write_par_blocks(inode, pos, bytes, page);
			BUG_ON(error);
		}
		//////////////// scorw end ///////////////


		if (mapping_writably_mapped(mapping))
			flush_dcache_page(page);

		copied = iov_iter_copy_from_user_atomic(page, i, offset, bytes);
		flush_dcache_page(page);

		status = a_ops->write_end(file, mapping, pos, bytes, copied,
						page, fsdata);
		//////////////// scorw start ///////////////
		if(write_to_par && !is_append_op)
		{
			scorw_read_barrier_end(scorw_find_inode(inode), blk_num, uncopied_block);
		}
		//////////////// scorw end ///////////////
		if (unlikely(status < 0))
			break;
		copied = status;

		cond_resched();

		iov_iter_advance(i, copied);
		if (unlikely(copied == 0)) {
			/*
			 * If we were unable to copy any data at all, we must
			 * fall back to a single segment length write.
			 *
			 * If we didn't fallback here, we could livelock
			 * because not all segments in the iov can be copied at
			 * once without a pagefault.
			 */
			bytes = min_t(unsigned long, PAGE_SIZE - offset,
						iov_iter_single_seg_count(i));
			goto again;
		}
		pos += copied;
		written += copied;

		balance_dirty_pages_ratelimited(mapping);
	} while (iov_iter_count(i));

	return written ? written : status;
}
//// scorw end////

static ssize_t ext4_buffered_write_iter(struct kiocb *iocb,
					struct iov_iter *from)
{
	ssize_t ret;
	struct uncopied_block *uncopied_block = NULL;
	
	int error = 0;
	loff_t offset = iocb->ki_pos;
	size_t len = iov_iter_count(from);
	int write_to_par = 0;
	int write_to_child = 0;
        
#ifndef USE_OLD_RANGE
	struct sharing_range_info shr = {
		.initialized = false,
		.partial_cow = false,
		.start_block = 0,
		.end_block = 0,
	};
	struct inode *p_inode = NULL;
	struct address_space *mapping = NULL;
#endif

	//vfs inode corresponding which write has to happen
	//
	struct inode *inode = file_inode(iocb->ki_filp);

	//printk("Inside ext4_buffered_write_iter, inode: %lu\n", inode->i_ino);

	//Maybe, no blocking/waiting during i/o
	if (iocb->ki_flags & IOCB_NOWAIT)
		return -EOPNOTSUPP;


	//One of the the things this fn. does is, it generates
	//correct offset where append has to happen. otherwise,
	//offset 0 is seen for append.
	ret = ext4_write_checks(iocb, from);
	if (ret <= 0)
		return ret;


        //////////////////////// scorw start /////////////////////////
	//// 	Idea obtained from generic_file_buffered_read()	/////
	////////////////////////////////////////////////////////////
	//inode_lock(inode);	//<--------------- Remove it after debugging

	if(scorw_is_child_file(inode, 0))
	{	
		write_to_child = 1;
		//printk("ext4_buffered_write_iter: Writing to child file\n");
#ifdef USE_OLD_RANGE
		error = scorw_write_child_blocks_begin(inode, offset, len, (void*)(&(uncopied_block)));
#else
		error = scorw_write_child_blocks_begin(inode, offset, len, (void*)(&(uncopied_block)), &shr);
#endif
		if(error)
		{
			return error;
		}
#ifndef USE_OLD_RANGE
	      if(shr.initialized || shr.partial_cow){
	           unsigned long last_offset = offset + len - 1; 
		   unsigned long last_blk_allowed = shr.end_block;
		   unsigned long last_offset_allowed = (last_blk_allowed << PAGE_SHIFT) + 4095;
		   //printk(KERN_INFO "Before: offset: %lld, last_offset: %lld, last_offset_allowed: %lld, len: %lld\n", offset, last_offset, last_offset_allowed, len);
		   if(last_offset_allowed < last_offset){
			  //len -= last_offset - last_offset_allowed; 
			  len = last_offset_allowed - offset + 1; 
			  iov_iter_truncate(from, len);
		   }
		   //printk(KERN_INFO "After: offset: %lld, last_offset: %lld, last_offset_allowed: %lld, len: %lld\n", offset, last_offset, last_offset_allowed, len);
	      }	      
	      if(shr.initialized){
		      struct scorw_inode* scorw_inode = scorw_find_inode(inode);
                      BUG_ON(!scorw_inode || scorw_inode->i_vfs_inode != inode);
                      p_inode = scorw_inode->i_par_vfs_inode;
		      mapping = iocb->ki_filp->f_mapping;
		      //printk(KERN_INFO "Writing shared at %lld of len %lld\n", offset, len);
	              inode_lock(p_inode);	
                      iocb->ki_filp->f_mapping = p_inode->i_mapping; 
	              current->backing_dev_info = inode_to_bdi(inode);
	              ret = scorw_generic_perform_write(iocb->ki_filp, from, iocb->ki_pos, write_to_par);
		      iocb->ki_filp->f_mapping = mapping; 
	              current->backing_dev_info = NULL;
	              inode_unlock(p_inode);
		      goto finalize_out;
	      }	      
#endif
	}
	//Note: scorw_write_par_blocks is now being called from scorw_generic_perform_write 
	else if(scorw_is_par_file(inode, 0))
	{
		//printk("ext4_buffered_write_iter: Writing to parent file\n");
		write_to_par = 1;
		//
		//error = scorw_write_par_blocks(inode, offset, len);
		//if(error)
		//{
		//	return error;
		//}
		//
	}
	/*
	else
	{
		printk("ext4_buffered_write_iter: Writing to neither child nor parent file\n");
	}
	*/
	
	
        ////////// scorw end//////////



	//inode is locked. Maybe, because read/write has to happen
	//to this inode. Eg: we want to read info blocks where 
	//data is stored and we don't want some other process to free
	//those blocks by truncate operation. Likewise, we need to change
	//file size, timestamps etc.
	//
	//acquires read-write semaphore of vfs inode
	//own_write(&inode->i_rwsem);
	//
	inode_lock(inode);	//<--------------- Uncomment it after debugging
	//printk("[pid: %u] ext4_buffered_write_iter: locking inode: %lu\n", current->pid, inode->i_ino);


	//Apart from checks, inode time is also updated within this function.
	//In inode time updation fn file_update_time,following comment is found:
	//	Update the mtime and ctime members of an inode and mark the inode for writeback. 
	//It internally calls,  mnt_want_write, which has a comment that tells
	//what marking inode for writeback means:
	//This tells the low-level filesystem that a write is about to be performed to  it, and makes sure that writes are allowed (mount is read-write, filesystem  is not frozen) before returning success.  When the write operation is finished, mnt_drop_write() must be called.  This is effectively a refcount.  



	current->backing_dev_info = inode_to_bdi(inode);

	
	

	//Does write_begin, copying data from userspace to page in page cache, write_end. 
	//Writeback is done periodically number of pages dirty for current process are 
	//greater than certain limit.
	//
	//returns amount of bytes written on success
	//
	//scorw start
	//
	//ret = generic_perform_write(iocb->ki_filp, from, iocb->ki_pos);
	ret = scorw_generic_perform_write(iocb->ki_filp, from, iocb->ki_pos, write_to_par);
	//scorw end
	current->backing_dev_info = NULL;

//out:
	//printk("[pid: %u] ext4_buffered_write_iter: unlocking inode: %lu\n", current->pid, inode->i_ino);
	inode_unlock(inode);

#ifndef USE_OLD_RANGE	
finalize_out:
#endif	
	//scorw start
	if(write_to_child)
	{	
		//printk("ext4_buffered_write_iter: Writing to child file\n");
		//BUG_ON(uncopied_block == NULL);	//It can be null, such as during pure append operation
#ifdef USE_OLD_RANGE
		error = scorw_write_child_blocks_end(inode, offset, len, uncopied_block);
#else
		error = scorw_write_child_blocks_end(inode, offset, len, uncopied_block, shr.initialized);
#endif
		if(error)
		{
			return error;
		}
	}
	//scorw end


	if (likely(ret > 0)) {
		iocb->ki_pos += ret;

	//From comment above generic_write_sync
	/*
	 * Sync the bytes written if this was a synchronous write.  Expect ki_pos
	 * to already be updated for the write, and will return either the amount
	 * of bytes passed in, or an error if syncing the file failed.
	 */
	// Relevant statement is 
	//if (iocb->ki_flags & IOCB_DSYNC) 
	//Assuming this flag is not set.
	//

		ret = generic_write_sync(iocb, ret);	
	}


	return ret;
}

static ssize_t ext4_handle_inode_extension(struct inode *inode, loff_t offset,
					   ssize_t written, size_t count)
{
	handle_t *handle;
	bool truncate = false;
	u8 blkbits = inode->i_blkbits;
	ext4_lblk_t written_blk, end_blk;

	//printk("Inside ext4_handle_inode_extension\n");

	/*
	 * Note that EXT4_I(inode)->i_disksize can get extended up to
	 * inode->i_size while the I/O was running due to writeback of delalloc
	 * blocks. But, the code in ext4_iomap_alloc() is careful to use
	 * zeroed/unwritten extents if this is possible; thus we won't leave
	 * uninitialized blocks in a file even if we didn't succeed in writing
	 * as much as we intended.
	 */
	WARN_ON_ONCE(i_size_read(inode) < EXT4_I(inode)->i_disksize);
	if (offset + count <= EXT4_I(inode)->i_disksize) {
		/*
		 * We need to ensure that the inode is removed from the orphan
		 * list if it has been added prematurely, due to writeback of
		 * delalloc blocks.
		 */
		if (!list_empty(&EXT4_I(inode)->i_orphan) && inode->i_nlink) {
			handle = ext4_journal_start(inode, EXT4_HT_INODE, 2);

			if (IS_ERR(handle)) {
				ext4_orphan_del(NULL, inode);
				return PTR_ERR(handle);
			}

			ext4_orphan_del(handle, inode);
			ext4_journal_stop(handle);
		}

		return written;
	}

	if (written < 0)
		goto truncate;

	handle = ext4_journal_start(inode, EXT4_HT_INODE, 2);
	if (IS_ERR(handle)) {
		written = PTR_ERR(handle);
		goto truncate;
	}

	if (ext4_update_inode_size(inode, offset + written))
		ext4_mark_inode_dirty(handle, inode);

	/*
	 * We may need to truncate allocated but not written blocks beyond EOF.
	 */
	written_blk = ALIGN(offset + written, 1 << blkbits);
	end_blk = ALIGN(offset + count, 1 << blkbits);
	if (written_blk < end_blk && ext4_can_truncate(inode))
		truncate = true;

	/*
	 * Remove the inode from the orphan list if it has been extended and
	 * everything went OK.
	 */
	if (!truncate && inode->i_nlink)
		ext4_orphan_del(handle, inode);
	ext4_journal_stop(handle);

	if (truncate) {
truncate:
		ext4_truncate_failed_write(inode);
		/*
		 * If the truncate operation failed early, then the inode may
		 * still be on the orphan list. In that case, we need to try
		 * remove the inode from the in-memory linked list.
		 */
		if (inode->i_nlink)
			ext4_orphan_del(NULL, inode);
	}

	return written;
}

static int ext4_dio_write_end_io(struct kiocb *iocb, ssize_t size,
				 int error, unsigned int flags)
{
	loff_t offset = iocb->ki_pos;
	struct inode *inode = file_inode(iocb->ki_filp);

	//printk("Inside ext4_dio_write_end_io\n");

	if (error)
		return error;

	if (size && flags & IOMAP_DIO_UNWRITTEN)
		return ext4_convert_unwritten_extents(NULL, inode,
						      offset, size);

	return 0;
}

static const struct iomap_dio_ops ext4_dio_write_ops = {
	.end_io = ext4_dio_write_end_io,
};

static ssize_t ext4_dio_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	ssize_t ret;
	size_t count;
	loff_t offset;
	handle_t *handle;
	struct inode *inode = file_inode(iocb->ki_filp);
	bool extend = false, overwrite = false, unaligned_aio = false;

	//printk("Inside ext4_dio_write_end_iter\n");

	if (iocb->ki_flags & IOCB_NOWAIT) {
		if (!inode_trylock(inode))
			return -EAGAIN;
	} else {
		inode_lock(inode);
	}

	if (!ext4_dio_supported(inode)) {
		inode_unlock(inode);
		/*
		 * Fallback to buffered I/O if the inode does not support
		 * direct I/O.
		 */
		return ext4_buffered_write_iter(iocb, from);
	}

	ret = ext4_write_checks(iocb, from);
	if (ret <= 0) {
		inode_unlock(inode);
		return ret;
	}

	/*
	 * Unaligned asynchronous direct I/O must be serialized among each
	 * other as the zeroing of partial blocks of two competing unaligned
	 * asynchronous direct I/O writes can result in data corruption.
	 */
	offset = iocb->ki_pos;
	count = iov_iter_count(from);
	if (ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS) &&
	    !is_sync_kiocb(iocb) && ext4_unaligned_aio(inode, from, offset)) {
		unaligned_aio = true;
		inode_dio_wait(inode);
	}

	/*
	 * Determine whether the I/O will overwrite allocated and initialized
	 * blocks. If so, check to see whether it is possible to take the
	 * dioread_nolock path.
	 */
	if (!unaligned_aio && ext4_overwrite_io(inode, offset, count) &&
	    ext4_should_dioread_nolock(inode)) {
		overwrite = true;
		downgrade_write(&inode->i_rwsem);
	}

	if (offset + count > EXT4_I(inode)->i_disksize) {
		handle = ext4_journal_start(inode, EXT4_HT_INODE, 2);
		if (IS_ERR(handle)) {
			ret = PTR_ERR(handle);
			goto out;
		}

		ret = ext4_orphan_add(handle, inode);
		if (ret) {
			ext4_journal_stop(handle);
			goto out;
		}

		extend = true;
		ext4_journal_stop(handle);
	}

	ret = iomap_dio_rw(iocb, from, &ext4_iomap_ops, &ext4_dio_write_ops,
			   is_sync_kiocb(iocb) || unaligned_aio || extend);

	if (extend)
		ret = ext4_handle_inode_extension(inode, offset, ret, count);

out:
	if (overwrite)
		inode_unlock_shared(inode);
	else
		inode_unlock(inode);

	if (ret >= 0 && iov_iter_count(from)) {
		ssize_t err;
		loff_t endbyte;

		offset = iocb->ki_pos;
		err = ext4_buffered_write_iter(iocb, from);
		if (err < 0)
			return err;

		/*
		 * We need to ensure that the pages within the page cache for
		 * the range covered by this I/O are written to disk and
		 * invalidated. This is in attempt to preserve the expected
		 * direct I/O semantics in the case we fallback to buffered I/O
		 * to complete off the I/O request.
		 */
		ret += err;
		endbyte = offset + err - 1;
		err = filemap_write_and_wait_range(iocb->ki_filp->f_mapping,
						   offset, endbyte);
		if (!err)
			invalidate_mapping_pages(iocb->ki_filp->f_mapping,
						 offset >> PAGE_SHIFT,
						 endbyte >> PAGE_SHIFT);
	}

	return ret;
}

#ifdef CONFIG_FS_DAX
static ssize_t
ext4_dax_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	ssize_t ret;
	size_t count;
	loff_t offset;
	handle_t *handle;
	bool extend = false;
	struct inode *inode = file_inode(iocb->ki_filp);

	//printk("Inside ext4_dax_write_iter\n");

	if (iocb->ki_flags & IOCB_NOWAIT) {
		if (!inode_trylock(inode))
			return -EAGAIN;
	} else {
		inode_lock(inode);
	}

	ret = ext4_write_checks(iocb, from);
	if (ret <= 0)
		goto out;

	offset = iocb->ki_pos;
	count = iov_iter_count(from);

	if (offset + count > EXT4_I(inode)->i_disksize) {
		handle = ext4_journal_start(inode, EXT4_HT_INODE, 2);
		if (IS_ERR(handle)) {
			ret = PTR_ERR(handle);
			goto out;
		}

		ret = ext4_orphan_add(handle, inode);
		if (ret) {
			ext4_journal_stop(handle);
			goto out;
		}

		extend = true;
		ext4_journal_stop(handle);
	}

	ret = dax_iomap_rw(iocb, from, &ext4_iomap_ops);

	if (extend)
		ret = ext4_handle_inode_extension(inode, offset, ret, count);
out:
	inode_unlock(inode);
	if (ret > 0)
		ret = generic_write_sync(iocb, ret);
	return ret;
}
#endif

//Recall, these arguments are basically representation
//of buffer to write data from and inode to write to.
//Todo: cross-verify
//
static ssize_t
ext4_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct inode *inode = file_inode(iocb->ki_filp);
	//printk("\n\n===============================");
	//printk("Inside of ext4_file_write_iter()\n");

	//printk("current->fs->pwd.mnt->mnt_sb->s_type->name: %s\n", current->fs->pwd.mnt->mnt_sb->s_type->name);
	//printk("EXT4_MODULE_NAME: %s\n",EXT4_MODULE_NAME );
	//printk("CWD_FILESYSTEM_EXT4_MODULE_NAME: %s\n",CWD_FILESYSTEM_EXT4_MODULE_NAME );

	/*
	if(0 == strcmp(EXT4_MODULE_NAME, CWD_FILESYSTEM_EXT4_MODULE_NAME ))
	{
		printk("cwd of current process is ext4-module\n");	
	}
	else
	{
		printk("cwd of current process is NOT ext4-module\n");	
	}
	*/


	if (unlikely(ext4_forced_shutdown(EXT4_SB(inode->i_sb))))
		return -EIO;

#ifdef CONFIG_FS_DAX
	if (IS_DAX(inode))
		return ext4_dax_write_iter(iocb, from);
#endif
	if (iocb->ki_flags & IOCB_DIRECT)
		return ext4_dio_write_iter(iocb, from);

	return ext4_buffered_write_iter(iocb, from);
}

#ifdef CONFIG_FS_DAX
static vm_fault_t ext4_dax_huge_fault(struct vm_fault *vmf,
		enum page_entry_size pe_size)
{
	int error = 0;
	vm_fault_t result;
	int retries = 0;
	handle_t *handle = NULL;
	struct inode *inode = file_inode(vmf->vma->vm_file);
	struct super_block *sb = inode->i_sb;

	//printk("Inside ext4_dax_huge_fault\n");

	/*
	 * We have to distinguish real writes from writes which will result in a
	 * COW page; COW writes should *not* poke the journal (the file will not
	 * be changed). Doing so would cause unintended failures when mounted
	 * read-only.
	 *
	 * We check for VM_SHARED rather than vmf->cow_page since the latter is
	 * unset for pe_size != PE_SIZE_PTE (i.e. only in do_cow_fault); for
	 * other sizes, dax_iomap_fault will handle splitting / fallback so that
	 * we eventually come back with a COW page.
	 */
	bool write = (vmf->flags & FAULT_FLAG_WRITE) &&
		(vmf->vma->vm_flags & VM_SHARED);
	pfn_t pfn;

	if (write) {
		sb_start_pagefault(sb);
		file_update_time(vmf->vma->vm_file);
		down_read(&EXT4_I(inode)->i_mmap_sem);
retry:
		handle = ext4_journal_start_sb(sb, EXT4_HT_WRITE_PAGE,
					       EXT4_DATA_TRANS_BLOCKS(sb));
		if (IS_ERR(handle)) {
			up_read(&EXT4_I(inode)->i_mmap_sem);
			sb_end_pagefault(sb);
			return VM_FAULT_SIGBUS;
		}
	} else {
		down_read(&EXT4_I(inode)->i_mmap_sem);
	}
	result = dax_iomap_fault(vmf, pe_size, &pfn, &error, &ext4_iomap_ops);
	if (write) {
		ext4_journal_stop(handle);

		if ((result & VM_FAULT_ERROR) && error == -ENOSPC &&
		    ext4_should_retry_alloc(sb, &retries))
			goto retry;
		/* Handling synchronous page fault? */
		if (result & VM_FAULT_NEEDDSYNC)
			result = dax_finish_sync_fault(vmf, pe_size, pfn);
		up_read(&EXT4_I(inode)->i_mmap_sem);
		sb_end_pagefault(sb);
	} else {
		up_read(&EXT4_I(inode)->i_mmap_sem);
	}

	return result;
}

static vm_fault_t ext4_dax_fault(struct vm_fault *vmf)
{
	return ext4_dax_huge_fault(vmf, PE_SIZE_PTE);
}

static const struct vm_operations_struct ext4_dax_vm_ops = {
	.fault		= ext4_dax_fault,
	.huge_fault	= ext4_dax_huge_fault,
	.page_mkwrite	= ext4_dax_fault,
	.pfn_mkwrite	= ext4_dax_fault,
};
#else
#define ext4_dax_vm_ops	ext4_file_vm_ops
#endif

static const struct vm_operations_struct ext4_file_vm_ops = {
	.fault		= ext4_filemap_fault,
	.map_pages	= filemap_map_pages,
	.page_mkwrite   = ext4_page_mkwrite,
};

static int ext4_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct inode *inode = file->f_mapping->host;
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	struct dax_device *dax_dev = sbi->s_daxdev;

	if (unlikely(ext4_forced_shutdown(sbi)))
		return -EIO;


	//printk("Inside ext4_file_mmap\n");

	/*
	 * We don't support synchronous mappings for non-DAX files and
	 * for DAX files if underneath dax_device is not synchronous.
	 */
	if (!daxdev_mapping_supported(vma, dax_dev))
		return -EOPNOTSUPP;

	file_accessed(file);
	if (IS_DAX(file_inode(file))) {
		vma->vm_ops = &ext4_dax_vm_ops;
		vma->vm_flags |= VM_HUGEPAGE;
	} else {
		vma->vm_ops = &ext4_file_vm_ops;
	}
	return 0;
}


/*
Fscrypt and fsverity are not used.
Ext4_sample_last_mounted is called.  Sample the mount point means?
Found following comment.

Sample where the filesystem has been mounted and
         * store it in the superblock for sysadmin convenience
         * when trying to sort through large numbers of block
         * devices or filesystem images.

Looks like refers to mountpoint name and means that this name is stored in 64 bytes of superblock.
Looks like happens once.
Eg: Earlier had mounted on ext4-dir directory. Then unmounted and mounted on a new directory called temp1.

[595714.313504] Inside ext4_sample_last_mounted. Before: sbi->s_es->s_last_mounted: /home/overlay/ext4-m/ext4-dir.
[595714.313505] Inside ext4_sample_last_mounted. Before: cp: /home/overlay/ext4-m/temp1.
[595714.313505] Inside ext4_sample_last_mounted. Before: sizeof(sbi->s_es->s_last_mounted): 64.
[595714.313506] Inside ext4_sample_last_mounted. After: sbi->s_es->s_last_mounted: /home/overlay/ext4-m/temp1.
[595714.313507] Inside ext4_sample_last_mounted. After: cp: /home/overlay/ext4-m/temp1.
[595714.313507] Inside ext4_sample_last_mounted. After: sizeof(sbi->s_es->s_last_mounted): 64.


https://github.com/torvalds/linux/commit/bc0b0d6d69ee9022f18ae264e62beb30ddeb322a
https://lkml.org/lkml/2021/1/18/787

*/
static int ext4_sample_last_mounted(struct super_block *sb,
				    struct vfsmount *mnt)
{
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	struct path path;
	char buf[64], *cp;
	handle_t *handle;
	int err;

	//printk("Inside ext4_sample_last_mounted\n");

	if (likely(sbi->s_mount_flags & EXT4_MF_MNTDIR_SAMPLED))
		return 0;

	if (sb_rdonly(sb) || !sb_start_intwrite_trylock(sb))
		return 0;

	sbi->s_mount_flags |= EXT4_MF_MNTDIR_SAMPLED;
	/*
	 * Sample where the filesystem has been mounted and
	 * store it in the superblock for sysadmin convenience
	 * when trying to sort through large numbers of block
	 * devices or filesystem images.
	 */
	memset(buf, 0, sizeof(buf));
	path.mnt = mnt;
	path.dentry = mnt->mnt_root;
	cp = d_path(&path, buf, sizeof(buf));
	err = 0;
	if (IS_ERR(cp))
		goto out;

	handle = ext4_journal_start_sb(sb, EXT4_HT_MISC, 1);
	err = PTR_ERR(handle);
	if (IS_ERR(handle))
		goto out;
	BUFFER_TRACE(sbi->s_sbh, "get_write_access");
	err = ext4_journal_get_write_access(handle, sbi->s_sbh);
	if (err)
		goto out_journal;


	//printk("Inside ext4_sample_last_mounted. mount point sample string.\n");	
	//printk("Inside ext4_sample_last_mounted. Before: sbi->s_es->s_last_mounted: %s.\n", sbi->s_es->s_last_mounted);	
	//printk("Inside ext4_sample_last_mounted. Before: cp: %s.\n", cp);	
	//printk("Inside ext4_sample_last_mounted. Before: sizeof(sbi->s_es->s_last_mounted): %d.\n", sizeof(sbi->s_es->s_last_mounted));	

	strlcpy(sbi->s_es->s_last_mounted, cp,
		sizeof(sbi->s_es->s_last_mounted));

	//printk("Inside ext4_sample_last_mounted. After: sbi->s_es->s_last_mounted: %s.\n", sbi->s_es->s_last_mounted);	
	//printk("Inside ext4_sample_last_mounted. After: cp: %s.\n", cp);	
	//printk("Inside ext4_sample_last_mounted. After: sizeof(sbi->s_es->s_last_mounted): %d.\n", sizeof(sbi->s_es->s_last_mounted));	


	ext4_handle_dirty_super(handle, sb);
out_journal:
	ext4_journal_stop(handle);
out:
	sb_end_intwrite(sb);
	return err;
}

/*Custom version of dquot_file_open()
 * Generic helper for ->open on filesystems supporting disk quotas.
 */
int scorw_dquot_file_open(struct inode *inode, struct file *file)
{
	int error;

	error = generic_file_open(inode, file);
	//if (!error && (file->f_mode & FMODE_WRITE))
	if (!error)
		error = dquot_initialize(inode);
	return error;
}

//opening file 
static int ext4_file_open(struct inode * inode, struct file * filp)
{
	int ret;
	
	struct scorw_inode *p_scorw_inode = NULL;
	struct scorw_inode *c_scorw_inode = NULL;

	//printk("\n\n============================================");
	//printk("ext4_file_open() called. inode->i_ino: %lu\n", inode->i_ino);
	if (unlikely(ext4_forced_shutdown(EXT4_SB(inode->i_sb))))
		return -EIO;

	ret = ext4_sample_last_mounted(inode->i_sb, filp->f_path.mnt);
	//printk("ret = ext4_sample_last_mounted(inode->i_sb: %, filp->f_path.mnt);");

	if (ret)
		return ret;

	ret = fscrypt_file_open(inode, filp);
	if (ret)
		return ret;

	ret = fsverity_file_open(inode, filp);
	if (ret)
		return ret;

	/*
	 * Set up the jbd2_inode if we are opening the inode for
	 * writing and the journal is present
	 */
	//printk("ext4_file_open() called. (filp->f_mode & FMODE_WRITE): %d\n", (filp->f_mode & FMODE_WRITE));
//	if (filp->f_mode & FMODE_WRITE) {
		ret = ext4_inode_attach_jinode(inode);
		if (ret < 0)
			return ret;
//	}


	//////////// scorw start //////////


	//print info about inodes in scorw inodes list
        //scorw_print_inode_list();

	//printk("ext4_file_open: checking whether opened file (%lu) is a scorw file\n", inode->i_ino);

	if(scorw_is_child_file(inode, 1))
	{
		//This lock makes sure that open(par),close(par) and creation of par scorw inodes inside special_open() happens atomically
		mutex_lock(&(inode->i_vfs_inode_open_close_lock));
		atomic_add(1, &(inode->i_vfs_inode_open_count));

		//printk("ext4_file_open: Yes! opened file is a child file\n");
		c_scorw_inode = scorw_get_inode(inode, 1, 0);	

		mutex_unlock(&(inode->i_vfs_inode_open_close_lock));
	}
	else if(scorw_is_par_file(inode, 1))
	{
		//This lock makes sure that open(par),close(par) and creation of par scorw inodes inside special_open() happens atomically
		mutex_lock(&(inode->i_vfs_inode_open_close_lock));
		atomic_add(1, &(inode->i_vfs_inode_open_count));

		//printk("ext4_file_open: Yes! opened file is a parent file\n");
		p_scorw_inode = scorw_get_inode(inode, 0, 0);

		mutex_unlock(&(inode->i_vfs_inode_open_close_lock));
	}

	//////////// scorw end //////////

	filp->f_mode |= FMODE_NOWAIT;
	//return dquot_file_open(inode, filp);
	return scorw_dquot_file_open(inode, filp);
}


/*
 * ext4_llseek() handles both block-mapped and extent-mapped maxbytes values
 * by calling generic_file_llseek_size() with the appropriate maxbytes
 * value for each.
 */
loff_t ext4_llseek(struct file *file, loff_t offset, int whence)
{
	struct inode *inode = file->f_mapping->host;
	loff_t maxbytes;


	//printk("Inside ext4_llseek\n");

	if (!(ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS)))
		maxbytes = EXT4_SB(inode->i_sb)->s_bitmap_maxbytes;
	else
		maxbytes = inode->i_sb->s_maxbytes;

	switch (whence) {
	default:
		return generic_file_llseek_size(file, offset, whence,
						maxbytes, i_size_read(inode));
	case SEEK_HOLE:
		inode_lock_shared(inode);
		offset = iomap_seek_hole(inode, offset,
					 &ext4_iomap_report_ops);
		inode_unlock_shared(inode);
		break;
	case SEEK_DATA:
		inode_lock_shared(inode);
		offset = iomap_seek_data(inode, offset,
					 &ext4_iomap_report_ops);
		inode_unlock_shared(inode);
		break;
	}

	if (offset < 0)
		return offset;
	return vfs_setpos(file, offset, maxbytes);
}

/*
 * defined in xattr.h
struct helper_inodes
{
        unsigned long p_inode_num;  //parent's inode number
        unsigned long f_inode_num;  //friend's inode number
        int num_ranges;             //how many ranges to store corresponding a child?
        struct child_range range[MAX_RANGES_SUPPORTED];
};
*/

//Overlayfs checks whether a filesystem supports efficient cloning 
//or not while performing copy up operation.
//If a filesystem provides remap_file_range implementation,
//then, it is used by overlayfs to handover the cloning 
//operation to filesystem,
//else, it does copy up itself by 
//block by block copying
//
//Return value: size of src/dest file in bytes
loff_t scorw_remap_file_range(struct file *src_file, loff_t off,
		struct file *dst_file, loff_t destoff, loff_t len,
		unsigned int remap_flags)
{
	struct helper_inodes h_inodes;
	struct file *frnd_file;
	int LEN = 256;
	char frnd_file_name[LEN];
	char *src_file_name = src_file->f_path.dentry->d_name.name;

	//prepare frnd file name
	memset(frnd_file_name, '\0', LEN);
	BUG_ON(strlen(src_file_name) > LEN-6);
	strlcpy(frnd_file_name, src_file_name, strlen(src_file_name)+1); 
	//printk("src: %s, strlen(src_file_name): %d, creating frnd file: %s\n", src_file_name, strlen(src_file_name), frnd_file_name);
	strlcat(frnd_file_name, "_frnd", strlen(src_file_name) + 6);
	//printk("strlen(src_file_name): %d, creating frnd file: %s\n", strlen(src_file_name), frnd_file_name);

	//create frnd file
	frnd_file = filp_open(frnd_file_name, O_RDWR | O_CREAT, 0644);
	BUG_ON(IS_ERR(frnd_file));

	//prepare metadata needed for clone
	h_inodes.p_inode_num = src_file->f_inode->i_ino;
	h_inodes.f_inode_num = frnd_file->f_inode->i_ino;
	h_inodes.num_ranges = 0;

	//close frnd file
	filp_close(frnd_file, NULL);

	//perform clone operation
	ext4_xattr_set(dst_file->f_inode, 1, "SCORW_PARENT", &h_inodes, sizeof(struct helper_inodes), 0);


	///////////////////////////////////////////////////////
	//Create data structures for already open child file
	//Note: code is taken from ext4_file_open()
	///////////////////////////////////////////////////////
        //This lock makes sure that open(par),close(par) and creation of par scorw inodes inside special_open() happens atomically
        mutex_lock(&(dst_file->f_inode->i_vfs_inode_open_close_lock));
        scorw_get_inode(dst_file->f_inode, 1, 0);
        mutex_unlock(&(dst_file->f_inode->i_vfs_inode_open_close_lock));
        //////////// scorw end //////////
	//printk("%s() clone performed\n", __func__);

	return len;
}

const struct file_operations ext4_file_operations = {
	.llseek		= ext4_llseek,
	.read_iter	= ext4_file_read_iter,
	.write_iter	= ext4_file_write_iter,
	.unlocked_ioctl = ext4_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ext4_compat_ioctl,
#endif
	.mmap		= ext4_file_mmap,
	.mmap_supported_flags = MAP_SYNC,
	.open		= ext4_file_open,
	.release	= ext4_release_file,
	.fsync		= ext4_sync_file,
	.get_unmapped_area = thp_get_unmapped_area,
	.splice_read	= generic_file_splice_read,
	.splice_write	= iter_file_splice_write,
	.fallocate	= ext4_fallocate,
	//scorw start
	.remap_file_range = scorw_remap_file_range,	
	//scorw end
//	.flush		= scorw_file_close,
};

const struct inode_operations ext4_file_inode_operations = {
	.setattr	= ext4_setattr,
	.getattr	= ext4_file_getattr,
	.listxattr	= ext4_listxattr,
	.get_acl	= ext4_get_acl,
	.set_acl	= ext4_set_acl,
	.fiemap		= ext4_fiemap,
};

