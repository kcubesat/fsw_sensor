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
#include <freertos/queue.h>


#include <dev/arm/at91sam7.h>
#include <dev/arm/cpu_pm.h>		//extern cpu_core_clk for use


#define CAMERA_BAUD		57600	//uCAM2 : 57600, LinkSprite : 38400

/* uCam.cpp reference */
char   _SYNC_COMMAND[6] = {0xAA,0x0D,0x00,0x00,0x00,0x00};
char   _ACK_COMMAND[6] =  {0xAA,0x0E,0x0D,0x00,0x00,0x00};
char   _INITIAL[6] =      {0xAA,0x01,0x00,0x07,0x00,0x01}; //Current image: Smallest
char   _PACK_SIZE[6] =    {0xAA,0x06,0x08,0x00,0x02,0x00};
char   _SNAPSHOT[6] =     {0xAA,0x05,0x00,0x00,0x00,0x00};
char   _GET_PICTURE[6]=   {0xAA,0x04,0x01,0x00,0x00,0x00};
char   _PACKET_ACK[6] =   {0xAA,0x0E,0x00,0x00,0x00,0x00};

	char recv_msg[10];
/* camera_test  */
int camera_usart_test(struct command_context *ctx)
{
	char * args = command_args(ctx);
	int testmode;
/* testmode 1 : puttest */
/* testmode 2 : gettest */
/* testmode 3 : normal */
	if (sscanf(args, "%u", &testmode) != 1)          return 0;

	int i = 0;
	int iter_cnt = 60;
	int cmd_len = 6;

	usart_init(1, cpu_core_clk, CAMERA_BAUD);	// use USART1
	printf ("init sync : baud = %d\n", CAMERA_BAUD);

	if (testmode == 1)
	{
		
		for ( i = 0 ; i < 3; i++ )
			usart_putstr(1, _SYNC_COMMAND, 6);

		printf ("putstr sync : command = ");
		for( i = 0; i < cmd_len; i++)
			printf ("%x ", _SYNC_COMMAND[i]);
		printf ("\n");

	} else if(testmode == 2) 
	{
		printf ("getc sync : recv = ", recv_msg);
		for ( i = 0 ; i < 12; i++ )
		{
			recv_msg[i] = usart_getc(1);
			printf ("%x ", recv_msg[i]);
		}
		printf ("\n");
	} else 
	{
		for ( i = 0 ; i < 3; i++ )
		{
			usart_putstr(1, _SYNC_COMMAND, 6);
			vTaskDelay(configTICK_RATE_HZ * 0.2);
		}

		printf ("putstr sync : command = ");
		for( i = 0; i < cmd_len; i++)
			printf ("%x ", _SYNC_COMMAND[i]);
		printf ("\n");

		printf ("getc sync : recv = ", recv_msg);
		for ( i = 0 ; i < 12; i++ )
		{
			recv_msg[i] = usart_getc(1);
			printf ("%x ", recv_msg[i]);
		}
		printf ("\n");
	}
	
	return 0;
}	
