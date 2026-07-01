/* IKOS Orthogonal Persistence - Rewind Command (#169, epic #159)
 *
 * See include/rewind.h and docs/architecture/time-travel.md.
 *
 * Pure composition of the keyframe ring (#168) and the replay engine (#165):
 * no allocator, no hardware, no I/O of its own.
 */

#include "rewind.h"

int rewind_init(rewind_ctx_t* rw, const keyframe_ring_t* ring,
                replay_engine_t* engine) {
    if (!rw || !ring || !engine) return REWIND_ERR_PARAM;
    rw->ring = ring;
    rw->engine = engine;
    rw->landed_keyframe = 0;
    rw->landed_epoch = 0;
    rw->landed_offset = 0;
    rw->runnable = false;
    return REWIND_OK;
}

int rewind_to(rewind_ctx_t* rw, uint64_t target_epoch, uint64_t target_offset) {
    if (!rw || !rw->ring || !rw->engine) return REWIND_ERR_PARAM;

    rw->runnable = false;

    /* Reject a rewind to the future: past the newest retained keyframe there is
     * nothing recorded to replay to. An empty ring has no newest, so this also
     * rejects rewinding before anything has been checkpointed. */
    uint64_t newest;
    if (!keyframe_ring_newest(rw->ring, &newest) || target_epoch > newest)
        return REWIND_ERR_OUT_OF_RANGE;

    /* Find the nearest retained keyframe at or before the target. A target that
     * predates the oldest retained keyframe has fallen off the ring. */
    uint64_t keyframe;
    if (!keyframe_ring_find(rw->ring, target_epoch, 0, &keyframe))
        return REWIND_ERR_OUT_OF_RANGE;

    /* Restore that keyframe and replay forward to the exact target. */
    if (replay_run(rw->engine, keyframe, target_epoch, target_offset) != REPLAY_OK)
        return REWIND_ERR_REPLAY;

    rw->landed_keyframe = keyframe;
    rw->landed_epoch = target_epoch;
    rw->landed_offset = target_offset;
    rw->runnable = rw->engine->done;
    return REWIND_OK;
}
