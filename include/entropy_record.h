/* IKOS Orthogonal Persistence - Deterministic Entropy (#164, epic #159)
 *
 * Random bytes differ on every run, so any code that branches on them diverges
 * under replay (see the nondeterminism catalog in
 * docs/architecture/time-travel.md, #160). The catalog pinpoints the live
 * source: auth_core.c's auth_generate_random reads /dev/urandom. This module
 * wraps such a read: on a live run it draws from the real entropy source and
 * records the bytes; on replay it returns the recorded bytes instead of drawing
 * fresh randomness, so a program that branches on random values replays
 * identically.
 *
 * Byte-stream analogue of the time-read wrapper (#163): where time reads are a
 * sequence of fixed 64-bit values, entropy is an arbitrary-length byte stream,
 * so the log is a byte buffer and reads may span or split across calls.
 *
 * The core is pure and host-testable: the live entropy source is injected as a
 * function pointer. The kernel adapter (entropy_record_sync.c) registers the
 * real source and exposes the gate future callers route through.
 *
 * The recorded bytes are the per-epoch delta that pairs with the input journal
 * (#161); the replay engine (#165) persists and reloads them.
 */

#ifndef ENTROPY_RECORD_H
#define ENTROPY_RECORD_H

#include <stdint.h>

typedef enum {
    ENTROPY_REC_OFF = 0,   /* passthrough: draw from the live source, record nothing */
    ENTROPY_REC_RECORD,    /* draw from the live source and log the bytes */
    ENTROPY_REC_REPLAY     /* return logged bytes; never draw fresh randomness */
} entropy_rec_mode_t;

#define ENTROPY_REC_OK          0
#define ENTROPY_REC_ERR_PARAM  -1
#define ENTROPY_REC_ERR_SOURCE -2   /* live source failed */

/* Fills `len` bytes of `buf` from the live entropy source; returns 0 on success. */
typedef int (*entropy_source_fn)(void* ctx, void* buf, uint32_t len);

typedef struct {
    entropy_rec_mode_t mode;
    entropy_source_fn  source;   /* live entropy (OFF and RECORD) */
    void*              ctx;
    uint64_t           epoch;    /* epoch the current bytes belong to */
    uint8_t*           log;      /* recorded bytes in draw order */
    uint32_t           capacity; /* bytes in log[] */
    uint32_t           count;    /* recorded (RECORD) or loaded (REPLAY) bytes */
    uint32_t           cursor;   /* next byte to return (REPLAY) */
    uint32_t           overflow; /* bytes dropped past capacity (RECORD) */
    uint32_t           underflow;/* bytes requested past the recorded count (REPLAY) */
} entropy_record_t;

/* Bind a record to a caller-provided byte buffer and a live source. source may
 * be NULL when mode is REPLAY (the live source is never drawn from). */
int entropy_record_init(entropy_record_t* er, entropy_rec_mode_t mode,
                        entropy_source_fn source, void* ctx,
                        uint8_t* buf, uint32_t capacity);

void entropy_record_set_mode(entropy_record_t* er, entropy_rec_mode_t mode);

/* Start a new epoch: clears the recorded count and the replay cursor. */
void entropy_record_begin_epoch(entropy_record_t* er, uint64_t epoch);

/* REPLAY: install the bytes recorded for `epoch`. */
int entropy_record_load(entropy_record_t* er, uint64_t epoch,
                        const uint8_t* bytes, uint32_t n);

/* The wrapped draw. OFF: fills from the live source. RECORD: fills from the
 * live source and logs the bytes. REPLAY: fills from the log without drawing
 * fresh randomness (past the end, zero-fills and counts an underflow). Returns
 * ENTROPY_REC_OK, or ENTROPY_REC_ERR_SOURCE if the live source failed. */
int entropy_record_fill(entropy_record_t* er, void* out, uint32_t len);

/* Recorded bytes for the current epoch (RECORD), for persisting into the
 * journal. Returns the count; *out points at the buffer (may be NULL). */
uint32_t entropy_record_bytes(const entropy_record_t* er, const uint8_t** out);

/* ---- Kernel adapter (entropy_record_sync.c) ----
 * A global entropy_record. Kernel code that needs randomness (for example
 * auth_generate_random) should register its live source once via
 * kentropy_set_source() and draw through kentropy_fill(), so the bytes are
 * captured on a record run and reproduced on replay. Default mode is OFF. */
#define KENTROPY_LOG_MAX 4096
void     kentropy_init(void);
void     kentropy_set_source(entropy_source_fn source, void* ctx);
int      kentropy_fill(void* out, uint32_t len);
void     kentropy_set_mode(entropy_rec_mode_t mode);
void     kentropy_begin_epoch(uint64_t epoch);
uint32_t kentropy_bytes(const uint8_t** out);
int      kentropy_load(uint64_t epoch, const uint8_t* bytes, uint32_t n);

#endif /* ENTROPY_RECORD_H */
