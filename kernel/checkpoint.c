/* IKOS Orthogonal Persistence - Checkpoint Engine
 *
 * See include/checkpoint.h and
 * docs/architecture/orthogonal-persistence.md. Issue #112 (epic #121).
 */

#include "checkpoint.h"
#include "process_manager.h"

/* Engine state. */
static checkpoint_state_t g_checkpoint = {0};

void checkpoint_init(void) {
    g_checkpoint.current_epoch = 0;
    g_checkpoint.spaces_marked = 0;
    g_checkpoint.pages_marked = 0;
    g_checkpoint.total_takes = 0;
    g_checkpoint.epoch_open = false;
}

uint64_t checkpoint_current_epoch(void) {
    return g_checkpoint.current_epoch;
}

const checkpoint_state_t* checkpoint_get_state(void) {
    return &g_checkpoint;
}

/* ----- Pure marking primitive (the policy core) ----- */

bool checkpoint_mark_pte(pte_t* pte) {
    if (!pte) {
        return false;
    }
    pte_t entry = *pte;

    /* Only currently-writable, present pages need protecting: a read-only
     * page cannot change, so there is nothing to copy on a later write. */
    if (!(entry & PAGE_PRESENT) || !(entry & PAGE_WRITABLE)) {
        return false;
    }

    /* Drop write permission and tag as snapshot copy-on-write. The next write
     * faults; the page-fault hook (#113) preserves the page before allowing
     * the write to proceed. No copy happens here. */
    entry &= ~(pte_t)PAGE_WRITABLE;
    entry |= PAGE_SNAPSHOT_COW;
    *pte = entry;
    return true;
}

/* ----- Per-space walk ----- */

int checkpoint_mark_space(vm_space_t* space, uint64_t epoch) {
    if (!space) {
        return CHECKPOINT_ERR_PARAM;
    }

    int marked = 0;
    vm_space_t* current = vmm_get_current_space();

    /* Walk the space's regions; only writable regions can hold writable
     * pages worth protecting. */
    for (vm_region_t* region = space->regions; region; region = region->next) {
        if (!(region->flags & VMM_FLAG_WRITE)) {
            continue;
        }

        for (uint64_t addr = region->start_addr; addr < region->end_addr;
             addr += PAGE_SIZE) {
            pte_t* pte = vmm_get_page_table(space, addr, PT_LEVEL, false);
            if (!pte) {
                continue; /* not mapped */
            }
            if (checkpoint_mark_pte(pte)) {
                marked++;
                /* Only the active address space has live TLB entries to
                 * invalidate; others are flushed on their next CR3 load. */
                if (space == current) {
                    vmm_flush_tlb_page(addr);
                }
            }
        }
    }

    space->checkpoint_epoch = epoch;
    return marked;
}

/* ----- Take ----- */

uint64_t checkpoint_take(void) {
    uint64_t epoch = g_checkpoint.current_epoch + 1;
    uint64_t spaces = 0;
    uint64_t pages = 0;

    /* Enumerate live user processes and mark each one's address space. The
     * kernel space (pid 0) is out of scope for v1 persistence. */
    uint32_t pids[PM_MAX_PROCESSES];
    uint32_t count = 0;
    if (pm_get_process_list(pids, PM_MAX_PROCESSES, &count) != 0) {
        return 0;
    }

    for (uint32_t i = 0; i < count; i++) {
        process_t* proc = pm_get_process(pids[i]);
        if (!proc || !proc->address_space) {
            continue;
        }
        int marked = checkpoint_mark_space(proc->address_space, epoch);
        if (marked < 0) {
            continue;
        }
        spaces++;
        pages += (uint64_t)marked;
    }

    g_checkpoint.current_epoch = epoch;
    g_checkpoint.spaces_marked = spaces;
    g_checkpoint.pages_marked = pages;
    g_checkpoint.total_takes++;
    g_checkpoint.epoch_open = true;
    return epoch;
}
