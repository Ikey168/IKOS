/* IKOS Orthogonal Persistence v2 - Boot restore adapter (#147)
 *
 * Binds the real subsystems to the pure restore sequencer
 * (checkpoint_restore_seq.c) so a whole-machine checkpoint comes back in the
 * defined order: kernel state, then driver re-attach, then user restore, with a
 * cold-boot fallback if a critical step fails (orthogonal-persistence-v2.md
 * sections 7 and 8). Compile-checked freestanding; the live restore is a QEMU
 * integration test.
 *
 * kernel_init() calls checkpoint_boot_v2(store) in place of the v1
 * checkpoint_boot(): it returns CHECKPOINT_OK when the machine resumed, or
 * CHECKPOINT_ERR_NO_CHECKPOINT to tell the caller to continue a cold boot.
 */

#include "checkpoint_restore_seq.h"
#include "checkpoint.h"
#include "checkpoint_driver.h"
#include "checkpoint_proctable.h"
#include "checkpoint_filetable.h"
#include "checkpoint_ipc.h"
#include "checkpoint_fb.h"
#include "snapshot_store.h"

extern void* kmalloc(size_t size);
extern void  kfree(void* ptr);

/* One kernel-state blob under reassembly, keyed by its tag (rec->pid). */
typedef struct {
    uint32_t tag;
    uint8_t* buf;
    uint32_t total;
    uint32_t received;
    int      in_use;
    int      complete;
} blob_slot_t;

#define BOOT_V2_MAX_BLOBS 6

typedef struct {
    snapshot_store_t* store;
} boot_v2_ctx_t;

static blob_slot_t* slot_for_tag(blob_slot_t* slots, uint32_t tag, uint32_t total) {
    for (int i = 0; i < BOOT_V2_MAX_BLOBS; i++) {
        if (slots[i].in_use && slots[i].tag == tag) {
            return &slots[i];
        }
    }
    for (int i = 0; i < BOOT_V2_MAX_BLOBS; i++) {
        if (!slots[i].in_use) {
            slots[i].tag = tag;
            slots[i].buf = (uint8_t*)kmalloc(total ? total : 1);
            slots[i].total = total;
            slots[i].received = 0;
            slots[i].in_use = slots[i].buf ? 1 : 0;
            slots[i].complete = 0;
            return slots[i].buf ? &slots[i] : 0;
        }
    }
    return 0; /* more distinct blobs than expected */
}

/* Dispatch a fully reassembled blob to the owning subsystem's restorer. */
static int dispatch_blob(blob_slot_t* s) {
    switch (s->tag) {
        case CHECKPOINT_TAG_PROCTABLE:        return checkpoint_restore_proctable(s->buf, s->total);
        case CHECKPOINT_TAG_FILETABLE:        return checkpoint_restore_filetable(s->buf, s->total);
        case CHECKPOINT_TAG_IPC:              return checkpoint_restore_ipc(s->buf, s->total);
        case CHECKPOINT_TAG_FRAMEBUFFER:      return checkpoint_fb_restore_state(s->buf, s->total);
        case CHECKPOINT_TAG_FRAMEBUFFER_PIXELS:
            return checkpoint_fb_restore_pixels(s->buf, s->total);
        default:                              return 0; /* unknown tag: ignore */
    }
}

/* ----- restore steps ----- */

static int boot_has_checkpoint(void* ctx) {
    boot_v2_ctx_t* c = (boot_v2_ctx_t*)ctx;
    snapshot_reader_t reader;
    return snapshot_store_load(c->store, &reader) == SNAPSHOT_OK ? 1 : 0;
}

/* Reassemble every kernel-state blob from the checkpoint and hand each to its
 * restorer. Page/context records are the user-restore step's job and are skipped
 * here. Keeps the framebuffer pixels blob (which the framebuffer reattach will
 * blit) alive past dispatch so the driver step can still read it. */
static int boot_restore_kernel_state(void* ctx) {
    boot_v2_ctx_t* c = (boot_v2_ctx_t*)ctx;

    snapshot_reader_t reader;
    if (snapshot_store_load(c->store, &reader) != SNAPSHOT_OK) {
        return -1;
    }

    blob_slot_t slots[BOOT_V2_MAX_BLOBS];
    for (int i = 0; i < BOOT_V2_MAX_BLOBS; i++) {
        slots[i].in_use = 0; slots[i].buf = 0; slots[i].complete = 0;
    }

    uint8_t page[SNAPSHOT_PAGE_SIZE];
    snapshot_page_record_t rec;
    rec.page_data = page;

    int rc = 0;
    while (snapshot_reader_next(&reader, &rec) == SNAPSHOT_OK) {
        if (!(rec.flags & CHECKPOINT_REC_KERNEL)) {
            continue; /* pages + context belong to the user-restore step */
        }
        uint32_t total = checkpoint_blob_total(rec.virt_addr);
        uint32_t index = checkpoint_blob_index(rec.virt_addr);

        blob_slot_t* s = slot_for_tag(slots, rec.pid, total);
        if (!s) { rc = -1; break; }

        int pr = checkpoint_blob_place(s->buf, s->total, total, index, page, &s->received);
        if (pr == CHECKPOINT_BLOB_ERR) { rc = -1; break; }
        if (pr == CHECKPOINT_BLOB_COMPLETE && !s->complete) {
            s->complete = 1;
            if (dispatch_blob(s) != 0) { rc = -1; break; }
        }
    }

    /* Free every blob except the framebuffer pixels: the framebuffer reattach
     * (driver step, next) blits from that buffer, so it must outlive this pass.
     * checkpoint_fb_restore_pixels() cached the pointer. */
    for (int i = 0; i < BOOT_V2_MAX_BLOBS; i++) {
        if (slots[i].in_use && slots[i].buf &&
            slots[i].tag != CHECKPOINT_TAG_FRAMEBUFFER_PIXELS) {
            kfree(slots[i].buf);
        }
    }
    return rc;
}

/* Re-attach every registered driver (#143). CONTINUE policy: a device that
 * cannot re-attach is severed and the restore proceeds (section 8), so this is
 * not treated as a critical failure. */
static int boot_reattach_drivers(void* ctx) {
    (void)ctx;
    checkpoint_driver_reattach_all(checkpoint_driver_global(),
                                   CHECKPOINT_REATTACH_CONTINUE);
    return 0;
}

/* Restore user address spaces + processes on top of the now-live kernel state,
 * via the existing v1 boot path (which applies external-state severing). */
static int boot_restore_user(void* ctx) {
    boot_v2_ctx_t* c = (boot_v2_ctx_t*)ctx;
    int restored = checkpoint_restore_boot(c->store);
    return restored < 0 ? -1 : 0;
}

int checkpoint_boot_v2(snapshot_store_t* store) {
    if (!store) {
        return CHECKPOINT_ERR_NO_CHECKPOINT; /* nothing configured: cold boot */
    }

    boot_v2_ctx_t ctx;
    ctx.store = store;

    checkpoint_restore_steps_t steps;
    steps.has_checkpoint       = boot_has_checkpoint;
    steps.restore_kernel_state = boot_restore_kernel_state;
    steps.reattach_drivers     = boot_reattach_drivers;
    steps.restore_user         = boot_restore_user;
    steps.ctx                  = &ctx;

    checkpoint_restore_result_t r = checkpoint_restore_sequence(&steps);
    return r.outcome == CHECKPOINT_RESTORE_RESUMED
         ? CHECKPOINT_OK
         : CHECKPOINT_ERR_NO_CHECKPOINT;
}
