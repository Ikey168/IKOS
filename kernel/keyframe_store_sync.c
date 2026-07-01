/* IKOS Orthogonal Persistence - Keyframe store kernel adapter (#195)
 *
 * See include/keyframe_store.h. Binds a keyframe retention store to a region of
 * the persistence device and drives a full checkpoint writeback through the
 * ring: claim the next region, stream the epoch's pages into it (reusing the
 * checkpoint engine's page walk), commit the region, and republish the ring
 * index. Kept out of keyframe_store.c so that core stays dependency-free and
 * host-testable.
 */

#include "keyframe_store.h"
#include "checkpoint.h"   /* checkpoint_current_epoch, stream_pages, after_commit */
#include <stddef.h>

/* Caller-allocated storage lives here so no allocator is needed. */
static keyframe_store_t g_keyframe_store;
static bool             g_keyframe_ready = false;

int keyframe_store_arm(fat_block_device_t* dev, uint32_t base_sector,
                       uint32_t index_sectors, uint32_t capacity,
                       uint32_t region_slot_sectors) {
    if (keyframe_store_init(&g_keyframe_store, dev, base_sector, index_sectors,
                            capacity, region_slot_sectors) != KEYFRAME_STORE_OK) {
        return KEYFRAME_STORE_ERR_PARAM;
    }
    /* Reload the retained window if one is present; otherwise provision a fresh
     * store. keyframe_store_load_index reads the persisted index (rebuilding
     * from region superblocks if it is torn). A brand-new device has neither,
     * so format when the reload comes back empty. */
    if (keyframe_store_load_index(&g_keyframe_store) != KEYFRAME_STORE_OK ||
        keyframe_ring_count(keyframe_store_ring(&g_keyframe_store)) == 0) {
        if (keyframe_store_format(&g_keyframe_store) != KEYFRAME_STORE_OK) {
            return KEYFRAME_STORE_ERR_IO;
        }
    }
    g_keyframe_ready = true;
    return KEYFRAME_STORE_OK;
}

keyframe_store_t* keyframe_store_get(void) {
    return g_keyframe_ready ? &g_keyframe_store : NULL;
}

int checkpoint_writeback_keyframe(void) {
    if (!g_keyframe_ready) return CHECKPOINT_ERR_PARAM;

    uint64_t epoch = checkpoint_current_epoch();

    snapshot_writer_t writer;
    if (keyframe_store_begin(&g_keyframe_store, epoch, &writer) != KEYFRAME_STORE_OK) {
        return CHECKPOINT_ERR_IO;
    }

    int rc = checkpoint_stream_pages(&writer);
    if (rc != CHECKPOINT_OK) {
        return rc; /* writer un-committed: the retained window is untouched */
    }

    if (keyframe_store_commit(&g_keyframe_store, &writer, epoch) != KEYFRAME_STORE_OK) {
        return CHECKPOINT_ERR_IO;
    }

    /* Region committed and the ring index republished: finish the epoch
     * (journal hook + drop captures + close). */
    checkpoint_after_commit(epoch);
    return CHECKPOINT_OK;
}
