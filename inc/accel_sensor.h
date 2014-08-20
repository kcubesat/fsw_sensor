/*
 * accel_sensor.h
 * LIS3LV02DQ_SPI
 * Created on : July 20, 2014
 *     Author : Han sang hyuk, Kim sung keun
*/
#ifndef ACCEL_SENSOR_H_
#define ACCEL_SENSOR_H_
#include <stdint.h>

#include <dev/spi.h>

/** Parameters describing **/

typedef struct{
	int low;
	int high;
} x_val_t;

/**
 * Helper function to setup a spi_chip_t.
 * This will setup the chip select in the correct SPI mode and speed
 * to communicate with the accel_sensor.
 * @param spi_dev pointer to a spi device (must be initialised)
 * @param spi_chip pointer to a spi chip where the settings are stored
 * @param cs the chip select of the current accel_sensor
 */
void accel_sensor_spi_setup_cs(spi_dev_t * spi_dev, spi_chip_t * spi_chip, uint8_t cs);

/**
 * Get ACCEL_SENSOR temperature
 * @param chip
 * @return temperature in deg. C
 */
float accel_sensor_read_accel(spi_chip_t * chip);

/**
 * Get ACCEL_SENSOR temperature in RAW
 * @param chip
 * @return temperature in RAW
 */
int16_t accel_sensor_read_raw(spi_chip_t * chip);

int cmd_accel_sensor_init(struct command_context *ctx);
int cmd_accel_sensor_test(struct command_context *ctx);

#endif /* ACCEL_SENSOR_H_ */
