/**
 * DataFlash driver for UFFS on NanoMind
 *
 * @author Jeppe Ledet-Pedersen
 * Copyright 2011 GomSpace ApS. All rights reserved.
 */
  
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <dev/spi.h>
#include <util/console.h>
#include <util/hexdump.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <uffs/uffs_device.h>
#include <uffs/uffs_flash.h>
#include <uffs/uffs_mtb.h>
#include <uffs/uffs_fs.h>
#include <uffs/uffs_fd.h>
#include <uffs/uffs_utils.h>

#include <uffs/at45db642d.h>

/** Pointer to SPI chip structure */
static spi_chip_t * df_spi_chip = NULL;

static int df_timedout(portTickType start_time, uint16_t timeout_ms) {
	return (((xTaskGetTickCount() - start_time) * (1000/configTICK_RATE_HZ)) > timeout_ms);
}

static int df_ready(void) {

	uint8_t tx_buf[2], rx_buf[2];

	/* Send the Status Register Read command. */
	tx_buf[0] = AT45DBX_CMDC_RD_STATUS_REG;
	tx_buf[1] = 0xFF;
	if (spi_dma_transfer(df_spi_chip, tx_buf, rx_buf, 2) == 0)
		return -1;

	return ((rx_buf[1] & AT45DBX_MSK_BUSY) != AT45DBX_BUSY);
	
}

/** Waits until the DF is ready. */
static int df_wait_ready(void) {

	int ready, timeout;
	portTickType start_time = xTaskGetTickCount();

	/* Read the status register until the DF is ready. */
	while (1) {
		timeout = df_timedout(start_time, 30000);
		ready = df_ready();
		if (ready < 0 || timeout)
			return -1;
		if (ready)
			break;
	}

	return 0;
}

/**
 * Generic helper function for reading flash pages.
 * @note This function should only be called by UFFS and not from user space
 * @param dev UFFS device
 * @param block Block number
 * @param page Page number
 * @param data Pointer to data buffer
 * @param data_len
 * @param ecc ECC of data
 * @param space Pointer to spare buffer
 * @param spare_len Length of spare to read
 */
static int df_read_page(uffs_Device *dev, u32 block, u32 page, u8 *data, int data_len, u8 *ecc, u8 *spare, int spare_len) {

	uint32_t mem_addr;
	uint8_t tx_buf[8 + DF_PAGE_DATA_SIZE + DF_PAGE_SPARE_SIZE];
	uint8_t rx_buf[8 + DF_PAGE_DATA_SIZE + DF_PAGE_SPARE_SIZE];

	/* Test for page overflow */
	if (data_len > DF_PAGE_DATA_SIZE || spare_len > DF_PAGE_SPARE_SIZE) {
		printf("UFFS: Attempt to read %u data bytes and %u spare bytes, aborting\r\n", data_len, spare_len);
		return UFFS_FLASH_UNKNOWN_ERR;
	}
	
	/**
	 * To start a page read from the standard DataFlash page size (1056 bytes), an
	 * opcode of D2H must be clocked into the device followed by three address bytes (which comprise
	 * the 24-bit page and byte address sequence) and a series of don’t care bytes (4 bytes if using the
	 * serial interface or 19 bytes if using the 8-bit interface). The first 13 bits (PA12 - PA0) of the 24-bit
	 * address sequence specify the page in main memory to be read, and the last 11 bits (BA10 - BA0)
	 * of the 24-bit address sequence specify the starting byte address within that page.
	 */

	/* Command */
	tx_buf[0] = AT45DBX_CMDA_RD_PAGE;

	/* Address */
	mem_addr  = (((block * dev->attr->pages_per_block + page) & 0x1FFF) << 11);
	tx_buf[1] = (mem_addr & 0xFF0000) >> 16;
	tx_buf[2] = (mem_addr & 0x00FF00) >> 8;
	tx_buf[3] = (mem_addr & 0x0000FF) >> 0;

	/* Don't care bytes */
	tx_buf[4] = 0xFF;
	tx_buf[5] = 0xFF;
	tx_buf[6] = 0xFF;
	tx_buf[7] = 0xFF;

	/**
	 * Following the don’t care bytes, additional pulses on SCK/CLK result in data being output on either the SO (serial
	 * output) pin or the eight output pins (I/O7 - I/O0). The CS pin must remain low during the loading
	 * of the opcode, the address bytes, the don’t care bytes, and the reading of data. When the end of
	 * a page in main memory is reached, the device will continue reading back at the beginning of the
	 * same page. A low-to-high transition on the CS pin will terminate the read operation and tri-state
	 * the output pins (SO or I/O7 - I/O0). The maximum SCK/CLK frequency allowable for the Main
	 * Memory Page Read is defined by the fSCK specification. The Main Memory Page Read bypasses
	 * both data buffers and leaves the contents of the buffers unchanged.
	 */

	/* Extra SCK/CLK for data transfer */
	memset(tx_buf + 8, 0xFF, DF_PAGE_DATA_SIZE + DF_PAGE_SPARE_SIZE);

	if(spi_lock_dev(df_spi_chip->spi_dev) < 0)
		return UFFS_FLASH_IO_ERR;

	/* Wait for flash to become ready */
	df_wait_ready();

	/* Transmit command */
	if (!spi_dma_transfer(df_spi_chip, tx_buf, rx_buf, 8 + DF_PAGE_DATA_SIZE + DF_PAGE_SPARE_SIZE)) {
		printf("SPI transfer failed!\r\n");
		return UFFS_FLASH_IO_ERR;
	}

	/* Wait for flash read to complete */
	df_wait_ready();
	spi_unlock_dev(df_spi_chip->spi_dev);
	/* Copy data to user */
	memcpy(data, rx_buf + 8, data_len);
	memcpy(spare, rx_buf + 8 + DF_PAGE_DATA_SIZE, spare_len);

	/* Update device stats */
	dev->st.page_read_count++;
	dev->st.spare_read_count++;

	return UFFS_FLASH_NO_ERR;

}

/**
 * Write page to flash page (data and spare area).
 * @note This function should only be called by UFFS and not from user space
 * @param dev UFFS device
 * @param block Block number
 * @param page Page number
 * @param data Pointer to data buffer
 * @param data_len Length of data in data buffer
 * @param spare Pointer to spare buffer
 * @param spare_len Length of spare buffer
 */
static int df_write_page(uffs_Device *dev, u32 block, u32 page, const u8 *data, int data_len, const u8 *spare, int spare_len) {

	uint32_t mem_addr;
	uint8_t tx_buf[4 + DF_PAGE_DATA_SIZE + DF_PAGE_SPARE_SIZE];
	uint8_t rx_buf[4 + DF_PAGE_DATA_SIZE + DF_PAGE_SPARE_SIZE];

	/* Test for page overflow */
	if (data_len > DF_PAGE_DATA_SIZE || spare_len > DF_PAGE_SPARE_SIZE) {
		printf("UFFS: Attempt to write %u data bytes and %u spare bytes to flash, aborting\r\n", data_len, spare_len);
		return UFFS_FLASH_UNKNOWN_ERR;
	}

	if(spi_lock_dev(df_spi_chip->spi_dev) < 0)
		return UFFS_FLASH_IO_ERR;

	/* If the data or spare area is empty, read the current contents */
	if (!data || data_len < 1 || !spare || spare_len < 1) {
		printf("UFFS: Empty data or spare area\r\n");
		df_read_page(dev, block, page, tx_buf + 4, DF_PAGE_DATA_SIZE, NULL, 
					   tx_buf + 4 + DF_PAGE_DATA_SIZE, DF_PAGE_SPARE_SIZE);
	}

	/* Copy data area */
	if (data && data_len > 0)
		memcpy(tx_buf + 4, data, data_len);

	/* Copy spare area */
	if (spare && spare_len > 0)
		memcpy(tx_buf + 4 + DF_PAGE_DATA_SIZE, spare, spare_len);

	/**
	 * To perform the main memory page program through buffer for the standard DataFlash page size (1056 bytes), a
	 * 1-byte opcode, 82H for buffer 1 or 85H for buffer 2, must first be clocked into the device, fol-
	 * lowed by three address bytes. The address bytes are comprised of 13 page address bits,
	 * (PA12-PA0) that select the page in the main memory where data is to be written, and 11 buffer
	 * address bits (BFA10-BFA0) that select the first byte in the buffer to be written.
	 * After all address bytes are clocked in, the part will take data from the input pins and
	 * store it in the specified data buffer. If the end of the buffer is reached, the device will wrap
	 * around back to the beginning of the buffer. When there is a low-to-high transition on the CS pin,
	 * the part will first erase the selected page in main memory to all 1s and then program the data
	 * stored in the buffer into that memory page. Both the erase and the programming of the page are
	 * internally self-timed and should take place in a maximum time of tEP. During this time, the status
	 * register and the RDY/BUSY pin will indicate that the part is busy.
	 */

	/* Command */
	tx_buf[0] = AT45DBX_CMDB_PR_PAGE_TH_BUF1;

	/* Address */
	mem_addr  = (((block * dev->attr->pages_per_block + page) & 0x1FFF) << 11);
	tx_buf[1] = (mem_addr & 0xFF0000) >> 16;
	tx_buf[2] = (mem_addr & 0x00FF00) >> 8;
	tx_buf[3] = (mem_addr & 0x0000FF) >> 0;

	/* Wait for flash to become ready */
	df_wait_ready();

	/* Start transfer of data */
	if (!spi_dma_transfer(df_spi_chip, tx_buf, rx_buf, 4 + DF_PAGE_DATA_SIZE + DF_PAGE_SPARE_SIZE)) {
		printf("SPI transfer failed!\r\n");
		return UFFS_FLASH_IO_ERR;
	}
	
	/* Wait for flash write to complete */
	df_wait_ready();

	spi_unlock_dev(df_spi_chip->spi_dev);

	/* Update device stats */
	if (data)
		dev->st.page_write_count++;
	if (spare)
		dev->st.spare_write_count++;
	
	return UFFS_FLASH_NO_ERR;

}

/**
 * Erase flash block.
 * @note This function should only be called by UFFS and not from user space.
 * @param dev UFFS device
 * @param blockNumber Block number
 */
static int df_erase_block(uffs_Device * dev, u32 blockNumber) {

	uint32_t mem_addr;
	uint8_t tx_buf[4], rx_buf[4];

	/**
	 * To perform a block erase for the standard DataFlash page size (1056 bytes), an opcode of 50H
	 * must be loaded into the device, followed by three address bytes comprised of 10 page address
	 * bits (PA12 -PA3) and 14 don’t care bits. The 10 page address bits are used to specify which
	 * block of eight pages is to be erased.
	 */

	/* Command */
	tx_buf[0] = AT45DBX_CMDB_ER_BLOCK;

	/* Address */
	mem_addr  = ((blockNumber & 0x3FF) << 14);
	tx_buf[1] = (mem_addr & 0xFF0000) >> 16;
	tx_buf[2] = (mem_addr & 0x00FF00) >> 8;
	tx_buf[3] = (mem_addr & 0x0000FF) >> 0;

	if(spi_lock_dev(df_spi_chip->spi_dev) < 0)
		return UFFS_FLASH_IO_ERR;
	/* Wait for flash to become ready */
	df_wait_ready();

	/**
	 * When a low-to-high transition occurs on the CS pin,
	 * the part will erase the selected block of eight pages.
	 */

	/* Transmit command */
	if(!spi_dma_transfer(df_spi_chip, tx_buf, rx_buf, 4)) {
		printf("SPI transfer failed!\r\n");
		return UFFS_FLASH_IO_ERR;
	}

	/* Wait for flash erase to complete */
	df_wait_ready();
	spi_unlock_dev(df_spi_chip->spi_dev);

	/* Update device stats */
	if (dev != NULL)
		dev->st.block_erase_count++;

	return UFFS_FLASH_NO_ERR;

}

/** Flash driver function table */
struct uffs_FlashOpsSt df_driver_ops = {
	.ReadPage			= df_read_page,
	.WritePage			= df_write_page,
	.EraseBlock 		= df_erase_block,
};

/** Flash chip configuration */
struct uffs_StorageAttrSt df_storage = {
	.total_blocks 		= DF_TOTAL_BLOCKS,
	.page_data_size 	= DF_PAGE_DATA_SIZE,
	.pages_per_block 	= DF_PAGES_PER_BLOCK,
	.spare_size 		= DF_PAGE_SPARE_SIZE,
	.block_status_offs	= DF_BLOCK_ST_OFFSET,
	.ecc_opt 			= UFFS_ECC_SOFT,
	.layout_opt 		= UFFS_LAYOUT_UFFS,
};

/**
 * Initialise DataFlash device
 * @note This function should only be called by UFFS and not from user space.
 * @param dev UFFS device
 */
URET df_init_device(uffs_Device * dev) {

	/* Set driver device functions */
	dev->ops = &df_driver_ops;

	if(spi_lock_dev(df_spi_chip->spi_dev) < 0)
		return UFFS_FLASH_IO_ERR;
	spi_write(df_spi_chip, 0x00FF);
	spi_read(df_spi_chip);

	uint8_t tx_buf[5], rx_buf[5];
	uint8_t status;

	tx_buf[0] = AT45DBX_CMDC_RD_STATUS_REG;
	spi_dma_transfer(df_spi_chip, tx_buf, rx_buf, 2);
	status = rx_buf[1];

#if 0
	/* Request / Reply buffers */
	memset(tx_buf, 0xFF, 5);
	tx_buf[0] = AT45DBX_CMDC_RD_MNFCT_DEV_ID_SM;
	spi_dma_transfer(df_spi_chip, tx_buf, rx_buf, 5);

	printf("Status %02X\r\n", status);
	printf("Device ID0 %02X\r\n", rx_buf[1]);
	printf("Device ID1 %02X\r\n", rx_buf[2]);
	printf("Device ID2 %02X\r\n", rx_buf[3]);
	printf("Device ID3 %02X\r\n", rx_buf[4]);
#endif
	spi_unlock_dev(df_spi_chip->spi_dev);
	/* Check to see if flash chip is recognised */
	if ((status & AT45DBX_MSK_DENSITY) == AT45DBX_DENSITY)
		return U_SUCC;
	else
		return U_FAIL;

}

/**
 * Release DataFlash device
 * @note This function should only be called by UFFS and not from user space.
 * @param dev UFFS device
 */
URET df_release_device(uffs_Device * dev) {

	return U_SUCC;

}

/**
 * Initialise DataFlash chip and UFFS
 * @note This function should only be called by UFFS and not from user space.
 * @param chip DataFlash SPI chip. This must be setup with spi_setup_chip() before calling this function.
 */
int df_spi_uffs_init(spi_chip_t * chip) {
	df_spi_chip = chip;
	return 0;
}

/* This command erases the entire DataFlash chip */
int df_uffs_chip_erase(void) {

	int block;

	/* Wait for flash to become ready */
	if(spi_lock_dev(df_spi_chip->spi_dev) < 0)
		return UFFS_FLASH_IO_ERR;
	df_wait_ready();
	
	for (block = 0; block < DF_TOTAL_BLOCKS; block++) {
		printf("Erasing block %u\r\n", block);
		df_erase_block(NULL, block);
	}
	spi_unlock_dev(df_spi_chip->spi_dev);
	return 0;

}
