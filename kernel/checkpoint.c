/* IKOS Orthogonal Persistence - Checkpoint Engine
 *
 * See include/checkpoint.h and
 * docs/architecture/orthogonal-persistence.md. Issue #112 (epic #121).
 */

#include "checkpoint.h"
#include "process_manager.h"
#include <stddef.h>

/* Freestanding helpers provided by the kernel. */
extern void* kmalloc(size_t size);
extern void  kfree(void* ptr);
extern void* memcpy(void* dest, const void* src, size_t size);

/* Engine state. */
static checkpoint_state_t g_checkpoint = {0};

/* In-memory capture list (the "snapshot log" drained by writeback, #115). */
static checkpoint_capture_t* g_captures = 0;
static uint64_t g_capture_count = 0;

void checkpoint_init(void) {
    g_checkpoint.current_epoch = 0;
    g_checkpoint.spaces_marked = 0;
    g_checkpoint.pages_marked = 0;
    g_checkpoint.total_takes = 0;
    g_checkpoint.epoch_open = false;
    checkpoint_clear_captures();
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

bool checkpoint_resolve_pte(pte_t* pte) {
    if (!pte) {
        return false;
    }
    pte_t entry = *pte;
    if (!(entry & PAGE_SNAPSHOT_COW)) {
        return false;
    }
    /* Restore write permission and drop the snapshot tag: the pre-write image
     * has been captured, so the page may be written normally again. */
    entry &= ~(pte_t)PAGE_SNAPSHOT_COW;
    entry |= PAGE_WRITABLE;
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

/* ----- Capture (page-fault hook) ----- */

checkpoint_capture_t* checkpoint_captures(void) {
    return g_captures;
}

uint64_t checkpoint_capture_count(void) {
    return g_capture_count;
}

void checkpoint_clear_captures(void) {
    checkpoint_capture_t* c = g_captures;
    while (c) {
        checkpoint_capture_t* next = c->next;
        if (c->data) {
            kfree(c->data);
        }
        kfree(c);
        c = next;
    }
    g_captures = 0;
    g_capture_count = 0;
}

int checkpoint_capture_page(uint32_t pid, uint64_t virt_addr,
                            const void* page_contents, pte_t* pte) {
    if (!pte || !page_contents) {
        return CHECKPOINT_ERR_PARAM;
    }
    if (!(*pte & PAGE_SNAPSHOT_COW)) {
        /* Not a snapshot page: nothing to preserve. */
        return CHECKPOINT_ERR_STATE;
    }

    checkpoint_capture_t* rec =
        (checkpoint_capture_t*)kmalloc(sizeof(checkpoint_capture_t));
    if (!rec) {
        return CHECKPOINT_ERR_PARAM;
    }
    rec->data = kmalloc(PAGE_SIZE);
    if (!rec->data) {
        kfree(rec);
        return CHECKPOINT_ERR_PARAM;
    }

    /* Preserve the pre-write contents, then make the page writable again. */
    memcpy(rec->data, page_contents, PAGE_SIZE);
    rec->epoch = g_checkpoint.current_epoch;
    rec->pid = pid;
    rec->flags = 0;
    rec->virt_addr = virt_addr;
    rec->next = g_captures;
    g_captures = rec;
    g_capture_count++;

    checkpoint_resolve_pte(pte);
    return CHECKPOINT_OK;
}

bool checkpoint_handle_write_fault(vm_space_t* space, uint64_t fault_addr) {
    if (!space) {
        return false;
    }
    uint64_t page_addr = fault_addr & ~((uint64_t)PAGE_SIZE - 1);

    pte_t* pte = vmm_get_page_table(space, page_addr, PT_LEVEL, false);
    if (!pte || !(*pte & PAGE_PRESENT) || !(*pte & PAGE_SNAPSHOT_COW)) {
        return false; /* not a snapshot-COW fault */
    }

    /* The faulting page is mapped at page_addr in the current address space,
     * so its current contents are readable there. */
    int rc = checkpoint_capture_page(space->owner_pid, page_addr,
                                     (const void*)page_addr, pte);
    if (rc != CHECKPOINT_OK) {
        return false;
    }

    vmm_flush_tlb_page(page_addr);
    return true;
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
