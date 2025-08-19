/* IKOS ext2/ext4 Filesystem System Calls Interface
 * 
 * This header defines the system call interface for ext2/ext4 filesystem
 * operations, allowing user-space applications to interact with the
 * ext2/ext4 filesystem implementation.
 */

#ifndef EXT2_SYSCALLS_H
#define EXT2_SYSCALLS_H

#include "ext2.h"
#include <stdint.h>

/* ext2/ext4 System Call Numbers */
#define SYS_EXT2_MOUNT                  860
#define SYS_EXT2_UNMOUNT                861
#define SYS_EXT2_FORMAT                 862
#define SYS_EXT2_FSCK                   863
#define SYS_EXT2_GET_INFO               864
#define SYS_EXT2_SET_LABEL              865
#define SYS_EXT2_GET_LABEL              866
#define SYS_EXT2_RESIZE                 867
#define SYS_EXT2_DEFRAG                 868
#define SYS_EXT2_TUNE                   869
#define SYS_EXT2_BACKUP_SUPER           870
#define SYS_EXT2_RESTORE_SUPER          871
#define SYS_EXT2_LIST_MOUNTS            872
#define SYS_EXT2_GET_MOUNT_INFO         873
#define SYS_EXT2_SET_MOUNT_OPTS         874

/* ext2/ext4 Mount Options */
typedef struct ext2_mount_options {
    uint32_t flags;                     /* Mount flags */
    bool read_only;                     /* Read-only mount */
    bool no_atime;                      /* Don't update access times */
    bool sync;                          /* Synchronous writes */
    bool dirsync;                       /* Synchronous directory operations */
    bool no_exec;                       /* Don't allow execution */
    bool no_suid;                       /* Don't honor SUID/SGID bits */
    bool no_dev;                        /* Don't allow device files */
    uint32_t uid;                       /* Default UID */
    uint32_t gid;                       /* Default GID */
    uint16_t umask;                     /* Default umask */
    char journal_path[256];             /* External journal path */
    uint32_t commit_interval;           /* Journal commit interval */
    bool data_ordered;                  /* Ordered data mode */
    bool data_writeback;                /* Writeback data mode */
    bool data_journal;                  /* Journal data mode */
    uint32_t reserved[8];               /* Reserved for future use */
} ext2_mount_options_t;

/* ext2/ext4 Filesystem Information */
typedef struct ext2_fs_info_user {
    char device_name[256];              /* Device name */
    char mount_point[1024];             /* Mount point */
    char fs_type[32];                   /* Filesystem type (ext2/ext3/ext4) */
    char volume_label[16];              /* Volume label */
    uint8_t uuid[16];                   /* Filesystem UUID */
    uint64_t total_blocks;              /* Total blocks */
    uint64_t free_blocks;               /* Free blocks */
    uint64_t total_inodes;              /* Total inodes */
    uint64_t free_inodes;               /* Free inodes */
    uint32_t block_size;                /* Block size */
    uint32_t inode_size;                /* Inode size */
    uint32_t blocks_per_group;          /* Blocks per group */
    uint32_t inodes_per_group;          /* Inodes per group */
    uint32_t groups_count;              /* Number of groups */
    uint32_t mount_count;               /* Mount count */
    uint32_t max_mount_count;           /* Maximum mount count */
    uint32_t last_check;                /* Last check time */
    uint32_t check_interval;            /* Check interval */
    uint32_t feature_compat;            /* Compatible features */
    uint32_t feature_incompat;          /* Incompatible features */
    uint32_t feature_ro_compat;         /* Read-only compatible features */
    bool has_journal;                   /* Has journal */
    bool has_extents;                   /* Has extents */
    bool has_64bit;                     /* Has 64-bit support */
    bool needs_recovery;                /* Needs journal recovery */
    uint32_t errors_count;              /* Error count */
    uint32_t reserved[16];              /* Reserved for future use */
} ext2_fs_info_user_t;

/* ext2/ext4 Format Options */
typedef struct ext2_format_options {
    char device_name[256];              /* Device to format */
    char volume_label[16];              /* Volume label */
    uint32_t block_size;                /* Block size (1024, 2048, 4096) */
    uint32_t inode_size;                /* Inode size (128, 256, 512, 1024) */
    uint32_t blocks_per_group;          /* Blocks per group (0 = auto) */
    uint32_t inodes_per_group;          /* Inodes per group (0 = auto) */
    uint32_t inode_ratio;               /* Bytes per inode (0 = auto) */
    uint32_t reserved_percent;          /* Reserved blocks percentage */
    bool create_journal;                /* Create journal (ext3/ext4) */
    uint32_t journal_size;              /* Journal size (0 = auto) */
    bool enable_extents;                /* Enable extents (ext4) */
    bool enable_64bit;                  /* Enable 64-bit (ext4) */
    bool enable_flex_bg;                /* Enable flexible block groups */
    bool enable_dir_index;              /* Enable directory indexing */
    bool enable_sparse_super;           /* Enable sparse superblocks */
    bool enable_large_file;             /* Enable large files */
    bool force;                         /* Force format even if mounted */
    bool quick;                         /* Quick format (don't check blocks) */
    bool verbose;                       /* Verbose output */
    uint32_t reserved[8];               /* Reserved for future use */
} ext2_format_options_t;

/* ext2/ext4 Filesystem Check Options */
typedef struct ext2_fsck_options {
    char device_name[256];              /* Device to check */
    bool check_only;                    /* Check only, don't fix */
    bool force_check;                   /* Force check even if clean */
    bool auto_fix;                      /* Automatically fix errors */
    bool interactive;                   /* Interactive mode */
    bool verbose;                       /* Verbose output */
    bool check_blocks;                  /* Check for bad blocks */
    bool check_inodes;                  /* Check inode consistency */
    bool check_directories;             /* Check directory structure */
    bool check_journal;                 /* Check journal consistency */
    bool check_extents;                 /* Check extent tree integrity */
    uint32_t reserved[8];               /* Reserved for future use */
} ext2_fsck_options_t;

/* ext2/ext4 Filesystem Check Results */
typedef struct ext2_fsck_results {
    bool filesystem_clean;              /* Filesystem is clean */
    bool errors_fixed;                  /* Errors were fixed */
    bool needs_reboot;                  /* System needs reboot */
    uint32_t errors_found;              /* Number of errors found */
    uint32_t errors_fixed_count;        /* Number of errors fixed */
    uint32_t blocks_checked;            /* Blocks checked */
    uint32_t inodes_checked;            /* Inodes checked */
    uint32_t bad_blocks_found;          /* Bad blocks found */
    uint32_t orphaned_inodes;           /* Orphaned inodes found */
    uint32_t duplicate_blocks;          /* Duplicate blocks found */
    char error_log[4096];               /* Error log messages */
    uint32_t reserved[8];               /* Reserved for future use */
} ext2_fsck_results_t;

/* ext2/ext4 Tuning Options */
typedef struct ext2_tune_options {
    char device_name[256];              /* Device to tune */
    bool set_max_mount_count;           /* Set maximum mount count */
    uint32_t max_mount_count;           /* Maximum mount count value */
    bool set_check_interval;            /* Set check interval */
    uint32_t check_interval;            /* Check interval in seconds */
    bool set_reserved_percent;          /* Set reserved blocks percentage */
    uint32_t reserved_percent;          /* Reserved blocks percentage */
    bool set_reserved_uid;              /* Set reserved blocks UID */
    uint32_t reserved_uid;              /* Reserved blocks UID */
    bool set_reserved_gid;              /* Set reserved blocks GID */
    uint32_t reserved_gid;              /* Reserved blocks GID */
    bool enable_feature;                /* Enable feature */
    bool disable_feature;               /* Disable feature */
    uint32_t feature_mask;              /* Feature mask */
    bool set_journal_device;            /* Set journal device */
    char journal_device[256];           /* Journal device path */
    uint32_t reserved[8];               /* Reserved for future use */
} ext2_tune_options_t;

/* Mount Information */
typedef struct ext2_mount_info {
    char device_name[256];              /* Device name */
    char mount_point[1024];             /* Mount point */
    char fs_type[32];                   /* Filesystem type */
    ext2_mount_options_t options;       /* Mount options */
    uint32_t mount_time;                /* Mount time */
    uint32_t access_time;               /* Last access time */
    bool read_only;                     /* Read-only mount */
    bool needs_recovery;                /* Needs journal recovery */
    uint32_t reserved[8];               /* Reserved for future use */
} ext2_mount_info_t;

/* System Call Handler Functions */
int sys_ext2_mount(const char* device, const char* mount_point, 
                   const ext2_mount_options_t* options);
int sys_ext2_unmount(const char* mount_point, bool force);
int sys_ext2_format(const ext2_format_options_t* options);
int sys_ext2_fsck(const ext2_fsck_options_t* options, ext2_fsck_results_t* results);
int sys_ext2_get_info(const char* device, ext2_fs_info_user_t* info);
int sys_ext2_set_label(const char* device, const char* label);
int sys_ext2_get_label(const char* device, char* label, size_t label_size);
int sys_ext2_resize(const char* device, uint64_t new_size);
int sys_ext2_defrag(const char* device, bool verbose);
int sys_ext2_tune(const ext2_tune_options_t* options);
int sys_ext2_backup_super(const char* device, const char* backup_file);
int sys_ext2_restore_super(const char* device, const char* backup_file);
int sys_ext2_list_mounts(ext2_mount_info_t* mounts, uint32_t* count);
int sys_ext2_get_mount_info(const char* mount_point, ext2_mount_info_t* info);
int sys_ext2_set_mount_opts(const char* mount_point, const ext2_mount_options_t* options);

#endif /* EXT2_SYSCALLS_H */
