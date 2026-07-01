/* IKOS Orthogonal Persistence - Divergence component scan (#197, epic #159)
 *
 * The divergence detector (#166) stores and compares per-component checksums;
 * this layer feeds it real component checksums at each epoch boundary. On a
 * record run it checksums each restored component (process table, scheduler,
 * user pages, ...) and records the sums; those sums ride in the input journal.
 * On replay it recomputes each component's checksum and compares it against the
 * recorded value, so a nondeterminism leak the wrappers missed halts at the
 * exact epoch and component.
 *
 * The scan core is pure and host-testable: each component's checksum comes from
 * an injected source function, and the record/compare sinks are injected too, so
 * tests drive the whole record->journal->replay round trip with mocks. The
 * kernel adapter (divergence_scan_sync.c) supplies the concrete component
 * sources over the live subsystems and wires the global detector and journal.
 *
 * See docs/architecture/time-travel.md.
 */

#ifndef DIVERGENCE_SCAN_H
#define DIVERGENCE_SCAN_H

#include <stdint.h>
#include <stdbool.h>

/* Computes one component's checksum over its current state. */
typedef uint32_t (*diverge_source_fn)(void* ctx);

typedef struct {
    uint32_t          component;  /* KDIVERGE_* id */
    diverge_source_fn source;     /* checksum of this component's state now */
    void*             ctx;        /* passed to source */
} diverge_source_t;

/* RECORD: checksum each component and hand (component, sum) to record_cb, also
 * writing them into out_ids[i] / out_sums[i] for journaling. Returns the number
 * of components scanned. record_cb may be NULL (just fill the out arrays). */
uint32_t diverge_scan_record(const diverge_source_t* srcs, uint32_t n,
                             int (*record_cb)(uint32_t comp, uint32_t sum),
                             uint32_t* out_ids, uint32_t* out_sums);

/* REPLAY: checksum each component and hand (component, sum) to check_cb, which
 * returns true on a match. Returns true only if every component matched (an
 * empty scan trivially matches). check_cb must be non-NULL. */
bool diverge_scan_check(const diverge_source_t* srcs, uint32_t n,
                        bool (*check_cb)(uint32_t comp, uint32_t sum));

/* ---- Kernel adapter (divergence_scan_sync.c) ----
 * A registry of live component sources plus the record/replay plumbing the
 * journal and replay driver drive at each epoch boundary. */

/* Register a component source (id, checksum fn, ctx). Idempotent registration
 * is the caller's responsibility; call kdiverge_reset_sources first to rebind. */
void kdiverge_register(uint32_t component, diverge_source_fn source, void* ctx);
void kdiverge_reset_sources(void);

/* Register the concrete in-tree component sources (process table, scheduler,
 * ...). Call once at boot after kdiverge_init(). */
void kdiverge_register_kernel_sources(void);

/* RECORD: put the detector in RECORD mode, begin `epoch`, and checksum every
 * registered component (kdiverge_record). The sums are stashed for journaling. */
void kdiverge_record_epoch(uint64_t epoch);

/* The sums stashed by the last kdiverge_record_epoch, as parallel (ids, sums)
 * arrays for journaling. Matches journal_capture_sources_t.divergence_sums. */
uint32_t kdiverge_journal_sums(const uint32_t** ids, const uint32_t** sums);

/* REPLAY: install the recorded (id, sum) pairs read from `epoch`'s journal as
 * the expected per-component checksums, then recompute and compare every
 * registered component. Returns true if all matched. The detector keeps the
 * first divergence (kdiverge_ok() / the detector's sticky verdict). */
int  kdiverge_expect_pairs(uint64_t epoch, const uint32_t* ids,
                           const uint32_t* sums, uint32_t n);
bool kdiverge_check_epoch(void);

#endif /* DIVERGENCE_SCAN_H */
