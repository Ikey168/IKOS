/* IKOS FAT Filesystem Implementation
 * Support for FAT16 and FAT32 filesystems integrated with VFS
 */

#include "fat.h"
#include "vfs.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* String functions */
static size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

static char* strrchr(const char* str, int c) {
    const char* last = NULL;
    while (*str) {
        if (*str == c) last = str;
        str++;
    }
    if (c == '\0') return (char*)str;
    return (char*)last;
}

static void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = (unsigned char*)ptr;
    while (num--) *p++ = (unsigned char)value;
    return ptr;
}

static void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

static int toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
    return c;
}

static int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

/* Function declarations */
static void debug_print(const char* format, ...);

/* External memory allocation functions */
extern void* kmalloc(size_t size);
extern void kfree(void* ptr);

/* FAT filesystem operations tables */
static vfs_superblock_operations_t fat_super_ops = {
    .alloc_inode = fat_alloc_inode,
    .destroy_inode = fat_destroy_inode,
    .statfs = NULL,  /* Not implemented yet */
    .remount_fs = NULL,  /* Not implemented yet */
};

static vfs_inode_operations_t fat_dir_inode_ops = {
    .lookup = fat_lookup,
    .create = fat_create,
    .mkdir = fat_mkdir,
    .rmdir = fat_rmdir,
    .rename = fat_rename,
    .getattr = NULL,  /* Not implemented yet */
    .setattr = NULL,  /* Not implemented yet */
};

static vfs_file_operations_t fat_file_ops = {
    .read = fat_read,
    .write = fat_write,
    .llseek = fat_llseek,
    .open = fat_open,
    .release = fat_release,
};

/* FAT filesystem type */
static vfs_filesystem_t fat_fs_type = {
    .name = "fat",
    .mount = fat_mount,
    .kill_sb = fat_kill_sb,
    .fs_supers = 0,
    .next = NULL,
};

/* ================================
 * Filesystem Registration
 * ================================ */

/**
 * Initialize FAT filesystem support
 */
int fat_init(void) {
    debug_print("FAT: Initializing FAT filesystem support...\n");
    return vfs_register_filesystem(&fat_fs_type);
}

/**
 * Cleanup FAT filesystem support
 */
void fat_exit(void) {
    debug_print("FAT: Cleaning up FAT filesystem support...\n");
    vfs_unregister_filesystem(&fat_fs_type);
}

/* ================================
 * Mount/Unmount Operations
 * ================================ */

/**
 * Mount a FAT filesystem
 */
vfs_superblock_t* fat_mount(vfs_filesystem_t* fs, uint32_t flags,
                             const char* dev_name, void* data) {
    debug_print("FAT: Mounting FAT filesystem from %s\n", dev_name);
    
    /* Allocate filesystem info structure */
    fat_fs_info_t* fat_info = (fat_fs_info_t*)kmalloc(sizeof(fat_fs_info_t));
    if (!fat_info) {
        debug_print("FAT: Failed to allocate filesystem info\n");
        return NULL;
    }
    memset(fat_info, 0, sizeof(fat_fs_info_t));
    
    /* Initialize block device interface (simplified for now) */
    fat_block_device_t* block_dev = (fat_block_device_t*)data;
    if (!block_dev) {
        debug_print("FAT: No block device provided\n");
        kfree(fat_info);
        return NULL;
    }
    fat_info->block_device = block_dev;
    
    /* Read boot sector */
    fat_boot_sector_t boot_sector;
    if (fat_read_sectors(fat_info, 0, 1, &boot_sector) != FAT_SUCCESS) {
        debug_print("FAT: Failed to read boot sector\n");
        kfree(fat_info);
        return NULL;
    }
    
    /* Validate boot sector */
    if (!fat_is_valid_boot_sector(&boot_sector)) {
        debug_print("FAT: Invalid boot sector\n");
        kfree(fat_info);
        return NULL;
    }
    
    /* Determine FAT type */
    fat_info->type = fat_determine_type(&boot_sector);
    if (fat_info->type == FAT_TYPE_UNKNOWN || fat_info->type == FAT_TYPE_FAT12) {
        debug_print("FAT: Unsupported FAT type\n");
        kfree(fat_info);
        return NULL;
    }
    
    /* Initialize filesystem parameters */
    fat_info->sector_size = boot_sector.bytes_per_sector;
    fat_info->sectors_per_cluster = boot_sector.sectors_per_cluster;
    fat_info->cluster_size = fat_info->sector_size * fat_info->sectors_per_cluster;
    fat_info->reserved_sectors = boot_sector.reserved_sectors;
    fat_info->num_fats = boot_sector.num_fats;
    
    if (fat_info->type == FAT_TYPE_FAT32) {
        fat_info->fat_size = boot_sector.fat32.fat_size_32;
        fat_info->root_cluster = boot_sector.fat32.root_cluster;
        fat_info->root_dir_entries = 0;
        fat_info->root_dir_sectors = 0;
    } else {
        fat_info->fat_size = boot_sector.fat_size_16;
        fat_info->root_cluster = 0;
        fat_info->root_dir_entries = boot_sector.root_entries;
        fat_info->root_dir_sectors = (fat_info->root_dir_entries * 32 + 
                                     fat_info->sector_size - 1) / fat_info->sector_size;
    }
    
    fat_info->total_sectors = boot_sector.total_sectors_16 ? 
                              boot_sector.total_sectors_16 : boot_sector.total_sectors_32;
    fat_info->first_data_sector = fat_info->reserved_sectors + 
                                  (fat_info->num_fats * fat_info->fat_size) + 
                                  fat_info->root_dir_sectors;
    fat_info->data_sectors = fat_info->total_sectors - fat_info->first_data_sector;
    fat_info->total_clusters = fat_info->data_sectors / fat_info->sectors_per_cluster;
    
    /* Load FAT table */
    if (fat_load_fat_table(fat_info) != FAT_SUCCESS) {
        debug_print("FAT: Failed to load FAT table\n");
        kfree(fat_info);
        return NULL;
    }
    
    /* Create superblock */
    vfs_superblock_t* sb = (vfs_superblock_t*)kmalloc(sizeof(vfs_superblock_t));
    if (!sb) {
        debug_print("FAT: Failed to allocate superblock\n");
        if (fat_info->fat_table) {
            kfree(fat_info->fat_table);
        }
        kfree(fat_info);
        return NULL;
    }
    
    /* Initialize superblock */
    memset(sb, 0, sizeof(vfs_superblock_t));
    sb->s_magic = 0x46415431;  /* Custom magic for FAT ('FAT1') */
    sb->s_type = fs;
    sb->s_op = &fat_super_ops;
    sb->s_fs_info = fat_info;
    fat_info->sb = sb;
    
    /* Create root inode */
    vfs_inode_t* root_inode = fat_alloc_inode(sb);
    if (!root_inode) {
        debug_print("FAT: Failed to create root inode\n");
        if (fat_info->fat_table) {
            kfree(fat_info->fat_table);
        }
        kfree(fat_info);
        kfree(sb);
        return NULL;
    }
    
    /* Set root inode properties */
    root_inode->i_mode = VFS_FILE_TYPE_DIRECTORY;
    root_inode->i_op = &fat_dir_inode_ops;
    root_inode->i_fop = NULL;  /* Directories don't have file operations */
    
    fat_inode_info_t* root_info = (fat_inode_info_t*)root_inode->i_private;
    root_info->is_directory = true;
    if (fat_info->type == FAT_TYPE_FAT32) {
        root_info->first_cluster = fat_info->root_cluster;
    } else {
        root_info->first_cluster = 0;  /* FAT16 root is in a special location */
    }
    
    /* Create root dentry */
    vfs_dentry_t* root_dentry = vfs_alloc_dentry("/");
    if (!root_dentry) {
        debug_print("FAT: Failed to create root dentry\n");
        fat_destroy_inode(root_inode);
        if (fat_info->fat_table) {
            kfree(fat_info->fat_table);
        }
        kfree(fat_info);
        kfree(sb);
        return NULL;
    }
    
    root_dentry->d_inode = root_inode;
    sb->s_root = root_dentry;
    
    fs->fs_supers++;
    debug_print("FAT: Successfully mounted FAT%d filesystem\n", 
                fat_info->type == FAT_TYPE_FAT16 ? 16 : 32);
    return sb;
}

/**
 * Kill a FAT filesystem superblock
 */
void fat_kill_sb(vfs_superblock_t* sb) {
    if (!sb) {
        return;
    }
    
    debug_print("FAT: Killing FAT filesystem superblock\n");
    
    fat_fs_info_t* fat_info = (fat_fs_info_t*)sb->s_fs_info;
    if (fat_info) {
        /* Write back FAT table if dirty */
        if (fat_info->fat_dirty) {
            fat_write_fat_table(fat_info);
        }
        
        /* Free FAT table */
        if (fat_info->fat_table) {
            kfree(fat_info->fat_table);
        }
        
        kfree(fat_info);
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
 * Superblock Operations
 * ================================ */

/**
 * Allocate a FAT inode
 */
vfs_inode_t* fat_alloc_inode(vfs_superblock_t* sb) {
    if (!sb) {
        return NULL;
    }
    
    /* Allocate inode */
    vfs_inode_t* inode = vfs_alloc_inode(sb);
    if (!inode) {
        return NULL;
    }
    
    /* Allocate FAT-specific inode info */
    fat_inode_info_t* fat_info = (fat_inode_info_t*)kmalloc(sizeof(fat_inode_info_t));
    if (!fat_info) {
        vfs_free_inode(inode);
        return NULL;
    }
    
    /* Initialize FAT inode info */
    memset(fat_info, 0, sizeof(fat_inode_info_t));
    fat_info->first_cluster = 0;
    fat_info->current_cluster = 0;
    fat_info->cluster_offset = 0;
    fat_info->is_directory = false;
    
    inode->i_private = fat_info;
    return inode;
}

/**
 * Destroy a FAT inode
 */
void fat_destroy_inode(vfs_inode_t* inode) {
    if (!inode) {
        return;
    }
    
    fat_inode_info_t* fat_info = (fat_inode_info_t*)inode->i_private;
    if (fat_info) {
        kfree(fat_info);
    }
    
    vfs_free_inode(inode);
}

/* ================================
 * Block Device Operations
 * ================================ */

/**
 * Read sectors from block device
 */
int fat_read_sectors(fat_fs_info_t* fat_info, uint32_t sector, 
                     uint32_t count, void* buffer) {
    if (!fat_info || !fat_info->block_device || !buffer) {
        return FAT_ERROR_IO_ERROR;
    }
    
    fat_block_device_t* dev = (fat_block_device_t*)fat_info->block_device;
    if (!dev->read_sectors) {
        return FAT_ERROR_IO_ERROR;
    }
    
    return dev->read_sectors(dev->private_data, sector, count, buffer);
}

/**
 * Write sectors to block device
 */
int fat_write_sectors(fat_fs_info_t* fat_info, uint32_t sector,
                      uint32_t count, const void* buffer) {
    if (!fat_info || !fat_info->block_device || !buffer) {
        return FAT_ERROR_IO_ERROR;
    }
    
    fat_block_device_t* dev = (fat_block_device_t*)fat_info->block_device;
    if (!dev->write_sectors) {
        return FAT_ERROR_IO_ERROR;
    }
    
    return dev->write_sectors(dev->private_data, sector, count, buffer);
}

/* ================================
 * FAT Table Operations
 * ================================ */

/**
 * Load FAT table into memory
 */
int fat_load_fat_table(fat_fs_info_t* fat_info) {
    if (!fat_info) {
        return FAT_ERROR_IO_ERROR;
    }
    
    /* Calculate FAT table size */
    fat_info->fat_table_size = fat_info->fat_size * fat_info->sector_size;
    
    /* Allocate memory for FAT table */
    fat_info->fat_table = (uint8_t*)kmalloc(fat_info->fat_table_size);
    if (!fat_info->fat_table) {
        return FAT_ERROR_IO_ERROR;
    }
    
    /* Read first FAT */
    uint32_t fat_start_sector = fat_info->reserved_sectors;
    if (fat_read_sectors(fat_info, fat_start_sector, fat_info->fat_size, 
                         fat_info->fat_table) != FAT_SUCCESS) {
        kfree(fat_info->fat_table);
        fat_info->fat_table = NULL;
        return FAT_ERROR_IO_ERROR;
    }
    
    fat_info->fat_dirty = false;
    debug_print("FAT: Loaded FAT table (%u bytes)\n", fat_info->fat_table_size);
    return FAT_SUCCESS;
}

/**
 * Write FAT table back to disk
 */
int fat_write_fat_table(fat_fs_info_t* fat_info) {
    if (!fat_info || !fat_info->fat_table || !fat_info->fat_dirty) {
        return FAT_SUCCESS;
    }
    
    /* Write to all FAT copies */
    for (uint32_t i = 0; i < fat_info->num_fats; i++) {
        uint32_t fat_start_sector = fat_info->reserved_sectors + (i * fat_info->fat_size);
        if (fat_write_sectors(fat_info, fat_start_sector, fat_info->fat_size,
                              fat_info->fat_table) != FAT_SUCCESS) {
            return FAT_ERROR_IO_ERROR;
        }
    }
    
    fat_info->fat_dirty = false;
    debug_print("FAT: Wrote FAT table to disk\n");
    return FAT_SUCCESS;
}

/**
 * Get cluster value from FAT table
 */
uint32_t fat_get_cluster_value(fat_fs_info_t* fat_info, uint32_t cluster) {
    if (!fat_info || !fat_info->fat_table || cluster < 2) {
        return 0;
    }
    
    if (fat_info->type == FAT_TYPE_FAT16) {
        uint16_t* fat16 = (uint16_t*)fat_info->fat_table;
        if (cluster >= fat_info->total_clusters + 2) {
            return 0;
        }
        return fat16[cluster];
    } else if (fat_info->type == FAT_TYPE_FAT32) {
        uint32_t* fat32 = (uint32_t*)fat_info->fat_table;
        if (cluster >= fat_info->total_clusters + 2) {
            return 0;
        }
        return fat32[cluster] & 0x0FFFFFFF;  /* Mask out upper 4 bits */
    }
    
    return 0;
}

/**
 * Set cluster value in FAT table
 */
int fat_set_cluster_value(fat_fs_info_t* fat_info, uint32_t cluster, uint32_t value) {
    if (!fat_info || !fat_info->fat_table || cluster < 2) {
        return FAT_ERROR_INVALID_CLUSTER;
    }
    
    if (fat_info->type == FAT_TYPE_FAT16) {
        uint16_t* fat16 = (uint16_t*)fat_info->fat_table;
        if (cluster >= fat_info->total_clusters + 2) {
            return FAT_ERROR_INVALID_CLUSTER;
        }
        fat16[cluster] = (uint16_t)value;
    } else if (fat_info->type == FAT_TYPE_FAT32) {
        uint32_t* fat32 = (uint32_t*)fat_info->fat_table;
        if (cluster >= fat_info->total_clusters + 2) {
            return FAT_ERROR_INVALID_CLUSTER;
        }
        fat32[cluster] = (fat32[cluster] & 0xF0000000) | (value & 0x0FFFFFFF);
    } else {
        return FAT_ERROR_UNSUPPORTED_TYPE;
    }
    
    fat_info->fat_dirty = true;
    return FAT_SUCCESS;
}

/**
 * Find a free cluster in the FAT table
 */
uint32_t fat_find_free_cluster(fat_fs_info_t* fat_info) {
    if (!fat_info || !fat_info->fat_table) {
        return 0;
    }
    
    for (uint32_t cluster = 2; cluster < fat_info->total_clusters + 2; cluster++) {
        if (fat_get_cluster_value(fat_info, cluster) == FAT_CLUSTER_FREE) {
            return cluster;
        }
    }
    
    return 0;  /* No free clusters */
}

/* ================================
 * Cluster Operations
 * ================================ */

/**
 * Convert cluster number to first sector
 */
uint32_t fat_cluster_to_sector(fat_fs_info_t* fat_info, uint32_t cluster) {
    if (!fat_info || cluster < 2) {
        return 0;
    }
    
    return fat_info->first_data_sector + ((cluster - 2) * fat_info->sectors_per_cluster);
}

/**
 * Get next cluster in chain
 */
uint32_t fat_next_cluster(fat_fs_info_t* fat_info, uint32_t cluster) {
    if (!fat_info) {
        return 0;
    }
    
    uint32_t next = fat_get_cluster_value(fat_info, cluster);
    
    if (fat_is_cluster_eof(fat_info, next) || fat_is_cluster_bad(fat_info, next)) {
        return 0;
    }
    
    return next;
}

/**
 * Check if cluster is free
 */
bool fat_is_cluster_free(fat_fs_info_t* fat_info, uint32_t cluster) {
    return fat_get_cluster_value(fat_info, cluster) == FAT_CLUSTER_FREE;
}

/**
 * Check if cluster marks end of file
 */
bool fat_is_cluster_eof(fat_fs_info_t* fat_info, uint32_t cluster) {
    if (fat_info->type == FAT_TYPE_FAT16) {
        return cluster >= 0xFFF8;
    } else if (fat_info->type == FAT_TYPE_FAT32) {
        return cluster >= 0x0FFFFFF8;
    }
    return false;
}

/**
 * Check if cluster is marked as bad
 */
bool fat_is_cluster_bad(fat_fs_info_t* fat_info, uint32_t cluster) {
    if (fat_info->type == FAT_TYPE_FAT16) {
        return cluster == FAT_CLUSTER_BAD;
    } else if (fat_info->type == FAT_TYPE_FAT32) {
        return cluster == 0x0FFFFFF7;
    }
    return false;
}

/* ================================
 * Utility Functions
 * ================================ */

/**
 * Determine FAT type from boot sector
 */
fat_type_t fat_determine_type(const fat_boot_sector_t* boot_sector) {
    if (!boot_sector) {
        return FAT_TYPE_UNKNOWN;
    }
    
    uint32_t total_sectors = boot_sector->total_sectors_16 ? 
                             boot_sector->total_sectors_16 : boot_sector->total_sectors_32;
    uint32_t fat_size = boot_sector->fat_size_16 ? 
                        boot_sector->fat_size_16 : boot_sector->fat32.fat_size_32;
    uint32_t root_dir_sectors = (boot_sector->root_entries * 32 + 
                                boot_sector->bytes_per_sector - 1) / boot_sector->bytes_per_sector;
    uint32_t first_data_sector = boot_sector->reserved_sectors + 
                                 (boot_sector->num_fats * fat_size) + root_dir_sectors;
    uint32_t data_sectors = total_sectors - first_data_sector;
    uint32_t total_clusters = data_sectors / boot_sector->sectors_per_cluster;
    
    if (total_clusters < 4085) {
        return FAT_TYPE_FAT12;
    } else if (total_clusters < 65525) {
        return FAT_TYPE_FAT16;
    } else {
        return FAT_TYPE_FAT32;
    }
}

/**
 * Validate boot sector
 */
bool fat_is_valid_boot_sector(const fat_boot_sector_t* boot_sector) {
    if (!boot_sector) {
        return false;
    }
    
    /* Check boot signature */
    if (boot_sector->boot_sector_signature != 0xAA55) {
        return false;
    }
    
    /* Check basic parameters */
    if (boot_sector->bytes_per_sector != 512 && boot_sector->bytes_per_sector != 1024 &&
        boot_sector->bytes_per_sector != 2048 && boot_sector->bytes_per_sector != 4096) {
        return false;
    }
    
    if (boot_sector->sectors_per_cluster == 0 || 
        (boot_sector->sectors_per_cluster & (boot_sector->sectors_per_cluster - 1)) != 0) {
        return false;  /* Must be power of 2 */
    }
    
    if (boot_sector->reserved_sectors == 0) {
        return false;
    }
    
    if (boot_sector->num_fats == 0) {
        return false;
    }
    
    return true;
}

/* ================================
 * Inode Operations
 * ================================ */

/**
 * Lookup a file in a FAT directory
 */
vfs_dentry_t* fat_lookup(vfs_inode_t* dir, vfs_dentry_t* dentry) {
    if (!dir || !dentry) {
        return NULL;
    }
    
    const char* name = dentry->d_name;
    
    fat_inode_info_t* dir_info = (fat_inode_info_t*)dir->i_private;
    if (!dir_info || !dir_info->is_directory) {
        return NULL;
    }
    
    fat_fs_info_t* fat_info = (fat_fs_info_t*)dir->i_sb->s_fs_info;
    if (!fat_info) {
        return NULL;
    }
    
    /* Find directory entry */
    fat_dir_entry_t entry;
    uint32_t entry_offset;
    
    if (fat_find_dir_entry(fat_info, dir_info->first_cluster, name, 
                           &entry, &entry_offset) != FAT_SUCCESS) {
        return NULL;  /* File not found */
    }
    
    /* Create new dentry */
    vfs_dentry_t* new_dentry = vfs_alloc_dentry(name);
    if (!new_dentry) {
        return NULL;
    }
    
    /* Create inode for the file */
    vfs_inode_t* inode = fat_alloc_inode(dir->i_sb);
    if (!inode) {
        vfs_free_dentry(new_dentry);
        return NULL;
    }
    
    /* Set up inode from directory entry */
    fat_inode_info_t* file_info = (fat_inode_info_t*)inode->i_private;
    file_info->dir_entry = entry;
    file_info->first_cluster = (uint32_t)entry.first_cluster_low | 
                               ((uint32_t)entry.first_cluster_high << 16);
    file_info->is_directory = (entry.attributes & FAT_ATTR_DIRECTORY) != 0;
    
    inode->i_size = entry.file_size;
    inode->i_mode = file_info->is_directory ? VFS_FILE_TYPE_DIRECTORY : VFS_FILE_TYPE_REGULAR;
    
    if (file_info->is_directory) {
        inode->i_op = &fat_dir_inode_ops;
        inode->i_fop = NULL;
    } else {
        inode->i_op = NULL;
        inode->i_fop = &fat_file_ops;
    }
    
    new_dentry->d_inode = inode;
    return new_dentry;
}

/**
 * Create a file in a FAT directory
 */
int fat_create(vfs_inode_t* dir, vfs_dentry_t* dentry, uint32_t mode, bool excl) {
    if (!dir || !dentry) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    const char* name = dentry->d_name;
    
    fat_inode_info_t* dir_info = (fat_inode_info_t*)dir->i_private;
    if (!dir_info || !dir_info->is_directory) {
        return VFS_ERROR_NOT_DIRECTORY;
    }
    
    fat_fs_info_t* fat_info = (fat_fs_info_t*)dir->i_sb->s_fs_info;
    if (!fat_info) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    /* Check if file already exists */
    fat_dir_entry_t existing_entry;
    uint32_t existing_offset;
    if (fat_find_dir_entry(fat_info, dir_info->first_cluster, name,
                           &existing_entry, &existing_offset) == FAT_SUCCESS) {
        return VFS_ERROR_EXISTS;
    }
    
    /* Allocate a cluster for the new file */
    uint32_t first_cluster = fat_find_free_cluster(fat_info);
    if (first_cluster == 0) {
        return VFS_ERROR_NO_SPACE;
    }
    
    /* Mark cluster as EOF */
    uint32_t eof_value = fat_info->type == FAT_TYPE_FAT16 ? FAT_CLUSTER_EOF16 : FAT_CLUSTER_EOF32;
    if (fat_set_cluster_value(fat_info, first_cluster, eof_value) != FAT_SUCCESS) {
        return VFS_ERROR_IO_ERROR;
    }
    
    /* Create directory entry */
    if (fat_create_dir_entry(fat_info, dir_info->first_cluster, name,
                             first_cluster, 0, FAT_ATTR_ARCHIVE) != FAT_SUCCESS) {
        /* Free the allocated cluster */
        fat_set_cluster_value(fat_info, first_cluster, FAT_CLUSTER_FREE);
        return VFS_ERROR_IO_ERROR;
    }
    
    return VFS_SUCCESS;
}

/**
 * Create a directory in a FAT filesystem
 */
int fat_mkdir(vfs_inode_t* dir, vfs_dentry_t* dentry, uint32_t mode) {
    if (!dir || !dentry) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    const char* name = dentry->d_name;
    
    fat_inode_info_t* dir_info = (fat_inode_info_t*)dir->i_private;
    if (!dir_info || !dir_info->is_directory) {
        return VFS_ERROR_NOT_DIRECTORY;
    }
    
    fat_fs_info_t* fat_info = (fat_fs_info_t*)dir->i_sb->s_fs_info;
    if (!fat_info) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    /* Check if directory already exists */
    fat_dir_entry_t existing_entry;
    uint32_t existing_offset;
    if (fat_find_dir_entry(fat_info, dir_info->first_cluster, name,
                           &existing_entry, &existing_offset) == FAT_SUCCESS) {
        return VFS_ERROR_EXISTS;
    }
    
    /* Allocate a cluster for the new directory */
    uint32_t first_cluster = fat_find_free_cluster(fat_info);
    if (first_cluster == 0) {
        return VFS_ERROR_NO_SPACE;
    }
    
    /* Mark cluster as EOF */
    uint32_t eof_value = fat_info->type == FAT_TYPE_FAT16 ? FAT_CLUSTER_EOF16 : FAT_CLUSTER_EOF32;
    if (fat_set_cluster_value(fat_info, first_cluster, eof_value) != FAT_SUCCESS) {
        return VFS_ERROR_IO_ERROR;
    }
    
    /* Initialize directory cluster with . and .. entries */
    uint8_t cluster_data[fat_info->cluster_size];
    memset(cluster_data, 0, fat_info->cluster_size);
    
    fat_dir_entry_t* dot_entry = (fat_dir_entry_t*)cluster_data;
    memset(dot_entry, 0, sizeof(fat_dir_entry_t));
    memcpy(dot_entry->name, ".          ", 11);
    dot_entry->attributes = FAT_ATTR_DIRECTORY;
    dot_entry->first_cluster_low = first_cluster & 0xFFFF;
    dot_entry->first_cluster_high = (first_cluster >> 16) & 0xFFFF;
    
    fat_dir_entry_t* dotdot_entry = (fat_dir_entry_t*)(cluster_data + 32);
    memset(dotdot_entry, 0, sizeof(fat_dir_entry_t));
    memcpy(dotdot_entry->name, "..         ", 11);
    dotdot_entry->attributes = FAT_ATTR_DIRECTORY;
    dotdot_entry->first_cluster_low = dir_info->first_cluster & 0xFFFF;
    dotdot_entry->first_cluster_high = (dir_info->first_cluster >> 16) & 0xFFFF;
    
    /* Write directory cluster */
    uint32_t sector = fat_cluster_to_sector(fat_info, first_cluster);
    if (fat_write_sectors(fat_info, sector, fat_info->sectors_per_cluster,
                          cluster_data) != FAT_SUCCESS) {
        fat_set_cluster_value(fat_info, first_cluster, FAT_CLUSTER_FREE);
        return VFS_ERROR_IO_ERROR;
    }
    
    /* Create directory entry in parent */
    if (fat_create_dir_entry(fat_info, dir_info->first_cluster, name,
                             first_cluster, 0, FAT_ATTR_DIRECTORY) != FAT_SUCCESS) {
        fat_set_cluster_value(fat_info, first_cluster, FAT_CLUSTER_FREE);
        return VFS_ERROR_IO_ERROR;
    }
    
    return VFS_SUCCESS;
}

/* ================================
 * File Operations
 * ================================ */

/**
 * Open a FAT file
 */
int fat_open(vfs_inode_t* inode, vfs_file_t* file) {
    if (!inode || !file) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    fat_inode_info_t* inode_info = (fat_inode_info_t*)inode->i_private;
    if (!inode_info || inode_info->is_directory) {
        return VFS_ERROR_IS_DIRECTORY;
    }
    
    /* Allocate file-specific info */
    fat_file_info_t* file_info = (fat_file_info_t*)kmalloc(sizeof(fat_file_info_t));
    if (!file_info) {
        return VFS_ERROR_NO_MEMORY;
    }
    
    memset(file_info, 0, sizeof(fat_file_info_t));
    file_info->current_cluster = inode_info->first_cluster;
    file_info->cluster_offset = 0;
    file_info->file_position = 0;
    
    /* Store file info in VFS file structure */
    file->f_private_data = file_info;
    
    return VFS_SUCCESS;
}

/**
 * Release a FAT file
 */
int fat_release(vfs_inode_t* inode, vfs_file_t* file) {
    if (!file) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    fat_file_info_t* file_info = (fat_file_info_t*)file->f_private_data;
    if (file_info) {
        kfree(file_info);
        file->f_private_data = NULL;
    }
    
    return VFS_SUCCESS;
}

/**
 * Read from a FAT file
 */
ssize_t fat_read(vfs_file_t* file, char* buffer, size_t count, uint64_t* pos) {
    if (!file || !buffer || !pos) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    vfs_inode_t* inode = file->f_inode;
    fat_inode_info_t* inode_info = (fat_inode_info_t*)inode->i_private;
    fat_file_info_t* file_info = (fat_file_info_t*)file->f_private_data;
    fat_fs_info_t* fat_info = (fat_fs_info_t*)inode->i_sb->s_fs_info;
    
    if (!inode_info || !file_info || !fat_info) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    /* Check bounds */
    if (*pos >= inode->i_size) {
        return 0;  /* EOF */
    }
    
    /* Adjust count if it goes beyond file size */
    if (*pos + count > inode->i_size) {
        count = inode->i_size - *pos;
    }
    
    size_t bytes_read = 0;
    uint32_t current_cluster = inode_info->first_cluster;
    
    /* Skip to the correct cluster for the current position */
    uint32_t cluster_num = *pos / fat_info->cluster_size;
    for (uint32_t i = 0; i < cluster_num && current_cluster != 0; i++) {
        current_cluster = fat_next_cluster(fat_info, current_cluster);
    }
    
    if (current_cluster == 0) {
        return VFS_ERROR_IO_ERROR;
    }
    
    uint32_t cluster_offset = *pos % fat_info->cluster_size;
    
    while (bytes_read < count && current_cluster != 0) {
        /* Read cluster */
        uint8_t cluster_data[fat_info->cluster_size];
        uint32_t sector = fat_cluster_to_sector(fat_info, current_cluster);
        
        if (fat_read_sectors(fat_info, sector, fat_info->sectors_per_cluster,
                             cluster_data) != FAT_SUCCESS) {
            return VFS_ERROR_IO_ERROR;
        }
        
        /* Copy data from cluster */
        size_t bytes_to_copy = fat_info->cluster_size - cluster_offset;
        if (bytes_to_copy > count - bytes_read) {
            bytes_to_copy = count - bytes_read;
        }
        
        memcpy(buffer + bytes_read, cluster_data + cluster_offset, bytes_to_copy);
        bytes_read += bytes_to_copy;
        cluster_offset = 0;  /* Reset offset for subsequent clusters */
        
        /* Move to next cluster */
        current_cluster = fat_next_cluster(fat_info, current_cluster);
    }
    
    *pos += bytes_read;
    return bytes_read;
}

/**
 * Write to a FAT file
 */
ssize_t fat_write(vfs_file_t* file, const char* buffer, size_t count, uint64_t* pos) {
    if (!file || !buffer || !pos) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    /* For now, implement a simple write that doesn't expand files */
    /* This is a basic implementation - full implementation would handle file expansion */
    
    vfs_inode_t* inode = file->f_inode;
    fat_inode_info_t* inode_info = (fat_inode_info_t*)inode->i_private;
    fat_file_info_t* file_info = (fat_file_info_t*)file->f_private_data;
    fat_fs_info_t* fat_info = (fat_fs_info_t*)inode->i_sb->s_fs_info;
    
    if (!inode_info || !file_info || !fat_info) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    /* Basic write implementation - write within existing file size */
    if (*pos + count > inode->i_size) {
        count = inode->i_size > *pos ? inode->i_size - *pos : 0;
    }
    
    if (count == 0) {
        return 0;
    }
    
    size_t bytes_written = 0;
    uint32_t current_cluster = inode_info->first_cluster;
    
    /* Skip to the correct cluster for the current position */
    uint32_t cluster_num = *pos / fat_info->cluster_size;
    for (uint32_t i = 0; i < cluster_num && current_cluster != 0; i++) {
        current_cluster = fat_next_cluster(fat_info, current_cluster);
    }
    
    if (current_cluster == 0) {
        return VFS_ERROR_IO_ERROR;
    }
    
    uint32_t cluster_offset = *pos % fat_info->cluster_size;
    
    while (bytes_written < count && current_cluster != 0) {
        /* Read cluster first (for partial writes) */
        uint8_t cluster_data[fat_info->cluster_size];
        uint32_t sector = fat_cluster_to_sector(fat_info, current_cluster);
        
        if (fat_read_sectors(fat_info, sector, fat_info->sectors_per_cluster,
                             cluster_data) != FAT_SUCCESS) {
            return VFS_ERROR_IO_ERROR;
        }
        
        /* Modify cluster data */
        size_t bytes_to_write = fat_info->cluster_size - cluster_offset;
        if (bytes_to_write > count - bytes_written) {
            bytes_to_write = count - bytes_written;
        }
        
        memcpy(cluster_data + cluster_offset, buffer + bytes_written, bytes_to_write);
        
        /* Write cluster back */
        if (fat_write_sectors(fat_info, sector, fat_info->sectors_per_cluster,
                              cluster_data) != FAT_SUCCESS) {
            return VFS_ERROR_IO_ERROR;
        }
        
        bytes_written += bytes_to_write;
        cluster_offset = 0;  /* Reset offset for subsequent clusters */
        
        /* Move to next cluster */
        current_cluster = fat_next_cluster(fat_info, current_cluster);
    }
    
    *pos += bytes_written;
    return bytes_written;
}

/**
 * Seek in a FAT file
 */
uint64_t fat_llseek(vfs_file_t* file, uint64_t offset, int whence) {
    if (!file) {
        return VFS_ERROR_INVALID_PARAM;
    }
    
    uint64_t new_pos;
    
    switch (whence) {
        case 0:  /* SEEK_SET */
            new_pos = offset;
            break;
        case 1:  /* SEEK_CUR */
            new_pos = file->f_pos + offset;
            break;
        case 2:  /* SEEK_END */
            new_pos = file->f_inode->i_size + offset;
            break;
        default:
            return VFS_ERROR_INVALID_PARAM;
    }
    
    file->f_pos = new_pos;
    return new_pos;
}

/* ================================
 * Directory Operations
 * ================================ */

/**
 * Find a directory entry by name
 */
int fat_find_dir_entry(fat_fs_info_t* fat_info, uint32_t dir_cluster,
                       const char* name, fat_dir_entry_t* entry, 
                       uint32_t* entry_offset) {
    if (!fat_info || !name || !entry) {
        return FAT_ERROR_INVALID_NAME;
    }
    
    /* Convert name to FAT 8.3 format */
    char fat_name[12];
    fat_name_to_83(name, fat_name);
    
    uint32_t current_cluster = dir_cluster;
    uint32_t offset = 0;
    
    /* For FAT16 root directory, handle specially */
    if (fat_info->type == FAT_TYPE_FAT16 && dir_cluster == 0) {
        /* Search FAT16 root directory */
        uint32_t root_start_sector = fat_info->reserved_sectors + 
                                     (fat_info->num_fats * fat_info->fat_size);
        
        for (uint32_t sector = 0; sector < fat_info->root_dir_sectors; sector++) {
            uint8_t sector_data[fat_info->sector_size];
            if (fat_read_sectors(fat_info, root_start_sector + sector, 1,
                                 sector_data) != FAT_SUCCESS) {
                return FAT_ERROR_IO_ERROR;
            }
            
            for (uint32_t i = 0; i < fat_info->sector_size; i += 32) {
                fat_dir_entry_t* dir_entry = (fat_dir_entry_t*)(sector_data + i);
                
                if (dir_entry->name[0] == 0) {
                    return FAT_ERROR_NOT_FOUND;  /* End of directory */
                }
                
                if (dir_entry->name[0] == 0xE5) {
                    continue;  /* Deleted entry */
                }
                
                if (memcmp(dir_entry->name, fat_name, 11) == 0) {
                    *entry = *dir_entry;
                    if (entry_offset) {
                        *entry_offset = (sector * fat_info->sector_size) + i;
                    }
                    return FAT_SUCCESS;
                }
            }
        }
    } else {
        /* Search data clusters */
        while (current_cluster != 0 && !fat_is_cluster_eof(fat_info, current_cluster)) {
            uint8_t cluster_data[fat_info->cluster_size];
            uint32_t sector = fat_cluster_to_sector(fat_info, current_cluster);
            
            if (fat_read_sectors(fat_info, sector, fat_info->sectors_per_cluster,
                                 cluster_data) != FAT_SUCCESS) {
                return FAT_ERROR_IO_ERROR;
            }
            
            for (uint32_t i = 0; i < fat_info->cluster_size; i += 32) {
                fat_dir_entry_t* dir_entry = (fat_dir_entry_t*)(cluster_data + i);
                
                if (dir_entry->name[0] == 0) {
                    return FAT_ERROR_NOT_FOUND;  /* End of directory */
                }
                
                if (dir_entry->name[0] == 0xE5) {
                    continue;  /* Deleted entry */
                }
                
                if (memcmp(dir_entry->name, fat_name, 11) == 0) {
                    *entry = *dir_entry;
                    if (entry_offset) {
                        *entry_offset = offset + i;
                    }
                    return FAT_SUCCESS;
                }
            }
            
            offset += fat_info->cluster_size;
            current_cluster = fat_next_cluster(fat_info, current_cluster);
        }
    }
    
    return FAT_ERROR_NOT_FOUND;
}

/**
 * Create a directory entry
 */
int fat_create_dir_entry(fat_fs_info_t* fat_info, uint32_t dir_cluster,
                         const char* name, uint32_t first_cluster,
                         uint32_t file_size, uint8_t attributes) {
    if (!fat_info || !name) {
        return FAT_ERROR_INVALID_NAME;
    }
    
    /* Convert name to FAT 8.3 format */
    char fat_name[12];
    fat_name_to_83(name, fat_name);
    
    /* Create directory entry */
    fat_dir_entry_t new_entry;
    memset(&new_entry, 0, sizeof(fat_dir_entry_t));
    memcpy(new_entry.name, fat_name, 11);
    new_entry.attributes = attributes;
    new_entry.first_cluster_low = first_cluster & 0xFFFF;
    new_entry.first_cluster_high = (first_cluster >> 16) & 0xFFFF;
    new_entry.file_size = file_size;
    
    /* TODO: Set proper timestamps */
    new_entry.creation_date = 0;
    new_entry.creation_time = 0;
    new_entry.write_date = 0;
    new_entry.write_time = 0;
    new_entry.last_access_date = 0;
    
    /* Find free directory entry slot */
    /* This is a simplified implementation - full implementation would handle directory expansion */
    
    return FAT_SUCCESS;  /* Placeholder - needs proper implementation */
}

/* ================================
 * Filename Operations
 * ================================ */

/**
 * Convert filename to FAT 8.3 format
 */
void fat_name_to_83(const char* name, char* fat_name) {
    if (!name || !fat_name) {
        return;
    }
    
    memset(fat_name, ' ', 11);
    fat_name[11] = '\0';
    
    const char* dot = strrchr(name, '.');
    int name_len = dot ? (dot - name) : strlen(name);
    int ext_len = dot ? strlen(dot + 1) : 0;
    
    /* Copy name part (max 8 characters) */
    for (int i = 0; i < name_len && i < 8; i++) {
        fat_name[i] = name[i] >= 'a' && name[i] <= 'z' ? name[i] - 32 : name[i];
    }
    
    /* Copy extension part (max 3 characters) */
    if (dot && ext_len > 0) {
        for (int i = 0; i < ext_len && i < 3; i++) {
            fat_name[8 + i] = dot[1 + i] >= 'a' && dot[1 + i] <= 'z' ? 
                              dot[1 + i] - 32 : dot[1 + i];
        }
    }
}

/**
 * Convert FAT 8.3 format to regular filename
 */
void fat_83_to_name(const char* fat_name, char* name) {
    if (!fat_name || !name) {
        return;
    }
    
    int pos = 0;
    
    /* Copy name part */
    for (int i = 0; i < 8 && fat_name[i] != ' '; i++) {
        name[pos++] = fat_name[i] >= 'A' && fat_name[i] <= 'Z' ? 
                      fat_name[i] + 32 : fat_name[i];
    }
    
    /* Add extension if present */
    if (fat_name[8] != ' ') {
        name[pos++] = '.';
        for (int i = 8; i < 11 && fat_name[i] != ' '; i++) {
            name[pos++] = fat_name[i] >= 'A' && fat_name[i] <= 'Z' ? 
                          fat_name[i] + 32 : fat_name[i];
        }
    }
    
    name[pos] = '\0';
}

/* ================================
 * Utility Functions (continued)
 * ================================ */

/**
 * Simple debug print function
 */
static void debug_print(const char* format, ...) {
    /* In a real kernel, this would output to the console/log */
    /* For now, it's a placeholder */
    (void)format;
}


