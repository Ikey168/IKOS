/* IKOS Virtual File System (VFS) Implementation
 * Provides a unified interface for different filesystem types
 */

#include "vfs.h"
#include "process.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Function declarations */
static void debug_print(const char* format, ...);

/* Global VFS state */
static bool vfs_initialized = false;
static vfs_mount_t* vfs_root_mount = NULL;
static vfs_dentry_t* vfs_root_dentry = NULL;
static vfs_mount_t* vfs_mounts = NULL;
static vfs_filesystem_t* vfs_filesystems = NULL;

/* File descriptor table */
static vfs_file_t* vfs_fd_table[VFS_MAX_OPEN_FILES];
static bool vfs_fd_used[VFS_MAX_OPEN_FILES];

/* VFS statistics */
static vfs_stats_t vfs_statistics = {0};

/* VFS locks (simple spinlocks for now) */
static volatile int vfs_mount_lock = 0;
static volatile int vfs_fd_lock = 0;
static volatile int vfs_fs_lock = 0;

/* Helper macros */
#define VFS_LOCK(lock) while (__sync_lock_test_and_set(&(lock), 1)) { /* spin */ }
#define VFS_UNLOCK(lock) __sync_lock_release(&(lock))

/* Forward declarations */
static int vfs_create_root_dentry(void);
static vfs_dentry_t* vfs_dentry_cache_lookup(vfs_dentry_t* parent, const char* name);
static int vfs_add_mount(vfs_mount_t* mount);
static void vfs_remove_mount(vfs_mount_t* mount);
static char* vfs_strdup(const char* str);
static void vfs_strfree(char* str);

/* ================================
 * VFS Core Functions
 * ================================ */

/**
 * Initialize the Virtual File System
 */
int vfs_init(void) {
    debug_print("VFS: Initializing Virtual File System...\n");
    
    if (vfs_initialized) {
        debug_print("VFS: Already initialized\n");
        return VFS_SUCCESS;
    }
    
    /* Clear global state */
    vfs_root_mount = NULL;
    vfs_root_dentry = NULL;
    vfs_mounts = NULL;
    vfs_filesystems = NULL;
    
    /* Initialize file descriptor table */
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        vfs_fd_table[i] = NULL;
        vfs_fd_used[i] = false;
    }
    
    /* Initialize statistics */
    memset(&vfs_statistics, 0, sizeof(vfs_statistics));
    
    /* Initialize locks */
    vfs_mount_lock = 0;
    vfs_fd_lock = 0;
    vfs_fs_lock = 0;
    
    /* Create root dentry */
    if (vfs_create_root_dentry() != VFS_SUCCESS) {
        debug_print("VFS: Failed to create root dentry\n");
        return VFS_ERROR_NO_MEMORY;
    }
    
    vfs_initialized = true;
    debug_print("VFS: Virtual File System initialized successfully\n");
    return VFS_SUCCESS;
}

/**
 * Shutdown the Virtual File System
 */
void vfs_shutdown(void) {
    debug_print("VFS: Shutting down Virtual File System...\n");
    
    if (!vfs_initialized) {
        return;
    }
    
    VFS_LOCK(vfs_fd_lock);
    
    /* Close all open files */
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (vfs_fd_used[i] && vfs_fd_table[i]) {
            vfs_close(i);
        }
    }
    
    VFS_UNLOCK(vfs_fd_lock);
    
    /* Unmount all filesystems */
    VFS_LOCK(vfs_mount_lock);
    vfs_mount_t* mount = vfs_mounts;
    while (mount) {
        vfs_mount_t* next = mount->mnt_next;
        vfs_remove_mount(mount);
        mount = next;
    }
    VFS_UNLOCK(vfs_mount_lock);
    
    /* Free root dentry */
    if (vfs_root_dentry) {
        vfs_free_dentry(vfs_root_dentry);
        vfs_root_dentry = NULL;
    }
    
    vfs_initialized = false;
    debug_print("VFS: Virtual File System shutdown complete\n");
}

/* ================================
 * Filesystem Registration
 * ================================ */

/**
 * Register a filesystem type
 */
int vfs_register_filesystem(vfs_filesystem_t* fs) {
    if (!vfs_initialized || !fs || !fs->name) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    VFS_LOCK(vfs_fs_lock);
    
    /* Check if filesystem already registered */
    vfs_filesystem_t* existing = vfs_filesystems;
    while (existing) {
        if (strcmp(existing->name, fs->name) == 0) {
            VFS_UNLOCK(vfs_fs_lock);
            return VFS_ERROR_EXISTS;
        }
        existing = existing->next;
    }
    
    /* Add to filesystem list */
    fs->next = vfs_filesystems;
    vfs_filesystems = fs;
    fs->fs_supers = 0;
    
    VFS_UNLOCK(vfs_fs_lock);
    
    debug_print("VFS: Registered filesystem '%s'\n", fs->name);
    return VFS_SUCCESS;
}

/**
 * Unregister a filesystem type
 */
int vfs_unregister_filesystem(vfs_filesystem_t* fs) {
    if (!vfs_initialized || !fs) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    VFS_LOCK(vfs_fs_lock);
    
    /* Check if filesystem has active superblocks */
    if (fs->fs_supers > 0) {
        VFS_UNLOCK(vfs_fs_lock);
        return VFS_ERROR_BUSY;
    }
    
    /* Remove from filesystem list */
    if (vfs_filesystems == fs) {
        vfs_filesystems = fs->next;
    } else {
        vfs_filesystem_t* prev = vfs_filesystems;
        while (prev && prev->next != fs) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = fs->next;
        }
    }
    
    VFS_UNLOCK(vfs_fs_lock);
    
    debug_print("VFS: Unregistered filesystem '%s'\n", fs->name);
    return VFS_SUCCESS;
}

/* ================================
 * Mount Operations
 * ================================ */

/**
 * Mount a filesystem
 */
int vfs_mount(const char* dev_name, const char* dir_name, 
              const char* fs_type, uint32_t flags, void* data) {
    if (!vfs_initialized || !dev_name || !dir_name || !fs_type) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    debug_print("VFS: Mounting %s on %s (type: %s)\n", dev_name, dir_name, fs_type);
    
    /* Find filesystem type */
    VFS_LOCK(vfs_fs_lock);
    vfs_filesystem_t* fs = vfs_filesystems;
    while (fs && strcmp(fs->name, fs_type) != 0) {
        fs = fs->next;
    }
    VFS_UNLOCK(vfs_fs_lock);
    
    if (!fs) {
        debug_print("VFS: Filesystem type '%s' not found\n", fs_type);
        return VFS_ERROR_NOT_SUPPORTED;
    }
    
    /* Create superblock */
    vfs_superblock_t* sb = fs->mount(fs, flags, dev_name, data);
    if (!sb) {
        debug_print("VFS: Failed to create superblock\n");
        return VFS_ERROR_IO_ERROR;
    }
    
    /* Create mount structure */
    vfs_mount_t* mount = (vfs_mount_t*)kmalloc(sizeof(vfs_mount_t));
    if (!mount) {
        if (fs->kill_sb) {
            fs->kill_sb(sb);
        }
        return VFS_ERROR_NO_MEMORY;
    }
    
    /* Initialize mount structure */
    memset(mount, 0, sizeof(vfs_mount_t));
    mount->mnt_sb = sb;
    mount->mnt_root = sb->s_root;
    mount->mnt_flags = flags;
    mount->mnt_count = 1;
    strncpy(mount->mnt_devname, dev_name, sizeof(mount->mnt_devname) - 1);
    strncpy(mount->mnt_dirname, dir_name, sizeof(mount->mnt_dirname) - 1);
    
    /* Handle root mount specially */
    if (strcmp(dir_name, "/") == 0 && !vfs_root_mount) {
        vfs_root_mount = mount;
        mount->mnt_mountpoint = vfs_root_dentry;
        mount->mnt_parent = NULL;
        vfs_root_dentry->d_mounted = mount;
    } else {
        /* Find mount point */
        vfs_dentry_t* mountpoint = vfs_path_lookup(dir_name, 0);
        if (!mountpoint) {
            kfree(mount);
            if (fs->kill_sb) {
                fs->kill_sb(sb);
            }
            return VFS_ERROR_NOT_FOUND;
        }
        
        mount->mnt_mountpoint = mountpoint;
        mount->mnt_parent = vfs_get_mount(dir_name);
        mountpoint->d_mounted = mount;
    }
    
    /* Add to mount list */
    if (vfs_add_mount(mount) != VFS_SUCCESS) {
        kfree(mount);
        if (fs->kill_sb) {
            fs->kill_sb(sb);
        }
        return VFS_ERROR_NO_MEMORY;
    }
    
    vfs_statistics.mounted_filesystems++;
    debug_print("VFS: Successfully mounted %s on %s\n", dev_name, dir_name);
    return VFS_SUCCESS;
}

/**
 * Unmount a filesystem
 */
int vfs_umount(const char* dir_name) {
    if (!vfs_initialized || !dir_name) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    debug_print("VFS: Unmounting %s\n", dir_name);
    
    /* Find mount point */
    vfs_mount_t* mount = vfs_get_mount(dir_name);
    if (!mount) {
        return VFS_ERROR_NOT_FOUND;
    }
    
    /* Check if busy */
    if (mount->mnt_count > 1) {
        return VFS_ERROR_BUSY;
    }
    
    /* Remove from mount list */
    vfs_remove_mount(mount);
    
    /* Clean up mount point */
    if (mount->mnt_mountpoint) {
        mount->mnt_mountpoint->d_mounted = NULL;
    }
    
    /* Kill superblock */
    if (mount->mnt_sb && mount->mnt_sb->s_type && mount->mnt_sb->s_type->kill_sb) {
        mount->mnt_sb->s_type->kill_sb(mount->mnt_sb);
    }
    
    /* Free mount structure */
    kfree(mount);
    
    vfs_statistics.mounted_filesystems--;
    debug_print("VFS: Successfully unmounted %s\n", dir_name);
    return VFS_SUCCESS;
}

/**
 * Get mount for a given path
 */
vfs_mount_t* vfs_get_mount(const char* path) {
    if (!vfs_initialized || !path) {
        return NULL;
    }
    
    VFS_LOCK(vfs_mount_lock);
    
    vfs_mount_t* best_match = NULL;
    size_t best_len = 0;
    
    vfs_mount_t* mount = vfs_mounts;
    while (mount) {
        size_t len = strlen(mount->mnt_dirname);
        if (strncmp(path, mount->mnt_dirname, len) == 0 && len > best_len) {
            best_match = mount;
            best_len = len;
        }
        mount = mount->mnt_next;
    }
    
    VFS_UNLOCK(vfs_mount_lock);
    
    return best_match ? best_match : vfs_root_mount;
}

/* ================================
 * File Operations
 * ================================ */

/**
 * Open a file
 */
int vfs_open(const char* path, uint32_t flags, uint32_t mode) {
    if (!vfs_initialized || !path) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    debug_print("VFS: Opening file '%s' with flags 0x%x\n", path, flags);
    
    /* Allocate file descriptor */
    int fd = vfs_alloc_fd();
    if (fd < 0) {
        return VFS_ERROR_NO_MEMORY;
    }
    
    /* Look up path */
    vfs_dentry_t* dentry = vfs_path_lookup(path, flags);
    if (!dentry) {
        /* Create new file if O_CREAT flag is set */
        if (flags & VFS_O_CREAT) {
            /* TODO: Implement file creation */
            vfs_free_fd(fd);
            return VFS_ERROR_NOT_SUPPORTED;
        }
        vfs_free_fd(fd);
        return VFS_ERROR_NOT_FOUND;
    }
    
    /* Check if it's a directory and O_DIRECTORY is required */
    if ((flags & VFS_O_DIRECTORY) && dentry->d_inode->i_mode != VFS_FILE_TYPE_DIRECTORY) {
        vfs_free_fd(fd);
        return VFS_ERROR_NOT_DIRECTORY;
    }
    
    /* Allocate file structure */
    vfs_file_t* file = (vfs_file_t*)kmalloc(sizeof(vfs_file_t));
    if (!file) {
        vfs_free_fd(fd);
        return VFS_ERROR_NO_MEMORY;
    }
    
    /* Initialize file structure */
    memset(file, 0, sizeof(vfs_file_t));
    file->f_dentry = dentry;
    file->f_inode = dentry->d_inode;
    file->f_op = dentry->d_inode->i_fop;
    file->f_flags = flags;
    file->f_mode = mode;
    file->f_pos = 0;
    file->f_count = 1;
    
    /* Call filesystem-specific open */
    if (file->f_op && file->f_op->open) {
        int result = file->f_op->open(file->f_inode, file);
        if (result != VFS_SUCCESS) {
            kfree(file);
            vfs_free_fd(fd);
            return result;
        }
    }
    
    /* Install file in descriptor table */
    vfs_install_fd(fd, file);
    
    vfs_statistics.open_files++;
    vfs_statistics.total_reads = 0;  /* Reset for this file */
    vfs_statistics.total_writes = 0;
    
    debug_print("VFS: Opened file '%s' with fd %d\n", path, fd);
    return fd;
}

/**
 * Close a file
 */
int vfs_close(int fd) {
    if (!vfs_initialized || fd < 0 || fd >= VFS_MAX_OPEN_FILES) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    VFS_LOCK(vfs_fd_lock);
    
    if (!vfs_fd_used[fd] || !vfs_fd_table[fd]) {
        VFS_UNLOCK(vfs_fd_lock);
        return VFS_ERROR_INVALID_PARAM;
    }
    
    vfs_file_t* file = vfs_fd_table[fd];
    
    /* Call filesystem-specific release */
    if (file->f_op && file->f_op->release) {
        file->f_op->release(file->f_inode, file);
    }
    
    /* Free file structure */
    kfree(file);
    
    /* Clear descriptor table entry */
    vfs_fd_table[fd] = NULL;
    vfs_fd_used[fd] = false;
    
    VFS_UNLOCK(vfs_fd_lock);
    
    vfs_statistics.open_files--;
    debug_print("VFS: Closed file descriptor %d\n", fd);
    return VFS_SUCCESS;
}

/**
 * Read from a file
 */
ssize_t vfs_read(int fd, void* buffer, size_t count) {
    if (!vfs_initialized || fd < 0 || fd >= VFS_MAX_OPEN_FILES || !buffer) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    vfs_file_t* file = vfs_get_file(fd);
    if (!file) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    /* Check read permission */
    if (!(file->f_flags & (VFS_O_RDONLY | VFS_O_RDWR))) {
        return VFS_ERROR_PERMISSION;
    }
    
    /* Call filesystem-specific read */
    if (!file->f_op || !file->f_op->read) {
        return VFS_ERROR_NOT_SUPPORTED;
    }
    
    ssize_t result = file->f_op->read(file, (char*)buffer, count, &file->f_pos);
    if (result > 0) {
        vfs_statistics.total_reads++;
        vfs_statistics.bytes_read += result;
    }
    
    return result;
}

/**
 * Write to a file
 */
ssize_t vfs_write(int fd, const void* buffer, size_t count) {
    if (!vfs_initialized || fd < 0 || fd >= VFS_MAX_OPEN_FILES || !buffer) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    vfs_file_t* file = vfs_get_file(fd);
    if (!file) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    /* Check write permission */
    if (!(file->f_flags & (VFS_O_WRONLY | VFS_O_RDWR))) {
        return VFS_ERROR_PERMISSION;
    }
    
    /* Call filesystem-specific write */
    if (!file->f_op || !file->f_op->write) {
        return VFS_ERROR_NOT_SUPPORTED;
    }
    
    ssize_t result = file->f_op->write(file, (const char*)buffer, count, &file->f_pos);
    if (result > 0) {
        vfs_statistics.total_writes++;
        vfs_statistics.bytes_written += result;
    }
    
    return result;
}

/* ================================
 * Helper Functions
 * ================================ */

/**
 * Create root dentry
 */
static int vfs_create_root_dentry(void) {
    vfs_root_dentry = vfs_alloc_dentry("/");
    if (!vfs_root_dentry) {
        return VFS_ERROR_NO_MEMORY;
    }
    
    vfs_root_dentry->d_parent = vfs_root_dentry; /* Root is its own parent */
    return VFS_SUCCESS;
}

/**
 * Add mount to mount list
 */
static int vfs_add_mount(vfs_mount_t* mount) {
    VFS_LOCK(vfs_mount_lock);
    mount->mnt_next = vfs_mounts;
    vfs_mounts = mount;
    VFS_UNLOCK(vfs_mount_lock);
    return VFS_SUCCESS;
}

/**
 * Remove mount from mount list
 */
static void vfs_remove_mount(vfs_mount_t* mount) {
    VFS_LOCK(vfs_mount_lock);
    
    if (vfs_mounts == mount) {
        vfs_mounts = mount->mnt_next;
    } else {
        vfs_mount_t* prev = vfs_mounts;
        while (prev && prev->mnt_next != mount) {
            prev = prev->mnt_next;
        }
        if (prev) {
            prev->mnt_next = mount->mnt_next;
        }
    }
    
    VFS_UNLOCK(vfs_mount_lock);
}

/**
 * Get statistics
 */
void vfs_get_stats(vfs_stats_t* stats) {
    if (!vfs_initialized || !stats) {
        return;
    }
    
    *stats = vfs_statistics;
}

/**
 * Simple debug print function
 */
static void debug_print(const char* format, ...) {
    /* In a real kernel, this would output to the console/log */
    /* For now, it's a placeholder */
    (void)format;
}

/* ================================
 * Path Resolution Functions
 * ================================ */

/**
 * Lookup a path and return the dentry
 */
vfs_dentry_t* vfs_path_lookup(const char* path, uint32_t flags) {
    if (!vfs_initialized || !path) {
        return NULL;
    }
    
    debug_print("VFS: Looking up path '%s'\n", path);
    
    /* Start from root */
    vfs_dentry_t* current = vfs_root_dentry;
    if (!current) {
        return NULL;
    }
    
    /* Handle absolute path */
    if (path[0] == '/') {
        path++; /* Skip leading slash */
    }
    
    /* If path is just "/", return root */
    if (path[0] == '\0') {
        return current;
    }
    
    /* Parse path components */
    char path_copy[VFS_MAX_PATH];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    char* token = strtok(path_copy, "/");
    while (token) {
        /* Look for child dentry */
        vfs_dentry_t* child = vfs_dentry_cache_lookup(current, token);
        if (!child) {
            /* Try to create dentry via filesystem */
            if (current->d_inode && current->d_inode->i_op && current->d_inode->i_op->lookup) {
                child = current->d_inode->i_op->lookup(current->d_inode, token);
            }
            
            if (!child) {
                debug_print("VFS: Path component '%s' not found\n", token);
                return NULL;
            }
        }
        
        current = child;
        token = strtok(NULL, "/");
    }
    
    return current;
}

/* ================================
 * Dentry Management Functions
 * ================================ */

/**
 * Allocate a new dentry
 */
vfs_dentry_t* vfs_alloc_dentry(const char* name) {
    vfs_dentry_t* dentry = (vfs_dentry_t*)kmalloc(sizeof(vfs_dentry_t));
    if (!dentry) {
        return NULL;
    }
    
    memset(dentry, 0, sizeof(vfs_dentry_t));
    
    if (name) {
        strncpy(dentry->d_name, name, sizeof(dentry->d_name) - 1);
        dentry->d_name[sizeof(dentry->d_name) - 1] = '\0';
    }
    
    dentry->d_count = 1;
    dentry->d_flags = 0;
    dentry->d_inode = NULL;
    dentry->d_parent = NULL;
    dentry->d_mounted = NULL;
    dentry->d_subdirs = NULL;
    dentry->d_next = NULL;
    
    return dentry;
}

/**
 * Free a dentry
 */
void vfs_free_dentry(vfs_dentry_t* dentry) {
    if (!dentry) {
        return;
    }
    
    /* Free child dentries */
    vfs_dentry_t* child = dentry->d_subdirs;
    while (child) {
        vfs_dentry_t* next = child->d_next;
        vfs_free_dentry(child);
        child = next;
    }
    
    /* Free the dentry itself */
    kfree(dentry);
}

/**
 * Add a child dentry
 */
void vfs_dentry_add_child(vfs_dentry_t* parent, vfs_dentry_t* child) {
    if (!parent || !child) {
        return;
    }
    
    child->d_parent = parent;
    child->d_next = parent->d_subdirs;
    parent->d_subdirs = child;
}

/**
 * Simple dentry cache lookup
 */
static vfs_dentry_t* vfs_dentry_cache_lookup(vfs_dentry_t* parent, const char* name) {
    if (!parent || !name) {
        return NULL;
    }
    
    vfs_dentry_t* child = parent->d_subdirs;
    while (child) {
        if (strcmp(child->d_name, name) == 0) {
            return child;
        }
        child = child->d_next;
    }
    
    return NULL;
}

/* ================================
 * Inode Management Functions
 * ================================ */

/**
 * Allocate a new inode
 */
vfs_inode_t* vfs_alloc_inode(vfs_superblock_t* sb) {
    if (!sb) {
        return NULL;
    }
    
    vfs_inode_t* inode = (vfs_inode_t*)kmalloc(sizeof(vfs_inode_t));
    if (!inode) {
        return NULL;
    }
    
    memset(inode, 0, sizeof(vfs_inode_t));
    
    inode->i_sb = sb;
    inode->i_count = 1;
    inode->i_nlink = 1;
    inode->i_uid = 0;
    inode->i_gid = 0;
    inode->i_size = 0;
    inode->i_mode = VFS_FILE_TYPE_REGULAR;
    inode->i_op = NULL;
    inode->i_fop = NULL;
    inode->i_private = NULL;
    
    /* Set timestamps to current time (placeholder) */
    inode->i_atime = 0;
    inode->i_mtime = 0;
    inode->i_ctime = 0;
    
    return inode;
}

/**
 * Free an inode
 */
void vfs_free_inode(vfs_inode_t* inode) {
    if (!inode) {
        return;
    }
    
    /* Call filesystem-specific cleanup */
    if (inode->i_sb && inode->i_sb->s_op && inode->i_sb->s_op->destroy_inode) {
        inode->i_sb->s_op->destroy_inode(inode);
    } else {
        kfree(inode);
    }
}

/* ================================
 * File Descriptor Management
 * ================================ */

/**
 * Allocate a file descriptor
 */
int vfs_alloc_fd(void) {
    VFS_LOCK(vfs_fd_lock);
    
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (!vfs_fd_used[i]) {
            vfs_fd_used[i] = true;
            vfs_fd_table[i] = NULL;
            VFS_UNLOCK(vfs_fd_lock);
            return i;
        }
    }
    
    VFS_UNLOCK(vfs_fd_lock);
    return -1; /* No free descriptors */
}

/**
 * Free a file descriptor
 */
void vfs_free_fd(int fd) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES) {
        return;
    }
    
    VFS_LOCK(vfs_fd_lock);
    vfs_fd_used[fd] = false;
    vfs_fd_table[fd] = NULL;
    VFS_UNLOCK(vfs_fd_lock);
}

/**
 * Install a file in the descriptor table
 */
void vfs_install_fd(int fd, vfs_file_t* file) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !file) {
        return;
    }
    
    VFS_LOCK(vfs_fd_lock);
    vfs_fd_table[fd] = file;
    VFS_UNLOCK(vfs_fd_lock);
}

/**
 * Get file structure from descriptor
 */
vfs_file_t* vfs_get_file(int fd) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES) {
        return NULL;
    }
    
    VFS_LOCK(vfs_fd_lock);
    vfs_file_t* file = vfs_fd_used[fd] ? vfs_fd_table[fd] : NULL;
    VFS_UNLOCK(vfs_fd_lock);
    
    return file;
}

/* ================================
 * String Utility Functions
 * ================================ */

static char* vfs_strdup(const char* str) {
    if (!str) {
        return NULL;
    }
    
    size_t len = strlen(str);
    char* copy = (char*)kmalloc(len + 1);
    if (!copy) {
        return NULL;
    }
    
    strcpy(copy, str);
    return copy;
}

static void vfs_strfree(char* str) {
    if (str) {
        kfree(str);
    }
}

/* Simple strtok implementation */
char* strtok(char* str, const char* delim) {
    static char* last = NULL;
    
    if (str) {
        last = str;
    }
    
    if (!last || !*last) {
        return NULL;
    }
    
    /* Skip leading delimiters */
    while (*last && strchr(delim, *last)) {
        last++;
    }
    
    if (!*last) {
        return NULL;
    }
    
    char* start = last;
    
    /* Find next delimiter */
    while (*last && !strchr(delim, *last)) {
        last++;
    }
    
    if (*last) {
        *last++ = '\0';
    }
    
    return start;
}

/* Simple strchr implementation */
char* strchr(const char* str, int c) {
    while (*str) {
        if (*str == c) {
            return (char*)str;
        }
        str++;
    }
    return (c == '\0') ? (char*)str : NULL;
}

/* Temporary stubs for memory allocation */
void* kmalloc(size_t size) {
    static char fake_heap[64 * 1024];
    static size_t offset = 0;
    
    if (offset + size > sizeof(fake_heap)) {
        return NULL;
    }
    
    void* ptr = &fake_heap[offset];
    offset += size;
    return ptr;
}

void kfree(void* ptr) {
    (void)ptr;
}
