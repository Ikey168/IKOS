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
#include "journal_capture.h"     /* journal_capture_store, JOURNAL_EV_DIVERGE */
#include "checkpoint_journal.h"  /* journal_reader_t, journal_reader_next */
#include "keyframe_store.h"      /* keyframe_store_get, keyframe_store_region_for */
#include "divergence.h"          /* kdiverge_set_mode, kdiverge_ok */
#include "divergence_scan.h"     /* kdiverge_expect_pairs, kdiverge_check_epoch */
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

/* ---- Divergence check at the epoch boundary (#197) ----
 * load_epoch runs with the live state == the start of `epoch` (right after the
 * restore or the previous epoch's re-drive), which is exactly the point whose
 * component checksums were recorded. Read that epoch's recorded sums from the
 * journal, install them as expected, and compare the recomputed sums. */

#define REPLAY_DIV_MAX KDIVERGE_COMPONENT_COUNT

static void replay_divergence_check(uint64_t epoch) {
    journal_store_t* js = journal_capture_store();
    if (!js) return;
    journal_reader_t rd;
    if (journal_store_load(js, &rd) != JOURNAL_OK) return;
    if (rd.epoch != epoch) return; /* that epoch's journal not retained */

    uint32_t ids[REPLAY_DIV_MAX];
    uint32_t sums[REPLAY_DIV_MAX];
    uint32_t n = 0;
    journal_event_t ev;
    while (journal_reader_next(&rd, &ev) == JOURNAL_OK) {
        if (ev.type != JOURNAL_EV_DIVERGE) continue;
        if (n >= REPLAY_DIV_MAX) break;
        ids[n] = (uint32_t)ev.lclock;   /* component id rides in lclock */
        sums[n] = (uint32_t)ev.value;   /* checksum rides in value */
        n++;
    }
    if (n == 0) return; /* no divergence sums recorded for this epoch */

    kdiverge_expect_pairs(epoch, ids, sums, n);
    kdiverge_check_epoch(); /* recompute + compare; detector keeps first leak */
}

/* load_subsystems wrapper: divergence-check this epoch's starting state, then
 * install the epoch's input deltas for re-drive. */
static int drv_load_subsystems(uint64_t epoch,
                               const uint64_t* pts, uint32_t n_pts,
                               const uint64_t* times, uint32_t n_times,
                               const uint8_t* entropy, uint32_t n_entropy) {
    replay_divergence_check(epoch);
#ifdef IKOS_DEBUG
    /* In debug builds a divergence halts replay at the exact epoch/component:
     * the detector recorded which, and returning an error aborts replay_run. */
    if (!kdiverge_ok()) return REPLAY_ERR_LOAD;
#endif
    return replay_load_subsystems(epoch, pts, n_pts, times, n_times,
                                  entropy, n_entropy);
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
    g_driver.load_subsystems = drv_load_subsystems;
    g_driver.drive_steps = drv_drive_steps;
    g_driver.ctx = NULL;

    replay_enter();                    /* the three wrappers to REPLAY */
    kdiverge_set_mode(DIVERGE_REPLAY); /* compare component checksums (#197) */
    int rc = replay_driver_run(&g_driver, keyframe_epoch, target_epoch,
                               target_offset);
    kdiverge_set_mode(DIVERGE_OFF);
    replay_exit();                     /* back to OFF */
    return rc;
}
