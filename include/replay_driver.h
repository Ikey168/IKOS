/* IKOS Orthogonal Persistence - Replay driver (#196, epic #159)
 *
 * The replay engine (#165) is a pure orchestrator: restore a keyframe, then for
 * each epoch load its recorded deltas and re-drive it. #165 shipped the engine
 * and the kernel's REPLAY-mode loaders but deferred the two hooks that read the
 * journal and re-drive the live scheduler. This driver assembles them.
 *
 * load_epoch reads one epoch's events from the input journal (#161/#194) and
 * splits them by type back into the three delta arrays (preemption points, time
 * reads, entropy bytes), then installs them into the replay wrappers via
 * replay_load_subsystems. run_epoch re-drives the scheduler for the epoch's
 * steps in REPLAY mode. Wrapped around the pure replay engine, this lands the
 * booted system at an arbitrary (epoch, offset).
 *
 * The core here is pure and host-testable: the journal read (an event source),
 * the subsystem load, the keyframe restore, and the scheduler drive are all
 * injected, so tests exercise the split and the re-drive loop with mocks. The
 * kernel adapter (replay_driver_sync.c) wires the real journal store, keyframe
 * store, and scheduler.
 *
 * See docs/architecture/time-travel.md.
 */

#ifndef REPLAY_DRIVER_H
#define REPLAY_DRIVER_H

#include <stdint.h>
#include "replay_engine.h"

/* Journal event codes the splitter recognizes. Mirrors checkpoint_journal.h
 * (TIMER/ENTROPY) and journal_capture.h (SCHED); kept local so this core stays
 * free of the on-disk journal headers. */
#define REPLAY_EV_TIMER     3
#define REPLAY_EV_ENTROPY   4
#define REPLAY_EV_SCHED     5

/* Per-epoch delta bounds; match the record wrappers (KTIME_LOG_MAX etc.). */
#define REPLAY_MAX_PTS      4096
#define REPLAY_MAX_TIMES    4096
#define REPLAY_MAX_ENTROPY  4096

/* One journal event, mirrored so the core needs no journal struct. */
typedef struct {
    uint32_t type;     /* REPLAY_EV_* */
    uint32_t len;      /* valid bytes packed into value (entropy events) */
    uint64_t lclock;
    uint64_t value;
} replay_event_t;

/* Injected per-epoch event source. begin_epoch positions the source at the
 * first event of `epoch` (returns 0 on success, negative if that epoch's
 * journal is unavailable). next yields events in recorded order: returns 1 and
 * fills *out, 0 when the epoch is exhausted, negative on error. */
typedef struct {
    int (*begin_epoch)(void* ctx, uint64_t epoch);
    int (*next)(void* ctx, replay_event_t* out);
    void* ctx;
} replay_event_source_t;

/* Split one epoch's events from `src` into the three delta arrays. Scheduler
 * points and time reads ride the event value; entropy events are unpacked (len
 * little-endian bytes) back into the byte stream. Caller supplies the arrays
 * (sized REPLAY_MAX_*); counts are returned via the n_* out params. Returns
 * REPLAY_OK, or REPLAY_ERR_LOAD on a source error or array overflow. */
int replay_split_epoch(replay_event_source_t* src, uint64_t epoch,
                       uint64_t* pts,     uint32_t* n_pts,
                       uint64_t* times,   uint32_t* n_times,
                       uint8_t*  entropy, uint32_t* n_entropy);

/* The driver: injected concrete steps plus scratch for the current epoch's
 * split deltas. Caller-allocated (it is large); no dynamic memory. */
typedef struct {
    /* Restore the whole-system keyframe at `epoch`. Returns 0 on success. */
    int (*restore_keyframe)(void* ctx, uint64_t epoch);
    /* Source of one epoch's journal events. */
    replay_event_source_t source;
    /* Install one epoch's split deltas into the replay wrappers (REPLAY mode). */
    int (*load_subsystems)(uint64_t epoch,
                           const uint64_t* pts, uint32_t n_pts,
                           const uint64_t* times, uint32_t n_times,
                           const uint8_t* entropy, uint32_t n_entropy);
    /* Re-drive `steps` scheduler steps of `epoch` in REPLAY mode. */
    int (*drive_steps)(void* ctx, uint64_t epoch, uint64_t steps);
    void* ctx;

    /* Scratch for the epoch currently being loaded/run. */
    uint64_t pts[REPLAY_MAX_PTS];
    uint32_t n_pts;
    uint64_t times[REPLAY_MAX_TIMES];
    uint32_t n_times;
    uint8_t  entropy[REPLAY_MAX_ENTROPY];
    uint32_t n_entropy;

    replay_engine_t engine;
} replay_driver_t;

/* Assemble the load_epoch/run_epoch/restore_keyframe hooks from the driver's
 * injected steps and run the replay engine from keyframe_epoch forward to
 * (target_epoch, target_offset). Returns REPLAY_OK or a negative REPLAY_ERR_*. */
int replay_driver_run(replay_driver_t* d, uint64_t keyframe_epoch,
                      uint64_t target_epoch, uint64_t target_offset);

/* ---- Kernel adapter (replay_driver_sync.c) ----
 * Wires the real journal store, keyframe store, and scheduler, and picks the
 * nearest retained keyframe at or before target_epoch. Restores the booted
 * kernel to (target_epoch, target_offset). Returns REPLAY_OK or a negative
 * code. Bracket with replay_enter()/replay_exit() is handled internally. */
int replay_to(uint64_t target_epoch, uint64_t target_offset);

#endif /* REPLAY_DRIVER_H */
