/** @file fsw_sensor_spi.c
 *
 */

/*

#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <dev/spi.h>
#include <dev/ap7/cache.h>
#include <dev/ap7/addrspace.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <util/log.h>

#include <fat_sd/integer.h>
#include <fat_sd/ffconf.h>
#include "diskio.h"


extern spi_dev_t spi_dev;

static int temp_sensor_map = {0};	// chip CS
static spi_chip_t temp_sensor_chip;

temp_sensor_spi_setup_cs(&spi_dev, &temp_sensor_chip, temp_sensor_map);
temp_sensor_read_temp(chip);
temp_sensor_read_raw(chip);

///////////////////////////////////

// test (reference cmd_panels.c 

int cmd_temp_sensor_test(struct command_context *ctx) {

	while(1) {
		if (usart_messages_waiting(USART_CONSOLE) != 0)
			break;

		printf("Temp %f\r\n", temp_sensor_read_temp(&temp_sensor_chip));
		vTaskDelay(100);
	}
	return CMD_ERROR_NONE;
}

static spi_chip_t *sd_chip;  // Pointer to SPI structuer 
static unsigned int baudrate; // The baudrate to run at full speed

// setup th SPI0 hardware 
spi_dev.variable_ps = 0;  // Set CS once, not for each read/write operation
spi_dev.pcs_decode = 1;   // Use chip select mux
spi_dev.index = 0;        // Use SPI0
spi_init_dev(&spi_dev);   // change  'dev-arm/spi/spi.c'

// #ifdef ENABLE_SD 

static inline void xmit_spi(BYTE dat) {
	spi_write(sd_chip, dat);
	spi_read(sd_chip);
	printf("Write o: %x\r\n", dat);
}

static inline BYTE rcvr_spi(void) {
	spi_write(sd_chip, 0xFF);
	BYTE c = spi_read(sd_chip);
	printf("Read %x\r\n", c);
}

static inline void rcer_spi_m(BYTE *dst) { 
	*dst = rcvr_spi();
}

char sd_spi_init(spi_chip_t * chip) {
	sd_chip = chip;
	baudrate = sd_chip->baudrate;
#if USE_DMA
	memset(ffbuff, 0xFF, 512);
#endif
	return sd_disk_initialize();
}
	
*/
