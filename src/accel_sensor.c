#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <dev/spi.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

/* reference gyro */
static void accel_sensor_write_reg(spi_chip_t * chip, uint8_t reg_base, uint16_t value) {
	if (spi_lock_dev(chip->spi_dev) <0)
		return;
	spi_write(chip, ((reg_base + 1) << 8) | 0x8000 | ((value >> 8) & 0xff));
	//printf("Writing %#x\r\n", ((reg_base + 1) << 8) | 0x8000 | ((value >> 8) & 0xff));
	spi_read(chip);
	spi_write(chip, (reg_base << 8) | 0x8000 | (value & 0xff));
	//printf("Writing %#x\r\n", (reg_base << 8) | 0x8000 | (value & 0xff));
	spi_read(chip);
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
/* reference lm70 */
void accel_sensor_spi_setup_cs(spi_dev_t * spi_dev, spi_chip_t * spi_chip, uint8_t cs) {
	spi_chip->spi_dev = spi_dev;	//A pointer to the physical device SPI0
	spi_chip->baudrate = 9600;	//This is only the initial baud rate, it will be increased by the driver
	spi_chip->spi_mode = 3;		// SPI mode
	spi_chip->bits = 16;		// Default value for transferring bytes
	spi_chip->cs = cs; 		// The chip select number
	spi_chip->reg = cs/4;		// The chip select register, The register bank to use
	spi_chip->stay_act = 0;		// Should the chip-select stay active until next SPI transmission? //reference lm70
	spi_chip->spck_delay = 4;	// Delay
	spi_chip->trans_delay = 60;	// Delay
	spi_setup_chip(spi_chip);
}

float accel_sensor_read_accel(spi_chip_t * chip) {
	if (spi_lock_dev(chip->spi_dev) < 0)
		return 0;
	int16_t x_val = accel_sensor_read_reg(chip, ACCEL_SENSOR_DATA_ACCEL_X_OUT) & 0x28; // low out x 
	int16_t y_val = accel_sensor_read_reg(chip, ACCEL_SENSOR_DATA_ACCEL_Y_OUT) & 0x2A; // low out y
	int16_t z_val = accel_sensor_read_reg(chip, ACCEL_SENSOR_DATA_ACCEL_Z_OUT) & 0x2C; // low out z
	return x_val, y_val, z_val;
}

/*
int16_t accel_sensor_read_raw(spi_chip_t * chip) {
	if (spi_lock_dev(chip->spi_dev) < 0)
		return 0;
	spi_write(chip, 0x0000);
	spi_unlock_dev(chip->spi_dev);
	return spi_read(chip);
}
*/

