/* Host-side unit test for reverse execution (#170).
 *
 * Drives a REAL rewind verb (#169) over a REAL keyframe ring (#168) and replay
 * engine (#165), with mock engine hooks that record which keyframe was restored.
 * Verifies:
 *   1. reverse-step lands one step before the current point, within an epoch.
 *   2. At a keyframe boundary, reverse-step crosses to the previous keyframe's
 *      last step (and the rewind restores that keyframe).
 *   3. Repeated reverse-steps are stable: the position sequence is exact and
 *      each step restores the correct keyframe (no drift).
 *   4. reverse-continue stops at an injected condition; with no stop it runs
 *      back to the oldest retained moment and reports REVERSE_AT_START.
 *
 * Build: gcc -I../include -o test_reverse test_reverse.c ../kernel/reverse.c \
 *          ../kernel/rewind.c ../kernel/keyframe_ring.c ../kernel/replay_engine.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "reverse.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* Mock engine hooks: record which keyframe was restored. */
typedef struct { uint64_t restored_keyframe; } sim_t;
static int sim_restore(void* c, uint64_t epoch) { ((sim_t*)c)->restored_keyframe = epoch; return 0; }
static int sim_load(void* c, uint64_t e) { (void)c; (void)e; return 0; }
static int sim_run(void* c, uint64_t e, uint64_t l) { (void)c; (void)e; (void)l; return 0; }

/* Per-keyframe journal lengths (steps until the next keyframe). */
static uint64_t elen(void* c, uint64_t e) {
    (void)c;
    if (e == 20) return 3;
    if (e == 30) return 4;
    if (e == 40) return 5;
    return 0;
}

static reverse_pos_t g_stop_at;
static bool stop_at(void* c, reverse_pos_t p) {
    (void)c;
    return p.epoch == g_stop_at.epoch && p.offset == g_stop_at.offset;
}

static bool pos_is(reverse_pos_t p, uint64_t e, uint64_t o) {
    return p.epoch == e && p.offset == o;
}

/* Build the fixture: ring {20,30,40}, engine(sim), rewind, reverse@(40,2). */
static void fixture(keyframe_ring_t* ring, sim_t* s, replay_engine_t* re,
                    rewind_ctx_t* rw, reverse_ctx_t* rv) {
    keyframe_ring_init(ring, 4);
    keyframe_ring_advance(ring, 20);
    keyframe_ring_advance(ring, 30);
    keyframe_ring_advance(ring, 40);
    s->restored_keyframe = 0;
    replay_hooks_t h = { sim_restore, sim_load, sim_run, s };
    replay_init(re, &h);
    rewind_init(rw, ring, re);
    reverse_init(rv, rw, elen, 0);
    reverse_set_position(rv, 40, 2);
}

int main(void) {
    printf("=== Reverse execution (#170) unit test ===\n");

    /* --- 1. reverse-step within an epoch --- */
    {
        keyframe_ring_t ring; sim_t s; replay_engine_t re; rewind_ctx_t rw; reverse_ctx_t rv;
        fixture(&ring, &s, &re, &rw, &rv);
        CHECK(reverse_step(&rv) == REVERSE_OK && pos_is(reverse_position(&rv), 40, 1),
              "reverse-step (40,2) -> (40,1)");
        CHECK(s.restored_keyframe == 40, "restored keyframe 40 for a within-epoch step");
        CHECK(reverse_step(&rv) == REVERSE_OK && pos_is(reverse_position(&rv), 40, 0),
              "reverse-step (40,1) -> (40,0)");
    }

    /* --- 2. reverse-step across a keyframe boundary --- */
    {
        keyframe_ring_t ring; sim_t s; replay_engine_t re; rewind_ctx_t rw; reverse_ctx_t rv;
        fixture(&ring, &s, &re, &rw, &rv);
        reverse_set_position(&rv, 40, 0);
        CHECK(reverse_step(&rv) == REVERSE_OK && pos_is(reverse_position(&rv), 30, 3),
              "reverse-step (40,0) -> (30,3): previous keyframe's last step");
        CHECK(s.restored_keyframe == 30, "rewind restored keyframe 30 across the boundary");
    }

    /* --- 3. Repeated reverse-steps are stable, no drift --- */
    {
        keyframe_ring_t ring; sim_t s; replay_engine_t re; rewind_ctx_t rw; reverse_ctx_t rv;
        fixture(&ring, &s, &re, &rw, &rv);
        /* Expected exact sequence from (40,2) down to the start. */
        const uint64_t exp[][2] = {
            {40,1},{40,0},{30,3},{30,2},{30,1},{30,0},{20,2},{20,1},{20,0}
        };
        const uint64_t kf[]   = { 40, 40, 30, 30, 30, 30, 20, 20, 20 };
        int ok = 1;
        for (unsigned i = 0; i < 9; i++) {
            if (reverse_step(&rv) != REVERSE_OK) { ok = 0; break; }
            if (!pos_is(reverse_position(&rv), exp[i][0], exp[i][1])) ok = 0;
            if (s.restored_keyframe != kf[i]) ok = 0;
        }
        CHECK(ok, "nine reverse-steps follow the exact position sequence, right keyframe each time");
        CHECK(reverse_step(&rv) == REVERSE_AT_START, "reverse-step at the oldest moment reports AT_START");
        CHECK(pos_is(reverse_position(&rv), 20, 0), "position unchanged once at the start");
    }

    /* --- 4a. reverse-continue stops at an injected condition --- */
    {
        keyframe_ring_t ring; sim_t s; replay_engine_t re; rewind_ctx_t rw; reverse_ctx_t rv;
        fixture(&ring, &s, &re, &rw, &rv);
        g_stop_at.epoch = 30; g_stop_at.offset = 1;
        CHECK(reverse_continue(&rv, stop_at, 0) == REVERSE_OK, "reverse-continue hits the stop");
        CHECK(pos_is(reverse_position(&rv), 30, 1), "reverse-continue landed exactly at the stop");
        CHECK(s.restored_keyframe == 30, "stopped with keyframe 30 restored");
    }

    /* --- 4b. reverse-continue with no stop runs back to the start --- */
    {
        keyframe_ring_t ring; sim_t s; replay_engine_t re; rewind_ctx_t rw; reverse_ctx_t rv;
        fixture(&ring, &s, &re, &rw, &rv);
        g_stop_at.epoch = 999; g_stop_at.offset = 999; /* never matches */
        CHECK(reverse_continue(&rv, stop_at, 0) == REVERSE_AT_START,
              "reverse-continue with no stop reports AT_START");
        CHECK(pos_is(reverse_position(&rv), 20, 0), "and lands at the oldest retained moment");
    }

    if (failures == 0) {
        printf("PASSED: reverse execution steps backward stably via keyframe + replay\n");
        return 0;
    }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
