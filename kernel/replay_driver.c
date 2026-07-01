/* IKOS Orthogonal Persistence - Replay driver core (#196)
 *
 * See include/replay_driver.h. Pure and host-testable: the journal read, the
 * subsystem load, the keyframe restore, and the scheduler drive are injected,
 * so this file has no allocator, hardware, or on-disk-journal dependency.
 */

#include "replay_driver.h"
#include <stddef.h>

int replay_split_epoch(replay_event_source_t* src, uint64_t epoch,
                       uint64_t* pts,     uint32_t* n_pts,
                       uint64_t* times,   uint32_t* n_times,
                       uint8_t*  entropy, uint32_t* n_entropy) {
    if (!src || !src->begin_epoch || !src->next ||
        !pts || !n_pts || !times || !n_times || !entropy || !n_entropy) {
        return REPLAY_ERR_PARAM;
    }

    *n_pts = 0;
    *n_times = 0;
    *n_entropy = 0;

    if (src->begin_epoch(src->ctx, epoch) != 0) {
        return REPLAY_ERR_LOAD; /* that epoch's journal is unavailable */
    }

    for (;;) {
        replay_event_t ev;
        int r = src->next(src->ctx, &ev);
        if (r < 0) return REPLAY_ERR_LOAD;
        if (r == 0) break; /* epoch exhausted */

        switch (ev.type) {
        case REPLAY_EV_SCHED:
            if (*n_pts >= REPLAY_MAX_PTS) return REPLAY_ERR_LOAD;
            pts[(*n_pts)++] = ev.value;
            break;
        case REPLAY_EV_TIMER:
            if (*n_times >= REPLAY_MAX_TIMES) return REPLAY_ERR_LOAD;
            times[(*n_times)++] = ev.value;
            break;
        case REPLAY_EV_ENTROPY: {
            /* Unpack up to 8 little-endian bytes; len says how many are valid. */
            uint32_t nbytes = ev.len <= 8u ? ev.len : 8u;
            for (uint32_t b = 0; b < nbytes; b++) {
                if (*n_entropy >= REPLAY_MAX_ENTROPY) return REPLAY_ERR_LOAD;
                entropy[(*n_entropy)++] = (uint8_t)(ev.value >> (8u * b));
            }
            break;
        }
        default:
            /* Unknown event types are ignored so the format can grow. */
            break;
        }
    }
    return REPLAY_OK;
}

/* ---- Hook trampolines: adapt the driver's injected steps to replay_hooks_t.
 * The engine passes the driver itself as ctx, so a trampoline can reach both the
 * scratch buffers and the user ctx (d->ctx). ---- */

static int drv_restore(void* c, uint64_t epoch) {
    replay_driver_t* d = (replay_driver_t*)c;
    return d->restore_keyframe(d->ctx, epoch);
}

static int drv_load(void* c, uint64_t epoch) {
    replay_driver_t* d = (replay_driver_t*)c;
    int rc = replay_split_epoch(&d->source, epoch,
                                d->pts, &d->n_pts,
                                d->times, &d->n_times,
                                d->entropy, &d->n_entropy);
    if (rc != REPLAY_OK) return rc;
    return d->load_subsystems(epoch, d->pts, d->n_pts, d->times, d->n_times,
                              d->entropy, d->n_entropy);
}

static int drv_run(void* c, uint64_t epoch, uint64_t limit) {
    replay_driver_t* d = (replay_driver_t*)c;
    uint64_t steps = limit;
    if (limit == REPLAY_WHOLE_EPOCH) {
        /* A whole epoch is re-driven through its last recorded switch point:
         * the deltas just loaded for this epoch bound the logical clock, and
         * the scheduler's replay clock starts at 0 each epoch. */
        steps = 0;
        for (uint32_t i = 0; i < d->n_pts; i++) {
            if (d->pts[i] > steps) steps = d->pts[i];
        }
    }
    return d->drive_steps(d->ctx, epoch, steps);
}

int replay_driver_run(replay_driver_t* d, uint64_t keyframe_epoch,
                      uint64_t target_epoch, uint64_t target_offset) {
    if (!d || !d->restore_keyframe || !d->load_subsystems || !d->drive_steps ||
        !d->source.begin_epoch || !d->source.next) {
        return REPLAY_ERR_PARAM;
    }

    replay_hooks_t hooks;
    hooks.restore_keyframe = drv_restore;
    hooks.load_epoch = drv_load;
    hooks.run_epoch = drv_run;
    hooks.ctx = d;

    int rc = replay_init(&d->engine, &hooks);
    if (rc != REPLAY_OK) return rc;
    return replay_run(&d->engine, keyframe_epoch, target_epoch, target_offset);
}
