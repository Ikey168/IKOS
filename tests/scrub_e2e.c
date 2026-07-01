/* "Scrub the machine backwards" end-to-end demo (#173).
 *
 * Records a session, then scrubs it backward through the whole time-travel
 * stack, showing that the reconstructed state at each past moment exactly
 * matches what it was at that time:
 *
 *   1. RECORD E epochs of a session whose state folds in nondeterministic time
 *      (#163) and entropy (#164) inputs. Snapshot a keyframe at the start of
 *      each epoch into the retention ring (#168) and capture each epoch's inputs
 *      (the journal, #161). Keep the true timeline for verification.
 *   2. REWIND (#169) to several past (epoch, offset) points via the replay
 *      engine (#165) and check the reconstructed state matches the timeline.
 *   3. REVERSE-STEP (#170) backward from a point and show the state at each
 *      earlier moment, again matching the timeline, including across keyframe
 *      boundaries.
 *
 * Exits non-zero on any mismatch, so it doubles as a CI gate.
 *
 * Build: gcc -Iinclude -o scrub_e2e tests/scrub_e2e.c kernel/time_record.c \
 *          kernel/entropy_record.c kernel/replay_engine.c kernel/keyframe_ring.c \
 *          kernel/rewind.c kernel/reverse.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "time_record.h"
#include "entropy_record.h"
#include "keyframe_ring.h"
#include "replay_engine.h"
#include "rewind.h"
#include "reverse.h"

#define E     4    /* epochs (keyframes) */
#define STEPS 4    /* steps per epoch */
#define SEED  0x1234ABCDULL

/* Recorded history. */
static uint64_t g_kf[E];                 /* keyframe state at each epoch start */
static uint64_t g_jt[E][STEPS];          /* journaled time reads per epoch */
static uint8_t  g_je[E][STEPS];          /* journaled entropy bytes per epoch */
static uint64_t g_timeline[E][STEPS + 1];/* true state after k steps of each epoch */

/* Live scrub state and replay wrappers. */
static uint64_t         g_h;
static time_record_t    g_tr;
static entropy_record_t g_er;

/* One session step: fold a time read and an entropy byte into the state. */
static void step(time_record_t* tr, entropy_record_t* er, uint64_t* h) {
    uint64_t t = time_record_read(tr);
    uint8_t e = 0;
    entropy_record_fill(er, &e, 1);
    *h = (*h) * 1103515245ull + t + (uint64_t)e + 0x9E3779B97F4A7C15ull;
}

/* ---- Mock nondeterministic sources for the record pass ---- */
typedef struct { uint64_t now; uint32_t s; } tsc_t;
static uint64_t tsc_read(void* c) {
    tsc_t* t = (tsc_t*)c;
    t->s = t->s * 1103515245u + 12345u;
    t->now += 1 + (t->s >> 25);
    return t->now;
}
typedef struct { uint32_t s; } prng_t;
static int prng_fill(void* c, void* buf, uint32_t len) {
    prng_t* p = (prng_t*)c;
    uint8_t* b = (uint8_t*)buf;
    for (uint32_t i = 0; i < len; i++) { p->s = p->s * 1103515245u + 12345u; b[i] = (uint8_t)(p->s >> 24); }
    return 0;
}

/* ---- Replay engine hooks over the scrub state ---- */
static int hook_restore(void* c, uint64_t epoch) { (void)c; g_h = g_kf[epoch]; return 0; }
static int hook_load(void* c, uint64_t epoch) {
    (void)c;
    if (time_record_load(&g_tr, epoch, g_jt[epoch], STEPS) != TIME_REC_OK) return -1;
    if (entropy_record_load(&g_er, epoch, g_je[epoch], STEPS) != ENTROPY_REC_OK) return -1;
    return 0;
}
static int hook_run(void* c, uint64_t epoch, uint64_t limit) {
    (void)c; (void)epoch;
    uint64_t n = (limit == REPLAY_WHOLE_EPOCH) ? STEPS : limit;
    for (uint64_t i = 0; i < n; i++) step(&g_tr, &g_er, &g_h);
    return 0;
}
static uint64_t elen(void* c, uint64_t e) { (void)c; (void)e; return STEPS; }

static int failures = 0;
static void expect(uint64_t got, uint64_t want, const char* what) {
    if (got == want) {
        printf("  ok:   %s state=0x%016llx\n", what, (unsigned long long)got);
    } else {
        printf("  FAIL: %s state=0x%016llx expected 0x%016llx\n",
               what, (unsigned long long)got, (unsigned long long)want);
        failures++;
    }
}

int main(void) {
    printf("=== Scrub the machine backwards (#173) ===\n");

    /* ---- 1. Record the session ---- */
    keyframe_ring_t ring;
    keyframe_ring_init(&ring, E);
    tsc_t tsc = { 1000, 0xBEEF };
    prng_t prng = { 0xC0FFEE };
    uint64_t tbuf[STEPS];
    uint8_t  ebuf[STEPS];
    time_record_init(&g_tr, TIME_REC_RECORD, tsc_read, &tsc, tbuf, STEPS);
    entropy_record_init(&g_er, ENTROPY_REC_RECORD, prng_fill, &prng, ebuf, STEPS);

    g_h = SEED;
    for (uint64_t ep = 0; ep < E; ep++) {
        g_kf[ep] = g_h;                       /* keyframe at the epoch boundary */
        keyframe_ring_advance(&ring, ep);
        time_record_begin_epoch(&g_tr, ep);
        entropy_record_begin_epoch(&g_er, ep);
        g_timeline[ep][0] = g_h;
        for (int s = 0; s < STEPS; s++) {
            step(&g_tr, &g_er, &g_h);
            g_timeline[ep][s + 1] = g_h;
        }
        const uint64_t* tv; uint32_t nt = time_record_values(&g_tr, &tv);
        const uint8_t*  eb; uint32_t ne = entropy_record_bytes(&g_er, &eb);
        for (uint32_t i = 0; i < nt && i < STEPS; i++) g_jt[ep][i] = tv[i];
        for (uint32_t i = 0; i < ne && i < STEPS; i++) g_je[ep][i] = eb[i];
    }
    printf("  recorded %d epochs x %d steps; keyframes at epochs 0..%d\n", E, STEPS, E - 1);
    printf("  final live state = 0x%016llx\n", (unsigned long long)g_h);

    /* ---- Build the replay/rewind/reverse stack over the recording ---- */
    time_record_init(&g_tr, TIME_REC_REPLAY, 0, 0, tbuf, STEPS);
    entropy_record_init(&g_er, ENTROPY_REC_REPLAY, 0, 0, ebuf, STEPS);
    replay_hooks_t hooks = { hook_restore, hook_load, hook_run, 0 };
    replay_engine_t re; replay_init(&re, &hooks);
    rewind_ctx_t rw; rewind_init(&rw, &ring, &re);
    reverse_ctx_t rv; reverse_init(&rv, &rw, elen, 0);

    /* ---- 2. Rewind to several past moments ---- */
    printf("scrub: rewind to past moments\n");
    struct { uint64_t ep, off; } targets[] = { {3,4},{3,2},{2,0},{1,3},{0,1} };
    for (unsigned i = 0; i < 5; i++) {
        uint64_t ep = targets[i].ep, off = targets[i].off;
        char label[32]; /* build "(ep,off)" cheaply */
        label[0] = 'r'; label[1] = 'e'; label[2] = 'w'; label[3] = ' ';
        label[4] = '('; label[5] = (char)('0' + ep); label[6] = ',';
        label[7] = (char)('0' + off); label[8] = ')'; label[9] = 0;
        int rc = rewind_to(&rw, ep, off);
        if (rc != REWIND_OK) { printf("  FAIL: rewind_to returned %d\n", rc); failures++; continue; }
        expect(g_h, g_timeline[ep][off], label);
    }

    /* ---- 3. Reverse-step backward, matching the timeline each step ---- */
    printf("scrub: reverse-step from (3,2)\n");
    reverse_set_position(&rv, 3, 2);
    rewind_to(&rw, 3, 2); /* land at the start point */
    for (int i = 0; i < 6; i++) {
        int rc = reverse_step(&rv);
        if (rc == REVERSE_AT_START) { printf("  ok:   reached the oldest retained moment\n"); break; }
        if (rc != REVERSE_OK) { printf("  FAIL: reverse_step returned %d\n", rc); failures++; break; }
        reverse_pos_t p = reverse_position(&rv);
        char label[32];
        label[0]='('; label[1]=(char)('0'+p.epoch); label[2]=',';
        label[3]=(char)('0'+p.offset); label[4]=')'; label[5]=0;
        expect(g_h, g_timeline[p.epoch][p.offset], label);
    }

    if (failures == 0) {
        printf("PASSED: every reconstructed past moment matched the recorded timeline\n");
        return 0;
    }
    printf("FAILED: %d mismatch(es)\n", failures);
    return 1;
}
