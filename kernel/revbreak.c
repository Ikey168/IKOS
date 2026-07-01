/* IKOS Orthogonal Persistence - Reverse Breakpoints and Watchpoints (#171)
 *
 * See include/revbreak.h and docs/architecture/time-travel.md.
 *
 * Pure layer over reverse execution (#170): no allocator, no hardware, no I/O
 * of its own. Both operations scan backward via reverse_step / reverse_continue,
 * which restore the nearest keyframe and replay to each candidate position, so
 * the injected predicate reads the real system state there.
 */

#include "revbreak.h"

typedef struct {
    revbreak_cond_fn cond;
    void*            ctx;
} bp_pack_t;

/* Adapt a breakpoint condition to the reverse-continue stop predicate. The
 * position is ignored: the condition reads the state restored at it. */
static bool bp_adapter(void* c, reverse_pos_t pos) {
    (void)pos;
    bp_pack_t* b = (bp_pack_t*)c;
    return b->cond(b->ctx);
}

int reverse_breakpoint(reverse_ctx_t* rv, revbreak_cond_fn cond, void* ctx,
                       reverse_pos_t* hit) {
    if (!rv || !cond) return REVBREAK_ERR_PARAM;
    bp_pack_t pack = { cond, ctx };
    int rc = reverse_continue(rv, bp_adapter, &pack);
    if (rc == REVERSE_OK) {
        if (hit) *hit = reverse_position(rv);
        return REVBREAK_OK;
    }
    if (rc == REVERSE_AT_START) return REVBREAK_NOT_FOUND;
    return REVBREAK_ERR;
}

int reverse_watchpoint(reverse_ctx_t* rv, revbreak_probe_fn probe, void* ctx,
                       reverse_pos_t* hit) {
    if (!rv || !rv->rw || !probe) return REVBREAK_ERR_PARAM;

    /* Land at the current position so the probe reads its value. */
    reverse_pos_t cur = reverse_position(rv);
    if (rewind_to(rv->rw, cur.epoch, cur.offset) != REWIND_OK) return REVBREAK_ERR;
    uint64_t newer = probe(ctx);

    /* Walk backward, comparing each position's value to the one after it. The
     * first difference (going backward) is the most recent write. */
    for (;;) {
        reverse_pos_t before = reverse_position(rv);  /* position holding `newer` */
        int rc = reverse_step(rv);
        if (rc == REVERSE_AT_START) return REVBREAK_NOT_FOUND;
        if (rc != REVERSE_OK) return REVBREAK_ERR;

        uint64_t older = probe(ctx);                  /* value one step earlier */
        if (older != newer) {
            /* The value changed between here and `before`: the write is at
             * `before`. Land there. */
            if (rewind_to(rv->rw, before.epoch, before.offset) != REWIND_OK)
                return REVBREAK_ERR;
            reverse_set_position(rv, before.epoch, before.offset);
            if (hit) *hit = before;
            return REVBREAK_OK;
        }
        newer = older;
    }
}
