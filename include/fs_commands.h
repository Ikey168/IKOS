/* IKOS File System Commands Header
 * Command-line utilities for filesystem operations
 */

#ifndef FS_COMMANDS_H
#define FS_COMMANDS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Execute a filesystem command from a command line string */
int fs_execute_command(const char* cmdline);

/* Interactive filesystem shell */
void fs_shell(void);

/* Individual command functions */
int cmd_mkdir(int argc, char* argv[]);
int cmd_rmdir(int argc, char* argv[]);
int cmd_ls(int argc, char* argv[]);
int cmd_cd(int argc, char* argv[]);
int cmd_pwd(int argc, char* argv[]);
int cmd_touch(int argc, char* argv[]);
int cmd_rm(int argc, char* argv[]);
int cmd_cp(int argc, char* argv[]);
int cmd_mv(int argc, char* argv[]);
int cmd_cat(int argc, char* argv[]);
int cmd_echo(int argc, char* argv[]);
int cmd_stat(int argc, char* argv[]);
int cmd_chmod(int argc, char* argv[]);
int cmd_find(int argc, char* argv[]);
int cmd_help(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif

#endif /* FS_COMMANDS_H */
