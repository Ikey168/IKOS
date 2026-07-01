/* Host-side unit test for the replay engine (#165).
 *
 * Verifies:
 *   1. Given a keyframe and target, replay restores the keyframe and re-drives
 *      the right epochs (whole epochs up to the target, then a partial epoch).
 *   2. The reconstructed state is usable: the restore hook rebuilds a simulated
 *      process table / scheduler state that the re-driven epochs then advance.
 *   3. Replaying the same target twice is deterministic (identical trace and
 *      identical final state).
 *   4. keyframe == target with offset 0 restores only; bad params and hook
 *      failures are reported.
 *
 * Build: gcc -I../include -o test_replay_engine \
 *            test_replay_engine.c ../kernel/replay_engine.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "replay_engine.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* A simulated reconstructed system: a process count and a running state hash
 * the re-driven epochs deterministically fold forward. Plus a call trace so we
 * can assert the exact sequence of restore/load/run steps. */
typedef struct {
    /* reconstructed "state" */
    uint32_t proc_count;
    uint64_t state_hash;
    /* trace */
    char     trace[256];
    uint32_t tlen;
    /* fault injection */
    int      fail_restore;
    int      fail_load_at;   /* -1 = never; else epoch to fail load */
} sim_t;

static void trace_put(sim_t* s, char kind, uint64_t a, uint64_t b) {
    /* compact, deterministic encoding of one step */
    if (s->tlen + 4 <= sizeof(s->trace)) {
        s->trace[s->tlen++] = kind;
        s->trace[s->tlen++] = (char)('0' + (a % 10));
        s->trace[s->tlen++] = (char)('0' + (b % 10));
        s->trace[s->tlen++] = '|';
    }
}

static int sim_restore(void* ctx, uint64_t epoch) {
    sim_t* s = (sim_t*)ctx;
    if (s->fail_restore) return -1;
    /* Rebuild a usable state from the keyframe. */
    s->proc_count = 3 + (uint32_t)epoch;
    s->state_hash = 0x100 + epoch;
    trace_put(s, 'R', epoch, 0);
    return 0;
}

static int sim_load(void* ctx, uint64_t epoch) {
    sim_t* s = (sim_t*)ctx;
    if (s->fail_load_at >= 0 && (uint64_t)s->fail_load_at == epoch) return -1;
    trace_put(s, 'L', epoch, 0);
    return 0;
}

static int sim_run(void* ctx, uint64_t epoch, uint64_t limit) {
    sim_t* s = (sim_t*)ctx;
    /* Fold the epoch (and how far into it) into the state deterministically. */
    uint64_t steps = (limit == REPLAY_WHOLE_EPOCH) ? 10 : limit;
    for (uint64_t i = 0; i < steps; i++)
        s->state_hash = s->state_hash * 31 + epoch * 7 + i;
    trace_put(s, 'X', epoch, steps);
    return 0;
}

static void sim_reset(sim_t* s) {
    for (unsigned i = 0; i < sizeof(*s); i++) ((char*)s)[i] = 0;
    s->fail_load_at = -1;
}

static replay_hooks_t hooks_for(sim_t* s) {
    replay_hooks_t h;
    h.restore_keyframe = sim_restore;
    h.load_epoch = sim_load;
    h.run_epoch = sim_run;
    h.ctx = s;
    return h;
}

static int streq(const char* a, const char* b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

int main(void) {
    printf("=== Replay engine (#165) unit test ===\n");

    /* --- 1. Reaches the target: whole epochs then a partial epoch --- */
    {
        sim_t s; sim_reset(&s);
        replay_hooks_t h = hooks_for(&s);
        replay_engine_t re;
        CHECK(replay_init(&re, &h) == REPLAY_OK, "init");
        /* keyframe 5, target 8, offset 4: restore 5; run 5,6,7 whole; run 8 x4 */
        CHECK(replay_run(&re, 5, 8, 4) == REPLAY_OK, "replay 5 -> (8, +4) succeeds");
        CHECK(re.done && re.current_epoch == 8, "engine reports done at target epoch");
        CHECK(re.epochs_run == 4, "three whole epochs plus one partial re-driven");
        /* Expected trace: R5 L5 X5 L6 X6 L7 X7 L8 X8, where whole-epoch runs
         * encode steps 10 as '0' (10 % 10) and the final run encodes 4. */
        const char* want = "R50|L50|X50|L60|X60|L70|X70|L80|X84|";
        CHECK(streq(s.trace, want, s.tlen) && s.tlen == 36,
              "restore/load/run trace is exactly the expected sequence");
    }

    /* --- 2. Reconstructed state is usable (restore populated it) --- */
    {
        sim_t s; sim_reset(&s);
        replay_hooks_t h = hooks_for(&s);
        replay_engine_t re; replay_init(&re, &h);
        replay_run(&re, 2, 4, 0);
        CHECK(s.proc_count == 3 + 2, "restore rebuilt a usable process table (from keyframe 2)");
        CHECK(s.state_hash != 0, "re-driven epochs advanced the reconstructed state");
    }

    /* --- 3. Deterministic across repeated replays of the same target --- */
    {
        sim_t a; sim_reset(&a);
        sim_t b; sim_reset(&b);
        replay_hooks_t ha = hooks_for(&a);
        replay_hooks_t hb = hooks_for(&b);
        replay_engine_t rea, reb;
        replay_init(&rea, &ha); replay_init(&reb, &hb);
        replay_run(&rea, 1, 6, 3);
        replay_run(&reb, 1, 6, 3);
        CHECK(a.state_hash == b.state_hash, "same target yields identical final state");
        CHECK(a.tlen == b.tlen && streq(a.trace, b.trace, a.tlen),
              "same target yields identical step trace");
    }

    /* --- 4. Edge cases and error propagation --- */
    {
        sim_t s; sim_reset(&s);
        replay_hooks_t h = hooks_for(&s);
        replay_engine_t re; replay_init(&re, &h);
        CHECK(replay_run(&re, 4, 4, 0) == REPLAY_OK && s.trace[0] == 'R' && s.tlen == 4,
              "keyframe == target, offset 0 restores only (no load/run)");

        sim_reset(&s);
        CHECK(replay_run(&re, 9, 8, 0) == REPLAY_ERR_PARAM,
              "keyframe after target is rejected");

        sim_reset(&s); s.fail_restore = 1;
        CHECK(replay_run(&re, 5, 7, 0) == REPLAY_ERR_RESTORE, "restore failure reported");

        sim_reset(&s); s.fail_load_at = 6;
        CHECK(replay_run(&re, 5, 8, 0) == REPLAY_ERR_LOAD, "load failure reported");

        replay_engine_t bad;
        replay_hooks_t empty; empty.restore_keyframe = 0; empty.load_epoch = 0;
        empty.run_epoch = 0; empty.ctx = 0;
        CHECK(replay_init(&bad, &empty) == REPLAY_ERR_PARAM, "init rejects missing hooks");
    }

    if (failures == 0) {
        printf("PASSED: replay reconstructs a target from keyframe + journal deterministically\n");
        return 0;
    }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
