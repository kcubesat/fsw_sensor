#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <util/console.h>
#include <util/bytesize.h>
#include <util/crc32.h>
#include <util/hexdump.h>
#include <dev/usart.h>
#include <dev/cpu.h>

#include <dirent.h>

#define MAX_PATH_LENGTH 128

int cmd_fs_test(struct command_context *ctx) {

	int fd;
	int size = 100000;
	unsigned int ms;
	portTickType start_time, stop_time;

	/* Get args */
	char path[100];
	char * args = command_args(ctx);
	if (args == NULL || sscanf(args, "%s", path) < 1)
		return CMD_ERROR_SYNTAX;

	/* Open file */
	fd = open(path, O_CREAT | O_TRUNC | O_RDWR);
	if (fd < 0) {
		printf("Failed to open %s\r\n", path);
		return CMD_ERROR_FAIL;
	}

	/* Time writing */
	char * data_w = pvPortMalloc(size);
	if (data_w == NULL) {
		printf("Failed to allocate memory buffer\r\n");
		goto err_open;
	}

	memset(data_w, 0x55, size);
	start_time = xTaskGetTickCount();
	if (write(fd, data_w, size) != size) {
		printf("Failed to write test data to %s\r\n", path);
		goto err_write;
	}

	fsync(fd);
	stop_time = xTaskGetTickCount();
	ms = (stop_time - start_time) * (1000/configTICK_RATE_HZ);
	printf("Wrote %u bytes in %u ms (%u KBytes/sec)\r\n", size, ms, size/ms);

	/* Go to beginning */
	lseek(fd, 0, SEEK_SET);

	/* Time reading */
	char * data_r = pvPortMalloc(size);
	if (data_w == NULL) {
		printf("Failed to allocate memory buffer\r\n");
		goto err_write;
	}

	start_time = xTaskGetTickCount();
	if (read(fd, data_r, size) != size) {
		printf("Failed to read test data from %s\r\n", path);
		goto err_read;
	}

	fsync(fd);
	stop_time = xTaskGetTickCount();
	ms = (stop_time - start_time) * (1000/configTICK_RATE_HZ);
	printf("Read %u bytes in %u ms (%u KBytes/sec)\r\n", size, ms, size/ms);

	/* Verify data */
	if (memcmp(data_w, data_r, size) != 0) {
		printf("Failed to verify data (read != written)!\r\n");
	} else {
		printf("Succesfully verified data\r\n");
	}

err_read:
	vPortFree(data_r);
err_write:
	vPortFree(data_w);
err_open:
	close(fd);

	return CMD_ERROR_NONE;

}

static int objects_under(const char *dir) {
	int count = 0;
	DIR *dirp;
	struct dirent *ent;

	dirp = opendir(dir);
	
	if (dirp) {
		while ((ent = readdir(dirp)) != NULL) {
			if (ent->d_name[0] != '.')
				count++;
		}
		closedir(dirp);
	}

	return count;
}

int cmd_fs_ls(struct command_context *ctx) {

	DIR * dirp;
	struct dirent *ent;
	struct stat stat_buf;
	char buf[MAX_PATH_LENGTH+2];
	char * sub;
	char bytebuf[25];

	/* Get args */
	char path[100];
	char * args = command_args(ctx);
	if (args == NULL || sscanf(args, "%s", path) < 1)
		return CMD_ERROR_SYNTAX;

	/*if (path[strlen(path)-1] == '/')
		path[strlen(path)-1] = '\0';*/

	dirp = opendir(path);
	if (dirp == NULL) {
		printf("ls: cannot open '%s' for list\r\n", path);
		return CMD_ERROR_FAIL;
	}

	/* Loop through directories */
	while ((ent = readdir(dirp))) {
		if (ent->d_name[0] == '.')
			continue;	
		strncpy(buf, path, MAX_PATH_LENGTH);
		sub = buf;
		if (path[strlen(path)-1] != '/')
			sub = strcat(buf, "/");
		sub = strcat(sub, ent->d_name);
		if (ent->d_type & DT_DIR) {
			sprintf(bytebuf, "%d", objects_under(sub));
			strcat(ent->d_name, "/");
		} else {
			stat(sub, &stat_buf);
			bytesize(bytebuf, 25, stat_buf.st_size);
		}

		/* Name */
		printf("%-15s %6s\r\n", ent->d_name, bytebuf);
	}

	closedir(dirp);

	return CMD_ERROR_NONE;

}

int cmd_fs_mkdir(struct command_context *ctx) {

	/* Get args */
	char path[100];
	char * args = command_args(ctx);
	if (args == NULL || sscanf(args, "%s", path) < 1)
		return CMD_ERROR_SYNTAX;

	if (mkdir(path, 0) < 0)
		printf("Create %s failed\r\n", path);

	return CMD_ERROR_NONE;

}

int cmd_fs_rm(struct command_context *ctx) {

	struct stat st;
	int ret;

	/* Get args */
	char path[100];
	char * args = command_args(ctx);
	if (args == NULL || sscanf(args, "%s", path) < 1)
		return CMD_ERROR_SYNTAX;

	if (stat(path, &st) < 0) {
		printf("rm: cannot stat %s\r\n", path);
		return CMD_ERROR_FAIL;
	}

	if (st.st_mode & S_IFDIR)
		ret = rmdir(path);
	else
		ret = remove(path);

	if (ret != 0)
		printf("rm: cannot remove %s\r\n", path);

	return CMD_ERROR_NONE;

}

int cmd_fs_mv(struct command_context *ctx) {

	int ret;

	/* Get args */
	char *old, *new;
	if (ctx->argc != 3)
		return CMD_ERROR_SYNTAX;

	old = ctx->argv[1];
	new = ctx->argv[2];
	ret = rename(old, new);

	if (ret != 0)
		printf("rm: cannot move %s\r\n", old);

	return CMD_ERROR_NONE;

}

int cmd_fs_touch(struct command_context *ctx) {

	/* Get args */
	char path[100];
	char * args = command_args(ctx);
	if (args == NULL || sscanf(args, "%s", path) < 1)
		return CMD_ERROR_SYNTAX;

	/* Open file */
	int fd = open(path, O_RDWR | O_CREAT);
	if (fd < 0) {
		printf("touch: cannot touch %s\r\n", path);
	} else {
		close(fd);
	}

	return CMD_ERROR_NONE;

}

int cmd_fs_cat(struct command_context *ctx) {

	/* Get args */
	char path[100];
	char * args = command_args(ctx);
	if (args == NULL || sscanf(args, "%s", path) < 1)
		return CMD_ERROR_SYNTAX;

	/* Open file */
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		printf("cat: cannot cat %s\r\n", path);
		return CMD_ERROR_FAIL;
	}

	char buf;
	while(read(fd, &buf, 1) > 0)
		printf("%c", buf);

	close(fd);

	if ((char)buf != '\n' && (char)buf != '\r')
		printf("\r\n");

	return CMD_ERROR_NONE;

}

/*! \brief Minimalist file editor to append char to a file.
 *
 * \note Hit ^q to exit and save file.
 */
static void cmd_fs_append_from_usart(int fd) {

	char c;
	int quit = 0;
	printf("Minimalist file editor to append chars to a file.\r\nHit ^q to exit and save file.\r\n");

	// Wait for ^q to quit.
	while (quit == 0) {

		/* Get character */
		c = usart_getc(USART_CONSOLE);

		switch (c) {

		/* CTRL + Q */
		case 0x11:
			quit = 1;
			break;

		default:
			usart_putc(USART_CONSOLE, c);
			if (write(fd, &c, 1) < 1)
				quit = 1;
			break;
		}
	}
}

int cmd_fs_append(struct command_context *ctx) {

	/* Get args */
	char path[100];
	char * args = command_args(ctx);
	if (args == NULL || sscanf(args, "%s", path) < 1)
		return CMD_ERROR_SYNTAX;

	/* Open file */
	int fd = open(path, O_APPEND | O_WRONLY);
	if (fd < 0) {
		printf("append: cannot append %s\r\n", path);
	} else {
		cmd_fs_append_from_usart(fd);
		close(fd);
	}

	return CMD_ERROR_NONE;

}

int cmd_fs_chksum(struct command_context *ctx) {

	/* Calculate CRC32 */
	char byte;
	unsigned int crc = 0xFFFFFFFF;

	/* Get args */
	char path[100];
	char * args = command_args(ctx);
	if (args == NULL || sscanf(args, "%s", path) < 1)
		return CMD_ERROR_SYNTAX;

	FILE *fp = fopen(path, "r");
	if (!fp) {
		printf("Failed to checksum %s\r\n", path);
		return CMD_ERROR_FAIL;
	}

	while(fread(&byte, 1, 1, fp) == 1)
		crc = chksum_crc32_step(crc, byte);

	crc = (crc ^ 0xFFFFFFFF);

	printf("Checksum completed: %X\r\n", crc);

	fclose(fp);

	return CMD_ERROR_NONE;

}

int cmd_fs_hexdump(struct command_context *ctx) {

	char * buf;
	struct stat stat_buf;
	size_t size;

	/* Get args */
	char path[100];
	char * args = command_args(ctx);
	if (args == NULL || sscanf(args, "%s", path) < 1)
		return CMD_ERROR_SYNTAX;

	/* Open file */
	FILE * fp = fopen(path, "r");
	if (!fp) {
		printf("hexdump: cannot open %s\r\n", path);
		goto err_open;
	}

	/* Read file size */
	if (stat(path, &stat_buf) != 0) {
		printf("hexdump: cannot stat %s\r\n", path);
		goto err_stat;
	}

	size = stat_buf.st_size;

	/* Allocate buffer */
	buf = pvPortMalloc(size);
	if (!buf) {
		printf("hexdump: cannot allocate memory for %s\r\n", path);
		goto err_stat;
	}

	/* Read file */
	if (fread(buf, 1, size, fp) != size) {
		printf("hexdump: failed to read %zu bytes from %s\r\n", size, path);
		goto err;
	}

	/* Finally, hexdump */
	hex_dump(buf, size);

err:
	vPortFree(buf);
err_stat:
	fclose(fp);
err_open:
	return CMD_ERROR_NONE;

}

struct command __root_command fs_commands[] = {
	{
		.name = "testfs",
		.help = "Test file system r/w-speeds",
		.handler = cmd_fs_test,
		.usage = "<file>",
	},{
		.name = "ls",
		.help = "List directory contents",
		.handler = cmd_fs_ls,
		.usage = "<dir>",
	},{
		.name = "rm",
		.help = "Remove file or directory",
		.handler = cmd_fs_rm,
		.usage = "<file/dir>",
	},{
		.name = "mv",
		.help = "Move file or directory",
		.handler = cmd_fs_mv,
		.usage = "<old> <new>",
	},{
		.name = "mkdir",
		.help = "Make directory",
		.handler = cmd_fs_mkdir,
		.usage = "<dir>",
	},{
		.name = "touch",
		.help = "Create empty file",
		.handler = cmd_fs_touch,
		.usage = "<file>",
	},{
		.name = "cat",
		.help = "Show contents of file",
		.handler = cmd_fs_cat,
		.usage = "<file>",
	},{
		.name = "append",
		.help = "Append data to file",
		.handler = cmd_fs_append,
		.usage = "<file>",
	},{
		.name = "chksum",
		.help = "Calculate CRC32 checksum of file",
		.handler = cmd_fs_chksum,
		.usage = "<path>",
	},{
		.name = "hexdump",
		.help = "Hexdump file",
		.handler = cmd_fs_hexdump,
		.usage = "<path>",
	},
};

/* Add debugging commands */
void cmd_fs_setup(void) {
	command_register(fs_commands);
}
