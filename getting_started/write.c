#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#define BLOCK_SIZE (4096)

int main(int argc, char* argv[])
{
	int n = 0;
	int fd = 0;
	unsigned long sOffset = 0;	//start offset
	unsigned long length = 0;
	int writeSize = 0;	//amount of data written in single write()
	int maxWriteSize = 0; 
	int sec = 0;
	char *buf = 0;
	
	srand(getpid());
	
	if(argc != 4)
	{
		printf("./prog filename startoffset length\n");
		return -1;
	}

	fd = open(argv[1], O_WRONLY , 0644);
	assert(fd >= 0);

	sOffset = atoll(argv[2]);
	assert(sOffset >= 0);

	length = atoll(argv[3]);
	assert(length > 0);

	maxWriteSize = BLOCK_SIZE; 
	buf = malloc(sizeof(char) * maxWriteSize);	
	assert(buf != 0);

	sec = 0;

	lseek(fd, sOffset, SEEK_SET); 
	
	while(length > 0)
	{
		for(int j = 0; j < maxWriteSize; j++)
		{
			n = rand()%26;
			buf[j] = (n) + 'A';
		}
		writeSize = (maxWriteSize < length ? maxWriteSize : length);
		write(fd, buf, writeSize);
		length -= writeSize;
		sleep(sec);
	}

	free(buf);
	close(fd);
}
