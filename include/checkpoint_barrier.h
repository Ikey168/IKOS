/* IKOS Orthogonal Persistence v2 - Checkpoint barrier (quiescent point)
 *
 * A whole-machine checkpoint can only be taken when the kernel is
 * self-consistent: no CPU holds a lock and no syscall is mid-flight. This is
 * the quiescent-point state machine from
 * docs/architecture/orthogonal-persistence-v2.md (#138).
 *
 * Lifecycle:  IDLE --arm--> ARMED --(all CPUs park)--> QUIESCENT --release--> IDLE
 *
 * The periodic trigger ARMS the barrier (it does not take a checkpoint inline).
 * Each CPU, on reaching a safe boundary (about to return to user mode, or in
 * the idle loop) with no lock held, parks. When every CPU is parked the system
 * is QUIESCENT and the checkpoint may be taken; release() then resumes the CPUs.
 *
 * On single-CPU IKOS this collapses to "arm, park the one CPU, take, release"
 * within a single tick, but the state machine is written for SMP.
 *
 * Pure and dependency-free: host-testable with no kernel stubs.
 */

#ifndef CHECKPOINT_BARRIER_H
#define CHECKPOINT_BARRIER_H

#include <stdint.h>
#include <stdbool.h>

#define CHECKPOINT_BARRIER_MAX_CPUS 8

typedef enum {
    CHECKPOINT_BARRIER_IDLE = 0,  /* no checkpoint pending */
    CHECKPOINT_BARRIER_ARMED,     /* requested; CPUs converging on the barrier */
    CHECKPOINT_BARRIER_QUIESCENT, /* all CPUs parked, no lock held; safe to take */
} checkpoint_barrier_state_t;

typedef struct {
    checkpoint_barrier_state_t state;
    uint32_t ncpus;        /* participating CPUs (1 on single-CPU IKOS) */
    uint32_t parked;       /* how many are currently parked */
    bool     is_parked[CHECKPOINT_BARRIER_MAX_CPUS];
} checkpoint_barrier_t;

/* Initialize for `ncpus` CPUs (clamped to [1, MAX_CPUS]); state IDLE. */
void checkpoint_barrier_init(checkpoint_barrier_t* b, uint32_t ncpus);

/* Request a checkpoint. IDLE -> ARMED, returns true (newly armed). If a
 * checkpoint is already in progress (ARMED/QUIESCENT) this is a no-op and
 * returns false. */
bool checkpoint_barrier_arm(checkpoint_barrier_t* b);

/* A CPU reports it has reached a safe boundary. `can_park` is false if the CPU
 * still holds a lock / is mid-syscall and cannot park yet. Only meaningful while
 * ARMED. Returns true if THIS call made the system QUIESCENT (the last CPU
 * parked) - the caller should then take the checkpoint and release(). Parking
 * the same CPU twice is idempotent. */
bool checkpoint_barrier_park(checkpoint_barrier_t* b, uint32_t cpu, bool can_park);

/* A parked CPU has to back out (e.g. it must take a lock): unpark it, dropping
 * back from QUIESCENT to ARMED if necessary. */
void checkpoint_barrier_unpark(checkpoint_barrier_t* b, uint32_t cpu);

/* True once every CPU is parked and it is safe to take the checkpoint. */
bool checkpoint_barrier_is_quiescent(const checkpoint_barrier_t* b);

/* Current state. */
checkpoint_barrier_state_t checkpoint_barrier_state(const checkpoint_barrier_t* b);

/* Checkpoint taken: resume all CPUs. Returns to IDLE, ready to be armed again. */
void checkpoint_barrier_release(checkpoint_barrier_t* b);

#endif /* CHECKPOINT_BARRIER_H */
