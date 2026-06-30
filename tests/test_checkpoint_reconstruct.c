/* Host-side unit test for process reconstruction on restore (#127).
 *
 * checkpoint_finalize_restore() is the generic step that hands each
 * reconstructed process to a registration callback. This test verifies it
 * invokes the callback once per reconstructed process, in order, carrying the
 * right pid, address space, and saved CPU context - the data the kernel
 * registrar uses to rebuild the process table and re-add processes to the
 * scheduler.
 *
 * Build: gcc -I../include -o test_checkpoint_reconstruct \
 *            test_checkpoint_reconstruct.c ../kernel/checkpoint.c \
 *            ../kernel/snapshot_store.c
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

/* Record what the registrar is asked to register. */
#define MAXREG 8
static struct {
    uint32_t pid;
    vm_space_t* space;
    bool has_context;
    uint8_t ctx0;     /* first context byte, to confirm the right blob */
} g_reg[MAXREG];
static int g_reg_n;
static int g_fail_at = -1;

static int mock_register(void* reg_ctx, const checkpoint_restored_process_t* rp) {
    (void)reg_ctx;
    if (g_fail_at >= 0 && g_reg_n >= g_fail_at) return CHECKPOINT_ERR_PARAM;
    g_reg[g_reg_n].pid = rp->pid;
    g_reg[g_reg_n].space = rp->space;
    g_reg[g_reg_n].has_context = rp->has_context;
    g_reg[g_reg_n].ctx0 = rp->has_context ? rp->context[0] : 0;
    g_reg_n++;
    return CHECKPOINT_OK;
}

int main(void) {
    printf("Test: process reconstruction finalize\n");

    /* Two reconstructed processes: pid 7 has a space + context, pid 9 has a
     * space but no context. */
    vm_space_t fake_space_a, fake_space_b;
    checkpoint_restored_process_t procs[2];
    memset(procs, 0, sizeof(procs));

    procs[0].pid = 7;
    procs[0].space = &fake_space_a;
    procs[0].has_context = true;
    procs[0].context_size = 16;
    procs[0].context[0] = 0xAB;

    procs[1].pid = 9;
    procs[1].space = &fake_space_b;
    procs[1].has_context = false;

    /* === each process registered once, in order, with the right fields === */
    g_reg_n = 0; g_fail_at = -1;
    int n = checkpoint_finalize_restore(procs, 2, mock_register, 0);
    CHECK(n == 2, "two processes registered");
    CHECK(g_reg_n == 2, "registrar called twice");
    CHECK(g_reg[0].pid == 7 && g_reg[1].pid == 9, "pids passed in order");
    CHECK(g_reg[0].space == &fake_space_a, "pid 7 gets its address space");
    CHECK(g_reg[1].space == &fake_space_b, "pid 9 gets its address space");
    CHECK(g_reg[0].has_context && g_reg[0].ctx0 == 0xAB, "pid 7 carries its context");
    CHECK(!g_reg[1].has_context, "pid 9 has no context");

    /* === a registrar failure aborts and propagates === */
    g_reg_n = 0; g_fail_at = 1; /* fail on the 2nd */
    int rc = checkpoint_finalize_restore(procs, 2, mock_register, 0);
    CHECK(rc == CHECKPOINT_ERR_PARAM, "registrar error propagates");
    CHECK(g_reg_n == 1, "stopped after the first registration");

    /* === guards === */
    CHECK(checkpoint_finalize_restore(0, 2, mock_register, 0) == CHECKPOINT_ERR_PARAM,
          "NULL procs rejected");
    CHECK(checkpoint_finalize_restore(procs, 0, mock_register, 0) == 0,
          "zero count registers nothing");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
