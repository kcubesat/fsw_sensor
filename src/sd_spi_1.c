/** @file sd_spi_gomspace.c
 *
 * @brief SD SPI driver
 * This driver uses the Gomspace driver library SPI functions to implement
 * a SD card driver.
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

#define DEBUG_SD_SPI	0
#define INFO_SD_SPI		1
#define USE_DMA			0

/* Definitions for MMC/SDC command */
#define CMD0	(0x40+0)	/* GO_IDLE_STATE */
#define CMD1	(0x40+1)	/* SEND_OP_COND (MMC) */
#define	ACMD41	(0xC0+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(0x40+8)	/* SEND_IF_COND */
#define CMD9	(0x40+9)	/* SEND_CSD */
#define CMD10	(0x40+10)	/* SEND_CID */
#define CMD12	(0x40+12)	/* STOP_TRANSMISSION */
#define ACMD13	(0xC0+13)	/* SD_STATUS (SDC) */
#define CMD16	(0x40+16)	/* SET_BLOCKLEN */
#define CMD17	(0x40+17)	/* READ_SINGLE_BLOCK */
#define CMD18	(0x40+18)	/* READ_MULTIPLE_BLOCK */
#define CMD23	(0x40+23)	/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0xC0+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(0x40+24)	/* WRITE_BLOCK */
#define CMD25	(0x40+25)	/* WRITE_MULTIPLE_BLOCK */
#define CMD55	(0x40+55)	/* APP_CMD */
#define CMD58	(0x40+58)	/* READ_OCR */

/* Card type flags (CardType) */
#define CT_MMC				0x01
#define CT_SD1				0x02
#define CT_SD2				0x04
#define CT_SDC				(CT_SD1|CT_SD2)
#define CT_BLOCK			0x08

static volatile DSTATUS Stat = STA_NOINIT; 	/* Disk status */
static BYTE CardType; 						/* b0:MMC, b1:SDv1, b2:SDv2, b3:Block addressing */
static spi_chip_t * sd_chip;				/* Pointer to SPI chip structure */
static unsigned int baudrate;				/* The baudrate to run at full speed */
#if USE_DMA
/* Note: the buffer size must be a multiple of CONFIG_SYS_DCACHE_LINESZ and
 * aligned to a cache line */
static uint8_t writebuff[512] __attribute__((aligned(CONFIG_SYS_DCACHE_LINESZ)));
static uint8_t readbuff[512] __attribute__((aligned(CONFIG_SYS_DCACHE_LINESZ)));
static uint8_t ffbuff[512] __attribute__((aligned(CONFIG_SYS_DCACHE_LINESZ)));
#endif

inline static BYTE check_timeout_ms(portTickType start_time, unsigned int timeout_ms) {
	if (((xTaskGetTickCount() - start_time) * (1000/configTICK_RATE_HZ)) < timeout_ms)
		return 1;
	else {
		printf("SD_SPI Timeout\r\n");
		Stat = STA_NOINIT;
		return 0;
	}
}

/**
 * Low level receive / tranmit functions for SPI (used by driver below)
 */

static inline void xmit_spi(BYTE dat) {
	spi_write(sd_chip, dat);
	spi_read(sd_chip);
	//printf("Write o: %x\r\n", dat);
}

static inline BYTE rcvr_spi(void) {
	spi_write(sd_chip, 0xFF);
	BYTE c = spi_read(sd_chip);
	//printf("Read %x\r\n", c);
	return c;
}

static inline void rcvr_spi_m(BYTE *dst) {
	*dst = rcvr_spi();
}

static BYTE wait_ready(void) {
	BYTE res;

	portTickType start_time = xTaskGetTickCount();

	do
		res = rcvr_spi();
	while ((res != 0xFF) && check_timeout_ms(start_time, 3000));

	return res;
}

/**
 * Helper function to receive an arbitrary size of data
 * @param buff Data buffer to store received data
 * @param btr Byte count (must be multiple of 4)
 * @return True/False
 */

static int rcvr_datablock(BYTE *buff, UINT btr) {

	if (btr > 512) {
		printf("Too large datablock\r\n");
		return 0;
	}

	BYTE token;

	portTickType start_time = xTaskGetTickCount();
	do { /* Wait for data packet in timeout of 100ms */
		token = rcvr_spi();
	} while ((token == 0xFF) && check_timeout_ms(start_time, 1000));
	if (token != 0xFE)
		return 0; /* If not valid data token, return with error */

#if USE_DMA
	if (spi_dma_transfer(sd_chip, ffbuff, readbuff, btr) == 0)
		return 0;
	//dcache_invalidate_range(readbuff, btr);
	memcpy(buff, readbuff, btr);
#else
	do { /* Receive the data block into buffer */
		rcvr_spi_m(buff++);
		rcvr_spi_m(buff++);
		rcvr_spi_m(buff++);
		rcvr_spi_m(buff++);
	} while (btr -= 4);
#endif /* USE_DMA */
	rcvr_spi(); /* Discard CRC */
	rcvr_spi();

	return 1; /* Return with success */
}

#if _READONLY == 0

/**
 * Helper function to transmit a single datablock
 * @param buff 512 byte data block to be transmitted
 * @param token Data/Stop token
 * @return True/False
 */

static int xmit_datablock(const BYTE *buff, BYTE token) {
	BYTE resp, wc __attribute__ ((unused));

	if (wait_ready() != 0xFF)
		return 0;

	//hex_dump(buff, 512);

	xmit_spi(token); /* Xmit data token */
	if (token != 0xFD) { /* Is data token */
		wc = 0;
#if USE_DMA
		memcpy(writebuff, buff, 512);
		//dcache_flush_range(writebuff, 512);
		if (spi_dma_transfer(sd_chip, writebuff, readbuff, 512) == 0)
			return 0;
#else
		do { /* Xmit the 512 byte data block to MMC */
			xmit_spi(*buff++);
			xmit_spi(*buff++);
		} while (--wc);
#endif /* USE_DMA */
		xmit_spi(0xFF); /* CRC (Dummy) */
		xmit_spi(0xFF); /* CRC (Dummy) */

		resp = rcvr_spi(); /* Receive data response */
		if ((resp & 0x1F) != 0x05) { /* If not accepted, return with error */
			return 0;
		}

	}

	return 1;
}
#endif /* _READONLY */

/**
 * Helper function to send a SD-spec command
 * @param cmd Command byte
 * @param arg Argument
 * @return Response
 */
static BYTE send_cmd(BYTE cmd, DWORD arg) {
	BYTE n, res;

	if (cmd & 0x80) { /* ACMD<n> is the command sequence of CMD55-CMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1)
			return res;
	}

	/* Select the card and wait for ready */
	if (cmd != CMD0)
		if (wait_ready() != 0xFF)
			return 0xFF;

	/* Send command packet */
	xmit_spi(cmd); /* Start + Command index */
	xmit_spi((BYTE) (arg >> 24)); /* Argument[31..24] */
	xmit_spi((BYTE) (arg >> 16)); /* Argument[23..16] */
	xmit_spi((BYTE) (arg >> 8)); /* Argument[15..8] */
	xmit_spi((BYTE) arg); /* Argument[7..0] */
	n = 0x01; /* Dummy CRC + Stop */
	if (cmd == CMD0)
		n = 0x95; /* Valid CRC for CMD0(0) */
	if (cmd == CMD8)
		n = 0x87; /* Valid CRC for CMD8(0x1AA) */
	xmit_spi(n);

	/* Receive command response */
	if (cmd == CMD12)
		rcvr_spi(); /* Skip a stuff byte when stop reading */
	n = 10; /* Wait for a valid response in timeout of 10 attempts */
	do
		res = rcvr_spi();
	while ((res & 0x80) && --n);

#if DEBUG_SD_SPI
	printf("CMD %hhx Response %u\r\n", cmd, res);
#endif
	return res; /* Return with the response value */
}

/**
 * 						PUBLIC FUNCTIONS
 *  These functions are the layer between the filesystem and the driver.
 */


/**
 * The disk_initialize function initializes the disk drive.
 * @return This function returns a disk status as the result. For details of the disk status, refer to the disk_status function.
 */
DSTATUS sd_disk_initialize(void) {
	BYTE n, cmd, ty, ocr[4];
	portTickType start_time;

	if (Stat & STA_NODISK) {
		log_error("SD_NOCARD", "");
		return Stat; /* No card in the socket */
	}

	if (Stat == 0)
		return Stat;

	if (spi_lock_dev(sd_chip->spi_dev) < 0)
		return Stat;

	/* Slow down during initialisation */
	sd_chip->baudrate = 200000;
	spi_setup_chip(sd_chip);

	/* Dummy clocks */
	static spi_chip_t spi_no_chip;
	spi_no_chip.spi_dev = sd_chip->spi_dev;
	spi_no_chip.baudrate = 200000;
	spi_no_chip.spi_mode = 0;
	spi_no_chip.bits = 8;
	spi_no_chip.cs = 0;
	spi_no_chip.reg = 0;
	spi_no_chip.spck_delay = 0;
	spi_no_chip.trans_delay = 0;
	spi_setup_chip(&spi_no_chip);

	for (n = 100; n; n--) {
		vTaskDelay(1);
		spi_write(&spi_no_chip, 0xFF);
		spi_read(&spi_no_chip);
	}

	ty = 0;
	if (send_cmd(CMD0, 0) == 1) { /* Enter Idle state */
		start_time = xTaskGetTickCount();
		if (send_cmd(CMD8, 0x1AA) == 1) { /* SDHC */
			for (n = 0; n < 4; n++) {
				ocr[n] = rcvr_spi(); /* Get trailing return value of R7 resp */
			}

			if (ocr[2] == 0x01 && ocr[3] == 0xAA) { /* The card can work at vdd range of 2.7-3.6V */
				while (check_timeout_ms(start_time, 1000) && send_cmd(ACMD41, 1UL << 30))
					; /* Wait for leaving idle state (ACMD41 with HCS bit) */
				if (check_timeout_ms(start_time, 1000) && send_cmd(CMD58, 0) == 0) { /* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++)
						ocr[n] = rcvr_spi();
					ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;
				}
			}
		} else { /* SDSC or MMC */
			if (send_cmd(ACMD41, 0) <= 1) {
				ty = CT_SD1;
				cmd = ACMD41; /* SDSC */
			} else {
				ty = CT_MMC;
				cmd = CMD1; /* MMC */
			}
			while (check_timeout_ms(start_time, 1000) && send_cmd(cmd, 0))
				; /* Wait for leaving idle state */
			if (!check_timeout_ms(start_time, 1000) || send_cmd(CMD16, 512) != 0) /* Set R/W block length to 512 */
				ty = 0;
		}
	}

	CardType = ty;

	/* Speed up after... */
	sd_chip->baudrate = baudrate;
	spi_setup_chip(sd_chip);

	if (ty) { /* Initialization succeeded */
		Stat &= ~STA_NOINIT; /* Clear STA_NOINIT */
		log_info("SD_OK", "Card Type %u", CardType);
	} else {
		log_error("SD_FAIL", "");
	}

	spi_unlock_dev(sd_chip->spi_dev);

	return Stat;
}



/**
 * The disk_status function returns the current disk status.
 * @return The disk status is returned in combination of following flags.
 * STA_NOINIT: Indicates that the disk drive has not been initialized. This flag is set on: system reset, disk removal and disk_initialize function failed, and cleared on: disk_initialize function succeeded.
 * STA_NODISK: Indicates that no medium in the drive. This is always cleared on fixed disk drive.
 * STA_PROTECTED: Indicates that the medium is write protected. This is always cleared on the drive that does not support write protect notch. Not valid when STA_NODISK is set.
 */
DSTATUS sd_disk_status(void) {
	return Stat;
}

/**
 * The disk_read function reads sector(s) from the disk drive.
 * @param buff Pointer to the byte array to store the read data. The buffer size of number of bytes to be read, sector size * sector count, is required. The memory address specified by upper layer may or may not be aligned to word boundary.
 * @param sector Specifies the start sector number in logical block address (LBA).
 * @param count Specifies number of sectors to read. The value can be 1 to 255.
 * @return
 * RES_OK (0): The function succeeded.
 * RES_ERROR: Any hard error occured during the read operation and could not recover it.
 * RES_PARERR: Invalid parameter.
 * RES_NOTRDY: The disk drive has not been initialized.
 */
DRESULT sd_disk_read(BYTE *buff, DWORD sector, BYTE count) {

#if DEBUG_SD_SPI
	printf("SD-SPI disk read sector %lu count %u\r\n", sector, count);
#endif

	if (!count)
		return RES_PARERR;
	if (Stat & STA_NOINIT)
		return RES_NOTRDY;
	if (spi_lock_dev(sd_chip->spi_dev) < 0)
		return RES_ERROR;

	if (!(CardType & CT_BLOCK))
		sector *= 512; /* Convert to byte address if needed */

	if (count == 1) { /* Single block read */
		if ((send_cmd(CMD17, sector) == 0) /* READ_SINGLE_BLOCK */
		&& rcvr_datablock(buff, 512)) {
			count = 0;
		}
	} else { /* Multiple block read */
		if (send_cmd(CMD18, sector) == 0) { /* READ_MULTIPLE_BLOCK */
			do {
				if (!rcvr_datablock(buff, 512))
					break;
				buff += 512;
			} while (--count);
			send_cmd(CMD12, 0); /* STOP_TRANSMISSION */
		}
	}

	spi_unlock_dev(sd_chip->spi_dev);

	return count ? RES_ERROR : RES_OK;
}

#if _READONLY == 0
/**
 * The disk_write writes sector(s) to the disk.
 * @param buff Pointer to the byte array to be written. The memory address specified by upper layer may or may not be aligned to word boundary.
 * @param sector Specifies the start sector number in logical block address (LBA).
 * @param count Specifies the number of sectors to write. The value can be 1 to 255.
 * @return
 * RES_OK (0): The function succeeded.
 * RES_ERROR: Any hard error occured during the write operation and could not recover it.
 * RES_WRPRT: The medium is write protected.
 * RES_PARERR: Invalid parameter.
 * RES_NOTRDY: The disk drive has not been initialized.
 */
DRESULT sd_disk_write(const BYTE *buff, DWORD sector, BYTE count) {

#if DEBUG_SD_SPI
	printf("SD-SPI disk write sector %lu count %u\r\n", sector, count);
#endif

	if (!count)
		return RES_PARERR;
	if (Stat & STA_NOINIT)
		return RES_NOTRDY;
	if (Stat & STA_PROTECT)
		return RES_WRPRT;
	if (spi_lock_dev(sd_chip->spi_dev) < 0)
		return RES_ERROR;

	if (!(CardType & CT_BLOCK))
		sector *= 512; /* Convert to byte address if needed */

	if (count == 1) { /* Single block write */
		if ((send_cmd(CMD24, sector) == 0) /* WRITE_BLOCK */
		&& xmit_datablock(buff, 0xFE))
			count = 0;
	} else { /* Multiple block write */
		if (CardType & CT_SDC)
			send_cmd(ACMD23, count);
		if (send_cmd(CMD25, sector) == 0) { /* WRITE_MULTIPLE_BLOCK */
			do {
				if (!xmit_datablock(buff, 0xFC))
					break;
				buff += 512;
			} while (--count);
			if (!xmit_datablock(0, 0xFD)) /* STOP_TRAN token */
				count = 1;
		}
	}

	spi_unlock_dev(sd_chip->spi_dev);

	return count ? RES_ERROR : RES_OK;
}
#endif /* _READONLY == 0 */

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL != 0
/**
 * The disk_ioctl function cntrols device specified features and miscellaneous functions other than disk read/write
 * @param ctrl Specifies the command code.
 * @param buff Pointer to the parameter buffer depends on the command code. When it is not used, specify a NULL pointer.
 * @return
 * RES_OK (0): The function succeeded.
 * RES_ERROR: Any error occured.
 * RES_PARERR: Invalid command code.
 * RES_NOTRDY: The disk drive has not been initialized.
 */
DRESULT sd_disk_ioctl(BYTE ctrl, void *buff) {

	DRESULT res;
	BYTE n, csd[16], *ptr = buff;
	WORD csize;

#if DEBUG_SD_SPI
	printf("SD-SPI disk ioctl %u\r\n", ctrl);
#endif

	if (spi_lock_dev(sd_chip->spi_dev) < 0)
		return RES_ERROR;

	res = RES_ERROR;

	if (ctrl == CTRL_POWER) {
		switch (*ptr) {
		case 0: /* Sub control code == 0 (POWER_OFF) */
			res = RES_OK;
			break;
		case 1: /* Sub control code == 1 (POWER_ON) */
			res = RES_OK;
			break;
		case 2: /* Sub control code == 2 (POWER_GET) */
			*(ptr + 1) = 1;
			res = RES_OK;
			break;
		default:
			res = RES_PARERR;
		}
	} else {
		if (Stat & STA_NOINIT)
			return RES_NOTRDY;

		switch (ctrl) {
		case CTRL_SYNC: /* Make sure that no pending write process */

			if (wait_ready() == 0xFF)
				res = RES_OK;
			break;

		case GET_SECTOR_COUNT: /* Get number of sectors on the disk (DWORD) */
			if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
				if ((csd[0] >> 6) == 1) { /* SDC ver 2.00 */
					csize = csd[9] + ((WORD) csd[8] << 8) + 1;
					*(DWORD*) buff = (DWORD) csize << 10;
				} else { /* SDC ver 1.XX or MMC*/
					n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3)
							<< 1) + 2;
					csize = (csd[8] >> 6) + ((WORD) csd[7] << 2)
							+ ((WORD) (csd[6] & 3) << 10) + 1;
					*(DWORD*) buff = (DWORD) csize << (n - 9);
				}
				res = RES_OK;
			}
			break;

		case GET_SECTOR_SIZE: /* Get R/W sector size (WORD) */
			*(WORD*) buff = 512;
			res = RES_OK;
			break;

		case GET_BLOCK_SIZE: /* Get erase block size in unit of sector (DWORD) */
			if (CardType & CT_SD2) { /* SDC ver 2.00 */
				if (send_cmd(ACMD13, 0) == 0) { /* Read SD status */
					rcvr_spi();
					if (rcvr_datablock(csd, 16)) { /* Read partial block */
						for (n = 64 - 16; n; n--)
							rcvr_spi(); /* Purge trailing data */
						*(DWORD*) buff = 16UL << (csd[10] >> 4);
						res = RES_OK;
					}
				}
			} else { /* SDC ver 1.XX or MMC */
				if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) { /* Read CSD */
					if (CardType & CT_SD1) { /* SDC ver 1.XX */
						*(DWORD*) buff = (((csd[10] & 63) << 1)
								+ ((WORD) (csd[11] & 128) >> 7) + 1)
								<< ((csd[13] >> 6) - 1);
					} else { /* MMC */
						*(DWORD*) buff = ((WORD) ((csd[10] & 124) >> 2) + 1)
								* (((csd[11] & 3) << 3)
										+ ((csd[11] & 224) >> 5) + 1);
					}
					res = RES_OK;
				}
			}
			break;

		case MMC_GET_TYPE: /* Get card type flags (1 byte) */
			*ptr = CardType;
			res = RES_OK;
			break;

		case MMC_GET_CSD: /* Receive CSD as a data block (16 bytes) */
			if (send_cmd(CMD9, 0) == 0 /* READ_CSD */
			&& rcvr_datablock(ptr, 16))
				res = RES_OK;
			break;

		case MMC_GET_CID: /* Receive CID as a data block (16 bytes) */
			if (send_cmd(CMD10, 0) == 0 /* READ_CID */
			&& rcvr_datablock(ptr, 16))
				res = RES_OK;
			break;

		case MMC_GET_OCR: /* Receive OCR as an R3 resp (4 bytes) */
			if (send_cmd(CMD58, 0) == 0) { /* READ_OCR */
				for (n = 4; n; n--)
					*ptr++ = rcvr_spi();
				res = RES_OK;
			}
			break;

		case MMC_GET_SDSTAT: /* Receive SD status as a data block (64 bytes) */
			if (send_cmd(ACMD13, 0) == 0) { /* SD_STATUS */
				rcvr_spi();
				if (rcvr_datablock(ptr, 64))
					res = RES_OK;
			}
			break;

		default:
			res = RES_PARERR;
		}

	}

	spi_unlock_dev(sd_chip->spi_dev);

	return res;
}
#endif /* _USE_IOCTL != 0 */

char sd_spi_init(spi_chip_t * chip) {
	sd_chip = chip;
	baudrate = sd_chip->baudrate;

#if USE_DMA
	memset(ffbuff, 0xFF, 512);
#endif

	return sd_disk_initialize();

}
