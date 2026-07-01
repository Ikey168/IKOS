/* Host-side unit test for deterministic preemption (#162).
 *
 * Verifies:
 *   1. OFF mode is exact passthrough and records nothing (unchanged behavior).
 *   2. RECORD captures the logical clock at each context switch.
 *   3. REPLAY reproduces switches at the identical logical points, ignoring the
 *      natural time-slice decision (so real timing cannot perturb the schedule).
 *   4. begin_epoch resets per-epoch state; load installs replay points.
 *   5. Overflow past the buffer capacity is counted, not written out of bounds.
 *
 * Build: gcc -I../include -o test_sched_record \
 *            test_sched_record.c ../kernel/sched_record.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "sched_record.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* A scripted "natural" round-robin decision: preempt when the simulated time
 * slice expires. The gap between expiries jitters to mimic the nondeterministic
 * spacing of real ticks. Returns want for the given tick index. */
static bool natural_want(uint32_t tick, uint32_t* slice_left, const uint32_t* slices,
                         uint32_t nslices, uint32_t* slice_idx) {
    (void)tick;
    if (*slice_left == 0) {
        /* pick the next slice length and start counting down */
        *slice_left = slices[(*slice_idx) % nslices];
        (*slice_idx)++;
    }
    (*slice_left)--;
    return (*slice_left == 0);
}

int main(void) {
    printf("=== Deterministic preemption (#162) unit test ===\n");

    /* --- 1. OFF passthrough --- */
    {
        uint64_t buf[64];
        sched_record_t r;
        sched_record_init(&r, SCHED_REC_OFF, buf, 64);
        bool a = sched_record_decide(&r, true, 10);
        bool b = sched_record_decide(&r, false, 11);
        CHECK(a == true && b == false, "OFF returns the natural decision unchanged");
        const uint64_t* pts;
        CHECK(sched_record_points(&r, &pts) == 0, "OFF records nothing");
    }

    /* --- 2 & 3. RECORD then REPLAY reproduce identical switch points --- */
    uint64_t rec_buf[64];
    sched_record_t rec;
    uint32_t recorded_switch_lclocks[64];
    uint32_t nrecorded = 0;
    {
        sched_record_init(&rec, SCHED_REC_RECORD, rec_buf, 64);
        sched_record_begin_epoch(&rec, 1);

        const uint32_t slices[] = {5, 3, 7, 4, 6};
        uint32_t slice_left = 0, slice_idx = 0;
        uint64_t lclock = 0;
        for (uint32_t tick = 0; tick < 50; tick++) {
            lclock++; /* one logical unit per tick */
            bool want = natural_want(tick, &slice_left, slices, 5, &slice_idx);
            bool sw = sched_record_decide(&rec, want, lclock);
            if (sw) recorded_switch_lclocks[nrecorded++] = (uint32_t)lclock;
        }
        const uint64_t* pts;
        uint32_t n = sched_record_points(&rec, &pts);
        CHECK(n == nrecorded && n > 0, "RECORD captured one point per switch");
        int match = 1;
        for (uint32_t i = 0; i < n; i++)
            if (pts[i] != recorded_switch_lclocks[i]) match = 0;
        CHECK(match, "recorded points equal the lclocks where switches occurred");
    }

    {
        /* Replay: feed want=false on every tick (natural decision suppressed),
         * and confirm switches land at exactly the recorded lclocks. */
        const uint64_t* rec_pts;
        uint32_t n = sched_record_points(&rec, &rec_pts);

        uint64_t rep_buf[64];
        sched_record_t rep;
        sched_record_init(&rep, SCHED_REC_REPLAY, rep_buf, 64);
        CHECK(sched_record_load(&rep, 1, rec_pts, n) == SCHED_REC_OK, "load replay points");

        uint32_t replay_switch_lclocks[64];
        uint32_t nreplay = 0;
        for (uint64_t lclock = 1; lclock <= 50; lclock++) {
            bool sw = sched_record_decide(&rep, /*want=*/false, lclock);
            if (sw) replay_switch_lclocks[nreplay++] = (uint32_t)lclock;
        }
        CHECK(nreplay == nrecorded, "replay produced the same number of switches");
        int same = (nreplay == nrecorded);
        for (uint32_t i = 0; i < nreplay && same; i++)
            if (replay_switch_lclocks[i] != recorded_switch_lclocks[i]) same = 0;
        CHECK(same, "replay switched at the identical logical points, ignoring `want`");
    }

    /* --- 4. begin_epoch resets state --- */
    {
        uint64_t buf[8];
        sched_record_t r;
        sched_record_init(&r, SCHED_REC_RECORD, buf, 8);
        sched_record_begin_epoch(&r, 1);
        sched_record_decide(&r, true, 3);
        sched_record_decide(&r, true, 9);
        const uint64_t* pts;
        CHECK(sched_record_points(&r, &pts) == 2, "two points before new epoch");
        sched_record_begin_epoch(&r, 2);
        CHECK(sched_record_points(&r, &pts) == 0, "begin_epoch cleared the points");
        CHECK(r.epoch == 2, "epoch advanced");
    }

    /* --- 5. Overflow is counted, not written past the buffer --- */
    {
        uint64_t buf[4];
        sched_record_t r;
        sched_record_init(&r, SCHED_REC_RECORD, buf, 4);
        sched_record_begin_epoch(&r, 1);
        for (uint64_t i = 1; i <= 10; i++) sched_record_decide(&r, true, i);
        const uint64_t* pts;
        CHECK(sched_record_points(&r, &pts) == 4, "count clamped to capacity");
        CHECK(r.overflow == 6, "overflow counted the dropped switches");
    }

    if (failures == 0) {
        printf("PASSED: preemption points record and replay deterministically\n");
        return 0;
    }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
