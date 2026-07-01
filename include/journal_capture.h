/* IKOS Orthogonal Persistence - Per-epoch journal capture (#194, epic #159)
 *
 * When a checkpoint epoch closes, the deterministic-replay journal for that
 * epoch must be written alongside the checkpoint: the scheduler's preemption
 * points, the time/cycle reads, and the entropy bytes that were consumed while
 * the epoch was live. A keyframe (the checkpoint) records the whole system
 * state; this journal records the nondeterministic delta between keyframes, so
 * the replay engine (#165/#196) can boot the nearest keyframe and re-drive
 * execution forward to any past moment.
 *
 * This is the capture side of the input journal (#161). The core here is pure
 * and host-testable: it pulls the epoch's deltas through injected source
 * function pointers (never the real subsystems) and writes them to a
 * journal_store_t, which itself talks to storage only through a
 * fat_block_device_t. The live wiring lives in the kernel adapter below
 * (journal_capture_sync.c).
 */

#ifndef JOURNAL_CAPTURE_H
#define JOURNAL_CAPTURE_H

#include <stdint.h>
#include "checkpoint_journal.h"  /* journal_store_t, JOURNAL_EV_*, JOURNAL_OK */

/* Scheduler preemption/context-switch point. Extends the JOURNAL_EV_* set in
 * checkpoint_journal.h; the switch point's logical clock rides in both the
 * event lclock and value. */
#define JOURNAL_EV_SCHED 5

/* Divergence checksum for one component at the epoch boundary (#197). lclock
 * carries the component id, value carries the checksum. */
#define JOURNAL_EV_DIVERGE 6

/* Injected delta sources. The live kernel wires these to
 * scheduler_preempt_points / ktime_values / kentropy_bytes; tests pass fakes.
 * Each returns the count for the current epoch and points *out at the buffer.
 * Any pointer may be NULL, in which case that class of delta is skipped. */
typedef struct {
    uint32_t (*preempt_points)(const uint64_t** out);  /* switch-point lclocks */
    uint32_t (*time_values)(const uint64_t** out);     /* captured RDTSC reads */
    uint32_t (*entropy_bytes)(const uint8_t** out);    /* captured entropy run */
    /* Divergence checksums for the epoch boundary (#197): returns the component
     * count and points *ids / *sums at parallel arrays. NULL to skip. */
    uint32_t (*divergence_sums)(const uint32_t** ids, const uint32_t** sums);
} journal_capture_sources_t;

/* Gather the epoch's deltas from src and write them to store as a single
 * journal for `epoch`, committing crash-consistently (the journal store's
 * superblock flip is the one commit point, so a crash mid-write leaves the
 * previous epoch's journal intact). Event order within the journal: every
 * scheduler point, every time read, the entropy run, then the divergence
 * checksums. base_lclock is recorded in the slot header for reference. Returns
 * JOURNAL_OK, or a negative JOURNAL_ERR_* (in which case nothing is committed). */
int journal_capture_epoch(journal_store_t* store, uint64_t epoch,
                          uint64_t base_lclock,
                          const journal_capture_sources_t* src);

/* ---- Kernel adapter (journal_capture_sync.c) ----
 * Binds a journal store to a region of the persistence device and registers a
 * checkpoint post-commit hook (checkpoint_set_journal_hook) that journals each
 * committed epoch's live deltas via journal_capture_epoch(). Call once at boot,
 * after the checkpoint store is armed, with a non-overlapping sector region.
 * Returns JOURNAL_OK when the journal is armed, or a negative JOURNAL_ERR_*. */
int journal_capture_init(fat_block_device_t* dev, uint32_t base_sector,
                         uint32_t slot_sectors);

/* The bound journal store, or NULL if journal_capture_init has not armed it.
 * Exposed for the replay path and tests. */
journal_store_t* journal_capture_store(void);

#endif /* JOURNAL_CAPTURE_H */
