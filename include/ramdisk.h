/* IKOS RAM disk - in-memory block device
 *
 * A simple 1 MiB RAM-backed block device exposing the fat_block_device_t
 * interface. Used (among other things) as the backing store for orthogonal
 * persistence checkpoints when no real disk region is dedicated (#129).
 */

#ifndef RAMDISK_H
#define RAMDISK_H

#include <stdint.h>
#include <stdbool.h>
#include "fat.h"   /* fat_block_device_t */

/* Allocate and zero the RAM disk (idempotent). Returns 0 on success. */
int ramdisk_init(void);

/* Free the RAM disk backing memory. */
void ramdisk_cleanup(void);

/* Return the block-device handle, initializing the RAM disk on first use.
 * Returns NULL if initialization fails. */
fat_block_device_t* ramdisk_get_device(void);

/* Optional helpers (FAT16 formatting / test data / stats). */
int ramdisk_format_fat16(void);
int ramdisk_create_test_file(void);
void ramdisk_get_stats(uint32_t* total_sectors, uint32_t* sector_size, bool* initialized);

#endif /* RAMDISK_H */
