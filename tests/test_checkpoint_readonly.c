/* Host-side test for read-only / code-page persistence (#131).
 *
 * Verifies:
 *   1. checkpoint_page_action() classifies pages correctly: clean writable ->
 *      persist writable, modified writable -> skip (captured elsewhere),
 *      read-only region -> persist read-only, absent -> skip.
 *   2. On restore, a read-only record (CHECKPOINT_REC_READONLY) is re-mapped
 *      WITHOUT write permission, while an ordinary page is mapped writable, and
 *      both pages' contents land in their frames.
 *
 * Build: gcc -I../include -o test_checkpoint_readonly test_checkpoint_readonly.c \
 *            ../kernel/checkpoint.c ../kernel/snapshot_store.c \
 *            ../kernel/checkpoint_extstate.c
 */

#include <stdint.h>
#include <stdbool.h>

typedef __SIZE_TYPE__ size_t;
typedef __UINTPTR_TYPE__ uintptr_t;
extern int   printf(const char*, ...);
extern void* malloc(size_t);
extern void  free(void*);
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
extern int   memcmp(const void*, const void*, size_t);

void* kmalloc(size_t s) { return malloc(s); }
void  kfree(void* p) { free(p); }

#include "checkpoint.h"

/* ---- Recording mocks for the restore path ---- */
static int g_fake_space; /* a non-null vm_space_t stand-in */
pte_t* vmm_get_page_table(vm_space_t* s, uint64_t a, int l, bool c) {
    (void)s; (void)a; (void)l; (void)c; return 0;
}
vm_space_t* vmm_get_current_space(void) { return 0; }
void vmm_flush_tlb_page(uint64_t a) { (void)a; }
uint64_t vmm_get_physical_addr(vm_space_t* s, uint64_t a) { (void)s; (void)a; return 0; }
vm_space_t* vmm_create_address_space(uint32_t pid) { (void)pid; return (vm_space_t*)&g_fake_space; }
uint64_t vmm_alloc_page(void) { return (uint64_t)(uintptr_t)malloc(PAGE_SIZE); }

#define MAXMAP 8
static struct { uint64_t virt; uint64_t phys; uint32_t flags; } g_maps[MAXMAP];
static int g_map_n;
int vmm_map_page(vm_space_t* s, uint64_t v, uint64_t p, uint32_t f) {
    (void)s;
    if (g_map_n < MAXMAP) { g_maps[g_map_n].virt = v; g_maps[g_map_n].phys = p; g_maps[g_map_n].flags = f; g_map_n++; }
    return 0;
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

/* ---- Mock block device ---- */
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

#define RW_VADDR 0x400000ULL
#define RO_VADDR 0x500000ULL

int main(void) {
    /* === 1. Decision matrix === */
    printf("Test 1: checkpoint_page_action\n");
    pte_t present_cow = PAGE_PRESENT | PAGE_SNAPSHOT_COW;
    pte_t present_plain = PAGE_PRESENT;            /* writable region, tag cleared */
    pte_t absent = 0;
    CHECK(checkpoint_page_action(true,  present_cow)   == CHECKPOINT_PAGE_PERSIST_RW, "clean writable -> persist RW");
    CHECK(checkpoint_page_action(true,  present_plain) == CHECKPOINT_PAGE_SKIP,       "modified writable -> skip");
    CHECK(checkpoint_page_action(false, present_plain) == CHECKPOINT_PAGE_PERSIST_RO, "read-only region -> persist RO");
    CHECK(checkpoint_page_action(false, present_cow)   == CHECKPOINT_PAGE_PERSIST_RO, "read-only region (any pte) -> RO");
    CHECK(checkpoint_page_action(true,  absent)        == CHECKPOINT_PAGE_SKIP,       "absent (writable) -> skip");
    CHECK(checkpoint_page_action(false, absent)        == CHECKPOINT_PAGE_SKIP,       "absent (read-only) -> skip");

    /* === 2. Restore maps RO records read-only, RW records writable === */
    printf("Test 2: restore honors the read-only flag\n");
    memset(g_disk, 0, sizeof(g_disk));
    fat_block_device_t dev = {0};
    dev.read_sectors = mock_read; dev.write_sectors = mock_write;
    dev.sector_size = SNAPSHOT_SECTOR_SIZE; dev.total_sectors = MOCK_SECTORS;
    snapshot_store_t store;
    snapshot_store_init(&store, &dev, 0, 256);
    snapshot_store_format(&store);

    /* Lay down one writable page and one read-only (code) page for pid 7. */
    uint8_t rwp[SNAPSHOT_PAGE_SIZE], rop[SNAPSHOT_PAGE_SIZE];
    for (int i = 0; i < SNAPSHOT_PAGE_SIZE; i++) { rwp[i] = (uint8_t)(i + 1); rop[i] = (uint8_t)(0xC0 ^ i); }
    snapshot_writer_t w;
    snapshot_store_begin(&store, 3, &w);
    snapshot_writer_add_page(&w, 7, RW_VADDR, 0, rwp);                      /* writable */
    snapshot_writer_add_page(&w, 7, RO_VADDR, CHECKPOINT_REC_READONLY, rop); /* read-only */
    snapshot_store_commit(&w);

    checkpoint_init();
    g_map_n = 0;
    int restored = checkpoint_restore_boot(&store);
    CHECK(restored == 2, "restored two pages");
    CHECK(g_map_n == 2, "two pages mapped");

    /* Locate each mapping by its virtual address. */
    int irw = -1, iro = -1;
    for (int i = 0; i < g_map_n; i++) {
        if (g_maps[i].virt == RW_VADDR) irw = i;
        if (g_maps[i].virt == RO_VADDR) iro = i;
    }
    CHECK(irw >= 0 && iro >= 0, "both virtual addresses mapped");
    CHECK(irw >= 0 && (g_maps[irw].flags & PAGE_WRITABLE) != 0, "writable page mapped PAGE_WRITABLE");
    CHECK(iro >= 0 && (g_maps[iro].flags & PAGE_WRITABLE) == 0, "read-only page mapped without PAGE_WRITABLE");
    CHECK(iro >= 0 && (g_maps[iro].flags & PAGE_PRESENT) != 0 && (g_maps[iro].flags & PAGE_USER) != 0,
          "read-only page still present + user");

    /* Contents landed in the allocated frames. */
    if (irw >= 0) CHECK(memcmp((void*)(uintptr_t)g_maps[irw].phys, rwp, SNAPSHOT_PAGE_SIZE) == 0, "writable page contents restored");
    if (iro >= 0) CHECK(memcmp((void*)(uintptr_t)g_maps[iro].phys, rop, SNAPSHOT_PAGE_SIZE) == 0, "read-only page contents restored");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
