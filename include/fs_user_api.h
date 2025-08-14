/* IKOS User-Space File System API Header
 * Provides user-space interface for file and directory operations
 */

#ifndef FS_USER_API_H
#define FS_USER_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "vfs.h"

/* ===== File and Directory Commands ===== */

/* Directory operations */
int fs_mkdir(const char* path, uint32_t mode);
int fs_rmdir(const char* path);
int fs_chdir(const char* path);
char* fs_getcwd(char* buf, size_t size);

/* Directory listing */
typedef struct fs_dirent {
    char name[256];
    vfs_file_type_t type;
    uint64_t size;
    uint32_t permissions;
    uint64_t mtime;
} fs_dirent_t;

int fs_opendir(const char* path);
int fs_readdir(int dirfd, fs_dirent_t* entry);
int fs_closedir(int dirfd);
int fs_ls(const char* path, fs_dirent_t* entries, int max_entries);

/* File operations */
int fs_open(const char* path, uint32_t flags, uint32_t mode);
int fs_close(int fd);
ssize_t fs_read(int fd, void* buffer, size_t count);
ssize_t fs_write(int fd, const void* buffer, size_t count);
int fs_unlink(const char* path);
int fs_rename(const char* oldpath, const char* newpath);
int fs_copy(const char* src, const char* dest);

/* File positioning */
uint64_t fs_lseek(int fd, uint64_t offset, int whence);
uint64_t fs_tell(int fd);

/* File attributes and metadata */
int fs_stat(const char* path, vfs_stat_t* stat);
int fs_fstat(int fd, vfs_stat_t* stat);
int fs_chmod(const char* path, uint32_t mode);
int fs_touch(const char* path);

/* File existence and type checking */
bool fs_exists(const char* path);
bool fs_is_file(const char* path);
bool fs_is_directory(const char* path);
uint64_t fs_size(const char* path);

/* Path utilities */
char* fs_basename(const char* path);
char* fs_dirname(const char* path);
char* fs_realpath(const char* path, char* resolved);
int fs_split_path(const char* path, char* dir, char* file);

/* Convenience functions */
int fs_create_file(const char* path, uint32_t mode);
ssize_t fs_read_file(const char* path, void* buffer, size_t size);
ssize_t fs_write_file(const char* path, const void* buffer, size_t size);
int fs_append_file(const char* path, const void* buffer, size_t size);

/* Error handling */
int fs_get_last_error(void);
const char* fs_error_string(int error);

/* ===== Command Implementation ===== */

/* Built-in command functions */
int cmd_mkdir(int argc, char* argv[]);
int cmd_rmdir(int argc, char* argv[]);
int cmd_ls(int argc, char* argv[]);
int cmd_cd(int argc, char* argv[]);
int cmd_pwd(int argc, char* argv[]);
int cmd_cat(int argc, char* argv[]);
int cmd_cp(int argc, char* argv[]);
int cmd_mv(int argc, char* argv[]);
int cmd_rm(int argc, char* argv[]);
int cmd_touch(int argc, char* argv[]);
int cmd_stat(int argc, char* argv[]);
int cmd_chmod(int argc, char* argv[]);

/* Command dispatcher */
typedef struct fs_command {
    const char* name;
    int (*handler)(int argc, char* argv[]);
    const char* description;
    const char* usage;
} fs_command_t;

int fs_execute_command(const char* cmdline);
void fs_list_commands(void);
int fs_register_command(const fs_command_t* cmd);

/* ===== File System Utilities ===== */

/* Working directory management */
int fs_init_cwd(void);
void fs_cleanup_cwd(void);

/* Path validation */
bool fs_is_valid_path(const char* path);
bool fs_is_absolute_path(const char* path);
char* fs_normalize_path(const char* path);

/* Permission helpers */
bool fs_can_read(const char* path);
bool fs_can_write(const char* path);
bool fs_can_execute(const char* path);

/* File type helpers */
const char* fs_type_string(vfs_file_type_t type);
char fs_type_char(vfs_file_type_t type);

/* Size formatting */
char* fs_format_size(uint64_t size, char* buffer, size_t bufsize);

/* Time formatting */
char* fs_format_time(uint64_t timestamp, char* buffer, size_t bufsize);

/* ===== Constants ===== */

/* File open flags (matches VFS flags) */
#define FS_O_RDONLY    VFS_O_RDONLY
#define FS_O_WRONLY    VFS_O_WRONLY
#define FS_O_RDWR      VFS_O_RDWR
#define FS_O_CREAT     VFS_O_CREAT
#define FS_O_EXCL      VFS_O_EXCL
#define FS_O_TRUNC     VFS_O_TRUNC
#define FS_O_APPEND    VFS_O_APPEND

/* Seek constants */
#define FS_SEEK_SET    VFS_SEEK_SET
#define FS_SEEK_CUR    VFS_SEEK_CUR
#define FS_SEEK_END    VFS_SEEK_END

/* File permissions */
#define FS_PERM_RUSR   VFS_S_IRUSR
#define FS_PERM_WUSR   VFS_S_IWUSR
#define FS_PERM_XUSR   VFS_S_IXUSR
#define FS_PERM_RGRP   VFS_S_IRGRP
#define FS_PERM_WGRP   VFS_S_IWGRP
#define FS_PERM_XGRP   VFS_S_IXGRP
#define FS_PERM_ROTH   VFS_S_IROTH
#define FS_PERM_WOTH   VFS_S_IWOTH
#define FS_PERM_XOTH   VFS_S_IXOTH

/* Common permission combinations */
#define FS_PERM_644    (FS_PERM_RUSR | FS_PERM_WUSR | FS_PERM_RGRP | FS_PERM_ROTH)
#define FS_PERM_755    (FS_PERM_RUSR | FS_PERM_WUSR | FS_PERM_XUSR | FS_PERM_RGRP | FS_PERM_XGRP | FS_PERM_ROTH | FS_PERM_XOTH)
#define FS_PERM_777    (FS_PERM_RUSR | FS_PERM_WUSR | FS_PERM_XUSR | FS_PERM_RGRP | FS_PERM_WGRP | FS_PERM_XGRP | FS_PERM_ROTH | FS_PERM_WOTH | FS_PERM_XOTH)

#endif /* FS_USER_API_H */
