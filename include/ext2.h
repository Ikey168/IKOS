/* IKOS ext2/ext4 Filesystem Implementation Header
 * 
 * Comprehensive ext2/ext4 filesystem support for IKOS operating system.
 * This implementation provides:
 * - Complete ext2 filesystem read/write support
 * - ext4 features including extents, journaling, and large files
 * - Integration with the existing VFS layer
 * - Advanced ext4 features like flexible block groups
 * - Backward compatibility with ext2
 */

#ifndef EXT2_H
#define EXT2_H

#include "vfs.h"
#include <stdint.h>
#include <stdbool.h>

/* ext2/ext4 Constants */
#define EXT2_SUPER_MAGIC        0xEF53      /* ext2/ext3/ext4 magic number */
#define EXT2_MIN_BLOCK_SIZE     1024        /* Minimum block size */
#define EXT2_MAX_BLOCK_SIZE     65536       /* Maximum block size */
#define EXT2_MIN_FRAG_SIZE      1024        /* Minimum fragment size */
#define EXT2_MAX_FRAG_SIZE      4096        /* Maximum fragment size */
#define EXT2_MAX_NAME_LEN       255         /* Maximum filename length */
#define EXT2_MAX_SYMLINK_LEN    4096        /* Maximum symlink length */

/* Block and inode numbers */
#define EXT2_BAD_INO            1           /* Bad blocks inode */
#define EXT2_ROOT_INO           2           /* Root directory inode */
#define EXT2_JOURNAL_INO        8           /* Journal inode (ext3/ext4) */
#define EXT2_FIRST_INO          11          /* First non-reserved inode */

/* Directory entry types */
#define EXT2_FT_UNKNOWN         0           /* Unknown file type */
#define EXT2_FT_REG_FILE        1           /* Regular file */
#define EXT2_FT_DIR             2           /* Directory */
#define EXT2_FT_CHRDEV          3           /* Character device */
#define EXT2_FT_BLKDEV          4           /* Block device */
#define EXT2_FT_FIFO            5           /* FIFO */
#define EXT2_FT_SOCK            6           /* Socket */
#define EXT2_FT_SYMLINK         7           /* Symbolic link */

/* File mode constants */
#define EXT2_S_IFMT             0xF000      /* File type mask */
#define EXT2_S_IFSOCK           0xC000      /* Socket */
#define EXT2_S_IFLNK            0xA000      /* Symbolic link */
#define EXT2_S_IFREG            0x8000      /* Regular file */
#define EXT2_S_IFBLK            0x6000      /* Block device */
#define EXT2_S_IFDIR            0x4000      /* Directory */
#define EXT2_S_IFCHR            0x2000      /* Character device */
#define EXT2_S_IFIFO            0x1000      /* FIFO */

/* Permission bits */
#define EXT2_S_ISUID            0x0800      /* Set UID */
#define EXT2_S_ISGID            0x0400      /* Set GID */
#define EXT2_S_ISVTX            0x0200      /* Sticky bit */
#define EXT2_S_IRUSR            0x0100      /* User read */
#define EXT2_S_IWUSR            0x0080      /* User write */
#define EXT2_S_IXUSR            0x0040      /* User execute */
#define EXT2_S_IRGRP            0x0020      /* Group read */
#define EXT2_S_IWGRP            0x0010      /* Group write */
#define EXT2_S_IXGRP            0x0008      /* Group execute */
#define EXT2_S_IROTH            0x0004      /* Other read */
#define EXT2_S_IWOTH            0x0002      /* Other write */
#define EXT2_S_IXOTH            0x0001      /* Other execute */

/* Superblock state flags */
#define EXT2_VALID_FS           1           /* Unmounted cleanly */
#define EXT2_ERROR_FS           2           /* Errors detected */
#define EXT3_ORPHAN_FS          4           /* Orphans being recovered */

/* Superblock feature flags */
#define EXT2_FEATURE_COMPAT_DIR_PREALLOC    0x0001  /* Directory preallocation */
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES   0x0002  /* AFS server inodes */
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL     0x0004  /* Has journal */
#define EXT2_FEATURE_COMPAT_EXT_ATTR        0x0008  /* Extended attributes */
#define EXT2_FEATURE_COMPAT_RESIZE_INODE    0x0010  /* Resize inode */
#define EXT2_FEATURE_COMPAT_DIR_INDEX       0x0020  /* Directory indexing */

#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001  /* Sparse superblocks */
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE   0x0002  /* Large files */
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR    0x0004  /* Binary tree directories */
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE    0x0008  /* Huge files */
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM     0x0010  /* Group descriptor checksums */
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK    0x0020  /* Many links per directory */
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE  0x0040  /* Large inodes */

#define EXT2_FEATURE_INCOMPAT_COMPRESSION   0x0001  /* Compression */
#define EXT2_FEATURE_INCOMPAT_FILETYPE      0x0002  /* Directory entries have type */
#define EXT3_FEATURE_INCOMPAT_RECOVER       0x0004  /* Needs recovery */
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV   0x0008  /* Journal device */
#define EXT2_FEATURE_INCOMPAT_META_BG       0x0010  /* Meta block groups */
#define EXT4_FEATURE_INCOMPAT_EXTENTS       0x0040  /* Extents */
#define EXT4_FEATURE_INCOMPAT_64BIT         0x0080  /* 64-bit */
#define EXT4_FEATURE_INCOMPAT_MMP           0x0100  /* Multiple mount protection */
#define EXT4_FEATURE_INCOMPAT_FLEX_BG       0x0200  /* Flexible block groups */

/* ext2 Superblock structure */
typedef struct ext2_superblock {
    uint32_t s_inodes_count;            /* Total inodes count */
    uint32_t s_blocks_count_lo;         /* Total blocks count (low 32 bits) */
    uint32_t s_r_blocks_count_lo;       /* Reserved blocks count (low 32 bits) */
    uint32_t s_free_blocks_count_lo;    /* Free blocks count (low 32 bits) */
    uint32_t s_free_inodes_count;       /* Free inodes count */
    uint32_t s_first_data_block;        /* First data block */
    uint32_t s_log_block_size;          /* Block size (1024 << s_log_block_size) */
    uint32_t s_log_frag_size;           /* Fragment size */
    uint32_t s_blocks_per_group;        /* Blocks per group */
    uint32_t s_frags_per_group;         /* Fragments per group */
    uint32_t s_inodes_per_group;        /* Inodes per group */
    uint32_t s_mtime;                   /* Mount time */
    uint32_t s_wtime;                   /* Write time */
    uint16_t s_mnt_count;               /* Mount count */
    uint16_t s_max_mnt_count;           /* Maximal mount count */
    uint16_t s_magic;                   /* Magic signature */
    uint16_t s_state;                   /* File system state */
    uint16_t s_errors;                  /* Behaviour when detecting errors */
    uint16_t s_minor_rev_level;         /* Minor revision level */
    uint32_t s_lastcheck;               /* Time of last check */
    uint32_t s_checkinterval;           /* Max time between checks */
    uint32_t s_creator_os;              /* OS */
    uint32_t s_rev_level;               /* Revision level */
    uint16_t s_def_resuid;              /* Default uid for reserved blocks */
    uint16_t s_def_resgid;              /* Default gid for reserved blocks */
    
    /* Extended fields for EXT2_DYNAMIC_REV superblocks only */
    uint32_t s_first_ino;               /* First non-reserved inode */
    uint16_t s_inode_size;              /* Size of inode structure */
    uint16_t s_block_group_nr;          /* Block group number of this superblock */
    uint32_t s_feature_compat;          /* Compatible feature set */
    uint32_t s_feature_incompat;        /* Incompatible feature set */
    uint32_t s_feature_ro_compat;       /* Readonly-compatible feature set */
    uint8_t s_uuid[16];                 /* 128-bit uuid for volume */
    char s_volume_name[16];             /* Volume name */
    char s_last_mounted[64];            /* Directory where last mounted */
    uint32_t s_algorithm_usage_bitmap;  /* For compression */
    
    /* Performance hints for ext3/ext4 */
    uint8_t s_prealloc_blocks;          /* Blocks to preallocate for files */
    uint8_t s_prealloc_dir_blocks;      /* Blocks to preallocate for dirs */
    uint16_t s_reserved_gdt_blocks;     /* Per group desc for online growth */
    
    /* ext3/ext4 journaling support */
    uint8_t s_journal_uuid[16];         /* uuid of journal superblock */
    uint32_t s_journal_inum;            /* inode number of journal file */
    uint32_t s_journal_dev;             /* device number of journal file */
    uint32_t s_last_orphan;             /* start of list of inodes to delete */
    uint32_t s_hash_seed[4];            /* HTREE hash seed */
    uint8_t s_def_hash_version;         /* Default hash version to use */
    uint8_t s_jnl_backup_type;          /* Journal backup type */
    uint16_t s_desc_size;               /* Group descriptor size (ext4) */
    uint32_t s_default_mount_opts;      /* Default mount options */
    uint32_t s_first_meta_bg;           /* First metablock block group */
    uint32_t s_mkfs_time;               /* Filesystem creation time */
    uint32_t s_jnl_blocks[17];          /* Backup of journal inode */
    
    /* ext4 64-bit support */
    uint32_t s_blocks_count_hi;         /* Blocks count (high 32 bits) */
    uint32_t s_r_blocks_count_hi;       /* Reserved blocks count (high 32 bits) */
    uint32_t s_free_blocks_count_hi;    /* Free blocks count (high 32 bits) */
    uint16_t s_min_extra_isize;         /* Minimum extra inode size */
    uint16_t s_want_extra_isize;        /* Desired extra inode size */
    uint32_t s_flags;                   /* Miscellaneous flags */
    uint16_t s_raid_stride;             /* RAID stride */
    uint16_t s_mmp_update_interval;     /* # seconds to wait in MMP checking */
    uint64_t s_mmp_block;               /* Block for multi-mount protection */
    uint32_t s_raid_stripe_width;       /* blocks on all data disks (N*stride) */
    uint8_t s_log_groups_per_flex;      /* FLEX_BG group size */
    uint8_t s_checksum_type;            /* Metadata checksum algorithm used */
    uint16_t s_reserved_pad;            /* Padding to next 32bits */
    uint64_t s_kbytes_written;          /* Number of lifetime kilobytes written */
    uint32_t s_snapshot_inum;           /* Inode number of active snapshot */
    uint32_t s_snapshot_id;             /* Sequential ID of active snapshot */
    uint64_t s_snapshot_r_blocks_count; /* Reserved blocks for active snapshot */
    uint32_t s_snapshot_list;           /* inode number of snapshot list head */
    uint32_t s_error_count;             /* Number of errors */
    uint32_t s_first_error_time;        /* First time an error happened */
    uint32_t s_first_error_ino;         /* Inode involved in first error */
    uint64_t s_first_error_block;       /* Block involved in first error */
    uint8_t s_first_error_func[32];     /* Function where error happened */
    uint32_t s_first_error_line;        /* Line number where error happened */
    uint32_t s_last_error_time;         /* Most recent error time */
    uint32_t s_last_error_ino;          /* Inode involved in last error */
    uint32_t s_last_error_line;         /* Line number where error happened */
    uint64_t s_last_error_block;        /* Block involved in last error */
    uint8_t s_last_error_func[32];      /* Function where error happened */
    uint8_t s_mount_opts[64];           /* Default mount options */
    uint32_t s_usr_quota_inum;          /* inode for tracking user quota */
    uint32_t s_grp_quota_inum;          /* inode for tracking group quota */
    uint32_t s_overhead_clusters;       /* overhead blocks/clusters in fs */
    uint32_t s_backup_bgs[2];           /* groups with sparse_super2 SBs */
    uint8_t s_encrypt_algos[4];         /* Encryption algorithms in use */
    uint8_t s_encrypt_pw_salt[16];      /* Salt used for string2key algorithm */
    uint32_t s_lpf_ino;                 /* Location of the lost+found inode */
    uint32_t s_prj_quota_inum;          /* inode for tracking project quota */
    uint32_t s_checksum_seed;           /* crc32c(uuid) if csum_seed set */
    uint32_t s_reserved[98];            /* Padding to the end of the block */
    uint32_t s_checksum;                /* crc32c(superblock) */
} __attribute__((packed)) ext2_superblock_t;

/* Group descriptor structure */
typedef struct ext2_group_desc {
    uint32_t bg_block_bitmap_lo;        /* Blocks bitmap block (low 32 bits) */
    uint32_t bg_inode_bitmap_lo;        /* Inodes bitmap block (low 32 bits) */
    uint32_t bg_inode_table_lo;         /* Inodes table block (low 32 bits) */
    uint16_t bg_free_blocks_count_lo;   /* Free blocks count (low 16 bits) */
    uint16_t bg_free_inodes_count_lo;   /* Free inodes count (low 16 bits) */
    uint16_t bg_used_dirs_count_lo;     /* Directories count (low 16 bits) */
    uint16_t bg_flags;                  /* EXT4_BG_* flags */
    uint32_t bg_exclude_bitmap_lo;      /* Exclude bitmap for snapshots (low 32 bits) */
    uint16_t bg_block_bitmap_csum_lo;   /* crc32c(s_uuid+grp_num+bbitmap) LE (low 16 bits) */
    uint16_t bg_inode_bitmap_csum_lo;   /* crc32c(s_uuid+grp_num+ibitmap) LE (low 16 bits) */
    uint16_t bg_itable_unused_lo;       /* Unused inodes count (low 16 bits) */
    uint16_t bg_checksum;               /* crc16(sb_uuid+group+desc) */
    
    /* ext4 64-bit extensions */
    uint32_t bg_block_bitmap_hi;        /* Blocks bitmap block (high 32 bits) */
    uint32_t bg_inode_bitmap_hi;        /* Inodes bitmap block (high 32 bits) */
    uint32_t bg_inode_table_hi;         /* Inodes table block (high 32 bits) */
    uint16_t bg_free_blocks_count_hi;   /* Free blocks count (high 16 bits) */
    uint16_t bg_free_inodes_count_hi;   /* Free inodes count (high 16 bits) */
    uint16_t bg_used_dirs_count_hi;     /* Directories count (high 16 bits) */
    uint16_t bg_itable_unused_hi;       /* Unused inodes count (high 16 bits) */
    uint32_t bg_exclude_bitmap_hi;      /* Exclude bitmap block (high 32 bits) */
    uint16_t bg_block_bitmap_csum_hi;   /* crc32c(s_uuid+grp_num+bbitmap) BE (high 16 bits) */
    uint16_t bg_inode_bitmap_csum_hi;   /* crc32c(s_uuid+grp_num+ibitmap) BE (high 16 bits) */
    uint32_t bg_reserved;               /* Padding */
} __attribute__((packed)) ext2_group_desc_t;

/* Inode structure */
typedef struct ext2_inode {
    uint16_t i_mode;                    /* File mode */
    uint16_t i_uid;                     /* Low 16 bits of Owner Uid */
    uint32_t i_size_lo;                 /* Size in bytes (low 32 bits) */
    uint32_t i_atime;                   /* Access time */
    uint32_t i_ctime;                   /* Creation time */
    uint32_t i_mtime;                   /* Modification time */
    uint32_t i_dtime;                   /* Deletion Time */
    uint16_t i_gid;                     /* Low 16 bits of Group Id */
    uint16_t i_links_count;             /* Links count */
    uint32_t i_blocks_lo;               /* Blocks count (low 32 bits) */
    uint32_t i_flags;                   /* File flags */
    uint32_t i_osd1;                    /* OS dependent 1 */
    uint32_t i_block[15];               /* Pointers to blocks */
    uint32_t i_generation;              /* File version (for NFS) */
    uint32_t i_file_acl_lo;             /* File ACL (low 32 bits) */
    uint32_t i_size_high;               /* Size in bytes (high 32 bits) / dir_acl */
    uint32_t i_obso_faddr;              /* Obsoleted fragment address */
    uint16_t i_blocks_high;             /* Blocks count (high 16 bits) */
    uint16_t i_file_acl_high;           /* File ACL (high 16 bits) */
    uint16_t i_uid_high;                /* High 16 bits of Owner Uid */
    uint16_t i_gid_high;                /* High 16 bits of Group Id */
    uint16_t i_checksum_lo;             /* crc32c(uuid+inum+inode) LE (low 16 bits) */
    uint16_t i_reserved;                /* Reserved for future use */
    
    /* ext4 large inode extensions */
    uint16_t i_extra_isize;             /* Size of this inode - 128 */
    uint16_t i_checksum_hi;             /* crc32c(uuid+inum+inode) BE (high 16 bits) */
    uint32_t i_ctime_extra;             /* Extra creation time (nano seconds << 2 | epoch) */
    uint32_t i_mtime_extra;             /* Extra modification time (nano seconds << 2 | epoch) */
    uint32_t i_atime_extra;             /* Extra access time (nano seconds << 2 | epoch) */
    uint32_t i_crtime;                  /* File creation time */
    uint32_t i_crtime_extra;            /* Extra file creation time (nano seconds << 2 | epoch) */
    uint32_t i_version_hi;              /* High 32 bits for 64-bit version */
    uint32_t i_projid;                  /* Project ID */
} __attribute__((packed)) ext2_inode_t;

/* Directory entry structure */
typedef struct ext2_dir_entry {
    uint32_t inode;                     /* Inode number */
    uint16_t rec_len;                   /* Directory entry length */
    uint8_t name_len;                   /* Name length */
    uint8_t file_type;                  /* File type */
    char name[];                        /* File name (variable length) */
} __attribute__((packed)) ext2_dir_entry_t;

/* ext4 extent tree structures */
typedef struct ext4_extent_header {
    uint16_t eh_magic;                  /* Magic number (0xF30A) */
    uint16_t eh_entries;                /* Number of valid entries */
    uint16_t eh_max;                    /* Capacity of store in entries */
    uint16_t eh_depth;                  /* Has tree real underlying blocks? */
    uint32_t eh_generation;             /* Generation of the tree */
} __attribute__((packed)) ext4_extent_header_t;

typedef struct ext4_extent_idx {
    uint32_t ei_block;                  /* Index covers logical blocks from 'block' */
    uint32_t ei_leaf_lo;                /* Pointer to the physical block of the next level (low 32 bits) */
    uint16_t ei_leaf_hi;                /* Pointer to the physical block of the next level (high 16 bits) */
    uint16_t ei_unused;                 /* Reserved */
} __attribute__((packed)) ext4_extent_idx_t;

typedef struct ext4_extent {
    uint32_t ee_block;                  /* First logical block extent covers */
    uint16_t ee_len;                    /* Number of blocks covered by extent */
    uint16_t ee_start_hi;               /* High 16 bits of physical block */
    uint32_t ee_start_lo;               /* Low 32 bits of physical block */
} __attribute__((packed)) ext4_extent_t;

/* ext2/ext4 filesystem information */
typedef struct ext2_fs_info {
    ext2_superblock_t superblock;       /* Cached superblock */
    ext2_group_desc_t* group_desc;      /* Group descriptors */
    uint32_t block_size;                /* Block size in bytes */
    uint32_t groups_count;              /* Number of block groups */
    uint32_t inodes_per_group;          /* Inodes per group */
    uint32_t blocks_per_group;          /* Blocks per group */
    uint32_t desc_per_block;            /* Group descriptors per block */
    uint32_t desc_blocks;               /* Number of descriptor blocks */
    uint32_t inode_size;                /* Size of inode structure */
    bool has_64bit;                     /* 64-bit feature enabled */
    bool has_extents;                   /* Extents feature enabled */
    bool has_journal;                   /* Journal feature enabled */
    bool has_flex_bg;                   /* Flexible block groups enabled */
    
    /* Device information */
    void* block_device;                 /* Block device handle */
    vfs_superblock_t* sb;               /* VFS superblock */
    
    /* Caching */
    void* block_cache;                  /* Block cache */
    void* inode_cache;                  /* Inode cache */
} ext2_fs_info_t;

/* ext2/ext4 inode information */
typedef struct ext2_inode_info {
    ext2_inode_t raw_inode;             /* Raw on-disk inode */
    uint32_t inode_num;                 /* Inode number */
    uint32_t block_group;               /* Block group containing inode */
    uint32_t flags;                     /* File flags */
    uint32_t* block_list;               /* Block list for indirect blocks */
    uint32_t block_count;               /* Number of allocated blocks */
    bool is_extent_based;               /* Uses extent tree */
    ext4_extent_header_t* extent_root;  /* Root of extent tree */
} ext2_inode_info_t;

/* Block device interface */
typedef struct ext2_block_device {
    int (*read_blocks)(void* device, uint64_t block, 
                       uint32_t count, void* buffer);
    int (*write_blocks)(void* device, uint64_t block,
                        uint32_t count, const void* buffer);
    uint32_t block_size;                /* Block size in bytes */
    uint64_t total_blocks;              /* Total number of blocks */
    void* private_data;                 /* Device-specific data */
} ext2_block_device_t;

/* Error codes */
#define EXT2_SUCCESS            0
#define EXT2_ERROR_IO           -1
#define EXT2_ERROR_CORRUPT      -2
#define EXT2_ERROR_NO_MEMORY    -3
#define EXT2_ERROR_INVALID      -4
#define EXT2_ERROR_NOT_FOUND    -5
#define EXT2_ERROR_NO_SPACE     -6
#define EXT2_ERROR_READ_ONLY    -7
#define EXT2_ERROR_NOT_SUPPORTED -8

/* Function prototypes */

/* Filesystem operations */
int ext2_init(void);
void ext2_exit(void);

/* Mount/unmount operations */
vfs_superblock_t* ext2_mount(vfs_filesystem_t* fs, uint32_t flags,
                              const char* dev_name, void* data);
void ext2_kill_sb(vfs_superblock_t* sb);

/* Superblock operations */
vfs_inode_t* ext2_alloc_inode(vfs_superblock_t* sb);
void ext2_destroy_inode(vfs_inode_t* inode);
int ext2_write_super(vfs_superblock_t* sb);
int ext2_sync_fs(vfs_superblock_t* sb);

/* Inode operations */
int ext2_create(vfs_inode_t* dir, vfs_dentry_t* dentry, uint32_t mode, bool excl);
int ext2_mkdir(vfs_inode_t* dir, vfs_dentry_t* dentry, uint32_t mode);
int ext2_rmdir(vfs_inode_t* dir, vfs_dentry_t* dentry);
int ext2_unlink(vfs_inode_t* dir, vfs_dentry_t* dentry);
int ext2_rename(vfs_inode_t* old_dir, vfs_dentry_t* old_dentry,
                vfs_inode_t* new_dir, vfs_dentry_t* new_dentry);
vfs_dentry_t* ext2_lookup(vfs_inode_t* dir, vfs_dentry_t* dentry);
int ext2_getattr(vfs_dentry_t* dentry, vfs_stat_t* stat);
int ext2_setattr(vfs_dentry_t* dentry, const vfs_stat_t* stat);

/* File operations */
ssize_t ext2_read(vfs_file_t* file, char* buffer, size_t count, uint64_t* pos);
ssize_t ext2_write(vfs_file_t* file, const char* buffer, size_t count, uint64_t* pos);
int ext2_readdir(vfs_file_t* file, vfs_dirent_t* dirent);
uint64_t ext2_llseek(vfs_file_t* file, uint64_t offset, int whence);

/* Block and inode management */
int ext2_read_block(ext2_fs_info_t* fs, uint64_t block, void* buffer);
int ext2_write_block(ext2_fs_info_t* fs, uint64_t block, const void* buffer);
int ext2_read_inode(ext2_fs_info_t* fs, uint32_t inode_num, ext2_inode_t* inode);
int ext2_write_inode(ext2_fs_info_t* fs, uint32_t inode_num, const ext2_inode_t* inode);
uint32_t ext2_alloc_block(ext2_fs_info_t* fs, uint32_t goal);
void ext2_free_block(ext2_fs_info_t* fs, uint32_t block);
uint32_t ext2_alloc_inode(ext2_fs_info_t* fs, uint32_t dir_ino, uint16_t mode);
void ext2_free_inode(ext2_fs_info_t* fs, uint32_t inode_num);

/* Directory operations */
int ext2_add_link(vfs_inode_t* dir, const char* name, vfs_inode_t* inode);
int ext2_delete_entry(vfs_inode_t* dir, const char* name);
int ext2_find_entry(vfs_inode_t* dir, const char* name, uint32_t* inode_num);

/* Extent tree operations (ext4) */
int ext4_ext_get_blocks(vfs_inode_t* inode, uint64_t block, 
                        uint32_t max_blocks, uint64_t* result, bool create);
int ext4_ext_truncate(vfs_inode_t* inode, uint64_t new_size);

/* Utility functions */
uint32_t ext2_block_to_group(ext2_fs_info_t* fs, uint32_t block);
uint32_t ext2_inode_to_group(ext2_fs_info_t* fs, uint32_t inode_num);
uint64_t ext2_group_first_block(ext2_fs_info_t* fs, uint32_t group);
uint64_t ext2_group_last_block(ext2_fs_info_t* fs, uint32_t group);
bool ext2_test_bit(const uint8_t* bitmap, uint32_t bit);
void ext2_set_bit(uint8_t* bitmap, uint32_t bit);
void ext2_clear_bit(uint8_t* bitmap, uint32_t bit);
uint32_t ext2_find_first_zero_bit(const uint8_t* bitmap, uint32_t size);

/* Checksum functions */
uint32_t ext2_crc32c(uint32_t crc, const void* data, size_t len);
uint16_t ext2_crc16(uint16_t crc, const void* data, size_t len);

/* Debugging functions */
void ext2_dump_superblock(const ext2_superblock_t* sb);
void ext2_dump_group_desc(const ext2_group_desc_t* gd, uint32_t group);
void ext2_dump_inode(const ext2_inode_t* inode, uint32_t inode_num);

#endif /* EXT2_H */
