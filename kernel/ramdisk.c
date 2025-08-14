/* IKOS Block Device Driver - Simple RAM Disk for FAT Testing
 * Simulates a block device in memory for FAT filesystem testing
 */

#include "fat.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* RAM disk configuration */
#define RAMDISK_SECTOR_SIZE     512
#define RAMDISK_TOTAL_SECTORS   2048    /* 1MB RAM disk */
#define RAMDISK_TOTAL_SIZE      (RAMDISK_TOTAL_SECTORS * RAMDISK_SECTOR_SIZE)

/* RAM disk structure */
typedef struct {
    uint8_t* data;                      /* RAM disk data */
    uint32_t sector_size;               /* Sector size */
    uint32_t total_sectors;             /* Total sectors */
    bool initialized;                   /* Initialization flag */
} ramdisk_t;

/* Global RAM disk instance */
static ramdisk_t g_ramdisk = {0};

/* Function declarations */
extern void* kmalloc(size_t size);
extern void kfree(void* ptr);
extern void* memset(void* ptr, int value, size_t size);
extern void* memcpy(void* dest, const void* src, size_t size);

/* Block device operations */
static int ramdisk_read_sectors(void* device, uint32_t sector, uint32_t count, void* buffer);
static int ramdisk_write_sectors(void* device, uint32_t sector, uint32_t count, const void* buffer);

/* Block device interface */
static fat_block_device_t ramdisk_device = {
    .read_sectors = ramdisk_read_sectors,
    .write_sectors = ramdisk_write_sectors,
    .sector_size = RAMDISK_SECTOR_SIZE,
    .total_sectors = RAMDISK_TOTAL_SECTORS,
    .private_data = &g_ramdisk,
};

/* ================================
 * RAM Disk Operations
 * ================================ */

/**
 * Initialize RAM disk
 */
int ramdisk_init(void) {
    if (g_ramdisk.initialized) {
        return 0;  /* Already initialized */
    }
    
    /* Allocate memory for RAM disk */
    g_ramdisk.data = (uint8_t*)kmalloc(RAMDISK_TOTAL_SIZE);
    if (!g_ramdisk.data) {
        return -1;
    }
    
    /* Initialize RAM disk */
    memset(g_ramdisk.data, 0, RAMDISK_TOTAL_SIZE);
    g_ramdisk.sector_size = RAMDISK_SECTOR_SIZE;
    g_ramdisk.total_sectors = RAMDISK_TOTAL_SECTORS;
    g_ramdisk.initialized = true;
    
    return 0;
}

/**
 * Cleanup RAM disk
 */
void ramdisk_cleanup(void) {
    if (g_ramdisk.initialized && g_ramdisk.data) {
        kfree(g_ramdisk.data);
        memset(&g_ramdisk, 0, sizeof(ramdisk_t));
    }
}

/**
 * Read sectors from RAM disk
 */
static int ramdisk_read_sectors(void* device, uint32_t sector, uint32_t count, void* buffer) {
    ramdisk_t* ramdisk = (ramdisk_t*)device;
    
    if (!ramdisk || !ramdisk->initialized || !ramdisk->data || !buffer) {
        return -1;
    }
    
    /* Check bounds */
    if (sector >= ramdisk->total_sectors || 
        sector + count > ramdisk->total_sectors) {
        return -1;
    }
    
    /* Copy data */
    uint32_t offset = sector * ramdisk->sector_size;
    uint32_t size = count * ramdisk->sector_size;
    memcpy(buffer, ramdisk->data + offset, size);
    
    return 0;
}

/**
 * Write sectors to RAM disk
 */
static int ramdisk_write_sectors(void* device, uint32_t sector, uint32_t count, const void* buffer) {
    ramdisk_t* ramdisk = (ramdisk_t*)device;
    
    if (!ramdisk || !ramdisk->initialized || !ramdisk->data || !buffer) {
        return -1;
    }
    
    /* Check bounds */
    if (sector >= ramdisk->total_sectors || 
        sector + count > ramdisk->total_sectors) {
        return -1;
    }
    
    /* Copy data */
    uint32_t offset = sector * ramdisk->sector_size;
    uint32_t size = count * ramdisk->sector_size;
    memcpy(ramdisk->data + offset, buffer, size);
    
    return 0;
}

/**
 * Get RAM disk device for FAT filesystem
 */
fat_block_device_t* ramdisk_get_device(void) {
    if (!g_ramdisk.initialized) {
        if (ramdisk_init() != 0) {
            return NULL;
        }
    }
    return &ramdisk_device;
}

/**
 * Format RAM disk with a simple FAT16 filesystem
 */
int ramdisk_format_fat16(void) {
    if (!g_ramdisk.initialized) {
        if (ramdisk_init() != 0) {
            return -1;
        }
    }
    
    /* Create a simple FAT16 boot sector */
    fat_boot_sector_t boot_sector;
    memset(&boot_sector, 0, sizeof(fat_boot_sector_t));
    
    /* Jump instruction */
    boot_sector.jump_boot[0] = 0xEB;
    boot_sector.jump_boot[1] = 0x3C;
    boot_sector.jump_boot[2] = 0x90;
    
    /* OEM name */
    memcpy(boot_sector.oem_name, "IKOS    ", 8);
    
    /* Basic parameters */
    boot_sector.bytes_per_sector = 512;
    boot_sector.sectors_per_cluster = 1;
    boot_sector.reserved_sectors = 1;
    boot_sector.num_fats = 2;
    boot_sector.root_entries = 224;
    boot_sector.total_sectors_16 = RAMDISK_TOTAL_SECTORS;
    boot_sector.media_type = 0xF8;
    boot_sector.fat_size_16 = 8;  /* 8 sectors per FAT */
    boot_sector.sectors_per_track = 32;
    boot_sector.num_heads = 2;
    boot_sector.hidden_sectors = 0;
    boot_sector.total_sectors_32 = 0;
    
    /* FAT16 specific fields */
    boot_sector.fat16.drive_number = 0x80;
    boot_sector.fat16.reserved = 0;
    boot_sector.fat16.boot_signature = 0x29;
    boot_sector.fat16.volume_id = 0x12345678;
    memcpy(boot_sector.fat16.volume_label, "IKOS RAMDSK", 11);
    memcpy(boot_sector.fat16.filesystem_type, "FAT16   ", 8);
    
    /* Boot sector signature */
    boot_sector.boot_sector_signature = 0xAA55;
    
    /* Write boot sector */
    if (ramdisk_write_sectors(&g_ramdisk, 0, 1, &boot_sector) != 0) {
        return -1;
    }
    
    /* Initialize FAT tables */
    uint16_t fat_table[256];  /* One sector worth of FAT entries */
    memset(fat_table, 0, sizeof(fat_table));
    
    /* Set first few FAT entries */
    fat_table[0] = 0xFFF8;  /* Media descriptor */
    fat_table[1] = 0xFFFF;  /* End of cluster chain */
    
    /* Write first FAT */
    if (ramdisk_write_sectors(&g_ramdisk, 1, 1, fat_table) != 0) {
        return -1;
    }
    
    /* Write second FAT (copy) */
    if (ramdisk_write_sectors(&g_ramdisk, 9, 1, fat_table) != 0) {
        return -1;
    }
    
    /* Clear root directory */
    uint8_t root_dir[512 * 14];  /* 14 sectors for root directory */
    memset(root_dir, 0, sizeof(root_dir));
    
    /* Write root directory */
    if (ramdisk_write_sectors(&g_ramdisk, 17, 14, root_dir) != 0) {
        return -1;
    }
    
    return 0;
}

/**
 * Create a test file in the RAM disk
 */
int ramdisk_create_test_file(void) {
    if (!g_ramdisk.initialized) {
        return -1;
    }
    
    /* Create a simple directory entry for "TEST.TXT" */
    fat_dir_entry_t test_entry;
    memset(&test_entry, 0, sizeof(fat_dir_entry_t));
    
    memcpy(test_entry.name, "TEST    TXT", 11);
    test_entry.attributes = FAT_ATTR_ARCHIVE;
    test_entry.first_cluster_low = 2;  /* First data cluster */
    test_entry.first_cluster_high = 0;
    test_entry.file_size = 13;  /* "Hello, World!" */
    
    /* Write directory entry to root directory */
    if (ramdisk_write_sectors(&g_ramdisk, 17, 1, &test_entry) != 0) {
        return -1;
    }
    
    /* Update FAT to mark cluster 2 as used and EOF */
    uint16_t fat_table[256];
    if (ramdisk_read_sectors(&g_ramdisk, 1, 1, fat_table) != 0) {
        return -1;
    }
    
    fat_table[2] = 0xFFFF;  /* End of file */
    
    if (ramdisk_write_sectors(&g_ramdisk, 1, 1, fat_table) != 0) {
        return -1;
    }
    
    if (ramdisk_write_sectors(&g_ramdisk, 9, 1, fat_table) != 0) {
        return -1;
    }
    
    /* Write file data */
    const char* test_data = "Hello, World!";
    uint8_t cluster_data[512];
    memset(cluster_data, 0, sizeof(cluster_data));
    memcpy(cluster_data, test_data, 13);
    
    /* Calculate data area start (sector 31 = 1 + 2*8 + 14) */
    if (ramdisk_write_sectors(&g_ramdisk, 31, 1, cluster_data) != 0) {
        return -1;
    }
    
    return 0;
}

/**
 * Get RAM disk statistics
 */
void ramdisk_get_stats(uint32_t* total_sectors, uint32_t* sector_size, bool* initialized) {
    if (total_sectors) {
        *total_sectors = g_ramdisk.total_sectors;
    }
    if (sector_size) {
        *sector_size = g_ramdisk.sector_size;
    }
    if (initialized) {
        *initialized = g_ramdisk.initialized;
    }
}
