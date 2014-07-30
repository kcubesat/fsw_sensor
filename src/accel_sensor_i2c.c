/*
 * accel_sensor_i2c.c
 * LIS3LV02DQ_I2C
 * Created on : July 23, 2014
 *     Author : Kim sung keun
 */

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <dev/arm/at91sam7.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <dev/i2c.h>
#include <dev/usart.h>
#include <dev/magnetometer.h>

#include <util/error.h>
#include <util/console.h>
#include <util/hexdump.h>
#include <util/clock.h>
#include <util/csp_buffer.h>

#include <accel_sensor_i2c.h>

#define ACCEL_SENSOR_CTRL_WHO_AM_I              0x0F
#define ACCEL_SENSOR_CTRL_REG1                  0x20    //Power on device, enable all axis, and turn off self test

//#define ACCEL_SENSOR_CTRL_

#define ACCEL_SENSOR_DATA_ACCEL_X_OUT_LOW       0x28
#define ACCEL_SENSOR_DATA_ACCEL_Y_OUT_LOW       0x2A
#define ACCEL_SENSOR_DATA_ACCEL_Z_OUT_LOW       0x2C
#define ACCEL_SENSOR_DATA_ACCEL_X_OUT_HIGH      0x29
#define ACCEL_SENSOR_DATA_ACCEL_Y_OUT_HIGH      0x2B
#define ACCEL_SENSOR_DATA_ACCEL_Z_OUT_HIGH      0x2D

// hmc5843.c [magnetometer] reference

typedef struct accel_sensor_reg_data_s {
	uint8_t id1;
	uint8_t id2;
	uint8_t id3;
	uint8_t conf_a;
	uint8_t conf_b;
	uint8_t mode;
	uint8_t x[2];
	uint8_t y[2];
	uint8_t z[2];
	uint8_t status;
} accel_sensor_reg_data_t;

// default values 
static int is_initialised = pdFALSE;
//static int meas = MAG_MEAS_NORM;
//static int gain = MAG_GAIN_1_0;
//static int rate = MAG_RATE_10;
//static int defaultmode = MAG_MODE_IDLE;
//static float scale = 1/1.3;

// Write register"reg" with value "val"
static int accel_sensor_write_reg(uint8_t reg, uint8_t val) {
  uint8_t txdata[2];
  txdata[0] = reg;
  txdata[1] = val;
  return i2c_master_transaction(2,0x1D, &txdata, 2,NULL,0,2);
}


// Read register "reg" with value "val"
static int accel_sensor_read_reg(uint8_t reg, uint8_t *val) {
  uint8_t txdata[1];
  txdata[0] = reg;
  return i2c_master_transaction(2,0x1D, &txdata, 1,val,1,2);
}


static int accel_sensor_on(void) {
	uint8_t val;
	val=accel_sensor_write_reg(0x20, 0x87);  // device on 
	return val;
}

static int accel_sensor_off(void) {
	uint8_t val;
	val=accel_sensor_write_reg(0x20, 0x07); // device off
	return val;
}



// Setup i2c to hmc5843(mag) ??????
/* void accel_sensor_init(void) {

	if(is_initialised)
		return;
//	 Turn-on-time: 200 us 
	UPIO_ECR = 1;
	UPIO_MDDR = 0;
	UPIO_OER = 0x00000001;
	if (UPIO_PDSR & 0x00000001) {
		UPIO_CODR = 0x00000001;
		vTaskDelay(configTICK_RATE_HZ * 0.5);
	}
	UPIO_SODR = 0x00000001;
	vTaskDelay(configTICK_RATE_HZ * 0.5);
	
//	 Setup I2C 
//	i2c_init(0, I2C_MASTER, 0x06, 60, 5, 5, NULL);

	vTaskDelay(configTICK_RATE_HZ * 0.2);

	is_initialised = pdTRUE;

}
*/



// Perform read of data-registers from hmc5843
int accel_sensor_read(mag_data_t * data) {

        int16_t tmpx, tmpy, tmpz;
        uint8_t txdata[2];
        uint8_t rxdata[2];
        int retval1, retval2; // retval3, retval4, retval5, retval6;

        txdata[0] = ACCEL_SENSOR_DATA_ACCEL_X_OUT_LOW;
	txdata[1] = ACCEL_SENSOR_DATA_ACCEL_X_OUT_HIGH;
//	txdata[2] = ACCEL_SENSOR_DATA_ACCEL_Y_OUT_LOW;
//	txdata[3] = ACCEL_SENSOR_DATA_ACCEL_Y_OUT_HIGH;
//	txdata[4] = ACCEL_SENSOR_DATA_ACCEL_Z_OUT_LOW;
//	txdata[5] = ACCEL_SENSOR_DATA_ACCEL_Z_OUT_HIGH;
	retval1 = i2c_master_transaction(0,0x1D, &txdata[0], 1, &rxdata[0],1,2);
	retval2 = i2c_master_transaction(0,0x1D, &txdata[1], 1, &rxdata[1],1,2);
//	retval3 = i2c_master_transaction(1,0x1D, txdata[2], 1,rxdata[2],1,2);
//	retval4 = i2c_master_transaction(1,0x1D, txdata[3], 1,rxdata[3],1,2);	
//	retval5 = i2c_master_transaction(1,0x1D, txdata[4], 1,rxdata[4],1,2);
//	retval6 = i2c_master_transaction(1,0x1D, txdata[5], 1,rxdata[5],1,2);
        if (retval1 == E_NO_ERR) {

                // Data is returned in a slave-frame structure 
                tmpx = rxdata[0] << 8 | rxdata[1];
//                tmpy = rxdata[2] << 8 | rxdata[3];
//                tmpz = rxdata[4] << 8 | rxdata[5];

		data->x = (float) tmpx ;
//                data->y = (float) tmpy * 1/1024;
//                data->z = (float) tmpz * 1/1024;

                return E_NO_ERR;
        } else {
                return E_TIMEOUT;
        }
}

// Do loop measurements
int accel_sensor_i2c_test(struct command_context *ctx) {
//	accel_sensor_init();
        accel_sensor_data_t data;
	uint8_t WHO_AM_I;	
	
        uint8_t txstatus[2];
	uint8_t rxstatus[2];
	uint8_t txpoweron[2];
	uint8_t txpoweroff[2];
	txstatus[0] = ACCEL_SENSOR_CTRL_WHO_AM_I;
	txstatus[1] = ACCEL_SENSOR_CTRL_REG1;
	txpoweron[0] = ACCEL_SENSOR_CTRL_REG1;
	txpoweron[1] = 0x87;
	txpoweroff[0] = ACCEL_SENSOR_CTRL_REG1;
	txpoweroff[1] = 0x07;	

	i2c_master_transaction(0,0x1D, &txstatus[0], 1, &rxstatus[0],1,2);
	i2c_master_transaction(0,0x1D, &txpoweron, 2, NULL,0,2);
	
//	i2c_master_transaction(2,0x1D, &txdata, 2,NULL,0,2);
//	accel_sensor_write_reg(ACCEL_SENSOR_CTRL_WHO_AM_I, &WHO_AM_I);

//	uint8_t val;	
//	val=accel_sensor_on();
	printf ("who am i : %x\n\r", rxstatus[1]);
//	printf ("device on : %x\n\r", val);
	
        while (1) {

                if (usart_messages_waiting(USART_CONSOLE) != 0)
//			i2c_master_transaction(2,0x1D, 0x2007, 2,NULL,0,2);		
                        break;

//		printf ("y: %x\n\r", accel_sensor_read(&data));

                if (accel_sensor_read(&data) == E_NO_ERR) {
                        console_clear();
                        printf("X: %4.1f G\n\r", data.x);
                }

                vTaskDelay(configTICK_RATE_HZ * 0.100);
        }
//	accel_sensor_off();
        return CMD_ERROR_NONE;
}
