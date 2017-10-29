/*
Used in "main.c" by "system()" function.
Below code is same with original code.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define DRIVER_NAME		"/dev/cncled"

#define INDEX_LED		0
#define INDEX_REG_LED		1
#define INDEX_GREEN_LED		2
#define INDEX_BLUE_LED		3
#define INDEX_MAX		4

void main(int argc , char **argv)
{
	unsigned short colorArray[INDEX_MAX];

	int fd;

	colorArray[INDEX_LED] =(unsigned short) atoi(argv[1]);

	colorArray[INDEX_REG_LED] =(unsigned short) atoi(argv[2]);
	colorArray[INDEX_GREEN_LED] =(unsigned short) atoi(argv[3]);
	colorArray[INDEX_BLUE_LED] =(unsigned short) atoi(argv[4]);

	// open  driver
	fd = open(DRIVER_NAME,O_RDWR);

	write(fd,&colorArray,6);

	close(fd);
}
