#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define LEN 32

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		printf("Usate: %s <path to directory containing child files>\n", argv[0]);
		exit(-1);
	}
	int fd = 0;
	int mode = 0;
	int ret = 0;
	DIR* dir = 0;
	struct dirent* dirent = 0;
	char* filepath;
	int len = 0;
	char *config = 0;
	struct stat statbuf;
	char size[LEN];
	char* fio_filename = 0;
	char fio_cmd[LEN];
	char* fio_script_name = "fio_script";

	
	dir = opendir(argv[1]);
	assert(dir != NULL);

	while((dirent = readdir(dir)) != NULL)
	{
		if((strcmp(dirent->d_name, ".") == 0) || (strcmp(dirent->d_name, "..") == 0))
		{
			continue;
		}

		len = strlen(argv[1]) + 1 + strlen(dirent->d_name) + 1;
		filepath = malloc(len * sizeof(char));
		assert(filepath != NULL);

		memset(filepath, '\0', len);
		strcpy(filepath, argv[1]);
		strcat(filepath, "/");
		strcat(filepath, dirent->d_name);

		printf("filepath: %s\n", filepath);

		//generate fio script to write to file
		/* Sample script:
			[randWrite]
			rw=randwrite
			size=8g
			ioengine=sync
			invalidate=0
			allow_file_create=0
			thread
			numjobs=1
			bs=3072
			filename=8GB_child
		*/
		fd = open(fio_script_name, O_WRONLY | O_TRUNC | O_CREAT, 0644);
		assert(fd >= 0);

		config="[job]\n";
		write(fd, config, strlen(config));

		//perform sequential and random write alternatively
		if(mode == 0)
		{
			config="rw=write\n";
			write(fd, config, strlen(config));
			mode = 1;
		}
		else
		{
			config="rw=randwrite\n";
			write(fd, config, strlen(config));
			mode = 0;
		}

		ret = stat(filepath, &statbuf);
		assert(ret != -1);

		memset(size, '\0', LEN);
		sprintf(size, "size=%lu\n", statbuf.st_size);
		write(fd, size, strlen(size));

		//fill 50% of file
		memset(size, '\0', LEN);
		sprintf(size, "io_size=%lu\n", statbuf.st_size / 2);
		write(fd, size, strlen(size));

		config="ioengine=sync\n";
		write(fd, config, strlen(config));

		config="invalidate=0\n";
		write(fd, config, strlen(config));

		config="allow_file_create=0\n";
		write(fd, config, strlen(config));

		config="thread\n";
		write(fd, config, strlen(config));

		config="numjobs=1\n";
		write(fd, config, strlen(config));

		config="bs=4096\n";
		write(fd, config, strlen(config));

		fio_filename = malloc(strlen(filepath) + LEN);
		assert(fio_filename != NULL);
		memset(fio_filename, '\0', strlen(filepath) + LEN);
		sprintf(fio_filename, "filename=%s\n", filepath);
		write(fd, fio_filename, strlen(fio_filename));


		//run fio
		memset(fio_cmd, '\0', LEN);
		sprintf(fio_cmd, "fio %s", fio_script_name);
		system(fio_cmd);

		close(fd);
		free(filepath);
		free(fio_filename);
	}

	closedir(dir);
	return 0;
}
