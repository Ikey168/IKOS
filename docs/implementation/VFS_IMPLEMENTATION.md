# IKOS Virtual File System (VFS) Implementation

## Overview

The IKOS Virtual File System (VFS) provides a unified interface layer for different filesystem types, enabling the kernel to work with multiple filesystem implementations through a common API. This implementation addresses Issue #22 by providing mount points, file operations, and support for multiple filesystem backends.

## Architecture

### Core Components

1. **VFS Layer** (`vfs.c`, `vfs.h`)
   - Central abstraction layer
   - Mount point management
   - File descriptor management
   - Path resolution
   - Filesystem registration

2. **RAM Filesystem** (`ramfs.c`)
   - In-memory filesystem implementation
   - Proof-of-concept backend
   - Demonstrates VFS interface usage

3. **Test Suite** (`test_vfs.c`)
   - Comprehensive testing framework
   - Validates all VFS functionality
   - Error condition testing

## Design Principles

### 1. Layered Architecture

```
Application Layer
    |
System Call Interface
    |
VFS Layer (vfs.c)
    |
Filesystem Backends (ramfs.c, future: ext2, fat32, etc.)
    |
Block Device Layer
```

### 2. Object-Oriented Design

The VFS uses C structures with function pointers to provide object-oriented behavior:

- **Superblock**: Represents a mounted filesystem instance
- **Inode**: Represents a file or directory
- **Dentry**: Directory entry, provides name-to-inode mapping
- **File**: Represents an open file descriptor

### 3. Operation Tables

Each object type has associated operation tables:

- `vfs_super_operations_t`: Superblock operations (alloc/destroy inodes, etc.)
- `vfs_inode_operations_t`: Inode operations (lookup, create, mkdir, etc.)
- `vfs_file_operations_t`: File operations (read, write, open, close, etc.)

## Key Features

### Mount Point Support

The VFS supports hierarchical mount points:

```c
// Mount root filesystem
vfs_mount("/dev/ram0", "/", "ramfs", 0, NULL);

// Mount additional filesystem
vfs_mount("/dev/sda1", "/home", "ext2", 0, NULL);
```

Mount points are tracked in a linked list, with path resolution finding the most specific mount for any given path.

### File Operations

Standard POSIX-like file operations:

```c
int fd = vfs_open("/path/to/file", VFS_O_RDWR | VFS_O_CREAT, 0644);
ssize_t bytes_read = vfs_read(fd, buffer, sizeof(buffer));
ssize_t bytes_written = vfs_write(fd, data, strlen(data));
vfs_close(fd);
```

### Path Resolution

The VFS provides path resolution that:
- Handles absolute and relative paths
- Traverses mount points correctly
- Supports symbolic links (framework in place)
- Resolves "." and ".." directory entries

### Filesystem Registration

New filesystem types can be registered:

```c
vfs_filesystem_t my_fs = {
    .name = "myfs",
    .mount = my_fs_mount,
    .kill_sb = my_fs_kill_sb,
};

vfs_register_filesystem(&my_fs);
```

## Data Structures

### Core VFS Structures

#### Superblock (`vfs_superblock_t`)
```c
typedef struct vfs_superblock {
    uint32_t s_magic;                    // Filesystem magic number
    struct vfs_filesystem* s_type;       // Filesystem type
    struct vfs_super_operations* s_op;   // Superblock operations
    struct vfs_dentry* s_root;           // Root dentry
    void* s_private;                     // Filesystem-specific data
} vfs_superblock_t;
```

#### Inode (`vfs_inode_t`)
```c
typedef struct vfs_inode {
    uint64_t i_ino;                      // Inode number
    uint32_t i_mode;                     // File type and permissions
    uint32_t i_nlink;                    // Hard link count
    uint64_t i_size;                     // File size in bytes
    uint32_t i_uid, i_gid;               // Owner and group IDs
    time_t i_atime, i_mtime, i_ctime;    // Access, modify, change times
    struct vfs_superblock* i_sb;         // Superblock pointer
    struct vfs_inode_operations* i_op;   // Inode operations
    struct vfs_file_operations* i_fop;   // File operations
    void* i_private;                     // Filesystem-specific data
    uint32_t i_count;                    // Reference count
} vfs_inode_t;
```

#### Dentry (`vfs_dentry_t`)
```c
typedef struct vfs_dentry {
    char d_name[VFS_MAX_NAME_LEN];       // Entry name
    struct vfs_inode* d_inode;           // Associated inode
    struct vfs_dentry* d_parent;         // Parent dentry
    struct vfs_dentry* d_subdirs;        // Child dentries
    struct vfs_dentry* d_next;           // Sibling dentry
    struct vfs_mount* d_mounted;         // Mount point (if any)
    struct vfs_superblock* d_sb;         // Superblock
    uint32_t d_count;                    // Reference count
    uint32_t d_flags;                    // Dentry flags
} vfs_dentry_t;
```

#### Mount (`vfs_mount_t`)
```c
typedef struct vfs_mount {
    struct vfs_superblock* mnt_sb;       // Mounted superblock
    struct vfs_dentry* mnt_root;         // Root of mounted tree
    struct vfs_dentry* mnt_mountpoint;   // Mount point dentry
    struct vfs_mount* mnt_parent;        // Parent mount
    struct vfs_mount* mnt_next;          // Next mount in list
    char mnt_devname[VFS_MAX_PATH];      // Device name
    char mnt_dirname[VFS_MAX_PATH];      // Mount point path
    uint32_t mnt_flags;                  // Mount flags
    uint32_t mnt_count;                  // Reference count
} vfs_mount_t;
```

### RAM Filesystem Structures

#### RAM Filesystem Inode Info
```c
typedef struct ramfs_inode_info {
    uint8_t* data;                       // File data buffer
    size_t size;                         // Current file size
    size_t capacity;                     // Buffer capacity
    bool is_directory;                   // Directory flag
} ramfs_inode_info_t;
```

## API Reference

### Initialization Functions

```c
int vfs_init(void);                      // Initialize VFS subsystem
void vfs_shutdown(void);                 // Shutdown VFS subsystem
```

### Filesystem Registration

```c
int vfs_register_filesystem(vfs_filesystem_t* fs);     // Register filesystem type
int vfs_unregister_filesystem(vfs_filesystem_t* fs);   // Unregister filesystem type
```

### Mount Operations

```c
int vfs_mount(const char* dev_name, const char* dir_name, 
              const char* fs_type, uint32_t flags, void* data);
int vfs_umount(const char* dir_name);
vfs_mount_t* vfs_get_mount(const char* path);
```

### File Operations

```c
int vfs_open(const char* path, uint32_t flags, uint32_t mode);
int vfs_close(int fd);
ssize_t vfs_read(int fd, void* buffer, size_t count);
ssize_t vfs_write(int fd, const void* buffer, size_t count);
```

### Path Resolution

```c
vfs_dentry_t* vfs_path_lookup(const char* path, uint32_t flags);
```

### Dentry Management

```c
vfs_dentry_t* vfs_alloc_dentry(const char* name);
void vfs_free_dentry(vfs_dentry_t* dentry);
void vfs_dentry_add_child(vfs_dentry_t* parent, vfs_dentry_t* child);
```

### Inode Management

```c
vfs_inode_t* vfs_alloc_inode(vfs_superblock_t* sb);
void vfs_free_inode(vfs_inode_t* inode);
```

### File Descriptor Management

```c
int vfs_alloc_fd(void);
void vfs_free_fd(int fd);
void vfs_install_fd(int fd, vfs_file_t* file);
vfs_file_t* vfs_get_file(int fd);
```

### Statistics

```c
void vfs_get_stats(vfs_stats_t* stats);
```

## Constants and Limits

```c
#define VFS_MAX_OPEN_FILES      256      // Maximum open files per process
#define VFS_MAX_PATH            4096     // Maximum path length
#define VFS_MAX_NAME_LEN        256      // Maximum filename length
#define VFS_MAX_MOUNTS          64       // Maximum number of mounts
```

## File Types

```c
#define VFS_FILE_TYPE_UNKNOWN       0
#define VFS_FILE_TYPE_REGULAR       1
#define VFS_FILE_TYPE_DIRECTORY     2
#define VFS_FILE_TYPE_CHAR_DEVICE   3
#define VFS_FILE_TYPE_BLOCK_DEVICE  4
#define VFS_FILE_TYPE_FIFO          5
#define VFS_FILE_TYPE_SOCKET        6
#define VFS_FILE_TYPE_SYMLINK       7
```

## Open Flags

```c
#define VFS_O_RDONLY        0x0000
#define VFS_O_WRONLY        0x0001
#define VFS_O_RDWR          0x0002
#define VFS_O_CREAT         0x0040
#define VFS_O_EXCL          0x0080
#define VFS_O_TRUNC         0x0200
#define VFS_O_APPEND        0x0400
#define VFS_O_NONBLOCK      0x0800
#define VFS_O_SYNC          0x1000
#define VFS_O_DIRECTORY     0x10000
```

## Error Codes

```c
#define VFS_SUCCESS             0
#define VFS_ERROR_INVALID_PARAM -1
#define VFS_ERROR_NO_MEMORY     -2
#define VFS_ERROR_NOT_FOUND     -3
#define VFS_ERROR_EXISTS        -4
#define VFS_ERROR_NOT_DIRECTORY -5
#define VFS_ERROR_IS_DIRECTORY  -6
#define VFS_ERROR_NOT_SUPPORTED -7
#define VFS_ERROR_PERMISSION    -8
#define VFS_ERROR_NO_SPACE      -9
#define VFS_ERROR_IO_ERROR      -10
#define VFS_ERROR_BUSY          -11
```

## Implementation Details

### Thread Safety

The current implementation uses simple spinlocks for thread safety:

```c
#define VFS_LOCK(lock) while (__sync_lock_test_and_set(&(lock), 1)) { /* spin */ }
#define VFS_UNLOCK(lock) __sync_lock_release(&(lock))
```

Separate locks protect different subsystems:
- `vfs_mount_lock`: Protects mount list
- `vfs_fd_lock`: Protects file descriptor table
- `vfs_fs_lock`: Protects filesystem type list

### Memory Management

The VFS uses kernel memory allocation functions:
- `kmalloc(size)`: Allocate kernel memory
- `kfree(ptr)`: Free kernel memory

Currently includes placeholder implementations for testing.

### Statistics Tracking

The VFS tracks usage statistics:

```c
typedef struct vfs_stats {
    uint64_t mounted_filesystems;
    uint64_t open_files;
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t bytes_read;
    uint64_t bytes_written;
} vfs_stats_t;
```

## Testing

### Test Coverage

The test suite covers:

1. **VFS Initialization**: Initialize/shutdown cycles
2. **Filesystem Registration**: Register/unregister filesystem types
3. **Mount Operations**: Mount/unmount filesystems
4. **File Operations**: Open/close/read/write files
5. **Path Resolution**: Lookup paths and handle errors
6. **Dentry Management**: Allocation and hierarchy management
7. **Inode Management**: Allocation and cleanup
8. **File Descriptor Management**: FD allocation and tracking
9. **RAM Filesystem**: Basic functionality testing
10. **Statistics**: Usage tracking
11. **Error Conditions**: Invalid parameters and edge cases

### Running Tests

```bash
# Build and run VFS tests
make test-vfs

# Build VFS components only
make vfs

# Run smoke tests
make vfs-smoke
```

## Future Enhancements

### Additional Filesystem Support

The VFS framework is designed to support additional filesystem types:

1. **Block-based Filesystems**: ext2, ext3, ext4
2. **Network Filesystems**: NFS, CIFS
3. **Special Filesystems**: procfs, sysfs, devfs
4. **Pseudo-filesystems**: tmpfs, pipefs

### Advanced Features

1. **Symbolic Links**: Full symlink support
2. **File Locking**: Advisory and mandatory locking
3. **Extended Attributes**: Metadata support
4. **Access Control Lists**: Fine-grained permissions
5. **Journaling**: Transaction support for reliability
6. **Caching**: Buffer cache and page cache integration

### Performance Optimizations

1. **Dentry Cache**: Hash table for faster lookups
2. **Inode Cache**: LRU cache for frequently accessed inodes
3. **Read-ahead**: Predictive reading for sequential access
4. **Write-behind**: Asynchronous write operations

## Integration with IKOS

### System Call Interface

The VFS integrates with the IKOS system call interface:

```c
// System call handlers
int sys_open(const char* pathname, int flags, mode_t mode);
int sys_close(int fd);
ssize_t sys_read(int fd, void* buf, size_t count);
ssize_t sys_write(int fd, const void* buf, size_t count);
```

### Process Management Integration

- File descriptor tables per process
- Working directory tracking
- File descriptor inheritance on fork()
- Cleanup on process exit

### Memory Management Integration

- Integration with VMM for file mapping
- Buffer cache management
- Page cache for file data

## Conclusion

The IKOS VFS implementation provides a solid foundation for filesystem support in the IKOS kernel. It successfully addresses Issue #22 by providing:

1. **Mount Points**: Hierarchical filesystem mounting
2. **File Operations**: Standard file I/O interface
3. **Multiple Backends**: Framework for different filesystem types

The modular design allows for easy extension with additional filesystem types and advanced features as the kernel evolves.
