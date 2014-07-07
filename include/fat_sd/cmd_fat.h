/*
 * fat_cmd.h
 *
 *  Created on: 24/11/2009
 *      Author: johan
 */

#ifndef FAT_CMD_H_
#define FAT_CMD_H_

#include <command/command.h>

void cmd_fat_setup(void);
int cmd_fat_test(struct command_context *ctx);
int cmd_fat_mkfs(struct command_context *ctx);
int cmd_fat_free(struct command_context *ctx);
int cmd_fat_ls(struct command_context *ctx);
int cmd_fat_cd(struct command_context *ctx);
int cmd_fat_mkdir(struct command_context *ctx);
int cmd_fat_rm(struct command_context *ctx);
int cmd_fat_touch(struct command_context *ctx);
int cmd_fat_cat(struct command_context *ctx);
int cmd_fat_append(struct command_context *ctx);
int cmd_fat_chdrv(struct command_context *ctx);

#endif /* FAT_CMD_H_ */
