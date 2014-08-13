/* 
 * accel_sensor.c
 * LIS3LV02DQ_SPI
 * Created on : July 20, 2014
 *     Author : Kim sung keun
*/
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <dev/spi.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <accel_sensor.h>

#define ACCEL_SENSOR_CTRL_WHO_AM_I		0x0F
#define ACCEL_SENSOR_CTRL_REG1			0x20	//Power on device, enable all axis, and turn off self test

#define ACCEL_SENSOR_DATA_ACCEL_X_OUT_LOW	0x28
#define ACCEL_SENSOR_DATA_ACCEL_Y_OUT_LOW	0x2A
#define ACCEL_SENSOR_DATA_ACCEL_Z_OUT_LOW	0x2C
#define ACCEL_SENSOR_DATA_ACCEL_X_OUT_HIGH	0x29
#define ACCEL_SENSOR_DATA_ACCEL_Y_OUT_HIGH	0x2B
#define ACCEL_SENSOR_DATA_ACCEL_Z_OUT_HIGH	0x2D


/* reference gyro */

static void accel_sensor_write_reg(spi_chip_t * chip, uint8_t reg_base, uint16_t value) {
	if (spi_lock_dev(chip->spi_dev) < 0)
		return;
//	spi_write(chip, ((reg_base + 1) << 8) | 0x8000 | ((value >> 8) & 0xff));
	spi_write(chip, ((reg_base << 8) | 0x0000 | (value & 0xff) ));
	//printf("Writing %#x\r\n", ((reg_base + 1) << 8) | 0x8000 | ((value >> 8) & 0xff));
	spi_read(chip);
//	spi_write(chip, (reg_base << 8) | 0x8000 | (value & 0xff));
	//printf("Writing %#x\r\n", (reg_base << 8) | 0x8000 | (value & 0xff));
//	spi_read(chip);
	spi_unlock_dev(chip->spi_dev);
}

static uint16_t accel_sensor_read_reg(spi_chip_t * chip, uint8_t reg_base) {
	uint16_t val;
	if (spi_lock_dev(chip->spi_dev) < 0)
		return 0;
	spi_write(chip, (reg_base << 8));
	val = spi_read(chip);
	//printf("Read %#x\r\n", val);
	spi_write(chip, (reg_base << 8));
	val = spi_read(chip);
	//printf("Read %#x\r\n", val);
	spi_unlock_dev(chip->spi_dev);
	return val;
}

/* reference lm70 */
void accel_sensor_spi_setup_cs(spi_dev_t * spi_dev, spi_chip_t * spi_chip, uint8_t cs) {
	spi_chip->spi_dev = spi_dev; 	// A pointer to the physical device SPI0
	spi_chip->baudrate = 9800; 	// This is only initial baud rate, it will be increased by the driver
	spi_chip->spi_mode = 2;		// SPI mode
	spi_chip->bits = 16;		// Default value for transferring bytes
	spi_chip->cs = cs; 		// The chip select number
	spi_chip->reg = cs/4;		// The chip select register, The register bank to use
	spi_chip->stay_act = 0;		// Should the chip-select stay active until next SPI transmission? //reference lm70
	spi_chip->spck_delay = 4;	// Delay
	spi_chip->trans_delay = 60;	// Delay
	spi_setup_chip(spi_chip);
}

void accel_sensor_spi_setup(spi_chip_t * chip) {
//	accel_sensor_write_reg(chip, ACCEL_SENSOR_CTRL_REG1, 0x8700); // device on
	uint16_t val=0;
	uint16_t val2=0;
	printf("val : %d\r\n" ,val);
	printf("val2: %d\r\n" ,val);
	
	val = accel_sensor_read_reg(chip, 0x1F);
	val2 = accel_sensor_read_reg(chip, 0x30);
	printf("WHO_AM_I : %d\r\n", val);
	printf("CTRL_REG1 : %d\r\n", val2);
}

float accel_sensor_read_accel(spi_chip_t * chip) {
	x_val_t * x_val;
 
	x_val->low = accel_sensor_read_reg(chip, ACCEL_SENSOR_DATA_ACCEL_X_OUT_LOW); // low out x 
	x_val->high = accel_sensor_read_reg(chip, ACCEL_SENSOR_DATA_ACCEL_X_OUT_HIGH); // low out x 
	printf("x=%d\r\n",x_val);	
	return 0;
}

void accel_sensor_spi_test (struct command_context *ctx) {
	
	spi_write(chip, COMMAND);
	val=spi_read(chip);
}
