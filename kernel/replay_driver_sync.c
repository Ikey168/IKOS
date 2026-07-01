/* IKOS Orthogonal Persistence - Replay driver kernel adapter (#196)
 *
 * See include/replay_driver.h. Wires the pure replay driver to the real
 * in-tree subsystems: the input journal (event source), the keyframe retention
 * store (restore the nearest keyframe at or before the target, #195), the
 * REPLAY-mode subsystem loaders (#165), and the live scheduler (re-drive). This
 * completes the load_epoch/run_epoch hooks deferred in #165 and gives the
 * booted kernel a single replay_to(epoch, offset) entry point.
 */

#include "replay_driver.h"
#include "replay_engine.h"       /* replay_enter/exit, replay_load_subsystems */
#include "journal_capture.h"     /* journal_capture_store */
#include "checkpoint_journal.h"  /* journal_reader_t, journal_reader_next */
#include "keyframe_store.h"      /* keyframe_store_get, keyframe_store_region_for */
#include "checkpoint.h"          /* checkpoint_restore_boot */
#include "scheduler.h"           /* scheduler_tick */
#include <stddef.h>

/* The driver carries large per-epoch scratch buffers; keep it in BSS. */
static replay_driver_t g_driver;

/* Journal reader for the epoch currently being loaded. */
static journal_reader_t g_jreader;
static bool             g_jreader_valid;

/* ---- Event source over the input journal (#161/#194) ----
 * The journal store holds the most recently committed epoch's journal, so an
 * epoch is available only while it is the active journal. begin_epoch validates
 * that; multi-epoch retention (a journal ring) is future work. */

static int src_begin_epoch(void* ctx, uint64_t epoch) {
    (void)ctx;
    g_jreader_valid = false;
    journal_store_t* js = journal_capture_store();
    if (!js) return -1;
    if (journal_store_load(js, &g_jreader) != JOURNAL_OK) return -1;
    if (g_jreader.epoch != epoch) return -1; /* that epoch's journal not retained */
    g_jreader_valid = true;
    return 0;
}

static int src_next(void* ctx, replay_event_t* out) {
    (void)ctx;
    if (!g_jreader_valid) return -1;
    journal_event_t ev;
    int rc = journal_reader_next(&g_jreader, &ev);
    if (rc == JOURNAL_ERR_NO_JOURNAL) return 0; /* epoch exhausted */
    if (rc != JOURNAL_OK) return -1;
    out->type = ev.type;
    out->len = ev.len;
    out->lclock = ev.lclock;
    out->value = ev.value;
    return 1;
}

/* ---- Restore hook: nearest retained keyframe at or before `epoch` ---- */

static int drv_restore_keyframe(void* ctx, uint64_t epoch) {
    (void)ctx;
    keyframe_store_t* ks = keyframe_store_get();
    if (!ks) return -1;
    uint64_t selected = 0;
    snapshot_store_t* region = keyframe_store_region_for(ks, epoch, &selected);
    if (!region) return -1;
    return checkpoint_restore_boot(region) < 0 ? -1 : 0;
}

/* ---- Drive hook: advance the scheduler `steps` times in REPLAY mode ---- */

static int drv_drive_steps(void* ctx, uint64_t epoch, uint64_t steps) {
    (void)ctx;
    (void)epoch;
    for (uint64_t i = 0; i < steps; i++) {
        scheduler_tick(); /* REPLAY mode forces switches at the loaded points */
    }
    return 0;
}

int replay_to(uint64_t target_epoch, uint64_t target_offset) {
    keyframe_store_t* ks = keyframe_store_get();
    if (!ks) return REPLAY_ERR_RESTORE;

    /* Replay starts from the nearest retained keyframe at or before the target. */
    uint64_t keyframe_epoch = 0;
    if (keyframe_store_region_for(ks, target_epoch, &keyframe_epoch) == NULL) {
        return REPLAY_ERR_RESTORE; /* target predates the retained window */
    }

    g_driver.restore_keyframe = drv_restore_keyframe;
    g_driver.source.begin_epoch = src_begin_epoch;
    g_driver.source.next = src_next;
    g_driver.source.ctx = NULL;
    g_driver.load_subsystems = replay_load_subsystems;
    g_driver.drive_steps = drv_drive_steps;
    g_driver.ctx = NULL;

    replay_enter(); /* the three wrappers to REPLAY */
    int rc = replay_driver_run(&g_driver, keyframe_epoch, target_epoch,
                               target_offset);
    replay_exit();  /* back to OFF */
    return rc;
}
