/* IKOS Orthogonal Persistence - Divergence component scan adapter (#197)
 *
 * See include/divergence_scan.h. Wires the pure scan core to the global
 * divergence detector (#166), a registry of live component sources, and the
 * journal: on a record run it checksums each registered component and stashes
 * the sums for journaling; on replay it installs the recorded sums and compares
 * the recomputed ones, so a nondeterminism leak halts at the exact epoch and
 * component.
 *
 * The concrete component sources checksum the restored subsystems. Process
 * table and scheduler order are wired here (both reachable through stable
 * process-manager accessors); further components register as their subsystems
 * expose deterministic snapshot accessors.
 */

#include "divergence_scan.h"
#include "divergence.h"          /* kdiverge_*, divergence_checksum */
#include "process_manager.h"     /* pm_get_process_list, pm_get_process */
#include "scheduler.h"           /* task_get_current */
#include <stddef.h>

/* ---- Source registry ---- */

static diverge_source_t g_sources[KDIVERGE_COMPONENT_COUNT];
static uint32_t         g_nsources;

/* Sums stashed by the last record scan, for journaling. */
static uint32_t g_rec_ids[KDIVERGE_COMPONENT_COUNT];
static uint32_t g_rec_sums[KDIVERGE_COMPONENT_COUNT];
static uint32_t g_rec_count;

void kdiverge_reset_sources(void) {
    g_nsources = 0;
}

void kdiverge_register(uint32_t component, diverge_source_fn source, void* ctx) {
    if (!source || g_nsources >= KDIVERGE_COMPONENT_COUNT) return;
    g_sources[g_nsources].component = component;
    g_sources[g_nsources].source = source;
    g_sources[g_nsources].ctx = ctx;
    g_nsources++;
}

/* ---- Concrete component sources over the restored subsystems ---- */

/* Process table: the pid and state of every live process, in list order. */
static uint32_t sum_proctable(void* ctx) {
    (void)ctx;
    uint32_t pids[PM_MAX_PROCESSES];
    uint32_t n = 0;
    if (pm_get_process_list(pids, PM_MAX_PROCESSES, &n) != 0) return 0;
    uint32_t crc = 0;
    for (uint32_t i = 0; i < n; i++) {
        process_t* p = pm_get_process(pids[i]);
        if (!p) continue;
        uint32_t pid = (uint32_t)p->pid;
        uint32_t st = (uint32_t)p->state;
        crc = divergence_checksum(crc, &pid, sizeof(pid));
        crc = divergence_checksum(crc, &st, sizeof(st));
    }
    return crc;
}

/* Scheduler: the currently-running pid and the ready order (the process list
 * order), which the deterministic-preemption replay must reproduce. */
static uint32_t sum_scheduler(void* ctx) {
    (void)ctx;
    task_t* cur = task_get_current();
    uint32_t cpid = cur ? (uint32_t)cur->pid : 0xFFFFFFFFu;
    uint32_t crc = divergence_checksum(0, &cpid, sizeof(cpid));
    uint32_t pids[PM_MAX_PROCESSES];
    uint32_t n = 0;
    if (pm_get_process_list(pids, PM_MAX_PROCESSES, &n) == 0 && n > 0) {
        crc = divergence_checksum(crc, pids, n * (uint32_t)sizeof(pids[0]));
    }
    return crc;
}

void kdiverge_register_kernel_sources(void) {
    kdiverge_reset_sources();
    kdiverge_register(KDIVERGE_PROCTABLE, sum_proctable, NULL);
    kdiverge_register(KDIVERGE_SCHEDULER, sum_scheduler, NULL);
}

/* ---- Record side ---- */

void kdiverge_record_epoch(uint64_t epoch) {
    kdiverge_set_mode(DIVERGE_RECORD);
    kdiverge_begin_epoch(epoch);
    g_rec_count = diverge_scan_record(g_sources, g_nsources, kdiverge_record,
                                      g_rec_ids, g_rec_sums);
}

uint32_t kdiverge_journal_sums(const uint32_t** ids, const uint32_t** sums) {
    if (ids) *ids = g_rec_ids;
    if (sums) *sums = g_rec_sums;
    return g_rec_count;
}

/* ---- Replay side ---- */

int kdiverge_expect_pairs(uint64_t epoch, const uint32_t* ids,
                          const uint32_t* sums, uint32_t n) {
    /* Build a dense array indexed by component id: divergence_expect installs
     * sums[i] for component i. Components not in the journal stay 0; only
     * registered components are ever checked, so unused slots do not matter. */
    uint32_t dense[DIVERGE_MAX_COMPONENTS];
    for (uint32_t i = 0; i < DIVERGE_MAX_COMPONENTS; i++) dense[i] = 0;
    uint32_t span = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (ids[i] >= DIVERGE_MAX_COMPONENTS) continue;
        dense[ids[i]] = sums[i];
        if (ids[i] + 1 > span) span = ids[i] + 1;
    }
    return kdiverge_expect(epoch, dense, span);
}

bool kdiverge_check_epoch(void) {
    return diverge_scan_check(g_sources, g_nsources, kdiverge_check);
}
