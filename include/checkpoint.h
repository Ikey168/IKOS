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

/* Inverse of checkpoint_mark_pte(): clear PAGE_SNAPSHOT_COW and restore
 * PAGE_WRITABLE so the page is writable again. Returns true if the page was a
 * snapshot-COW page (and was thus resolved), false otherwise. Pure; exposed
 * for unit testing. */
bool checkpoint_resolve_pte(pte_t* pte);

/* ----- Captured pages (the in-memory "snapshot log") -----
 *
 * The page-fault hook copies a page's pre-write contents into a capture record
 * before un-protecting it. The background writeback pass (#115) drains these
 * records to the on-disk snapshot store, then calls checkpoint_clear_captures(). */
typedef struct checkpoint_capture {
    uint64_t epoch;       /* checkpoint epoch this page belongs to */
    uint32_t pid;         /* owning process */
    uint32_t flags;       /* reserved / region flags */
    uint64_t virt_addr;   /* page-aligned virtual address */
    void*    data;        /* PAGE_SIZE copy of the pre-write contents */
    struct checkpoint_capture* next;
} checkpoint_capture_t;

/* Preserve a snapshot-COW page: copy page_contents into a new capture record
 * tagged with the current epoch, then resolve *pte back to writable. Returns
 * CHECKPOINT_OK on capture, CHECKPOINT_ERR_STATE if the PTE is not snapshot-COW,
 * or CHECKPOINT_ERR_PARAM / a negative code on bad input or allocation failure.
 * Exposed for unit testing; normally reached via checkpoint_handle_write_fault(). */
int checkpoint_capture_page(uint32_t pid, uint64_t virt_addr,
                            const void* page_contents, pte_t* pte);

/* Page-fault hook entry point. If fault_addr in `space` is a present,
 * snapshot-COW page, capture its current contents, restore writability, flush
 * the TLB, and return true (the faulting write may now proceed). Returns false
 * if the page is not a snapshot-COW page (the fault is someone else's). */
bool checkpoint_handle_write_fault(vm_space_t* space, uint64_t fault_addr);

/* Head of the capture list (most-recent-first), its length, and a routine to
 * free every capture record. Used by the writeback pass (#115). */
checkpoint_capture_t* checkpoint_captures(void);
uint64_t checkpoint_capture_count(void);
void checkpoint_clear_captures(void);

/* Take a checkpoint: bump the epoch, mark every live user address space, and
 * return the new epoch. Returns 0 on failure. Does no disk I/O and copies no
 * page — the bounded pause is O(mapped pages walked), not O(RAM touched). */
uint64_t checkpoint_take(void);

#endif /* CHECKPOINT_H */
