/* IKOS Orthogonal Persistence - Checkpoint Engine
 *
 * The checkpoint engine implements the "stop the world for ~1ms" snapshot
 * described in docs/architecture/orthogonal-persistence.md.
 *
 * checkpoint_take() walks every live user address space and marks each
 * currently-writable page read-only + PAGE_SNAPSHOT_COW, then bumps the
 * global checkpoint epoch and returns. NO page is copied during this phase;
 * pages are captured lazily by the page-fault hook (#113) on first write, and
 * streamed to disk by the background writeback pass (#115).
 *
 * Issue #112 (epic #121).
 */

#ifndef CHECKPOINT_H
#define CHECKPOINT_H

#include <stdint.h>
#include <stdbool.h>
#include "vmm.h"   /* vm_space_t, pte_t, PAGE_* flags */

/* Error codes (negative); non-negative returns are counts/epochs. */
#define CHECKPOINT_OK         0
#define CHECKPOINT_ERR_PARAM -1
#define CHECKPOINT_ERR_STATE -2

/* Engine state, observable for tests, stats, and the writeback pass. */
typedef struct {
    uint64_t current_epoch;   /* epoch of the most recent checkpoint_take() */
    uint64_t spaces_marked;   /* address spaces marked in the last take */
    uint64_t pages_marked;    /* pages flipped to snapshot-COW in the last take */
    uint64_t total_takes;     /* lifetime count of checkpoints taken */
    bool     epoch_open;      /* an epoch has been taken but not yet written back */
} checkpoint_state_t;

/* Initialize the engine (epoch 0, nothing open). */
void checkpoint_init(void);

/* The epoch of the most recent checkpoint (0 before the first take). */
uint64_t checkpoint_current_epoch(void);

/* Read-only view of engine state. */
const checkpoint_state_t* checkpoint_get_state(void);

/* Pure marking primitive — the policy core, exposed for unit testing and for
 * reuse by the page-fault hook (#113).
 *
 * If the page is present AND writable, clear PAGE_WRITABLE, set
 * PAGE_SNAPSHOT_COW, and return true (the page now faults on write).
 * Otherwise leave *pte unchanged and return false. Never copies anything. */
bool checkpoint_mark_pte(pte_t* pte);

/* Mark every currently-writable page in one address space as snapshot-COW for
 * the given epoch and record the epoch on the space. Returns the number of
 * pages marked, or a negative CHECKPOINT_ERR_* code. Copies no page. */
int checkpoint_mark_space(vm_space_t* space, uint64_t epoch);

/* Take a checkpoint: bump the epoch, mark every live user address space, and
 * return the new epoch. Returns 0 on failure. Does no disk I/O and copies no
 * page — the bounded pause is O(mapped pages walked), not O(RAM touched). */
uint64_t checkpoint_take(void);

#endif /* CHECKPOINT_H */
