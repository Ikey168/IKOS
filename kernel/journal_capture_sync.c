/* IKOS Orthogonal Persistence - Journal capture kernel adapter (#194)
 *
 * See include/journal_capture.h. This wires the pure capture core to the real
 * kernel: a journal store bound to a region of the persistence device, the live
 * delta sources (scheduler / time / entropy record buffers), and a checkpoint
 * post-commit hook so every committed epoch is journaled alongside its
 * checkpoint. Kept out of journal_capture.c so that core stays dependency-free
 * and host-testable.
 */

#include "journal_capture.h"
#include "scheduler.h"       /* scheduler_preempt_points */
#include "time_record.h"     /* ktime_values */
#include "entropy_record.h"  /* kentropy_bytes */
#include "checkpoint.h"      /* checkpoint_set_journal_hook */
#include <stddef.h>

/* The journal store bound to the persistence device (caller-allocated storage
 * lives here so no allocator is needed). */
static journal_store_t g_journal_store;
static bool            g_journal_ready = false;

/* Live delta sources: the record-subsystem accessors, each returning the
 * current epoch's captured buffer. */
static const journal_capture_sources_t g_live_sources = {
    .preempt_points = scheduler_preempt_points,
    .time_values    = ktime_values,
    .entropy_bytes  = kentropy_bytes,
};

/* Checkpoint post-commit hook: journal the epoch that just committed. Runs
 * before the next epoch's recording begins (checkpoints never overlap), so the
 * accessors still hold this epoch's deltas. */
static int journal_capture_hook(uint64_t epoch) {
    if (!g_journal_ready) {
        return JOURNAL_ERR_STATE;
    }
    return journal_capture_epoch(&g_journal_store, epoch, 0, &g_live_sources);
}

int journal_capture_init(fat_block_device_t* dev, uint32_t base_sector,
                         uint32_t slot_sectors) {
    if (journal_store_init(&g_journal_store, dev, base_sector, slot_sectors)
            != JOURNAL_OK) {
        return JOURNAL_ERR_IO;
    }

    /* Format only if the region holds no valid journal yet, so a journal left
     * by a previous run survives a reboot and can be replayed. */
    journal_reader_t probe;
    if (journal_store_load(&g_journal_store, &probe) != JOURNAL_OK) {
        if (journal_store_format(&g_journal_store) != JOURNAL_OK) {
            return JOURNAL_ERR_IO;
        }
    }

    g_journal_ready = true;
    checkpoint_set_journal_hook(journal_capture_hook);
    return JOURNAL_OK;
}

journal_store_t* journal_capture_store(void) {
    return g_journal_ready ? &g_journal_store : NULL;
}
