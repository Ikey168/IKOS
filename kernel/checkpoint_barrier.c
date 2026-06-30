/* IKOS Orthogonal Persistence v2 - Checkpoint barrier (quiescent point)
 *
 * See include/checkpoint_barrier.h. Pure state machine, no kernel deps.
 * Issue #138.
 */

#include "checkpoint_barrier.h"

void checkpoint_barrier_init(checkpoint_barrier_t* b, uint32_t ncpus) {
    if (!b) {
        return;
    }
    if (ncpus < 1) {
        ncpus = 1;
    }
    if (ncpus > CHECKPOINT_BARRIER_MAX_CPUS) {
        ncpus = CHECKPOINT_BARRIER_MAX_CPUS;
    }
    b->state = CHECKPOINT_BARRIER_IDLE;
    b->ncpus = ncpus;
    b->parked = 0;
    for (uint32_t i = 0; i < CHECKPOINT_BARRIER_MAX_CPUS; i++) {
        b->is_parked[i] = false;
    }
}

bool checkpoint_barrier_arm(checkpoint_barrier_t* b) {
    if (!b || b->state != CHECKPOINT_BARRIER_IDLE) {
        return false; /* already in progress */
    }
    b->state = CHECKPOINT_BARRIER_ARMED;
    b->parked = 0;
    for (uint32_t i = 0; i < CHECKPOINT_BARRIER_MAX_CPUS; i++) {
        b->is_parked[i] = false;
    }
    return true;
}

bool checkpoint_barrier_park(checkpoint_barrier_t* b, uint32_t cpu, bool can_park) {
    if (!b || cpu >= b->ncpus || b->state != CHECKPOINT_BARRIER_ARMED) {
        return false;
    }
    if (!can_park) {
        return false; /* CPU still holds a lock / is mid-syscall */
    }
    if (b->is_parked[cpu]) {
        return false; /* idempotent: already counted */
    }

    b->is_parked[cpu] = true;
    b->parked++;

    if (b->parked >= b->ncpus) {
        b->state = CHECKPOINT_BARRIER_QUIESCENT;
        return true; /* this call achieved quiescence */
    }
    return false;
}

void checkpoint_barrier_unpark(checkpoint_barrier_t* b, uint32_t cpu) {
    if (!b || cpu >= b->ncpus) {
        return;
    }
    if (b->state == CHECKPOINT_BARRIER_IDLE || !b->is_parked[cpu]) {
        return;
    }
    b->is_parked[cpu] = false;
    if (b->parked > 0) {
        b->parked--;
    }
    /* Lost quiescence: a CPU is running again, so fall back to ARMED. */
    if (b->state == CHECKPOINT_BARRIER_QUIESCENT) {
        b->state = CHECKPOINT_BARRIER_ARMED;
    }
}

bool checkpoint_barrier_is_quiescent(const checkpoint_barrier_t* b) {
    return b && b->state == CHECKPOINT_BARRIER_QUIESCENT;
}

checkpoint_barrier_state_t checkpoint_barrier_state(const checkpoint_barrier_t* b) {
    return b ? b->state : CHECKPOINT_BARRIER_IDLE;
}

void checkpoint_barrier_release(checkpoint_barrier_t* b) {
    if (!b) {
        return;
    }
    b->state = CHECKPOINT_BARRIER_IDLE;
    b->parked = 0;
    for (uint32_t i = 0; i < CHECKPOINT_BARRIER_MAX_CPUS; i++) {
        b->is_parked[i] = false;
    }
}
