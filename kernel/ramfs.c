/* IKOS RAM Filesystem Implementation
 * A simple in-memory filesystem for demonstration
 */

#include "vfs.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* RAM filesystem constants */
#define RAMFS_MAGIC 0x858458f6
#define RAMFS_MAX_FILES 256
#define RAMFS_MAX_FILE_SIZE (64 * 1024)  /* 64KB max file size */

/* RAM filesystem structures */
typedef struct ramfs_inode_info {
    uint8_t* data;
    size_t size;
    size_t capacity;
    bool is_directory;
} ramfs_inode_info_t;

typedef struct ramfs_sb_info {
    uint32_t magic;
    uint32_t file_count;
    uint32_t max_files;
    vfs_inode_t* inodes[RAMFS_MAX_FILES];
} ramfs_sb_info_t;

/* Forward declarations */
static vfs_superblock_t* ramfs_mount(vfs_filesystem_t* fs, uint32_t flags, 
                                     const char* dev_name, void* data);
static void ramfs_kill_sb(vfs_superblock_t* sb);
static vfs_inode_t* ramfs_alloc_inode(vfs_superblock_t* sb);
static void ramfs_destroy_inode(vfs_inode_t* inode);
static vfs_dentry_t* ramfs_lookup(vfs_inode_t* dir, const char* name);
static int ramfs_create(vfs_inode_t* dir, const char* name, uint32_t mode);
static int ramfs_mkdir(vfs_inode_t* dir, const char* name, uint32_t mode);
static int ramfs_open(vfs_inode_t* inode, vfs_file_t* file);
static int ramfs_release(vfs_inode_t* inode, vfs_file_t* file);
static ssize_t ramfs_read(vfs_file_t* file, char* buffer, size_t count, loff_t* pos);
static ssize_t ramfs_write(vfs_file_t* file, const char* buffer, size_t count, loff_t* pos);

/* Function declarations for memory allocation */
extern void* kmalloc(size_t size);
extern void kfree(void* ptr);

/* RAM filesystem operations */
static vfs_super_operations_t ramfs_super_ops = {
    .alloc_inode = ramfs_alloc_inode,
    .destroy_inode = ramfs_destroy_inode,
    .statfs = NULL,  /* Not implemented */
    .remount_fs = NULL,  /* Not implemented */
};

static vfs_inode_operations_t ramfs_dir_inode_ops = {
    .lookup = ramfs_lookup,
    .create = ramfs_create,
    .mkdir = ramfs_mkdir,
    .rmdir = NULL,  /* Not implemented */
    .rename = NULL,  /* Not implemented */
    .getattr = NULL,  /* Not implemented */
    .setattr = NULL,  /* Not implemented */
};

static vfs_file_operations_t ramfs_file_ops = {
    .open = ramfs_open,
    .release = ramfs_release,
    .read = ramfs_read,
    .write = ramfs_write,
    .llseek = NULL,  /* Not implemented */
    .ioctl = NULL,  /* Not implemented */
    .mmap = NULL,  /* Not implemented */
};

/* RAM filesystem type */
static vfs_filesystem_t ramfs_fs_type = {
    .name = "ramfs",
    .mount = ramfs_mount,
    .kill_sb = ramfs_kill_sb,
    .fs_supers = 0,
    .next = NULL,
};

/* ================================
 * Superblock Operations
 * ================================ */

/**
 * Mount a RAM filesystem
 */
static vfs_superblock_t* ramfs_mount(vfs_filesystem_t* fs, uint32_t flags,
                                     const char* dev_name, void* data) {
    (void)flags;
    (void)dev_name;
    (void)data;
    
    /* Allocate superblock */
    vfs_superblock_t* sb = (vfs_superblock_t*)kmalloc(sizeof(vfs_superblock_t));
    if (!sb) {
        return NULL;
    }
    
    /* Allocate superblock private data */
    ramfs_sb_info_t* sbi = (ramfs_sb_info_t*)kmalloc(sizeof(ramfs_sb_info_t));
    if (!sbi) {
        kfree(sb);
        return NULL;
    }
    
    /* Initialize superblock */
    memset(sb, 0, sizeof(vfs_superblock_t));
    sb->s_magic = RAMFS_MAGIC;
    sb->s_type = fs;
    sb->s_op = &ramfs_super_ops;
    sb->s_private = sbi;
    
    /* Initialize superblock private data */
    memset(sbi, 0, sizeof(ramfs_sb_info_t));
    sbi->magic = RAMFS_MAGIC;
    sbi->file_count = 0;
    sbi->max_files = RAMFS_MAX_FILES;
    
    /* Create root inode */
    vfs_inode_t* root_inode = ramfs_alloc_inode(sb);
    if (!root_inode) {
        kfree(sbi);
        kfree(sb);
        return NULL;
    }
    
    /* Set root inode as directory */
    root_inode->i_mode = VFS_FILE_TYPE_DIRECTORY;
    root_inode->i_op = &ramfs_dir_inode_ops;
    root_inode->i_fop = NULL;  /* Directories don't have file operations */
    
    /* Mark as directory in private data */
    ramfs_inode_info_t* root_info = (ramfs_inode_info_t*)root_inode->i_private;
    root_info->is_directory = true;
    
    /* Create root dentry */
    vfs_dentry_t* root_dentry = vfs_alloc_dentry("/");
    if (!root_dentry) {
        ramfs_destroy_inode(root_inode);
        kfree(sbi);
        kfree(sb);
        return NULL;
    }
    
    root_dentry->d_inode = root_inode;
    root_dentry->d_sb = sb;
    sb->s_root = root_dentry;
    
    fs->fs_supers++;
    return sb;
}

/**
 * Kill a RAM filesystem superblock
 */
static void ramfs_kill_sb(vfs_superblock_t* sb) {
    if (!sb) {
        return;
    }
    
    ramfs_sb_info_t* sbi = (ramfs_sb_info_t*)sb->s_private;
    if (sbi) {
        /* Free all inodes */
        for (uint32_t i = 0; i < sbi->max_files; i++) {
            if (sbi->inodes[i]) {
                ramfs_destroy_inode(sbi->inodes[i]);
            }
        }
        kfree(sbi);
    }
    
    /* Free root dentry */
    if (sb->s_root) {
        vfs_free_dentry(sb->s_root);
    }
    
    if (sb->s_type) {
        sb->s_type->fs_supers--;
    }
    
    kfree(sb);
}

/* ================================
 * Inode Operations
 * ================================ */

/**
 * Allocate a RAM filesystem inode
 */
static vfs_inode_t* ramfs_alloc_inode(vfs_superblock_t* sb) {
    if (!sb) {
        return NULL;
    }
    
    ramfs_sb_info_t* sbi = (ramfs_sb_info_t*)sb->s_private;
    if (!sbi || sbi->file_count >= sbi->max_files) {
        return NULL;
    }
    
    /* Allocate inode */
    vfs_inode_t* inode = vfs_alloc_inode(sb);
    if (!inode) {
        return NULL;
    }
    
    /* Allocate RAM filesystem specific data */
    ramfs_inode_info_t* info = (ramfs_inode_info_t*)kmalloc(sizeof(ramfs_inode_info_t));
    if (!info) {
        vfs_free_inode(inode);
        return NULL;
    }
    
    /* Initialize private data */
    memset(info, 0, sizeof(ramfs_inode_info_t));
    info->data = NULL;
    info->size = 0;
    info->capacity = 0;
    info->is_directory = false;
    
    inode->i_private = info;
    inode->i_ino = sbi->file_count++;  /* Simple inode numbering */
    
    /* Find free slot in inode table */
    for (uint32_t i = 0; i < sbi->max_files; i++) {
        if (!sbi->inodes[i]) {
            sbi->inodes[i] = inode;
            break;
        }
    }
    
    return inode;
}

/**
 * Destroy a RAM filesystem inode
 */
static void ramfs_destroy_inode(vfs_inode_t* inode) {
    if (!inode) {
        return;
    }
    
    ramfs_inode_info_t* info = (ramfs_inode_info_t*)inode->i_private;
    if (info) {
        /* Free data buffer */
        if (info->data) {
            kfree(info->data);
        }
        kfree(info);
    }
    
    /* Remove from superblock inode table */
    if (inode->i_sb && inode->i_sb->s_private) {
        ramfs_sb_info_t* sbi = (ramfs_sb_info_t*)inode->i_sb->s_private;
        for (uint32_t i = 0; i < sbi->max_files; i++) {
            if (sbi->inodes[i] == inode) {
                sbi->inodes[i] = NULL;
                break;
            }
        }
    }
    
    vfs_free_inode(inode);
}

/**
 * Lookup a file in a RAM filesystem directory
 */
static vfs_dentry_t* ramfs_lookup(vfs_inode_t* dir, const char* name) {
    if (!dir || !name) {
        return NULL;
    }
    
    ramfs_inode_info_t* dir_info = (ramfs_inode_info_t*)dir->i_private;
    if (!dir_info || !dir_info->is_directory) {
        return NULL;
    }
    
    /* For simplicity, we'll create a new file if it doesn't exist */
    /* In a real implementation, you'd search existing files */
    
    /* Create new dentry */
    vfs_dentry_t* dentry = vfs_alloc_dentry(name);
    if (!dentry) {
        return NULL;
    }
    
    /* For now, return empty dentry - file creation happens in create() */
    return dentry;
}

/**
 * Create a file in a RAM filesystem directory
 */
static int ramfs_create(vfs_inode_t* dir, const char* name, uint32_t mode) {
    if (!dir || !name) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    ramfs_inode_info_t* dir_info = (ramfs_inode_info_t*)dir->i_private;
    if (!dir_info || !dir_info->is_directory) {
        return VFS_ERROR_NOT_DIRECTORY;
    }
    
    /* Create new inode */
    vfs_inode_t* inode = ramfs_alloc_inode(dir->i_sb);
    if (!inode) {
        return VFS_ERROR_NO_MEMORY;
    }
    
    /* Set as regular file */
    inode->i_mode = VFS_FILE_TYPE_REGULAR;
    inode->i_op = NULL;  /* Regular files don't have inode operations */
    inode->i_fop = &ramfs_file_ops;
    
    return VFS_SUCCESS;
}

/**
 * Create a directory in a RAM filesystem
 */
static int ramfs_mkdir(vfs_inode_t* dir, const char* name, uint32_t mode) {
    if (!dir || !name) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    ramfs_inode_info_t* dir_info = (ramfs_inode_info_t*)dir->i_private;
    if (!dir_info || !dir_info->is_directory) {
        return VFS_ERROR_NOT_DIRECTORY;
    }
    
    /* Create new inode */
    vfs_inode_t* inode = ramfs_alloc_inode(dir->i_sb);
    if (!inode) {
        return VFS_ERROR_NO_MEMORY;
    }
    
    /* Set as directory */
    inode->i_mode = VFS_FILE_TYPE_DIRECTORY;
    inode->i_op = &ramfs_dir_inode_ops;
    inode->i_fop = NULL;  /* Directories don't have file operations */
    
    /* Mark as directory in private data */
    ramfs_inode_info_t* info = (ramfs_inode_info_t*)inode->i_private;
    info->is_directory = true;
    
    return VFS_SUCCESS;
}

/* ================================
 * File Operations
 * ================================ */

/**
 * Open a RAM filesystem file
 */
static int ramfs_open(vfs_inode_t* inode, vfs_file_t* file) {
    if (!inode || !file) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    ramfs_inode_info_t* info = (ramfs_inode_info_t*)inode->i_private;
    if (!info || info->is_directory) {
        return VFS_ERROR_IS_DIRECTORY;
    }
    
    /* File is ready to use */
    return VFS_SUCCESS;
}

/**
 * Release a RAM filesystem file
 */
static int ramfs_release(vfs_inode_t* inode, vfs_file_t* file) {
    (void)inode;
    (void)file;
    
    /* Nothing special to do for RAM filesystem */
    return VFS_SUCCESS;
}

/**
 * Read from a RAM filesystem file
 */
static ssize_t ramfs_read(vfs_file_t* file, char* buffer, size_t count, loff_t* pos) {
    if (!file || !buffer || !pos) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    vfs_inode_t* inode = file->f_inode;
    ramfs_inode_info_t* info = (ramfs_inode_info_t*)inode->i_private;
    
    if (!info || info->is_directory) {
        return VFS_ERROR_IS_DIRECTORY;
    }
    
    /* Check bounds */
    if (*pos >= (loff_t)info->size) {
        return 0;  /* EOF */
    }
    
    /* Adjust count if it goes beyond file size */
    if (*pos + count > info->size) {
        count = info->size - *pos;
    }
    
    /* Copy data */
    if (info->data && count > 0) {
        memcpy(buffer, info->data + *pos, count);
        *pos += count;
        return count;
    }
    
    return 0;
}

/**
 * Write to a RAM filesystem file
 */
static ssize_t ramfs_write(vfs_file_t* file, const char* buffer, size_t count, loff_t* pos) {
    if (!file || !buffer || !pos) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    vfs_inode_t* inode = file->f_inode;
    ramfs_inode_info_t* info = (ramfs_inode_info_t*)inode->i_private;
    
    if (!info || info->is_directory) {
        return VFS_ERROR_IS_DIRECTORY;
    }
    
    /* Check if we need to expand the buffer */
    size_t new_size = *pos + count;
    if (new_size > RAMFS_MAX_FILE_SIZE) {
        return VFS_ERROR_NO_SPACE;
    }
    
    if (new_size > info->capacity) {
        /* Allocate new buffer */
        size_t new_capacity = (new_size + 4095) & ~4095;  /* Round up to 4K */
        if (new_capacity > RAMFS_MAX_FILE_SIZE) {
            new_capacity = RAMFS_MAX_FILE_SIZE;
        }
        
        uint8_t* new_data = (uint8_t*)kmalloc(new_capacity);
        if (!new_data) {
            return VFS_ERROR_NO_MEMORY;
        }
        
        /* Copy existing data */
        if (info->data && info->size > 0) {
            memcpy(new_data, info->data, info->size);
            kfree(info->data);
        }
        
        info->data = new_data;
        info->capacity = new_capacity;
    }
    
    /* Write data */
    memcpy(info->data + *pos, buffer, count);
    *pos += count;
    
    /* Update file size */
    if (new_size > info->size) {
        info->size = new_size;
        inode->i_size = new_size;
    }
    
    return count;
}

/* ================================
 * Registration Function
 * ================================ */

/**
 * Register the RAM filesystem
 */
int ramfs_init(void) {
    return vfs_register_filesystem(&ramfs_fs_type);
}

/**
 * Unregister the RAM filesystem
 */
void ramfs_exit(void) {
    vfs_unregister_filesystem(&ramfs_fs_type);
}
