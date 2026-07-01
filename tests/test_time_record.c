/* Host-side unit test for virtualized time reads (#163).
 *
 * Verifies:
 *   1. OFF is passthrough and records nothing.
 *   2. RECORD logs each live clock read.
 *   3. REPLAY returns the recorded values and never touches hardware: a program
 *      that branches on the clock replays identically even when the live source
 *      is fed hostile values.
 *   4. begin_epoch resets state; load installs replay values.
 *   5. Overflow (record) and underflow (replay) are counted, not out of bounds.
 *
 * Build: gcc -I../include -o test_time_record \
 *            test_time_record.c ../kernel/time_record.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "time_record.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* A mock "TSC": returns a running counter that jumps by an irregular stride,
 * mimicking the nondeterministic spacing of a real cycle counter. */
typedef struct { uint64_t now; int step; } mock_tsc_t;
static uint64_t mock_tsc_read(void* ctx) {
    mock_tsc_t* m = (mock_tsc_t*)ctx;
    static const uint64_t strides[] = {97, 13, 251, 41, 5, 163};
    m->now += strides[(m->step++) % 6];
    return m->now;
}

/* A hostile source: if replay ever touches it, values will diverge. */
static uint64_t hostile_read(void* ctx) { (void)ctx; return 0xDEADBEEFDEADBEEFULL; }

/* The "program under test": read the clock N times and derive a branch bit from
 * each value, returning the bit pattern plus the raw reads. */
static uint32_t run_program(time_record_t* tr, uint32_t n, uint64_t* reads_out) {
    uint32_t bits = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t t = time_record_read(tr);
        reads_out[i] = t;
        if ((t % 7) < 3) bits |= (1u << (i & 31)); /* a clock-dependent branch */
    }
    return bits;
}

int main(void) {
    printf("=== Virtualized time reads (#163) unit test ===\n");

    /* --- 1. OFF passthrough --- */
    {
        mock_tsc_t tsc = { 0, 0 };
        uint64_t buf[16];
        time_record_t tr;
        time_record_init(&tr, TIME_REC_OFF, mock_tsc_read, &tsc, buf, 16);
        uint64_t a = time_record_read(&tr);
        uint64_t b = time_record_read(&tr);
        CHECK(a != b && b > a, "OFF returns live, advancing clock values");
        const uint64_t* vals;
        CHECK(time_record_values(&tr, &vals) == 0, "OFF records nothing");
    }

    /* --- 2 & 3. RECORD then REPLAY reproduce values and branches identically --- */
    uint64_t rec_buf[16];
    time_record_t rec;
    uint64_t rec_reads[8];
    uint32_t rec_bits;
    {
        mock_tsc_t tsc = { 1000, 0 };
        time_record_init(&rec, TIME_REC_RECORD, mock_tsc_read, &tsc, rec_buf, 16);
        time_record_begin_epoch(&rec, 1);
        rec_bits = run_program(&rec, 8, rec_reads);
        const uint64_t* vals;
        uint32_t n = time_record_values(&rec, &vals);
        CHECK(n == 8, "RECORD logged one value per read");
        int match = 1;
        for (uint32_t i = 0; i < 8; i++) if (vals[i] != rec_reads[i]) match = 0;
        CHECK(match, "logged values equal the reads the program saw");
    }

    {
        /* Replay with a HOSTILE live source: if hardware is ever read, the
         * program's reads and branch bits will not match the recording. */
        const uint64_t* rec_vals;
        uint32_t n = time_record_values(&rec, &rec_vals);

        uint64_t rep_buf[16];
        time_record_t rep;
        time_record_init(&rep, TIME_REC_REPLAY, hostile_read, 0, rep_buf, 16);
        CHECK(time_record_load(&rep, 1, rec_vals, n) == TIME_REC_OK, "load replay values");

        uint64_t rep_reads[8];
        uint32_t rep_bits = run_program(&rep, 8, rep_reads);

        int same = 1;
        for (uint32_t i = 0; i < 8; i++) if (rep_reads[i] != rec_reads[i]) same = 0;
        CHECK(same, "REPLAY returns the recorded values, not the hostile hardware");
        CHECK(rep_bits == rec_bits, "clock-dependent branches replay identically");
    }

    /* --- 4. begin_epoch reset --- */
    {
        mock_tsc_t tsc = { 0, 0 };
        uint64_t buf[16];
        time_record_t tr;
        time_record_init(&tr, TIME_REC_RECORD, mock_tsc_read, &tsc, buf, 16);
        time_record_begin_epoch(&tr, 1);
        time_record_read(&tr); time_record_read(&tr);
        const uint64_t* vals;
        CHECK(time_record_values(&tr, &vals) == 2, "two values before new epoch");
        time_record_begin_epoch(&tr, 2);
        CHECK(time_record_values(&tr, &vals) == 0, "begin_epoch cleared the values");
        CHECK(tr.epoch == 2, "epoch advanced");
    }

    /* --- 5. Overflow (record) and underflow (replay) are counted --- */
    {
        mock_tsc_t tsc = { 0, 0 };
        uint64_t buf[4];
        time_record_t tr;
        time_record_init(&tr, TIME_REC_RECORD, mock_tsc_read, &tsc, buf, 4);
        time_record_begin_epoch(&tr, 1);
        for (int i = 0; i < 9; i++) time_record_read(&tr);
        const uint64_t* vals;
        CHECK(time_record_values(&tr, &vals) == 4, "record count clamped to capacity");
        CHECK(tr.overflow == 5, "record overflow counted");

        uint64_t vbuf[4] = {10, 20, 30, 40};
        time_record_t rp;
        time_record_init(&rp, TIME_REC_REPLAY, 0, 0, buf, 4);
        time_record_load(&rp, 1, vbuf, 4);
        uint64_t v = 0;
        for (int i = 0; i < 4; i++) v = time_record_read(&rp);
        CHECK(v == 40, "replay returns recorded values in order");
        uint64_t extra = time_record_read(&rp); /* past the end */
        CHECK(extra == 40 && rp.underflow == 1, "replay past the end returns last, counts underflow");
    }

    if (failures == 0) {
        printf("PASSED: time reads record and replay deterministically\n");
        return 0;
    }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
