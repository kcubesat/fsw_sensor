/*
 * sd_spi.h
 *
 *  Created on: 17/03/2010
 *      Author: oem
 */

#ifndef SD_SPI_H_
#define SD_SPI_H_

#include <dev/spi.h>

char sd_spi_init(spi_chip_t * chip);
char df_spi_init(spi_chip_t * chip);

#endif /* SD_SPI_H_ */
