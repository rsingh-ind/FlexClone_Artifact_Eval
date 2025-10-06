#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

int main()
{
	int ret = 0;
	struct stat statbuf;
	ret = stat("products.db_child", &statbuf);
	assert(ret == 0);

	printf("child file size: %ld\n", statbuf.st_size);

	return 0;
}
