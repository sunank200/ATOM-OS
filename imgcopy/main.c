#include<stdio.h>
#include<string.h>
#include "fscalls.h"

void exit(int status);
void closeall(void);
void initialize(void);
int main(int argc, char  **argv)
{
	char s[10000];
	int k,sum=0;
	int fd0;
	FILE *fd1;

	initialize();

	if (argc < 3)
	{
		printf("Usage:\n\ncpfile -o fimage-source host-target\ncpfile host-source fimage-target\n");
		exit(0);
	}
	if (strcmp(argv[1], "-o") == 0)
	{
		fd0 = syscall_open(argv[2],O_RDONLY,0);
		fd1 = fopen(argv[3],"wb");

		if (fd0 < 0 || fd1 < 0)
		{
			printf("Error opening input/output files\n");
			exit(0);
		}
		while ((k=syscall_read(fd0, s, 10000))>0)
		{
			fwrite(s,k,1,fd1);
			sum += k;
		}
		syscall_close(fd0);
		fclose(fd1);
	}
	else
	{
		fd0 = syscall_open(argv[2],O_WRONLY,0);
		fd1 = fopen(argv[1],"rb");
		if (fd0 < 0 || fd1 < 0)
		{
			printf("Error opening input/output files\n");
			exit(0);
		}
		
		while ((k=fread(s, 1, 10000, fd1))>0)
		{
			syscall_write(fd0,s,k);
			sum += k;
		}
		syscall_close(fd0);
		fclose(fd1);
	}
	closeall();
	//printf("Image copied\n");
	return 0;
}
