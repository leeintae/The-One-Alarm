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


#define BUS_LED_DRIVER_NAME "/dev/cnled"

int main(int argc , char **argv)
{
	int ledNo = 0;
	int ledControl = 0;
	int wdata ,rdata,temp ;

	int fd;

	if (isalnum((int)argv[1][0])) // number
	{
		ledNo = (int)(argv[1][0] - '0');
	}

	if ( (argv[2][0] == '0' ) || ( argv[2][0] == '1' ))
	{
		ledControl = (int)(argv[2][0] - '0');
	}

	// open  driver
	fd = open(BUS_LED_DRIVER_NAME,O_RDWR);

	// control led

	if ( ledNo == 0 )
	{
		if ( ledControl ==  1 ) wdata = 0xff;
		else wdata = 0;
	}
	else
	{
		read(fd,&rdata,4);
		temp = 1;

		if ( ledControl ==1 )
		{
			temp <<=(ledNo-1);
			wdata = rdata | temp;
		}
		else
		{
			temp =  ~(temp<<(ledNo-1));
			wdata = rdata & temp;
		}
	}
	write(fd,&wdata,4);

	close(fd);

	return 0;
}
