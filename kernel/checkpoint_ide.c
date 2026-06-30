/* IKOS Orthogonal Persistence - IDE-backed checkpoint store (#130)
 *
 * See include/checkpoint_ide.h. Bridges fat_block_device_t sector I/O to the
 * IDE driver's ide_read_sectors/ide_write_sectors.
 */

#include "checkpoint_ide.h"
#include "ide_driver.h"   /* ide_device_t, ide_read_sectors/ide_write_sectors, IDE_SUCCESS */

static int ide_bdev_read(void* device, uint32_t sector, uint32_t count, void* buffer) {
    checkpoint_ide_binding_t* b = (checkpoint_ide_binding_t*)device;
    if (!b || !b->ide || !buffer) {
        return -1;
    }
    int rc = ide_read_sectors((ide_device_t*)b->ide, b->drive,
                              (uint64_t)sector, (uint16_t)count, buffer);
    return rc == IDE_SUCCESS ? 0 : -1;
}

static int ide_bdev_write(void* device, uint32_t sector, uint32_t count, const void* buffer) {
    checkpoint_ide_binding_t* b = (checkpoint_ide_binding_t*)device;
    if (!b || !b->ide || !buffer) {
        return -1;
    }
    int rc = ide_write_sectors((ide_device_t*)b->ide, b->drive,
                               (uint64_t)sector, (uint16_t)count, buffer);
    return rc == IDE_SUCCESS ? 0 : -1;
}

fat_block_device_t* checkpoint_ide_bind(fat_block_device_t* bdev,
                                        checkpoint_ide_binding_t* binding,
                                        void* ide, uint8_t drive,
                                        uint64_t total_sectors) {
    if (!bdev || !binding || !ide) {
        return 0;
    }
    binding->ide = ide;
    binding->drive = drive;

    bdev->read_sectors = ide_bdev_read;
    bdev->write_sectors = ide_bdev_write;
    bdev->sector_size = 512;
    bdev->total_sectors = (uint32_t)total_sectors;
    bdev->private_data = binding;
    return bdev;
}
