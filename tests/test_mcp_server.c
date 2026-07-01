/* Host-side unit test for the MCP stdio JSON-RPC server loop (#199).
 *
 * Drives the pure transport with a scripted newline-delimited request stream and
 * captures the output, over the REAL MCP tool surface (mcp.c) with mock ops,
 * verifying:
 *   1. A tools/list request is read as a line and its response written +'\n'.
 *   2. tools/call rewind_to / reverse_step / list_checkpoints dispatch through
 *      the loop to the injected ops, each producing one response line.
 *   3. Blank lines between requests are skipped.
 *   4. The loop ends cleanly at end of stream, having served every request.
 *
 * Build: gcc -I../include -o test_mcp_server \
 *            test_mcp_server.c ../kernel/mcp_server.c ../kernel/mcp.c
 */

#include <stdint.h>
#include <stdbool.h>

typedef __SIZE_TYPE__ size_t;
extern int printf(const char*, ...);

#include "mcp_server.h"
#include "mcp.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* ---- Mock time-travel ops (like test_mcp.c) ---- */
typedef struct { int lists, rewinds, steps; uint64_t rewind_epoch, rewind_offset; } mock_t;
static int m_list(void* c, uint64_t* o, uint64_t* n, uint32_t* cnt) {
    mock_t* m = (mock_t*)c; m->lists++; *o = 20; *n = 40; *cnt = 3; return 0;
}
static int m_rewind(void* c, uint64_t e, uint64_t off) {
    mock_t* m = (mock_t*)c; m->rewinds++; m->rewind_epoch = e; m->rewind_offset = off; return 0;
}
static int m_step(void* c, uint64_t* e, uint64_t* o) {
    mock_t* m = (mock_t*)c; m->steps++; *e = 30; *o = 3; return 0;
}
static int m_watch(void* c, uint64_t* e, uint64_t* o) {
    (void)c; *e = 40; *o = 1; return 0;
}

static mock_t g_mock;
static mcp_ops_t g_ops;

/* Bound handle step for the loop: dispatch through the real mcp_handle. */
static int handle(const char* req, uint32_t reqlen, char* out, uint32_t outcap) {
    return mcp_handle(&g_ops, req, reqlen, out, outcap);
}

/* ---- Scripted byte transport ---- */
static char g_in[2048]; static uint32_t g_in_len, g_in_pos;
static char g_out[4096]; static uint32_t g_out_len;
static void in_reset(void) { g_in_len = 0; g_in_pos = 0; }
static void in_push(const char* s) { while (*s) g_in[g_in_len++] = *s++; }
static void out_reset(void) { g_out_len = 0; }
static int mock_get(void* c) { (void)c; return g_in_pos < g_in_len ? (uint8_t)g_in[g_in_pos++] : -1; }
static int mock_put(void* c, uint8_t b) { (void)c; if (g_out_len < sizeof(g_out)) g_out[g_out_len++] = (char)b; return 0; }
static mcp_transport_t g_t = { mock_get, mock_put, 0 };

static bool out_contains(const char* s) {
    for (uint32_t i = 0; i < g_out_len; i++) {
        uint32_t j = 0;
        while (s[j] && i + j < g_out_len && g_out[i + j] == s[j]) j++;
        if (!s[j]) return true;
    }
    return false;
}
static uint32_t count_newlines(void) {
    uint32_t c = 0;
    for (uint32_t i = 0; i < g_out_len; i++) if (g_out[i] == '\n') c++;
    return c;
}

int main(void) {
    printf("test_mcp_server\n");
    g_ops.list_checkpoints = m_list;
    g_ops.rewind_to = m_rewind;
    g_ops.reverse_step = m_step;
    g_ops.watch_last_write = m_watch;
    g_ops.ctx = &g_mock;

    /* ---- 1: a single tools/list request round-trips as a response line ---- */
    in_reset(); out_reset();
    in_push("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}\n");
    int rc = mcp_server_serve_once(&g_t, handle);
    CHECK(rc == MCP_SERVER_OK, "serve_once tools/list OK");
    CHECK(out_contains("rewind_to") && out_contains("reverse_step"),
          "tools/list response lists the tools");
    CHECK(out_contains("\"id\":1"), "response echoes the request id");
    CHECK(g_out_len > 0 && g_out[g_out_len - 1] == '\n', "response terminated by newline");
    CHECK(count_newlines() == 1, "exactly one response line written");

    /* ---- 2/3: a multi-request stream (with a blank line) drives the loop ---- */
    in_reset(); out_reset();
    g_mock = (mock_t){0};
    in_push("{\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"rewind_to\","
            "\"arguments\":{\"epoch\":35,\"offset\":2}}}\n");
    in_push("\r\n");                     /* a blank line between requests */
    in_push("{\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"reverse_step\","
            "\"arguments\":{}}}\n");
    in_push("{\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"list_checkpoints\","
            "\"arguments\":{}}}\n");
    rc = mcp_server_loop(&g_t, handle);
    CHECK(rc == MCP_SERVER_CLOSED, "loop ends cleanly at end of stream");
    CHECK(g_mock.rewinds == 1 && g_mock.rewind_epoch == 35 && g_mock.rewind_offset == 2,
          "rewind_to dispatched with epoch=35 offset=2 through the loop");
    CHECK(g_mock.steps == 1, "reverse_step dispatched through the loop");
    CHECK(g_mock.lists == 1, "list_checkpoints dispatched through the loop");
    CHECK(out_contains("rewound to epoch=35 offset=2"), "rewind result line written");
    CHECK(out_contains("stepped back to epoch=30 offset=3"), "reverse_step result line written");
    CHECK(out_contains("count=3 oldest=20 newest=40"), "list_checkpoints result line written");
    CHECK(count_newlines() == 3, "three response lines (blank line skipped)");

    /* ---- 4: empty stream closes without a response ---- */
    in_reset(); out_reset();
    rc = mcp_server_serve_once(&g_t, handle);
    CHECK(rc == MCP_SERVER_CLOSED && g_out_len == 0, "empty stream reports CLOSED, no output");

    printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
