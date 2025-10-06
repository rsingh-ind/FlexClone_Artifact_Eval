#include<stdio.h>
#include<assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#define MAGIC_NUM 0xMAGIC_EXT4
#define SIZE 1048576 


void isFn(char *buf, int end)
{
	printf("isFn() called\n");
	int i = 0;
	int j = 0; 
	int start = -1;
	char curC = '\0';
	
	//printf("Finding ending of prev fn from j: %d\n", j);
	for(j = end-1; j >= 0; j--)
	{
		//printf("%c", buf[j]);
		//Ending of prev. function found?
		if(buf[j] == '}')
		{
			start = j+1;
			break;
		}
	}
	/*
	printf("\nTentative Ending of prev. fn found at j: %d, start: %d, end: %d\n", j, start, end);
	printf("Tentative string: \n");
	for(i = start; i < end; i++)
	{
		printf("%c", buf[i]);
	}
	printf("\n\n");
	*/

	if(start>= 0)
	{
		for(i = start; i < end; i++)
		{
			curC = buf[i];
			printf("%c", curC);
			if(!(
				(curC >= 'a' && curC <= 'z') 
				|| (curC >= 'A' && curC <= 'Z')
				|| (curC >= '0' && curC <= '1')
				|| (curC == ' ')
				|| (curC == '*')
				|| (curC == '_')
				|| (curC == '(')
				|| (curC == ')')
				|| (curC == '[')
				|| (curC == ']')
				|| (curC == '\n')
				|| (curC == ',')
		  	))
			{
				break;
				printf("Break on this char\n");
			}
		}
		printf("\n");

		if(i == end)
		{
			printf("fn Defn Found\n");
			for(i = start; i < end; i++)
			{
				printf("%c", buf[i]);
			}
			printf("\n");
		}
	}
		
}

int main(int argc, char* argv[])
{
	char buf[SIZE];
	if(argc != 2)
	{
		printf("Format: $./executable <filename>");
	}
	char *filename = argv[1];
	int i = 0;
	int fd = 0;
	int nRead = 0;
	char prevC = '\0';
	char prevNC = '\0';	//char at prev newline 
	char curC = '\0';
	int fnDefnStart = 0;	//Todo: handle the case when fnDefnStart cycles
				//Eg: fnDefnStart = 4090, i = 10
	int fnDefn = 0;
	int newLineStart = 0;


	fd = open(filename, O_RDWR);
	assert(fd > 2);

	nRead = read(fd, buf, SIZE);
	i = 0;
	while(i < nRead)
	{
		curC = buf[i];
		if(curC == '{')
		{
			isFn(buf, i);
		}
		++i;
	}	
	
	close(fd);
	return 0;
	
}
