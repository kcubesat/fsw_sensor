/**
 * @file vfs.c
 * Virtual File System dispatcher
 *
 * The VFS acts as a wrapper between the Posix API and
 * the various file system APIs.
 *
 * @author Jeppe Ledet-Pedersen
 * Copyright 2011 GomSpace ApS. All rights reserved.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <vfs/vfs.h>

#define container_of(ptr, type, member) ({ 						\
	const typeof( ((type *)0)->member ) *__mptr = (ptr); 		\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define vfs_fd_has_fop(_fildes, _fop) 							\
	((_fildes) >= VFS_FD_OFFSET && 								\
	 (_fildes) < VFS_FD_OFFSET + VFS_MAX_OPEN_FILES && 			\
	 fd_table[(_fildes) - VFS_FD_OFFSET].state == VFS_FD_USED && \
	 fd_table[(_fildes) - VFS_FD_OFFSET].partition && 			\
	 fd_table[(_fildes) - VFS_FD_OFFSET].partition->fops && 	\
	 fd_table[(_fildes) - VFS_FD_OFFSET].partition->fops->_fop)

#define vfs_part_has_fop(_part, _fop) (_part && _part->fops && _part->fops->_fop)

#define vfs_part_call(_part, _fop, ...) _part->fops->_fop(__VA_ARGS__)

#define vfs_fd_call(_fildes, _fop, ...)							\
	fd_table[(_fildes) - VFS_FD_OFFSET].partition->fops->_fop(&fd_table[_fildes - VFS_FD_OFFSET], ## __VA_ARGS__)

#define vfs_dirp_to_fd(_dirp) ({(((unsigned)container_of(_dirp, struct vfs_fd, dirp) - (unsigned)fd_table) / sizeof(struct vfs_fd)) + VFS_FD_OFFSET;})

static struct vfs_fd fd_table[VFS_MAX_OPEN_FILES] = {[0 ... VFS_MAX_OPEN_FILES - 1] = {.state = VFS_FD_FREE}};
static struct vfs_partition *partitions = NULL;
static unsigned int num_partitions = 0;

static struct vfs_partition *vfs_find_partition(char *name) {

	unsigned int i;
	struct vfs_partition *p;

	for (i = 0; i < num_partitions; i++) {
		p = &partitions[i];
		if (strcmp(name, p->name) == 0)
			return p;
	}

	return NULL;
}

static int vfs_extract_partition(const char *path, char *out, unsigned int outlen) {

	if (*path++ != '/')
		return -1;
	
	while (*path && *path != '/' && outlen-- > 1) 
		*out++ = *path++;
	*out = '\0';

	return 0;
}

static int subst_partition(const char *path, const char *part, char *out, unsigned int outlen) {

	if (*path++ != '/')
		return -1;

	while (*path && *path != '/' && outlen-- > 1) 
		path++;

	while (*part)
		*out++ = *part++;

	while (*path)
		*out++ = *path++;

	*out = '\0';

	return 0;
}

static int cmpxchg(int *var, int old, int new) {

	int org;

	portENTER_CRITICAL();
	org = *var;
	if (org == old) 
		*var = new;
	portEXIT_CRITICAL();

	return org;
}

static int vfs_fd_get(void) {

	int i, ret = -1;

	for (i = 0; i < VFS_MAX_OPEN_FILES; i++) {
		if (cmpxchg((int *) &fd_table[i].state, VFS_FD_FREE, VFS_FD_USED) == VFS_FD_FREE) {
			ret = i + VFS_FD_OFFSET;
			break;
		}
	}

	return ret;
}

static int vfs_fd_put(int fd) {
	return cmpxchg((int *) &fd_table[fd - VFS_FD_OFFSET].state, VFS_FD_USED, VFS_FD_FREE) != VFS_FD_USED;
}

int vfs_init(struct vfs_partition *arg_partitions, unsigned int arg_num_partitions) {

	partitions = arg_partitions;
	num_partitions = arg_num_partitions;
	return 0;

}

int vfs_open(const char *path, int oflag, int mode) {

	char part[VFS_MAX_PARTITION_NAME];
	struct vfs_partition *p;
	int fd_num, ret;
	
	if (vfs_extract_partition(path, part, VFS_MAX_PARTITION_NAME) < 0)
		return -1;

	p = vfs_find_partition(part);
	if (!vfs_part_has_fop(p, open))
		return -1;

	if (p->true_name) {
		subst_partition(path, p->true_name, part, VFS_MAX_PARTITION_NAME);
		path = part;
	}

	fd_num = vfs_fd_get();
	if (fd_num < 0)
		return -1;

	fd_table[fd_num - VFS_FD_OFFSET].partition = p;
	fd_table[fd_num - VFS_FD_OFFSET].path = strndup(path, VFS_MAX_PARTITION_NAME);

	ret = vfs_part_call(p, open, &fd_table[fd_num - VFS_FD_OFFSET], path, oflag, mode);

	if (ret < 0) {
		if (fd_table[fd_num - VFS_FD_OFFSET].path)
			free(fd_table[fd_num - VFS_FD_OFFSET].path);
		vfs_fd_put(fd_num);
		return -1;
	}

	return fd_num;
}

int vfs_close(int fildes) {

	int ret;

	if (!vfs_fd_has_fop(fildes, close))
		return -1;

	ret = vfs_fd_call(fildes, close);
	if (fd_table[fildes - VFS_FD_OFFSET].path)
		free(fd_table[fildes - VFS_FD_OFFSET].path);

	vfs_fd_put(fildes);

	return ret;
}

int vfs_read(int fildes, void *buf, int nbyte) {

	if (!vfs_fd_has_fop(fildes, read))
		return -1;

	return vfs_fd_call(fildes, read, buf, nbyte);
}

int vfs_write(int fildes, const void *buf, int nbyte) {

	if (!vfs_fd_has_fop(fildes, write))
		return -1;

	return vfs_fd_call(fildes, write, buf, nbyte);
}

off_t vfs_lseek(int fildes, off_t offset, int whence) {
	
	off_t retoffset = -1;

	if (!vfs_fd_has_fop(fildes, lseek))
		return -1;

	if (vfs_fd_call(fildes, lseek, &retoffset, offset, whence) < 0)
		return -1;

	return retoffset;
}

int vfs_fsync(int fildes) {

	if (!vfs_fd_has_fop(fildes, fsync))
		return -1;

	return vfs_fd_call(fildes, fsync);
}

int vfs_rename(const char *old, const char *new) {

	char partold[VFS_MAX_PARTITION_NAME], partnew[VFS_MAX_PARTITION_NAME];
	struct vfs_partition *pold, *pnew;
	
	if (vfs_extract_partition(old, partold, VFS_MAX_PARTITION_NAME) < 0)
		return -1;

	if (vfs_extract_partition(new, partnew, VFS_MAX_PARTITION_NAME) < 0)
		return -1;

	pold = vfs_find_partition(partold);
	pnew = vfs_find_partition(partnew);

	/* We don't support moving between filesystems yet */
	if (pold == pnew) {
		if (!vfs_part_has_fop(pold, rename))
			return -1;

		if (pold->true_name) {
			subst_partition(old, pold->true_name, partold, VFS_MAX_PARTITION_NAME);
			subst_partition(new, pnew->true_name, partnew, VFS_MAX_PARTITION_NAME);
			old = partold;
			new = partnew;
		}

		return vfs_part_call(pold, rename, old, new);
	} else {
		printf("Cannot rename from one filesystem to another\r\n");
		return -1;
	}
}

int vfs_link(const char *oldpath, const char *newpath) {

	char partold[VFS_MAX_PARTITION_NAME], partnew[VFS_MAX_PARTITION_NAME];
	struct vfs_partition *p;
	
	if (vfs_extract_partition(oldpath, partold, VFS_MAX_PARTITION_NAME) < 0)
		return -1;

	if (vfs_extract_partition(newpath, partnew, VFS_MAX_PARTITION_NAME) < 0)
		return -1;

	/* link(2) cannot span file systems */
	if (strncmp(partold, partnew, VFS_MAX_PARTITION_NAME) != 0)
		return -1;

	p = vfs_find_partition(partold);
	if (!vfs_part_has_fop(p, link))
		return -1;

	if (p->true_name) {
		subst_partition(oldpath, p->true_name, partold, VFS_MAX_PARTITION_NAME);
		subst_partition(newpath, p->true_name, partnew, VFS_MAX_PARTITION_NAME);
		oldpath = partold;
		newpath = partnew;
	}

	return vfs_part_call(p, link, oldpath, newpath);
}

int vfs_unlink(const char *path) {

	char part[VFS_MAX_PARTITION_NAME];
	struct vfs_partition *p;
	
	if (vfs_extract_partition(path, part, VFS_MAX_PARTITION_NAME) < 0)
		return -1;

	p = vfs_find_partition(part);
	if (!vfs_part_has_fop(p, unlink))
		return -1;

	if (p->true_name) {
		subst_partition(path, p->true_name, part, VFS_MAX_PARTITION_NAME);
		path = part;
	}

	return vfs_part_call(p, unlink, path);
}

int vfs_stat(const char *path, struct stat *buf) {

	char part[VFS_MAX_PARTITION_NAME];
	struct vfs_partition *p;
	
	if (vfs_extract_partition(path, part, VFS_MAX_PARTITION_NAME) < 0)
		return -1;

	p = vfs_find_partition(part);
	if (!vfs_part_has_fop(p, stat))
		return -1;

	if (p->true_name) {
		subst_partition(path, p->true_name, part, VFS_MAX_PARTITION_NAME);
		path = part;
	}

	return vfs_part_call(p, stat, path, buf);
}

int vfs_lstat(const char *path, struct stat *buf) {

	char part[VFS_MAX_PARTITION_NAME];
	struct vfs_partition *p;
	
	if (vfs_extract_partition(path, part, VFS_MAX_PARTITION_NAME) < 0)
		return -1;

	p = vfs_find_partition(part);
	if (!vfs_part_has_fop(p, lstat))
		return -1;

	if (p->true_name) {
		subst_partition(path, p->true_name, part, VFS_MAX_PARTITION_NAME);
		path = part;
	}

	return vfs_part_call(p, lstat, path, buf);
}

int vfs_fstat(int fildes, struct stat *buf) {

	if (!vfs_fd_has_fop(fildes, fstat))
		return -1;

	return vfs_fd_call(fildes, fstat, buf);
}

DIR *vfs_opendir(const char *path) {

	char part[VFS_MAX_PARTITION_NAME];
	struct vfs_partition *p;
	int fd_num;

	if (vfs_extract_partition(path, part, VFS_MAX_PARTITION_NAME) < 0)
		return NULL;

	p = vfs_find_partition(part);
	if (!vfs_part_has_fop(p, opendir))
		return NULL;

	if (p->true_name) {
		subst_partition(path, p->true_name, part, VFS_MAX_PARTITION_NAME);
		path = part;
	}

	fd_num = vfs_fd_get();
	if (fd_num < 0)
		return NULL;

	fd_table[fd_num - VFS_FD_OFFSET].partition = p;
	fd_table[fd_num - VFS_FD_OFFSET].path = strndup(path, VFS_MAX_PARTITION_NAME);

	if (vfs_part_call(p, opendir, &fd_table[fd_num - VFS_FD_OFFSET], path) < 0) {
		if (fd_table[fd_num - VFS_FD_OFFSET].path)
			free(fd_table[fd_num - VFS_FD_OFFSET].path);
		vfs_fd_put(fd_num);
		return NULL;
	}

	return &fd_table[fd_num - VFS_FD_OFFSET].dirp;
}

int vfs_closedir(DIR *drp) {

	int ret, fd;

	fd = vfs_dirp_to_fd(drp);
	if (!vfs_fd_has_fop(fd, closedir))
		return -1;

	ret = vfs_fd_call(fd, closedir);
	if (fd_table[fd - VFS_FD_OFFSET].path)
		free(fd_table[fd - VFS_FD_OFFSET].path);

	vfs_fd_put(vfs_dirp_to_fd(drp));

	return ret;
}

struct dirent *vfs_readdir(DIR *drp) {

	int ret;

	if (!vfs_fd_has_fop(vfs_dirp_to_fd(drp), readdir))
		return NULL;

	ret = fd_table[vfs_dirp_to_fd(drp) - VFS_FD_OFFSET].partition->fops->readdir(&fd_table[vfs_dirp_to_fd(drp) - VFS_FD_OFFSET], &drp->ent);

	if (ret < 0)
		return NULL;

	return &drp->ent;
}

void vfs_rewinddir(DIR *drp) {

	if (!vfs_fd_has_fop(vfs_dirp_to_fd(drp), rewinddir))
		return;

	fd_table[vfs_dirp_to_fd(drp) - VFS_FD_OFFSET].partition->fops->rewinddir(&fd_table[vfs_dirp_to_fd(drp) - VFS_FD_OFFSET]);
}

int vfs_mkdir(const char *path, mode_t mode) {
	
	char part[VFS_MAX_PARTITION_NAME];
	struct vfs_partition *p;
	
	if (vfs_extract_partition(path, part, VFS_MAX_PARTITION_NAME) < 0)
		return -1;

	p = vfs_find_partition(part);
	if (!p || !p->fops || !p->fops->mkdir)
		return -1;

	if (p->true_name) {
		subst_partition(path, p->true_name, part, VFS_MAX_PARTITION_NAME);
		path = part;
	}

	return p->fops->mkdir(path, mode);
}

int vfs_rmdir(const char *path) {

	char part[VFS_MAX_PARTITION_NAME];
	struct vfs_partition *p;
	
	if (vfs_extract_partition(path, part, VFS_MAX_PARTITION_NAME) < 0)
		return -1;

	p = vfs_find_partition(part);
	if (!p || !p->fops || !p->fops->rmdir)
		return -1;

	if (p->true_name) {
		subst_partition(path, p->true_name, part, VFS_MAX_PARTITION_NAME);
		path = part;
	}

	return p->fops->rmdir(path);
}

void vfs_seekdir(DIR *drp, long offset) {

	if (!vfs_fd_has_fop(vfs_dirp_to_fd(drp), seekdir))
		return;

	fd_table[vfs_dirp_to_fd(drp) - VFS_FD_OFFSET].partition->fops->seekdir(&fd_table[vfs_dirp_to_fd(drp) - VFS_FD_OFFSET], offset);
}

long vfs_telldir(DIR *drp) {

	long location;

	if (!vfs_fd_has_fop(vfs_dirp_to_fd(drp), telldir))
		return -1;

	fd_table[vfs_dirp_to_fd(drp) - VFS_FD_OFFSET].partition->fops->telldir(&fd_table[vfs_dirp_to_fd(drp) - VFS_FD_OFFSET], &location);

	return location;
}

int closedir(DIR *drp) {
	return vfs_closedir(drp);
}

DIR *opendir(const char *name) {
	return vfs_opendir(name);
}

struct dirent *readdir(DIR *drp) {
	return vfs_readdir(drp);
}

void rewinddir(DIR *drp) {
	vfs_rewinddir(drp);
}

int mkdir(const char *path, mode_t mode) {
	return vfs_mkdir(path, mode);
}

int rmdir(const char *path) {
	return vfs_rmdir(path);
}

void seekdir(DIR *drp, long offset) {
	vfs_seekdir(drp, offset);
}

long telldir(DIR *drp) {
	return vfs_telldir(drp);
}

