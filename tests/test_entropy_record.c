/* Host-side unit test for deterministic entropy (#164).
 *
 * Verifies:
 *   1. OFF is passthrough and records nothing.
 *   2. RECORD logs the drawn bytes, across calls of differing lengths.
 *   3. REPLAY returns the recorded bytes and never draws fresh randomness: a
 *      program that branches on random values replays identically even when the
 *      live source is hostile.
 *   4. begin_epoch resets state; load installs replay bytes.
 *   5. Overflow (record) and underflow (replay) are counted, not out of bounds.
 *
 * Build: gcc -I../include -o test_entropy_record \
 *            test_entropy_record.c ../kernel/entropy_record.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "entropy_record.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* A mock entropy source: a simple LCG, so "random" but reproducible per seed. */
typedef struct { uint32_t state; } mock_prng_t;
static int mock_prng_fill(void* ctx, void* buf, uint32_t len) {
    mock_prng_t* p = (mock_prng_t*)ctx;
    uint8_t* b = (uint8_t*)buf;
    for (uint32_t i = 0; i < len; i++) {
        p->state = p->state * 1103515245u + 12345u;
        b[i] = (uint8_t)(p->state >> 24);
    }
    return 0;
}

/* A hostile source: if replay ever draws from it, bytes will diverge. */
static int hostile_fill(void* ctx, void* buf, uint32_t len) {
    (void)ctx;
    uint8_t* b = (uint8_t*)buf;
    for (uint32_t i = 0; i < len; i++) b[i] = 0xFF;
    return 0;
}

/* The "program under test": draw randomness in a few differently-sized chunks
 * and derive a branch bit from each byte, returning the bit pattern and the raw
 * bytes it saw. */
static uint32_t run_program(entropy_record_t* er, uint8_t* seen, uint32_t total) {
    uint32_t bits = 0, got = 0;
    const uint32_t chunks[] = {3, 1, 5, 7, 4}; /* uneven draw sizes */
    uint32_t ci = 0;
    while (got < total) {
        uint32_t want = chunks[ci++ % 5];
        if (got + want > total) want = total - got;
        entropy_record_fill(er, seen + got, want);
        got += want;
    }
    for (uint32_t i = 0; i < total; i++)
        if ((seen[i] & 3) == 0) bits |= (1u << (i & 31)); /* entropy-dependent branch */
    return bits;
}

int main(void) {
    printf("=== Deterministic entropy (#164) unit test ===\n");

    /* --- 1. OFF passthrough --- */
    {
        mock_prng_t prng = { 1 };
        uint8_t buf[64];
        entropy_record_t er;
        entropy_record_init(&er, ENTROPY_REC_OFF, mock_prng_fill, &prng, buf, 64);
        uint8_t out[8];
        CHECK(entropy_record_fill(&er, out, 8) == ENTROPY_REC_OK, "OFF fills from live source");
        const uint8_t* bytes;
        CHECK(entropy_record_bytes(&er, &bytes) == 0, "OFF records nothing");
    }

    /* --- 2 & 3. RECORD then REPLAY reproduce bytes and branches identically --- */
    uint8_t rec_buf[64];
    entropy_record_t rec;
    uint8_t rec_seen[20];
    uint32_t rec_bits;
    {
        mock_prng_t prng = { 0xC0FFEE };
        entropy_record_init(&rec, ENTROPY_REC_RECORD, mock_prng_fill, &prng, rec_buf, 64);
        entropy_record_begin_epoch(&rec, 1);
        rec_bits = run_program(&rec, rec_seen, 20);
        const uint8_t* bytes;
        uint32_t n = entropy_record_bytes(&rec, &bytes);
        CHECK(n == 20, "RECORD logged every drawn byte across uneven chunks");
        int match = 1;
        for (uint32_t i = 0; i < 20; i++) if (bytes[i] != rec_seen[i]) match = 0;
        CHECK(match, "logged bytes equal what the program drew");
    }

    {
        /* Replay with a HOSTILE live source: if fresh randomness is ever drawn,
         * the program's bytes and branch bits will not match the recording. */
        const uint8_t* rec_bytes;
        uint32_t n = entropy_record_bytes(&rec, &rec_bytes);

        uint8_t rep_buf[64];
        entropy_record_t rep;
        entropy_record_init(&rep, ENTROPY_REC_REPLAY, hostile_fill, 0, rep_buf, 64);
        CHECK(entropy_record_load(&rep, 1, rec_bytes, n) == ENTROPY_REC_OK, "load replay bytes");

        uint8_t rep_seen[20];
        uint32_t rep_bits = run_program(&rep, rep_seen, 20);

        int same = 1;
        for (uint32_t i = 0; i < 20; i++) if (rep_seen[i] != rec_seen[i]) same = 0;
        CHECK(same, "REPLAY returns recorded bytes, not the hostile source");
        CHECK(rep_bits == rec_bits, "entropy-dependent branches replay identically");
    }

    /* --- 4. begin_epoch reset --- */
    {
        mock_prng_t prng = { 7 };
        uint8_t buf[64];
        entropy_record_t er;
        entropy_record_init(&er, ENTROPY_REC_RECORD, mock_prng_fill, &prng, buf, 64);
        entropy_record_begin_epoch(&er, 1);
        uint8_t tmp[10];
        entropy_record_fill(&er, tmp, 10);
        const uint8_t* bytes;
        CHECK(entropy_record_bytes(&er, &bytes) == 10, "ten bytes before new epoch");
        entropy_record_begin_epoch(&er, 2);
        CHECK(entropy_record_bytes(&er, &bytes) == 0, "begin_epoch cleared the bytes");
        CHECK(er.epoch == 2, "epoch advanced");
    }

    /* --- 5. Overflow (record) and underflow (replay) are counted --- */
    {
        mock_prng_t prng = { 3 };
        uint8_t buf[4];
        entropy_record_t er;
        entropy_record_init(&er, ENTROPY_REC_RECORD, mock_prng_fill, &prng, buf, 4);
        entropy_record_begin_epoch(&er, 1);
        uint8_t tmp[10];
        entropy_record_fill(&er, tmp, 10);
        const uint8_t* bytes;
        CHECK(entropy_record_bytes(&er, &bytes) == 4, "record count clamped to capacity");
        CHECK(er.overflow == 6, "record overflow counted the dropped bytes");

        uint8_t vbuf[4] = {0x11, 0x22, 0x33, 0x44};
        entropy_record_t rp;
        entropy_record_init(&rp, ENTROPY_REC_REPLAY, hostile_fill, 0, buf, 4);
        entropy_record_load(&rp, 1, vbuf, 4);
        uint8_t out[6];
        entropy_record_fill(&rp, out, 6); /* two past the end */
        CHECK(out[0] == 0x11 && out[3] == 0x44, "replay returns recorded bytes in order");
        CHECK(out[4] == 0 && out[5] == 0 && rp.underflow == 2,
              "replay past the end zero-fills and counts underflow");
    }

    if (failures == 0) {
        printf("PASSED: entropy records and replays deterministically\n");
        return 0;
    }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
