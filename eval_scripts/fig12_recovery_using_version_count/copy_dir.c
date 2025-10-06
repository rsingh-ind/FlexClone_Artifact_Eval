#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

//Assuming, there are no sub-directories in directory containing parent files
void process_dir(char *src_dir, char *dest_dir, char *helper_dir)
{
	int error = 0;
	DIR *dirp = 0;
	struct dirent *dentry = 0;	
	char *mkdir_command = 0;
	int mkdir_command_size = 0;
	char *copy_command = 0;
	int copy_command_size = 0;
	int ret = 0;
	struct stat statbuf;
	unsigned long child_inode_num = 0;
	char *symlink_path = 0;
	int symlink_name_len = 32;
	char symlink_name[symlink_name_len];
	int symlink_path_len = 0;
	int symlink_dir_len = 0;
	char *symlink_command = 0;
	int symlink_command_size = 0;

	//src
	char *src_child_dir_path = 0;
	int src_child_dir_name_len = 0;
	int src_child_dir_path_len = 0;
	int src_dir_len = 0;

	//dest
	char *dest_child_dir_path = 0;
	int dest_child_dir_name_len = 0;
	int dest_child_dir_path_len = 0;
	int dest_dir_len = 0;

	//helper
	char *helper_child_dir_path = 0;
	int helper_child_dir_name_len = 0;
	int helper_child_dir_path_len = 0;
	int helper_dir_len = 0;


	dirp = opendir(src_dir);
	if(dirp == NULL)
	{
		printf("opendir failed for following directory: %s\n", src_dir);
		perror("opendir failed: ");
	}
	assert(dirp != NULL);

	//printf("%s: src_dir: %s, dest_dir: %s, helper_dir: %s\n", __func__, src_dir, dest_dir, helper_dir);
	/*
	error = mkdir(dest_dir, 0775);
	assert(error == 0);

	error = mkdir(helper_dir, 0775);
	assert(error == 0);
	*/

	//create destination directory
	//(create intermediate directories if they don't exist)
	mkdir_command_size = strlen(dest_dir) + 128;
	mkdir_command = malloc(sizeof(char) * mkdir_command_size);
	assert(mkdir_command != NULL);
	memset(mkdir_command, '\0', mkdir_command_size);

	sprintf(mkdir_command, "mkdir -p %s", dest_dir);
	system(mkdir_command);
	free(mkdir_command);

	//create helper directory
	//(create intermediate directories if they don't exist)
	mkdir_command_size = strlen(helper_dir) + 128;
	mkdir_command = malloc(sizeof(char) * mkdir_command_size);
	assert(mkdir_command != NULL);
	memset(mkdir_command, '\0', mkdir_command_size);

	sprintf(mkdir_command, "mkdir -p %s", helper_dir);
	system(mkdir_command);
	free(mkdir_command);

	errno = 0;
	while(dentry = readdir(dirp))
	{
		if(dentry->d_type != DT_REG)
		{
			continue;
		}

		//new src dir path
		src_dir_len = strlen(src_dir);	
		src_child_dir_name_len = strlen(dentry->d_name);
		src_child_dir_path_len = src_dir_len + 1 + src_child_dir_name_len + 1;

		src_child_dir_path = malloc(sizeof(char) * src_child_dir_path_len);
		assert(src_child_dir_path != NULL);
		memset(src_child_dir_path, '\0', src_child_dir_path_len);

		sprintf(src_child_dir_path, "%s/%s", src_dir, dentry->d_name); 

		//new dest dir path
		dest_dir_len = strlen(dest_dir);	
		dest_child_dir_name_len = strlen(dentry->d_name);
		dest_child_dir_path_len = dest_dir_len + 1 + dest_child_dir_name_len + 1;

		dest_child_dir_path = malloc(sizeof(char) * dest_child_dir_path_len);
		assert(dest_child_dir_path != NULL);
		memset(dest_child_dir_path, '\0', dest_child_dir_path_len);

		sprintf(dest_child_dir_path, "%s/%s", dest_dir, dentry->d_name); 

		//new helper dir path
		helper_dir_len = strlen(helper_dir);	
		helper_child_dir_name_len = strlen(dentry->d_name) + strlen("_frnd");
		helper_child_dir_path_len = helper_dir_len + 1 + helper_child_dir_name_len + 1;

		helper_child_dir_path = malloc(sizeof(char) * helper_child_dir_path_len);
		assert(helper_child_dir_path != NULL);
		memset(helper_child_dir_path, '\0', helper_child_dir_path_len);

		sprintf(helper_child_dir_path, "%s/%s_frnd", helper_dir, dentry->d_name); 
		printf("helper path: %s\n", helper_child_dir_path);


		//Perform copy operation
		copy_command_size = strlen(src_child_dir_path) + strlen(dest_child_dir_path) + strlen(helper_child_dir_path)+ 128;
		copy_command = malloc(sizeof(char) * copy_command_size);
		assert(copy_command != NULL);
		memset(copy_command, '\0', copy_command_size);

		sprintf(copy_command, "./setxattr_generic -c %s -p %s -f %s", dest_child_dir_path, src_child_dir_path, helper_child_dir_path);
		system(copy_command);


		//create symlink for child file in helper directory (alongside frnd files)
		ret = stat(dest_child_dir_path, &statbuf);
		if(ret < 0)
		{
			printf("Error: Failed to stat file: %s)\n", dest_child_dir_path);

		}

		child_inode_num = statbuf.st_ino;
		memset(symlink_name, '\0', symlink_name_len);
		sprintf(symlink_name, "%lu", child_inode_num);

		symlink_path_len = helper_dir_len + 1 + symlink_name_len + 1;

		symlink_path = malloc(sizeof(char) * symlink_path_len);
		assert(symlink_path != NULL);
		memset(symlink_path, '\0', symlink_path_len);

		sprintf(symlink_path, "%s/%s", helper_dir, symlink_name); 


		symlink_command_size = strlen(dest_child_dir_path) + strlen(symlink_path)+ 128;
		symlink_command = malloc(sizeof(char) * symlink_command_size);
		assert(symlink_command != NULL);
		memset(symlink_command, '\0', symlink_command_size);

		sprintf(symlink_command, "ln -s %s %s", dest_child_dir_path, symlink_path);
		system(symlink_command);

		//cleanup
		free(symlink_command);
		free(copy_command);
		free(src_child_dir_path);
		free(dest_child_dir_path);
		free(helper_child_dir_path);
	}
	assert(errno == 0);
	closedir(dirp);
}

int main(int argc, char* argv[])
{
	if(argc != 4)
	{
		printf("Usage: %s <path of src directory to copy> <dest path where to copy> <helper path (Eg: to store frnd files)>\n", argv[0]);
		printf("Note: setxattr utility should be present in current directory (where %s is present)\n", argv[0]);
		return -1;
	}

	if((argv[1][0] != '/') || (argv[2][0] != '/') || (argv[3][0] != '/'))
	{
		printf("Error: Pass absolute paths!\n");
		return -1;
	}

	process_dir(argv[1], argv[2], argv[3]);

	return 0;
}
