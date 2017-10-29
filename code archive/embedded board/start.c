/*
For convenient initial setting.
Insert all drivers and set wireless lan.
*/

#include <stdlib.h>

void main(void)
{
	system("insmod drivers/buzzerdrv.ko");
	system("insmod drivers/cleddrv.ko");
	system("insmod drivers/fnddrv.ko");
	system("insmod drivers/leddrv.ko");
	system("insmod drivers/oleddrv.ko");
	system("insmod drivers/btswdrv.ko");
	system("insmod drivers/cdc-acm.ko");
	system("insmod drivers/dipswdrv.ko");
	system("insmod drivers/keydrv.ko");
	system("insmod drivers/mleddrv.ko");
	system("insmod drivers/tlcddrv.ko");

	system("insmod drivers/bcmdhd.ko");
	system("wpa_supplicant -Dwext -iwlan0 -c /etc/wpa_supplicant/wpa_supplicant.conf -B");
	system("ifconfig wlan0 192.168.43.43");
}
