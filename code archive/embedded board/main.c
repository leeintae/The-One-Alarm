#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define KEYPAD_DRIVER_NAME "/dev/cnkey"
#define SEVEN_SEGMENT_DRIVER_NAME "/dev/cnfnd"
#define DIPSWITCH_DRIVER_NAME "/dev/cndipsw"
#define MODEMDEVICE "/dev/ttyACM0"
#define BUZZER_DRIVER_NAME "/dev/cnbuzzer"
#define	FBDEV_FILE "/dev/fb0"
#define INPUT_DEVICE_LIST	"/proc/bus/input/devices"
#define EVENT_STR	"/dev/input/event"

#define BAUDRATE B9600
#define FALSE 0
#define TRUE 1

#define MAX_FND_NUM 6
#define DOT_OR_DATA 0x80

//Global variables of main.c
int GAME_HOUR; // game hour (i.e. hour that alarm will ring)
int GAME_MIN; // game minute (i.e. minute that alarm will ring)
int GAME_LEVEL; // game level(1: sound game, 2: text game, 3: heart game)
int GAME_OPTION; // game option(1: low, 2: usual, 3: highly)
// above four values can be set by user using android.

int ALARM_END; // Controls overall flow of this system. ALARM_END = 0 means alarm is ended & ALARM_END = 1 means alarm is ringing.
int MAX_VALUE; // In level 1 game and level 3 game, maximum value that user has to reach.
int SENSOR_VALUE; // Value from Arduino is stored in this variable.

// function for arduino thread
void* get_sensor_value(void* param)
{
	int fd, c, res, i;
	struct termios oldtio, newtio;
	char buf[255];
	FILE* file;
	char* game_type;

	fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY );

	if (fd <0) {perror(MODEMDEVICE); exit(-1); }

	tcgetattr(fd,&oldtio); /* save current port settings */

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME] = 0;   /* inter-character timer unused */
	newtio.c_cc[VMIN] = 3;   /* blocking read until 3 chars received */

	tcflush(fd, TCIFLUSH);
	tcsetattr(fd,TCSANOW,&newtio);

  if(GAME_LEVEL == 1) game_type = "1\n"; // sound game
  else if(GAME_LEVEL == 3) game_type = "3\n"; // heart game

	file = fopen(MODEMDEVICE, "w"); // open arduino device in write mode
	for(i=0;i<2;i++)
	{
		fprintf(file, "%c", game_type[i]); // send game level to arduino
		sleep(1);
	}
	fclose(file);

  while (ALARM_END == 0) // continuously read data from arduino.
	{
		res = read(fd,buf,255);
		buf[res]=0;
		SENSOR_VALUE = atoi(buf); // read value is stored in SENSOR_VALUE
		if(SENSOR_VALUE > MAX_VALUE)
			ALARM_END = 1; // if SENSOR_VALUE exceeds MAX_VALUE, ALARM_END become 1.

		memset(buf, 0, 255); // reset buf[] for next input from arduino
	}

		tcsetattr(fd,TCSANOW,&oldtio);
}//end of get_sensor_value()

// function for buzzer thread
void* run_buzzer(void* param)
{
	int turnOff = 0;
	int m1 = 28;
	int m2 = 30;
	int m3 = 35;
	int m4 = 25;
	int m5 = 27;
	int m6 = 33;

	//open buzzer driver
	int fd = open(BUZZER_DRIVER_NAME, O_RDWR);

	do{
		write(fd, &m1, 4);
		usleep(50000);
		write(fd, &m2, 4);
		usleep(50000);
		write(fd, &m3, 4);
		usleep(50000);
		write(fd, &m4, 4);
		usleep(50000);
		write(fd, &m5, 4);
		usleep(50000);
		write(fd, &m6, 4);
		usleep(50000);
	}while(ALARM_END == 0); // ring buzzer until ALARM_END become 1.

	write(fd, &turnOff, 4); // after ALARM_END became 1 by success of game, turn off the buzzer.

	close(fd);
}//end run_buzzer()

//function for full color led thread
void* run_color_led(void* param)
{
	do{
		system("./cledtest 0 255 0 0");
	}while(ALARM_END == 0); // turn on color led in RED color UNTIL ALARM_END become 1.

	system("./cledtest 0 0 255 0"); // after ALARM_END became 1 by success of game, turn on color led in GREEN color.
}//end of run_color_led()

//function for bus led thread
void* run_bus_led(void* param)
{
	int value = MAX_VALUE/8; // because # of bus led is 8, divide MAX_VALUE by 8.

	system("./ledtest 0 0"); // turn all bus led off for initial state.

	if(GAME_LEVEL == 1 || GAME_LEVEL == 3){ // bus led is used ONLY in level 1 and level 3.
		do{
			switch(SENSOR_VALUE / value){ // as SENSOR_VALUE is larger, by below code, bus led is gradually turned on.
			case 8: system("./ledtest 8 1");
			case 7: system("./ledtest 7 1");
			case 6: system("./ledtest 6 1");
			case 5: system("./ledtest 5 1");
			case 4: system("./ledtest 4 1");
			case 3: system("./ledtest 3 1");
			case 2: system("./ledtest 2 1");
			case 1: system("./ledtest 1 1");
			}
		}while(ALARM_END==0);
		// after ALARM_END became 1, notify to user that alarm game is ended by below code.
		system("./ledtest 0 1");
		sleep(1);
		system("./ledtest 0 0");
		sleep(1);
		system("./ledtest 0 1");
	}
}//end of run_bus_led()

//function for dip switch thread
void* run_dip_switch()
{
	int password = 0xF0F0; // arbitrarily decided super password.

	int fd;
	int readValue;

	// open dip switch driver
	fd = open(DIPSWITCH_DRIVER_NAME, O_RDWR);

	while(ALARM_END == 0)
	{
		read(fd, &readValue, 4); // continuously reads data of dip switched.
		if(readValue == password) ALARM_END = 1; // if read data is same with super password, ALARM_END become 1.
	}
}//end of run_dip_switch()

//functions for 7-segmen
const unsigned short segNum[10] =
{
	0x3F, // 0
	0x06,
	0x5B,
	0x4F,
	0x66,
	0x6D,
	0x7D,
	0x27,
	0x7F,
	0x6F  // 9
};
const unsigned short segSelMask[MAX_FND_NUM] =
{
	0xFE00,
	0xFD00,
	0xFB00,
	0xF700,
	0xEF00,
	0xDF00
};
static struct termios oldt, newt;
void changemode(int dir)
{
	if( dir == 1)
	{
		tcgetattr(STDIN_FILENO , &oldt);
		newt = oldt;
		newt.c_lflag &= ~(ICANON | ECHO );
		tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	}
	else tcsetattr(STDIN_FILENO , TCSANOW, &oldt);
}

int kbhit(void)
{
	struct timeval tv;
	fd_set rdfs;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	FD_ZERO(&rdfs);
	FD_SET(STDIN_FILENO , &rdfs);

	select(STDIN_FILENO + 1 , &rdfs , NULL, NULL, &tv);

	return FD_ISSET(STDIN_FILENO , &rdfs);
}

#define ONE_SEG_DISPLAY_TIME_USEC	1000
// return 1 => exit  , 0 => success
int fndDisp(int driverfile, int num , int dotflag,int durationSec)
{
	int cSelCounter,loopCounter;
	int temp , totalCount, i ;
	unsigned short wdata;
	int dotEnable[MAX_FND_NUM];
	int fndChar[MAX_FND_NUM];

	for (i = 0; i < MAX_FND_NUM ; i++ )
		dotEnable[i] = dotflag & (0x1 << i);

	// if 6 fnd
	temp = num % 1000000;
	fndChar[0]= temp /100000;

	temp = num % 100000;
	fndChar[1]= temp /10000;

	temp = num % 10000;
	fndChar[2] = temp /1000;

	temp = num %1000;
	fndChar[3] = temp /100;

	temp = num %100;
	fndChar[4] = temp /10;

	fndChar[5] = num %10;

	totalCount = durationSec*(1000000 / ONE_SEG_DISPLAY_TIME_USEC);

	cSelCounter = 0;
	loopCounter = 0;
	while(1)
	{
		wdata = segNum[fndChar[cSelCounter]]  | segSelMask[cSelCounter] ;
		if (dotEnable[cSelCounter])
			wdata |= DOT_OR_DATA;

		write(driverfile,&wdata,2);

		cSelCounter++;
		if ( cSelCounter >= MAX_FND_NUM ) cSelCounter = 0;

		usleep(ONE_SEG_DISPLAY_TIME_USEC);

		loopCounter++;
		if ( loopCounter > totalCount ) break;

		if (kbhit())
		{
			if ( getchar() == (int)'q')
			{
				wdata= 0;
				write(driverfile,&wdata,2);
				printf("Exit fndtest\n");
				return 0;
			}
		}
	}

	wdata= 0;
	write(driverfile,&wdata,2);

	return 1;
}
//end of functions for 7-segment

//function for 7-segment thread
void* run_seven_segment(void* param)
{
	int fd;

	if(GAME_LEVEL == 1 || GAME_LEVEL == 3){ // 7-segment is ONLY used in level 1 & level 3.

		//open 7-segment driver
		fd = open(SEVEN_SEGMENT_DRIVER_NAME,O_RDWR);

		changemode(1);

		do{
			fndDisp(fd, SENSOR_VALUE, 0, 1);
		}while(ALARM_END==0); // continuously display SENSOR_VALUE for one second UNTIL ALARM_END become 1.

		fndDisp(fd, MAX_VALUE, 0, 1); // finally, display MAX_VALUE for one second.
		fndDisp(fd, 0, 0, 2);
	}
}//end of run_seven_segment()

//function for textLCD + keypad + dot Matrix  thread
void* run_tlcd_keypad_dot(void* param)
{
	int rdata; // To store read data
	int value; // To store meaningful data
	char YOUR_ANSWER[4]; // four values(meaningful data) are stored in YOUR_ANSWER[]

	int index_no;

	char* answers[4] = { // questions & answers are pre-defined.
		"B49D", "47AD", "E82B", "71FC"
	};

	//open keypad driver
	int fd = open(KEYPAD_DRIVER_NAME,O_RDWR);
	int cnt;

	srand(time(NULL)); // set seed number for random function by using time()
	index_no = rand()%4; // number of question & answer(0 ~ 3)

	while(GAME_LEVEL==2 && ALARM_END==0) // outer while loop is continued until YOUR_ANSWER is correct of alarm is turned off forcibly.
	{
		system("./tlcdtest r"); // clear text lcd.
		sleep(1);
		system("./tlcdtest c 1 1 1 1"); // set cursor at line 1 column 0.
		system("./tlcdtest c 1 1 1 1");
		system("./tlcdtest c 1 1 1 1");
		sleep(1);

		// display question on text lcd's first line.
		if(index_no == 0) system("./tlcdtest w 1011010010011101");
		else if(index_no == 1) system("./tlcdtest w 0100011110101101");
		else if(index_no == 2) system("./tlcdtest w 1110100000101011");
		else if(index_no == 3) system("./tlcdtest w 0111000111111100");

		// answer is 4 digit and 'cnt' counts this.
		cnt = 0;

		while(cnt < 4) // inner while loop is continued until 4 digit(YOUR_ANSWER[]) is inputted.
		{
RR:		read(fd, &rdata, 4); // read one digit.
			if(rdata != 0) // 0 is ignored.
			{
				value = rdata; // meaningful 'rdata' is stored in 'value'.

				while(1) // sleep until user put away from the keypad button.
				{
					read(fd, &rdata, 4);
					if(rdata == 0) break;
				}

				switch(value){ // each keypad's button is regarded as a number by below codes.
				case 1: YOUR_ANSWER[cnt] = '1';
						break;
				case 2: YOUR_ANSWER[cnt] = '2';
						break;
				case 3: YOUR_ANSWER[cnt] = '3';
						break;
				case 4: YOUR_ANSWER[cnt] = '4';
						break;
				case 5: YOUR_ANSWER[cnt] = '5';
						break;
				case 6: YOUR_ANSWER[cnt] = '6';
						break;
				case 7: YOUR_ANSWER[cnt] = '7';
						break;
				case 8: YOUR_ANSWER[cnt] = '8';
						break;
				case 9: YOUR_ANSWER[cnt] = '9';
						break;
				case 10: YOUR_ANSWER[cnt] = 'A';
						break;
				case 11: YOUR_ANSWER[cnt] = 'B';
						break;
				case 12: YOUR_ANSWER[cnt] = 'C';
						break;
				case 13: YOUR_ANSWER[cnt] = 'D';
						break;
				case 14: YOUR_ANSWER[cnt] = 'E';
						break;
				case 15: YOUR_ANSWER[cnt] = 'F';
						break;
				case 16: system("./tlcdtest r 2"); // if user pushed 16th button of keypad, delete all of previous inputted values.
					 system("./tlcdtest w delete_all"); // nofity to user
					 cnt = 0; // clear 'cnt' value.
					 sleep(1);
					 system("./tlcdtest c 1 1 2 1"); // set cursor on second line, 0 column.
					 system("./tlcdtest c 1 1 2 1");
					 system("./tlcdtest c 1 1 2 1");
					 system("./tlcdtest r 2"); // clear second line of text lcd.

					goto RR; // start re-input.
				}//end of switch clause

				system("./tlcdtest r 2");
				system("./tlcdtest c 1 1 2 1");

				// show user's inputted button value.

				if(YOUR_ANSWER[cnt]=='1') system("./tlcdtest w value:1");
				else if(YOUR_ANSWER[cnt]=='2') system("./tlcdtest w value:2");
				else if(YOUR_ANSWER[cnt]=='3') system("./tlcdtest w value:3");
				else if(YOUR_ANSWER[cnt]=='4') system("./tlcdtest w value:4");
				else if(YOUR_ANSWER[cnt]=='5') system("./tlcdtest w value:5 ");
				else if(YOUR_ANSWER[cnt]=='6') system("./tlcdtest w value:6");
				else if(YOUR_ANSWER[cnt]=='7') system("./tlcdtest w value:7");
				else if(YOUR_ANSWER[cnt]=='8') system("./tlcdtest w value:8");
				else if(YOUR_ANSWER[cnt]=='9') system("./tlcdtest w value:9");
				else if(YOUR_ANSWER[cnt]=='A') system("./tlcdtest w value:A");
				else if(YOUR_ANSWER[cnt]=='B') system("./tlcdtest w value:B");
				else if(YOUR_ANSWER[cnt]=='C') system("./tlcdtest w value:C");
				else if(YOUR_ANSWER[cnt]=='D') system("./tlcdtest w value:D");
				else if(YOUR_ANSWER[cnt]=='E') system("./tlcdtest w value:E");
				else if(YOUR_ANSWER[cnt]=='F') system("./tlcdtest w value:F");
				sleep(1);

				cnt++; // increase one to 'cnt'.
			}// end of if clause
		}//end of inner while loop
		system("./tlcdtest r 2");
		sleep(1);
		if(strcmp(YOUR_ANSWER, answers[index_no])==0){ // if user's inputted answer is correct
			system("./dot s 2 00"); // show "ㅇㅇ" on dot matrix.
			ALARM_END = 1; // ALARM_END become 1 because text game is finished.
			break; // escape outer while loop
		}
		else{ // if user's inputted answer is wrong
			system("./dot s 5 11"); // show "ㄴㄴ" on dot matrix.
			system("./tlcdtest c 1 1 2 1");
			system("./tlcdtest c 1 1 2 1");
			system("./tlcdtest c 1 1 2 1");
			system("./tlcdtest w wrong_answer"); // notify to user that your answer is wrong.
			sleep(1);
			system("./tlcdtest c 1 1 2 1");
			system("./tlcdtest r 2");
			continue; // continue outer while loop
		}

	};//end of outer while loop

	//clear text lcd.
	system("./tlcdtest c 1 1 1 1");
	system("./tlcdtest c 1 1 1 1");
	system("./tlcdtest c 1 1 1 1");
	system("./tlcdtest r");
	sleep(1);
}//end of run_keypad()

//start of functions for touchPanel
#define  MAX_BUFF	20

int screen_width;
int screen_height;
int bits_per_pixel;
int line_length;

#define MAX_TOUCH_X	0x740
#define MAX_TOUCH_Y	0x540

void readFirstCoordinate(int fd, int* cx , int* cy)
{
	struct input_event event;
	int readSize;
	while(1)
	{
		readSize = read(fd, &event, sizeof(event));

		if ( readSize == sizeof(event) )
		{
			if( event.type == EV_ABS )
			{
				if (event.code == ABS_MT_POSITION_X )
					*cx = event.value*screen_width/MAX_TOUCH_X;
				else if ( event.code == ABS_MT_POSITION_Y )
					*cy = event.value*screen_height/MAX_TOUCH_Y;
			}
			else if ((event.type == EV_SYN) && (event.code == SYN_REPORT ))
				break;
		}
	}
}

void initScreen(unsigned char *fb_mem )
{
    int coor_y;
    int coor_x;
    unsigned long *ptr;

    for(coor_y = 0; coor_y < screen_height; coor_y++)
    {
        ptr = (unsigned long *)fb_mem + screen_width * coor_y ;
        for(coor_x = 0; coor_x < screen_width; coor_x++)
            *ptr++  =   0x000000;
    }
}

//index values that user clicked on touch panel.
int index_X;
int index_Y;

void* run_touch(void* param)
{
	char eventFullPathName[100];
	int	fb_fd,fp;

	struct fb_var_screeninfo fbvar;
	struct fb_fix_screeninfo fbfix;
	unsigned char* fb_mapped;
	int mem_size;

	sprintf(eventFullPathName,"%s%d",EVENT_STR,2);

	fp = open( eventFullPathName, O_RDONLY);

	access(FBDEV_FILE, F_OK);
	fb_fd = open(FBDEV_FILE, O_RDWR);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &fbvar);
	ioctl(fb_fd, FBIOGET_FSCREENINFO, &fbfix);

	screen_width = fbvar.xres;
	screen_height = fbvar.yres;
	bits_per_pixel = fbvar.bits_per_pixel;
	line_length = fbfix.line_length;

	mem_size = screen_width * screen_height * 4;
	fb_mapped = (unsigned char *)mmap(0, mem_size,PROT_READ|PROT_WRITE, MAP_SHARED, fb_fd, 0);

	initScreen(fb_mapped);

	while(1)
	{
		readFirstCoordinate(fp,&index_X, &index_Y); // continuously read user's click point.
	}
}//end of run_touch()

// Below are functions for change bitmap images(.bmp files)
int is_it_lv1()
{
	if(430 <= index_X && index_X <= 550){
		if(440 <= index_Y && index_Y <= 470)
			return TRUE; // return TRUE if user clicked index value is in displaying "1.bmp" condition.
	}
	return FALSE;
}

int is_it_lv2()
{
	if(430 <= index_X && index_X <= 550){
		if(570 <= index_Y && index_Y <= 600)
			return TRUE; // return TRUE if user clicked index value is in displaying "2.bmp" condition.
	}
	return FALSE;
}

int is_it_lv3()
{
	if(430 <= index_X && index_X <= 550){
		if(700 <= index_Y && index_Y <= 730)
			return TRUE; // return TRUE if user clicked index value is in displaying "3.bmp" condition.
	}
	return FALSE;
}

int is_it_goBack()
{
	if(1110 <= index_X && index_X <= 1160){
		if(60 <= index_Y && index_Y <= 130)
			return TRUE; // return TRUE if user clicked index value is in displaying "main.bmp" condition.
	}
	return FALSE;
}

// function for bmp thread
void* show_bmp(void* param)
{
	int flag; // indicate current displaying bmp file.
	//1: 1.bmp(sound game), 2: 2.bmp(text game), 3: 3.bmp(heart game), 4: main.bmp(main page)

	system("./bitmap bmp/main.bmp"); // display main.bmp initial state.
	flag = 4;

  while(1) // continuously operates.
	{
		if(is_it_lv1()==TRUE && flag==4){
			system("./bitmap bmp/1.bmp");
			flag = 1;
		}else if(is_it_lv2()==TRUE && flag==4){
			system("./bitmap bmp/2.bmp");
			flag = 2;
		}else if(is_it_lv3()==TRUE && flag==4){
			system("./bitmap bmp/3.bmp");
			flag = 3;
		}else if(is_it_goBack()==TRUE && flag!=4){
			system("./bitmap bmp/main.bmp");
			flag = 4;
		}
	}//end of while loop
}//end of show_bmp()


// start of codes for client

// if GAME_LEVEL is 1, MAX_VALUE is used for sound sensor
// and if GAME_LEVEL is 3, MAX_VALUE is used for pulse sensor
// in this function, MAX_VALUE is determined.
void set_game_option()
{
	if(GAME_LEVEL==1){
		switch(GAME_OPTION){
			case 1: MAX_VALUE = 10000; break;
			case 2: MAX_VALUE = 20000; break;
			case 3: MAX_VALUE = 30000; break;
		}
	}else if(GAME_LEVEL==3){
		switch(GAME_OPTION){
			case 1: MAX_VALUE = 2000; break;
			case 2: MAX_VALUE = 3000; break;
			case 3: MAX_VALUE = 4000; break;
		}
	}
}//end of set_game_option()

// function for client thread.
void* run_client(void* param)
{
	int count = 0;
	int option;
	int toSeperate = 0;

	char *token;
	char colon[] = ":"; // client reads data from server, and data are partitioned by ":"

	int n;
	int clientSocket;
	char buffer[1024];
	struct sockaddr_in serverAddr;
	socklen_t addr_size;

	while(1){
		/*---- Create the socket. The three arguments are: ----*/
		/* 1) Internet domain 2) Stream socket 3) Default protocol (TCP in this case) */
REPLAY:		clientSocket = socket(PF_INET, SOCK_STREAM, 0);

		/*---- Configure settings of the server address struct ----*/
		// Address family = Internet
		serverAddr.sin_family = AF_INET;
		// Set port number, using htons function to use proper byte order
		serverAddr.sin_port = htons(8988);
		// Set IP address to localhost
		serverAddr.sin_addr.s_addr = inet_addr("192.168.43.54");
		// Set all bits of the padding field to 0
		memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

		/*---- Connect the socket to the server using the address struct ----*/
		addr_size = sizeof(serverAddr);
		connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size);

		/*---- Read the message from the server into the buffer ----*/
		strcat(buffer, "\r\n");
		bzero(buffer,1024);
	  	n = read(clientSocket,buffer,1023);
		printf("buffer: %s\n", buffer);
		/*---- Parsing the input data to useful ----*/
		token = NULL;

		token = strtok(buffer, colon);

		while(token != NULL) {

			switch(toSeperate) {
			case 0: GAME_HOUR = atoi(token); break;
			case 1: GAME_MIN = atoi(token); break;
			case 2: GAME_LEVEL = atoi(token); break;
			case 3: GAME_OPTION = atoi(token);
				toSeperate = 0; break;
			}
			set_game_option();

			token = strtok(NULL, colon);
			toSeperate++;
		}//end of parsing
		toSeperate--;

		sleep(1);
	}//end of infinite loop
}//end of run_client()

// function for getting current time
struct tm get_current_time()
{
	struct tm *clock;
	time_t current;

	time(&current);
	clock = localtime(&current);

	return (*clock);
}


// start of main()
void main(void)
{
	struct tm current_time; // To contain current time.

	// variables for using threads.
	pthread_t touch;
	pthread_t display_bmp;
	pthread_t arduino;
	pthread_t buzzer;
	pthread_t colorLED;
	pthread_t busLED;
	pthread_t dipswitch;
	pthread_t sevenSegment;
	pthread_t tlcd_keypad_dot;
	pthread_t client;

	// below three threads are always on threads.
	pthread_create(&client, NULL, &run_client, NULL);
	pthread_create(&touch, NULL, &run_touch, NULL);
	pthread_create(&display_bmp, NULL, &show_bmp, NULL);

	while(TRUE) // infinite loop.
	{
		while(TRUE)
		{
			current_time = get_current_time(); // get current time.
			sleep(1);
			if(current_time.tm_hour == GAME_HOUR)
			{
				if(current_time.tm_min == GAME_MIN){
					ALARM_END = 0;  // if current time is same with GAME TIME(GAME_HOUR : GAME_MIN), alarm game starts.
					break;
				}
			}
		}// end of time checking while loop.

		// First, show each game's image on OLED.
		if(GAME_LEVEL == 1){
			system("./oledtest t");
			system("./oledtest i");
			system("./oledtest d img/1.img");
		}else if(GAME_LEVEL == 2){
			system("./oledtest t");
			system("./oledtest i");
			system("./oledtest d img/2.img");
		}else if(GAME_LEVEL == 3){
			system("./oledtest t");
			system("./oledtest i");
			system("./oledtest d img/3.img");
		}

		// creates each component's threads and run.
		pthread_create(&arduino, NULL, &get_sensor_value, NULL);
		pthread_create(&buzzer, NULL, &run_buzzer, NULL);
		pthread_create(&colorLED, NULL, &run_color_led, NULL);
		pthread_create(&busLED, NULL, &run_bus_led, NULL);
		pthread_create(&dipswitch, NULL, &run_dip_switch, NULL);
		pthread_create(&sevenSegment, NULL, &run_seven_segment, NULL);
		pthread_create(&tlcd_keypad_dot, NULL, &run_tlcd_keypad_dot, NULL);

		// while ALARM_END is 0, main thread has to sleep.
		while(ALARM_END == 0)
		{
			;
		}

		// after ALARM_END became 1, main thread wake up for next alarm game.

		// initialize global variables before start next alarm game.
		GAME_HOUR = 0;
		GAME_MIN = 0;
		GAME_LEVEL = 0;
		GAME_OPTION = 0;
		MAX_VALUE = 0;
		SENSOR_VALUE = 0;

		// cancel all threads before start next alarm game.
		pthread_cancel(arduino);
		pthread_cancel(buzzer);
		pthread_cancel(colorLED);
		pthread_cancel(busLED);
		pthread_cancel(dipswitch);
		pthread_cancel(sevenSegment);
		pthread_cancel(tlcd_keypad_dot);

	}//infinite loop
}//end of main
