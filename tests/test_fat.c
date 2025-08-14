/* IKOS FAT Filesystem Test Suite
 * Comprehensive tests for FAT16/FAT32 filesystem functionality
 */

#include "fat.h"
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
static void test_fat_initialization(void);
static void test_boot_sector_validation(void);
static void test_fat_type_detection(void);
static void test_fat_table_operations(void);
static void test_cluster_operations(void);
static void test_filename_operations(void);
static void test_directory_operations(void);
static void test_file_operations(void);
static void test_mount_operations(void);
static void test_vfs_integration(void);
static void test_ramdisk_operations(void);
static void test_error_conditions(void);

/* External function declarations */
extern int ramdisk_init(void);
extern void ramdisk_cleanup(void);
extern fat_block_device_t* ramdisk_get_device(void);
extern int ramdisk_format_fat16(void);
extern int ramdisk_create_test_file(void);

/* Test data */
static fat_boot_sector_t test_boot_sector;
static fat_fs_info_t test_fat_info;

/* ================================
 * Test Implementation
 * ================================ */

/**
 * Main test runner
 */
int main(void) {
    printf("IKOS FAT Filesystem Test Suite\n");
    printf("===============================\n");
    
    /* Initialize VFS for integration tests */
    if (vfs_init() != VFS_SUCCESS) {
        printf("Failed to initialize VFS\n");
        return 1;
    }
    
    /* Run all tests */
    test_fat_initialization();
    test_boot_sector_validation();
    test_fat_type_detection();
    test_fat_table_operations();
    test_cluster_operations();
    test_filename_operations();
    test_directory_operations();
    test_file_operations();
    test_ramdisk_operations();
    test_mount_operations();
    test_vfs_integration();
    test_error_conditions();
    
    /* Cleanup */
    vfs_shutdown();
    ramdisk_cleanup();
    
    /* Print summary */
    printf("\n===============================\n");
    printf("Test Summary:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Success rate: %.1f%%\n", 
           tests_run > 0 ? (float)tests_passed / tests_run * 100.0f : 0.0f);
    
    return tests_failed > 0 ? 1 : 0;
}

/**
 * Test FAT filesystem initialization
 */
static void test_fat_initialization(void) {
    TEST_START("FAT Initialization");
    
    /* Test FAT filesystem registration */
    int result = fat_init();
    TEST_ASSERT(result == VFS_SUCCESS, "FAT filesystem registration should succeed");
    
    /* Test double initialization safety */
    result = fat_init();
    TEST_ASSERT(result == VFS_ERROR_EXISTS, "Double registration should fail");
    
    /* Test cleanup */
    fat_exit();
    TEST_ASSERT(true, "FAT filesystem cleanup should complete without error");
    
    /* Re-register for subsequent tests */
    result = fat_init();
    TEST_ASSERT(result == VFS_SUCCESS, "FAT filesystem re-registration should succeed");
    
    TEST_END();
}

/**
 * Test boot sector validation
 */
static void test_boot_sector_validation(void) {
    TEST_START("Boot Sector Validation");
    
    /* Create valid boot sector */
    memset(&test_boot_sector, 0, sizeof(fat_boot_sector_t));
    test_boot_sector.bytes_per_sector = 512;
    test_boot_sector.sectors_per_cluster = 1;
    test_boot_sector.reserved_sectors = 1;
    test_boot_sector.num_fats = 2;
    test_boot_sector.root_entries = 224;
    test_boot_sector.total_sectors_16 = 2048;
    test_boot_sector.fat_size_16 = 8;
    test_boot_sector.boot_sector_signature = 0xAA55;
    
    /* Test valid boot sector */
    bool valid = fat_is_valid_boot_sector(&test_boot_sector);
    TEST_ASSERT(valid, "Valid boot sector should pass validation");
    
    /* Test invalid signature */
    test_boot_sector.boot_sector_signature = 0x1234;
    valid = fat_is_valid_boot_sector(&test_boot_sector);
    TEST_ASSERT(!valid, "Invalid signature should fail validation");
    
    /* Test invalid sector size */
    test_boot_sector.boot_sector_signature = 0xAA55;
    test_boot_sector.bytes_per_sector = 100;
    valid = fat_is_valid_boot_sector(&test_boot_sector);
    TEST_ASSERT(!valid, "Invalid sector size should fail validation");
    
    /* Test invalid cluster size */
    test_boot_sector.bytes_per_sector = 512;
    test_boot_sector.sectors_per_cluster = 3;  /* Not power of 2 */
    valid = fat_is_valid_boot_sector(&test_boot_sector);
    TEST_ASSERT(!valid, "Invalid cluster size should fail validation");
    
    /* Test zero reserved sectors */
    test_boot_sector.sectors_per_cluster = 1;
    test_boot_sector.reserved_sectors = 0;
    valid = fat_is_valid_boot_sector(&test_boot_sector);
    TEST_ASSERT(!valid, "Zero reserved sectors should fail validation");
    
    /* Test null pointer */
    valid = fat_is_valid_boot_sector(NULL);
    TEST_ASSERT(!valid, "NULL boot sector should fail validation");
    
    TEST_END();
}

/**
 * Test FAT type detection
 */
static void test_fat_type_detection(void) {
    TEST_START("FAT Type Detection");
    
    /* Reset to valid boot sector */
    test_boot_sector.bytes_per_sector = 512;
    test_boot_sector.sectors_per_cluster = 1;
    test_boot_sector.reserved_sectors = 1;
    test_boot_sector.num_fats = 2;
    test_boot_sector.fat_size_16 = 8;
    test_boot_sector.boot_sector_signature = 0xAA55;
    
    /* Test FAT16 detection */
    test_boot_sector.root_entries = 224;
    test_boot_sector.total_sectors_16 = 2048;
    test_boot_sector.total_sectors_32 = 0;
    
    fat_type_t type = fat_determine_type(&test_boot_sector);
    TEST_ASSERT(type == FAT_TYPE_FAT16, "Small filesystem should be detected as FAT16");
    
    /* Test FAT32 detection */
    test_boot_sector.root_entries = 0;
    test_boot_sector.total_sectors_16 = 0;
    test_boot_sector.total_sectors_32 = 100000;
    test_boot_sector.fat_size_16 = 0;
    test_boot_sector.fat32.fat_size_32 = 100;
    
    type = fat_determine_type(&test_boot_sector);
    TEST_ASSERT(type == FAT_TYPE_FAT32, "Large filesystem should be detected as FAT32");
    
    /* Test null pointer */
    type = fat_determine_type(NULL);
    TEST_ASSERT(type == FAT_TYPE_UNKNOWN, "NULL boot sector should return unknown type");
    
    TEST_END();
}

/**
 * Test FAT table operations
 */
static void test_fat_table_operations(void) {
    TEST_START("FAT Table Operations");
    
    /* Initialize test FAT info structure */
    memset(&test_fat_info, 0, sizeof(fat_fs_info_t));
    test_fat_info.type = FAT_TYPE_FAT16;
    test_fat_info.sector_size = 512;
    test_fat_info.fat_size = 1;
    test_fat_info.total_clusters = 100;
    
    /* Allocate FAT table for testing */
    test_fat_info.fat_table_size = 512;
    test_fat_info.fat_table = (uint8_t*)malloc(512);
    TEST_ASSERT(test_fat_info.fat_table != NULL, "FAT table allocation should succeed");
    
    if (test_fat_info.fat_table) {
        memset(test_fat_info.fat_table, 0, 512);
        
        /* Test setting cluster values */
        int result = fat_set_cluster_value(&test_fat_info, 2, 0xFFFF);
        TEST_ASSERT(result == FAT_SUCCESS, "Setting cluster value should succeed");
        
        /* Test getting cluster values */
        uint32_t value = fat_get_cluster_value(&test_fat_info, 2);
        TEST_ASSERT(value == 0xFFFF, "Getting cluster value should return correct value");
        
        /* Test finding free clusters */
        fat_set_cluster_value(&test_fat_info, 3, 0);  /* Mark as free */
        uint32_t free_cluster = fat_find_free_cluster(&test_fat_info);
        TEST_ASSERT(free_cluster == 3, "Should find first free cluster");
        
        /* Test cluster state checks */
        bool is_free = fat_is_cluster_free(&test_fat_info, 3);
        TEST_ASSERT(is_free, "Cluster 3 should be free");
        
        bool is_eof = fat_is_cluster_eof(&test_fat_info, 0xFFFF);
        TEST_ASSERT(is_eof, "0xFFFF should be EOF for FAT16");
        
        free(test_fat_info.fat_table);
    }
    
    /* Test invalid parameters */
    uint32_t value = fat_get_cluster_value(NULL, 2);
    TEST_ASSERT(value == 0, "Getting cluster value with NULL info should return 0");
    
    int result = fat_set_cluster_value(NULL, 2, 0xFFFF);
    TEST_ASSERT(result == FAT_ERROR_INVALID_CLUSTER, "Setting cluster value with NULL info should fail");
    
    TEST_END();
}

/**
 * Test cluster operations
 */
static void test_cluster_operations(void) {
    TEST_START("Cluster Operations");
    
    /* Initialize test FAT info */
    memset(&test_fat_info, 0, sizeof(fat_fs_info_t));
    test_fat_info.type = FAT_TYPE_FAT16;
    test_fat_info.sectors_per_cluster = 1;
    test_fat_info.first_data_sector = 31;
    
    /* Test cluster to sector conversion */
    uint32_t sector = fat_cluster_to_sector(&test_fat_info, 2);
    TEST_ASSERT(sector == 31, "First data cluster should map to first data sector");
    
    sector = fat_cluster_to_sector(&test_fat_info, 3);
    TEST_ASSERT(sector == 32, "Second data cluster should map to second data sector");
    
    /* Test invalid cluster */
    sector = fat_cluster_to_sector(&test_fat_info, 0);
    TEST_ASSERT(sector == 0, "Invalid cluster should return 0");
    
    sector = fat_cluster_to_sector(NULL, 2);
    TEST_ASSERT(sector == 0, "NULL FAT info should return 0");
    
    TEST_END();
}

/**
 * Test filename operations
 */
static void test_filename_operations(void) {
    TEST_START("Filename Operations");
    
    /* Test 8.3 filename conversion */
    char fat_name[12];
    
    fat_name_to_83("test.txt", fat_name);
    TEST_ASSERT(memcmp(fat_name, "TEST    TXT", 11) == 0, "test.txt should convert correctly");
    
    fat_name_to_83("HELLO", fat_name);
    TEST_ASSERT(memcmp(fat_name, "HELLO      ", 11) == 0, "HELLO should convert correctly");
    
    fat_name_to_83("verylongfilename.extension", fat_name);
    TEST_ASSERT(memcmp(fat_name, "VERYLONGEXT", 11) == 0, "Long filename should be truncated");
    
    /* Test reverse conversion */
    char normal_name[13];
    
    fat_83_to_name("TEST    TXT", normal_name);
    TEST_ASSERT(strcmp(normal_name, "test.txt") == 0, "FAT name should convert back correctly");
    
    fat_83_to_name("HELLO      ", normal_name);
    TEST_ASSERT(strcmp(normal_name, "hello") == 0, "HELLO should convert back correctly");
    
    /* Test null pointers */
    fat_name_to_83(NULL, fat_name);
    TEST_ASSERT(true, "NULL name should not crash");
    
    fat_83_to_name(NULL, normal_name);
    TEST_ASSERT(true, "NULL FAT name should not crash");
    
    TEST_END();
}

/**
 * Test directory operations
 */
static void test_directory_operations(void) {
    TEST_START("Directory Operations");
    
    /* These tests would require a proper block device setup */
    /* For now, test the interface functions exist */
    
    TEST_ASSERT(fat_find_dir_entry != NULL, "fat_find_dir_entry function should exist");
    TEST_ASSERT(fat_create_dir_entry != NULL, "fat_create_dir_entry function should exist");
    
    /* Test invalid parameters */
    fat_dir_entry_t entry;
    uint32_t offset;
    int result = fat_find_dir_entry(NULL, 0, "test", &entry, &offset);
    TEST_ASSERT(result == FAT_ERROR_INVALID_NAME, "NULL FAT info should fail");
    
    result = fat_find_dir_entry(&test_fat_info, 0, NULL, &entry, &offset);
    TEST_ASSERT(result == FAT_ERROR_INVALID_NAME, "NULL name should fail");
    
    TEST_END();
}

/**
 * Test file operations
 */
static void test_file_operations(void) {
    TEST_START("File Operations");
    
    /* Test that file operation functions exist */
    TEST_ASSERT(fat_open != NULL, "fat_open function should exist");
    TEST_ASSERT(fat_release != NULL, "fat_release function should exist");
    TEST_ASSERT(fat_read != NULL, "fat_read function should exist");
    TEST_ASSERT(fat_write != NULL, "fat_write function should exist");
    TEST_ASSERT(fat_llseek != NULL, "fat_llseek function should exist");
    
    /* Test invalid parameters */
    int result = fat_open(NULL, NULL);
    TEST_ASSERT(result == VFS_ERROR_INVALID_PARAM, "NULL parameters should fail");
    
    result = fat_release(NULL, NULL);
    TEST_ASSERT(result == VFS_ERROR_INVALID_PARAM, "NULL parameters should fail");
    
    ssize_t bytes = fat_read(NULL, NULL, 0, NULL);
    TEST_ASSERT(bytes == VFS_ERROR_INVALID_PARAM, "NULL parameters should fail");
    
    bytes = fat_write(NULL, NULL, 0, NULL);
    TEST_ASSERT(bytes == VFS_ERROR_INVALID_PARAM, "NULL parameters should fail");
    
    TEST_END();
}

/**
 * Test RAM disk operations
 */
static void test_ramdisk_operations(void) {
    TEST_START("RAM Disk Operations");
    
    /* Test RAM disk initialization */
    int result = ramdisk_init();
    TEST_ASSERT(result == 0, "RAM disk initialization should succeed");
    
    /* Test getting device */
    fat_block_device_t* device = ramdisk_get_device();
    TEST_ASSERT(device != NULL, "Should get valid block device");
    
    if (device) {
        TEST_ASSERT(device->read_sectors != NULL, "Read function should be available");
        TEST_ASSERT(device->write_sectors != NULL, "Write function should be available");
        TEST_ASSERT(device->sector_size == 512, "Sector size should be 512");
        TEST_ASSERT(device->total_sectors == 2048, "Should have 2048 sectors");
    }
    
    /* Test formatting */
    result = ramdisk_format_fat16();
    TEST_ASSERT(result == 0, "FAT16 formatting should succeed");
    
    /* Test creating test file */
    result = ramdisk_create_test_file();
    TEST_ASSERT(result == 0, "Test file creation should succeed");
    
    /* Test statistics */
    uint32_t total_sectors, sector_size;
    bool initialized;
    ramdisk_get_stats(&total_sectors, &sector_size, &initialized);
    TEST_ASSERT(initialized, "RAM disk should be initialized");
    TEST_ASSERT(total_sectors == 2048, "Should report correct total sectors");
    TEST_ASSERT(sector_size == 512, "Should report correct sector size");
    
    TEST_END();
}

/**
 * Test mount operations
 */
static void test_mount_operations(void) {
    TEST_START("Mount Operations");
    
    /* Get formatted RAM disk */
    fat_block_device_t* device = ramdisk_get_device();
    TEST_ASSERT(device != NULL, "Should have RAM disk device");
    
    if (device) {
        /* Test mounting FAT filesystem */
        vfs_superblock_t* sb = fat_mount(&fat_fs_type, 0, "/dev/ram0", device);
        TEST_ASSERT(sb != NULL, "FAT mount should succeed");
        
        if (sb) {
            TEST_ASSERT(sb->s_magic == 0xFAT16FAT, "Should have correct magic number");
            TEST_ASSERT(sb->s_private != NULL, "Should have private data");
            TEST_ASSERT(sb->s_root != NULL, "Should have root dentry");
            
            fat_fs_info_t* fat_info = (fat_fs_info_t*)sb->s_private;
            TEST_ASSERT(fat_info->type == FAT_TYPE_FAT16, "Should detect FAT16");
            TEST_ASSERT(fat_info->sector_size == 512, "Should have correct sector size");
            TEST_ASSERT(fat_info->fat_table != NULL, "Should have loaded FAT table");
            
            /* Test unmounting */
            fat_kill_sb(sb);
            TEST_ASSERT(true, "FAT unmount should complete without error");
        }
    }
    
    /* Test invalid mount parameters */
    vfs_superblock_t* sb = fat_mount(NULL, 0, "/dev/ram0", device);
    TEST_ASSERT(sb == NULL, "Mount with NULL filesystem should fail");
    
    sb = fat_mount(&fat_fs_type, 0, "/dev/ram0", NULL);
    TEST_ASSERT(sb == NULL, "Mount with NULL device should fail");
    
    TEST_END();
}

/**
 * Test VFS integration
 */
static void test_vfs_integration(void) {
    TEST_START("VFS Integration");
    
    /* Get formatted RAM disk */
    fat_block_device_t* device = ramdisk_get_device();
    TEST_ASSERT(device != NULL, "Should have RAM disk device");
    
    if (device) {
        /* Test mounting through VFS */
        int result = vfs_mount("/dev/ram0", "/", "fat", 0, device);
        TEST_ASSERT(result == VFS_SUCCESS, "VFS mount should succeed");
        
        if (result == VFS_SUCCESS) {
            /* Test file operations through VFS */
            int fd = vfs_open("/test.txt", VFS_O_RDONLY, 0);
            TEST_ASSERT(fd >= 0, "Should be able to open test file");
            
            if (fd >= 0) {
                char buffer[32];
                ssize_t bytes_read = vfs_read(fd, buffer, sizeof(buffer) - 1);
                TEST_ASSERT(bytes_read > 0, "Should read data from file");
                
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    TEST_ASSERT(strcmp(buffer, "Hello, World!") == 0, 
                                "Should read correct file content");
                }
                
                int close_result = vfs_close(fd);
                TEST_ASSERT(close_result == VFS_SUCCESS, "Should close file successfully");
            }
            
            /* Test unmounting through VFS */
            result = vfs_umount("/");
            TEST_ASSERT(result == VFS_SUCCESS || result == VFS_ERROR_NOT_FOUND, 
                        "VFS unmount should succeed or fail gracefully");
        }
    }
    
    TEST_END();
}

/**
 * Test error conditions
 */
static void test_error_conditions(void) {
    TEST_START("Error Conditions");
    
    /* Test operations with uninitialized FAT */
    fat_fs_info_t empty_info;
    memset(&empty_info, 0, sizeof(fat_fs_info_t));
    
    uint32_t value = fat_get_cluster_value(&empty_info, 2);
    TEST_ASSERT(value == 0, "Should return 0 for uninitialized FAT");
    
    int result = fat_set_cluster_value(&empty_info, 2, 0xFFFF);
    TEST_ASSERT(result == FAT_ERROR_INVALID_CLUSTER, "Should fail for uninitialized FAT");
    
    /* Test invalid cluster numbers */
    result = fat_set_cluster_value(&test_fat_info, 0, 0xFFFF);
    TEST_ASSERT(result == FAT_ERROR_INVALID_CLUSTER, "Should fail for cluster 0");
    
    result = fat_set_cluster_value(&test_fat_info, 1, 0xFFFF);
    TEST_ASSERT(result == FAT_ERROR_INVALID_CLUSTER, "Should fail for cluster 1");
    
    /* Test null filesystem operations */
    vfs_superblock_t* sb = fat_mount(NULL, 0, "test", NULL);
    TEST_ASSERT(sb == NULL, "Mount with NULL filesystem should fail");
    
    fat_kill_sb(NULL);
    TEST_ASSERT(true, "Kill NULL superblock should not crash");
    
    vfs_inode_t* inode = fat_alloc_inode(NULL);
    TEST_ASSERT(inode == NULL, "Alloc inode with NULL superblock should fail");
    
    TEST_END();
}

/* Simple memory allocation for testing */
void* malloc(size_t size) {
    static char heap[64 * 1024];  /* 64KB heap */
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

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

void* memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return (void*)(long)(p1[i] - p2[i]);
        }
    }
    return NULL;
}
