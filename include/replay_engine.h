/* IKOS Orthogonal Persistence - Replay Engine (#165, epic #159)
 *
 * "Replay mode: keyframe + journal to target epoch." A keyframe (checkpoint)
 * captures the whole system at an epoch; the input journal (#161) plus the
 * record/replay wrappers (preemption #162, time #163, entropy #164) capture the
 * delta between keyframes. This engine reconstructs any past moment: restore
 * the nearest keyframe at or before the target, then re-drive execution forward
 * epoch by epoch, feeding each epoch's recorded deltas, until the target is
 * reached. The target is a checkpoint epoch plus an offset into that epoch's
 * journal.
 *
 * The orchestration core is pure and host-testable: restoring a keyframe,
 * loading an epoch's deltas, and re-driving an epoch are injected as hooks, so
 * tests drive them with mocks. The kernel adapter (replay_engine_sync.c) wires
 * the real restore path (checkpoint_restore_boot) and the in-tree REPLAY-mode
 * loaders for the three wrappers.
 *
 * See docs/architecture/time-travel.md.
 */

#ifndef REPLAY_ENGINE_H
#define REPLAY_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#define REPLAY_OK            0
#define REPLAY_ERR_PARAM    -1
#define REPLAY_ERR_RESTORE  -2   /* restore_keyframe hook failed */
#define REPLAY_ERR_LOAD     -3   /* load_epoch hook failed */
#define REPLAY_ERR_RUN      -4   /* run_epoch hook failed */

/* Passed to run_epoch to mean "re-drive the whole epoch" (as opposed to a
 * bounded number of steps for the final, partial epoch). */
#define REPLAY_WHOLE_EPOCH  0xFFFFFFFFFFFFFFFFULL

/* Injected steps. All return 0 on success. */
typedef struct {
    /* Restore the whole-system keyframe at `epoch` (the nearest checkpoint at
     * or before the target). Reconstructs process table, scheduler, contexts. */
    int (*restore_keyframe)(void* ctx, uint64_t epoch);
    /* Load epoch `epoch`'s recorded deltas into the replay wrappers and put
     * them in REPLAY mode. */
    int (*load_epoch)(void* ctx, uint64_t epoch);
    /* Re-drive up to `limit` steps of epoch `epoch` (REPLAY_WHOLE_EPOCH = all
     * of it). Consumes that epoch's loaded deltas. */
    int (*run_epoch)(void* ctx, uint64_t epoch, uint64_t limit);
    void* ctx;
} replay_hooks_t;

typedef struct {
    replay_hooks_t hooks;
    uint64_t keyframe_epoch;   /* epoch we restored from */
    uint64_t target_epoch;     /* epoch to stop at */
    uint64_t target_offset;    /* steps into the target epoch to stop after */
    uint64_t current_epoch;    /* progress */
    uint32_t epochs_run;       /* full + partial epochs re-driven */
    bool     done;
} replay_engine_t;

/* Bind the engine to its hooks. */
int replay_init(replay_engine_t* re, const replay_hooks_t* hooks);

/* Restore `keyframe_epoch`, then re-drive forward to (target_epoch,
 * target_offset). keyframe_epoch must be at or before target_epoch. Full epochs
 * from the keyframe up to target_epoch are run whole; the target epoch is run
 * for target_offset steps (0 stops exactly at the target epoch boundary). */
int replay_run(replay_engine_t* re, uint64_t keyframe_epoch,
               uint64_t target_epoch, uint64_t target_offset);

/* ---- Kernel adapter (replay_engine_sync.c) ----
 * Concrete wiring to the in-tree subsystems. replay_enter/exit toggle the three
 * wrappers between REPLAY and OFF; replay_load_subsystems installs one epoch's
 * deltas; replay_bind_store/replay_restore_current wire the checkpoint restore
 * path. The store is passed as void* so this header stays free of the snapshot
 * types. */
void replay_enter(void);
void replay_exit(void);
int  replay_load_subsystems(uint64_t epoch,
                            const uint64_t* preempt_pts, uint32_t n_pts,
                            const uint64_t* time_vals,   uint32_t n_time,
                            const uint8_t*  entropy,     uint32_t n_entropy);
void replay_bind_store(void* snapshot_store);
int  replay_restore_current(void);

#endif /* REPLAY_ENGINE_H */
