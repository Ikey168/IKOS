/* IKOS Virtual File System (VFS) Header
 * Provides a unified interface for different filesystem types
 */

#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Type definitions for compatibility */
typedef long long ssize_t;              /* Signed size type */

/* VFS Configuration */
#define VFS_MAX_PATH_LENGTH         1024    /* Maximum path length */
#define VFS_MAX_FILENAME_LENGTH     255     /* Maximum filename length */
#define VFS_MAX_MOUNT_POINTS        32      /* Maximum mount points */
#define VFS_MAX_OPEN_FILES          1024    /* Maximum open files globally */
#define VFS_MAX_FILESYSTEMS         16      /* Maximum registered filesystems */
#define VFS_MAX_DEVICES             64      /* Maximum block devices */

/* File types */
typedef enum {
    VFS_FILE_TYPE_REGULAR = 0,      /* Regular file */
    VFS_FILE_TYPE_DIRECTORY,        /* Directory */
    VFS_FILE_TYPE_SYMLINK,          /* Symbolic link */
    VFS_FILE_TYPE_CHARDEV,          /* Character device */
    VFS_FILE_TYPE_BLOCKDEV,         /* Block device */
    VFS_FILE_TYPE_FIFO,             /* Named pipe (FIFO) */
    VFS_FILE_TYPE_SOCKET,           /* Socket */
    VFS_FILE_TYPE_UNKNOWN           /* Unknown type */
} vfs_file_type_t;

/* File access modes */
#define VFS_O_RDONLY    0x0001      /* Read only */
#define VFS_O_WRONLY    0x0002      /* Write only */
#define VFS_O_RDWR      0x0003      /* Read and write */
#define VFS_O_CREAT     0x0004      /* Create if doesn't exist */
#define VFS_O_EXCL      0x0008      /* Fail if exists (with O_CREAT) */
#define VFS_O_TRUNC     0x0010      /* Truncate to zero length */
#define VFS_O_APPEND    0x0020      /* Append mode */
#define VFS_O_NONBLOCK  0x0040      /* Non-blocking I/O */
#define VFS_O_SYNC      0x0080      /* Synchronous I/O */
#define VFS_O_DIRECTORY 0x0100      /* Must be a directory */

/* File permissions */
#define VFS_S_IRUSR     0x0100      /* Read permission for owner */
#define VFS_S_IWUSR     0x0080      /* Write permission for owner */
#define VFS_S_IXUSR     0x0040      /* Execute permission for owner */
#define VFS_S_IRGRP     0x0020      /* Read permission for group */
#define VFS_S_IWGRP     0x0010      /* Write permission for group */
#define VFS_S_IXGRP     0x0008      /* Execute permission for group */
#define VFS_S_IROTH     0x0004      /* Read permission for others */
#define VFS_S_IWOTH     0x0002      /* Write permission for others */
#define VFS_S_IXOTH     0x0001      /* Execute permission for others */

/* Permission aliases for our application loader */
#define VFS_PERM_READ   (VFS_S_IRUSR | VFS_S_IRGRP | VFS_S_IROTH)
#define VFS_PERM_WRITE  (VFS_S_IWUSR | VFS_S_IWGRP | VFS_S_IWOTH) 
#define VFS_PERM_EXEC   (VFS_S_IXUSR | VFS_S_IXGRP | VFS_S_IXOTH)

/* File handle types */
typedef int file_handle_t;
#define VFS_INVALID_HANDLE  -1

/* Seek constants */
#define VFS_SEEK_SET    0           /* Seek from beginning */
#define VFS_SEEK_CUR    1           /* Seek from current position */
#define VFS_SEEK_END    2           /* Seek from end */

/* Error codes */
#define VFS_SUCCESS             0   /* Operation successful */
#define VFS_ERROR_INVALID_PARAM -1  /* Invalid parameter */
#define VFS_ERROR_NOT_FOUND     -2  /* File or directory not found */
#define VFS_ERROR_PERMISSION    -3  /* Permission denied */
#define VFS_ERROR_EXISTS        -4  /* File already exists */
#define VFS_ERROR_NOT_DIRECTORY -5  /* Not a directory */
#define VFS_ERROR_IS_DIRECTORY  -6  /* Is a directory */
#define VFS_ERROR_NO_SPACE      -7  /* No space left on device */
#define VFS_ERROR_READ_ONLY     -8  /* Read-only filesystem */
#define VFS_ERROR_NAME_TOO_LONG -9  /* Filename too long */
#define VFS_ERROR_NO_MEMORY     -10 /* Out of memory */
#define VFS_ERROR_IO_ERROR      -11 /* I/O error */
#define VFS_ERROR_NOT_SUPPORTED -12 /* Operation not supported */
#define VFS_ERROR_BUSY          -13 /* Device or resource busy */
#define VFS_ERROR_CROSS_DEVICE  -14 /* Cross-device link */

/* Forward declarations */
struct vfs_inode;
struct vfs_file;
struct vfs_dentry;
struct vfs_mount;
struct vfs_filesystem;
struct vfs_superblock;

/* File attributes structure */
typedef struct vfs_stat {
    uint64_t st_ino;                /* Inode number */
    vfs_file_type_t st_mode;        /* File type */
    uint32_t st_perm;               /* File permissions */
    uint32_t permissions;           /* Alias for st_perm for compatibility */
    uint32_t st_nlink;              /* Number of hard links */
    uint32_t st_uid;                /* User ID */
    uint32_t st_gid;                /* Group ID */
    uint64_t st_size;               /* File size in bytes */
    uint64_t size;                  /* Alias for st_size for compatibility */
    uint64_t st_blocks;             /* Number of blocks allocated */
    uint32_t st_blksize;            /* Block size */
    uint64_t st_atime;              /* Last access time */
    uint64_t st_mtime;              /* Last modification time */
    uint64_t st_ctime;              /* Last status change time */
    uint32_t st_dev;                /* Device ID */
    uint32_t st_rdev;               /* Device ID for special files */
} vfs_stat_t;

/* Directory entry structure */
typedef struct vfs_dirent {
    uint64_t d_ino;                         /* Inode number */
    uint32_t d_reclen;                      /* Record length */
    vfs_file_type_t d_type;                 /* File type */
    char d_name[VFS_MAX_FILENAME_LENGTH];   /* Filename */
} vfs_dirent_t;

/* Inode operations structure */
typedef struct vfs_inode_operations {
    /* File operations */
    int (*create)(struct vfs_inode* dir, struct vfs_dentry* dentry, 
                  uint32_t mode, bool excl);
    int (*link)(struct vfs_dentry* old_dentry, struct vfs_inode* dir, 
                struct vfs_dentry* dentry);
    int (*unlink)(struct vfs_inode* dir, struct vfs_dentry* dentry);
    int (*symlink)(struct vfs_inode* dir, struct vfs_dentry* dentry, 
                   const char* symname);
    int (*rename)(struct vfs_inode* old_dir, struct vfs_dentry* old_dentry,
                  struct vfs_inode* new_dir, struct vfs_dentry* new_dentry);
    
    /* Directory operations */
    int (*mkdir)(struct vfs_inode* dir, struct vfs_dentry* dentry, uint32_t mode);
    int (*rmdir)(struct vfs_inode* dir, struct vfs_dentry* dentry);
    
    /* Lookup operations */
    struct vfs_dentry* (*lookup)(struct vfs_inode* dir, struct vfs_dentry* dentry);
    
    /* Attribute operations */
    int (*getattr)(struct vfs_dentry* dentry, vfs_stat_t* stat);
    int (*setattr)(struct vfs_dentry* dentry, const vfs_stat_t* stat);
    
    /* Permission check */
    int (*permission)(struct vfs_inode* inode, int mask);
} vfs_inode_operations_t;

/* File operations structure */
typedef struct vfs_file_operations {
    /* I/O operations */
    ssize_t (*read)(struct vfs_file* file, char* buffer, size_t count, uint64_t* pos);
    ssize_t (*write)(struct vfs_file* file, const char* buffer, size_t count, uint64_t* pos);
    int (*flush)(struct vfs_file* file);
    int (*fsync)(struct vfs_file* file);
    
    /* File management */
    int (*open)(struct vfs_inode* inode, struct vfs_file* file);
    int (*release)(struct vfs_inode* inode, struct vfs_file* file);
    
    /* Directory operations */
    int (*readdir)(struct vfs_file* file, vfs_dirent_t* dirent);
    
    /* Seek operations */
    uint64_t (*llseek)(struct vfs_file* file, uint64_t offset, int whence);
    
    /* Memory mapping (future) */
    int (*mmap)(struct vfs_file* file, void* addr, size_t length, int prot, int flags);
} vfs_file_operations_t;

/* Superblock operations structure */
typedef struct vfs_superblock_operations {
    /* Inode management */
    struct vfs_inode* (*alloc_inode)(struct vfs_superblock* sb);
    void (*destroy_inode)(struct vfs_inode* inode);
    
    /* Filesystem operations */
    int (*write_super)(struct vfs_superblock* sb);
    int (*sync_fs)(struct vfs_superblock* sb);
    int (*statfs)(struct vfs_superblock* sb, vfs_stat_t* stat);
    
    /* Mount/unmount operations */
    int (*remount_fs)(struct vfs_superblock* sb, int* flags, char* data);
    void (*umount_begin)(struct vfs_superblock* sb);
} vfs_superblock_operations_t;

/* Inode structure */
typedef struct vfs_inode {
    uint64_t i_ino;                     /* Inode number */
    vfs_file_type_t i_mode;             /* File type */
    uint32_t i_perm;                    /* File permissions */
    uint32_t i_uid;                     /* User ID */
    uint32_t i_gid;                     /* Group ID */
    uint32_t i_nlink;                   /* Number of hard links */
    uint64_t i_size;                    /* File size */
    uint64_t i_blocks;                  /* Number of blocks */
    uint32_t i_blksize;                 /* Block size */
    uint64_t i_atime;                   /* Last access time */
    uint64_t i_mtime;                   /* Last modification time */
    uint64_t i_ctime;                   /* Last status change time */
    
    struct vfs_superblock* i_sb;        /* Superblock */
    const vfs_inode_operations_t* i_op; /* Inode operations */
    const vfs_file_operations_t* i_fop; /* File operations */
    
    void* i_private;                    /* Filesystem-specific data */
    uint32_t i_state;                   /* Inode state flags */
    uint32_t i_count;                   /* Reference count */
} vfs_inode_t;

/* Directory entry structure */
typedef struct vfs_dentry {
    char d_name[VFS_MAX_FILENAME_LENGTH];   /* Entry name */
    struct vfs_inode* d_inode;              /* Associated inode */
    struct vfs_dentry* d_parent;            /* Parent directory */
    struct vfs_dentry* d_child;             /* First child */
    struct vfs_dentry* d_sibling;           /* Next sibling */
    struct vfs_mount* d_mounted;            /* Mount point (if any) */
    uint32_t d_flags;                       /* Dentry flags */
    uint32_t d_count;                       /* Reference count */
    void* d_fsdata;                         /* Filesystem-specific data */
} vfs_dentry_t;

/* File structure */
typedef struct vfs_file {
    struct vfs_dentry* f_dentry;        /* Directory entry */
    struct vfs_inode* f_inode;          /* Inode */
    const vfs_file_operations_t* f_op;  /* File operations */
    uint32_t f_flags;                   /* File flags */
    uint32_t f_mode;                    /* File mode */
    uint64_t f_pos;                     /* Current file position */
    uint32_t f_count;                   /* Reference count */
    uint32_t f_owner;                   /* Owner process ID */
    void* f_private_data;               /* Filesystem-specific data */
} vfs_file_t;

/* Superblock structure */
typedef struct vfs_superblock {
    uint32_t s_blocksize;               /* Block size */
    uint64_t s_maxbytes;                /* Maximum file size */
    uint32_t s_magic;                   /* Filesystem magic number */
    uint32_t s_flags;                   /* Mount flags */
    struct vfs_dentry* s_root;          /* Root directory entry */
    const vfs_superblock_operations_t* s_op; /* Superblock operations */
    struct vfs_filesystem* s_type;      /* Filesystem type */
    void* s_fs_info;                    /* Filesystem-specific info */
    char s_id[32];                      /* Filesystem identifier */
} vfs_superblock_t;

/* Mount structure */
typedef struct vfs_mount {
    struct vfs_dentry* mnt_mountpoint;  /* Mount point dentry */
    struct vfs_dentry* mnt_root;        /* Root of mounted filesystem */
    struct vfs_superblock* mnt_sb;      /* Superblock */
    struct vfs_mount* mnt_parent;       /* Parent mount */
    struct vfs_mount* mnt_next;         /* Next mount in list */
    char mnt_devname[64];               /* Device name */
    char mnt_dirname[VFS_MAX_PATH_LENGTH]; /* Mount point path */
    uint32_t mnt_flags;                 /* Mount flags */
    uint32_t mnt_count;                 /* Reference count */
} vfs_mount_t;

/* Filesystem type structure */
typedef struct vfs_filesystem {
    const char* name;                   /* Filesystem name */
    uint32_t fs_flags;                  /* Filesystem flags */
    
    /* Mount operations */
    struct vfs_superblock* (*mount)(struct vfs_filesystem* fs_type,
                                   uint32_t flags, const char* dev_name,
                                   void* data);
    void (*kill_sb)(struct vfs_superblock* sb);
    
    /* Module information */
    struct vfs_filesystem* next;        /* Next in list */
    uint32_t fs_supers;                 /* Number of superblocks */
} vfs_filesystem_t;

/* Block device structure */
typedef struct vfs_block_device {
    uint32_t bd_major;                  /* Major device number */
    uint32_t bd_minor;                  /* Minor device number */
    uint64_t bd_size;                   /* Device size in bytes */
    uint32_t bd_block_size;             /* Block size */
    char bd_name[32];                   /* Device name */
    
    /* I/O operations */
    int (*bd_read)(struct vfs_block_device* bdev, uint64_t sector,
                   void* buffer, size_t count);
    int (*bd_write)(struct vfs_block_device* bdev, uint64_t sector,
                    const void* buffer, size_t count);
    int (*bd_flush)(struct vfs_block_device* bdev);
    
    void* bd_private;                   /* Device-specific data */
    uint32_t bd_flags;                  /* Device flags */
} vfs_block_device_t;

/* ================================
 * VFS Core Functions
 * ================================ */

/* VFS initialization and management */
int vfs_init(void);
void vfs_shutdown(void);

/* Filesystem registration */
int vfs_register_filesystem(vfs_filesystem_t* fs);
int vfs_unregister_filesystem(vfs_filesystem_t* fs);

/* Mount operations */
int vfs_mount(const char* dev_name, const char* dir_name, 
              const char* fs_type, uint32_t flags, void* data);
int vfs_umount(const char* dir_name);
vfs_mount_t* vfs_get_mount(const char* path);

/* File operations */
int vfs_open(const char* path, uint32_t flags, uint32_t mode);
int vfs_close(int fd);
ssize_t vfs_read(int fd, void* buffer, size_t count);
ssize_t vfs_write(int fd, const void* buffer, size_t count);
int vfs_flush(int fd);
int vfs_fsync(int fd);
uint64_t vfs_lseek(int fd, uint64_t offset, int whence);

/* Directory operations */
int vfs_mkdir(const char* path, uint32_t mode);
int vfs_rmdir(const char* path);
int vfs_opendir(const char* path);
int vfs_readdir(int fd, vfs_dirent_t* dirent);
int vfs_closedir(int fd);

/* File management */
int vfs_unlink(const char* path);
int vfs_link(const char* oldpath, const char* newpath);
int vfs_symlink(const char* target, const char* linkpath);
int vfs_rename(const char* oldpath, const char* newpath);

/* File attributes */
int vfs_stat(const char* path, vfs_stat_t* stat);
int vfs_fstat(int fd, vfs_stat_t* stat);
int vfs_chmod(const char* path, uint32_t mode);
int vfs_chown(const char* path, uint32_t uid, uint32_t gid);

/* Path operations */
char* vfs_getcwd(char* buf, size_t size);
int vfs_chdir(const char* path);
char* vfs_realpath(const char* path, char* resolved_path);

/* ================================
 * VFS Internal Functions
 * ================================ */

/* Path resolution */
vfs_dentry_t* vfs_path_lookup(const char* path, uint32_t flags);
char* vfs_normalize_path(const char* path);
int vfs_path_walk(const char* path, vfs_dentry_t** dentry);

/* Dentry operations */
vfs_dentry_t* vfs_alloc_dentry(const char* name);
void vfs_free_dentry(vfs_dentry_t* dentry);
vfs_dentry_t* vfs_dentry_lookup(vfs_dentry_t* parent, const char* name);

/* Inode operations */
vfs_inode_t* vfs_alloc_inode(vfs_superblock_t* sb);
void vfs_free_inode(vfs_inode_t* inode);
int vfs_inode_permission(vfs_inode_t* inode, int mask);

/* File descriptor management */
int vfs_alloc_fd(void);
void vfs_free_fd(int fd);
vfs_file_t* vfs_get_file(int fd);
int vfs_install_fd(int fd, vfs_file_t* file);

/* Block device operations */
int vfs_register_blkdev(uint32_t major, const char* name, 
                        const vfs_file_operations_t* fops);
int vfs_unregister_blkdev(uint32_t major, const char* name);
vfs_block_device_t* vfs_get_blkdev(uint32_t major, uint32_t minor);

/* ================================
 * VFS Debugging and Statistics
 * ================================ */

/* VFS statistics */
typedef struct vfs_stats {
    uint32_t open_files;                /* Number of open files */
    uint32_t active_dentries;           /* Number of active dentries */
    uint32_t active_inodes;             /* Number of active inodes */
    uint32_t mounted_filesystems;       /* Number of mounted filesystems */
    uint64_t total_reads;               /* Total read operations */
    uint64_t total_writes;              /* Total write operations */
    uint64_t bytes_read;                /* Total bytes read */
    uint64_t bytes_written;             /* Total bytes written */
} vfs_stats_t;

void vfs_get_stats(vfs_stats_t* stats);
void vfs_reset_stats(void);

/* Debugging functions */
void vfs_dump_mounts(void);
void vfs_dump_open_files(void);
void vfs_dump_dentry_cache(void);

#endif /* VFS_H */
