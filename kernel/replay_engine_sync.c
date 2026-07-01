/* IKOS Orthogonal Persistence - Replay Engine, kernel adapter (#165)
 *
 * Concrete wiring of the pure replay engine (replay_engine.c) to the in-tree
 * subsystems that are already merged: the checkpoint restore path and the
 * REPLAY-mode loaders for the three record/replay wrappers (preemption #162,
 * time #163, entropy #164).
 *
 * The per-epoch deltas themselves come from the input journal (#161); a kernel
 * replay driver reads an epoch's events from the journal and calls
 * replay_load_subsystems() with them, then re-drives that epoch. Those two
 * halves (journal read + re-drive loop) assemble the load_epoch/run_epoch hooks
 * for replay_run(); this adapter provides the pieces that are unambiguously in
 * tree today.
 */

#include "replay_engine.h"
#include "scheduler.h"       /* scheduler_preempt_{set_mode,load} (#162) */
#include "time_record.h"     /* ktime_{set_mode,load} (#163) */
#include "entropy_record.h"  /* kentropy_{set_mode,load} (#164) */
#include "checkpoint.h"      /* checkpoint_restore_boot, snapshot_store_t */

void replay_enter(void) {
    scheduler_preempt_set_mode(SCHED_REC_REPLAY);
    ktime_set_mode(TIME_REC_REPLAY);
    kentropy_set_mode(ENTROPY_REC_REPLAY);
}

void replay_exit(void) {
    scheduler_preempt_set_mode(SCHED_REC_OFF);
    ktime_set_mode(TIME_REC_OFF);
    kentropy_set_mode(ENTROPY_REC_OFF);
}

int replay_load_subsystems(uint64_t epoch,
                           const uint64_t* preempt_pts, uint32_t n_pts,
                           const uint64_t* time_vals,   uint32_t n_time,
                           const uint8_t*  entropy,     uint32_t n_entropy) {
    if (scheduler_preempt_load(epoch, preempt_pts, n_pts) != 0) return REPLAY_ERR_LOAD;
    if (ktime_load(epoch, time_vals, n_time) != 0) return REPLAY_ERR_LOAD;
    if (kentropy_load(epoch, entropy, n_entropy) != 0) return REPLAY_ERR_LOAD;
    return REPLAY_OK;
}

/* The checkpoint store backing the keyframes, bound once at init. */
static snapshot_store_t* g_replay_store;

void replay_bind_store(void* snapshot_store) {
    g_replay_store = (snapshot_store_t*)snapshot_store;
}

int replay_restore_current(void) {
    if (!g_replay_store) return REPLAY_ERR_RESTORE;
    /* Reuses the existing restore path: reconstructs process table, scheduler,
     * and contexts from the store's checkpoint. Selecting an arbitrary past
     * keyframe (rather than the latest) needs the keyframe retention ring
     * (#168); today the store holds the nearest available checkpoint. */
    return checkpoint_restore_boot(g_replay_store) < 0 ? REPLAY_ERR_RESTORE
                                                       : REPLAY_OK;
}
