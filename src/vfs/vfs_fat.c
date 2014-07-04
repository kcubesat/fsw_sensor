/**
 * @file vfs_fat.c
 * VFS backend for FAT
 *
 * @author Jeppe Ledet-Pedersen
 * Copyright 2011 GomSpace ApS. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

#include <vfs/vfs.h>

#include <fat_sd/ff.h>

int vfs_fat_open(struct vfs_fd *fd, const char *path, int oflag, int mode) {

	int result = -1, flags = 0;

	if (!fd->fp)
		fd->fp = malloc(sizeof(FIL));

	if (!fd->fp)
		return -1;

	if (oflag == 0)
		flags |= (FA_READ | FA_WRITE | FA_OPEN_EXISTING);
	if (oflag & O_RDWR)
		flags |= (FA_READ | FA_WRITE | FA_OPEN_EXISTING);
	if (oflag & O_RDONLY)
		flags |= (FA_READ | FA_WRITE | FA_OPEN_EXISTING);
	if (oflag & O_WRONLY)
		flags |= (FA_READ | FA_WRITE | FA_OPEN_EXISTING);
	if (oflag & O_CREAT)
		flags |= FA_OPEN_ALWAYS;
	if (oflag & O_TRUNC)
		flags |= FA_OPEN_ALWAYS;
	if (oflag & (O_CREAT | O_TRUNC))
		flags |= FA_OPEN_ALWAYS;

	result = f_open(fd->fp, path, flags);
	if (result != FR_OK) {
		free(fd->fp);
		fd->fp = NULL;
	}

	return result == FR_OK ? 0 : -1;
}

int vfs_fat_close(struct vfs_fd *fd) {

	int result = -1;

	if (fd->fp) {
		result = f_close(fd->fp);
		free(fd->fp);
		fd->fp = NULL;
	}

	return result == FR_OK ? 0 : -1;
}

int vfs_fat_read(struct vfs_fd *fd, void *buf, int nbyte) {

	int result = -1;
	unsigned int read;

	if (fd->fp)
		result = f_read(fd->fp, buf, nbyte, &read);
	
	return result == FR_OK ? (int) read : -1;
}

int vfs_fat_write(struct vfs_fd *fd, const void *buf, int nbyte) {

	int result = -1;
	unsigned int written;

	if (fd->fp)
		result = f_write(fd->fp, buf, nbyte, &written);
	
	return result == FR_OK ? (int) written : -1;
}

int vfs_fat_lseek(struct vfs_fd *fd, off_t *retoffset, off_t offset, int whence) {

	int result = -1;

	if (whence == SEEK_CUR) {
		offset = f_tell((FIL *) fd->fp) + offset;
	}

	if (whence == SEEK_END) {
		offset = f_size((FIL *) fd->fp) + offset;
	}

	if (fd->fp)
		result = f_lseek(fd->fp, (unsigned long) offset);

	if (result == FR_OK) {
		*retoffset = offset;
		return 0;
	}

	return -1;
}

int vfs_fat_fsync(struct vfs_fd *fd) {
	
	int result = -1;

	if (fd->fp)
		result = f_sync(fd->fp);

	return result == FR_OK ? 0 : -1;
}

int vfs_fat_rename(const char *old, const char *new) {
	int result = f_rename(old, new);
	return result == FR_OK ? 0 : -1;
}

int vfs_fat_unlink(const char *path) {
	int result = f_unlink(path);
	return result == FR_OK ? 0 : -1;
}

int vfs_fat_stat(const char *path, struct stat *buf) {

	FILINFO finf = {0};

#if _USE_LFN
	char lfn[(_MAX_LFN + 1) * 2];
	finf.lfname = lfn;
	finf.lfsize = sizeof(lfn);
#endif

	int result = f_stat(path, &finf);

	memset(buf, 0, sizeof(*buf));
	if (result == FR_OK) {
		buf->st_mode  = (finf.fattrib & AM_DIR) ? S_IFDIR : 0;
		buf->st_size  = finf.fsize;
		buf->st_atime = finf.ftime;
		buf->st_mtime = finf.ftime;
		buf->st_ctime = finf.ftime;
	}
	
	return result == FR_OK ? 0 : -1;
}

int vfs_fat_lstat(const char *path, struct stat *buf) {
	return vfs_fat_stat(path, buf);
}

int vfs_fat_fstat(struct vfs_fd *fd, struct stat *buf) {
	return vfs_fat_stat(fd->path, buf);
}

int vfs_fat_opendir(struct vfs_fd *fd, const char *path) {
	
	int result;

	if (!fd->fp)
		fd->fp = malloc(sizeof(FATDIR));

	if (!fd->fp)
		return -1;

	result = f_opendir(fd->fp, path);
	if (result != FR_OK) {
		free(fd->fp);
		fd->fp = NULL;
	}

	return result == FR_OK ? 0 : -1;
}

int vfs_fat_closedir(struct vfs_fd *fd) {

	/* FAT doesn't have closedir, so we just free the FATDIR structure */
	if (fd->fp) {
		free(fd->fp);
		fd->fp = NULL;
	}

	return 0;
}

int vfs_fat_readdir(struct vfs_fd *fd, struct dirent *dirent) {

	int result;
	FILINFO finf = {0};
	char *fn;

	memset(dirent, 0, sizeof(*dirent));

#if _USE_LFN
	finf.lfname = dirent->d_name;
	finf.lfsize = MAX_FILENAME_LENGTH;
#endif


	result = f_readdir(fd->fp, &finf);
	if (result == FR_OK && finf.fname[0]) {
		dirent->d_type = (finf.fattrib & AM_DIR) ? DT_DIR: 0;
		dirent->d_reclen = finf.fsize;
#if _USE_LFN
		fn = *finf.lfname ? finf.lfname : finf.fname;
#else
		fn = finf.fname;
#endif
		strncpy(dirent->d_name, fn, MAX_FILENAME_LENGTH);
	}

	return result == FR_OK && finf.fname[0] ? 0 : -1;
}

int vfs_fat_rewinddir(struct vfs_fd *fd) {
	/* rewinddir in FAT is just another opendir */
	return vfs_fat_opendir(fd, fd->path);
}

int vfs_fat_mkdir(const char *path, mode_t mode) {
	int result = f_mkdir(path);
	return result == FR_OK ? 0 : -1;
}

int vfs_fat_rmdir(const char *path) {
	int result = f_unlink(path);
	return result == FR_OK ? 0 : -1;
}

struct vfs_ops vfs_fat_ops = {
	.fs = "fat",
	.open = vfs_fat_open,
	.close = vfs_fat_close,
	.read = vfs_fat_read,
	.write = vfs_fat_write,
	.lseek = vfs_fat_lseek,
	.fsync = vfs_fat_fsync,
	.rename = vfs_fat_rename,
	.unlink = vfs_fat_unlink,
	.stat = vfs_fat_stat,
	.lstat = vfs_fat_lstat,
	.fstat = vfs_fat_fstat,
	.closedir = vfs_fat_closedir,
	.opendir = vfs_fat_opendir,
	.readdir = vfs_fat_readdir,
	.rewinddir = vfs_fat_rewinddir,
	.mkdir = vfs_fat_mkdir, 
	.rmdir = vfs_fat_rmdir,
};
