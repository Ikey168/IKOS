/* IKOS User-Space File System API Implementation
 * Provides user-space interface for file and directory operations
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs_user_api.h"
#include "vfs.h"

/* Global state */
static char current_working_directory[VFS_MAX_PATH_LENGTH] = "/";
static int last_error = VFS_SUCCESS;

/* Helper function prototypes */
static char* safe_strdup(const char* str);
static void safe_free(char* ptr);

/* ===== Directory Operations ===== */

int fs_mkdir(const char* path, uint32_t mode) {
    if (!path) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int result = vfs_mkdir(path, mode);
    last_error = result;
    return result;
}

int fs_rmdir(const char* path) {
    if (!path) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int result = vfs_rmdir(path);
    last_error = result;
    return result;
}

int fs_chdir(const char* path) {
    if (!path) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int result = vfs_chdir(path);
    if (result == VFS_SUCCESS) {
        /* Update our local copy of current directory */
        vfs_getcwd(current_working_directory, sizeof(current_working_directory));
    }
    last_error = result;
    return result;
}

char* fs_getcwd(char* buf, size_t size) {
    if (!buf || size == 0) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return NULL;
    }
    
    return vfs_getcwd(buf, size);
}

/* ===== Directory Listing ===== */

int fs_opendir(const char* path) {
    if (!path) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int result = vfs_opendir(path);
    last_error = (result < 0) ? result : VFS_SUCCESS;
    return result;
}

int fs_readdir(int dirfd, fs_dirent_t* entry) {
    if (dirfd < 0 || !entry) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    vfs_dirent_t vfs_entry;
    int result = vfs_readdir(dirfd, &vfs_entry);
    
    if (result == VFS_SUCCESS) {
        strncpy(entry->name, vfs_entry.d_name, sizeof(entry->name) - 1);
        entry->name[sizeof(entry->name) - 1] = '\0';
        entry->type = vfs_entry.d_type;
        
        /* Get additional metadata */
        vfs_stat_t stat;
        if (vfs_fstat(dirfd, &stat) == VFS_SUCCESS) {
            entry->size = stat.st_size;
            entry->permissions = stat.st_perm;
            entry->mtime = stat.st_mtime;
        } else {
            entry->size = 0;
            entry->permissions = 0;
            entry->mtime = 0;
        }
    }
    
    last_error = result;
    return result;
}

int fs_closedir(int dirfd) {
    if (dirfd < 0) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int result = vfs_closedir(dirfd);
    last_error = result;
    return result;
}

int fs_ls(const char* path, fs_dirent_t* entries, int max_entries) {
    if (!path || !entries || max_entries <= 0) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int dirfd = fs_opendir(path);
    if (dirfd < 0) {
        return -1;
    }
    
    int count = 0;
    while (count < max_entries) {
        if (fs_readdir(dirfd, &entries[count]) != VFS_SUCCESS) {
            break;
        }
        count++;
    }
    
    fs_closedir(dirfd);
    return count;
}

/* ===== File Operations ===== */

int fs_open(const char* path, uint32_t flags, uint32_t mode) {
    if (!path) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int result = vfs_open(path, flags, mode);
    last_error = (result < 0) ? result : VFS_SUCCESS;
    return result;
}

int fs_close(int fd) {
    if (fd < 0) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int result = vfs_close(fd);
    last_error = result;
    return result;
}

ssize_t fs_read(int fd, void* buffer, size_t count) {
    if (fd < 0 || !buffer) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    ssize_t result = vfs_read(fd, buffer, count);
    last_error = (result < 0) ? (int)result : VFS_SUCCESS;
    return result;
}

ssize_t fs_write(int fd, const void* buffer, size_t count) {
    if (fd < 0 || !buffer) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    ssize_t result = vfs_write(fd, buffer, count);
    last_error = (result < 0) ? (int)result : VFS_SUCCESS;
    return result;
}

int fs_unlink(const char* path) {
    if (!path) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int result = vfs_unlink(path);
    last_error = result;
    return result;
}

int fs_rename(const char* oldpath, const char* newpath) {
    if (!oldpath || !newpath) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int result = vfs_rename(oldpath, newpath);
    last_error = result;
    return result;
}

int fs_copy(const char* src, const char* dest) {
    if (!src || !dest) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int src_fd = fs_open(src, FS_O_RDONLY, 0);
    if (src_fd < 0) {
        return -1;
    }
    
    int dest_fd = fs_open(dest, FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, FS_PERM_644);
    if (dest_fd < 0) {
        fs_close(src_fd);
        return -1;
    }
    
    char buffer[4096];
    ssize_t bytes_copied = 0;
    ssize_t bytes_read;
    
    while ((bytes_read = fs_read(src_fd, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_written = fs_write(dest_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            fs_close(src_fd);
            fs_close(dest_fd);
            last_error = VFS_ERROR_IO_ERROR;
            return -1;
        }
        bytes_copied += bytes_written;
    }
    
    fs_close(src_fd);
    fs_close(dest_fd);
    
    if (bytes_read < 0) {
        last_error = VFS_ERROR_IO_ERROR;
        return -1;
    }
    
    return (int)bytes_copied;
}

/* ===== File Positioning ===== */

uint64_t fs_lseek(int fd, uint64_t offset, int whence) {
    if (fd < 0) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return (uint64_t)-1;
    }
    
    uint64_t result = vfs_lseek(fd, offset, whence);
    last_error = VFS_SUCCESS;
    return result;
}

uint64_t fs_tell(int fd) {
    return fs_lseek(fd, 0, FS_SEEK_CUR);
}

/* ===== File Attributes ===== */

int fs_stat(const char* path, vfs_stat_t* stat) {
    if (!path || !stat) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int result = vfs_stat(path, stat);
    last_error = result;
    return result;
}

int fs_fstat(int fd, vfs_stat_t* stat) {
    if (fd < 0 || !stat) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int result = vfs_fstat(fd, stat);
    last_error = result;
    return result;
}

int fs_chmod(const char* path, uint32_t mode) {
    if (!path) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    int result = vfs_chmod(path, mode);
    last_error = result;
    return result;
}

int fs_touch(const char* path) {
    if (!path) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    /* Try to open existing file to update timestamp */
    int fd = fs_open(path, FS_O_WRONLY, 0);
    if (fd >= 0) {
        fs_close(fd);
        return VFS_SUCCESS;
    }
    
    /* Create new file if it doesn't exist */
    fd = fs_open(path, FS_O_WRONLY | FS_O_CREAT, FS_PERM_644);
    if (fd >= 0) {
        fs_close(fd);
        return VFS_SUCCESS;
    }
    
    return -1;
}

/* ===== File Existence and Type Checking ===== */

bool fs_exists(const char* path) {
    if (!path) {
        return false;
    }
    
    vfs_stat_t stat;
    return fs_stat(path, &stat) == VFS_SUCCESS;
}

bool fs_is_file(const char* path) {
    vfs_stat_t stat;
    if (fs_stat(path, &stat) != VFS_SUCCESS) {
        return false;
    }
    return stat.st_mode == VFS_FILE_TYPE_REGULAR;
}

bool fs_is_directory(const char* path) {
    vfs_stat_t stat;
    if (fs_stat(path, &stat) != VFS_SUCCESS) {
        return false;
    }
    return stat.st_mode == VFS_FILE_TYPE_DIRECTORY;
}

uint64_t fs_size(const char* path) {
    vfs_stat_t stat;
    if (fs_stat(path, &stat) != VFS_SUCCESS) {
        return 0;
    }
    return stat.st_size;
}

/* ===== Path Utilities ===== */

char* fs_basename(const char* path) {
    if (!path) return NULL;
    
    const char* last_slash = strrchr(path, '/');
    if (!last_slash) {
        return safe_strdup(path);
    }
    
    return safe_strdup(last_slash + 1);
}

char* fs_dirname(const char* path) {
    if (!path) return NULL;
    
    const char* last_slash = strrchr(path, '/');
    if (!last_slash) {
        return safe_strdup(".");
    }
    
    if (last_slash == path) {
        return safe_strdup("/");
    }
    
    size_t len = last_slash - path;
    char* dir = malloc(len + 1);
    if (dir) {
        strncpy(dir, path, len);
        dir[len] = '\0';
    }
    return dir;
}

char* fs_realpath(const char* path, char* resolved) {
    if (!path) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return NULL;
    }
    
    return vfs_realpath(path, resolved);
}

int fs_split_path(const char* path, char* dir, char* file) {
    if (!path || !dir || !file) {
        last_error = VFS_ERROR_INVALID_PARAM;
        return -1;
    }
    
    char* dirname = fs_dirname(path);
    char* basename = fs_basename(path);
    
    if (!dirname || !basename) {
        safe_free(dirname);
        safe_free(basename);
        last_error = VFS_ERROR_NO_MEMORY;
        return -1;
    }
    
    strcpy(dir, dirname);
    strcpy(file, basename);
    
    safe_free(dirname);
    safe_free(basename);
    
    return VFS_SUCCESS;
}

/* ===== Convenience Functions ===== */

int fs_create_file(const char* path, uint32_t mode) {
    int fd = fs_open(path, FS_O_WRONLY | FS_O_CREAT | FS_O_EXCL, mode);
    if (fd >= 0) {
        fs_close(fd);
        return VFS_SUCCESS;
    }
    return -1;
}

ssize_t fs_read_file(const char* path, void* buffer, size_t size) {
    int fd = fs_open(path, FS_O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }
    
    ssize_t bytes_read = fs_read(fd, buffer, size);
    fs_close(fd);
    return bytes_read;
}

ssize_t fs_write_file(const char* path, const void* buffer, size_t size) {
    int fd = fs_open(path, FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, FS_PERM_644);
    if (fd < 0) {
        return -1;
    }
    
    ssize_t bytes_written = fs_write(fd, buffer, size);
    fs_close(fd);
    return bytes_written;
}

int fs_append_file(const char* path, const void* buffer, size_t size) {
    int fd = fs_open(path, FS_O_WRONLY | FS_O_CREAT | FS_O_APPEND, FS_PERM_644);
    if (fd < 0) {
        return -1;
    }
    
    ssize_t bytes_written = fs_write(fd, buffer, size);
    fs_close(fd);
    return (bytes_written == (ssize_t)size) ? VFS_SUCCESS : -1;
}

/* ===== Error Handling ===== */

int fs_get_last_error(void) {
    return last_error;
}

const char* fs_error_string(int error) {
    switch (error) {
        case VFS_SUCCESS: return "Success";
        case VFS_ERROR_INVALID_PARAM: return "Invalid parameter";
        case VFS_ERROR_NOT_FOUND: return "File or directory not found";
        case VFS_ERROR_PERMISSION: return "Permission denied";
        case VFS_ERROR_EXISTS: return "File already exists";
        case VFS_ERROR_NOT_DIRECTORY: return "Not a directory";
        case VFS_ERROR_IS_DIRECTORY: return "Is a directory";
        case VFS_ERROR_NO_SPACE: return "No space left on device";
        case VFS_ERROR_READ_ONLY: return "Read-only filesystem";
        case VFS_ERROR_NAME_TOO_LONG: return "Filename too long";
        case VFS_ERROR_NO_MEMORY: return "Out of memory";
        case VFS_ERROR_IO_ERROR: return "I/O error";
        case VFS_ERROR_NOT_SUPPORTED: return "Operation not supported";
        case VFS_ERROR_BUSY: return "Device or resource busy";
        case VFS_ERROR_CROSS_DEVICE: return "Cross-device link";
        default: return "Unknown error";
    }
}

/* ===== Helper Functions ===== */

static char* safe_strdup(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* copy = malloc(len + 1);
    if (copy) {
        strcpy(copy, str);
    }
    return copy;
}

static void safe_free(char* ptr) {
    if (ptr) {
        free(ptr);
    }
}

/* ===== Working Directory Management ===== */

int fs_init_cwd(void) {
    strcpy(current_working_directory, "/");
    return VFS_SUCCESS;
}

void fs_cleanup_cwd(void) {
    /* Nothing to clean up currently */
}

/* ===== Path Validation ===== */

bool fs_is_valid_path(const char* path) {
    if (!path || strlen(path) == 0) {
        return false;
    }
    
    if (strlen(path) >= VFS_MAX_PATH_LENGTH) {
        return false;
    }
    
    /* Check for invalid characters (basic validation) */
    for (const char* p = path; *p; p++) {
        if (*p == '\0' || *p == '\n' || *p == '\r') {
            return false;
        }
    }
    
    return true;
}

bool fs_is_absolute_path(const char* path) {
    return path && path[0] == '/';
}

char* fs_normalize_path(const char* path) {
    if (!path) {
        return NULL;
    }
    
    return vfs_normalize_path(path);
}

/* ===== Permission Helpers ===== */

bool fs_can_read(const char* path) {
    vfs_stat_t stat;
    if (fs_stat(path, &stat) != VFS_SUCCESS) {
        return false;
    }
    
    /* Simplified permission check - assumes current user is owner */
    return (stat.st_perm & FS_PERM_RUSR) != 0;
}

bool fs_can_write(const char* path) {
    vfs_stat_t stat;
    if (fs_stat(path, &stat) != VFS_SUCCESS) {
        return false;
    }
    
    /* Simplified permission check - assumes current user is owner */
    return (stat.st_perm & FS_PERM_WUSR) != 0;
}

bool fs_can_execute(const char* path) {
    vfs_stat_t stat;
    if (fs_stat(path, &stat) != VFS_SUCCESS) {
        return false;
    }
    
    /* Simplified permission check - assumes current user is owner */
    return (stat.st_perm & FS_PERM_XUSR) != 0;
}

/* ===== Utility Functions ===== */

const char* fs_type_string(vfs_file_type_t type) {
    switch (type) {
        case VFS_FILE_TYPE_REGULAR: return "regular file";
        case VFS_FILE_TYPE_DIRECTORY: return "directory";
        case VFS_FILE_TYPE_SYMLINK: return "symbolic link";
        case VFS_FILE_TYPE_CHARDEV: return "character device";
        case VFS_FILE_TYPE_BLOCKDEV: return "block device";
        case VFS_FILE_TYPE_FIFO: return "named pipe";
        case VFS_FILE_TYPE_SOCKET: return "socket";
        default: return "unknown";
    }
}

char fs_type_char(vfs_file_type_t type) {
    switch (type) {
        case VFS_FILE_TYPE_REGULAR: return '-';
        case VFS_FILE_TYPE_DIRECTORY: return 'd';
        case VFS_FILE_TYPE_SYMLINK: return 'l';
        case VFS_FILE_TYPE_CHARDEV: return 'c';
        case VFS_FILE_TYPE_BLOCKDEV: return 'b';
        case VFS_FILE_TYPE_FIFO: return 'p';
        case VFS_FILE_TYPE_SOCKET: return 's';
        default: return '?';
    }
}

char* fs_format_size(uint64_t size, char* buffer, size_t bufsize) {
    if (!buffer || bufsize == 0) {
        return NULL;
    }
    
    if (size < 1024) {
        snprintf(buffer, bufsize, "%llu B", (unsigned long long)size);
    } else if (size < 1024 * 1024) {
        snprintf(buffer, bufsize, "%.1f KB", size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        snprintf(buffer, bufsize, "%.1f MB", size / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, bufsize, "%.1f GB", size / (1024.0 * 1024.0 * 1024.0));
    }
    
    return buffer;
}

char* fs_format_time(uint64_t timestamp, char* buffer, size_t bufsize) {
    if (!buffer || bufsize == 0) {
        return NULL;
    }
    
    /* Simple timestamp formatting - in a real system would use proper time formatting */
    snprintf(buffer, bufsize, "%llu", (unsigned long long)timestamp);
    return buffer;
}
