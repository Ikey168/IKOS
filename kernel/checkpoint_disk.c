/* IKOS Orthogonal Persistence v2 - Disk (IDE) driver re-attach (#144)
 *
 * Pure reconcile core. No kernel deps. See include/checkpoint_disk.h.
 */

#include "checkpoint_disk.h"

uint64_t checkpoint_disk_read_magic(const void* buf512) {
    const unsigned char* b = (const unsigned char*)buf512;
    uint64_t m = 0;
    for (int i = 7; i >= 0; i--) {
        m = (m << 8) | (uint64_t)b[i];
    }
    return m;
}

int checkpoint_disk_quiesce(const checkpoint_disk_ops_t* ops) {
    if (!ops) {
        return CHECKPOINT_DISK_ERR_PARAM;
    }
    /* Nothing to flush is fine; a real flush that fails is not. */
    if (ops->flush && ops->flush(ops->ctx) != 0) {
        return CHECKPOINT_DISK_ERR_FLUSH;
    }
    return CHECKPOINT_DISK_OK;
}

int checkpoint_disk_reattach(const checkpoint_disk_ops_t* ops) {
    if (!ops || !ops->read_sector) {
        return CHECKPOINT_DISK_ERR_PARAM;
    }
    /* Re-probe first: the controller and drive registers are meaningless after
     * a power cut, so re-identify before trusting any read. */
    if (ops->reprobe && ops->reprobe(ops->ctx) != 0) {
        return CHECKPOINT_DISK_ERR_REPROBE;
    }
    unsigned char sector[512];
    if (ops->read_sector(ops->ctx, ops->store_lba, sector) != 0) {
        return CHECKPOINT_DISK_ERR_READ;
    }
    /* The store region is only trustworthy if the superblock magic is intact:
     * confirms we re-attached the disk we actually checkpointed on. */
    if (checkpoint_disk_read_magic(sector) != CHECKPOINT_DISK_SB_MAGIC) {
        return CHECKPOINT_DISK_ERR_MAGIC;
    }
    return CHECKPOINT_DISK_OK;
}
