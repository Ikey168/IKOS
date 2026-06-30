/* Host-side test for boot-store wiring (#129).
 *
 * Exercises checkpoint_persistence_init() against a real ramdisk-backed store:
 *   1. It binds + formats the store and arms the periodic trigger.
 *   2. With a fresh (empty) store, checkpoint_boot() reports no checkpoint
 *      (the kernel cold-boots).
 *   3. The trigger is armed: ticks take an automatic checkpoint at the interval.
 *   4. After that checkpoint is written back, checkpoint_boot() restores from
 *      the registered store (>= 0), proving the store is wired as the boot store.
 *
 * Build: gcc -I../include -o test_persistence_init test_persistence_init.c \
 *            ../kernel/checkpoint.c ../kernel/snapshot_store.c \
 *            ../kernel/checkpoint_extstate.c ../kernel/ramdisk.c
 */

#include <stdint.h>
#include <stdbool.h>

typedef __SIZE_TYPE__ size_t;
extern int   printf(const char*, ...);
extern void* malloc(size_t);
extern void  free(void*);
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);

void* kmalloc(size_t s) { return malloc(s); }
void  kfree(void* p) { free(p); }

#include "checkpoint.h"
#include "ramdisk.h"

/* Engine link stubs. */
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
struct process* process_get_by_pid(int pid) { (void)pid; return 0; }
struct process* process_create(const char* a, const char* b) { (void)a; (void)b; return 0; }
int pm_table_add_process(struct process* p) { (void)p; return 0; }
int scheduler_add_process(struct process* p) { (void)p; return 0; }

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

int main(void) {
    printf("Test: boot-store wiring (checkpoint_persistence_init)\n");

    fat_block_device_t* dev = ramdisk_get_device();
    CHECK(dev != 0, "ramdisk device available");
    CHECK(dev->sector_size == SNAPSHOT_SECTOR_SIZE, "ramdisk sector size matches store");

    checkpoint_init();

    snapshot_store_t store;
    int rc = checkpoint_persistence_init(&store, dev,
                                         CHECKPOINT_STORE_BASE_SECTOR,
                                         CHECKPOINT_STORE_SLOT_SECTORS,
                                         /* interval */ 4);
    CHECK(rc == CHECKPOINT_OK, "persistence_init arms the store");

    /* NULL device => persistence stays disabled, does not disturb the armed store. */
    snapshot_store_t junk;
    CHECK(checkpoint_persistence_init(&junk, 0, 0, 256, 4) == CHECKPOINT_ERR_PARAM,
          "NULL device rejected");

    /* Fresh store: nothing to restore yet -> cold boot. */
    CHECK(checkpoint_boot() == CHECKPOINT_ERR_NO_CHECKPOINT, "empty store -> cold boot");

    /* Trigger armed at interval 4: ticks 1-3 do nothing, tick 4 checkpoints. */
    CHECK(checkpoint_tick() == false, "tick 1");
    CHECK(checkpoint_tick() == false, "tick 2");
    CHECK(checkpoint_tick() == false, "tick 3");
    CHECK(checkpoint_tick() == true,  "tick 4: automatic checkpoint taken");

    /* Commit it, then boot must restore from the registered store. */
    CHECK(checkpoint_writeback(&store) == CHECKPOINT_OK, "writeback commits to the ramdisk store");
    CHECK(checkpoint_boot() >= 0, "boot restores from the registered store");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
