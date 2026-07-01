/* IKOS Orthogonal Persistence v2 - Boot restore ordering (#147)
 *
 * Pure sequencer + kernel-blob reassembly math. No kernel deps. See
 * include/checkpoint_restore_seq.h.
 */

#include "checkpoint_restore_seq.h"

static checkpoint_restore_result_t cold(checkpoint_restore_phase_t reached, int fell_back) {
    checkpoint_restore_result_t r;
    r.outcome   = CHECKPOINT_RESTORE_COLD_BOOT;
    r.reached   = reached;
    r.fell_back = fell_back;
    return r;
}

checkpoint_restore_result_t
checkpoint_restore_sequence(const checkpoint_restore_steps_t* steps) {
    /* A missing wiring is a fatal misconfiguration: cold-boot rather than resume
     * a machine we cannot fully bring back. */
    if (!steps || !steps->has_checkpoint || !steps->restore_kernel_state ||
        !steps->reattach_drivers || !steps->restore_user) {
        return cold(CHECKPOINT_PHASE_NONE, 1);
    }

    void* ctx = steps->ctx;

    /* No checkpoint: a clean cold boot, not a fallback. */
    if (!steps->has_checkpoint(ctx)) {
        return cold(CHECKPOINT_PHASE_NONE, 0);
    }

    /* 1. Kernel state must come back before anything that depends on it. */
    if (steps->restore_kernel_state(ctx) != 0) {
        return cold(CHECKPOINT_PHASE_KERNEL_STATE, 1);
    }

    /* 2. Re-attach drivers on top of live kernel state. The step decides whether
     * a re-attach failure is critical (abort) or tolerable (device severed). */
    if (steps->reattach_drivers(ctx) != 0) {
        return cold(CHECKPOINT_PHASE_DRIVER_REATTACH, 1);
    }

    /* 3. Restore user address spaces / processes last, with severing applied. */
    if (steps->restore_user(ctx) != 0) {
        return cold(CHECKPOINT_PHASE_USER_RESTORE, 1);
    }

    checkpoint_restore_result_t r;
    r.outcome   = CHECKPOINT_RESTORE_RESUMED;
    r.reached   = CHECKPOINT_PHASE_DONE;
    r.fell_back = 0;
    return r;
}

/* ----- Kernel-state blob reassembly ----- */

int checkpoint_blob_place(uint8_t* buf, uint32_t cap, uint32_t total,
                          uint32_t index, const void* chunk, uint32_t* received) {
    if (!buf || !chunk || !received || total == 0 || total > cap) {
        return CHECKPOINT_BLOB_ERR;
    }

    uint32_t off = index * CHECKPOINT_BLOB_CHUNK;
    if (off >= total) {
        return CHECKPOINT_BLOB_ERR; /* chunk index past the end of the blob */
    }

    uint32_t n = total - off;
    if (n > CHECKPOINT_BLOB_CHUNK) {
        n = CHECKPOINT_BLOB_CHUNK;
    }

    const uint8_t* src = (const uint8_t*)chunk;
    for (uint32_t i = 0; i < n; i++) {
        buf[off + i] = src[i];
    }
    *received += n;

    return (*received >= total) ? CHECKPOINT_BLOB_COMPLETE : CHECKPOINT_BLOB_OK;
}
