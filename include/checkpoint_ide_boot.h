/* IKOS Orthogonal Persistence - IDE durable-store boot wiring (#201, epic #159)
 *
 * The IDE-backed checkpoint store adapter (#130, checkpoint_ide.c) bridges an
 * IDE drive to a fat_block_device_t but was never wired into the boot path, so
 * the checkpoint store, journal, and keyframe ring lived only on the volatile
 * RAM disk. This wires the durable store in: initialize the primary IDE
 * controller, identify its drives, and bind the first present drive as the
 * persistence backing device, so checkpoints survive a real power cut.
 *
 * Hardware-coupled (it touches the IDE controller), so it is verified by the
 * freestanding compile check; the binding it produces is exercised host-side by
 * the checkpoint_ide adapter test and the IDE durability test.
 */

#ifndef CHECKPOINT_IDE_BOOT_H
#define CHECKPOINT_IDE_BOOT_H

#include "fat.h"   /* fat_block_device_t */

/* Initialize the primary IDE controller, identify its drives, and bind the
 * first present drive as a durable checkpoint-store backing device. Returns the
 * bound block device (non-volatile, survives a power cut), or NULL when no IDE
 * drive is present so the caller can fall back to the volatile RAM disk.
 * Storage is static; call once at boot. */
fat_block_device_t* checkpoint_ide_boot_bind(void);

#endif /* CHECKPOINT_IDE_BOOT_H */
