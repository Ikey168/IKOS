/* IKOS Orthogonal Persistence - Virtualized Time Reads (#163, epic #159)
 *
 * Time and cycle reads (RDTSC, timer counters) return a different value on
 * every run, so any code that branches on them diverges under replay (see the
 * nondeterminism catalog in docs/architecture/time-travel.md, #160). This
 * module wraps such a read: on a live run it reads the real clock and records
 * the value; on replay it returns the recorded value instead of touching
 * hardware, so a program that branches on the clock replays identically.
 *
 * The catalog notes that IKOS has no live RDTSC read yet (numa_allocator.c's
 * get_rdtsc is a stub returning 0), so this lands the gate first: future time
 * reads go through time_record_read()/ktime_read() rather than a raw RDTSC.
 *
 * The core is pure and host-testable: the live clock is injected as a function
 * pointer, so tests drive it with a mock source. The kernel adapter
 * (time_record_sync.c) supplies the real x86 RDTSC source.
 *
 * The recorded values are the per-epoch delta that pairs with the input journal
 * (#161); the replay engine (#165) persists and reloads them.
 */

#ifndef TIME_RECORD_H
#define TIME_RECORD_H

#include <stdint.h>

typedef enum {
    TIME_REC_OFF = 0,   /* passthrough: read the live clock, record nothing */
    TIME_REC_RECORD,    /* read the live clock and log each value */
    TIME_REC_REPLAY     /* return logged values; never touch hardware */
} time_rec_mode_t;

#define TIME_REC_OK          0
#define TIME_REC_ERR_PARAM  -1

/* Reads the live hardware clock (RDTSC or a timer counter). */
typedef uint64_t (*time_source_fn)(void* ctx);

typedef struct {
    time_rec_mode_t mode;
    time_source_fn  source;   /* live clock read (OFF and RECORD) */
    void*           ctx;
    uint64_t        epoch;    /* epoch the current values belong to */
    uint64_t*       log;      /* recorded values in read order */
    uint32_t        capacity; /* slots in log[] */
    uint32_t        count;    /* recorded (RECORD) or loaded (REPLAY) count */
    uint32_t        cursor;   /* next value to return (REPLAY) */
    uint32_t        overflow; /* reads past capacity (RECORD) */
    uint32_t        underflow;/* reads past the recorded count (REPLAY) */
    uint64_t        last;     /* last value returned */
} time_record_t;

/* Bind a record to a caller-provided value buffer and a live clock source.
 * source may be NULL when mode is REPLAY (hardware is never read). */
int time_record_init(time_record_t* tr, time_rec_mode_t mode,
                     time_source_fn source, void* ctx,
                     uint64_t* buf, uint32_t capacity);

void time_record_set_mode(time_record_t* tr, time_rec_mode_t mode);

/* Start a new epoch: clears the recorded count and the replay cursor. */
void time_record_begin_epoch(time_record_t* tr, uint64_t epoch);

/* REPLAY: install the values recorded for `epoch`. */
int time_record_load(time_record_t* tr, uint64_t epoch,
                     const uint64_t* vals, uint32_t n);

/* The wrapped read. OFF: returns the live clock. RECORD: reads the live clock,
 * logs it, and returns it. REPLAY: returns the next logged value without
 * touching hardware (past the end, returns the last value and counts an
 * underflow). */
uint64_t time_record_read(time_record_t* tr);

/* Recorded values for the current epoch (RECORD), for persisting into the
 * journal. Returns the count; *out points at the buffer (may be NULL). */
uint32_t time_record_values(const time_record_t* tr, const uint64_t** out);

/* ---- Kernel adapter (time_record_sync.c) ----
 * A global time_record backed by the real x86 RDTSC. Kernel code that needs a
 * cycle/time value should call ktime_read() so the read is captured for replay.
 * Default mode is OFF, so this reads live hardware until a record/replay run
 * enables it. */
#define KTIME_LOG_MAX 4096
void     ktime_init(void);
uint64_t ktime_read(void);
void     ktime_set_mode(time_rec_mode_t mode);
void     ktime_begin_epoch(uint64_t epoch);
uint32_t ktime_values(const uint64_t** out);
int      ktime_load(uint64_t epoch, const uint64_t* vals, uint32_t n);

#endif /* TIME_RECORD_H */
