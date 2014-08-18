/*
 * camera_usart.h
 * LinkSprite JPEG camera or uCAM2 camera
 * Created on : July 20, 2014
 *     Author : Han sang hyuk, Kim sung keun
*/
#ifndef CAMERA_USART_H_
#define CAMERA_USART_H_
#include <stdint.h>
#include <dev/usart.h>

int camera_usart_init(struct command_context *ctx);
int camera_usart_test(struct command_context *ctx);
int camera_usart_sync(struct command_context *ctx);
int camera_picture_get(struct command_context *ctx);
#endif /* CAMERA_USART_H_ */
