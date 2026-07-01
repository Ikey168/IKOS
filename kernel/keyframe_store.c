/* IKOS Orthogonal Persistence - Keyframe retention store core (#195)
 *
 * See include/keyframe_store.h. Pure and host-testable: reuses snapshot_store
 * for the per-region page data and keyframe_ring for the index; the only extra
 * commit point is the persisted ring index. No allocator, no hardware.
 */

#include "keyframe_store.h"
#include <stddef.h>

/* Freestanding helper provided by the kernel. */
extern void* memset(void* dest, int value, size_t size);

/* ---- Sector I/O for the index ---- */

static int idx_read(keyframe_store_t* ks, uint32_t sector, void* buf) {
    fat_block_device_t* d = ks->dev;
    if (!d || !d->read_sectors) return KEYFRAME_STORE_ERR_IO;
    return d->read_sectors(d->private_data, sector, 1, buf) == 0
               ? KEYFRAME_STORE_OK : KEYFRAME_STORE_ERR_IO;
}

static int idx_write(keyframe_store_t* ks, uint32_t sector, const void* buf) {
    fat_block_device_t* d = ks->dev;
    if (!d || !d->write_sectors) return KEYFRAME_STORE_ERR_IO;
    return d->write_sectors(d->private_data, sector, 1, buf) == 0
               ? KEYFRAME_STORE_OK : KEYFRAME_STORE_ERR_IO;
}

/* First sector of region `slot`, past the index. */
static uint32_t region_base(const keyframe_store_t* ks, uint32_t slot) {
    return ks->base_sector + ks->index_sectors + slot * ks->region_sectors;
}

/* Re-point the scratch snapshot store at region `slot`. */
static int region_open(keyframe_store_t* ks, uint32_t slot) {
    if (snapshot_store_init(&ks->region, ks->dev, region_base(ks, slot),
                            ks->region_slot_sectors) != SNAPSHOT_OK) {
        return KEYFRAME_STORE_ERR_PARAM;
    }
    snapshot_store_set_version(&ks->region, ks->kernel_version);
    return KEYFRAME_STORE_OK;
}

/* ---- Persisted index (a cache; the regions are the source of truth) ---- */

static int write_index(keyframe_store_t* ks) {
    uint8_t buf[KEYFRAME_STORE_INDEX_MAX_SECTORS * SNAPSHOT_SECTOR_SIZE];
    memset(buf, 0, sizeof(buf));
    if (keyframe_ring_pack(&ks->ring, buf, sizeof(buf)) < 0) {
        return KEYFRAME_STORE_ERR_PARAM;
    }
    for (uint32_t i = 0; i < ks->index_sectors; i++) {
        int rc = idx_write(ks, ks->base_sector + i, buf + i * SNAPSHOT_SECTOR_SIZE);
        if (rc != KEYFRAME_STORE_OK) return rc;
    }
    return KEYFRAME_STORE_OK;
}

/* Try to reload the persisted index. KEYFRAME_STORE_OK if it unpacks and its
 * capacity matches this store's geometry; otherwise a negative code so the
 * caller falls back to a rebuild. */
static int read_index(keyframe_store_t* ks) {
    uint8_t buf[KEYFRAME_STORE_INDEX_MAX_SECTORS * SNAPSHOT_SECTOR_SIZE];
    memset(buf, 0, sizeof(buf));
    for (uint32_t i = 0; i < ks->index_sectors; i++) {
        int rc = idx_read(ks, ks->base_sector + i, buf + i * SNAPSHOT_SECTOR_SIZE);
        if (rc != KEYFRAME_STORE_OK) return rc;
    }
    keyframe_ring_t tmp;
    if (keyframe_ring_unpack(&tmp, buf, sizeof(buf)) != KEYFRAME_RING_OK) {
        return KEYFRAME_STORE_ERR_CRC;
    }
    if (tmp.capacity != ks->capacity) {
        return KEYFRAME_STORE_ERR_PARAM; /* geometry changed: rebuild instead */
    }
    ks->ring = tmp;
    return KEYFRAME_STORE_OK;
}

/* Rebuild the ring by probing each region's superblock (self-describing with
 * its epoch), then persist the rebuilt cache. */
static int rebuild_ring(keyframe_store_t* ks) {
    keyframe_ring_init(&ks->ring, ks->capacity);

    uint32_t first_empty = ks->capacity; /* none */
    bool have_empty = false;
    uint64_t oldest = 0;
    uint32_t oldest_slot = 0;
    bool any = false;

    for (uint32_t i = 0; i < ks->capacity; i++) {
        if (region_open(ks, i) != KEYFRAME_STORE_OK) return KEYFRAME_STORE_ERR_IO;
        snapshot_reader_t rd;
        if (snapshot_store_load(&ks->region, &rd) == SNAPSHOT_OK) {
            ks->ring.slots[i].epoch = rd.epoch;
            ks->ring.slots[i].valid = 1;
            ks->ring.count++;
            if (!any || rd.epoch < oldest) { oldest = rd.epoch; oldest_slot = i; any = true; }
        } else if (!have_empty) {
            first_empty = i;
            have_empty = true;
        }
    }

    /* Next region to claim: the first empty slot, or (when full) the one holding
     * the oldest epoch, matching the ring's evict-oldest order. */
    ks->ring.head = have_empty ? first_empty : (any ? oldest_slot : 0);
    return write_index(ks);
}

/* ---- Public API ---- */

uint32_t keyframe_store_total_sectors(uint32_t index_sectors, uint32_t capacity,
                                      uint32_t region_slot_sectors) {
    uint32_t region_sectors = 1 + 2 * region_slot_sectors;
    return index_sectors + capacity * region_sectors;
}

int keyframe_store_init(keyframe_store_t* ks, fat_block_device_t* dev,
                        uint32_t base_sector, uint32_t index_sectors,
                        uint32_t capacity, uint32_t region_slot_sectors) {
    if (!ks || !dev) return KEYFRAME_STORE_ERR_PARAM;
    if (capacity == 0 || capacity > KEYFRAME_RING_MAX) return KEYFRAME_STORE_ERR_PARAM;
    if (region_slot_sectors < SNAPSHOT_SECTORS_PER_RECORD + 1) {
        return KEYFRAME_STORE_ERR_PARAM; /* need room for the header + a record */
    }
    /* The index must hold the packed ring and stay within the I/O buffer bound. */
    uint32_t need = (keyframe_ring_packed_size() + SNAPSHOT_SECTOR_SIZE - 1)
                    / SNAPSHOT_SECTOR_SIZE;
    if (index_sectors < need || index_sectors > KEYFRAME_STORE_INDEX_MAX_SECTORS) {
        return KEYFRAME_STORE_ERR_PARAM;
    }

    memset(ks, 0, sizeof(*ks));
    ks->dev = dev;
    ks->base_sector = base_sector;
    ks->index_sectors = index_sectors;
    ks->capacity = capacity;
    ks->region_slot_sectors = region_slot_sectors;
    ks->region_sectors = 1 + 2 * region_slot_sectors;
    ks->kernel_version = 0;
    keyframe_ring_init(&ks->ring, capacity);
    ks->initialized = true;
    return KEYFRAME_STORE_OK;
}

void keyframe_store_set_version(keyframe_store_t* ks, uint32_t kernel_version) {
    if (ks) ks->kernel_version = kernel_version;
}

int keyframe_store_format(keyframe_store_t* ks) {
    if (!ks || !ks->initialized) return KEYFRAME_STORE_ERR_STATE;
    keyframe_ring_init(&ks->ring, ks->capacity);
    for (uint32_t i = 0; i < ks->capacity; i++) {
        if (region_open(ks, i) != KEYFRAME_STORE_OK) return KEYFRAME_STORE_ERR_IO;
        if (snapshot_store_format(&ks->region) != SNAPSHOT_OK) return KEYFRAME_STORE_ERR_IO;
    }
    return write_index(ks);
}

int keyframe_store_load_index(keyframe_store_t* ks) {
    if (!ks || !ks->initialized) return KEYFRAME_STORE_ERR_STATE;
    if (read_index(ks) == KEYFRAME_STORE_OK) return KEYFRAME_STORE_OK;
    return rebuild_ring(ks);
}

int keyframe_store_begin(keyframe_store_t* ks, uint64_t epoch,
                         snapshot_writer_t* writer) {
    if (!ks || !ks->initialized || !writer) return KEYFRAME_STORE_ERR_PARAM;
    /* Peek the slot the next advance will claim; do not mutate the ring until
     * the region has committed (so a failed write keeps the old window). */
    uint32_t slot = ks->ring.head;
    if (region_open(ks, slot) != KEYFRAME_STORE_OK) return KEYFRAME_STORE_ERR_PARAM;
    if (snapshot_store_begin(&ks->region, epoch, writer) != SNAPSHOT_OK) {
        return KEYFRAME_STORE_ERR_IO;
    }
    return KEYFRAME_STORE_OK;
}

int keyframe_store_commit(keyframe_store_t* ks, snapshot_writer_t* writer,
                          uint64_t epoch) {
    if (!ks || !ks->initialized || !writer) return KEYFRAME_STORE_ERR_PARAM;
    if (snapshot_store_commit(writer) != SNAPSHOT_OK) {
        return KEYFRAME_STORE_ERR_IO; /* ring/index untouched: old window survives */
    }
    /* Region durably committed: fold the epoch into the ring (claims ring.head,
     * the region we just wrote) and republish the index. */
    keyframe_ring_advance(&ks->ring, epoch);
    return write_index(ks);
}

int keyframe_store_load_epoch(keyframe_store_t* ks, uint64_t target,
                              snapshot_reader_t* reader, uint64_t* epoch_out) {
    if (!ks || !ks->initialized || !reader) return KEYFRAME_STORE_ERR_PARAM;
    uint32_t slot = 0;
    uint64_t e = 0;
    if (!keyframe_ring_find(&ks->ring, target, &slot, &e)) {
        return KEYFRAME_STORE_ERR_NO_KEYFRAME;
    }
    if (region_open(ks, slot) != KEYFRAME_STORE_OK) return KEYFRAME_STORE_ERR_PARAM;
    if (snapshot_store_load(&ks->region, reader) != SNAPSHOT_OK) {
        return KEYFRAME_STORE_ERR_IO;
    }
    if (reader->epoch != e) return KEYFRAME_STORE_ERR_CRC; /* index/region disagree */
    if (epoch_out) *epoch_out = e;
    return KEYFRAME_STORE_OK;
}

int keyframe_store_load_latest(keyframe_store_t* ks, snapshot_reader_t* reader,
                               uint64_t* epoch_out) {
    if (!ks || !ks->initialized || !reader) return KEYFRAME_STORE_ERR_PARAM;
    uint64_t newest = 0;
    if (!keyframe_ring_newest(&ks->ring, &newest)) {
        return KEYFRAME_STORE_ERR_NO_KEYFRAME;
    }
    return keyframe_store_load_epoch(ks, newest, reader, epoch_out);
}

const keyframe_ring_t* keyframe_store_ring(const keyframe_store_t* ks) {
    return ks ? &ks->ring : NULL;
}

snapshot_store_t* keyframe_store_region_for(keyframe_store_t* ks, uint64_t target,
                                            uint64_t* epoch_out) {
    if (!ks || !ks->initialized) return NULL;
    uint32_t slot = 0;
    uint64_t e = 0;
    if (!keyframe_ring_find(&ks->ring, target, &slot, &e)) return NULL;
    if (region_open(ks, slot) != KEYFRAME_STORE_OK) return NULL;
    if (epoch_out) *epoch_out = e;
    return &ks->region;
}
