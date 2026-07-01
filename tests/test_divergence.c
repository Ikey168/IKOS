/* Host-side unit test for the divergence detector (#166).
 *
 * Verifies:
 *   1. State is checksummed at epoch boundaries on both record and replay, and
 *      an identical replay passes every check.
 *   2. A mutated component is caught and the first divergence is reported with
 *      the epoch and the component that diverged.
 *   3. The detector runs as a guard: divergence_has_diverged() flips exactly
 *      when a mismatch occurs (what a debug-build assertion keys on).
 *   4. OFF disables checking; the checksum helper is deterministic.
 *
 * Build: gcc -I../include -o test_divergence \
 *            test_divergence.c ../kernel/divergence.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "divergence.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* A simulated system state: a few components, each a byte blob we checksum. */
#define NCOMP 4
typedef struct { uint8_t blob[NCOMP][32]; } sysstate_t;

static void state_fill(sysstate_t* s, uint64_t epoch) {
    for (int c = 0; c < NCOMP; c++)
        for (int i = 0; i < 32; i++)
            s->blob[c][i] = (uint8_t)(c * 41 + epoch * 7 + i);
}

/* Checksum every component of a state into sums[NCOMP]. */
static void state_sums(const sysstate_t* s, uint32_t* sums) {
    for (int c = 0; c < NCOMP; c++)
        sums[c] = divergence_checksum(0, s->blob[c], 32);
}

int main(void) {
    printf("=== Divergence detector (#166) unit test ===\n");

    /* Record checksums for a few epochs of a live run. */
    uint32_t recorded[3][NCOMP];
    for (uint64_t e = 0; e < 3; e++) {
        sysstate_t s; state_fill(&s, e);
        divergence_t d; divergence_init(&d, DIVERGE_RECORD);
        divergence_begin_epoch(&d, e);
        uint32_t sums[NCOMP]; state_sums(&s, sums);
        for (int c = 0; c < NCOMP; c++) divergence_record(&d, c, sums[c]);
        for (int c = 0; c < NCOMP; c++) {
            bool present;
            recorded[e][c] = divergence_recorded(&d, c, &present);
            if (!present) recorded[e][c] = 0xBAD;
        }
    }

    /* --- 1. Identical replay passes every check --- */
    {
        divergence_t d; divergence_init(&d, DIVERGE_REPLAY);
        int all_ok = 1;
        for (uint64_t e = 0; e < 3; e++) {
            sysstate_t s; state_fill(&s, e); /* same state as the live run */
            divergence_expect(&d, e, recorded[e], NCOMP);
            uint32_t sums[NCOMP]; state_sums(&s, sums);
            for (int c = 0; c < NCOMP; c++)
                if (!divergence_check(&d, c, sums[c])) all_ok = 0;
        }
        CHECK(all_ok, "identical replay passes every component check at every epoch");
        CHECK(!divergence_has_diverged(&d), "no divergence reported for an identical replay");
        CHECK(d.checks == 3 * NCOMP && d.mismatches == 0, "all checks counted, zero mismatches");
    }

    /* --- 2 & 3. A mutated component at epoch 1 is caught and pinpointed --- */
    {
        divergence_t d; divergence_init(&d, DIVERGE_REPLAY);
        bool flipped_when_expected = false;
        for (uint64_t e = 0; e < 3; e++) {
            sysstate_t s; state_fill(&s, e);
            if (e == 1) s.blob[2][10] ^= 0xFF; /* nondeterminism leak in component 2 */
            divergence_expect(&d, e, recorded[e], NCOMP);
            uint32_t sums[NCOMP]; state_sums(&s, sums);
            for (int c = 0; c < NCOMP; c++) {
                bool before = divergence_has_diverged(&d);
                bool ok = divergence_check(&d, c, sums[c]);
                bool after = divergence_has_diverged(&d);
                if (!before && !ok && after && e == 1 && c == 2)
                    flipped_when_expected = true;
            }
        }
        CHECK(divergence_has_diverged(&d), "a mutated component triggers divergence");
        CHECK(flipped_when_expected, "the guard flips exactly at the diverging check");
        CHECK(d.diverged_epoch == 1 && d.diverged_component == 2,
              "divergence reported at the right epoch and component");
        CHECK(d.diverged_expected == recorded[1][2] && d.diverged_actual != recorded[1][2],
              "report carries expected vs actual checksum");
    }

    /* --- 4. OFF disables checking; checksum helper is deterministic --- */
    {
        divergence_t d; divergence_init(&d, DIVERGE_OFF);
        CHECK(divergence_check(&d, 0, 0xDEADBEEF), "OFF always passes");
        CHECK(!divergence_has_diverged(&d), "OFF never diverges");

        uint8_t buf[16];
        for (int i = 0; i < 16; i++) buf[i] = (uint8_t)(i * 3 + 1);
        CHECK(divergence_checksum(0, buf, 16) == divergence_checksum(0, buf, 16),
              "checksum is deterministic");
    }

    if (failures == 0) {
        printf("PASSED: divergence detector catches replay nondeterminism leaks\n");
        return 0;
    }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
