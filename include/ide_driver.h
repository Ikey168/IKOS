/* IKOS IDE/ATA Storage Controller Driver
 * Issue #15 - Device Driver Framework
 * 
 * Implementation of IDE/ATA storage controller driver for disk access.
 * Supports both PATA (Parallel ATA) and basic SATA functionality.
 */

#ifndef IDE_DRIVER_H
#define IDE_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "device_manager.h"

/* ================================
 * IDE/ATA Constants
 * ================================ */

/* IDE I/O ports */
#define IDE_PRIMARY_BASE        0x1F0
#define IDE_PRIMARY_CTRL        0x3F6
#define IDE_SECONDARY_BASE      0x170
#define IDE_SECONDARY_CTRL      0x376

/* IDE register offsets */
#define IDE_REG_DATA            0x00
#define IDE_REG_ERROR           0x01    /* Read */
#define IDE_REG_FEATURES        0x01    /* Write */
#define IDE_REG_SECTOR_COUNT    0x02
#define IDE_REG_LBA_LOW         0x03
#define IDE_REG_LBA_MID         0x04
#define IDE_REG_LBA_HIGH        0x05
#define IDE_REG_DRIVE_SELECT    0x06
#define IDE_REG_STATUS          0x07    /* Read */
#define IDE_REG_COMMAND         0x07    /* Write */

/* Control register offsets */
#define IDE_CTRL_ALT_STATUS     0x00    /* Read */
#define IDE_CTRL_DEVICE_CTRL    0x00    /* Write */

/* Status register bits */
#define IDE_STATUS_ERR          0x01    /* Error */
#define IDE_STATUS_IDX          0x02    /* Index */
#define IDE_STATUS_CORR         0x04    /* Corrected data */
#define IDE_STATUS_DRQ          0x08    /* Data request */
#define IDE_STATUS_DSC          0x10    /* Drive seek complete */
#define IDE_STATUS_DF           0x20    /* Drive fault */
#define IDE_STATUS_DRDY         0x40    /* Drive ready */
#define IDE_STATUS_BSY          0x80    /* Busy */

/* Error register bits */
#define IDE_ERROR_AMNF          0x01    /* Address mark not found */
#define IDE_ERROR_TK0NF         0x02    /* Track 0 not found */
#define IDE_ERROR_ABRT          0x04    /* Command aborted */
#define IDE_ERROR_MCR           0x08    /* Media change request */
#define IDE_ERROR_IDNF          0x10    /* ID not found */
#define IDE_ERROR_MC            0x20    /* Media changed */
#define IDE_ERROR_UNC           0x40    /* Uncorrectable data error */
#define IDE_ERROR_BBK           0x80    /* Bad block detected */

/* ATA Commands */
#define IDE_CMD_READ_SECTORS    0x20
#define IDE_CMD_READ_SECTORS_EXT 0x24
#define IDE_CMD_WRITE_SECTORS   0x30
#define IDE_CMD_WRITE_SECTORS_EXT 0x34
#define IDE_CMD_IDENTIFY        0xEC
#define IDE_CMD_IDENTIFY_PACKET 0xA1
#define IDE_CMD_FLUSH_CACHE     0xE7
#define IDE_CMD_SET_FEATURES    0xEF

/* Drive selection bits */
#define IDE_DRIVE_MASTER        0xA0
#define IDE_DRIVE_SLAVE         0xB0
#define IDE_DRIVE_LBA           0x40

/* Device control register bits */
#define IDE_CTRL_nIEN           0x02    /* Interrupt enable (negated) */
#define IDE_CTRL_SRST           0x04    /* Software reset */

/* ================================
 * Data Structures
 * ================================ */

/* IDE controller information */
typedef struct {
    uint16_t io_base;           /* Base I/O port */
    uint16_t ctrl_base;         /* Control register base */
    uint8_t irq;                /* IRQ line */
    bool is_primary;            /* Primary or secondary controller */
} ide_controller_t;

/* IDE drive information */
typedef struct {
    bool present;               /* Drive is present */
    bool is_packet;             /* ATAPI packet device */
    uint16_t cylinders;         /* Number of cylinders */
    uint16_t heads;             /* Number of heads */
    uint16_t sectors_per_track; /* Sectors per track */
    uint64_t total_sectors;     /* Total LBA sectors */
    uint16_t bytes_per_sector;  /* Bytes per sector (usually 512) */
    bool lba48_supported;       /* 48-bit LBA support */
    bool dma_supported;         /* DMA support */
    char model[41];             /* Drive model string */
    char serial[21];            /* Drive serial number */
    char firmware[9];           /* Firmware revision */
} ide_drive_info_t;

/* IDE device structure */
typedef struct {
    ide_controller_t controller;
    ide_drive_info_t drives[2]; /* Master and slave */
    device_t* device;           /* Associated device manager device */
    bool initialized;           /* Controller initialized */
    uint32_t access_count;      /* Access counter */
    uint64_t last_access_time;  /* Last access timestamp */
} ide_device_t;

/* ================================
 * IDE Driver API
 * ================================ */

/* Driver initialization */
int ide_driver_init(void);
void ide_driver_shutdown(void);

/* Controller management */
int ide_init_controller(ide_device_t* ide_dev, uint16_t io_base, uint16_t ctrl_base, uint8_t irq);
int ide_reset_controller(ide_device_t* ide_dev);
int ide_identify_drives(ide_device_t* ide_dev);

/* Low-level I/O */
uint8_t ide_read_reg(ide_device_t* ide_dev, uint8_t reg);
void ide_write_reg(ide_device_t* ide_dev, uint8_t reg, uint8_t value);
uint8_t ide_read_ctrl(ide_device_t* ide_dev);
void ide_write_ctrl(ide_device_t* ide_dev, uint8_t value);
uint16_t ide_read_data(ide_device_t* ide_dev);
void ide_write_data(ide_device_t* ide_dev, uint16_t value);

/* Status and error handling */
int ide_wait_ready(ide_device_t* ide_dev, uint32_t timeout_ms);
int ide_wait_drq(ide_device_t* ide_dev, uint32_t timeout_ms);
bool ide_check_error(ide_device_t* ide_dev);
uint8_t ide_get_error(ide_device_t* ide_dev);

/* Drive selection and addressing */
int ide_select_drive(ide_device_t* ide_dev, uint8_t drive);
int ide_setup_lba(ide_device_t* ide_dev, uint8_t drive, uint64_t lba, uint16_t count);

/* Data transfer operations */
int ide_read_sectors(ide_device_t* ide_dev, uint8_t drive, uint64_t lba, 
                     uint16_t count, void* buffer);
int ide_write_sectors(ide_device_t* ide_dev, uint8_t drive, uint64_t lba, 
                      uint16_t count, const void* buffer);
int ide_flush_cache(ide_device_t* ide_dev, uint8_t drive);

/* Drive identification and information */
int ide_identify_drive(ide_device_t* ide_dev, uint8_t drive);
int ide_get_drive_info(ide_device_t* ide_dev, uint8_t drive, ide_drive_info_t* info);
bool ide_drive_present(ide_device_t* ide_dev, uint8_t drive);

/* Block device interface */
typedef struct {
    int (*read_sectors)(void* device, uint64_t sector, uint32_t count, void* buffer);
    int (*write_sectors)(void* device, uint64_t sector, uint32_t count, const void* buffer);
    uint32_t sector_size;
    uint64_t total_sectors;
    void* private_data;
} ide_block_device_t;

ide_block_device_t* ide_get_block_device(ide_device_t* ide_dev, uint8_t drive);

/* Device manager integration */
int ide_probe_device(device_t* device);
int ide_attach_device(device_t* device);
int ide_detach_device(device_t* device);
int ide_device_ioctl(device_t* device, uint32_t cmd, void* arg);

/* Statistics and monitoring */
typedef struct {
    uint32_t controllers_found;
    uint32_t drives_found;
    uint32_t total_reads;
    uint32_t total_writes;
    uint32_t read_errors;
    uint32_t write_errors;
    uint64_t bytes_read;
    uint64_t bytes_written;
} ide_stats_t;

void ide_get_stats(ide_stats_t* stats);
void ide_print_drive_info(const ide_drive_info_t* info);
void ide_print_all_drives(void);

/* ================================
 * Error Codes
 * ================================ */

#define IDE_SUCCESS             0
#define IDE_ERROR_TIMEOUT       -1
#define IDE_ERROR_DRIVE_ERROR   -2
#define IDE_ERROR_NO_DRIVE      -3
#define IDE_ERROR_BAD_SECTOR    -4
#define IDE_ERROR_NOT_READY     -5
#define IDE_ERROR_INVALID_PARAM -6
#define IDE_ERROR_IO_ERROR      -7

/* ================================
 * IOCTL Commands
 * ================================ */

#define IDE_IOCTL_GET_DRIVE_INFO    0x1000
#define IDE_IOCTL_FLUSH_CACHE       0x1001
#define IDE_IOCTL_RESET_CONTROLLER  0x1002
#define IDE_IOCTL_GET_STATS         0x1003

#endif /* IDE_DRIVER_H */
