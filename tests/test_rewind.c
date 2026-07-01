/* Host-side unit test for the rewind command (#169).
 *
 * Drives a REAL keyframe ring (#168) and a REAL replay engine (#165), with mock
 * engine hooks that record what the engine was asked to do. Verifies:
 *   1. rewind_to restores the nearest keyframe at or before the target and
 *      replays forward to it, landing runnable.
 *   2. Targets outside the retained ring are rejected with a clear error
 *      (before the oldest keyframe, or after the newest).
 *   3. A replay failure en route is surfaced.
 *
 * Build: gcc -I../include -o test_rewind test_rewind.c \
 *            ../kernel/rewind.c ../kernel/keyframe_ring.c ../kernel/replay_engine.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "rewind.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* Mock engine hooks: record the keyframe restored and the target reached. */
typedef struct {
    uint64_t restored_keyframe;
    uint64_t last_run_epoch;
    uint32_t epochs_run;
    int      fail_restore;
} sim_t;

static int sim_restore(void* c, uint64_t epoch) {
    sim_t* s = (sim_t*)c;
    if (s->fail_restore) return -1;
    s->restored_keyframe = epoch;
    return 0;
}
static int sim_load(void* c, uint64_t epoch) { (void)c; (void)epoch; return 0; }
static int sim_run(void* c, uint64_t epoch, uint64_t limit) {
    sim_t* s = (sim_t*)c; (void)limit;
    s->last_run_epoch = epoch;
    s->epochs_run++;
    return 0;
}

static void engine_with(replay_engine_t* re, sim_t* s) {
    replay_hooks_t h;
    h.restore_keyframe = sim_restore;
    h.load_epoch = sim_load;
    h.run_epoch = sim_run;
    h.ctx = s;
    replay_init(re, &h);
}

/* A ring retaining keyframes {20, 30, 40}. */
static void ring_20_30_40(keyframe_ring_t* r) {
    keyframe_ring_init(r, 4);
    keyframe_ring_advance(r, 20);
    keyframe_ring_advance(r, 30);
    keyframe_ring_advance(r, 40);
}

int main(void) {
    printf("=== Rewind command (#169) unit test ===\n");

    /* --- 1. Rewind to a point between keyframes --- */
    {
        keyframe_ring_t ring; ring_20_30_40(&ring);
        sim_t s = {0}; replay_engine_t re; engine_with(&re, &s);
        rewind_ctx_t rw; CHECK(rewind_init(&rw, &ring, &re) == REWIND_OK, "init");

        CHECK(rewind_to(&rw, 35, 3) == REWIND_OK, "rewind to (35, +3) succeeds");
        CHECK(s.restored_keyframe == 30, "restored the nearest keyframe at or before 35 (=30)");
        CHECK(rw.landed_keyframe == 30 && rw.landed_epoch == 35 && rw.landed_offset == 3,
              "landed at the requested target");
        CHECK(rw.runnable, "system is runnable at the target");
        /* engine re-drove epochs 30..34 whole, then 35 partial */
        CHECK(s.epochs_run == 6 && s.last_run_epoch == 35, "replayed forward through the target epoch");
    }

    /* --- 2. Exact-keyframe target --- */
    {
        keyframe_ring_t ring; ring_20_30_40(&ring);
        sim_t s = {0}; replay_engine_t re; engine_with(&re, &s);
        rewind_ctx_t rw; rewind_init(&rw, &ring, &re);
        CHECK(rewind_to(&rw, 40, 0) == REWIND_OK && s.restored_keyframe == 40,
              "rewind to an exact keyframe restores it directly");
        CHECK(rw.runnable, "runnable at the exact keyframe");
    }

    /* --- 3. Out-of-range targets are rejected clearly --- */
    {
        keyframe_ring_t ring; ring_20_30_40(&ring);
        sim_t s = {0}; replay_engine_t re; engine_with(&re, &s);
        rewind_ctx_t rw; rewind_init(&rw, &ring, &re);

        CHECK(rewind_to(&rw, 15, 0) == REWIND_ERR_OUT_OF_RANGE,
              "target before the oldest keyframe is rejected");
        CHECK(rewind_to(&rw, 100, 0) == REWIND_ERR_OUT_OF_RANGE,
              "target after the newest keyframe is rejected");
        CHECK(!rw.runnable, "a rejected rewind leaves the system not-runnable");
        CHECK(s.restored_keyframe == 0, "a rejected rewind never restored anything");
    }

    /* --- 4. Empty ring rejects any rewind --- */
    {
        keyframe_ring_t ring; keyframe_ring_init(&ring, 4);
        sim_t s = {0}; replay_engine_t re; engine_with(&re, &s);
        rewind_ctx_t rw; rewind_init(&rw, &ring, &re);
        CHECK(rewind_to(&rw, 10, 0) == REWIND_ERR_OUT_OF_RANGE,
              "an empty ring has no keyframe to rewind to");
    }

    /* --- 5. A replay failure en route is surfaced --- */
    {
        keyframe_ring_t ring; ring_20_30_40(&ring);
        sim_t s = {0}; s.fail_restore = 1;
        replay_engine_t re; engine_with(&re, &s);
        rewind_ctx_t rw; rewind_init(&rw, &ring, &re);
        CHECK(rewind_to(&rw, 30, 0) == REWIND_ERR_REPLAY, "replay failure surfaced as REWIND_ERR_REPLAY");
        CHECK(!rw.runnable, "not runnable after a failed replay");
    }

    /* --- 6. Bad params --- */
    {
        rewind_ctx_t rw;
        keyframe_ring_t ring; ring_20_30_40(&ring);
        replay_engine_t re; sim_t s = {0}; engine_with(&re, &s);
        CHECK(rewind_init(&rw, 0, &re) == REWIND_ERR_PARAM, "init rejects a NULL ring");
        CHECK(rewind_init(&rw, &ring, 0) == REWIND_ERR_PARAM, "init rejects a NULL engine");
    }

    if (failures == 0) {
        printf("PASSED: rewind restores the nearest keyframe and replays to the target\n");
        return 0;
    }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
