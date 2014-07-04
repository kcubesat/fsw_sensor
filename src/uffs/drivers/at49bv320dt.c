/**
 * AT49BV320DT driver for UFFS on NanoMind
 *
 * @author Jeppe Ledet-Pedersen
 * Copyright 2011 GomSpace ApS. All rights reserved.
 */

/**
 * FLASH LOGIC LAYOUT
 *  The AT49BV320DT has 63 sectors of 64K (SA0-SA62) and 8 sectors of 8K (SA63-SA70).
 *  We emulate NAND flash layout by dividing each sector into 62 pages of 1024 byte data
 *  and 32 byte spare. This gives a total use of 65472 bytes per sector, which means that
 *  the last 64 bytes of each sector are wasted.
 *
 *  The flash is divided into these segments:
 *   - SW Upload:   SA00-SA30 (31x64K sectors)
 *   - File System: SA31-SA62 (32x64K sectors)
 *   - Boot table:  SA63-SA70 (8x8K sectors)
 */
  
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <dev/arm/flash.h>

#include <util/error.h>
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

/** Include driver and define base addr */
#include <uffs/at49bv320dt.h>
#define UFFS_FLASH_BASE	0x48000000

/**
 * Calculate flash address from page and block number
 * @param block Block number
 * @param page Page number
 * @param ofs Offset in page
 */
char * flash_addr_from_page(int block, int page, int ofs) {

	return (char *)((unsigned int) flash_addr(block, UFFS_FLASH_BASE) + (FLASH_PAGE_SIZE * page) + ofs);

}

/**
 * Generic helper function for reading flash pages.
 * @note This function should only be called by UFFS and not from user space
 * @param dev UFFS device
 * @param block Block number
 * @param pageNum Page number
 * @param buf Pointer to data buffer
 * @param ofs Data offset
 * @param len Length of data to read
 */
static int flash_read_page(uffs_Device *dev, u32 block, u32 page, u8 *data, int data_len, u8 *ecc, u8 *spare, int spare_len) {

	char * a = flash_addr_from_page(block, page, 0);

	/* Test for page overflow */
	if (data_len > FLASH_PAGE_DATA_SIZE || spare_len > FLASH_PAGE_SPARE_SIZE) {
		printf("UFFS: Attempt to read %u data bytes and %u spare bytes, aborting\r\n", data_len, spare_len);
		return UFFS_FLASH_UNKNOWN_ERR;
	}

	/* Read data area */
	if (data) {
		flash_read(a, (char *)data, data_len);
		dev->st.page_read_count++;
	}

	/* Read spare area */
	if (spare) {
		flash_read(a + FLASH_PAGE_DATA_SIZE, (char *)spare, spare_len);
		dev->st.spare_read_count++;
	}

	return UFFS_FLASH_NO_ERR;

}

/**
 * Write full page to flash page (data and spare area).
 * @note This function should only be called by UFFS and not from user space
 * @param dev UFFS device
 * @param block Block number
 * @param pageNum Page number
 * @param page Pointer to data buffer
 * @param len Length of data in data buffer
 * @param tag Pointer to tag buffer
 * @param tag_len Length of data in tag buffer
 * @param ecc Calculate ECC in hardware
 */
static int flash_write_page(uffs_Device *dev, u32 block, u32 page, const u8 *data, int data_len, const u8 *spare, int spare_len) {
	
	uint8_t buf[FLASH_PAGE_DATA_SIZE + FLASH_PAGE_SPARE_SIZE];

	/* Test for page overflow */
	if (data_len > FLASH_PAGE_DATA_SIZE || spare_len > FLASH_PAGE_SPARE_SIZE) {
		printf("UFFS: Attempt to write %u data bytes and %u spare bytes to flash, aborting\r\n", data_len, spare_len);
		return UFFS_FLASH_UNKNOWN_ERR;
	}

	/* If the data or spare area is empty, read the current contents */
	if (!data || data_len < 1 || !spare || spare_len < 1) {
		printf("UFFS: Empty data or spare area\r\n");
		flash_read_page(dev, block, page, buf, FLASH_PAGE_DATA_SIZE, NULL,
					   buf + FLASH_PAGE_DATA_SIZE, FLASH_PAGE_SPARE_SIZE);
	}

	/* Copy data area */
	if (data && data_len > 0) 
		memcpy(buf, data, data_len);

	/* Spare area */
	if (spare && spare_len > 0) 
		memcpy(buf + FLASH_PAGE_DATA_SIZE, spare, spare_len);

	/* Calculate page address */
	char * addr = flash_addr_from_page(block, page, 0);

	/* Program data */
	if (flash_program(addr, (char *)buf, FLASH_PAGE_DATA_SIZE + FLASH_PAGE_SPARE_SIZE, 0) != E_NO_ERR) {
		printf("UFFS: Failed to program to flash\r\n");
		return UFFS_FLASH_IO_ERR;
	}

	return UFFS_FLASH_NO_ERR;

}

/**
 * Erase flash block.
 * @note This function should only be called by UFFS and not from user space.
 * @param dev UFFS device
 * @param blockNumber Block number
 */
static int flash_erase_block_ex(uffs_Device * dev, u32 blockNumber) {

	if (flash_erase_block(blockNumber, UFFS_FLASH_BASE) == E_NO_ERR)
		return UFFS_FLASH_NO_ERR;
	else
		return UFFS_FLASH_IO_ERR;

}

/** Flash driver function table */
struct uffs_FlashOpsSt flash_driver_ops = {
	.ReadPage 			= flash_read_page,
	.WritePage			= flash_write_page,
	.EraseBlock 		= flash_erase_block_ex,
};

/** Flash chip configuration */
struct uffs_StorageAttrSt flash_storage = {
	.total_blocks 		= FLASH_TOTAL_BLOCKS,
	.page_data_size 	= FLASH_PAGE_DATA_SIZE,
	.pages_per_block 	= FLASH_PAGES_PER_BLOCK,
	.spare_size 		= FLASH_PAGE_SPARE_SIZE,
	.block_status_offs	= FLASH_BLOCK_ST_OFFSET,
	.ecc_opt 			= UFFS_ECC_SOFT,
	.layout_opt 		= UFFS_LAYOUT_UFFS,
};

/**
 * Initialise DataFlash device
 * @note This function should only be called by UFFS and not from user space.
 * @param dev UFFS device
 */
URET flash_init_device(uffs_Device * dev) {

	/* Set driver device functions */
	dev->ops = &flash_driver_ops;

	if (flash_init() == E_NO_ERR)
		return U_SUCC;
	else
		return U_FAIL;

}

/**
 * Release DataFlash device
 * @note This function should only be called by UFFS and not from user space.
 * @param dev UFFS device
 */
URET flash_release_device(uffs_Device * dev) {

	return U_SUCC;

}

int flash_uffs_chip_erase(void) {

	/* This command erases the entire chip */
	if (flash_erase_chip(UFFS_FLASH_BASE) != E_NO_ERR) {
		printf("Failed to erase flash chip\r\n");
		return -1;
	}

	return 0;

}
