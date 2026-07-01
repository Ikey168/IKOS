/* Time-travel end-to-end harness (#167).
 *
 * Proves the deterministic-replay property against the REAL modules, headless
 * (no emulator), the way persistence_demo.sh proves durability:
 *
 *   1. RECORD a session whose result depends on nondeterministic inputs (time
 *      reads via #163 and entropy draws via #164), capturing those inputs.
 *   2. PERSIST the captured inputs to the input journal (#161), CRC-protected.
 *   3. REPLAY the session, driven by the replay engine (#165), reloading the
 *      inputs from the journal into the wrappers in REPLAY mode.
 *   4. ASSERT the replayed final state is BYTE-IDENTICAL to the recording, and
 *      the divergence detector (#166) confirms no nondeterminism leak.
 *   5. CONTROL: a fresh live run with different inputs produces a DIFFERENT
 *      state, so the match in step 4 is meaningful and not trivially constant.
 *
 * Exits non-zero on any failure, so it doubles as a CI gate.
 *
 * Build: gcc -Iinclude -o timetravel_e2e tests/timetravel_e2e.c \
 *          kernel/time_record.c kernel/entropy_record.c kernel/replay_engine.c \
 *          kernel/divergence.c kernel/checkpoint_journal.c
 */

#include <stdint.h>
#include <stdbool.h>

/* Declare libc bits directly to avoid <string.h>, which pulls in <sys/types.h>
 * whose ssize_t collides with IKOS's vfs.h (reached via fat.h). */
typedef __SIZE_TYPE__ size_t;
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
extern int   memcmp(const void*, const void*, size_t);
extern int   printf(const char*, ...);

#include "time_record.h"
#include "entropy_record.h"
#include "divergence.h"
#include "replay_engine.h"
#include "checkpoint_journal.h"

/* ---- Session shape ---- */
#define TOTAL_STEPS 32
#define STATE_BYTES 32

/* ---- In-memory mock block device for the journal (headless) ---- */
#define MOCK_SECTORS 512
static struct { uint8_t data[MOCK_SECTORS * JOURNAL_SECTOR_SIZE]; } g_mock;

static int mock_read(void* dev, uint32_t sector, uint32_t count, void* buf) {
    (void)dev;
    if ((uint64_t)sector + count > MOCK_SECTORS) return -1;
    memcpy(buf, g_mock.data + (size_t)sector * JOURNAL_SECTOR_SIZE,
           (size_t)count * JOURNAL_SECTOR_SIZE);
    return 0;
}
static int mock_write(void* dev, uint32_t sector, uint32_t count, const void* buf) {
    (void)dev;
    if ((uint64_t)sector + count > MOCK_SECTORS) return -1;
    memcpy(g_mock.data + (size_t)sector * JOURNAL_SECTOR_SIZE, buf,
           (size_t)count * JOURNAL_SECTOR_SIZE);
    return 0;
}
static fat_block_device_t g_bdev;
static fat_block_device_t* make_dev(void) {
    g_bdev.read_sectors = mock_read;
    g_bdev.write_sectors = mock_write;
    g_bdev.sector_size = JOURNAL_SECTOR_SIZE;
    g_bdev.total_sectors = MOCK_SECTORS;
    g_bdev.private_data = 0;
    return &g_bdev;
}

/* ---- Mock "live" nondeterministic sources (deterministic per seed) ---- */
typedef struct { uint64_t now; uint32_t stride; } tsc_t;
static uint64_t tsc_read(void* c) {
    tsc_t* t = (tsc_t*)c;
    t->stride = t->stride * 1103515245u + 12345u;
    t->now += 1 + (t->stride >> 24);
    return t->now;
}
typedef struct { uint32_t s; } prng_t;
static int prng_fill(void* c, void* buf, uint32_t len) {
    prng_t* p = (prng_t*)c;
    uint8_t* b = (uint8_t*)buf;
    for (uint32_t i = 0; i < len; i++) {
        p->s = p->s * 1103515245u + 12345u;
        b[i] = (uint8_t)(p->s >> 24);
    }
    return 0;
}

/* ---- The session program: result depends on time and entropy ---- */
typedef struct {
    time_record_t*    tr;
    entropy_record_t* er;
    uint8_t           state[STATE_BYTES];
} session_t;

static void session_reset(session_t* s) {
    for (int i = 0; i < STATE_BYTES; i++) s->state[i] = (uint8_t)(0x5A + i);
}
static void session_step(session_t* s) {
    uint64_t t = time_record_read(s->tr);
    uint8_t e = 0;
    entropy_record_fill(s->er, &e, 1);
    uint8_t carry = (uint8_t)(t ^ (t >> 8) ^ (t >> 16) ^ (t >> 24));
    for (int i = 0; i < STATE_BYTES; i++) {
        s->state[i] = (uint8_t)(s->state[i] * 31u + carry + e + (uint8_t)i);
        carry = s->state[i];
    }
}
static void session_run(session_t* s) {
    session_reset(s);
    for (int i = 0; i < TOTAL_STEPS; i++) session_step(s);
}

/* ---- Replay engine hooks over the session ---- */
typedef struct {
    session_t*      sess;
    const uint64_t* time_vals;  uint32_t nt;
    const uint8_t*  ent_bytes;  uint32_t ne;
} replay_ctx_t;

static int hook_restore(void* c, uint64_t epoch) {
    (void)epoch;
    session_reset(((replay_ctx_t*)c)->sess);
    return 0;
}
static int hook_load(void* c, uint64_t epoch) {
    replay_ctx_t* r = (replay_ctx_t*)c;
    if (time_record_load(r->sess->tr, epoch, r->time_vals, r->nt) != TIME_REC_OK)
        return -1;
    if (entropy_record_load(r->sess->er, epoch, r->ent_bytes, r->ne) != ENTROPY_REC_OK)
        return -1;
    return 0;
}
static int hook_run(void* c, uint64_t epoch, uint64_t limit) {
    (void)epoch;
    replay_ctx_t* r = (replay_ctx_t*)c;
    uint64_t steps = (limit == REPLAY_WHOLE_EPOCH) ? TOTAL_STEPS : limit;
    for (uint64_t i = 0; i < steps; i++) session_step(r->sess);
    return 0;
}

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

int main(void) {
    printf("=== Time-travel end-to-end: record, replay, assert bit-identical (#167) ===\n");

    /* ---- 1. RECORD ---- */
    tsc_t  tsc  = { 1000, 0xA5A5 };
    prng_t prng = { 0xC0FFEE };
    uint64_t tlog[TOTAL_STEPS];
    uint8_t  elog[TOTAL_STEPS];
    time_record_t tr;
    entropy_record_t er;
    time_record_init(&tr, TIME_REC_RECORD, tsc_read, &tsc, tlog, TOTAL_STEPS);
    entropy_record_init(&er, ENTROPY_REC_RECORD, prng_fill, &prng, elog, TOTAL_STEPS);
    time_record_begin_epoch(&tr, 0);
    entropy_record_begin_epoch(&er, 0);

    session_t rec = { &tr, &er, {0} };
    session_run(&rec);
    uint8_t state_record[STATE_BYTES];
    memcpy(state_record, rec.state, STATE_BYTES);
    printf("  step 1: recorded a %d-step session (%d time reads, %d entropy draws)\n",
           TOTAL_STEPS, TOTAL_STEPS, TOTAL_STEPS);

    /* ---- 2. PERSIST captured inputs to the journal ---- */
    const uint64_t* tv; uint32_t nt = time_record_values(&tr, &tv);
    const uint8_t*  eb; uint32_t ne = entropy_record_bytes(&er, &eb);
    memset(&g_mock, 0, sizeof(g_mock));
    journal_store_t js;
    journal_store_init(&js, make_dev(), 0 /*base*/, 8 /*slot sectors*/);
    journal_store_format(&js);
    journal_writer_t jw;
    journal_store_begin(&js, 0 /*epoch*/, 0 /*base lclock*/, &jw);
    for (uint32_t i = 0; i < nt; i++)
        journal_writer_append(&jw, JOURNAL_EV_TIMER, i, tv[i]);
    for (uint32_t i = 0; i < ne; i++)
        journal_writer_append(&jw, JOURNAL_EV_ENTROPY, nt + i, eb[i]);
    CHECK(journal_store_commit(&jw) == JOURNAL_OK, "persisted recorded inputs to the journal");

    /* ---- 3. REPLAY: reload inputs from the journal, drive the replay engine ---- */
    uint64_t time_vals[TOTAL_STEPS]; uint32_t r_nt = 0;
    uint8_t  ent_bytes[TOTAL_STEPS]; uint32_t r_ne = 0;
    journal_reader_t jr;
    CHECK(journal_store_load(&js, &jr) == JOURNAL_OK, "reloaded the journal");
    for (;;) {
        journal_event_t ev;
        if (journal_reader_next(&jr, &ev) != JOURNAL_OK) break;
        if (ev.type == JOURNAL_EV_TIMER)   time_vals[r_nt++] = ev.value;
        else if (ev.type == JOURNAL_EV_ENTROPY) ent_bytes[r_ne++] = (uint8_t)ev.value;
    }
    CHECK(r_nt == nt && r_ne == ne, "journal round-tripped every recorded input in order");

    time_record_t tr2;
    entropy_record_t er2;
    uint64_t tbuf[TOTAL_STEPS];
    uint8_t  ebuf[TOTAL_STEPS];
    time_record_init(&tr2, TIME_REC_REPLAY, 0, 0, tbuf, TOTAL_STEPS);
    entropy_record_init(&er2, ENTROPY_REC_REPLAY, 0, 0, ebuf, TOTAL_STEPS);

    session_t rep = { &tr2, &er2, {0} };
    replay_ctx_t rctx = { &rep, time_vals, r_nt, ent_bytes, r_ne };
    replay_hooks_t hooks = { hook_restore, hook_load, hook_run, &rctx };
    replay_engine_t re;
    replay_init(&re, &hooks);
    CHECK(replay_run(&re, 0 /*keyframe*/, 0 /*target epoch*/, TOTAL_STEPS /*offset*/) == REPLAY_OK,
          "replayed the session from the journal via the replay engine");

    /* ---- 4. ASSERT bit-identical, and confirm with the divergence detector ---- */
    CHECK(memcmp(state_record, rep.state, STATE_BYTES) == 0,
          "replayed final state is BYTE-IDENTICAL to the recording");

    divergence_t dv;
    uint32_t sum_rec = divergence_checksum(0, state_record, STATE_BYTES);
    divergence_init(&dv, DIVERGE_REPLAY);
    divergence_expect(&dv, 0, &sum_rec, 1);
    uint32_t sum_rep = divergence_checksum(0, rep.state, STATE_BYTES);
    divergence_check(&dv, 0, sum_rep);
    CHECK(!divergence_has_diverged(&dv), "divergence detector confirms no nondeterminism leak");

    /* ---- 5. CONTROL: different live inputs => different state ---- */
    tsc_t  tsc2  = { 1000, 0x3C3C };
    prng_t prng2 = { 0x12345 };
    uint64_t tlog2[TOTAL_STEPS];
    uint8_t  elog2[TOTAL_STEPS];
    time_record_t tr3;
    entropy_record_t er3;
    time_record_init(&tr3, TIME_REC_OFF, tsc_read, &tsc2, tlog2, TOTAL_STEPS);
    entropy_record_init(&er3, ENTROPY_REC_OFF, prng_fill, &prng2, elog2, TOTAL_STEPS);
    session_t ctl = { &tr3, &er3, {0} };
    session_run(&ctl);
    CHECK(memcmp(state_record, ctl.state, STATE_BYTES) != 0,
          "a fresh live run with different inputs differs (so the match is meaningful)");

    if (failures == 0) {
        printf("PASSED: recorded session replayed byte-identically from the journal\n");
        return 0;
    }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
