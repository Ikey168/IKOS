/* IKOS Orthogonal Persistence - Reverse Execution (#170, epic #159)
 *
 * See include/reverse.h and docs/architecture/time-travel.md.
 *
 * Pure composition over the rewind verb (#169): no allocator, no hardware, no
 * I/O of its own.
 */

#include "reverse.h"

int reverse_init(reverse_ctx_t* rc, rewind_ctx_t* rw,
                 reverse_epoch_len_fn epoch_len, void* ctx) {
    if (!rc || !rw || !epoch_len) return REVERSE_ERR_PARAM;
    rc->rw = rw;
    rc->epoch_len = epoch_len;
    rc->ctx = ctx;
    rc->cur.epoch = 0;
    rc->cur.offset = 0;
    return REVERSE_OK;
}

void reverse_set_position(reverse_ctx_t* rc, uint64_t epoch, uint64_t offset) {
    if (!rc) return;
    rc->cur.epoch = epoch;
    rc->cur.offset = offset;
}

reverse_pos_t reverse_position(const reverse_ctx_t* rc) {
    if (rc) return rc->cur;
    reverse_pos_t z = { 0, 0 };
    return z;
}

/* Compute the step one unit before `cur`. Within an epoch, decrement the offset.
 * At a keyframe boundary (offset 0), cross to the previous retained keyframe's
 * last step, skipping any empty epochs. Returns false at the oldest moment. */
static bool prev_pos(reverse_ctx_t* rc, reverse_pos_t cur, reverse_pos_t* out) {
    if (cur.offset > 0) {
        out->epoch = cur.epoch;
        out->offset = cur.offset - 1;
        return true;
    }
    /* At a boundary: find the keyframe strictly before this one. */
    uint64_t epoch = cur.epoch;
    while (epoch > 0) {
        uint64_t prev_kf;
        if (!keyframe_ring_find(rc->rw->ring, epoch - 1, 0, &prev_kf))
            return false; /* no earlier retained keyframe */
        uint64_t len = rc->epoch_len(rc->ctx, prev_kf);
        if (len > 0) {
            out->epoch = prev_kf;
            out->offset = len - 1;
            return true;
        }
        epoch = prev_kf; /* empty epoch: keep walking back */
    }
    return false;
}

int reverse_step(reverse_ctx_t* rc) {
    if (!rc || !rc->rw) return REVERSE_ERR_PARAM;
    reverse_pos_t prev;
    if (!prev_pos(rc, rc->cur, &prev)) return REVERSE_AT_START;
    if (rewind_to(rc->rw, prev.epoch, prev.offset) != REWIND_OK)
        return REVERSE_ERR_REWIND;
    rc->cur = prev;
    return REVERSE_OK;
}

int reverse_continue(reverse_ctx_t* rc, reverse_stop_fn should_stop, void* pctx) {
    if (!rc || !rc->rw) return REVERSE_ERR_PARAM;
    for (;;) {
        reverse_pos_t prev;
        if (!prev_pos(rc, rc->cur, &prev)) return REVERSE_AT_START;
        if (rewind_to(rc->rw, prev.epoch, prev.offset) != REWIND_OK)
            return REVERSE_ERR_REWIND;
        rc->cur = prev;
        if (should_stop && should_stop(pctx, prev)) return REVERSE_OK;
    }
}
