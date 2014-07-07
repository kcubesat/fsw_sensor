/** @file fsw_sensor_spi.c
 *
 */

/*

#include <stdio.h>
#include <string.h>

#include <dev/spi.h>
#include <dev/ap7/cache.h>
#include <dev/ap7/addrspace.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <util/log.h>


static spi_chip_t *sd_chip;  / * Pointer to SPI structuer * /

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

*/


