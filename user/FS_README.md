# IKOS User-Space File System API

This directory contains the user-space file system API and examples for the IKOS operating system. This implementation provides a comprehensive interface for file and directory operations, commands, and example applications.

## Overview

The IKOS file system API consists of several components:

- **fs_user_api.h/c** - Core filesystem API providing POSIX-like operations
- **fs_commands.h/c** - Command-line utilities (mkdir, ls, cp, etc.)
- **fs_test.c** - Comprehensive test suite
- **file_manager_example.c** - Interactive file manager application
- **simple_editor.c** - Basic text editor using the API

## Features

### Directory Operations
- `fs_mkdir()` - Create directories
- `fs_rmdir()` - Remove empty directories
- `fs_chdir()` - Change current directory
- `fs_getcwd()` - Get current working directory
- `fs_ls()` - List directory contents
- `fs_opendir()`, `fs_readdir()`, `fs_closedir()` - Directory iteration

### File Operations
- `fs_open()`, `fs_close()` - Open and close files
- `fs_read()`, `fs_write()` - Read and write file data
- `fs_lseek()`, `fs_tell()` - File positioning
- `fs_unlink()` - Delete files
- `fs_rename()` - Rename/move files
- `fs_copy()` - Copy files
- `fs_touch()` - Create empty files or update timestamps

### File Information and Metadata
- `fs_stat()`, `fs_fstat()` - Get file statistics
- `fs_chmod()` - Change file permissions
- `fs_exists()` - Check if file exists
- `fs_is_file()`, `fs_is_directory()` - Check file type
- `fs_size()` - Get file size
- `fs_can_read()`, `fs_can_write()`, `fs_can_execute()` - Check permissions

### Path Utilities
- `fs_basename()`, `fs_dirname()` - Extract filename and directory parts
- `fs_realpath()` - Resolve absolute path
- `fs_split_path()` - Split path into directory and filename
- `fs_normalize_path()` - Normalize path format

### Convenience Functions
- `fs_create_file()` - Create an empty file
- `fs_read_file()`, `fs_write_file()` - Read/write entire files
- `fs_append_file()` - Append data to files

### Command-Line Interface
All standard filesystem commands are implemented:
- `mkdir` - Create directories (with -p option)
- `rmdir` - Remove directories
- `ls` - List files (with -l and -a options)
- `cd` - Change directory
- `pwd` - Print working directory
- `touch` - Create files or update timestamps
- `rm` - Remove files (with -r for recursive)
- `cp` - Copy files
- `mv` - Move/rename files
- `cat` - Display file contents
- `echo` - Write text (with > redirection)
- `stat` - Show file information
- `chmod` - Change permissions
- `find` - Find files by name
- `help` - Show command help

## Building

Use the provided Makefile to build all components:

```bash
# Build everything
make all

# Build just the filesystem library
make fs

# Build example programs
make examples

# Build and run tests
make test

# Show all available targets
make help
```

### Individual Targets

```bash
# File system library
make libfs_user.a

# Test program
make fs_test

# Example applications
make file_manager
make simple_editor

# IPC compatibility (for standalone testing)
make ipc
```

## Usage Examples

### Basic File Operations

```c
#include "fs_user_api.h"

int main() {
    // Initialize filesystem
    fs_init_cwd();
    
    // Create a directory
    fs_mkdir("/tmp/test", FS_PERM_755);
    
    // Create and write to a file
    const char* data = "Hello, IKOS!";
    fs_write_file("/tmp/test/hello.txt", data, strlen(data));
    
    // Read the file back
    char buffer[256];
    ssize_t bytes_read = fs_read_file("/tmp/test/hello.txt", buffer, sizeof(buffer));
    buffer[bytes_read] = '\\0';
    printf("File contents: %s\\n", buffer);
    
    // Check file information
    vfs_stat_t stat;
    if (fs_stat("/tmp/test/hello.txt", &stat) == 0) {
        printf("File size: %llu bytes\\n", stat.st_size);
    }
    
    // List directory
    fs_dirent_t entries[10];
    int count = fs_ls("/tmp/test", entries, 10);
    for (int i = 0; i < count; i++) {
        printf("Found: %s\\n", entries[i].name);
    }
    
    // Cleanup
    fs_unlink("/tmp/test/hello.txt");
    fs_rmdir("/tmp/test");
    
    return 0;
}
```

### Using Commands

```c
#include "fs_commands.h"

int main() {
    fs_init_cwd();
    
    // Execute filesystem commands
    fs_execute_command("mkdir /projects");
    fs_execute_command("echo 'Hello World' > /projects/readme.txt");
    fs_execute_command("ls -l /projects");
    fs_execute_command("cat /projects/readme.txt");
    
    // Start interactive shell
    fs_shell();
    
    return 0;
}
```

## Running Examples

### File Manager
Interactive file manager with menu-driven interface:

```bash
make demo-file-manager
# or
./file_manager
```

Features:
- Browse directories
- Create files and directories
- Copy, move, delete operations
- View file information
- Built-in filesystem shell

### Simple Text Editor
Basic text editor with line-based editing:

```bash
make demo-editor
# or
./simple_editor [filename]
```

Commands:
- `l` - List lines
- `i <line> <text>` - Insert text
- `e <line> <text>` - Edit line
- `d <line>` - Delete line
- `s [filename]` - Save file
- `f <pattern>` - Find text
- `r <old> <new>` - Replace text

### Test Suite
Comprehensive test suite covering all functionality:

```bash
make test-fs
# or
./fs_test
```

Tests include:
- Directory operations
- File I/O
- Metadata operations
- Path utilities
- Command interface
- Error handling

## API Reference

### File Flags
- `FS_O_RDONLY` - Read only
- `FS_O_WRONLY` - Write only
- `FS_O_RDWR` - Read and write
- `FS_O_CREAT` - Create if not exists
- `FS_O_EXCL` - Fail if exists
- `FS_O_TRUNC` - Truncate to zero length
- `FS_O_APPEND` - Append mode

### File Permissions
- `FS_PERM_RUSR`, `FS_PERM_WUSR`, `FS_PERM_XUSR` - User permissions
- `FS_PERM_RGRP`, `FS_PERM_WGRP`, `FS_PERM_XGRP` - Group permissions
- `FS_PERM_ROTH`, `FS_PERM_WOTH`, `FS_PERM_XOTH` - Other permissions
- `FS_PERM_644`, `FS_PERM_755` - Common permission combinations

### Seek Origins
- `FS_SEEK_SET` - From beginning
- `FS_SEEK_CUR` - From current position
- `FS_SEEK_END` - From end

### File Types
- `VFS_FILE_TYPE_REGULAR` - Regular file
- `VFS_FILE_TYPE_DIRECTORY` - Directory
- `VFS_FILE_TYPE_SYMLINK` - Symbolic link
- `VFS_FILE_TYPE_CHARDEV` - Character device
- `VFS_FILE_TYPE_BLOCKDEV` - Block device

## Error Handling

All functions return appropriate error codes defined in VFS:
- `VFS_SUCCESS` - Operation successful
- `VFS_ERROR_NOT_FOUND` - File not found
- `VFS_ERROR_PERMISSION` - Permission denied
- `VFS_ERROR_EXISTS` - File already exists
- `VFS_ERROR_INVALID_PARAM` - Invalid parameter

Use `fs_get_last_error()` to get the last error code and `fs_error_string()` to get human-readable error messages.

## Integration with IKOS

This user-space API interfaces with the IKOS VFS subsystem through system calls. The implementation includes:

- Compatibility layer for standalone testing (ipc_syscall_stubs.c)
- Integration with existing VFS infrastructure
- Support for all VFS file types and operations
- POSIX-like interface for familiar usage

## Testing

The test suite validates:
- ✓ All directory operations
- ✓ File creation, reading, writing
- ✓ File positioning and seeking
- ✓ Metadata operations
- ✓ Path utilities
- ✓ Command interface
- ✓ Error handling
- ✓ Permission management

Run `make test-fs` to execute all tests.

## Contributing

When extending the filesystem API:

1. Add new functions to `fs_user_api.h` and implement in `fs_user_api.c`
2. Add corresponding commands to `fs_commands.c` if applicable
3. Update tests in `fs_test.c`
4. Add examples demonstrating new functionality
5. Update this README and inline documentation

## Files Overview

| File | Purpose |
|------|---------|
| `fs_user_api.h` | Main API header with function declarations |
| `fs_user_api.c` | Core filesystem API implementation |
| `fs_commands.h` | Command interface header |
| `fs_commands.c` | Command-line utilities implementation |
| `fs_test.c` | Comprehensive test suite |
| `file_manager_example.c` | Interactive file manager demo |
| `simple_editor.c` | Text editor example |
| `ipc_syscall_stubs.c` | Compatibility stubs for testing |
| `Makefile` | Build system configuration |
| `README.md` | This documentation |

## Dependencies

- IKOS VFS subsystem (`vfs.h`, `vfs.c`)
- Standard C library functions
- POSIX-like types and constants

The implementation is designed to be portable and can be adapted for other operating systems with minimal changes to the underlying VFS interface layer.
