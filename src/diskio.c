/** @file diskio.c
 *
 * @brief Interface to the Disk I/O layer of the FatFS
 * This file is an interface layer calling different driver-routines depending on the
 * "physical-drive" number.
 *
 */

#include "diskio.h"

#define SD			0
#define DF			1

/**
 * The disk_initialize function initializes the disk drive.
 * @param drv Specifies the physical drive number to initialize.
 * @return This function returns a disk status as the result. For details of the disk status, refer to the disk_status function.
 */
DSTATUS disk_initialize(BYTE drv) {
	switch (drv) {
	case SD:
		return sd_disk_initialize();
	}
	return STA_NOINIT;
}

/**
 * The disk_status function returns the current disk status.
 * @param drv Specifies the physical drive number to be confirmed.
 * @return The disk status is returned in combination of following flags.
 * STA_NOINIT: Indicates that the disk drive has not been initialized. This flag is set on: system reset, disk removal and disk_initialize function failed, and cleared on: disk_initialize function succeeded.
 * STA_NODISK: Indicates that no medium in the drive. This is always cleared on fixed disk drive.
 * STA_PROTECTED: Indicates that the medium is write protected. This is always cleared on the drive that does not support write protect notch. Not valid when STA_NODISK is set.
 */
DSTATUS disk_status(BYTE drv) {
	switch (drv) {
	case SD:
		return sd_disk_status();
	}
	return STA_NOINIT;
}

/**
 * The disk_read function reads sector(s) from the disk drive.
 * @param drv Specifies the physical drive number.
 * @param buff Pointer to the byte array to store the read data. The buffer size of number of bytes to be read, sector size * sector count, is required. The memory address specified by upper layer may or may not be aligned to word boundary.
 * @param sector Specifies the start sector number in logical block address (LBA).
 * @param count Specifies number of sectors to read. The value can be 1 to 255.
 * @return
 * RES_OK (0): The function succeeded.
 * RES_ERROR: Any hard error occured during the read operation and could not recover it.
 * RES_PARERR: Invalid parameter.
 * RES_NOTRDY: The disk drive has not been initialized.
 */
DRESULT disk_read(BYTE drv, BYTE *buff, DWORD sector, BYTE count) {
	switch (drv) {
	case SD:
		return sd_disk_read(buff, sector, count);
	}
	return RES_PARERR;
}

#if _READONLY == 0
/**
 * The disk_write writes sector(s) to the disk.
 * @param drv Specifies the physical drive number.
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
DRESULT disk_write(BYTE drv, const BYTE *buff, DWORD sector, BYTE count) {

	switch (drv) {
	case SD:
		return sd_disk_write(buff, sector, count);
	}
	return RES_PARERR;
}
#endif /* _READONLY */

#if _USE_IOCTL != 0
/**
 * The disk_ioctl function cntrols device specified features and miscellaneous functions other than disk read/write
 * @param drv Specifies the drive number (0-9).
 * @param ctrl Specifies the command code.
 * @param buff Pointer to the parameter buffer depends on the command code. When it is not used, specify a NULL pointer.
 * @return
 * RES_OK (0): The function succeeded.
 * RES_ERROR: Any error occured.
 * RES_PARERR: Invalid command code.
 * RES_NOTRDY: The disk drive has not been initialized.
 */
DRESULT disk_ioctl(BYTE drv, BYTE ctrl, void *buff) {
	switch (drv) {
	case SD:
		return sd_disk_ioctl(ctrl, buff);
	}
	return RES_PARERR;
}
#endif /* _USE_IOCTL != 0 */

