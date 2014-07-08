#ifndef TEMP_SENSOR_H_
#define TEMP_SENSOR_H_

#include <dev/spi.h>

/**
 * Helper function to setup a spi_chip_t.
 * This will setup the chip select in the correct SPI mode and speed
 * to communicate with the temp_sensor.
 * @param spi_dev pointer to a spi device (must be initialised)
 * @param spi_chip pointer to a spi chip where the settings are stored
 * @param cs the chip select of the current temp_sensor
 */
void temp_sensor_spi_setup_cs(spi_dev_t * spi_dev, spi_chip_t * spi_chip, uint8_t cs);

/**
 * Get TEMP_SENSOR temperature
 * @param chip
 * @return temperature in deg. C
 */
float temp_sensor_read_temp(spi_chip_t * chip);

/**
 * Get TEMP_SENSOR temperature in RAW
 * @param chip
 * @return temperature in RAW
 */
int16_t temp_sensor_read_raw(spi_chip_t * chip);

#endif /* TEMP_SENSOR_H_ */
