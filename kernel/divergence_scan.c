/* IKOS Orthogonal Persistence - Divergence component scan core (#197)
 *
 * See include/divergence_scan.h. Pure and host-testable: it only calls the
 * injected component sources and record/compare sinks, so it has no allocator,
 * hardware, or subsystem dependency.
 */

#include "divergence_scan.h"

uint32_t diverge_scan_record(const diverge_source_t* srcs, uint32_t n,
                             int (*record_cb)(uint32_t comp, uint32_t sum),
                             uint32_t* out_ids, uint32_t* out_sums) {
    if (!srcs || !out_ids || !out_sums) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (!srcs[i].source) continue;
        uint32_t sum = srcs[i].source(srcs[i].ctx);
        out_ids[count] = srcs[i].component;
        out_sums[count] = sum;
        if (record_cb) record_cb(srcs[i].component, sum);
        count++;
    }
    return count;
}

bool diverge_scan_check(const diverge_source_t* srcs, uint32_t n,
                        bool (*check_cb)(uint32_t comp, uint32_t sum)) {
    if (!srcs || !check_cb) return false;
    bool ok = true;
    for (uint32_t i = 0; i < n; i++) {
        if (!srcs[i].source) continue;
        uint32_t sum = srcs[i].source(srcs[i].ctx);
        /* Check every component even after a mismatch so the detector sees each
         * one; the first divergence is what it keeps (sticky). */
        if (!check_cb(srcs[i].component, sum)) ok = false;
    }
    return ok;
}
