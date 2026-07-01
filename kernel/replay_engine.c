/* IKOS Orthogonal Persistence - Replay Engine (#165, epic #159)
 *
 * See include/replay_engine.h and docs/architecture/time-travel.md.
 *
 * Pure orchestration core: no allocator, no hardware, no I/O. The three steps
 * (restore a keyframe, load an epoch's deltas, re-drive an epoch) are injected,
 * so this is host-testable with mocks.
 */

#include "replay_engine.h"

int replay_init(replay_engine_t* re, const replay_hooks_t* hooks) {
    if (!re || !hooks) return REPLAY_ERR_PARAM;
    if (!hooks->restore_keyframe || !hooks->load_epoch || !hooks->run_epoch)
        return REPLAY_ERR_PARAM;
    re->hooks = *hooks;
    re->keyframe_epoch = 0;
    re->target_epoch = 0;
    re->target_offset = 0;
    re->current_epoch = 0;
    re->epochs_run = 0;
    re->done = false;
    return REPLAY_OK;
}

int replay_run(replay_engine_t* re, uint64_t keyframe_epoch,
               uint64_t target_epoch, uint64_t target_offset) {
    if (!re) return REPLAY_ERR_PARAM;
    if (keyframe_epoch > target_epoch) return REPLAY_ERR_PARAM;

    replay_hooks_t* h = &re->hooks;
    void* c = h->ctx;

    re->keyframe_epoch = keyframe_epoch;
    re->target_epoch = target_epoch;
    re->target_offset = target_offset;
    re->epochs_run = 0;
    re->done = false;

    /* 1. Restore the nearest keyframe at or before the target. */
    if (h->restore_keyframe(c, keyframe_epoch) != 0) return REPLAY_ERR_RESTORE;
    re->current_epoch = keyframe_epoch;

    /* 2. Re-drive whole epochs from the keyframe up to the target epoch. */
    for (uint64_t e = keyframe_epoch; e < target_epoch; e++) {
        if (h->load_epoch(c, e) != 0) return REPLAY_ERR_LOAD;
        if (h->run_epoch(c, e, REPLAY_WHOLE_EPOCH) != 0) return REPLAY_ERR_RUN;
        re->current_epoch = e + 1;
        re->epochs_run++;
    }

    /* 3. Re-drive the target epoch for target_offset steps. An offset of 0
     * stops exactly at the target epoch's keyframe boundary. */
    if (target_offset > 0) {
        if (h->load_epoch(c, target_epoch) != 0) return REPLAY_ERR_LOAD;
        if (h->run_epoch(c, target_epoch, target_offset) != 0) return REPLAY_ERR_RUN;
        re->epochs_run++;
    }

    re->current_epoch = target_epoch;
    re->done = true;
    return REPLAY_OK;
}
