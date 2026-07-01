/* IKOS Orthogonal Persistence - Deterministic Preemption (#162, epic #159)
 *
 * Preemption timing is the dominant source of replay divergence (see
 * docs/architecture/time-travel.md, #160): on a live run the scheduler is
 * preempted wherever the PIT tick happens to land in the instruction stream,
 * which varies run to run. To make replay deterministic, this module records
 * the logical point of every context switch on the live run, and on replay
 * forces switches at exactly those same points instead of at the (now absent)
 * real time-slice boundary.
 *
 * "Logical point" is a monotonic per-epoch clock supplied by the scheduler.
 * The first cut advances it once per timer tick (the same cadence the
 * checkpoint trigger already counts); a later cut can swap in a retired-
 * instruction count without changing this interface. The clock source is
 * injected, so this core is pure and host-testable, like the barrier (#138)
 * and the input journal (#161).
 *
 * The recorded points are the per-epoch delta that pairs with the input
 * journal; the replay engine (#165) persists and reloads them.
 */

#ifndef SCHED_RECORD_H
#define SCHED_RECORD_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SCHED_REC_OFF = 0,   /* passthrough: the natural policy decision stands */
    SCHED_REC_RECORD,    /* note the logical clock at each switch */
    SCHED_REC_REPLAY     /* force switches at the recorded logical clocks */
} sched_rec_mode_t;

#define SCHED_REC_OK          0
#define SCHED_REC_ERR_PARAM  -1

typedef struct {
    sched_rec_mode_t mode;
    uint64_t  epoch;      /* epoch the current points belong to */
    uint64_t* points;     /* logical clock at each switch, in recorded order */
    uint32_t  capacity;   /* slots in points[] */
    uint32_t  count;      /* recorded (RECORD) or loaded (REPLAY) switch count */
    uint32_t  cursor;     /* next unmatched point (REPLAY) */
    uint32_t  overflow;   /* switches dropped past capacity (RECORD) */
} sched_record_t;

/* Bind a record to a caller-provided points buffer and set the initial mode. */
int sched_record_init(sched_record_t* r, sched_rec_mode_t mode,
                      uint64_t* buf, uint32_t capacity);

/* Switch modes (for example OFF to RECORD when tracing begins). */
void sched_record_set_mode(sched_record_t* r, sched_rec_mode_t mode);

/* Start a new epoch: clears the recorded count and the replay cursor. The
 * scheduler calls this when checkpoint_current_epoch() advances. */
void sched_record_begin_epoch(sched_record_t* r, uint64_t epoch);

/* REPLAY: install the switch points recorded for `epoch`. */
int sched_record_load(sched_record_t* r, uint64_t epoch,
                      const uint64_t* pts, uint32_t n);

/* The single decision hook the scheduler calls each tick. `want` is the
 * policy's natural decision (for round robin: the time slice just hit zero).
 *   OFF:    returns `want`; records nothing.
 *   RECORD: if `want`, appends `lclock`; returns `want`.
 *   REPLAY: ignores `want`; returns true once `lclock` reaches the next
 *           recorded point, consuming it, so switches land at identical points.
 */
bool sched_record_decide(sched_record_t* r, bool want, uint64_t lclock);

/* Recorded switch points for the current epoch (RECORD), for persisting into
 * the journal. Returns the count; *out points at the buffer (may be NULL). */
uint32_t sched_record_points(const sched_record_t* r, const uint64_t** out);

#endif /* SCHED_RECORD_H */
