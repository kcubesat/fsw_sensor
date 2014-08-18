/*
 * accel_sensor_i2c.h
 * LIS3LV02DQ_I2C
 *  Created on: July 23, 2014
 *      Author: Han sang hyuk, Kim sung keun
 */


#include <command/command.h>

typedef struct accel_sensor_data_s {
        float x;
        float y;
        float z;
} accel_sensor_data_t;

int accel_sensor_i2c_test(struct command_context *ctx);
