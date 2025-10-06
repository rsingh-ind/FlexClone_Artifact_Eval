#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/xattr.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define BUF_SIZE 256

double what_time_is_it()
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return (double)now.tv_sec + ((double)now.tv_nsec)*1e-9;
}

//Assuming, name of frnd file ends with string '_frnd<integers(optional)>'
//Eg: 4MB_frnd, 4MB_frnd1, 4MB_frnd123 etc.
int is_frnd_file(char *filename)
{
	int i = 0;
	int len = strlen(filename);
	char *last_alphabet_pos = 0;

	//printf("Checking whether '%s' (len: %d) is a frnd file or not\n", filename, len);
	if(len < 5)	//filename should at min. contain characters '_frnd'
	{
		return 0;
	}

	i = len-1;
	last_alphabet_pos = &filename[i];
	while(i > 4)
	{
		if(isdigit(*last_alphabet_pos))
		{
			//printf("i: %d, char: '%c' is a digit\n", i, *last_alphabet_pos);
			i--;
			last_alphabet_pos = &filename[i];
			continue;
		}
		break;
	}

	//printf("filename[%d]: %c, filename[%d]: %c, filename[%d]: %c, filename[%d]: %c, filename[%d]: %c,\n", i, filename[i], i-1, filename[i-1], i-2, filename[i-2], i-3, filename[i-3], i-4, filename[i-4]);
	if((filename[i] == 'd') && (filename[i-1] == 'n') && (filename[i-2] == 'r') && (filename[i-3] == 'f') && (filename[i-4] == '_'))
	{
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int fd = 0;
	int op = 0;
	int ret = 0;
	char *frnd_name = 0;
	DIR *frnd_dir = 0;
	struct dirent *dirent = 0;
	char child_name[BUF_SIZE]; 
	unsigned long child_inode_num = 0;
	unsigned long child_version_count = 0;
	unsigned long frnd_version_count = 0;
	unsigned long num_files_needing_recovery = 0;
	double t1 = 0.0;
    	double t2 = 0.0;

	if(argc != 3)
	{
		printf("Usage: %s <frnd files directory> <what to do ('0' or '1')? 0: just print count of files needing recovery 1: perform actual recovery\n", argv[0]);
		exit(-1);
	}

	t1 = what_time_is_it();
	op = atoi(argv[2]);

	//enter directory containing frnd files
	ret = chdir(argv[1]);
	assert(ret == 0);

	//open directory containing frnd files
	frnd_dir = opendir(".");
	assert(frnd_dir != NULL);

	errno = 0;

	//Match version count of each frnd and its associated child file
	//Note: Assuming, directory containing frnd files doesn't contain sub-directories
	while(dirent = readdir(frnd_dir))
	{
		//skip file that is not a frnd file
		if(!is_frnd_file(dirent->d_name))
		{
			continue;
		}

		frnd_name = dirent->d_name;

		//read version count of frnd file
		ret = getxattr(frnd_name, "user.v", &frnd_version_count, 8);
		assert(ret != -1);

		//read inode num of the child associated with frnd file
		ret = getxattr(frnd_name, "user.SCORW_CHILD", &child_inode_num, 8);
		assert(ret != -1);
		//printf("frnd name: %s, frnd inode num: %lu, child inode num: %lu\n", frnd_name, dirent->d_ino, child_inode_num);

		//generate child symlink name
		memset(child_name, '\0', BUF_SIZE);
		sprintf(child_name, "%lu", child_inode_num);

		//read version count of child file
		ret = getxattr(child_name, "user.v", &child_version_count, 8);
		assert(ret != -1);

		//Need recovery?
		if(child_version_count != frnd_version_count)
		{
			++num_files_needing_recovery;
			//printf("child version cnt: %lu and frnd version cnt: %lu mismatch!\n", child_version_count, frnd_version_count);

			//Trigger recovery of frnd file
			if(op)
			{
				fd = open(child_name, O_RDWR);
				assert(fd != 0);
				close(fd);
			}

		}
		else
		{
			//printf("child version cnt: %lu and frnd version cnt: %lu match!\n", child_version_count, frnd_version_count);
		}

	}
	assert(errno == 0);
	t2 = what_time_is_it();

	//Note: units of t1 and t2 are seconds. Thus, t2 - t1 is also in seconds.
	//Recall, to convert result from seconds to milliseconds, we have to multiply result by 1000,
	//To convert result from seconds to microseconds, we have to multiply result by 1000000
	//printf("Time taken: %g ms\n", (t2-t1)*1000);
	printf("%g\n", (t2-t1)*1000);

	//printf("===========================================\n");
	//printf("Num friend files needing recovery: %lu\n", num_files_needing_recovery);
	//printf("===========================================\n");

	return 0;
}
