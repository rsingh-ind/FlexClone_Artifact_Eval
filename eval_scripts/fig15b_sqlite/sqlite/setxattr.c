#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/xattr.h>
 
#define MAX_RANGES_SUPPORTED 	4
#define BLOCK_SIZE		4096	//4 KB block
#define BLOCK_SIZE_BITS		12	//4 KB block

//range is inclusive of start and end
//Block numbers start from 0
struct child_range
{
	long long start;	//start block num
	long long end;		//end block num
};

struct helper_inodes
{
	unsigned long p_inode;	//parent's inode
	unsigned long f_inode;	//friend's inode
	int num_ranges;		//how many ranges to store corresponding a child?
	struct child_range range[MAX_RANGES_SUPPORTED];	//maintain static snapshot of these blocks
};
 
int main(int argc, char* argv[])
{
	int i = 0;
        int cfd = -1;
        int pfd = -1;
        int ffd = -1;
	int pInoNum = -1;
	int ret = 0;
	int value = 0;
	struct stat statbuf;
	struct helper_inodes h_inodes;
	int valid_args_count = 0;
	int basic_args_count = 4;		//4 basic args  ./prog <child file> <parent file> <friend file>
	int max_args_count = basic_args_count + (MAX_RANGES_SUPPORTED * 2);	//basic args + 2 args per range
	long long prev_blk_num = -1;
	long long cur_blk_num = -1;
	long long num_par_blks = 0;
	int invalid_range_arg = 0;
	int num_ranges = 0;

	char *attr_name = "user.SCORW_PARENT";

	//Make sure the count of the args passed is correct
	for(i = basic_args_count; i <= max_args_count; i=i+2)
	{
		if(i == argc)
		{
			valid_args_count = 1;
			break;
		}
	}

	if(!valid_args_count)
	{
		printf("syntax: ./prog <child file> <parent file> <friend file> [start end start end ..]\n");
		printf("Note: Currently max %d ranges are allowed\n", MAX_RANGES_SUPPORTED);
		return -1;
	}

        //printf("Opening files\n");
        cfd = open(argv[1], O_RDWR | O_CREAT, 0644);
	assert(cfd >= 0);

        pfd = open(argv[2], O_RDWR);
	assert(pfd >= 0);

        ffd = open(argv[3], O_RDWR | O_CREAT, 0644);
	assert(ffd >= 0);

	//printf("Performing fstat call\n");
	fstat(pfd, &statbuf);
	h_inodes.p_inode = statbuf.st_ino;
	num_par_blks = (((statbuf.st_size) % BLOCK_SIZE) == 0) ? (statbuf.st_size >> BLOCK_SIZE_BITS) : (1 + (statbuf.st_size >> BLOCK_SIZE_BITS));

	fstat(ffd, &statbuf);
	h_inodes.f_inode = statbuf.st_ino;
	//printf("Returned from fstat call\n");

	//Initially, no range is valid
	for(i=0; i < MAX_RANGES_SUPPORTED; i++)
	{
		h_inodes.range[i].start = -1;	
		h_inodes.range[i].end = -1;
	}
	//perform sanity checks on the range arguments
	//1) No 2 ranges should overlap
	//2) Ranges should be in increasing order
	//3) Range can't be negative
	//4) Range should fit in par file size
	prev_blk_num = -1;
	invalid_range_arg = 0;
	for(i=basic_args_count; i<argc; i++)
	{
		cur_blk_num = atoll(argv[i]);

		//ranges overlap (or) ranges are not in increasing order (or) range is negative
		if(cur_blk_num <= prev_blk_num)
		{
			invalid_range_arg = 1;
			break;
		}

		//Range doesn't fit par file size
		if(cur_blk_num >= num_par_blks)
		{
			invalid_range_arg = 2;
			break;
		}
		prev_blk_num = cur_blk_num;
	}
	
	if(invalid_range_arg)
	{
		if(invalid_range_arg == 1)
		{
			printf("Error: ranges overlap (or) ranges are not in increasing order (or) range is negative\n");
		}
		else if(invalid_range_arg == 2)
		{
			printf("Error: range doesn't fit par file size\n");
		}
		else
		{
			printf("Error: some other range error\n");
		}
		return -1;
	}

	//prepare range array
	num_ranges = ((argc - basic_args_count) / 2);
	if(num_ranges == 0)
	{
		//No range supplied. So, cover full range.
		h_inodes.range[0].start = 0;	
		h_inodes.range[0].end = num_par_blks-1;
		h_inodes.num_ranges = 1;
	}
	else
	{ 	//Cover provided range
		for(i=0; i < num_ranges; i++)
		{
			h_inodes.range[i].start = atoll(argv[(2*i)+basic_args_count]);
			h_inodes.range[i].end = atoll(argv[(2*i)+1+basic_args_count]);
		}
		h_inodes.num_ranges = num_ranges;
	}

	/*
	for(i=0; i < MAX_RANGES_SUPPORTED; i++)
	{
		printf("Range %d: %lld:%lld, num_par_blks: %lld, h_inodes.num_ranges: %d\n", i, h_inodes.range[i].start, h_inodes.range[i].end, num_par_blks, h_inodes.num_ranges);
	}
	*/


	//printf("Performing setxattr call\n");
	ret = fsetxattr(cfd, attr_name , &h_inodes, sizeof(struct helper_inodes), 0);
	if(ret == -1)
	{
		perror("setxattr failed");
		return -1;
	}
	//printf("Returned from setxattr call\n");

 
        //printf("closing files\n");
        close(cfd);
        close(pfd);
        close(ffd);

	//This call is required to initialize the frnd file
        cfd = open(argv[1], O_RDWR, 0644);

	return 0;
}
