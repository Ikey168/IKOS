/* Host-side unit test for the deterministic-replay input journal (#161).
 *
 * Uses an in-memory mock block device to verify:
 *   1. Events round-trip in recorded order, including across a sector boundary.
 *   2. A crash BEFORE the superblock flip leaves journal N-1 loadable
 *      (the new journal is written to the inactive slot).
 *   3. A CRC mismatch (corrupted event sector) is rejected at load time.
 *   4. An empty journal (zero events) round-trips cleanly.
 *
 * Build: gcc -I../include -o test_checkpoint_journal \
 *            test_checkpoint_journal.c ../kernel/checkpoint_journal.c
 */

#include <stdint.h>
#include <stdbool.h>

/* Declare the libc bits we use directly, rather than including <string.h>/
 * <stdlib.h>/<stdio.h>. Those pull in <sys/types.h>, whose ssize_t typedef
 * conflicts with the one in IKOS's vfs.h (reached via fat.h). */
typedef __SIZE_TYPE__ size_t;
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
extern int   printf(const char*, ...);

#include "checkpoint_journal.h"

/* ----- Mock block device backed by a flat buffer ----- */

#define MOCK_SECTORS 512
typedef struct {
    uint8_t data[MOCK_SECTORS * JOURNAL_SECTOR_SIZE];
    int fail_after_writes; /* -1 = never; otherwise abort the Nth+ write */
    int writes;
} mock_dev_t;

static int mock_read(void* device, uint32_t sector, uint32_t count, void* buffer) {
    mock_dev_t* m = (mock_dev_t*)device;
    if ((uint64_t)sector + count > MOCK_SECTORS) return -1;
    memcpy(buffer, m->data + (size_t)sector * JOURNAL_SECTOR_SIZE,
           (size_t)count * JOURNAL_SECTOR_SIZE);
    return 0;
}
static int mock_write(void* device, uint32_t sector, uint32_t count, const void* buffer) {
    mock_dev_t* m = (mock_dev_t*)device;
    if (m->fail_after_writes >= 0 && m->writes >= m->fail_after_writes) return -1;
    m->writes++;
    if ((uint64_t)sector + count > MOCK_SECTORS) return -1;
    memcpy(m->data + (size_t)sector * JOURNAL_SECTOR_SIZE, buffer,
           (size_t)count * JOURNAL_SECTOR_SIZE);
    return 0;
}

static mock_dev_t g_mock;
static fat_block_device_t g_bdev;

static fat_block_device_t* make_dev(void) {
    memset(&g_mock, 0, sizeof(g_mock));
    g_mock.fail_after_writes = -1;
    g_bdev.read_sectors = mock_read;
    g_bdev.write_sectors = mock_write;
    g_bdev.sector_size = JOURNAL_SECTOR_SIZE;
    g_bdev.total_sectors = MOCK_SECTORS;
    g_bdev.private_data = &g_mock;
    return &g_bdev;
}

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* Store geometry: superblock + two slots, 3 sectors each (1 header + 2 event
 * sectors => 32 events per slot). */
#define BASE_SECTOR 10
#define SLOT_SECTORS 3

/* Deterministic event fields for index i. */
static uint32_t ev_type(uint32_t i)  { return (i % 4) + 1; }         /* JOURNAL_EV_* */
static uint64_t ev_lclock(uint32_t i){ return 1000 + i; }
static uint64_t ev_value(uint32_t i) { return 0xAB00 + i * 7; }

static int write_journal(journal_store_t* store, uint64_t epoch, uint32_t n) {
    journal_writer_t w;
    int rc = journal_store_begin(store, epoch, 500 + epoch, &w);
    if (rc != JOURNAL_OK) return rc;
    for (uint32_t i = 0; i < n; i++) {
        rc = journal_writer_append(&w, ev_type(i), ev_lclock(i), ev_value(i));
        if (rc != JOURNAL_OK) return rc;
    }
    return journal_store_commit(&w);
}

/* Read back and confirm order + fields for n events at the expected epoch. */
static int verify_journal(journal_store_t* store, uint64_t epoch, uint32_t n) {
    journal_reader_t r;
    int rc = journal_store_load(store, &r);
    if (rc != JOURNAL_OK) { printf("    load rc=%d\n", rc); return rc; }
    if (r.epoch != epoch || r.event_count != n) {
        printf("    epoch/count mismatch: got epoch=%llu count=%u\n",
               (unsigned long long)r.epoch, r.event_count);
        return -100;
    }
    for (uint32_t i = 0; i < n; i++) {
        journal_event_t ev;
        rc = journal_reader_next(&r, &ev);
        if (rc != JOURNAL_OK) { printf("    next rc=%d at %u\n", rc, i); return rc; }
        if (ev.epoch != epoch || ev.type != ev_type(i) ||
            ev.lclock != ev_lclock(i) || ev.value != ev_value(i)) {
            printf("    field mismatch at %u\n", i);
            return -101;
        }
    }
    /* Iteration must now be exhausted. */
    journal_event_t tail;
    if (journal_reader_next(&r, &tail) != JOURNAL_ERR_NO_JOURNAL) return -102;
    return JOURNAL_OK;
}

int main(void) {
    printf("=== Input journal (#161) unit test ===\n");

    /* --- 1. Round-trip order, spanning two event sectors (20 > 16) --- */
    {
        journal_store_t store;
        fat_block_device_t* dev = make_dev();
        CHECK(journal_store_init(&store, dev, BASE_SECTOR, SLOT_SECTORS) == JOURNAL_OK,
              "init");
        CHECK(journal_store_format(&store) == JOURNAL_OK, "format");
        CHECK(write_journal(&store, 1, 20) == JOURNAL_OK, "write 20 events (2 sectors)");
        CHECK(verify_journal(&store, 1, 20) == JOURNAL_OK,
              "read back 20 events in order across a sector boundary");
    }

    /* --- 2. Crash before the superblock flip preserves the old journal --- */
    {
        journal_store_t store;
        fat_block_device_t* dev = make_dev();
        journal_store_init(&store, dev, BASE_SECTOR, SLOT_SECTORS);
        journal_store_format(&store);
        CHECK(write_journal(&store, 7, 3) == JOURNAL_OK, "commit journal epoch 7");

        /* Begin epoch 8 into the inactive slot, but fail the superblock flip.
         * Commit order is: event sector(s), slot header, superblock. With 3
         * events that is 1 event sector + 1 header = 2 writes; the 3rd write
         * (the superblock) must fail. */
        g_mock.writes = 0;
        g_mock.fail_after_writes = 2;
        int rc = write_journal(&store, 8, 3);
        CHECK(rc != JOURNAL_OK, "commit of epoch 8 fails at the superblock flip");
        g_mock.fail_after_writes = -1;

        CHECK(verify_journal(&store, 7, 3) == JOURNAL_OK,
              "epoch 7 journal still loads intact after the failed flip");
    }

    /* --- 3. A corrupted event sector is rejected at load --- */
    {
        journal_store_t store;
        fat_block_device_t* dev = make_dev();
        journal_store_init(&store, dev, BASE_SECTOR, SLOT_SECTORS);
        journal_store_format(&store);
        write_journal(&store, 5, 10);

        journal_reader_t r;
        CHECK(journal_store_load(&store, &r) == JOURNAL_OK, "loads before corruption");

        /* Flip a byte in the first event sector of slot 0 (base+1 header, base+2
         * first event sector). */
        uint32_t evsec = (BASE_SECTOR + 1) + 1; /* superblock+1 = slot0 base; +1 = first event sector */
        g_mock.data[(size_t)evsec * JOURNAL_SECTOR_SIZE + 4] ^= 0xFF;

        CHECK(journal_store_load(&store, &r) == JOURNAL_ERR_CRC,
              "corrupted event sector is rejected with JOURNAL_ERR_CRC");
    }

    /* --- 4. Empty journal (zero events) round-trips --- */
    {
        journal_store_t store;
        fat_block_device_t* dev = make_dev();
        journal_store_init(&store, dev, BASE_SECTOR, SLOT_SECTORS);
        journal_store_format(&store);
        CHECK(write_journal(&store, 2, 0) == JOURNAL_OK, "commit empty journal");

        journal_reader_t r;
        CHECK(journal_store_load(&store, &r) == JOURNAL_OK, "empty journal loads");
        CHECK(r.event_count == 0, "empty journal has zero events");
        journal_event_t ev;
        CHECK(journal_reader_next(&r, &ev) == JOURNAL_ERR_NO_JOURNAL,
              "empty journal yields no events");
    }

    /* --- 5. Loading an unformatted region reports no journal --- */
    {
        journal_store_t store;
        fat_block_device_t* dev = make_dev();
        journal_store_init(&store, dev, BASE_SECTOR, SLOT_SECTORS);
        journal_reader_t r;
        CHECK(journal_store_load(&store, &r) == JOURNAL_ERR_NO_JOURNAL,
              "unformatted region reports JOURNAL_ERR_NO_JOURNAL");
    }

    if (failures == 0) {
        printf("PASSED: input journal records and replays inputs in order\n");
        return 0;
    }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
