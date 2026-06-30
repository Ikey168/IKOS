/* Host-side unit test for the checkpoint barrier state machine (#138).
 *
 * Pure state machine, no kernel deps. Verifies the lifecycle
 * IDLE -> ARMED -> QUIESCENT -> IDLE across single- and multi-CPU
 * configurations, plus park/unpark edge cases.
 *
 * Build: gcc -I../include -o test_checkpoint_barrier test_checkpoint_barrier.c \
 *            ../kernel/checkpoint_barrier.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "checkpoint_barrier.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

int main(void) {
    checkpoint_barrier_t b;

    /* === init + clamping === */
    printf("Test 1: init\n");
    checkpoint_barrier_init(&b, 0); /* clamps to 1 */
    CHECK(checkpoint_barrier_state(&b) == CHECKPOINT_BARRIER_IDLE, "starts IDLE");
    CHECK(b.ncpus == 1, "ncpus 0 clamps to 1");
    checkpoint_barrier_init(&b, 999); /* clamps to MAX */
    CHECK(b.ncpus == CHECKPOINT_BARRIER_MAX_CPUS, "ncpus clamps to MAX");

    /* === single-CPU collapse === */
    printf("Test 2: single CPU\n");
    checkpoint_barrier_init(&b, 1);
    CHECK(checkpoint_barrier_arm(&b) == true, "arm IDLE -> true");
    CHECK(checkpoint_barrier_state(&b) == CHECKPOINT_BARRIER_ARMED, "now ARMED");
    CHECK(checkpoint_barrier_arm(&b) == false, "re-arm is a no-op");
    CHECK(checkpoint_barrier_park(&b, 0, true) == true, "the one CPU parks -> quiescent");
    CHECK(checkpoint_barrier_is_quiescent(&b), "is quiescent");
    checkpoint_barrier_release(&b);
    CHECK(checkpoint_barrier_state(&b) == CHECKPOINT_BARRIER_IDLE, "release -> IDLE");
    CHECK(checkpoint_barrier_arm(&b) == true, "can re-arm after release");

    /* === multi-CPU: quiescent only when the LAST CPU parks === */
    printf("Test 3: multi CPU\n");
    checkpoint_barrier_init(&b, 3);
    checkpoint_barrier_arm(&b);
    CHECK(checkpoint_barrier_park(&b, 0, true) == false, "cpu0 parks, not yet quiescent");
    CHECK(checkpoint_barrier_park(&b, 2, true) == false, "cpu2 parks, not yet quiescent");
    CHECK(!checkpoint_barrier_is_quiescent(&b), "still ARMED with one CPU running");
    CHECK(checkpoint_barrier_park(&b, 1, true) == true, "last CPU parks -> quiescent");
    CHECK(checkpoint_barrier_is_quiescent(&b), "quiescent once all parked");

    /* === edge cases === */
    printf("Test 4: edges\n");
    checkpoint_barrier_init(&b, 2);
    checkpoint_barrier_arm(&b);
    CHECK(checkpoint_barrier_park(&b, 0, false) == false, "can_park=false does not park");
    CHECK(checkpoint_barrier_park(&b, 0, true) == false, "cpu0 parks (1 of 2)");
    CHECK(checkpoint_barrier_park(&b, 0, true) == false, "parking cpu0 again is idempotent");
    CHECK(checkpoint_barrier_park(&b, 5, true) == false, "cpu >= ncpus rejected");
    CHECK(checkpoint_barrier_park(&b, 1, true) == true, "cpu1 parks -> quiescent");

    /* park when not armed */
    checkpoint_barrier_init(&b, 1);
    CHECK(checkpoint_barrier_park(&b, 0, true) == false, "park while IDLE rejected");

    /* === unpark drops quiescence === */
    printf("Test 5: unpark\n");
    checkpoint_barrier_init(&b, 2);
    checkpoint_barrier_arm(&b);
    checkpoint_barrier_park(&b, 0, true);
    checkpoint_barrier_park(&b, 1, true);
    CHECK(checkpoint_barrier_is_quiescent(&b), "both parked -> quiescent");
    checkpoint_barrier_unpark(&b, 1);
    CHECK(checkpoint_barrier_state(&b) == CHECKPOINT_BARRIER_ARMED, "unpark drops back to ARMED");
    CHECK(!checkpoint_barrier_is_quiescent(&b), "no longer quiescent");
    CHECK(checkpoint_barrier_park(&b, 1, true) == true, "re-park -> quiescent again");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
