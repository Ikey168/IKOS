/* IKOS Orthogonal Persistence - Deterministic Entropy (#164, epic #159)
 *
 * See include/entropy_record.h and docs/architecture/time-travel.md.
 *
 * Pure decision core: no allocator, no hardware, no I/O. The caller owns the
 * byte buffer and injects the live entropy source, so this is host-testable
 * with a mock source.
 */

#include "entropy_record.h"

int entropy_record_init(entropy_record_t* er, entropy_rec_mode_t mode,
                        entropy_source_fn source, void* ctx,
                        uint8_t* buf, uint32_t capacity) {
    if (!er || !buf || capacity == 0) return ENTROPY_REC_ERR_PARAM;
    er->mode = mode;
    er->source = source;
    er->ctx = ctx;
    er->epoch = 0;
    er->log = buf;
    er->capacity = capacity;
    er->count = 0;
    er->cursor = 0;
    er->overflow = 0;
    er->underflow = 0;
    return ENTROPY_REC_OK;
}

void entropy_record_set_mode(entropy_record_t* er, entropy_rec_mode_t mode) {
    if (!er) return;
    er->mode = mode;
}

void entropy_record_begin_epoch(entropy_record_t* er, uint64_t epoch) {
    if (!er) return;
    er->epoch = epoch;
    er->count = 0;
    er->cursor = 0;
    er->overflow = 0;
    er->underflow = 0;
}

int entropy_record_load(entropy_record_t* er, uint64_t epoch,
                        const uint8_t* bytes, uint32_t n) {
    if (!er || (!bytes && n > 0)) return ENTROPY_REC_ERR_PARAM;
    if (n > er->capacity) return ENTROPY_REC_ERR_PARAM;
    er->epoch = epoch;
    er->count = n;
    er->cursor = 0;
    er->overflow = 0;
    er->underflow = 0;
    for (uint32_t i = 0; i < n; i++) {
        er->log[i] = bytes[i];
    }
    return ENTROPY_REC_OK;
}

/* Draw from the live source, tolerating a NULL source (zero-fills). */
static int draw_live(entropy_record_t* er, uint8_t* out, uint32_t len) {
    if (!er->source) {
        for (uint32_t i = 0; i < len; i++) out[i] = 0;
        return ENTROPY_REC_OK;
    }
    return er->source(er->ctx, out, len) == 0 ? ENTROPY_REC_OK
                                              : ENTROPY_REC_ERR_SOURCE;
}

int entropy_record_fill(entropy_record_t* er, void* out, uint32_t len) {
    if (!er || (!out && len > 0)) return ENTROPY_REC_ERR_PARAM;
    uint8_t* o = (uint8_t*)out;

    switch (er->mode) {
    case ENTROPY_REC_RECORD: {
        int rc = draw_live(er, o, len);
        if (rc != ENTROPY_REC_OK) return rc;
        for (uint32_t i = 0; i < len; i++) {
            if (er->count < er->capacity) {
                er->log[er->count++] = o[i];
            } else {
                er->overflow++;
            }
        }
        return ENTROPY_REC_OK;
    }

    case ENTROPY_REC_REPLAY:
        /* Return recorded bytes; never draw fresh randomness. Past the end,
         * zero-fill and count an underflow so #166 can flag the divergence. */
        for (uint32_t i = 0; i < len; i++) {
            if (er->cursor < er->count) {
                o[i] = er->log[er->cursor++];
            } else {
                o[i] = 0;
                er->underflow++;
            }
        }
        return ENTROPY_REC_OK;

    case ENTROPY_REC_OFF:
    default:
        return draw_live(er, o, len);
    }
}

uint32_t entropy_record_bytes(const entropy_record_t* er, const uint8_t** out) {
    if (!er) { if (out) *out = 0; return 0; }
    if (out) *out = er->log;
    return er->count;
}
