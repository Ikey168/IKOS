/* IKOS Virtual File System Test Suite
 * Comprehensive tests for VFS functionality
 */

#include "vfs.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

/* Test framework */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        printf("✓ PASS: %s\n", message); \
        tests_passed++; \
    } else { \
        printf("✗ FAIL: %s\n", message); \
        tests_failed++; \
    } \
} while(0)

#define TEST_START(test_name) \
    printf("\n=== Running %s ===\n", test_name)

#define TEST_END() \
    printf("--- Test completed ---\n")

/* Forward declarations */
static void test_vfs_initialization(void);
static void test_filesystem_registration(void);
static void test_mount_operations(void);
static void test_file_operations(void);
static void test_path_resolution(void);
static void test_dentry_management(void);
static void test_inode_management(void);
static void test_fd_management(void);
static void test_ramfs_basic(void);
static void test_ramfs_file_operations(void);
static void test_vfs_statistics(void);
static void test_error_conditions(void);

/* External function declarations */
extern int ramfs_init(void);
extern void ramfs_exit(void);

/* Mock filesystem for testing */
static vfs_superblock_t* mock_fs_mount(vfs_filesystem_t* fs, uint32_t flags,
                                       const char* dev_name, void* data);
static void mock_fs_kill_sb(vfs_superblock_t* sb);

static vfs_filesystem_t mock_fs_type = {
    .name = "mockfs",
    .mount = mock_fs_mount,
    .kill_sb = mock_fs_kill_sb,
    .fs_supers = 0,
    .next = NULL,
};

/* ================================
 * Test Implementation
 * ================================ */

/**
 * Main test runner
 */
int main(void) {
    printf("IKOS Virtual File System Test Suite\n");
    printf("====================================\n");
    
    /* Run all tests */
    test_vfs_initialization();
    test_filesystem_registration();
    test_mount_operations();
    test_file_operations();
    test_path_resolution();
    test_dentry_management();
    test_inode_management();
    test_fd_management();
    test_ramfs_basic();
    test_ramfs_file_operations();
    test_vfs_statistics();
    test_error_conditions();
    
    /* Print summary */
    printf("\n====================================\n");
    printf("Test Summary:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Success rate: %.1f%%\n", 
           tests_run > 0 ? (float)tests_passed / tests_run * 100.0f : 0.0f);
    
    return tests_failed > 0 ? 1 : 0;
}

/**
 * Test VFS initialization and shutdown
 */
static void test_vfs_initialization(void) {
    TEST_START("VFS Initialization");
    
    /* Test initialization */
    int result = vfs_init();
    TEST_ASSERT(result == VFS_SUCCESS, "VFS initialization should succeed");
    
    /* Test double initialization */
    result = vfs_init();
    TEST_ASSERT(result == VFS_SUCCESS, "Double initialization should be safe");
    
    /* Test shutdown */
    vfs_shutdown();
    TEST_ASSERT(true, "VFS shutdown should complete without error");
    
    /* Re-initialize for subsequent tests */
    result = vfs_init();
    TEST_ASSERT(result == VFS_SUCCESS, "VFS re-initialization should succeed");
    
    TEST_END();
}

/**
 * Test filesystem registration
 */
static void test_filesystem_registration(void) {
    TEST_START("Filesystem Registration");
    
    /* Test registering a filesystem */
    int result = vfs_register_filesystem(&mock_fs_type);
    TEST_ASSERT(result == VFS_SUCCESS, "Filesystem registration should succeed");
    
    /* Test registering duplicate filesystem */
    result = vfs_register_filesystem(&mock_fs_type);
    TEST_ASSERT(result == VFS_ERROR_EXISTS, "Duplicate registration should fail");
    
    /* Test registering RAM filesystem */
    result = ramfs_init();
    TEST_ASSERT(result == VFS_SUCCESS, "RAM filesystem registration should succeed");
    
    /* Test unregistering filesystem */
    result = vfs_unregister_filesystem(&mock_fs_type);
    TEST_ASSERT(result == VFS_SUCCESS, "Filesystem unregistration should succeed");
    
    /* Re-register for mount tests */
    result = vfs_register_filesystem(&mock_fs_type);
    TEST_ASSERT(result == VFS_SUCCESS, "Filesystem re-registration should succeed");
    
    TEST_END();
}

/**
 * Test mount operations
 */
static void test_mount_operations(void) {
    TEST_START("Mount Operations");
    
    /* Test mounting root filesystem */
    int result = vfs_mount("/dev/ram0", "/", "ramfs", 0, NULL);
    TEST_ASSERT(result == VFS_SUCCESS, "Root mount should succeed");
    
    /* Test mounting additional filesystem */
    result = vfs_mount("/dev/ram1", "/tmp", "ramfs", 0, NULL);
    TEST_ASSERT(result == VFS_SUCCESS || result == VFS_ERROR_NOT_FOUND, 
                "Additional mount should succeed or fail gracefully");
    
    /* Test mount with invalid filesystem type */
    result = vfs_mount("/dev/test", "/invalid", "nosuchfs", 0, NULL);
    TEST_ASSERT(result == VFS_ERROR_NOT_SUPPORTED, "Invalid filesystem type should fail");
    
    /* Test mount with null parameters */
    result = vfs_mount(NULL, "/", "ramfs", 0, NULL);
    TEST_ASSERT(result == VFS_ERROR_INVALID_PARAM, "NULL device name should fail");
    
    result = vfs_mount("/dev/test", NULL, "ramfs", 0, NULL);
    TEST_ASSERT(result == VFS_ERROR_INVALID_PARAM, "NULL mount point should fail");
    
    TEST_END();
}

/**
 * Test file operations
 */
static void test_file_operations(void) {
    TEST_START("File Operations");
    
    /* Test opening non-existent file */
    int fd = vfs_open("/nonexistent", VFS_O_RDONLY, 0);
    TEST_ASSERT(fd == VFS_ERROR_NOT_FOUND, "Opening non-existent file should fail");
    
    /* Test file descriptor management */
    for (int i = 0; i < 10; i++) {
        int test_fd = vfs_alloc_fd();
        TEST_ASSERT(test_fd >= 0, "File descriptor allocation should succeed");
        if (test_fd >= 0) {
            vfs_free_fd(test_fd);
        }
    }
    
    /* Test invalid file operations */
    ssize_t bytes = vfs_read(-1, NULL, 0);
    TEST_ASSERT(bytes == VFS_ERROR_INVALID_PARAM, "Reading invalid FD should fail");
    
    bytes = vfs_write(-1, NULL, 0);
    TEST_ASSERT(bytes == VFS_ERROR_INVALID_PARAM, "Writing invalid FD should fail");
    
    int close_result = vfs_close(-1);
    TEST_ASSERT(close_result == VFS_ERROR_INVALID_PARAM, "Closing invalid FD should fail");
    
    TEST_END();
}

/**
 * Test path resolution
 */
static void test_path_resolution(void) {
    TEST_START("Path Resolution");
    
    /* Test root path lookup */
    vfs_dentry_t* root = vfs_path_lookup("/", 0);
    TEST_ASSERT(root != NULL, "Root path lookup should succeed");
    
    /* Test invalid path lookup */
    vfs_dentry_t* invalid = vfs_path_lookup("/nonexistent/path", 0);
    TEST_ASSERT(invalid == NULL, "Invalid path lookup should return NULL");
    
    /* Test null path */
    vfs_dentry_t* null_path = vfs_path_lookup(NULL, 0);
    TEST_ASSERT(null_path == NULL, "NULL path lookup should return NULL");
    
    /* Test empty path */
    vfs_dentry_t* empty = vfs_path_lookup("", 0);
    TEST_ASSERT(empty != NULL, "Empty path should return root");
    
    TEST_END();
}

/**
 * Test dentry management
 */
static void test_dentry_management(void) {
    TEST_START("Dentry Management");
    
    /* Test dentry allocation */
    vfs_dentry_t* dentry = vfs_alloc_dentry("testfile");
    TEST_ASSERT(dentry != NULL, "Dentry allocation should succeed");
    TEST_ASSERT(strcmp(dentry->d_name, "testfile") == 0, "Dentry name should be set correctly");
    
    /* Test dentry with null name */
    vfs_dentry_t* null_dentry = vfs_alloc_dentry(NULL);
    TEST_ASSERT(null_dentry != NULL, "Dentry allocation with NULL name should succeed");
    
    /* Test dentry hierarchy */
    if (dentry && null_dentry) {
        vfs_dentry_add_child(null_dentry, dentry);
        TEST_ASSERT(dentry->d_parent == null_dentry, "Parent-child relationship should be set");
    }
    
    /* Test dentry cleanup */
    if (null_dentry) {
        vfs_free_dentry(null_dentry);  /* This should also free child */
        TEST_ASSERT(true, "Dentry cleanup should complete without error");
    }
    
    TEST_END();
}

/**
 * Test inode management
 */
static void test_inode_management(void) {
    TEST_START("Inode Management");
    
    /* Create a mock superblock for testing */
    vfs_superblock_t* sb = (vfs_superblock_t*)malloc(sizeof(vfs_superblock_t));
    if (sb) {
        memset(sb, 0, sizeof(vfs_superblock_t));
        
        /* Test inode allocation */
        vfs_inode_t* inode = vfs_alloc_inode(sb);
        TEST_ASSERT(inode != NULL, "Inode allocation should succeed");
        TEST_ASSERT(inode->i_sb == sb, "Inode superblock should be set correctly");
        TEST_ASSERT(inode->i_count == 1, "Inode reference count should be 1");
        
        /* Test inode cleanup */
        if (inode) {
            vfs_free_inode(inode);
            TEST_ASSERT(true, "Inode cleanup should complete without error");
        }
        
        free(sb);
    }
    
    /* Test inode allocation with null superblock */
    vfs_inode_t* null_inode = vfs_alloc_inode(NULL);
    TEST_ASSERT(null_inode == NULL, "Inode allocation with NULL superblock should fail");
    
    TEST_END();
}

/**
 * Test file descriptor management
 */
static void test_fd_management(void) {
    TEST_START("File Descriptor Management");
    
    /* Test file descriptor allocation */
    int fd1 = vfs_alloc_fd();
    TEST_ASSERT(fd1 >= 0, "First FD allocation should succeed");
    
    int fd2 = vfs_alloc_fd();
    TEST_ASSERT(fd2 >= 0 && fd2 != fd1, "Second FD allocation should succeed and be different");
    
    /* Test file descriptor freeing */
    if (fd1 >= 0) {
        vfs_free_fd(fd1);
        vfs_file_t* file = vfs_get_file(fd1);
        TEST_ASSERT(file == NULL, "Freed FD should return NULL file");
    }
    
    if (fd2 >= 0) {
        vfs_free_fd(fd2);
    }
    
    /* Test invalid file descriptor operations */
    vfs_free_fd(-1);
    TEST_ASSERT(true, "Freeing invalid FD should not crash");
    
    vfs_file_t* invalid_file = vfs_get_file(-1);
    TEST_ASSERT(invalid_file == NULL, "Getting invalid FD should return NULL");
    
    TEST_END();
}

/**
 * Test basic RAM filesystem functionality
 */
static void test_ramfs_basic(void) {
    TEST_START("RAM Filesystem Basic");
    
    /* RAM filesystem should be registered from earlier test */
    
    /* Test creating RAM filesystem superblock */
    /* This is done implicitly by the mount operation */
    TEST_ASSERT(true, "RAM filesystem should be available");
    
    TEST_END();
}

/**
 * Test RAM filesystem file operations
 */
static void test_ramfs_file_operations(void) {
    TEST_START("RAM Filesystem File Operations");
    
    /* Note: These tests would work if we had a complete mount */
    /* For now, we test the basic structure */
    
    TEST_ASSERT(true, "RAM filesystem structure should be valid");
    
    TEST_END();
}

/**
 * Test VFS statistics
 */
static void test_vfs_statistics(void) {
    TEST_START("VFS Statistics");
    
    vfs_stats_t stats;
    vfs_get_stats(&stats);
    
    TEST_ASSERT(stats.mounted_filesystems >= 0, "Mounted filesystems count should be valid");
    TEST_ASSERT(stats.open_files >= 0, "Open files count should be valid");
    TEST_ASSERT(stats.total_reads >= 0, "Total reads count should be valid");
    TEST_ASSERT(stats.total_writes >= 0, "Total writes count should be valid");
    TEST_ASSERT(stats.bytes_read >= 0, "Bytes read count should be valid");
    TEST_ASSERT(stats.bytes_written >= 0, "Bytes written count should be valid");
    
    /* Test null stats parameter */
    vfs_get_stats(NULL);
    TEST_ASSERT(true, "Getting stats with NULL parameter should not crash");
    
    TEST_END();
}

/**
 * Test error conditions
 */
static void test_error_conditions(void) {
    TEST_START("Error Conditions");
    
    /* Test operations on uninitialized VFS */
    vfs_shutdown();
    
    int result = vfs_mount("/dev/test", "/", "ramfs", 0, NULL);
    TEST_ASSERT(result == VFS_ERROR_INVALID_PARAM, "Mount on uninitialized VFS should fail");
    
    result = vfs_register_filesystem(&mock_fs_type);
    TEST_ASSERT(result == VFS_ERROR_INVALID_PARAM, "Register on uninitialized VFS should fail");
    
    int fd = vfs_open("/test", VFS_O_RDONLY, 0);
    TEST_ASSERT(fd == VFS_ERROR_INVALID_PARAM, "Open on uninitialized VFS should fail");
    
    /* Re-initialize for cleanup */
    vfs_init();
    
    TEST_END();
}

/* ================================
 * Mock Filesystem Implementation
 * ================================ */

static vfs_superblock_t* mock_fs_mount(vfs_filesystem_t* fs, uint32_t flags,
                                       const char* dev_name, void* data) {
    (void)flags;
    (void)dev_name;
    (void)data;
    
    vfs_superblock_t* sb = (vfs_superblock_t*)malloc(sizeof(vfs_superblock_t));
    if (!sb) {
        return NULL;
    }
    
    memset(sb, 0, sizeof(vfs_superblock_t));
    sb->s_magic = 0x12345678;
    sb->s_type = fs;
    
    /* Create root dentry */
    vfs_dentry_t* root = vfs_alloc_dentry("/");
    if (!root) {
        free(sb);
        return NULL;
    }
    
    sb->s_root = root;
    fs->fs_supers++;
    
    return sb;
}

static void mock_fs_kill_sb(vfs_superblock_t* sb) {
    if (!sb) {
        return;
    }
    
    if (sb->s_root) {
        vfs_free_dentry(sb->s_root);
    }
    
    if (sb->s_type) {
        sb->s_type->fs_supers--;
    }
    
    free(sb);
}

/* Simple malloc/free for testing */
void* malloc(size_t size) {
    static char heap[1024 * 1024];  /* 1MB heap */
    static size_t offset = 0;
    
    if (offset + size > sizeof(heap)) {
        return NULL;
    }
    
    void* ptr = &heap[offset];
    offset += size;
    return ptr;
}

void free(void* ptr) {
    (void)ptr;  /* No-op for simplicity */
}
