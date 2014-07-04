/*
 * cmd_fat_sd.c
 *
 *  Created on: 18/03/2010
 *      Author: oem
 */

#include <string.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <util/console.h>
#include <dev/usart.h>
#include <fat_sd/ff.h>
#include <fat_sd/cmd_fat.h>

char cur_ls_path[128];

struct command fat_subcommands[] = {
	{
		.name = "mkfs",
		.help = "FAT format file system",
		.usage = "<device>",
		.handler = cmd_fat_mkfs,
	},{
		.name = "free",
		.help = "FAT show free space",
		.handler = cmd_fat_free,
	},
};

struct command __root_command fat_commands[] = {
	{
		.name = "fat",
		.help = "FAT filesystem commands",
		.chain = INIT_CHAIN(fat_subcommands),
	}
};

void cmd_fat_setup(void) {

	memset(cur_ls_path, 0, 128);

	/* If memory was loaded okay, add plugins */
	command_register(fat_commands);

}

int cmd_fat_mkfs(struct command_context *ctx) {

	/* Get args */
	char * args = command_args(ctx);
	unsigned int dev;
	if (sscanf(args, "%u", &dev) != 1)
		return CMD_ERROR_SYNTAX;

	printf("Formatting drive %u\r\n", (BYTE) dev);

	int result;
	result = f_mkfs(dev, 0, 0);
	printf("Format Result %u\r\n", result);

	return CMD_ERROR_NONE;

}

int cmd_fat_free(struct command_context *ctx) {

	FATFS * fs;
	unsigned long int freeclusters;
	int result = f_getfree("/", &freeclusters, &fs);
	printf("Wee %u freeclusters %lu fs %p\r\n", result, freeclusters, fs);
	if (result == FR_OK) {
		printf("Clusters free %lu sectors per cluster %u, sector size %u\r\n", freeclusters, fs->csize, fs->ssize);
		printf("Bytes free: %lu of %lu\r\n", freeclusters * fs->csize * fs->ssize, (fs->n_fatent - 2) * fs->csize * fs->ssize);
	}

	return CMD_ERROR_NONE;

}
