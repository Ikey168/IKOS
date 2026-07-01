/* Host-side unit test for the GDB reverse-debugging stub (#172).
 *
 * Verifies:
 *   1. RSP framing: checksum, frame, and unframe round-trip; a bad checksum is
 *      flagged.
 *   2. qSupported advertises ReverseStep+ / ReverseContinue+ so gdb will send
 *      the reverse packets.
 *   3. "bs" maps to reverse-step and "bc" to reverse-continue, each replying
 *      with a stop; "?" replies with a stop; unknown packets reply empty.
 *
 * Build: gcc -I../include -o test_gdbstub test_gdbstub.c ../kernel/gdbstub.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "gdbstub.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

static bool streq(const char* a, uint32_t alen, const char* lit) {
    uint32_t n = 0; while (lit[n]) n++;
    if (alen != n) return false;
    for (uint32_t i = 0; i < n; i++) if (a[i] != lit[i]) return false;
    return true;
}
static bool contains(const char* hay, uint32_t hlen, const char* needle) {
    uint32_t n = 0; while (needle[n]) n++;
    if (n == 0 || hlen < n) return false;
    for (uint32_t i = 0; i + n <= hlen; i++) {
        bool m = true;
        for (uint32_t j = 0; j < n; j++) if (hay[i + j] != needle[j]) { m = false; break; }
        if (m) return true;
    }
    return false;
}

/* Mock reverse ops that record what gdb drove. */
typedef struct { int steps; int continues; } rev_t;
static int mock_step(void* c) { ((rev_t*)c)->steps++; return 0; }
static int mock_cont(void* c) { ((rev_t*)c)->continues++; return 0; }

int main(void) {
    printf("=== GDB reverse-debugging stub (#172) unit test ===\n");

    /* --- 1. Framing --- */
    {
        /* checksum("OK") = 'O'(0x4F) + 'K'(0x4B) = 0x9A */
        CHECK(gdbstub_checksum("OK", 2) == 0x9A, "checksum of OK is 0x9a");

        char frame[64];
        int n = gdbstub_frame("OK", 2, frame, sizeof(frame));
        CHECK(n == 6 && streq(frame, (uint32_t)n, "$OK#9a"), "frame builds $OK#9a");

        char payload[64]; bool ok = false;
        int p = gdbstub_unframe(frame, (uint32_t)n, payload, sizeof(payload), &ok);
        CHECK(p == 2 && ok && streq(payload, (uint32_t)p, "OK"), "unframe round-trips and validates");

        char bad[] = "$OK#00";
        gdbstub_unframe(bad, 6, payload, sizeof(payload), &ok);
        CHECK(!ok, "a wrong checksum is flagged");
    }

    /* --- 2 & 3. Packet dispatch --- */
    {
        rev_t rev = { 0, 0 };
        gdbstub_ops_t ops = { mock_step, mock_cont, &rev };
        char out[128];
        int n;

        n = gdbstub_handle(&ops, "qSupported:multiprocess+", 24, out, sizeof(out));
        CHECK(n > 0 && contains(out, (uint32_t)n, "ReverseStep+") &&
              contains(out, (uint32_t)n, "ReverseContinue+"),
              "qSupported advertises the reverse packets");

        n = gdbstub_handle(&ops, "?", 1, out, sizeof(out));
        CHECK(streq(out, (uint32_t)n, "S05"), "? reports a stop");

        n = gdbstub_handle(&ops, "bs", 2, out, sizeof(out));
        CHECK(streq(out, (uint32_t)n, "S05") && rev.steps == 1,
              "bs drives reverse-step and replies with a stop");

        n = gdbstub_handle(&ops, "bc", 2, out, sizeof(out));
        CHECK(streq(out, (uint32_t)n, "S05") && rev.continues == 1,
              "bc drives reverse-continue and replies with a stop");

        n = gdbstub_handle(&ops, "vMustReplyEmpty", 15, out, sizeof(out));
        CHECK(n == 0, "an unsupported packet replies empty");

        CHECK(rev.steps == 1 && rev.continues == 1, "reverse ops driven exactly once each");
    }

    if (failures == 0) {
        printf("PASSED: gdb reverse-step / reverse-continue map to the reverse engine\n");
        return 0;
    }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
