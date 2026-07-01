/* Host-side unit test for the divergence component scan (#197).
 *
 * Drives the record -> journal -> replay round trip with mock component sources
 * and the real pure cores (divergence_scan + divergence), verifying:
 *   1. A record scan checksums every component and yields (id, sum) pairs.
 *   2. Those pairs round-trip through the JOURNAL_EV_DIVERGE encoding (component
 *      id in lclock, checksum in value) and install as the expected sums.
 *   3. A replay scan of the SAME state matches every component (no divergence).
 *   4. A replay scan of CHANGED state is caught, with the detector reporting the
 *      exact epoch and diverging component.
 *
 * Build: gcc -I../include -o test_divergence_scan \
 *            test_divergence_scan.c ../kernel/divergence_scan.c \
 *            ../kernel/divergence.c
 */

#include <stdint.h>
#include <stdbool.h>

#include "divergence_scan.h"
#include "divergence.h"

extern int printf(const char*, ...);

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* Component ids (mirror KDIVERGE_*). */
enum { C_PROC = 0, C_SCHED = 1, C_PAGES = 2, NCOMP = 3 };

/* Mock component state: each source checksums a byte the test can mutate. */
static uint8_t g_state[NCOMP];

static uint32_t src_proc(void* ctx)  { (void)ctx; return divergence_checksum(0, &g_state[C_PROC],  1); }
static uint32_t src_sched(void* ctx) { (void)ctx; return divergence_checksum(0, &g_state[C_SCHED], 1); }
static uint32_t src_pages(void* ctx) { (void)ctx; return divergence_checksum(0, &g_state[C_PAGES], 1); }

/* A local detector wired through free-function sinks (mirrors kdiverge_*). */
static divergence_t g_det;
static int  det_record(uint32_t comp, uint32_t sum) { return divergence_record(&g_det, comp, sum); }
static bool det_check(uint32_t comp, uint32_t sum)  { return divergence_check(&g_det, comp, sum); }

/* Dense-expect from (id, sum) pairs, as the kernel adapter does. */
static void expect_pairs(uint64_t epoch, const uint32_t* ids,
                         const uint32_t* sums, uint32_t n) {
    uint32_t dense[DIVERGE_MAX_COMPONENTS];
    for (uint32_t i = 0; i < DIVERGE_MAX_COMPONENTS; i++) dense[i] = 0;
    uint32_t span = 0;
    for (uint32_t i = 0; i < n; i++) {
        dense[ids[i]] = sums[i];
        if (ids[i] + 1 > span) span = ids[i] + 1;
    }
    divergence_expect(&g_det, epoch, dense, span);
}

int main(void) {
    printf("test_divergence_scan\n");

    diverge_source_t srcs[NCOMP] = {
        { C_PROC,  src_proc,  0 },
        { C_SCHED, src_sched, 0 },
        { C_PAGES, src_pages, 0 },
    };

    /* Some starting state. */
    g_state[C_PROC] = 0x11; g_state[C_SCHED] = 0x22; g_state[C_PAGES] = 0x33;

    /* --- 1: record scan --- */
    divergence_init(&g_det, DIVERGE_RECORD);
    divergence_begin_epoch(&g_det, 42);
    uint32_t ids[NCOMP], sums[NCOMP];
    uint32_t n = diverge_scan_record(srcs, NCOMP, det_record, ids, sums);
    CHECK(n == 3, "record scan covered all 3 components");
    CHECK(ids[0] == C_PROC && ids[1] == C_SCHED && ids[2] == C_PAGES,
          "component ids in registration order");

    /* --- 2: encode as JOURNAL_EV_DIVERGE (id in lclock, sum in value) and
     * decode back, as the journal + replay path do --- */
    uint64_t ev_lclock[NCOMP], ev_value[NCOMP];
    for (uint32_t i = 0; i < n; i++) { ev_lclock[i] = ids[i]; ev_value[i] = sums[i]; }
    uint32_t dids[NCOMP], dsums[NCOMP];
    for (uint32_t i = 0; i < n; i++) { dids[i] = (uint32_t)ev_lclock[i]; dsums[i] = (uint32_t)ev_value[i]; }

    /* --- 3: replay scan of identical state matches --- */
    divergence_init(&g_det, DIVERGE_REPLAY);
    expect_pairs(42, dids, dsums, n);
    bool ok = diverge_scan_check(srcs, NCOMP, det_check);
    CHECK(ok, "replay of identical state: all components match");
    CHECK(!divergence_has_diverged(&g_det), "no divergence recorded");

    /* --- 4: mutate one component's state; replay catches it --- */
    divergence_init(&g_det, DIVERGE_REPLAY);
    expect_pairs(42, dids, dsums, n);
    g_state[C_SCHED] = 0xEE; /* the scheduler component now differs */
    ok = diverge_scan_check(srcs, NCOMP, det_check);
    CHECK(!ok, "replay of changed state: scan reports a mismatch");
    CHECK(divergence_has_diverged(&g_det), "divergence recorded");
    CHECK(g_det.diverged_epoch == 42, "divergence reported at the right epoch");
    CHECK(g_det.diverged_component == C_SCHED, "divergence pinned to the scheduler component");

    /* The unchanged components still match (only C_SCHED diverged). */
    g_state[C_SCHED] = 0x22; /* restore */
    divergence_init(&g_det, DIVERGE_REPLAY);
    expect_pairs(42, dids, dsums, n);
    CHECK(diverge_scan_check(srcs, NCOMP, det_check),
          "restoring the component clears the divergence");

    printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
