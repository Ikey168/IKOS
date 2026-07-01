/* IKOS Orthogonal Persistence v2 - Boot restore ordering (#147)
 *
 * The v2 integration capstone. On boot, when a valid checkpoint exists, the
 * whole machine must come back in a defined order (orthogonal-persistence-v2.md
 * section 7):
 *   1. restore kernel state (process table + scheduler #140, VFS #141, IPC #142,
 *      reassembled from the kernel-state records #139);
 *   2. re-attach drivers (#143 framework; disk #144, framebuffer #145);
 *   3. restore user address spaces and processes on top (the v1 path), now with
 *      live kernel state, applying external-state severing (#146).
 *
 * If there is no checkpoint we cold-boot cleanly; if kernel-state restore or a
 * critical driver re-attach fails we fall back to cold boot (section 8). That
 * ordering + fallback decision is the whole point of this module, so it is a
 * pure, host-testable sequencer driven by injected step callbacks. The kernel
 * adapter (checkpoint_boot_v2.c) binds the real subsystems to the steps.
 *
 * This header also carries the kernel-state blob reassembly helpers: capture
 * (#139) chunks a blob into page records tagged (size << 32 | index); restore
 * has to put the chunks back together, and that offset math is pure and worth
 * testing on its own.
 */

#ifndef CHECKPOINT_RESTORE_SEQ_H
#define CHECKPOINT_RESTORE_SEQ_H

#include <stdint.h>

/* ----- Restore ordering / fallback sequencer ----- */

typedef enum {
    CHECKPOINT_RESTORE_COLD_BOOT = 0, /* no checkpoint, or a fallback after failure */
    CHECKPOINT_RESTORE_RESUMED   = 1, /* the whole machine came back */
} checkpoint_restore_outcome_t;

/* The furthest phase the sequence entered (for diagnostics / the fallback). */
typedef enum {
    CHECKPOINT_PHASE_NONE = 0,
    CHECKPOINT_PHASE_KERNEL_STATE,
    CHECKPOINT_PHASE_DRIVER_REATTACH,
    CHECKPOINT_PHASE_USER_RESTORE,
    CHECKPOINT_PHASE_DONE,
} checkpoint_restore_phase_t;

/* Injected restore steps. has_checkpoint returns nonzero if a valid checkpoint
 * exists; the three restore steps return 0 on success and nonzero on a failure
 * that forces a cold-boot fallback. The driver step folds its own
 * continue-vs-abort policy (#143) into that 0/nonzero result. */
typedef struct {
    int (*has_checkpoint)(void* ctx);
    int (*restore_kernel_state)(void* ctx);
    int (*reattach_drivers)(void* ctx);
    int (*restore_user)(void* ctx);
    void* ctx;
} checkpoint_restore_steps_t;

typedef struct {
    checkpoint_restore_outcome_t outcome;
    checkpoint_restore_phase_t   reached;   /* last phase entered */
    int                          fell_back; /* nonzero if a failure forced cold boot */
} checkpoint_restore_result_t;

/* Run the restore in the defined order with cold-boot fallback. A missing step
 * callback (or missing steps struct) is treated as a fatal misconfiguration and
 * cold-boots. No checkpoint is a clean cold boot (fell_back == 0). */
checkpoint_restore_result_t
checkpoint_restore_sequence(const checkpoint_restore_steps_t* steps);

/* ----- Kernel-state blob reassembly (#139 chunk format) ----- */

/* A kernel-blob chunk record encodes its total blob size and chunk index in the
 * record's virt_addr field as (size << 32) | index. */
static inline uint32_t checkpoint_blob_total(uint64_t virt_addr) {
    return (uint32_t)(virt_addr >> 32);
}
static inline uint32_t checkpoint_blob_index(uint64_t virt_addr) {
    return (uint32_t)(virt_addr & 0xFFFFFFFFu);
}

#define CHECKPOINT_BLOB_CHUNK 4096  /* bytes per chunk record (= PAGE_SIZE) */

#define CHECKPOINT_BLOB_OK        0
#define CHECKPOINT_BLOB_ERR      -1  /* bad args / index or size out of range */
#define CHECKPOINT_BLOB_COMPLETE  1  /* this chunk completed the blob */

/* Copy one chunk into its place in a reassembly buffer. `index` is the chunk's
 * position, `total` the full blob size (both from the record's virt_addr).
 * `chunk` holds up to CHECKPOINT_BLOB_CHUNK valid bytes; only the bytes that
 * belong to this index (total - index*CHUNK, capped at CHUNK) are copied.
 * *received accumulates the bytes placed so far. Returns CHECKPOINT_BLOB_COMPLETE
 * once *received reaches total, CHECKPOINT_BLOB_OK while still in progress, or
 * CHECKPOINT_BLOB_ERR on bad input or if the blob would overflow `cap`. */
int checkpoint_blob_place(uint8_t* buf, uint32_t cap, uint32_t total,
                          uint32_t index, const void* chunk, uint32_t* received);

#endif /* CHECKPOINT_RESTORE_SEQ_H */
