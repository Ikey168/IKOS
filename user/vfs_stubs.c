/* VFS Stub Functions for User-Space Testing
 * Provides stub implementations of VFS functions for standalone testing
 */

#include "vfs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

/* Simple mapping to host filesystem for testing */

int vfs_mkdir(const char* path, uint32_t mode) {
    return mkdir(path, mode);
}

int vfs_rmdir(const char* path) {
    return rmdir(path);
}

int vfs_chdir(const char* path) {
    return chdir(path);
}

char* vfs_getcwd(char* buf, size_t size) {
    return getcwd(buf, size);
}

int vfs_open(const char* path, uint32_t flags, uint32_t mode) {
    int host_flags = 0;
    
    if (flags & VFS_O_RDONLY) host_flags |= O_RDONLY;
    if (flags & VFS_O_WRONLY) host_flags |= O_WRONLY;
    if (flags & VFS_O_RDWR) host_flags |= O_RDWR;
    if (flags & VFS_O_CREAT) host_flags |= O_CREAT;
    if (flags & VFS_O_TRUNC) host_flags |= O_TRUNC;
    if (flags & VFS_O_APPEND) host_flags |= O_APPEND;
    
    return open(path, host_flags, mode);
}

int vfs_close(int fd) {
    return close(fd);
}

ssize_t vfs_read(int fd, void* buffer, size_t count) {
    return read(fd, buffer, count);
}

ssize_t vfs_write(int fd, const void* buffer, size_t count) {
    return write(fd, buffer, count);
}

uint64_t vfs_lseek(int fd, uint64_t offset, int whence) {
    return lseek(fd, offset, whence);
}

int vfs_unlink(const char* path) {
    return unlink(path);
}

int vfs_rename(const char* oldpath, const char* newpath) {
    return rename(oldpath, newpath);
}

int vfs_stat(const char* path, vfs_stat_t* stat) {
    struct stat host_stat;
    if (stat(path, &host_stat) != 0) {
        return -errno;
    }
    
    stat->st_size = host_stat.st_size;
    stat->st_mtime = host_stat.st_mtime;
    stat->st_perm = host_stat.st_mode & 0777;
    
    if (S_ISREG(host_stat.st_mode)) {
        stat->st_mode = VFS_FILE_TYPE_REGULAR;
    } else if (S_ISDIR(host_stat.st_mode)) {
        stat->st_mode = VFS_FILE_TYPE_DIRECTORY;
    } else {
        stat->st_mode = VFS_FILE_TYPE_REGULAR;
    }
    
    return 0;
}

int vfs_fstat(int fd, vfs_stat_t* stat) {
    struct stat host_stat;
    if (fstat(fd, &host_stat) != 0) {
        return -errno;
    }
    
    stat->st_size = host_stat.st_size;
    stat->st_mtime = host_stat.st_mtime;
    stat->st_perm = host_stat.st_mode & 0777;
    
    if (S_ISREG(host_stat.st_mode)) {
        stat->st_mode = VFS_FILE_TYPE_REGULAR;
    } else if (S_ISDIR(host_stat.st_mode)) {
        stat->st_mode = VFS_FILE_TYPE_DIRECTORY;
    } else {
        stat->st_mode = VFS_FILE_TYPE_REGULAR;
    }
    
    return 0;
}

int vfs_chmod(const char* path, uint32_t mode) {
    return chmod(path, mode);
}

int vfs_opendir(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) {
        return -errno;
    }
    return (int)(intptr_t)dir;  // Cast DIR* to int for compatibility
}

int vfs_readdir(int dirfd, vfs_dirent_t* entry) {
    DIR* dir = (DIR*)(intptr_t)dirfd;
    struct dirent* host_entry = readdir(dir);
    
    if (!host_entry) {
        return -1;  // End of directory
    }
    
    strncpy(entry->d_name, host_entry->d_name, sizeof(entry->d_name) - 1);
    entry->d_name[sizeof(entry->d_name) - 1] = '\0';
    
    switch (host_entry->d_type) {
        case DT_REG:
            entry->d_type = VFS_FILE_TYPE_REGULAR;
            break;
        case DT_DIR:
            entry->d_type = VFS_FILE_TYPE_DIRECTORY;
            break;
        case DT_LNK:
            entry->d_type = VFS_FILE_TYPE_SYMLINK;
            break;
        default:
            entry->d_type = VFS_FILE_TYPE_REGULAR;
            break;
    }
    
    return 0;
}

int vfs_closedir(int dirfd) {
    DIR* dir = (DIR*)(intptr_t)dirfd;
    return closedir(dir);
}

char* vfs_realpath(const char* path, char* resolved) {
    return realpath(path, resolved);
}

char* vfs_normalize_path(const char* path) {
    return strdup(path);  // Simple implementation
}
