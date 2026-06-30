/* Host-side unit test for the checkpoint writeback pass (#115).
 *
 * Verifies that checkpoint_writeback():
 *   1. Streams every captured page into the snapshot store and commits it
 *      (the checkpoint becomes loadable: superblock flipped, slot valid).
 *   2. Round-trips each captured page's pid / virt_addr / contents to disk.
 *   3. Clears the in-memory capture list and closes the epoch afterwards.
 *
 * The still-clean-page walk is driven by the process manager; this test stubs
 * an empty process list, so writeback persists exactly the captured pages.
 *
 * Build: gcc -I../include -o test_checkpoint_writeback \
 *            test_checkpoint_writeback.c ../kernel/checkpoint.c \
 *            ../kernel/snapshot_store.c ../kernel/checkpoint_extstate.c ../kernel/checkpoint_barrier.c
 */

#include <stdint.h>
#include <stdbool.h>

/* Avoid libc headers (their <sys/types.h> ssize_t clashes with IKOS vfs.h). */
typedef __SIZE_TYPE__ size_t;
extern int   printf(const char*, ...);
extern void* malloc(size_t);
extern void  free(void*);
extern void* aligned_alloc(size_t, size_t);
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
extern int   memcmp(const void*, const void*, size_t);

/* checkpoint.c / snapshot_store.c call these freestanding helpers. */
void* kmalloc(size_t s) { return malloc(s); }
void  kfree(void* p) { free(p); }

#include "checkpoint.h"      /* pulls vmm.h + snapshot_store.h + fat.h */

/* ----- Mocks so checkpoint.c links (none exercised: empty process list) ----- */
pte_t* vmm_get_page_table(vm_space_t* s, uint64_t a, int l, bool c) {
    (void)s; (void)a; (void)l; (void)c; return 0;
}
vm_space_t* vmm_get_current_space(void) { return 0; }
void vmm_flush_tlb_page(uint64_t a) { (void)a; }
uint64_t vmm_get_physical_addr(vm_space_t* s, uint64_t a) { (void)s; (void)a; return 0; }
vm_space_t* vmm_create_address_space(uint32_t pid) { (void)pid; return 0; }
uint64_t vmm_alloc_page(void) { return 0; }
int vmm_map_page(vm_space_t* s, uint64_t v, uint64_t p, uint32_t f) {
    (void)s; (void)v; (void)p; (void)f; return 0;
}
struct process;
int pm_get_process_list(uint32_t* p, uint32_t m, uint32_t* c) {
    (void)p; (void)m; if (c) *c = 0; return 0;
}
struct process* pm_get_process(uint32_t pid) { (void)pid; return 0; }
/* Process-reconstruction stubs (checkpoint_register_kernel, unused here). */
struct process* process_get_by_pid(int pid) { (void)pid; return 0; }
struct process* process_create(const char* a, const char* b) { (void)a; (void)b; return 0; }
int pm_table_add_process(struct process* p) { (void)p; return 0; }
int scheduler_add_process(struct process* p) { (void)p; return 0; }

/* ----- In-memory mock block device ----- */
#define MOCK_SECTORS 4096
static uint8_t g_disk[MOCK_SECTORS * SNAPSHOT_SECTOR_SIZE];

static int mock_read(void* dev, uint32_t sector, uint32_t count, void* buf) {
    (void)dev;
    if ((uint64_t)sector + count > MOCK_SECTORS) return -1;
    memcpy(buf, g_disk + (size_t)sector * SNAPSHOT_SECTOR_SIZE,
           (size_t)count * SNAPSHOT_SECTOR_SIZE);
    return 0;
}
static int mock_write(void* dev, uint32_t sector, uint32_t count, const void* buf) {
    (void)dev;
    if ((uint64_t)sector + count > MOCK_SECTORS) return -1;
    memcpy(g_disk + (size_t)sector * SNAPSHOT_SECTOR_SIZE, buf,
           (size_t)count * SNAPSHOT_SECTOR_SIZE);
    return 0;
}

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

#define NPAGES 3
#define VBASE  0x400000ULL

static void fill_page(uint8_t* p, int i) {
    for (int k = 0; k < (int)PAGE_SIZE; k++) p[k] = (uint8_t)(i * 41 + k * 3 + 7);
}

int main(void) {
    printf("Test: checkpoint_writeback streams captures to disk\n");

    memset(g_disk, 0, sizeof(g_disk));
    fat_block_device_t dev = {0};
    dev.read_sectors = mock_read;
    dev.write_sectors = mock_write;
    dev.sector_size = SNAPSHOT_SECTOR_SIZE;
    dev.total_sectors = MOCK_SECTORS;
    dev.private_data = 0;

    snapshot_store_t store;
    CHECK(snapshot_store_init(&store, &dev, 0, 256) == SNAPSHOT_OK, "store init");
    CHECK(snapshot_store_format(&store) == SNAPSHOT_OK, "store format");

    checkpoint_init();
    CHECK(checkpoint_capture_count() == 0, "no captures after init");

    /* Build NPAGES captures via the real capture path. */
    for (int i = 0; i < NPAGES; i++) {
        uint8_t* page = (uint8_t*)aligned_alloc(PAGE_SIZE, PAGE_SIZE);
        fill_page(page, i);
        pte_t pte = (0x10000ULL + i * 0x1000) | PAGE_PRESENT | PAGE_SNAPSHOT_COW;
        int rc = checkpoint_capture_page(5, VBASE + i * PAGE_SIZE, page, &pte);
        CHECK(rc == CHECKPOINT_OK, "capture page");
        free(page);
    }
    CHECK(checkpoint_capture_count() == NPAGES, "three pages captured");

    /* Writeback. */
    int wb = checkpoint_writeback(&store);
    CHECK(wb == CHECKPOINT_OK, "writeback returns OK");
    CHECK(checkpoint_capture_count() == 0, "capture list cleared after writeback");
    CHECK(checkpoint_get_state()->epoch_open == false, "epoch closed after writeback");

    /* The checkpoint must now load back with all pages intact. */
    snapshot_reader_t reader;
    CHECK(snapshot_store_load(&store, &reader) == SNAPSHOT_OK, "checkpoint loads");
    CHECK(reader.record_count == NPAGES, "slot has all three pages");

    bool seen[NPAGES] = {false, false, false};
    int content_ok = 1, meta_ok = 1, n = 0;
    uint8_t page[PAGE_SIZE], expect[PAGE_SIZE];
    snapshot_page_record_t rec; rec.page_data = page;
    while (snapshot_reader_next(&reader, &rec) == SNAPSHOT_OK) {
        int idx = (int)((rec.virt_addr - VBASE) / PAGE_SIZE);
        if (idx < 0 || idx >= NPAGES) { meta_ok = 0; n++; continue; }
        seen[idx] = true;
        if (rec.pid != 5) meta_ok = 0;
        fill_page(expect, idx);
        if (memcmp(page, expect, PAGE_SIZE) != 0) content_ok = 0;
        n++;
    }
    CHECK(n == NPAGES, "iterated all three records");
    CHECK(seen[0] && seen[1] && seen[2], "every virt_addr present exactly once");
    CHECK(meta_ok, "pid round-trips for every page");
    CHECK(content_ok, "page contents round-trip for every page");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
