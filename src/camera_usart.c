/**
 * camera_usart.c
 * LinkSprite JPEG camera or uCAM2
 * Created on : Aug 7, 2014
 * Author : kim sung keun
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <dev/arm/at91sam7.h>
#include <dev/arm/cpu_pm.h>		//extern cpu_core_clk for use


#define CAMERA_BAUD		38400

/* uCam.cpp reference */
char   _SYNC_COMMAND[6] = {0xAA,0x0D,0x00,0x00,0x00,0x00};
char   _ACK_COMMAND[6] =  {0xAA,0x0E,0x0D,0x00,0x00,0x00};
char   _INITIAL[6] =      {0xAA,0x01,0x00,0x07,0x00,0x01}; //Current image: Smallest
char   _PACK_SIZE[6] =    {0xAA,0x06,0x08,0x00,0x02,0x00};
char   _SNAPSHOT[6] =     {0xAA,0x05,0x00,0x00,0x00,0x00};
char   _GET_PICTURE[6]=   {0xAA,0x04,0x01,0x00,0x00,0x00};
char   _PACKET_ACK[6] =   {0xAA,0x0E,0x00,0x00,0x00,0x00};

	char sync;
/* camera_test  */
int camera_usart_test(struct command_context *ctx_) {
	usart_init(1, cpu_core_clk, CAMERA_BAUD);	// use USART1
	
	usart_putstr(1, (char*)_SYNC_COMMAND, 6);

	sync=usart_getc(1);
	printf("sync %x", sync);

	return 0;
}	
