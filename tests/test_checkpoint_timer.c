/* Host-side unit test for the periodic checkpoint trigger (#117).
 *
 * Verifies checkpoint_tick():
 *   1. Disabled: never triggers, regardless of tick count.
 *   2. Enabled with interval N: triggers exactly on the Nth tick, advancing
 *      the epoch and opening it.
 *   3. No overlap: once a checkpoint is open (taken, not yet written back),
 *      later interval boundaries do NOT take another one.
 *   4. Resume: after writeback closes the epoch, the next tick triggers again.
 *
 * Build: gcc -I../include -o test_checkpoint_timer test_checkpoint_timer.c \
 *            ../kernel/checkpoint.c ../kernel/snapshot_store.c \
 *            ../kernel/checkpoint_extstate.c ../kernel/checkpoint_barrier.c
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

void* kmalloc(size_t s) { return malloc(s); }
void  kfree(void* p) { free(p); }

#include "checkpoint.h"

/* ----- Stubs so checkpoint.c links. Empty process list => take() marks no
 * pages but still opens an epoch and returns a non-zero epoch number. ----- */
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

/* ----- Mock block device for the writeback step ----- */
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

static bool tick_n(int n) { /* tick n times, return the last result */
    bool r = false;
    for (int i = 0; i < n; i++) r = checkpoint_tick();
    return r;
}

int main(void) {
    printf("Test: periodic checkpoint trigger\n");

    /* === 1. Disabled never triggers === */
    checkpoint_init();
    checkpoint_timer_configure(false, 3);
    bool any = false;
    for (int i = 0; i < 10; i++) any = any || checkpoint_tick();
    CHECK(!any, "disabled: no checkpoint over 10 ticks");
    CHECK(checkpoint_current_epoch() == 0, "disabled: epoch stays 0");

    /* === 2. Enabled, interval 3: fires on the 3rd tick === */
    checkpoint_init();
    checkpoint_timer_configure(true, 3);
    CHECK(checkpoint_tick() == false, "tick 1: no checkpoint");
    CHECK(checkpoint_tick() == false, "tick 2: no checkpoint");
    CHECK(checkpoint_tick() == true,  "tick 3: checkpoint taken");
    CHECK(checkpoint_current_epoch() == 1, "epoch advanced to 1");
    CHECK(checkpoint_get_state()->epoch_open == true, "epoch is open");
    CHECK(checkpoint_get_state()->total_takes == 1, "one take so far");

    /* === 3. No overlap while the epoch is still open === */
    bool again = tick_n(6); /* well past another interval */
    CHECK(again == false, "no second checkpoint while previous is open");
    CHECK(checkpoint_current_epoch() == 1, "epoch unchanged at 1");
    CHECK(checkpoint_get_state()->total_takes == 1, "still one take");

    /* === 4. After writeback closes the epoch, the trigger resumes === */
    memset(g_disk, 0, sizeof(g_disk));
    fat_block_device_t dev = {0};
    dev.read_sectors = mock_read; dev.write_sectors = mock_write;
    dev.sector_size = SNAPSHOT_SECTOR_SIZE; dev.total_sectors = MOCK_SECTORS;
    snapshot_store_t store;
    snapshot_store_init(&store, &dev, 0, 256);
    snapshot_store_format(&store);
    CHECK(checkpoint_writeback(&store) == CHECKPOINT_OK, "writeback closes the epoch");
    CHECK(checkpoint_get_state()->epoch_open == false, "epoch closed after writeback");

    /* Counter is already past the interval (held during the overlap), so the
     * very next tick takes the next checkpoint. */
    CHECK(checkpoint_tick() == true, "next tick after writeback triggers");
    CHECK(checkpoint_current_epoch() == 2, "epoch advanced to 2");
    CHECK(checkpoint_get_state()->total_takes == 2, "two takes total");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
