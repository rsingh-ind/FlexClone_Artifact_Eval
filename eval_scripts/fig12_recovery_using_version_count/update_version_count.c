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

int main(int argc, char *argv[])
{
	int ret = 0;
	char *child_name = 0; 
	DIR *child_dir = 0;
	struct dirent *dirent = 0;
	unsigned count = 0;
	unsigned long child_version_count = 0;

	if(argc != 3)
	{
		printf("Usage: %s <child files directory> <how many child files to update? Eg: 5000>\n", argv[0]);
		exit(-1);
	}

	count = atol(argv[2]);

	//enter directory containing child files
	ret = chdir(argv[1]);
	assert(ret == 0);

	//open directory containing child files
	child_dir = opendir(".");
	assert(child_dir != NULL);

	errno = 0;

	//Note: Assuming, directory containing child files doesn't contain sub-directories
	while(dirent = readdir(child_dir))
	{
		if((strcmp(dirent->d_name, ".") == 0) || (strcmp(dirent->d_name, "..") == 0))
		{
			continue;
		}

		if(count == 0)
		{
			break;
		}
		count--;

		child_name = dirent->d_name;

		//read version count of child file
		ret = getxattr(child_name, "user.v", &child_version_count, 8);
		assert(ret != -1);

		//change child version count
		child_version_count += 5;
		ret = setxattr(child_name, "user.v", &child_version_count, 8, XATTR_REPLACE);
		assert(ret != -1);
	}
	assert(errno == 0);

	return 0;
}
