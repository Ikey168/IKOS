/* Host-side unit test for the keyframe retention ring (#168).
 *
 * Verifies:
 *   1. Configurable retention: the ring holds up to N keyframes.
 *   2. Old keyframes are reclaimed beyond N (oldest evicted, reported).
 *   3. find() returns the nearest retained keyframe at or before a target, and
 *      nothing before the rewind horizon.
 *   4. pack/unpack round-trips the index and rejects a corrupted buffer.
 *
 * Build: gcc -I../include -o test_keyframe_ring \
 *            test_keyframe_ring.c ../kernel/keyframe_ring.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "keyframe_ring.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

int main(void) {
    printf("=== Keyframe retention ring (#168) unit test ===\n");

    /* --- 1. Configurable retention, filling below capacity --- */
    {
        keyframe_ring_t r;
        CHECK(keyframe_ring_init(&r, 4) == KEYFRAME_RING_OK, "init with N=4");
        CHECK(keyframe_ring_init(&r, 0) == KEYFRAME_RING_ERR_PARAM, "N=0 rejected");
        CHECK(keyframe_ring_init(&r, KEYFRAME_RING_MAX + 1) == KEYFRAME_RING_ERR_PARAM,
              "N over the max rejected");

        keyframe_ring_init(&r, 4);
        keyframe_ring_advance(&r, 10);
        keyframe_ring_advance(&r, 20);
        keyframe_ring_advance(&r, 30);
        CHECK(keyframe_ring_count(&r) == 3, "three keyframes retained");
        CHECK(!keyframe_ring_reclaimed(&r, 0), "nothing reclaimed below capacity");
        uint64_t oldest, newest;
        CHECK(keyframe_ring_oldest(&r, &oldest) && oldest == 10, "oldest is 10");
        CHECK(keyframe_ring_newest(&r, &newest) && newest == 30, "newest is 30");
    }

    /* --- 2. Reclaim beyond N --- */
    {
        keyframe_ring_t r; keyframe_ring_init(&r, 3);
        keyframe_ring_advance(&r, 10);
        keyframe_ring_advance(&r, 20);
        keyframe_ring_advance(&r, 30);   /* full: {10,20,30} */
        uint64_t evicted;
        keyframe_ring_advance(&r, 40);   /* evict 10: {40,20,30} */
        CHECK(keyframe_ring_count(&r) == 3, "count stays at capacity");
        CHECK(keyframe_ring_reclaimed(&r, &evicted) && evicted == 10,
              "oldest keyframe (10) reclaimed and reported");
        uint64_t oldest, newest;
        keyframe_ring_oldest(&r, &oldest); keyframe_ring_newest(&r, &newest);
        CHECK(oldest == 20 && newest == 40, "horizon advanced to {20..40}");
    }

    /* --- 3. find nearest at or before target --- */
    {
        keyframe_ring_t r; keyframe_ring_init(&r, 3);
        keyframe_ring_advance(&r, 20);
        keyframe_ring_advance(&r, 30);
        keyframe_ring_advance(&r, 40);   /* retained {20,30,40} */
        uint32_t slot; uint64_t epoch;

        CHECK(keyframe_ring_find(&r, 40, &slot, &epoch) && epoch == 40, "exact newest");
        CHECK(keyframe_ring_find(&r, 35, 0, &epoch) && epoch == 30, "between: nearest below is 30");
        CHECK(keyframe_ring_find(&r, 25, 0, &epoch) && epoch == 20, "between: nearest below is 20");
        CHECK(keyframe_ring_find(&r, 20, 0, &epoch) && epoch == 20, "exact oldest");
        CHECK(keyframe_ring_find(&r, 100, 0, &epoch) && epoch == 40, "past newest clamps to newest");
        CHECK(!keyframe_ring_find(&r, 15, 0, 0), "before the horizon returns nothing");
    }

    /* --- 4. pack / unpack round-trip and CRC rejection --- */
    {
        keyframe_ring_t r; keyframe_ring_init(&r, 5);
        keyframe_ring_advance(&r, 7);
        keyframe_ring_advance(&r, 9);

        uint8_t buf[2048];
        int n = keyframe_ring_pack(&r, buf, sizeof(buf));
        CHECK(n > 0 && (uint32_t)n == keyframe_ring_packed_size(), "pack reports its size");

        keyframe_ring_t r2;
        CHECK(keyframe_ring_unpack(&r2, buf, (uint32_t)n) == KEYFRAME_RING_OK, "unpack ok");
        uint64_t e1, e2;
        keyframe_ring_newest(&r, &e1); keyframe_ring_newest(&r2, &e2);
        CHECK(r2.capacity == 5 && r2.count == 2 && e1 == e2, "unpacked ring matches");

        buf[8] ^= 0xFF; /* corrupt a byte inside the struct region */
        CHECK(keyframe_ring_unpack(&r2, buf, (uint32_t)n) == KEYFRAME_RING_ERR_CRC,
              "corrupted buffer rejected by CRC");

        uint8_t small[4];
        CHECK(keyframe_ring_pack(&r, small, sizeof(small)) == KEYFRAME_RING_ERR_SIZE,
              "pack rejects an undersized buffer");
    }

    /* --- 5. Degenerate N=1 keeps only the latest (store-like) --- */
    {
        keyframe_ring_t r; keyframe_ring_init(&r, 1);
        keyframe_ring_advance(&r, 100);
        uint64_t evicted;
        keyframe_ring_advance(&r, 200);
        CHECK(keyframe_ring_count(&r) == 1, "N=1 retains a single keyframe");
        CHECK(keyframe_ring_reclaimed(&r, &evicted) && evicted == 100, "N=1 evicts the prior one");
        uint64_t only;
        CHECK(keyframe_ring_newest(&r, &only) && only == 200, "only the latest remains");
    }

    if (failures == 0) {
        printf("PASSED: retention ring keeps the last N keyframes and finds rewind targets\n");
        return 0;
    }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
