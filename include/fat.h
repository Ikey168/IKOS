/* IKOS FAT Filesystem Support
 * Implementation of FAT16 and FAT32 filesystem support for VFS
 */

#ifndef FAT_H
#define FAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "vfs.h"

/* Type definitions for compatibility */
typedef long long ssize_t;              /* Signed size type */

/* FAT filesystem constants */
#define FAT_SECTOR_SIZE         512
#define FAT_MAX_FILENAME        11      /* 8.3 format */
#define FAT_MAX_LONGNAME        255     /* Long filename support */
#define FAT_CLUSTER_FREE        0x0000
#define FAT_CLUSTER_BAD         0xFFF7
#define FAT_CLUSTER_EOF16       0xFFFF
#define FAT_CLUSTER_EOF32       0x0FFFFFFF
#define FAT_ROOT_DIR_CLUSTER    2       /* First data cluster for FAT32 */

/* FAT types */
typedef enum {
    FAT_TYPE_UNKNOWN = 0,
    FAT_TYPE_FAT12,
    FAT_TYPE_FAT16,
    FAT_TYPE_FAT32
} fat_type_t;

/* FAT Boot Sector (512 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  jump_boot[3];              /* Jump instruction */
    uint8_t  oem_name[8];               /* OEM name */
    uint16_t bytes_per_sector;          /* Bytes per sector */
    uint8_t  sectors_per_cluster;       /* Sectors per cluster */
    uint16_t reserved_sectors;          /* Reserved sectors */
    uint8_t  num_fats;                  /* Number of FATs */
    uint16_t root_entries;              /* Root directory entries (FAT12/16) */
    uint16_t total_sectors_16;          /* Total sectors (if < 65536) */
    uint8_t  media_type;                /* Media descriptor */
    uint16_t fat_size_16;               /* FAT size in sectors (FAT12/16) */
    uint16_t sectors_per_track;         /* Sectors per track */
    uint16_t num_heads;                 /* Number of heads */
    uint32_t hidden_sectors;            /* Hidden sectors */
    uint32_t total_sectors_32;          /* Total sectors (if >= 65536) */
    
    /* FAT12/16 specific fields */
    union {
        struct __attribute__((packed)) {
            uint8_t  drive_number;      /* Drive number */
            uint8_t  reserved;          /* Reserved */
            uint8_t  boot_signature;    /* Extended boot signature */
            uint32_t volume_id;         /* Volume serial number */
            uint8_t  volume_label[11];  /* Volume label */
            uint8_t  filesystem_type[8]; /* Filesystem type */
            uint8_t  boot_code[448];    /* Boot code */
        } fat16;
        
        struct __attribute__((packed)) {
            uint32_t fat_size_32;       /* FAT size in sectors */
            uint16_t ext_flags;         /* Extension flags */
            uint16_t fs_version;        /* Filesystem version */
            uint32_t root_cluster;      /* Root directory cluster */
            uint16_t fs_info;           /* Filesystem info sector */
            uint16_t backup_boot;       /* Backup boot sector */
            uint8_t  reserved[12];      /* Reserved */
            uint8_t  drive_number;      /* Drive number */
            uint8_t  reserved1;         /* Reserved */
            uint8_t  boot_signature;    /* Extended boot signature */
            uint32_t volume_id;         /* Volume serial number */
            uint8_t  volume_label[11];  /* Volume label */
            uint8_t  filesystem_type[8]; /* Filesystem type */
            uint8_t  boot_code[420];    /* Boot code */
        } fat32;
    };
    
    uint16_t boot_sector_signature;     /* Boot sector signature (0xAA55) */
} fat_boot_sector_t;

/* FAT Directory Entry (32 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  name[11];                  /* Filename (8.3 format) */
    uint8_t  attributes;                /* File attributes */
    uint8_t  reserved;                  /* Reserved for Windows NT */
    uint8_t  creation_time_tenth;       /* Creation time (tenths of seconds) */
    uint16_t creation_time;             /* Creation time */
    uint16_t creation_date;             /* Creation date */
    uint16_t last_access_date;          /* Last access date */
    uint16_t first_cluster_high;        /* High 16 bits of cluster (FAT32) */
    uint16_t write_time;                /* Last write time */
    uint16_t write_date;                /* Last write date */
    uint16_t first_cluster_low;         /* Low 16 bits of cluster */
    uint32_t file_size;                 /* File size in bytes */
} fat_dir_entry_t;

/* Long Filename Entry (32 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  order;                     /* Order of this entry */
    uint16_t name1[5];                  /* First 5 characters (UTF-16) */
    uint8_t  attributes;                /* Attributes (always 0x0F) */
    uint8_t  type;                      /* Type (always 0) */
    uint8_t  checksum;                  /* Checksum of short name */
    uint16_t name2[6];                  /* Next 6 characters (UTF-16) */
    uint16_t first_cluster;             /* First cluster (always 0) */
    uint16_t name3[2];                  /* Last 2 characters (UTF-16) */
} fat_lfn_entry_t;

/* FAT File Attributes */
#define FAT_ATTR_READ_ONLY      0x01
#define FAT_ATTR_HIDDEN         0x02
#define FAT_ATTR_SYSTEM         0x04
#define FAT_ATTR_VOLUME_ID      0x08
#define FAT_ATTR_DIRECTORY      0x10
#define FAT_ATTR_ARCHIVE        0x20
#define FAT_ATTR_LONG_NAME      0x0F    /* Long filename entry */

/* FAT Filesystem Information */
typedef struct {
    fat_type_t type;                    /* FAT type (12/16/32) */
    uint32_t sector_size;               /* Bytes per sector */
    uint32_t cluster_size;              /* Bytes per cluster */
    uint32_t sectors_per_cluster;       /* Sectors per cluster */
    uint32_t reserved_sectors;          /* Reserved sectors */
    uint32_t num_fats;                  /* Number of FATs */
    uint32_t fat_size;                  /* FAT size in sectors */
    uint32_t root_dir_sectors;          /* Root directory sectors */
    uint32_t total_sectors;             /* Total sectors */
    uint32_t data_sectors;              /* Data sectors */
    uint32_t total_clusters;            /* Total clusters */
    uint32_t first_data_sector;         /* First data sector */
    uint32_t root_cluster;              /* Root directory cluster (FAT32) */
    uint32_t root_dir_entries;          /* Root directory entries (FAT12/16) */
    
    /* Cached information */
    uint8_t* fat_table;                 /* Cached FAT table */
    uint32_t fat_table_size;            /* FAT table size in bytes */
    bool fat_dirty;                     /* FAT table needs writing */
    
    /* Block device information */
    void* block_device;                 /* Block device handle */
    
    /* VFS integration */
    vfs_superblock_t* sb;               /* Associated superblock */
} fat_fs_info_t;

/* FAT Inode Information */
typedef struct {
    uint32_t first_cluster;             /* First cluster of file */
    uint32_t current_cluster;           /* Current cluster for sequential access */
    uint32_t cluster_offset;            /* Offset within current cluster */
    bool is_directory;                  /* Directory flag */
    fat_dir_entry_t dir_entry;          /* Original directory entry */
} fat_inode_info_t;

/* FAT File Information */
typedef struct {
    uint32_t current_cluster;           /* Current cluster for I/O */
    uint32_t cluster_offset;            /* Offset within current cluster */
    uint32_t file_position;             /* Current file position */
} fat_file_info_t;

/* Function prototypes */

/* Filesystem operations */
int fat_init(void);
void fat_exit(void);

/* Mount/unmount operations */
vfs_superblock_t* fat_mount(vfs_filesystem_t* fs, uint32_t flags,
                             const char* dev_name, void* data);
void fat_kill_sb(vfs_superblock_t* sb);

/* Superblock operations */
vfs_inode_t* fat_alloc_inode(vfs_superblock_t* sb);
void fat_destroy_inode(vfs_inode_t* inode);
int fat_write_super(vfs_superblock_t* sb);

/* Inode operations */
vfs_dentry_t* fat_lookup(vfs_inode_t* dir, vfs_dentry_t* dentry);
int fat_create(vfs_inode_t* dir, vfs_dentry_t* dentry, uint32_t mode, bool excl);
int fat_mkdir(vfs_inode_t* dir, vfs_dentry_t* dentry, uint32_t mode);
int fat_rmdir(vfs_inode_t* dir, vfs_dentry_t* dentry);
int fat_unlink(vfs_inode_t* dir, vfs_dentry_t* dentry);
int fat_rename(vfs_inode_t* old_dir, vfs_dentry_t* old_dentry,
               vfs_inode_t* new_dir, vfs_dentry_t* new_dentry);

/* File operations */
int fat_open(vfs_inode_t* inode, vfs_file_t* file);
int fat_release(vfs_inode_t* inode, vfs_file_t* file);
ssize_t fat_read(vfs_file_t* file, char* buffer, size_t count, uint64_t* pos);
ssize_t fat_write(vfs_file_t* file, const char* buffer, size_t count, uint64_t* pos);
uint64_t fat_llseek(vfs_file_t* file, uint64_t offset, int whence);

/* Block device operations */
int fat_read_sectors(fat_fs_info_t* fat_info, uint32_t sector, 
                     uint32_t count, void* buffer);
int fat_write_sectors(fat_fs_info_t* fat_info, uint32_t sector,
                      uint32_t count, const void* buffer);

/* FAT table operations */
int fat_load_fat_table(fat_fs_info_t* fat_info);
int fat_write_fat_table(fat_fs_info_t* fat_info);
uint32_t fat_get_cluster_value(fat_fs_info_t* fat_info, uint32_t cluster);
int fat_set_cluster_value(fat_fs_info_t* fat_info, uint32_t cluster, uint32_t value);
uint32_t fat_find_free_cluster(fat_fs_info_t* fat_info);
int fat_allocate_cluster_chain(fat_fs_info_t* fat_info, uint32_t start_cluster, 
                               uint32_t num_clusters);
int fat_free_cluster_chain(fat_fs_info_t* fat_info, uint32_t start_cluster);

/* Cluster operations */
uint32_t fat_sector_to_cluster(fat_fs_info_t* fat_info, uint32_t sector);
uint32_t fat_cluster_to_sector(fat_fs_info_t* fat_info, uint32_t cluster);
uint32_t fat_next_cluster(fat_fs_info_t* fat_info, uint32_t cluster);
bool fat_is_cluster_free(fat_fs_info_t* fat_info, uint32_t cluster);
bool fat_is_cluster_eof(fat_fs_info_t* fat_info, uint32_t cluster);
bool fat_is_cluster_bad(fat_fs_info_t* fat_info, uint32_t cluster);

/* Directory operations */
int fat_read_dir_entry(fat_fs_info_t* fat_info, uint32_t cluster, 
                       uint32_t offset, fat_dir_entry_t* entry);
int fat_write_dir_entry(fat_fs_info_t* fat_info, uint32_t cluster,
                        uint32_t offset, const fat_dir_entry_t* entry);
int fat_find_dir_entry(fat_fs_info_t* fat_info, uint32_t dir_cluster,
                       const char* name, fat_dir_entry_t* entry, 
                       uint32_t* entry_offset);
int fat_create_dir_entry(fat_fs_info_t* fat_info, uint32_t dir_cluster,
                         const char* name, uint32_t first_cluster,
                         uint32_t file_size, uint8_t attributes);
int fat_delete_dir_entry(fat_fs_info_t* fat_info, uint32_t dir_cluster,
                         uint32_t entry_offset);

/* Filename operations */
void fat_name_to_83(const char* name, char* fat_name);
void fat_83_to_name(const char* fat_name, char* name);
bool fat_is_valid_name(const char* name);
uint8_t fat_checksum_name(const uint8_t* short_name);

/* Long filename support */
int fat_read_long_name(fat_fs_info_t* fat_info, uint32_t cluster,
                       uint32_t offset, char* long_name, size_t max_len);
int fat_write_long_name(fat_fs_info_t* fat_info, uint32_t cluster,
                        uint32_t offset, const char* long_name,
                        const char* short_name);

/* Utility functions */
fat_type_t fat_determine_type(const fat_boot_sector_t* boot_sector);
bool fat_is_valid_boot_sector(const fat_boot_sector_t* boot_sector);
uint32_t fat_calculate_data_sectors(const fat_boot_sector_t* boot_sector);
uint32_t fat_calculate_total_clusters(const fat_boot_sector_t* boot_sector);

/* Debug and statistics */
void fat_print_boot_sector(const fat_boot_sector_t* boot_sector);
void fat_print_dir_entry(const fat_dir_entry_t* entry);
void fat_get_stats(fat_fs_info_t* fat_info, void* stats);

/* Error codes specific to FAT */
#define FAT_SUCCESS                 0
#define FAT_ERROR_INVALID_BOOT      -1
#define FAT_ERROR_UNSUPPORTED_TYPE  -2
#define FAT_ERROR_IO_ERROR          -3
#define FAT_ERROR_NO_SPACE          -4
#define FAT_ERROR_INVALID_CLUSTER   -5
#define FAT_ERROR_CLUSTER_CHAIN     -6
#define FAT_ERROR_INVALID_NAME      -7
#define FAT_ERROR_FILE_EXISTS       -8
#define FAT_ERROR_NOT_FOUND         -9
#define FAT_ERROR_NOT_EMPTY         -10

/* Block device interface */
typedef struct {
    int (*read_sectors)(void* device, uint32_t sector, uint32_t count, void* buffer);
    int (*write_sectors)(void* device, uint32_t sector, uint32_t count, const void* buffer);
    uint32_t sector_size;
    uint32_t total_sectors;
    void* private_data;
} fat_block_device_t;

#endif /* FAT_H */
