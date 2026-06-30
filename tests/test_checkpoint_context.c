/* Host-side unit test for process-context persistence (#126).
 *
 * Verifies that a process CPU context round-trips through the real checkpoint
 * path: capture -> writeback -> commit -> restore, and that restore correctly
 * distinguishes a context record (CHECKPOINT_REC_CONTEXT) from an ordinary
 * address-space page that is checkpointed alongside it.
 *
 * Build: gcc -I../include -o test_checkpoint_context test_checkpoint_context.c \
 *            ../kernel/checkpoint.c ../kernel/snapshot_store.c \
 *            ../kernel/checkpoint_extstate.c ../kernel/checkpoint_barrier.c
 */

#include <stdint.h>
#include <stdbool.h>

typedef __SIZE_TYPE__ size_t;
extern int   printf(const char*, ...);
extern void* malloc(size_t);
extern void  free(void*);
extern void* aligned_alloc(size_t, size_t);
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
extern int   memcmp(const void*, const void*, size_t);

void* kmalloc(size_t s) { return malloc(s); }
void  kfree(void* p) { free(p); }

#include "checkpoint.h"

/* Engine link stubs (empty process list: we drive capture explicitly). */
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

/* Mirror of process_context_t's layout/size (registers + control + segments). */
typedef struct {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, rflags, cr3;
    uint16_t cs, ds, es, fs, gs, ss;
} ctx_t;

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* Restore apply: separate context records from page records. */
static ctx_t g_recovered;
static int   g_got_ctx;
static int   g_pages;
static int recover(void* unused, const snapshot_page_record_t* rec) {
    (void)unused;
    if (rec->flags & CHECKPOINT_REC_CONTEXT) {
        memcpy(&g_recovered, rec->page_data, sizeof(g_recovered));
        g_got_ctx++;
    } else {
        g_pages++;
    }
    return CHECKPOINT_OK;
}

int main(void) {
    printf("Test: process context persistence\n");

    memset(g_disk, 0, sizeof(g_disk));
    fat_block_device_t dev = {0};
    dev.read_sectors = mock_read; dev.write_sectors = mock_write;
    dev.sector_size = SNAPSHOT_SECTOR_SIZE; dev.total_sectors = MOCK_SECTORS;
    snapshot_store_t store;
    snapshot_store_init(&store, &dev, 0, 256);
    snapshot_store_format(&store);

    checkpoint_init();

    /* A recognizable register set. */
    ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.rax = 0x1111111111111111ULL;
    ctx.rip = 0x0000000000401000ULL;
    ctx.rsp = 0x00007fffffffe000ULL;
    ctx.cr3 = 0x0000000000123000ULL;
    ctx.cs = 0x23; ctx.ss = 0x1b; ctx.rflags = 0x202;

    /* Open an epoch, then capture: one context + one ordinary page. */
    checkpoint_take(); /* pm empty -> marks nothing, just advances/opens the epoch */

    CHECK(checkpoint_capture_context(7, &ctx, sizeof(ctx)) == CHECKPOINT_OK, "capture context");
    CHECK(checkpoint_capture_context(7, 0, sizeof(ctx)) == CHECKPOINT_ERR_PARAM, "NULL context rejected");
    CHECK(checkpoint_capture_context(7, &ctx, SNAPSHOT_PAGE_SIZE + 1) == CHECKPOINT_ERR_PARAM,
          "oversized context rejected");

    uint8_t* page = (uint8_t*)aligned_alloc(SNAPSHOT_PAGE_SIZE, SNAPSHOT_PAGE_SIZE);
    for (int i = 0; i < SNAPSHOT_PAGE_SIZE; i++) page[i] = (uint8_t)(i ^ 0x5A);
    pte_t pte = 0x9000ULL | PAGE_PRESENT | PAGE_SNAPSHOT_COW;
    CHECK(checkpoint_capture_page(7, 0x400000, page, &pte) == CHECKPOINT_OK, "capture page");

    CHECK(checkpoint_capture_count() == 2, "two records captured (context + page)");

    CHECK(checkpoint_writeback(&store) == CHECKPOINT_OK, "writeback commits");

    /* Restore and separate the records. */
    g_got_ctx = 0; g_pages = 0;
    memset(&g_recovered, 0, sizeof(g_recovered));
    int n = checkpoint_restore(&store, recover, 0);
    CHECK(n == 2, "restored two records");
    CHECK(g_got_ctx == 1, "exactly one context record seen");
    CHECK(g_pages == 1, "exactly one page record seen");

    CHECK(memcmp(&g_recovered, &ctx, sizeof(ctx)) == 0, "context bytes round-trip exactly");
    CHECK(g_recovered.rip == 0x401000ULL, "RIP preserved");
    CHECK(g_recovered.rsp == 0x00007fffffffe000ULL, "RSP preserved");
    CHECK(g_recovered.rax == 0x1111111111111111ULL, "RAX preserved");
    CHECK(g_recovered.cs == 0x23 && g_recovered.ss == 0x1b, "segment registers preserved");

    free(page);
    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
