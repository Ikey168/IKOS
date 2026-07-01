/* Host-side unit test for the GDB serial transport loop (#198).
 *
 * Drives the pure transport with a scripted input byte stream and captures the
 * output, over the REAL RSP stub (gdbstub.c/.sync), verifying:
 *   1. A packet is read past stray acks, acked with '+', and its framed reply
 *      is written, then gdb's reply-ack is consumed.
 *   2. qSupported advertises the reverse packets, and bs / bc drive the injected
 *      reverse ops through the whole transport.
 *   3. A '-' reply-ack triggers a retransmit of the same reply.
 *   4. A Ctrl-C between packets yields a stop reply.
 *   5. The loop runs multiple packets and ends cleanly when the stream closes.
 *
 * Build: gcc -I../include -o test_gdb_serial \
 *            test_gdb_serial.c ../kernel/gdb_serial.c ../kernel/gdbstub.c \
 *            ../kernel/gdbstub_sync.c
 */

#include <stdint.h>
#include <stdbool.h>

typedef __SIZE_TYPE__ size_t;
extern int printf(const char*, ...);

#include "gdb_serial.h"
#include "gdbstub.h"

/* gdbstub_sync.c calls into the reverse engine; stub those so we link without
 * the whole reverse stack and can observe that bs/bc reached them. */
static int g_step_calls, g_cont_calls;
int kreverse_step(void) { g_step_calls++; return 0; }
int kreverse_continue(uint64_t a, uint64_t b) { (void)a; (void)b; g_cont_calls++; return 0; }

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* ---- Scripted byte transport ---- */

static char g_in[512];
static uint32_t g_in_len, g_in_pos;
static char g_out[1024];
static uint32_t g_out_len;

static void in_reset(void) { g_in_len = 0; g_in_pos = 0; }
static void in_push(const char* s) { while (*s) g_in[g_in_len++] = *s++; }
static void in_push_byte(char c) { g_in[g_in_len++] = c; }
static void out_reset(void) { g_out_len = 0; }

static int mock_get(void* ctx) {
    (void)ctx;
    if (g_in_pos >= g_in_len) return -1; /* stream closed */
    return (uint8_t)g_in[g_in_pos++];
}
static int mock_put(void* ctx, uint8_t b) {
    (void)ctx;
    if (g_out_len < sizeof(g_out)) g_out[g_out_len++] = (char)b;
    return 0;
}
static gdb_serial_ops_t g_ops = { mock_get, mock_put, 0 };

/* Does the captured output contain substring `s`? */
static bool out_contains(const char* s) {
    for (uint32_t i = 0; i + 1 <= g_out_len; i++) {
        uint32_t j = 0;
        while (s[j] && i + j < g_out_len && g_out[i + j] == s[j]) j++;
        if (!s[j]) return true;
    }
    return false;
}

/* Build a framed packet "$payload#cc" into the input stream. */
static void in_push_packet(const char* payload) {
    char framed[300];
    int n = gdbstub_frame(payload, (uint32_t)__builtin_strlen(payload),
                          framed, sizeof(framed));
    for (int i = 0; i < n; i++) in_push_byte(framed[i]);
}

int main(void) {
    printf("test_gdb_serial\n");
    gdbstub_bind_reverse();

    /* ---- 1/2: qSupported through the transport, with a stray leading '+' ---- */
    in_reset(); out_reset();
    in_push("+");                 /* stray ack, must be skipped */
    in_push_packet("qSupported");
    in_push("+");                 /* gdb acks our reply */
    int rc = gdb_serial_serve_once(&g_ops, gdbstub_serve);
    CHECK(rc == GDB_SERIAL_OK, "serve_once qSupported OK");
    CHECK(g_out_len >= 1 && g_out[0] == '+', "our packet ack '+' sent first");
    CHECK(out_contains("ReverseStep+"), "reply advertises ReverseStep+");
    CHECK(out_contains("ReverseContinue+"), "reply advertises ReverseContinue+");

    /* ---- 2: bs / bc drive the reverse ops ---- */
    in_reset(); out_reset();
    g_step_calls = g_cont_calls = 0;
    in_push_packet("bs"); in_push("+");
    in_push_packet("bc"); in_push("+");
    rc = gdb_serial_loop(&g_ops, gdbstub_serve);
    CHECK(rc == GDB_SERIAL_CLOSED, "loop ends cleanly when stream closes");
    CHECK(g_step_calls == 1, "bs drove reverse_step once");
    CHECK(g_cont_calls == 1, "bc drove reverse_continue once");
    CHECK(out_contains("$S05#"), "bs/bc reply with a stop packet");

    /* ---- 3: '-' reply-ack forces a retransmit ---- */
    in_reset(); out_reset();
    in_push_packet("?");
    in_push("-");   /* reject the first reply */
    in_push("+");   /* accept the retransmit */
    rc = gdb_serial_serve_once(&g_ops, gdbstub_serve);
    CHECK(rc == GDB_SERIAL_OK, "serve_once with a NAK retransmit OK");
    /* The stop reply "$S05#b8" should appear twice (original + retransmit). */
    int occurrences = 0;
    for (uint32_t i = 0; i + 5 <= g_out_len; i++) {
        if (g_out[i]=='$'&&g_out[i+1]=='S'&&g_out[i+2]=='0'&&g_out[i+3]=='5'&&g_out[i+4]=='#')
            occurrences++;
    }
    CHECK(occurrences == 2, "reply retransmitted once after '-'");

    /* ---- 4: Ctrl-C between packets yields a stop reply ---- */
    in_reset(); out_reset();
    in_push_byte(0x03);
    in_push("+");   /* ack the stop reply */
    rc = gdb_serial_serve_once(&g_ops, gdbstub_serve);
    CHECK(rc == GDB_SERIAL_OK, "Ctrl-C handled");
    CHECK(out_contains("$S05#"), "interrupt answered with a stop packet");

    /* ---- 5: closed stream at packet boundary ---- */
    in_reset(); out_reset();
    rc = gdb_serial_serve_once(&g_ops, gdbstub_serve);
    CHECK(rc == GDB_SERIAL_CLOSED, "empty stream reports CLOSED");

    printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
