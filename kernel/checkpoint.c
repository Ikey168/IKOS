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

/* Periodic-trigger state (#117). */
static bool     g_timer_enabled = false;
static uint64_t g_timer_interval = CHECKPOINT_DEFAULT_INTERVAL_TICKS;
static uint64_t g_timer_ticks = 0;

void checkpoint_init(void) {
    g_checkpoint.current_epoch = 0;
    g_checkpoint.spaces_marked = 0;
    g_checkpoint.pages_marked = 0;
    g_checkpoint.total_takes = 0;
    g_checkpoint.epoch_open = false;
    g_timer_ticks = 0;
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

/* ----- Writeback ----- */

/* Persist still-clean snapshot-COW pages from every live user space. Such a
 * page was marked at checkpoint_take() and never written since (otherwise the
 * fault hook would have cleared its tag and captured it), so its frame still
 * holds the checkpoint-time content and can be read live now. */
static int writeback_clean_pages(snapshot_writer_t* writer) {
    uint32_t pids[PM_MAX_PROCESSES];
    uint32_t count = 0;
    if (pm_get_process_list(pids, PM_MAX_PROCESSES, &count) != 0) {
        return CHECKPOINT_OK; /* nothing to walk */
    }

    uint8_t* buf = (uint8_t*)kmalloc(PAGE_SIZE);
    if (!buf) {
        return CHECKPOINT_ERR_PARAM;
    }

    for (uint32_t i = 0; i < count; i++) {
        process_t* proc = pm_get_process(pids[i]);
        if (!proc || !proc->address_space) {
            continue;
        }
        vm_space_t* space = proc->address_space;

        for (vm_region_t* region = space->regions; region; region = region->next) {
            if (!(region->flags & VMM_FLAG_WRITE)) {
                continue;
            }
            for (uint64_t addr = region->start_addr; addr < region->end_addr;
                 addr += PAGE_SIZE) {
                pte_t* pte = vmm_get_page_table(space, addr, PT_LEVEL, false);
                if (!pte || !(*pte & PAGE_PRESENT) || !(*pte & PAGE_SNAPSHOT_COW)) {
                    continue; /* unmapped, or already captured (tag cleared) */
                }
                uint64_t phys = vmm_get_physical_addr(space, addr);
                if (!phys) {
                    continue;
                }
                /* Physical frames are directly addressable in the kernel, the
                 * same convention vmm.c uses to walk page tables. */
                memcpy(buf, (const void*)phys, PAGE_SIZE);
                if (snapshot_writer_add_page(writer, space->owner_pid, addr, 0,
                                             buf) != SNAPSHOT_OK) {
                    kfree(buf);
                    return CHECKPOINT_ERR_IO;
                }
            }
        }
    }

    kfree(buf);
    return CHECKPOINT_OK;
}

int checkpoint_writeback(snapshot_store_t* store) {
    if (!store) {
        return CHECKPOINT_ERR_PARAM;
    }

    snapshot_writer_t writer;
    if (snapshot_store_begin(store, g_checkpoint.current_epoch, &writer) != SNAPSHOT_OK) {
        return CHECKPOINT_ERR_IO;
    }

    /* 1. Modified pages: the captured pre-checkpoint images. */
    for (checkpoint_capture_t* c = g_captures; c; c = c->next) {
        if (snapshot_writer_add_page(&writer, c->pid, c->virt_addr, c->flags,
                                     c->data) != SNAPSHOT_OK) {
            return CHECKPOINT_ERR_IO;
        }
    }

    /* 2. Still-clean pages: live content (== checkpoint-time content). */
    int rc = writeback_clean_pages(&writer);
    if (rc != CHECKPOINT_OK) {
        return rc;
    }

    /* 3. Finalize: slot CRC + the superblock flip (the single commit point). */
    if (snapshot_store_commit(&writer) != SNAPSHOT_OK) {
        return CHECKPOINT_ERR_IO;
    }

    /* 4. Drop the in-memory snapshot log and close the epoch. */
    checkpoint_clear_captures();
    g_checkpoint.epoch_open = false;
    return CHECKPOINT_OK;
}

/* ----- Restore ----- */

int checkpoint_restore(snapshot_store_t* store, checkpoint_apply_fn apply, void* ctx) {
    if (!store || !apply) {
        return CHECKPOINT_ERR_PARAM;
    }

    snapshot_reader_t reader;
    int rc = snapshot_store_load(store, &reader);
    if (rc != SNAPSHOT_OK) {
        /* No valid checkpoint (or corrupt): the caller should cold-boot. */
        return CHECKPOINT_ERR_NO_CHECKPOINT;
    }

    uint8_t* buf = (uint8_t*)kmalloc(PAGE_SIZE);
    if (!buf) {
        return CHECKPOINT_ERR_PARAM;
    }

    int restored = 0;
    snapshot_page_record_t rec;
    rec.page_data = buf;
    while (snapshot_reader_next(&reader, &rec) == SNAPSHOT_OK) {
        rc = apply(ctx, &rec);
        if (rc != CHECKPOINT_OK) {
            kfree(buf);
            return rc;
        }
        restored++;
    }
    kfree(buf);

    /* Resume at the checkpoint's epoch so the next take() advances from there. */
    g_checkpoint.current_epoch = reader.epoch;
    g_checkpoint.epoch_open = false;
    return restored;
}

/* ----- Boot adapter: reconstruct address spaces from a checkpoint ----- */

/* Small pid -> restored-space table built up as pages are replayed. */
#define CHECKPOINT_RESTORE_MAX_SPACES 64
typedef struct {
    uint32_t pids[CHECKPOINT_RESTORE_MAX_SPACES];
    vm_space_t* spaces[CHECKPOINT_RESTORE_MAX_SPACES];
    int count;
} checkpoint_restore_ctx_t;

static vm_space_t* restore_space_for(checkpoint_restore_ctx_t* c, uint32_t pid) {
    for (int i = 0; i < c->count; i++) {
        if (c->pids[i] == pid) {
            return c->spaces[i];
        }
    }
    if (c->count >= CHECKPOINT_RESTORE_MAX_SPACES) {
        return 0;
    }
    vm_space_t* space = vmm_create_address_space(pid);
    if (!space) {
        return 0;
    }
    c->pids[c->count] = pid;
    c->spaces[c->count] = space;
    c->count++;
    return space;
}

static int checkpoint_restore_apply_kernel(void* ctx, const snapshot_page_record_t* rec) {
    checkpoint_restore_ctx_t* c = (checkpoint_restore_ctx_t*)ctx;

    vm_space_t* space = restore_space_for(c, rec->pid);
    if (!space) {
        return CHECKPOINT_ERR_PARAM;
    }

    uint64_t phys = vmm_alloc_page();
    if (!phys) {
        return CHECKPOINT_ERR_PARAM;
    }

    /* Physical frames are directly addressable in the kernel (same convention
     * vmm.c uses); load the checkpointed page contents into the new frame. */
    memcpy((void*)phys, rec->page_data, PAGE_SIZE);

    if (vmm_map_page(space, rec->virt_addr, phys,
                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) != 0) {
        return CHECKPOINT_ERR_PARAM;
    }

    /* Page reconstruction only. External / non-persistable handles (sockets,
     * DMA, devices) are severed per restored process via extstate_sever_all()
     * (checkpoint_extstate.h, #118); that runs when #119 reconstructs the
     * process table, since fds live on the process, not on a page. */
    return CHECKPOINT_OK;
}

int checkpoint_restore_boot(snapshot_store_t* store) {
    checkpoint_restore_ctx_t ctx;
    ctx.count = 0;
    return checkpoint_restore(store, checkpoint_restore_apply_kernel, &ctx);
}

/* ----- Boot-store registration ----- */

static snapshot_store_t* g_boot_store = 0;

void checkpoint_set_boot_store(snapshot_store_t* store) {
    g_boot_store = store;
}

int checkpoint_boot(void) {
    if (!g_boot_store) {
        return CHECKPOINT_ERR_NO_CHECKPOINT; /* nothing configured: cold boot */
    }
    return checkpoint_restore_boot(g_boot_store);
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

/* ----- Periodic trigger ----- */

void checkpoint_timer_configure(bool enabled, uint64_t interval_ticks) {
    g_timer_enabled = enabled;
    g_timer_interval = interval_ticks ? interval_ticks : 1;
    g_timer_ticks = 0;
}

bool checkpoint_tick(void) {
    if (!g_timer_enabled) {
        return false;
    }

    g_timer_ticks++;
    if (g_timer_ticks < g_timer_interval) {
        return false;
    }

    /* Interval elapsed. If the previous checkpoint is still being written back,
     * don't overlap it: hold the counter at the threshold and retry next tick
     * (so the checkpoint isn't skipped entirely, just deferred). */
    if (g_checkpoint.epoch_open) {
        return false;
    }

    g_timer_ticks = 0;
    return checkpoint_take() != 0;
}
