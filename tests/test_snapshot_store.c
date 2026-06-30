/* Host-side unit test for the orthogonal-persistence snapshot store (#114).
 *
 * Uses an in-memory mock block device to verify:
 *   1. A new checkpoint is written to the INACTIVE slot (last-good slot
 *      bytes are never touched).
 *   2. A crash BEFORE the superblock flip leaves checkpoint N-1 loadable.
 *   3. A CRC mismatch (corrupted slot) is rejected at load time.
 *
 * Build: gcc -I../include -o test_snapshot_store test_snapshot_store.c
 * (compiled standalone; provides its own kmalloc/kfree/mem* shims).
 */

#include <stdint.h>
#include <stdbool.h>

/* Declare the libc bits we use directly, rather than including <stdlib.h>/
 * <string.h>/<stdio.h>. Those pull in <sys/types.h>, whose ssize_t typedef
 * conflicts with the one in IKOS's vfs.h (reached via fat.h). */
typedef __SIZE_TYPE__ size_t;
extern void* malloc(size_t);
extern void  free(void*);
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
extern int   memcmp(const void*, const void*, size_t);
extern int   printf(const char*, ...);

/* The store calls these as freestanding kernel helpers; map to libc here. */
void* kmalloc(size_t size) { return malloc(size); }
void  kfree(void* ptr) { free(ptr); }

#include "snapshot_store.h"
#include "../kernel/snapshot_store.c"

/* ----- Mock block device backed by a flat buffer ----- */

#define MOCK_SECTORS 4096
typedef struct {
    uint8_t data[MOCK_SECTORS * SNAPSHOT_SECTOR_SIZE];
    int fail_after_writes; /* -1 = never; otherwise abort the Nth+ write */
    int writes;
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
    if (m->fail_after_writes >= 0 && m->writes >= m->fail_after_writes) return -1;
    m->writes++;
    if ((uint64_t)sector + count > MOCK_SECTORS) return -1;
    memcpy(m->data + (size_t)sector * SNAPSHOT_SECTOR_SIZE, buffer,
           (size_t)count * SNAPSHOT_SECTOR_SIZE);
    return 0;
}

static mock_dev_t g_mock;
static fat_block_device_t g_bdev;

static fat_block_device_t* make_dev(void) {
    memset(&g_mock, 0, sizeof(g_mock));
    g_mock.fail_after_writes = -1;
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

/* Fill a page with a recognizable pattern for a given (pid, epoch). */
static void fill_page(uint8_t* p, uint32_t pid, uint64_t epoch) {
    for (int i = 0; i < SNAPSHOT_PAGE_SIZE; i++)
        p[i] = (uint8_t)(pid * 31 + epoch * 7 + i);
}

/* local mirror of the PTE flag value (avoid pulling in vmm.h) */
#define PAGE_SNAPSHOT_COW_FLAG 0x200

static int write_checkpoint(snapshot_store_t* store, uint64_t epoch,
                            uint32_t pid, int npages) {
    snapshot_writer_t w;
    if (snapshot_store_begin(store, epoch, &w) != SNAPSHOT_OK) return -1;
    uint8_t page[SNAPSHOT_PAGE_SIZE];
    for (int i = 0; i < npages; i++) {
        fill_page(page, pid, epoch + i);
        if (snapshot_writer_add_page(&w, pid, 0x400000 + i * 0x1000,
                                     PAGE_SNAPSHOT_COW_FLAG, page) != SNAPSHOT_OK)
            return -1;
    }
    return snapshot_store_commit(&w);
}

int main(void) {
    const uint32_t base = 0, slot_sectors = 256;

    /* === Test 1: inactive-slot isolation === */
    printf("Test 1: new checkpoint goes to the inactive slot\n");
    {
        snapshot_store_t store;
        fat_block_device_t* dev = make_dev();
        snapshot_store_init(&store, dev, base, slot_sectors);
        snapshot_store_format(&store);

        /* Checkpoint A -> slot 0. */
        CHECK(write_checkpoint(&store, 100, 1, 3) == SNAPSHOT_OK, "commit epoch 100");

        /* Snapshot slot 0 bytes, then write checkpoint B (epoch 200). */
        uint32_t slot0_base = 1;                      /* base + 1 */
        uint32_t slot1_base = 1 + slot_sectors;       /* base + 1 + slot_sectors */
        uint8_t slot0_before[slot_sectors * SNAPSHOT_SECTOR_SIZE];
        memcpy(slot0_before, g_mock.data + (size_t)slot0_base * SNAPSHOT_SECTOR_SIZE,
               sizeof(slot0_before));

        CHECK(write_checkpoint(&store, 200, 2, 4) == SNAPSHOT_OK, "commit epoch 200");

        uint8_t slot0_after[slot_sectors * SNAPSHOT_SECTOR_SIZE];
        memcpy(slot0_after, g_mock.data + (size_t)slot0_base * SNAPSHOT_SECTOR_SIZE,
               sizeof(slot0_after));
        CHECK(memcmp(slot0_before, slot0_after, sizeof(slot0_before)) == 0,
              "slot 0 (last-good) untouched while writing checkpoint 200");

        /* Loading should now return epoch 200 from slot 1. */
        snapshot_reader_t r;
        CHECK(snapshot_store_load(&store, &r) == SNAPSHOT_OK, "load latest");
        CHECK(r.epoch == 200, "latest epoch is 200");
        CHECK(r.record_count == 4, "latest has 4 pages");
        (void)slot1_base;

        /* Verify page contents round-trip. */
        uint8_t page[SNAPSHOT_PAGE_SIZE], expect[SNAPSHOT_PAGE_SIZE];
        snapshot_page_record_t rec; rec.page_data = page;
        int idx = 0, content_ok = 1;
        while (snapshot_reader_next(&r, &rec) == SNAPSHOT_OK) {
            fill_page(expect, 2, 200 + idx);
            if (memcmp(page, expect, SNAPSHOT_PAGE_SIZE) != 0) content_ok = 0;
            if (rec.flags != PAGE_SNAPSHOT_COW_FLAG) content_ok = 0;
            idx++;
        }
        CHECK(idx == 4 && content_ok, "all 4 pages round-trip with correct content/flags");
    }

    /* === Test 2: crash before the superblock flip === */
    printf("Test 2: crash before superblock flip keeps checkpoint N-1\n");
    {
        snapshot_store_t store;
        fat_block_device_t* dev = make_dev();
        snapshot_store_init(&store, dev, base, slot_sectors);
        snapshot_store_format(&store);

        CHECK(write_checkpoint(&store, 100, 1, 3) == SNAPSHOT_OK, "commit epoch 100");

        /* Begin checkpoint 200 but make the device fail on the FINAL write
         * (the superblock flip). Count writes for a 2-page checkpoint:
         * 2 records * 9 sectors each = 2 writes/record (meta + page) => 4,
         * + slot header = 5, + superblock = 6. Fail on the 6th (index 5). */
        g_mock.writes = 0;
        g_mock.fail_after_writes = 5; /* allow writes 0..4, fail write #5 (superblock) */

        snapshot_writer_t w;
        snapshot_store_begin(&store, 200, &w);
        uint8_t page[SNAPSHOT_PAGE_SIZE];
        fill_page(page, 2, 200);
        snapshot_writer_add_page(&w, 2, 0x400000, 0, page);
        fill_page(page, 2, 201);
        snapshot_writer_add_page(&w, 2, 0x401000, 0, page);
        int commit_rc = snapshot_store_commit(&w); /* superblock write fails */
        CHECK(commit_rc != SNAPSHOT_OK, "commit reports failure when flip is lost");

        /* Re-open: superblock still points at epoch 100. */
        g_mock.fail_after_writes = -1;
        snapshot_reader_t r;
        CHECK(snapshot_store_load(&store, &r) == SNAPSHOT_OK, "load after crash");
        CHECK(r.epoch == 100, "still see checkpoint 100, not the torn 200");
        CHECK(r.record_count == 3, "checkpoint 100 intact (3 pages)");
    }

    /* === Test 3: CRC mismatch is rejected === */
    printf("Test 3: corrupted slot is rejected at load\n");
    {
        snapshot_store_t store;
        fat_block_device_t* dev = make_dev();
        snapshot_store_init(&store, dev, base, slot_sectors);
        snapshot_store_format(&store);

        CHECK(write_checkpoint(&store, 100, 1, 3) == SNAPSHOT_OK, "commit epoch 100");

        /* Corrupt a byte inside slot 0's first page record (a data sector). */
        uint32_t slot0_base = 1;
        size_t corrupt = (size_t)(slot0_base + 1 + 1) * SNAPSHOT_SECTOR_SIZE + 10;
        g_mock.data[corrupt] ^= 0xFF;

        snapshot_reader_t r;
        int rc = snapshot_store_load(&store, &r);
        CHECK(rc == SNAPSHOT_ERR_CRC, "load rejects corrupted slot with ERR_CRC");
    }

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
