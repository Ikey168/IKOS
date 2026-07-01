/* IKOS Orthogonal Persistence - Deterministic Preemption (#162, epic #159)
 *
 * See include/sched_record.h and docs/architecture/time-travel.md.
 *
 * Pure decision core: no allocator, no hardware, no I/O. The scheduler owns the
 * points buffer and the logical clock and calls sched_record_decide() at the
 * point it would otherwise consult the round-robin time slice.
 */

#include "sched_record.h"

int sched_record_init(sched_record_t* r, sched_rec_mode_t mode,
                      uint64_t* buf, uint32_t capacity) {
    if (!r || !buf || capacity == 0) return SCHED_REC_ERR_PARAM;
    r->mode = mode;
    r->epoch = 0;
    r->points = buf;
    r->capacity = capacity;
    r->count = 0;
    r->cursor = 0;
    r->overflow = 0;
    return SCHED_REC_OK;
}

void sched_record_set_mode(sched_record_t* r, sched_rec_mode_t mode) {
    if (!r) return;
    r->mode = mode;
}

void sched_record_begin_epoch(sched_record_t* r, uint64_t epoch) {
    if (!r) return;
    r->epoch = epoch;
    r->count = 0;
    r->cursor = 0;
    r->overflow = 0;
}

int sched_record_load(sched_record_t* r, uint64_t epoch,
                      const uint64_t* pts, uint32_t n) {
    if (!r || (!pts && n > 0)) return SCHED_REC_ERR_PARAM;
    if (n > r->capacity) return SCHED_REC_ERR_PARAM;
    r->epoch = epoch;
    r->count = n;
    r->cursor = 0;
    r->overflow = 0;
    for (uint32_t i = 0; i < n; i++) {
        r->points[i] = pts[i];
    }
    return SCHED_REC_OK;
}

bool sched_record_decide(sched_record_t* r, bool want, uint64_t lclock) {
    if (!r) return want;

    switch (r->mode) {
    case SCHED_REC_RECORD:
        if (want) {
            if (r->count < r->capacity) {
                r->points[r->count++] = lclock;
            } else {
                r->overflow++;
            }
        }
        return want;

    case SCHED_REC_REPLAY:
        /* Preempt exactly where the live run did: when the logical clock has
         * reached the next recorded switch point, consume it and switch. The
         * natural `want` is deliberately ignored so real time-slice timing
         * cannot perturb the replayed schedule. */
        if (r->cursor < r->count && lclock >= r->points[r->cursor]) {
            r->cursor++;
            return true;
        }
        return false;

    case SCHED_REC_OFF:
    default:
        return want;
    }
}

uint32_t sched_record_points(const sched_record_t* r, const uint64_t** out) {
    if (!r) { if (out) *out = 0; return 0; }
    if (out) *out = r->points;
    return r->count;
}
