/* IKOS Orthogonal Persistence - Keyframe retention store (#195, epic #159)
 *
 * The snapshot store (snapshot_store.h) keeps two double-buffered slots: the
 * latest checkpoint and the previous one, which is enough to resume on boot but
 * not to rewind to any of the last N moments. This layer spreads checkpoints
 * across N independent store regions, driven by the keyframe retention ring
 * (#168): each checkpoint claims the ring's next slot and is written into that
 * region, the ring index is persisted (its CRC-protected pack) so the retained
 * window survives a reboot, and restore can select any retained keyframe by
 * epoch rather than only the latest.
 *
 * Each region is an ordinary snapshot_store, so all page-record read/write/CRC
 * and per-region crash-consistency (inactive-slot write + superblock flip) are
 * reused unchanged. The one added commit point is the ring index: a checkpoint
 * is folded into the in-memory ring and the index is rewritten only after its
 * region has durably committed, so a crash mid-write leaves the previously
 * retained window intact. The index is a cache: if it is torn or absent at
 * boot, the ring is rebuilt by scanning the region superblocks (each is
 * self-describing with its epoch), so the retained window is never lost to a
 * torn index.
 *
 * Pure and host-testable: it talks to storage only through a
 * fat_block_device_t (for the index sectors) and through snapshot_store /
 * keyframe_ring, and carries its own buffers, so it needs no allocator.
 *
 * See docs/architecture/time-travel.md.
 */

#ifndef KEYFRAME_STORE_H
#define KEYFRAME_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include "fat.h"             /* fat_block_device_t */
#include "snapshot_store.h"  /* snapshot_store_t, writer/reader */
#include "keyframe_ring.h"   /* keyframe_ring_t */

/* The packed ring index never exceeds this many sectors (keyframe_ring_pack is
 * about 1060 bytes => 3 sectors; 4 leaves headroom and bounds the I/O buffer). */
#define KEYFRAME_STORE_INDEX_MAX_SECTORS 4

/* Error codes (distinct from SNAPSHOT_* / KEYFRAME_RING_*). */
#define KEYFRAME_STORE_OK            0
#define KEYFRAME_STORE_ERR_PARAM    -1
#define KEYFRAME_STORE_ERR_IO       -2
#define KEYFRAME_STORE_ERR_CRC      -3   /* index/region epoch disagree */
#define KEYFRAME_STORE_ERR_NO_KEYFRAME -4 /* target predates the retained window */
#define KEYFRAME_STORE_ERR_STATE    -5

typedef struct {
    fat_block_device_t* dev;
    uint32_t base_sector;         /* first index sector */
    uint32_t index_sectors;       /* sectors reserved for the packed ring index */
    uint32_t capacity;            /* N retained keyframes */
    uint32_t region_slot_sectors; /* per-slot sectors inside each region store */
    uint32_t region_sectors;      /* derived: 1 + 2 * region_slot_sectors */
    uint32_t kernel_version;      /* stamped into every region store (#139) */
    keyframe_ring_t ring;         /* in-memory index, persisted to the index sectors */
    snapshot_store_t region;      /* scratch store, re-pointed per region */
    bool     initialized;
} keyframe_store_t;

/* Sectors a keyframe store of `capacity` regions (each region_slot_sectors per
 * slot) occupies from base_sector, including index_sectors. Callers use this to
 * lay the store out without overlapping neighbours. */
uint32_t keyframe_store_total_sectors(uint32_t index_sectors, uint32_t capacity,
                                      uint32_t region_slot_sectors);

/* Bind a keyframe store to a device region. index_sectors must be at least the
 * packed-index size (see KEYFRAME_STORE_INDEX_MAX_SECTORS) and no larger than
 * that bound; capacity is 1..KEYFRAME_RING_MAX; region_slot_sectors sizes each
 * region's snapshot store (>= 1 + 9 so a page record fits). Writes nothing. */
int keyframe_store_init(keyframe_store_t* ks, fat_block_device_t* dev,
                        uint32_t base_sector, uint32_t index_sectors,
                        uint32_t capacity, uint32_t region_slot_sectors);

/* Stamp every region store with the kernel build version (#139). */
void keyframe_store_set_version(keyframe_store_t* ks, uint32_t kernel_version);

/* Provision a brand-new store: an empty ring, a formatted (empty) superblock in
 * every region, and a fresh persisted index. */
int keyframe_store_format(keyframe_store_t* ks);

/* Reload the retained window at boot. Unpacks the persisted index when it is
 * intact; otherwise rebuilds the ring by scanning the region superblocks (and
 * rewrites the index cache). Returns KEYFRAME_STORE_OK, or a negative code. */
int keyframe_store_load_index(keyframe_store_t* ks);

/* Begin a checkpoint at `epoch`: claim the ring's next region and open a
 * snapshot writer on it. Add pages with snapshot_writer_add_page, then finish
 * with keyframe_store_commit. */
int keyframe_store_begin(keyframe_store_t* ks, uint64_t epoch,
                         snapshot_writer_t* writer);

/* Commit the checkpoint opened by keyframe_store_begin: durably commit the
 * region (superblock flip), then fold the epoch into the ring and rewrite the
 * persisted index. A region-commit failure leaves the ring and index untouched
 * so the previous window survives. */
int keyframe_store_commit(keyframe_store_t* ks, snapshot_writer_t* writer,
                          uint64_t epoch);

/* Open the nearest retained keyframe at or before `target` for reading, filling
 * *epoch_out (may be NULL) with the epoch actually selected. Returns
 * KEYFRAME_STORE_ERR_NO_KEYFRAME when target predates the retained window. */
int keyframe_store_load_epoch(keyframe_store_t* ks, uint64_t target,
                              snapshot_reader_t* reader, uint64_t* epoch_out);

/* Open the newest retained keyframe (boot resume). */
int keyframe_store_load_latest(keyframe_store_t* ks, snapshot_reader_t* reader,
                               uint64_t* epoch_out);

/* The in-memory ring index (retained epochs, horizon), for logging and tests. */
const keyframe_ring_t* keyframe_store_ring(const keyframe_store_t* ks);

/* Select the nearest retained keyframe at or before `target` and open its
 * region store (re-pointing the store's internal scratch), returning that store
 * so the caller can drive the checkpoint restore path (checkpoint_restore_boot)
 * against it. Fills *epoch_out (may be NULL) with the epoch selected. Returns
 * NULL if `target` predates the retained window. The returned pointer is owned
 * by the keyframe store and is only valid until the next region operation. */
snapshot_store_t* keyframe_store_region_for(keyframe_store_t* ks, uint64_t target,
                                            uint64_t* epoch_out);

/* ---- Kernel adapter (keyframe_store_sync.c) ----
 * Binds a process-wide keyframe store to a device region and reloads (or
 * formats) its retained window. Call once at boot with a non-overlapping
 * region. */
int keyframe_store_arm(fat_block_device_t* dev, uint32_t base_sector,
                       uint32_t index_sectors, uint32_t capacity,
                       uint32_t region_slot_sectors);

/* The armed keyframe store, or NULL if keyframe_store_arm has not run. */
keyframe_store_t* keyframe_store_get(void);

/* Run a full checkpoint writeback through the retention ring: claim the next
 * region, stream the current epoch's pages into it, commit the region, and
 * republish the ring index. Returns CHECKPOINT_OK or a negative CHECKPOINT_ERR_*
 * (see checkpoint.h). */
int checkpoint_writeback_keyframe(void);

#endif /* KEYFRAME_STORE_H */
