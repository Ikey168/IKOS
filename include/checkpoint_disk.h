/* IKOS Orthogonal Persistence v2 - Disk (IDE) driver re-attach (#144)
 *
 * Implements the re-attach contract (#143) for the IDE disk that backs the
 * checkpoint store, per orthogonal-persistence-v2.md section 4:
 *   - quiesce: flush the drive's write cache so the store reaches a durable
 *     boundary before a checkpoint is committed;
 *   - reattach: after a restore, re-probe the drive, then confirm the
 *     checkpoint-store region is readable and carries the store superblock
 *     magic, so we know the disk that came back is the one we checkpointed on.
 *
 * The hardware operations are injected through a small vtable so the reconcile
 * logic is a pure, host-testable core (like the barrier in #138 and the driver
 * registry in #143). The kernel adapter (checkpoint_disk_sync.c) wires the real
 * ide_driver.c calls in and registers the drive into the global registry.
 */

#ifndef CHECKPOINT_DISK_H
#define CHECKPOINT_DISK_H

#include <stdint.h>

/* Magic at offset 0 of the checkpoint-store superblock sector. Mirrors
 * SNAPSHOT_SB_MAGIC ("IKOSSNBP") in snapshot_store.h, duplicated here so this
 * reconcile core stays dependency-free and host-testable. */
#define CHECKPOINT_DISK_SB_MAGIC 0x494B4F53534E4250ULL

#define CHECKPOINT_DISK_OK           0
#define CHECKPOINT_DISK_ERR_PARAM   -1
#define CHECKPOINT_DISK_ERR_FLUSH   -2   /* quiesce: cache flush failed */
#define CHECKPOINT_DISK_ERR_REPROBE -3   /* reattach: drive re-probe failed */
#define CHECKPOINT_DISK_ERR_READ    -4   /* reattach: store region unreadable */
#define CHECKPOINT_DISK_ERR_MAGIC   -5   /* reattach: store superblock magic wrong */

/* Hardware operations for one IDE drive, injected so the reconcile logic can be
 * tested against a mock disk. Each returns 0 on success. read_sector fills a
 * 512-byte buffer with the sector at the given LBA. */
typedef struct {
    int (*flush)(void* ctx);                             /* flush write cache */
    int (*reprobe)(void* ctx);                           /* re-identify drive(s) */
    int (*read_sector)(void* ctx, uint64_t lba, void* buf512);
    void*    ctx;
    uint64_t store_lba;   /* sector of the checkpoint-store superblock */
} checkpoint_disk_ops_t;

/* Reach a durable boundary before a checkpoint: flush the write cache. */
int checkpoint_disk_quiesce(const checkpoint_disk_ops_t* ops);

/* After a restore: re-probe the drive, then confirm the checkpoint-store region
 * is readable and its superblock carries CHECKPOINT_DISK_SB_MAGIC. */
int checkpoint_disk_reattach(const checkpoint_disk_ops_t* ops);

/* Little-endian u64 read of the first 8 bytes of a sector buffer. */
uint64_t checkpoint_disk_read_magic(const void* buf512);

/* Kernel adapter (checkpoint_disk_sync.c): bind the boot IDE drive backing the
 * checkpoint store and register its persist_ops (#143) into the global driver
 * registry. ide is an ide_device_t* (opaque here). Returns 0, or -1 on bad
 * arguments or a full registry. */
int checkpoint_disk_register(void* ide, uint8_t drive, uint64_t store_lba);

#endif /* CHECKPOINT_DISK_H */
