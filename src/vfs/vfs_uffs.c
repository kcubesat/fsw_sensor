/**
 * @file vfs_uffs.c
 * VFS backend for UFFS
 *
 * @author Jeppe Ledet-Pedersen
 * Copyright 2011 GomSpace ApS. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <uffs/uffs.h>
#include <uffs/uffs_fd.h>
#include <uffs/uffs_public.h>

#include <vfs/vfs.h>

int vfs_uffs_open(struct vfs_fd *fd, const char *path, int oflag, int mode) {
	int fildes = uffs_open(path, oflag);
	if (fildes < 0)
		return -1;

	fd->fd = fildes;

	return 0;
}

int vfs_uffs_close(struct vfs_fd *fd) {
	return uffs_close(fd->fd);
}

int vfs_uffs_read(struct vfs_fd *fd, void *buf, int nbyte) {
	return uffs_read(fd->fd, buf, nbyte);
}

int vfs_uffs_write(struct vfs_fd *fd, const void *buf, int nbyte) {
	return uffs_write(fd->fd, buf, nbyte);
}

int vfs_uffs_lseek(struct vfs_fd *fd, off_t *retoffset, off_t offset, int whence) {
	*retoffset = uffs_seek(fd->fd, offset, whence);
	return 0;
}

int vfs_uffs_fsync(struct vfs_fd *fd) {
	return uffs_flush(fd->fd);
}

int vfs_uffs_rename(const char *old, const char *new) {
	return uffs_rename(old, new);
}

int vfs_uffs_unlink(const char *pathname) {
	return uffs_remove(pathname);
}

int vfs_uffs_stat(const char *path, struct stat *buf) {
	struct uffs_stat st;
	int ret = uffs_stat(path, &st);

	buf->st_dev = st.st_dev;
	buf->st_ino= st.st_ino;
	buf->st_mode= st.st_mode;
	buf->st_nlink= st.st_nlink;
	buf->st_uid= st.st_uid;
	buf->st_gid= st.st_gid;
	buf->st_rdev= st.st_rdev;
	buf->st_size= st.st_size;
	buf->st_blksize= st.st_blksize;
	buf->st_blocks= st.st_blocks;
	buf->st_atime= st.st_atime;
	buf->st_mtime= st.st_mtime;
	buf->st_ctime= st.st_ctime;

	return ret;
}

int vfs_uffs_lstat(const char *path, struct stat *buf) {
	return vfs_uffs_stat(path, buf);
}

int vfs_uffs_fstat(struct vfs_fd *fd, struct stat *buf) {
	struct uffs_stat st;
	int ret = uffs_fstat(fd->fd, &st);

	buf->st_dev = st.st_dev;
	buf->st_ino= st.st_ino;
	buf->st_mode= st.st_mode;
	buf->st_nlink= st.st_nlink;
	buf->st_uid= st.st_uid;
	buf->st_gid= st.st_gid;
	buf->st_rdev= st.st_rdev;
	buf->st_size= st.st_size;
	buf->st_blksize= st.st_blksize;
	buf->st_blocks= st.st_blocks;
	buf->st_atime= st.st_atime;
	buf->st_mtime= st.st_mtime;
	buf->st_ctime= st.st_ctime;

	return ret;
}

int vfs_uffs_opendir(struct vfs_fd *fd, const char *name) {
	fd->dirp.real = uffs_opendir(name);
	if (!fd->dirp.real)
		return -1;
	return 0;
}

int vfs_uffs_closedir(struct vfs_fd *fd) {
	return uffs_closedir(fd->dirp.real);
}

int vfs_uffs_readdir(struct vfs_fd *fd, struct dirent *dirent) {
	struct uffs_dirent *ent = uffs_readdir(fd->dirp.real);
	if (ent == NULL)
		return -1;

	dirent->d_ino = ent->d_ino;
	dirent->d_off = ent->d_off;
	dirent->d_reclen = ent->d_reclen;
	dirent->d_namelen = ent->d_namelen;
	dirent->d_type = (ent->d_type & FILE_ATTR_DIR) ? DT_DIR : 0;
	strncpy(dirent->d_name, ent->d_name, MAX_FILENAME_LENGTH);

	return 0;
}

int vfs_uffs_rewinddir(struct vfs_fd *fd) {
	uffs_rewinddir(fd->dirp.real);
	return 0;
}

int vfs_uffs_mkdir(const char *path, mode_t mode) {
	return uffs_mkdir(path);
}

int vfs_uffs_rmdir(const char *path) {
	return uffs_rmdir(path);
}

struct vfs_ops vfs_uffs_ops = {
	.fs = "uffs",
	.open = vfs_uffs_open,
	.close = vfs_uffs_close,
	.read = vfs_uffs_read,
	.write = vfs_uffs_write,
	.lseek = vfs_uffs_lseek,
	.fsync = vfs_uffs_fsync,
	.rename = vfs_uffs_rename,
	.unlink = vfs_uffs_unlink,
	.stat = vfs_uffs_stat,
	.lstat = vfs_uffs_lstat,
	.fstat = vfs_uffs_fstat,
	.closedir = vfs_uffs_closedir,
	.opendir = vfs_uffs_opendir,
	.readdir = vfs_uffs_readdir,
	.rewinddir = vfs_uffs_rewinddir,
	.mkdir = vfs_uffs_mkdir, 
	.rmdir = vfs_uffs_rmdir,
};
