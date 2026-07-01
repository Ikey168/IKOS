/* IKOS Orthogonal Persistence - Virtualized Time Reads (#163, epic #159)
 *
 * See include/time_record.h and docs/architecture/time-travel.md.
 *
 * Pure decision core: no allocator, no hardware, no I/O. The caller owns the
 * value buffer and injects the live clock as a function pointer, so this is
 * host-testable with a mock source.
 */

#include "time_record.h"

int time_record_init(time_record_t* tr, time_rec_mode_t mode,
                     time_source_fn source, void* ctx,
                     uint64_t* buf, uint32_t capacity) {
    if (!tr || !buf || capacity == 0) return TIME_REC_ERR_PARAM;
    tr->mode = mode;
    tr->source = source;
    tr->ctx = ctx;
    tr->epoch = 0;
    tr->log = buf;
    tr->capacity = capacity;
    tr->count = 0;
    tr->cursor = 0;
    tr->overflow = 0;
    tr->underflow = 0;
    tr->last = 0;
    return TIME_REC_OK;
}

void time_record_set_mode(time_record_t* tr, time_rec_mode_t mode) {
    if (!tr) return;
    tr->mode = mode;
}

void time_record_begin_epoch(time_record_t* tr, uint64_t epoch) {
    if (!tr) return;
    tr->epoch = epoch;
    tr->count = 0;
    tr->cursor = 0;
    tr->overflow = 0;
    tr->underflow = 0;
}

int time_record_load(time_record_t* tr, uint64_t epoch,
                     const uint64_t* vals, uint32_t n) {
    if (!tr || (!vals && n > 0)) return TIME_REC_ERR_PARAM;
    if (n > tr->capacity) return TIME_REC_ERR_PARAM;
    tr->epoch = epoch;
    tr->count = n;
    tr->cursor = 0;
    tr->overflow = 0;
    tr->underflow = 0;
    for (uint32_t i = 0; i < n; i++) {
        tr->log[i] = vals[i];
    }
    return TIME_REC_OK;
}

/* Read the live source, tolerating a NULL source (returns the last value). */
static uint64_t read_live(time_record_t* tr) {
    if (!tr->source) return tr->last;
    return tr->source(tr->ctx);
}

uint64_t time_record_read(time_record_t* tr) {
    if (!tr) return 0;

    switch (tr->mode) {
    case TIME_REC_RECORD: {
        uint64_t v = read_live(tr);
        if (tr->count < tr->capacity) {
            tr->log[tr->count++] = v;
        } else {
            tr->overflow++;
        }
        tr->last = v;
        return v;
    }

    case TIME_REC_REPLAY: {
        uint64_t v;
        if (tr->cursor < tr->count) {
            v = tr->log[tr->cursor++];
        } else {
            /* More reads than were recorded: a divergence. Return the last
             * value so callers stay monotonic, and count it so #166 can flag
             * it. Hardware is deliberately never read here. */
            tr->underflow++;
            v = tr->last;
        }
        tr->last = v;
        return v;
    }

    case TIME_REC_OFF:
    default: {
        uint64_t v = read_live(tr);
        tr->last = v;
        return v;
    }
    }
}

uint32_t time_record_values(const time_record_t* tr, const uint64_t** out) {
    if (!tr) { if (out) *out = 0; return 0; }
    if (out) *out = tr->log;
    return tr->count;
}
