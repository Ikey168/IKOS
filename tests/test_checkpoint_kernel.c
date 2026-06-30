/* Host-side test for kernel-state records + superblock version stamp (#139).
 *
 * 1. A checkpoint whose superblock kernel-version stamp differs from the store's
 *    expected version is rejected with SNAPSHOT_ERR_VERSION (cold boot).
 * 2. An arbitrary-size kernel-state blob round-trips: checkpoint_capture_kernel_blob
 *    chunks it into CHECKPOINT_REC_KERNEL records, writeback commits them, and a
 *    fresh load reassembles the exact bytes (size + chunk index decoded from each
 *    record's virt_addr).
 *
 * Build: gcc -I../include -o test_checkpoint_kernel test_checkpoint_kernel.c \
 *            ../kernel/checkpoint.c ../kernel/snapshot_store.c \
 *            ../kernel/checkpoint_extstate.c ../kernel/checkpoint_barrier.c
 */

#include <stdint.h>
#include <stdbool.h>

typedef __SIZE_TYPE__ size_t;
extern int   printf(const char*, ...);
extern void* malloc(size_t);
extern void  free(void*);
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
extern int   memcmp(const void*, const void*, size_t);

void* kmalloc(size_t s) { return malloc(s); }
void  kfree(void* p) { free(p); }

#include "checkpoint.h"

/* Engine link stubs (empty process list). */
pte_t* vmm_get_page_table(vm_space_t* s, uint64_t a, int l, bool c) { (void)s;(void)a;(void)l;(void)c; return 0; }
vm_space_t* vmm_get_current_space(void) { return 0; }
void vmm_flush_tlb_page(uint64_t a) { (void)a; }
uint64_t vmm_get_physical_addr(vm_space_t* s, uint64_t a) { (void)s;(void)a; return 0; }
vm_space_t* vmm_create_address_space(uint32_t pid) { (void)pid; return 0; }
uint64_t vmm_alloc_page(void) { return 0; }
int vmm_map_page(vm_space_t* s, uint64_t v, uint64_t p, uint32_t f) { (void)s;(void)v;(void)p;(void)f; return 0; }
struct process;
int pm_get_process_list(uint32_t* p, uint32_t m, uint32_t* c) { (void)p;(void)m; if (c) *c = 0; return 0; }
struct process* pm_get_process(uint32_t pid) { (void)pid; return 0; }
struct process* process_get_by_pid(int pid) { (void)pid; return 0; }
struct process* process_create(const char* a, const char* b) { (void)a;(void)b; return 0; }
int pm_table_add_process(struct process* p) { (void)p; return 0; }
int scheduler_add_process(struct process* p) { (void)p; return 0; }

/* Mock block device. */
#define MOCK_SECTORS 4096
static uint8_t g_disk[MOCK_SECTORS * SNAPSHOT_SECTOR_SIZE];
static int mock_read(void* d, uint32_t s, uint32_t n, void* b) {
    (void)d; if ((uint64_t)s + n > MOCK_SECTORS) return -1;
    memcpy(b, g_disk + (size_t)s * SNAPSHOT_SECTOR_SIZE, (size_t)n * SNAPSHOT_SECTOR_SIZE); return 0;
}
static int mock_write(void* d, uint32_t s, uint32_t n, const void* b) {
    (void)d; if ((uint64_t)s + n > MOCK_SECTORS) return -1;
    memcpy(g_disk + (size_t)s * SNAPSHOT_SECTOR_SIZE, b, (size_t)n * SNAPSHOT_SECTOR_SIZE); return 0;
}

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

static void mkdev(fat_block_device_t* dev) {
    memset(g_disk, 0, sizeof(g_disk));
    memset(dev, 0, sizeof(*dev));
    dev->read_sectors = mock_read; dev->write_sectors = mock_write;
    dev->sector_size = SNAPSHOT_SECTOR_SIZE; dev->total_sectors = MOCK_SECTORS;
}

int main(void) {
    /* === 1. Version stamp rejects a mismatched kernel build === */
    printf("Test 1: kernel-version stamp\n");
    {
        fat_block_device_t dev; mkdev(&dev);
        snapshot_store_t store;
        snapshot_store_init(&store, &dev, 0, 256);
        snapshot_store_set_version(&store, 100);
        snapshot_store_format(&store);

        /* Write a real checkpoint (stamped version 100). */
        snapshot_writer_t w;
        snapshot_store_begin(&store, 1, &w);
        uint8_t page[SNAPSHOT_PAGE_SIZE]; memset(page, 0xAB, sizeof(page));
        snapshot_writer_add_page(&w, 1, 0x1000, 0, page);
        snapshot_store_commit(&w);

        snapshot_reader_t r;
        CHECK(snapshot_store_load(&store, &r) == SNAPSHOT_OK, "matching version loads");

        snapshot_store_set_version(&store, 200); /* pretend a kernel upgrade */
        CHECK(snapshot_store_load(&store, &r) == SNAPSHOT_ERR_VERSION, "mismatched version rejected");

        snapshot_store_set_version(&store, 100);
        CHECK(snapshot_store_load(&store, &r) == SNAPSHOT_OK, "matching version loads again");
    }

    /* === 2. Kernel-state blob round-trips through the store === */
    printf("Test 2: kernel-state blob\n");
    {
        fat_block_device_t dev; mkdev(&dev);
        snapshot_store_t store;
        snapshot_store_init(&store, &dev, 0, 256); /* default version 0 */
        snapshot_store_format(&store);

        checkpoint_init();

        /* A 9000-byte blob spans 3 pages (4096 + 4096 + 808). */
        const uint32_t SZ = 9000;
        static uint8_t blob[9000];
        for (uint32_t i = 0; i < SZ; i++) blob[i] = (uint8_t)(i * 37 + 11);

        checkpoint_take(); /* advance epoch (pm empty) */
        CHECK(checkpoint_capture_kernel_blob(0xBEEF, blob, SZ) == CHECKPOINT_OK, "capture blob");
        CHECK(checkpoint_capture_count() == 3, "blob chunked into 3 records");
        CHECK(checkpoint_capture_kernel_blob(0xBEEF, 0, SZ) == CHECKPOINT_ERR_PARAM, "NULL data rejected");

        CHECK(checkpoint_writeback(&store) == CHECKPOINT_OK, "writeback commits");

        /* Reload and reassemble from the kernel records. */
        snapshot_reader_t r;
        CHECK(snapshot_store_load(&store, &r) == SNAPSHOT_OK, "load");

        static uint8_t reasm[16384];
        memset(reasm, 0, sizeof(reasm));
        uint8_t page[SNAPSHOT_PAGE_SIZE];
        snapshot_page_record_t rec; rec.page_data = page;
        uint32_t total = 0, chunks = 0, tag_ok = 1;
        while (snapshot_reader_next(&r, &rec) == SNAPSHOT_OK) {
            if (!(rec.flags & CHECKPOINT_REC_KERNEL)) continue;
            if (rec.pid != 0xBEEF) tag_ok = 0;
            uint32_t sz = (uint32_t)(rec.virt_addr >> 32);
            uint32_t idx = (uint32_t)(rec.virt_addr & 0xFFFFFFFFu);
            total = sz;
            uint32_t off = idx * SNAPSHOT_PAGE_SIZE;
            uint32_t n = (sz - off) < SNAPSHOT_PAGE_SIZE ? (sz - off) : SNAPSHOT_PAGE_SIZE;
            memcpy(reasm + off, page, n);
            chunks++;
        }
        CHECK(chunks == 3, "reassembled 3 chunks");
        CHECK(tag_ok, "every chunk carries the tag");
        CHECK(total == SZ, "decoded total size matches");
        CHECK(memcmp(reasm, blob, SZ) == 0, "blob bytes round-trip exactly");
    }

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
