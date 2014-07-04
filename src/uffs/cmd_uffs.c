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

/* UFFS includes */
#include <uffs/uffs_utils.h>
#include <uffs/uffs_mtb.h>
#include <uffs/uffs_fd.h>
#include <uffs/uffs_config.h>
#include <uffs/uffs_pool.h>
#include <dirent.h>

/* UFFS Device interfaces */
#include <uffs/at45db642d.h>
#include <uffs/at49bv320dt.h>

#define MAX_PATH_LENGTH 128

int cmd_uffs_mkfs(struct command_context *ctx) {

	/* Get args */
	char path[100];
	char * args = command_args(ctx);
	if (args == NULL || sscanf(args, "%s", path) < 1)
		return CMD_ERROR_SYNTAX;

	uffs_Device * dev;

	printf("Formatting partition: \"%s\"\r\n", path);

	/* Get device struct */
	dev = uffs_GetDeviceFromMountPoint(path);
	if (dev == NULL) {
		printf("Failed to get device for mount point %s\r\n", path);
		return CMD_ERROR_FAIL;
	}

	/* Warn if other tasks have open file handles */
	if (dev->ref_count != 1)
		printf("WARNING: ref count of device is %d\r\n", dev->ref_count);

	/* Attempt format */
	if (uffs_FormatDevice(dev, U_TRUE) != U_SUCC)
		printf("Failed to format partition\r\n");
	else
		printf("Successfully formatted partition\r\n");

	/* Reset to let all tasks reopen their fd's */
	if (cpu_set_reset_cause)
		cpu_set_reset_cause(CPU_RESET_FS_FORMAT);
	cpu_reset();

	/* We should never get to this point, but if we do
	 * decrease the device reference count */
	uffs_PutDevice(dev);

	return CMD_ERROR_NONE;

}

int cmd_uffs_erase(struct command_context *ctx) {

	if (ctx->argc != 2)
		return CMD_ERROR_SYNTAX;
	
	if (!strcmp(ctx->argv[1], "flash")) 
		return flash_uffs_chip_erase() ? CMD_ERROR_FAIL : CMD_ERROR_NONE;
	else if (!strcmp(ctx->argv[1], "df"))
		return df_uffs_chip_erase() ? CMD_ERROR_FAIL : CMD_ERROR_NONE;

	printf("Invalid flash. Use either \'flash\' or \'df\'\r\n");
	return CMD_ERROR_SYNTAX;

}

int cmd_uffs_stat(struct command_context *ctx) {

	/* Get args */
	char path[100];
	char * args = command_args(ctx);
	if (args == NULL || sscanf(args, "%s", path) < 1)
		return CMD_ERROR_SYNTAX;

	uffs_Device * dev;
	uffs_FlashStat * s;
	TreeNode * node;

	dev = uffs_GetDeviceFromMountPoint(path);
	if (dev == NULL) {
		printf("Can't get device from mount point %s\r\n", path);
		return CMD_ERROR_FAIL;
	}

	s = &(dev->st);

	printf("Basic info\r\n");
	printf("  TreeNode size:         %zu\r\n", sizeof(TreeNode));
	printf("  TagStore size:         %zu\r\n", sizeof(struct uffs_TagStoreSt));
	printf("  MaxCachedBlockInfo:    %d\r\n", MAX_CACHED_BLOCK_INFO);
	printf("  MaxPageBuffers:        %d\r\n", MAX_PAGE_BUFFERS);
	printf("  MaxDirtyPagesPerBlock: %d\r\n", MAX_DIRTY_PAGES_IN_A_BLOCK);
	printf("  MaxPathLength:         %d\r\n", MAX_PATH_LENGTH);
	printf("  MaxObjectHandles:      %d\r\n", MAX_OBJECT_HANDLE);
	printf("  FreeObjectHandles:     %d\r\n", uffs_PoolGetFreeCount(uffs_GetObjectPool()));
	printf("  MaxDirHandles:         %d\r\n", MAX_DIR_HANDLE);
	printf("  FreeDirHandles:        %d\r\n", uffs_PoolGetFreeCount(uffs_DirEntryBufGetPool()));

	printf("Statistics for '%s'\r\n", path);
	printf("  Block Erased:          %d\r\n", s->block_erase_count);
	printf("  Write Page:            %d\r\n", s->page_write_count);
	printf("  Write Spare:           %d\r\n", s->spare_write_count);
	printf("  Read Page:             %d\r\n", s->page_read_count - s->page_header_read_count);
	printf("  Read Header:           %d\r\n", s->page_header_read_count);
	printf("  Read Spare:            %d\r\n", s->spare_read_count);

	printf("Partition info for '%s'\r\n", path);
	printf("  Space total:           %d\r\n", uffs_GetDeviceTotal(dev));
	printf("  Space used:            %d\r\n", uffs_GetDeviceUsed(dev));
	printf("  Space free:            %d\r\n", uffs_GetDeviceFree(dev));
	printf("  Page Size:             %d\r\n", dev->attr->page_data_size);
	printf("  Spare Size:            %d\r\n", dev->attr->spare_size);
	printf("  Pages Per Block:       %d\r\n", dev->attr->pages_per_block);
	printf("  Block size:            %d\r\n", dev->attr->page_data_size * dev->attr->pages_per_block);
	printf("  Total blocks:          %d of %d\r\n", (dev->par.end - dev->par.start + 1), dev->attr->total_blocks);
	if (dev->tree.bad) {
		printf("Bad blocks: ");
		node = dev->tree.bad;
		while(node) {
			printf("%d, ", node->u.list.block);
			node = node->u.list.next;
		}
		printf("\r\n");
	}

	uffs_BufInspect(dev);

	uffs_PutDevice(dev);

	return CMD_ERROR_NONE;

}

int cmd_uffs_df(struct command_context *ctx) {
	uffs_MountTable * tab = uffs_MtbGetMounted();

	if (!tab) {
		printf("No mount points to list\r\n");
		return CMD_ERROR_NONE;
	}

	printf("Filesystem\tSize\tUsed\tAvail\tUse%%\r\n");
	while (tab) {
		char sizebuf[100], usebuf[100], availbuf[100];

		bytesize(sizebuf, 100, uffs_GetDeviceTotal(tab->dev));
		bytesize(usebuf, 100, uffs_GetDeviceUsed(tab->dev));
		bytesize(availbuf, 100, uffs_GetDeviceFree(tab->dev));

		printf("%s\t\t%s\t%s\t%s\t%d%%\r\n",
				tab->mount,
				sizebuf,
				usebuf,
				availbuf,
				(uffs_GetDeviceUsed(tab->dev)*100)/uffs_GetDeviceTotal(tab->dev)
				);
		tab = tab->next;
	}

	return CMD_ERROR_NONE;
}

struct command uffs_subcommands[] = {
	{
		.name = "mkfs",
		.help = "Format file system partition",
		.handler = cmd_uffs_mkfs,
		.usage = "<partition>",
	},{
		.name = "erase",
		.help = "Erase DataFlash completely",
		.handler = cmd_uffs_erase,
		.usage = "<flash>",
	},{
		.name = "stat",
		.help = "Show file system statistics",
		.handler = cmd_uffs_stat,
		.usage = "<partition>",
	},{
		.name = "df",
		.help = "Report file system disk space usage",
		.handler = cmd_uffs_df,
	},
};

struct command __root_command uffs_commands[] = {
	{
		.name = "uffs",
		.help = "UFFS filesystem commands",
		.chain = INIT_CHAIN(uffs_subcommands),
	},
};

/* Add debugging commands */
void cmd_uffs_setup(void) {
	command_register(uffs_commands);
}
