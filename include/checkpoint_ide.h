/* IKOS Orthogonal Persistence - IDE-backed checkpoint store (#130)
 *
 * Adapts an IDE drive to the fat_block_device_t interface so the checkpoint
 * store can live on a real, NON-VOLATILE disk (unlike the volatile RAM disk
 * used in #129). A checkpoint written here survives an actual power cut.
 *
 * The IDE device is held as an opaque pointer so this header stays decoupled
 * from the IDE driver headers (and is host-testable with a mock backend).
 */

#ifndef CHECKPOINT_IDE_H
#define CHECKPOINT_IDE_H

#include <stdint.h>
#include "fat.h"   /* fat_block_device_t */

/* Binds one IDE drive (controller + master/slave selector) as the backend of a
 * fat_block_device_t. Stored in the block device's private_data. */
typedef struct {
    void*   ide;    /* ide_device_t* (opaque here) */
    uint8_t drive;  /* 0 = master, 1 = slave */
} checkpoint_ide_binding_t;

/* Fill *bdev so it reads/writes 512-byte sectors via the given IDE drive,
 * recording the binding in *binding. total_sectors bounds the device. Returns
 * bdev, or NULL on bad arguments. */
fat_block_device_t* checkpoint_ide_bind(fat_block_device_t* bdev,
                                        checkpoint_ide_binding_t* binding,
                                        void* ide, uint8_t drive,
                                        uint64_t total_sectors);

#endif /* CHECKPOINT_IDE_H */
