/* Host-side unit test for the checkpoint restore engine (#116).
 *
 * Verifies that checkpoint_restore():
 *   1. Replays every page of the latest valid checkpoint through the apply
 *      callback, in slot order, with correct pid / virt_addr / contents.
 *   2. Restores the global epoch to the checkpoint's epoch.
 *   3. Reports CHECKPOINT_ERR_NO_CHECKPOINT on an empty store (cold-boot path).
 *
 * The checkpoint is laid down directly via the snapshot store writer, then
 * read back by the restore loop, so this also exercises the on-disk format
 * end to end.
 *
 * Build: gcc -I../include -o test_checkpoint_restore \
 *            test_checkpoint_restore.c ../kernel/checkpoint.c \
 *            ../kernel/snapshot_store.c ../kernel/checkpoint_extstate.c ../kernel/checkpoint_barrier.c
 */

#include <stdint.h>
#include <stdbool.h>

/* Avoid libc headers (their <sys/types.h> ssize_t clashes with IKOS vfs.h). */
typedef __SIZE_TYPE__ size_t;
extern int   printf(const char*, ...);
extern void* malloc(size_t);
extern void  free(void*);
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
extern int   memcmp(const void*, const void*, size_t);

void* kmalloc(size_t s) { return malloc(s); }
void  kfree(void* p) { free(p); }

#include "checkpoint.h"   /* pulls vmm.h + snapshot_store.h + fat.h */

/* ----- Stubs so checkpoint.c links (none exercised by the restore core) ----- */
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
static int mock_read(void* d, uint32_t s, uint32_t n, void* b) {
    (void)d; if ((uint64_t)s + n > MOCK_SECTORS) return -1;
    memcpy(b, g_disk + (size_t)s * SNAPSHOT_SECTOR_SIZE, (size_t)n * SNAPSHOT_SECTOR_SIZE);
    return 0;
}
static int mock_write(void* d, uint32_t s, uint32_t n, const void* b) {
    (void)d; if ((uint64_t)s + n > MOCK_SECTORS) return -1;
    memcpy(g_disk + (size_t)s * SNAPSHOT_SECTOR_SIZE, b, (size_t)n * SNAPSHOT_SECTOR_SIZE);
    return 0;
}

/* ----- Mock apply: record what the restore loop replays ----- */
#define MAXREC 16
static struct { uint32_t pid; uint64_t va; uint8_t first; uint8_t last; } g_applied[MAXREC];
static int g_applied_n;
static int g_apply_fail_at = -1; /* abort on the Nth apply if >= 0 */

static int mock_apply(void* ctx, const snapshot_page_record_t* rec) {
    (void)ctx;
    if (g_apply_fail_at >= 0 && g_applied_n >= g_apply_fail_at) return CHECKPOINT_ERR_PARAM;
    const uint8_t* p = (const uint8_t*)rec->page_data;
    g_applied[g_applied_n].pid = rec->pid;
    g_applied[g_applied_n].va = rec->virt_addr;
    g_applied[g_applied_n].first = p[0];
    g_applied[g_applied_n].last = p[SNAPSHOT_PAGE_SIZE - 1];
    g_applied_n++;
    return CHECKPOINT_OK;
}

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

#define VBASE 0x800000ULL
#define EPOCH 9

static fat_block_device_t make_dev(void) {
    memset(g_disk, 0, sizeof(g_disk));
    fat_block_device_t d = {0};
    d.read_sectors = mock_read; d.write_sectors = mock_write;
    d.sector_size = SNAPSHOT_SECTOR_SIZE; d.total_sectors = MOCK_SECTORS;
    return d;
}

/* Lay down a checkpoint of `n` pages, pid `pid`, epoch EPOCH. */
static void write_checkpoint(snapshot_store_t* store, uint32_t pid, int n) {
    snapshot_writer_t w;
    snapshot_store_begin(store, EPOCH, &w);
    uint8_t page[SNAPSHOT_PAGE_SIZE];
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < SNAPSHOT_PAGE_SIZE; k++) page[k] = (uint8_t)(i * 13 + k);
        snapshot_writer_add_page(&w, pid, VBASE + i * SNAPSHOT_PAGE_SIZE, 0, page);
    }
    snapshot_store_commit(&w);
}

int main(void) {
    /* === Test 1: cold-boot path on an empty store === */
    printf("Test 1: empty store => cold boot\n");
    {
        fat_block_device_t dev = make_dev();
        snapshot_store_t store;
        snapshot_store_init(&store, &dev, 0, 256);
        snapshot_store_format(&store);

        checkpoint_init();
        g_applied_n = 0; g_apply_fail_at = -1;
        int rc = checkpoint_restore(&store, mock_apply, 0);
        CHECK(rc == CHECKPOINT_ERR_NO_CHECKPOINT, "no checkpoint reported (cold boot)");
        CHECK(g_applied_n == 0, "apply never called");
    }

    /* === Test 2: restore replays all pages and restores the epoch === */
    printf("Test 2: restore replays a 3-page checkpoint\n");
    {
        fat_block_device_t dev = make_dev();
        snapshot_store_t store;
        snapshot_store_init(&store, &dev, 0, 256);
        snapshot_store_format(&store);
        write_checkpoint(&store, 7, 3);

        checkpoint_init();
        CHECK(checkpoint_current_epoch() == 0, "epoch 0 before restore");
        g_applied_n = 0; g_apply_fail_at = -1;

        int rc = checkpoint_restore(&store, mock_apply, 0);
        CHECK(rc == 3, "restore returns 3 pages");
        CHECK(g_applied_n == 3, "apply called for all 3 pages");
        CHECK(checkpoint_current_epoch() == EPOCH, "global epoch restored to 9");

        int meta_ok = 1, content_ok = 1;
        bool seen[3] = {false, false, false};
        for (int j = 0; j < g_applied_n; j++) {
            int i = (int)((g_applied[j].va - VBASE) / SNAPSHOT_PAGE_SIZE);
            if (i < 0 || i >= 3) { meta_ok = 0; continue; }
            seen[i] = true;
            if (g_applied[j].pid != 7) meta_ok = 0;
            if (g_applied[j].first != (uint8_t)(i * 13 + 0)) content_ok = 0;
            if (g_applied[j].last != (uint8_t)(i * 13 + (SNAPSHOT_PAGE_SIZE - 1))) content_ok = 0;
        }
        CHECK(seen[0] && seen[1] && seen[2], "every page virt_addr replayed once");
        CHECK(meta_ok, "pid correct for every replayed page");
        CHECK(content_ok, "page contents correct for every replayed page");
    }

    /* === Test 3: apply failure aborts the restore === */
    printf("Test 3: apply failure aborts restore\n");
    {
        fat_block_device_t dev = make_dev();
        snapshot_store_t store;
        snapshot_store_init(&store, &dev, 0, 256);
        snapshot_store_format(&store);
        write_checkpoint(&store, 7, 3);

        checkpoint_init();
        g_applied_n = 0; g_apply_fail_at = 1; /* fail on the 2nd page */
        int rc = checkpoint_restore(&store, mock_apply, 0);
        CHECK(rc == CHECKPOINT_ERR_PARAM, "restore propagates apply error");
        CHECK(g_applied_n == 1, "stopped after the first successful apply");
    }

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
