/**
 * camera_usart.c
 * LinkSprite JPEG camera or uCAM2
 * Created on : Aug 7, 2014
 * Author : han sang hyuk , kim sung keun
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
char   _INITIAL[6] =      {0xAA,0x01,0x00,0x06,0x01,0x00}; // 16bit color RAW RGB, 80*60
char   _PACK_SIZE[6] =    {0xAA,0x06,0x08,0x00,0x02,0x00};
char   _SNAPSHOT[6] =     {0xAA,0x05,0x01,0x00,0x00,0x00};
char   _GET_PICTURE[6]=   {0xAA,0x04,0x02,0x00,0x00,0x00};
char   _PACKET_ACK[6] =   {0xAA,0x0E,0x00,0x00,0x00,0x00};

/* camera_init  */
int camera_usart_init(struct command_context *ctx)
{
	usart_init(1, cpu_core_clk, CAMERA_BAUD);	// use USART1
	printf ("init : baudrate = %d\n", CAMERA_BAUD);
return 0;
}

/* camera_test  */
int camera_usart_test(struct command_context *ctx)
{
	char * args = command_args(ctx);
	int testmode;
/* testmode 1 : put test */
/* testmode 2 : get test */
/* testmode 3 : normal */
	if (sscanf(args, "%u", &testmode) != 1)          return 0;

	int i = 0;
	int cmd_len = 6;
	char recv_msg[11];

	if (testmode == 1)
	{
		for ( i = 0 ; i < 3; i++ )
			usart_putstr(1, _SYNC_COMMAND, 6);

		printf ("putstr test : command = ");
		for( i = 0; i < cmd_len; i++)
			printf ("%x ", _SYNC_COMMAND[i]);
		printf ("\n");

	} else if(testmode == 2) 
	{
		printf ("getc test : recv = ", recv_msg);
		for ( i = 0 ; i < 12; i++ )
		{
			recv_msg[i] = usart_getc(1);
			printf ("%x ", recv_msg[i]);
		}
		printf ("\n");
	} else 
	{
		for ( i = 0 ; i < 2; i++ )
		{
			usart_putstr(1, _SYNC_COMMAND, 6);
		}
		printf ("putstr test : command = ");
		for( i = 0; i < cmd_len; i++)
			printf ("%x ", _SYNC_COMMAND[i]);
		printf ("\n");
//		vTaskDelay(configTICK_RATE_HZ * 0.2);
		printf ("getc test : recv = ", recv_msg);
		for ( i = 0 ; i < 12; i++ )
		{	
			recv_msg[i] = usart_getc(1);
			printf ("%x ", recv_msg[i]);
		}
			printf ("\n");
	}
}


int camera_usart_sync(struct command_context *ctx)
{
	int i = 0;
	int attempt = 0;
	int cmd_len = 6;
	char recv_msg[11];
	unsigned char ack_counter;
	unsigned char ack_received=0;

	while (attempt < 30)
	{
		attempt = attempt++;
		usart_putstr(1, _SYNC_COMMAND, 6);
		printf ("putstr sync command = ");
		for( i = 0; i < cmd_len; i++)
			printf ("%x ", _SYNC_COMMAND[i]);
		printf ("\n");
//		vTaskDelay(configTICK_RATE_HZ * 0.2);
		printf ("getc sync recv = ", recv_msg);
		for ( i = 0 ; i < 12; i++ )
		{	
			recv_msg[i] = usart_getc(1);
			printf ("%x ", recv_msg[i]);
		}
		printf ("\n");
		if (recv_msg[0] == 0xAA && recv_msg[1] == 0x0E &&
		recv_msg[2] == 0x0D && recv_msg[4] == 0x00 &&
		recv_msg[5] == 0x00) 
		{
			ack_counter = recv_msg[3];
			usart_putstr(1, _ACK_COMMAND, 6);
//			read(fd, recv_msg, 6);
			if (recv_msg[6] == 0xAA && recv_msg[7] == 0x0D &&
                      	recv_msg[8] == 0x00 && recv_msg[9] == 0x00 &&
                      	recv_msg[10] == 0x00 && recv_msg[11] == 0x00) 
			{
                               	ack_received = 1;
                              	break;
                      	}
              	}
	}
}

/* camera_picture_get 
	camera_setting and snapshot and get */

int camera_picture_get(struct command_context *ctx)
{
	int i = 0;
	int attempt = 0;
	int cmd_len = 6;
	char recv_ack[6];
	char recv_msg[6];
	char recv_pic[4803];		// is ok queue size ?? need check
					// picture data ID 'AA 0A 01' +3byte

	/* initial  */
	while (attempt <5) {
		attempt = attempt++;
		usart_putstr(1, _INITIAL, 6);
		printf ("camera setting command = ");
		for( i = 0; i < cmd_len; i++)
			printf ("%x ", _INITIAL[i]);
		printf ("\n");
	
		printf ("camera setting recv = ", recv_ack);
		for ( i = 0 ; i < 6; i++ )
		{	
			recv_ack[i] = usart_getc(1);
			printf ("%x ", recv_ack[i]);
		}
		printf ("\n");
		if (recv_ack[0] == 0xAA && recv_ack[1] == 0x0E &&
		recv_ack[2] == 0x01 && recv_ack[4] == 0x00 &&
		recv_ack[5] == 0x00) 
		{
			break; }

	}
	/* snapshot  *//*
	usart_putstr(1, _SNAPSHOT, 6);
	printf ("snapshot command = ");
	for ( i = 0 ; i < 6; i++ )
	{	
		recv_msg[i] = usart_getc(1);
		printf ("%x ", recv_msg[i]);
	}
	printf ("\n");
*/

	/* get picture */
	usart_putstr(1, _GET_PICTURE, 6);
	printf ("get pic command = ");
	for( i = 0; i < cmd_len; i++)
		printf ("%x ", _GET_PICTURE[i]);
	printf ("\n");

	printf ("get pic recv = ", recv_msg);
	for ( i = 0 ; i < 6; i++ )
	{	
		recv_msg[i] = usart_getc(1);
		printf ("%x ", recv_msg[i]);
	}
	printf ("\n");
	
	printf ("ing = ", recv_msg);
	for ( i = 0 ; i < 4803; i++ )
	{	
		recv_pic[i] = usart_getc(1);
		printf (".");
	}
}

