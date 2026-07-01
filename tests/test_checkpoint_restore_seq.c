/* Host-side test for the v2 boot restore ordering + blob reassembly (#147).
 *
 * Pure logic, no kernel deps. Verifies the restore sequencer runs the phases in
 * order (kernel state, drivers, user), cold-boots cleanly when there is no
 * checkpoint, and falls back to cold boot at whichever phase fails; plus the
 * kernel-blob reassembly offset math (single/multi chunk, completion, bounds).
 *
 * Build: gcc -I../include -o test_checkpoint_restore_seq \
 *            test_checkpoint_restore_seq.c ../kernel/checkpoint_restore_seq.c
 */

#include <stdint.h>
extern int printf(const char*, ...);

#include "checkpoint_restore_seq.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* Mock steps: record the call order and let each phase be forced to fail. */
static int g_seq[8], g_seq_n;
static int g_has_cp;
static int g_fail_kernel, g_fail_drivers, g_fail_user;

static void seq_reset(void) {
    g_seq_n = 0; g_has_cp = 1;
    g_fail_kernel = g_fail_drivers = g_fail_user = 0;
}

enum { STEP_HAS = 1, STEP_KERNEL = 2, STEP_DRIVERS = 3, STEP_USER = 4 };

static int m_has(void* c)     { (void)c; g_seq[g_seq_n++] = STEP_HAS;     return g_has_cp; }
static int m_kernel(void* c)  { (void)c; g_seq[g_seq_n++] = STEP_KERNEL;  return g_fail_kernel; }
static int m_drivers(void* c) { (void)c; g_seq[g_seq_n++] = STEP_DRIVERS; return g_fail_drivers; }
static int m_user(void* c)    { (void)c; g_seq[g_seq_n++] = STEP_USER;    return g_fail_user; }

static checkpoint_restore_steps_t make_steps(void) {
    checkpoint_restore_steps_t s;
    s.has_checkpoint = m_has; s.restore_kernel_state = m_kernel;
    s.reattach_drivers = m_drivers; s.restore_user = m_user; s.ctx = 0;
    return s;
}

int main(void) {
    checkpoint_restore_steps_t s;
    checkpoint_restore_result_t r;

    printf("Test 1: full restore runs phases in order\n");
    seq_reset(); s = make_steps();
    r = checkpoint_restore_sequence(&s);
    CHECK(r.outcome == CHECKPOINT_RESTORE_RESUMED, "resumed");
    CHECK(r.reached == CHECKPOINT_PHASE_DONE, "reached DONE");
    CHECK(r.fell_back == 0, "no fallback");
    CHECK(g_seq_n == 4 && g_seq[0] == STEP_HAS && g_seq[1] == STEP_KERNEL
          && g_seq[2] == STEP_DRIVERS && g_seq[3] == STEP_USER,
          "order: has, kernel-state, drivers, user");

    printf("Test 2: no checkpoint is a clean cold boot\n");
    seq_reset(); g_has_cp = 0; s = make_steps();
    r = checkpoint_restore_sequence(&s);
    CHECK(r.outcome == CHECKPOINT_RESTORE_COLD_BOOT, "cold boot");
    CHECK(r.fell_back == 0, "not a fallback (nothing to restore)");
    CHECK(g_seq_n == 1, "no restore phase ran");

    printf("Test 3: kernel-state failure falls back before drivers\n");
    seq_reset(); g_fail_kernel = 1; s = make_steps();
    r = checkpoint_restore_sequence(&s);
    CHECK(r.outcome == CHECKPOINT_RESTORE_COLD_BOOT && r.fell_back == 1, "cold-boot fallback");
    CHECK(r.reached == CHECKPOINT_PHASE_KERNEL_STATE, "stopped at kernel-state");
    CHECK(g_seq_n == 2, "drivers and user never ran");

    printf("Test 4: driver re-attach failure falls back before user\n");
    seq_reset(); g_fail_drivers = 1; s = make_steps();
    r = checkpoint_restore_sequence(&s);
    CHECK(r.outcome == CHECKPOINT_RESTORE_COLD_BOOT && r.fell_back == 1, "cold-boot fallback");
    CHECK(r.reached == CHECKPOINT_PHASE_DRIVER_REATTACH, "stopped at driver re-attach");
    CHECK(g_seq_n == 3, "user restore never ran");

    printf("Test 5: user restore failure falls back\n");
    seq_reset(); g_fail_user = 1; s = make_steps();
    r = checkpoint_restore_sequence(&s);
    CHECK(r.outcome == CHECKPOINT_RESTORE_COLD_BOOT && r.fell_back == 1, "cold-boot fallback");
    CHECK(r.reached == CHECKPOINT_PHASE_USER_RESTORE, "stopped at user restore");

    printf("Test 6: missing wiring cold-boots\n");
    r = checkpoint_restore_sequence(0);
    CHECK(r.outcome == CHECKPOINT_RESTORE_COLD_BOOT && r.fell_back == 1, "NULL steps cold-boot");
    s = make_steps(); s.reattach_drivers = 0;
    r = checkpoint_restore_sequence(&s);
    CHECK(r.outcome == CHECKPOINT_RESTORE_COLD_BOOT && r.reached == CHECKPOINT_PHASE_NONE,
          "a NULL step is a fatal misconfiguration");

    /* ----- kernel-blob reassembly ----- */
    printf("Test 7: virt_addr header decode\n");
    {
        uint64_t va = ((uint64_t)5000 << 32) | 2u;
        CHECK(checkpoint_blob_total(va) == 5000, "total decoded");
        CHECK(checkpoint_blob_index(va) == 2, "index decoded");
    }

    printf("Test 8: single-chunk blob completes\n");
    {
        uint8_t chunk[CHECKPOINT_BLOB_CHUNK];
        for (int i = 0; i < CHECKPOINT_BLOB_CHUNK; i++) chunk[i] = (uint8_t)(i * 3 + 1);
        uint8_t buf[100];
        uint32_t recv = 0;
        int rc = checkpoint_blob_place(buf, sizeof(buf), 100, 0, chunk, &recv);
        CHECK(rc == CHECKPOINT_BLOB_COMPLETE, "one chunk completes a small blob");
        CHECK(recv == 100, "all bytes received");
        int ok = 1; for (int i = 0; i < 100; i++) if (buf[i] != chunk[i]) ok = 0;
        CHECK(ok, "bytes placed correctly");
    }

    printf("Test 9: two-chunk blob reassembles in order\n");
    {
        uint32_t total = CHECKPOINT_BLOB_CHUNK + 512; /* spills into a 2nd chunk */
        uint8_t src[CHECKPOINT_BLOB_CHUNK + 512];
        for (uint32_t i = 0; i < total; i++) src[i] = (uint8_t)(i & 0xFF);
        uint8_t c0[CHECKPOINT_BLOB_CHUNK], c1[CHECKPOINT_BLOB_CHUNK];
        for (int i = 0; i < CHECKPOINT_BLOB_CHUNK; i++) c0[i] = src[i];
        for (int i = 0; i < 512; i++) c1[i] = src[CHECKPOINT_BLOB_CHUNK + i];

        uint8_t buf[CHECKPOINT_BLOB_CHUNK + 512];
        uint32_t recv = 0;
        int rc0 = checkpoint_blob_place(buf, sizeof(buf), total, 0, c0, &recv);
        CHECK(rc0 == CHECKPOINT_BLOB_OK, "first chunk in progress");
        CHECK(recv == CHECKPOINT_BLOB_CHUNK, "first chunk full size counted");
        int rc1 = checkpoint_blob_place(buf, sizeof(buf), total, 1, c1, &recv);
        CHECK(rc1 == CHECKPOINT_BLOB_COMPLETE, "second chunk completes");
        CHECK(recv == total, "total received");
        int ok = 1; for (uint32_t i = 0; i < total; i++) if (buf[i] != src[i]) ok = 0;
        CHECK(ok, "full blob reassembled byte-identical");
    }

    printf("Test 10: reassembly bounds\n");
    {
        uint8_t chunk[CHECKPOINT_BLOB_CHUNK] = {0};
        uint8_t buf[100];
        uint32_t recv = 0;
        CHECK(checkpoint_blob_place(buf, sizeof(buf), 200, 0, chunk, &recv) == CHECKPOINT_BLOB_ERR,
              "blob larger than the buffer rejected");
        recv = 0;
        CHECK(checkpoint_blob_place(buf, sizeof(buf), 100, 1, chunk, &recv) == CHECKPOINT_BLOB_ERR,
              "chunk index past the end rejected");
        recv = 0;
        CHECK(checkpoint_blob_place(0, sizeof(buf), 100, 0, chunk, &recv) == CHECKPOINT_BLOB_ERR,
              "NULL buffer rejected");
    }

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
