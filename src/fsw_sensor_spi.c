/** @file fsw_sensor_spi.c
 *
 */


#include <stdio.h>
#include <string.h>

#include <dev/spi.h>
#include <dev/ap7/cache.h>
#include <dev/ap7/addrspace.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <util/log.h>

#include <fat_sd/integer.h>
#include <fat_sd/ffconf.h>
#include "diskio.h"

spi_dev_t spi_dev;
static spi_chip_t temp_sensor_chip[temp_MAP_SIZE];
static spi_chip_t *sd_chip;  // Pointer to SPI structuer 
static unsigned int baudrate; // The baudrate to run at full speed

//* vTaskInit(void * pvParmeters) *//
/* setup th SPI0 hardware */
spi_dev.variable_ps = 0;  // Set CS once, not for each read/write operation
spi_dev.pcs_decode = 1;   // Use chip select mux
spi_dev.index = 0;        // Use SPI0
spi_init_dev(&spi_dev);   // change  'dev-arm/spi/spi.c'

/* #ifdef ENABLE_SD */

static spi_chip_t spi_dummy_chip;
spi_dummy_chip.cs = 0;
spi_dummy_chip.bits = 8;
spi_dummy_chip.spi_dev = &spi_dev;
int i;
for (i =0; i <10; i++) {
	spi_write(&spi_dummy_chip, 0xFF);
	spi_read(&spi_dummy_chip);
}

// Setup the SD card SPI chip, must be started at low speed 

static FATFS fs0;
static spi_chip_t spi_sd_chip;
spi_sd_chip.spi_dev = &spi_dev;     // A pointer to the physicall device SPI0
spi_sd_chip.baudrate = 10000000;    // This is only the initial baud rate, it will be increased by the driver
spi_sd_chip.spi_mode = 0;           // SPI mode 
spi_sd_chip.bits= 8;                // Default value for transferring bytes
spi_sd_chip.cs = CONFIG_SD_CS;      // The SD card is on chip-select 0
spi_sd_chip.reg = CONFIG_SD_CS  / 4;// The SD card in on cd register 0
spi_sd_chip.spck_dealy = 0;         // No delays
spi_sd_chip.trans_delay = 0;        // No delays
spi_setup_chip(&spi_sd_chip);       // change  'dev-arm/spi/spi.c'

vTaskDelay(100);
result = sd_spi_init(&spi_sd_chip);  // change 'libfsw_sensor/src/sd_spi.c'

if (result == 0) {
	result = f_mount(0, &fs0);
	printf("temp sensor detected, mount result %d\r\n", result);
} else {
	printf("temp sensor not found\r\n");
}


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
