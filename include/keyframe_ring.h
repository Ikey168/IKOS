/* IKOS Orthogonal Persistence - Keyframe Retention Ring (#168, epic #159)
 *
 * The snapshot store keeps two double-buffered slots: the latest checkpoint and
 * the previous one. That is enough to resume on boot, but time-travel needs to
 * rewind to any of the last N moments (#169), not just the most recent. This
 * ring tracks the last N checkpoint keyframes: each new checkpoint claims the
 * next slot, reclaiming the oldest when the ring is full, and a lookup finds the
 * nearest retained keyframe at or before a target epoch (the starting point a
 * rewind restores from, then replays forward).
 *
 * This core is the ring index only: which slots hold which epochs, plus a
 * CRC-protected pack/unpack so the index can live in a superblock sector. The
 * per-slot page data is stored by the snapshot store; a slot here maps to a
 * store region. The core is pure and host-testable, like the barrier (#138).
 *
 * Disk-cost trade-off (AC): the retention window in wall-clock time is about
 * N * checkpoint_interval, and the disk cost is about N * checkpoint_size in the
 * worst case (less if slots share unchanged pages). Larger N buys deeper rewind
 * at linear disk cost; a shorter interval buys finer rewind granularity at more
 * frequent writeback. Pick N and the interval for the rewind depth and
 * granularity the deployment needs against its disk budget.
 *
 * See docs/architecture/time-travel.md.
 */

#ifndef KEYFRAME_RING_H
#define KEYFRAME_RING_H

#include <stdint.h>
#include <stdbool.h>

#define KEYFRAME_RING_MAX   64   /* upper bound on retained keyframes */

#define KEYFRAME_RING_OK        0
#define KEYFRAME_RING_ERR_PARAM -1
#define KEYFRAME_RING_ERR_CRC   -2   /* unpack: checksum mismatch */
#define KEYFRAME_RING_ERR_SIZE  -3   /* pack/unpack: buffer too small / bad */

typedef struct {
    uint64_t epoch;   /* epoch stored in this slot */
    uint32_t valid;   /* 1 if the slot holds a retained keyframe */
    uint32_t reserved;
} keyframe_slot_t;

typedef struct {
    uint32_t capacity;         /* N: retained keyframes (1..KEYFRAME_RING_MAX) */
    uint32_t count;            /* currently retained (<= capacity) */
    uint32_t head;             /* index the next keyframe will claim */
    uint32_t reserved;
    /* Bookkeeping for the most recent reclaim (for logging what fell off). */
    uint32_t reclaimed_valid;  /* 1 if the last keyframe_ring_advance evicted one */
    uint32_t reserved2;
    uint64_t reclaimed_epoch;  /* the evicted epoch, when reclaimed_valid */
    keyframe_slot_t slots[KEYFRAME_RING_MAX];
} keyframe_ring_t;

/* Initialize an empty ring retaining `capacity` keyframes. */
int keyframe_ring_init(keyframe_ring_t* r, uint32_t capacity);

/* Claim the next slot for a checkpoint at `epoch`, reclaiming the oldest slot
 * when the ring is full. Returns the slot index the checkpoint should use.
 * After the call, keyframe_ring_reclaimed() reports whether (and which) old
 * keyframe was evicted. */
uint32_t keyframe_ring_advance(keyframe_ring_t* r, uint64_t epoch);

/* Find the nearest retained keyframe at or before `target`. On success returns
 * true and fills *slot_out / *epoch_out (either may be NULL). Returns false if
 * `target` predates the oldest retained keyframe (outside the rewind horizon). */
bool keyframe_ring_find(const keyframe_ring_t* r, uint64_t target,
                        uint32_t* slot_out, uint64_t* epoch_out);

/* Oldest / newest retained epochs (the rewind horizon and the latest keyframe).
 * Return false if the ring is empty. */
bool keyframe_ring_oldest(const keyframe_ring_t* r, uint64_t* epoch_out);
bool keyframe_ring_newest(const keyframe_ring_t* r, uint64_t* epoch_out);

/* Whether the last advance() reclaimed a keyframe, and which epoch. */
bool keyframe_ring_reclaimed(const keyframe_ring_t* r, uint64_t* epoch_out);

uint32_t keyframe_ring_count(const keyframe_ring_t* r);

/* Serialize the ring index into `buf` with a trailing CRC32, for persisting to
 * a superblock sector. Returns the number of bytes written, or a negative error
 * if the buffer is too small. keyframe_ring_packed_size() gives the size. */
int keyframe_ring_pack(const keyframe_ring_t* r, void* buf, uint32_t buflen);
uint32_t keyframe_ring_packed_size(void);

/* Restore a ring index from a packed buffer, validating the CRC. */
int keyframe_ring_unpack(keyframe_ring_t* r, const void* buf, uint32_t buflen);

/* CRC32 (IEEE 802.3, reflected, poly 0xEDB88320), seed 0. Exposed for tests. */
uint32_t keyframe_ring_crc32(uint32_t crc, const void* data, uint32_t len);

#endif /* KEYFRAME_RING_H */
