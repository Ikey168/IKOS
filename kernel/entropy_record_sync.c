/* IKOS Orthogonal Persistence - Deterministic Entropy, kernel adapter (#164)
 *
 * Wires the pure entropy_record core (entropy_record.c) to a global instance
 * and exposes the gate kernel randomness routes through. Because the real
 * entropy source in IKOS is /dev/urandom (read by auth_core.c's
 * auth_generate_random, which is not freestanding), the live source is
 * registered at runtime via kentropy_set_source() rather than hard-wired here,
 * keeping this adapter dependency-free.
 *
 * Intended use: at init the kernel calls kentropy_set_source() with a reader
 * for its entropy device, and code that needs randomness draws through
 * kentropy_fill(). On a record run the bytes are captured; on replay they are
 * reproduced.
 */

#include "entropy_record.h"

static uint8_t        g_kentropy_log[KENTROPY_LOG_MAX];
static entropy_record_t g_kentropy;

void kentropy_init(void) {
    /* No source yet: kentropy_set_source() installs the real one. Until then
     * OFF mode zero-fills, which is safe and obviously non-random. */
    entropy_record_init(&g_kentropy, ENTROPY_REC_OFF, 0, 0,
                        g_kentropy_log, KENTROPY_LOG_MAX);
}

void kentropy_set_source(entropy_source_fn source, void* ctx) {
    g_kentropy.source = source;
    g_kentropy.ctx = ctx;
}

int kentropy_fill(void* out, uint32_t len) {
    return entropy_record_fill(&g_kentropy, out, len);
}

void kentropy_set_mode(entropy_rec_mode_t mode) {
    entropy_record_set_mode(&g_kentropy, mode);
}

void kentropy_begin_epoch(uint64_t epoch) {
    entropy_record_begin_epoch(&g_kentropy, epoch);
}

uint32_t kentropy_bytes(const uint8_t** out) {
    return entropy_record_bytes(&g_kentropy, out);
}

int kentropy_load(uint64_t epoch, const uint8_t* bytes, uint32_t n) {
    return entropy_record_load(&g_kentropy, epoch, bytes, n);
}
