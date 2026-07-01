/* Host-side unit test for the keyframe retention store (#195).
 *
 * Uses an in-memory mock block device to verify the three acceptance criteria:
 *   1. Checkpoints write across N store regions per the ring (retain the last N,
 *      evicting the oldest), each region loadable with its own epoch.
 *   2. The ring index is persisted and reloaded on boot (a fresh store bound to
 *      the same device sees the same retained window), including a rebuild from
 *      the region superblocks when the persisted index is torn.
 *   3. Restore can select an arbitrary retained keyframe by epoch, and a target
 *      before the horizon is rejected.
 *
 * Build: gcc -I../include -o test_keyframe_store \
 *            test_keyframe_store.c ../kernel/keyframe_store.c \
 *            ../kernel/snapshot_store.c ../kernel/keyframe_ring.c
 */

#include <stdint.h>
#include <stdbool.h>

/* Declare the libc bits we use directly, rather than including <string.h> etc.,
 * whose <sys/types.h> ssize_t collides with IKOS's vfs.h (reached via fat.h). */
typedef __SIZE_TYPE__ size_t;
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
extern void* malloc(size_t);
extern void  free(void*);
extern int   printf(const char*, ...);

/* snapshot_store.c calls these as freestanding kernel helpers; map to libc. */
void* kmalloc(size_t size) { return malloc(size); }
void  kfree(void* ptr) { free(ptr); }

#include "keyframe_store.h"

/* ----- Mock block device backed by a flat buffer ----- */

#define MOCK_SECTORS 4096
typedef struct {
    uint8_t data[MOCK_SECTORS * SNAPSHOT_SECTOR_SIZE];
} mock_dev_t;

static int mock_read(void* device, uint32_t sector, uint32_t count, void* buffer) {
    mock_dev_t* m = (mock_dev_t*)device;
    if ((uint64_t)sector + count > MOCK_SECTORS) return -1;
    memcpy(buffer, m->data + (size_t)sector * SNAPSHOT_SECTOR_SIZE,
           (size_t)count * SNAPSHOT_SECTOR_SIZE);
    return 0;
}
static int mock_write(void* device, uint32_t sector, uint32_t count, const void* buffer) {
    mock_dev_t* m = (mock_dev_t*)device;
    if ((uint64_t)sector + count > MOCK_SECTORS) return -1;
    memcpy(m->data + (size_t)sector * SNAPSHOT_SECTOR_SIZE, buffer,
           (size_t)count * SNAPSHOT_SECTOR_SIZE);
    return 0;
}

static mock_dev_t g_mock;
static fat_block_device_t g_bdev;

static fat_block_device_t* make_dev(void) {
    memset(&g_mock, 0, sizeof(g_mock));
    g_bdev.read_sectors = mock_read;
    g_bdev.write_sectors = mock_write;
    g_bdev.sector_size = SNAPSHOT_SECTOR_SIZE;
    g_bdev.total_sectors = MOCK_SECTORS;
    g_bdev.private_data = &g_mock;
    return &g_bdev;
}

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* Store geometry: a 3-sector index plus N regions, each a snapshot store of
 * 2 slots of 10 sectors (1 header + one 9-sector page record). */
#define BASE_SECTOR   20
#define INDEX_SECTORS 3
#define CAPACITY      4
#define REGION_SLOT   10

/* Write one checkpoint holding a single page whose first bytes encode `epoch`,
 * so a later load can prove it read back the right keyframe. */
static int put_checkpoint(keyframe_store_t* ks, uint64_t epoch) {
    static uint8_t page[SNAPSHOT_PAGE_SIZE];
    memset(page, 0, sizeof(page));
    memcpy(page, &epoch, sizeof(epoch));

    snapshot_writer_t w;
    if (keyframe_store_begin(ks, epoch, &w) != KEYFRAME_STORE_OK) return -1;
    if (snapshot_writer_add_page(&w, 1 /*pid*/, 0x1000, 0, page) != SNAPSHOT_OK) return -1;
    return keyframe_store_commit(ks, &w, epoch);
}

/* Load the keyframe nearest to `target` and return the epoch tag stored in its
 * page, or 0 on failure. Also asserts the reader's epoch matches selected. */
static uint64_t get_checkpoint_tag(keyframe_store_t* ks, uint64_t target,
                                   uint64_t* selected_out) {
    static uint8_t page[SNAPSHOT_PAGE_SIZE];
    snapshot_reader_t rd;
    uint64_t selected = 0;
    if (keyframe_store_load_epoch(ks, target, &rd, &selected) != KEYFRAME_STORE_OK) {
        return 0;
    }
    if (selected_out) *selected_out = selected;
    snapshot_page_record_t rec;
    rec.page_data = page;
    if (snapshot_reader_next(&rd, &rec) != SNAPSHOT_OK) return 0;
    uint64_t tag = 0;
    memcpy(&tag, page, sizeof(tag));
    return tag;
}

int main(void) {
    printf("test_keyframe_store\n");

    fat_block_device_t* dev = make_dev();
    keyframe_store_t ks;
    CHECK(keyframe_store_init(&ks, dev, BASE_SECTOR, INDEX_SECTORS, CAPACITY,
                              REGION_SLOT) == KEYFRAME_STORE_OK, "store init");
    CHECK(keyframe_store_format(&ks) == KEYFRAME_STORE_OK, "store format");

    /* --- AC1: write more checkpoints than the ring holds (epochs 1..6) --- */
    for (uint64_t e = 1; e <= 6; e++) {
        CHECK(put_checkpoint(&ks, e) == KEYFRAME_STORE_OK, "checkpoint written");
    }
    const keyframe_ring_t* ring = keyframe_store_ring(&ks);
    CHECK(keyframe_ring_count(ring) == CAPACITY, "ring retains exactly N=4");
    uint64_t oldest = 0, newest = 0;
    keyframe_ring_oldest(ring, &oldest);
    keyframe_ring_newest(ring, &newest);
    CHECK(oldest == 3 && newest == 6, "retained window is epochs 3..6");

    /* --- AC3: select arbitrary retained keyframes by epoch --- */
    uint64_t sel = 0;
    CHECK(get_checkpoint_tag(&ks, 5, &sel) == 5 && sel == 5,
          "exact select: epoch 5 reads back epoch 5's page");
    CHECK(get_checkpoint_tag(&ks, 4, &sel) == 4 && sel == 4,
          "exact select: epoch 4");
    /* A target between keyframes picks the nearest at or before it. */
    CHECK(get_checkpoint_tag(&ks, 100, &sel) == 6 && sel == 6,
          "future target selects newest (epoch 6)");
    /* A target before the horizon (epochs 1,2 were evicted) is rejected. */
    snapshot_reader_t rd;
    CHECK(keyframe_store_load_epoch(&ks, 2, &rd, NULL)
              == KEYFRAME_STORE_ERR_NO_KEYFRAME, "pre-horizon target rejected");

    /* --- AC2: index persists and reloads on a fresh store over same device --- */
    keyframe_store_t ks2;
    CHECK(keyframe_store_init(&ks2, dev, BASE_SECTOR, INDEX_SECTORS, CAPACITY,
                              REGION_SLOT) == KEYFRAME_STORE_OK, "reopen init");
    CHECK(keyframe_store_load_index(&ks2) == KEYFRAME_STORE_OK, "reload index");
    const keyframe_ring_t* ring2 = keyframe_store_ring(&ks2);
    CHECK(keyframe_ring_count(ring2) == CAPACITY, "reloaded window still N=4");
    keyframe_ring_oldest(ring2, &oldest);
    keyframe_ring_newest(ring2, &newest);
    CHECK(oldest == 3 && newest == 6, "reloaded window is epochs 3..6");
    CHECK(get_checkpoint_tag(&ks2, 5, NULL) == 5,
          "reloaded store selects epoch 5 correctly");
    /* Appending on the reloaded store keeps rolling the window forward. */
    CHECK(put_checkpoint(&ks2, 7) == KEYFRAME_STORE_OK, "append epoch 7 after reload");
    keyframe_ring_oldest(keyframe_store_ring(&ks2), &oldest);
    keyframe_ring_newest(keyframe_store_ring(&ks2), &newest);
    CHECK(oldest == 4 && newest == 7, "window rolled to epochs 4..7");

    /* --- AC2 (robustness): torn index rebuilds from region superblocks --- */
    /* Corrupt the persisted index sectors, then reopen: load_index must fall
     * back to scanning regions and recover the same retained window. */
    memset(g_mock.data + (size_t)BASE_SECTOR * SNAPSHOT_SECTOR_SIZE, 0xFF,
           (size_t)INDEX_SECTORS * SNAPSHOT_SECTOR_SIZE);
    keyframe_store_t ks3;
    CHECK(keyframe_store_init(&ks3, dev, BASE_SECTOR, INDEX_SECTORS, CAPACITY,
                              REGION_SLOT) == KEYFRAME_STORE_OK, "reopen for rebuild");
    CHECK(keyframe_store_load_index(&ks3) == KEYFRAME_STORE_OK,
          "load_index rebuilds from regions");
    const keyframe_ring_t* ring3 = keyframe_store_ring(&ks3);
    CHECK(keyframe_ring_count(ring3) == CAPACITY, "rebuilt window has N=4");
    keyframe_ring_oldest(ring3, &oldest);
    keyframe_ring_newest(ring3, &newest);
    CHECK(oldest == 4 && newest == 7, "rebuilt window is epochs 4..7");
    CHECK(get_checkpoint_tag(&ks3, 6, NULL) == 6,
          "rebuilt store selects epoch 6 correctly");
    /* After a rebuild the next checkpoint must evict the oldest (epoch 4). */
    CHECK(put_checkpoint(&ks3, 8) == KEYFRAME_STORE_OK, "append epoch 8 after rebuild");
    keyframe_ring_oldest(keyframe_store_ring(&ks3), &oldest);
    keyframe_ring_newest(keyframe_store_ring(&ks3), &newest);
    CHECK(oldest == 5 && newest == 8, "post-rebuild window rolled to epochs 5..8");

    /* --- param guards --- */
    CHECK(keyframe_store_init(&ks, dev, BASE_SECTOR, 1 /*too small*/, CAPACITY,
                              REGION_SLOT) == KEYFRAME_STORE_ERR_PARAM,
          "undersized index rejected");
    CHECK(keyframe_store_init(&ks, dev, BASE_SECTOR, INDEX_SECTORS, 0 /*bad N*/,
                              REGION_SLOT) == KEYFRAME_STORE_ERR_PARAM,
          "zero capacity rejected");

    printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
