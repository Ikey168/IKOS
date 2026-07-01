/* Host-side unit test for the MCP time-travel interface (#159 Milestone E).
 *
 * Verifies:
 *   1. The JSON scanners extract integer and string fields.
 *   2. tools/list advertises the time-travel tools.
 *   3. tools/call dispatches rewind_to / reverse_step / watch_last_write /
 *      list_checkpoints to the injected ops, echoes the request id, and reports
 *      tool errors as isError.
 *
 * Build: gcc -I../include -o test_mcp test_mcp.c ../kernel/mcp.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "mcp.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

static uint32_t slen(const char* s) { uint32_t n = 0; while (s[n]) n++; return n; }
static bool contains(const char* hay, uint32_t hlen, const char* needle) {
    uint32_t n = slen(needle);
    if (n == 0 || hlen < n) return false;
    for (uint32_t i = 0; i + n <= hlen; i++) {
        bool m = true;
        for (uint32_t j = 0; j < n; j++) if (hay[i + j] != needle[j]) { m = false; break; }
        if (m) return true;
    }
    return false;
}

/* Mock ops recording what the agent drove. */
typedef struct {
    uint64_t rewind_epoch, rewind_offset; int rewinds;
    int steps;
    int watches;
} mock_t;
static int m_list(void* c, uint64_t* o, uint64_t* n, uint32_t* cnt) {
    (void)c; *o = 20; *n = 40; *cnt = 3; return 0;
}
static int m_rewind(void* c, uint64_t e, uint64_t off) {
    mock_t* m = (mock_t*)c; m->rewind_epoch = e; m->rewind_offset = off; m->rewinds++;
    return (e <= 40) ? 0 : -1;
}
static int m_step(void* c, uint64_t* e, uint64_t* o) {
    mock_t* m = (mock_t*)c; m->steps++; *e = 30; *o = 3; return 0;
}
static int m_watch(void* c, uint64_t* e, uint64_t* o) {
    mock_t* m = (mock_t*)c; m->watches++; *e = 40; *o = 1; return 0;
}

int main(void) {
    printf("=== MCP time-travel interface unit test ===\n");

    /* --- 1. JSON scanners --- */
    {
        const char* j = "{\"id\":7,\"params\":{\"name\":\"rewind_to\",\"arguments\":{\"epoch\":30,\"offset\":2}}}";
        uint32_t n = slen(j);
        uint64_t v = 0; char s[32];
        CHECK(mcp_json_int(j, n, "epoch", &v) && v == 30, "int field epoch=30");
        CHECK(mcp_json_int(j, n, "offset", &v) && v == 2, "int field offset=2");
        CHECK(mcp_json_int(j, n, "id", &v) && v == 7, "int field id=7");
        CHECK(mcp_json_str(j, n, "name", s, sizeof(s)) && contains(s, slen(s), "rewind_to"),
              "string field name=rewind_to");
    }

    mock_t mock = {0};
    mcp_ops_t ops = { m_list, m_rewind, m_step, m_watch, &mock };
    char out[512];

    /* --- 2. tools/list --- */
    {
        const char* req = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}";
        int n = mcp_handle(&ops, req, slen(req), out, sizeof(out));
        CHECK(n > 0 && contains(out, (uint32_t)n, "rewind_to") &&
              contains(out, (uint32_t)n, "watch_last_write") &&
              contains(out, (uint32_t)n, "reverse_step") &&
              contains(out, (uint32_t)n, "list_checkpoints"),
              "tools/list advertises the four time-travel tools");
        CHECK(contains(out, (uint32_t)n, "\"id\":1"), "tools/list echoes the request id");
    }

    /* --- 3. tools/call dispatch --- */
    {
        const char* req = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\","
                          "\"params\":{\"name\":\"rewind_to\",\"arguments\":{\"epoch\":35,\"offset\":2}}}";
        int n = mcp_handle(&ops, req, slen(req), out, sizeof(out));
        CHECK(mock.rewinds == 1 && mock.rewind_epoch == 35 && mock.rewind_offset == 2,
              "rewind_to dispatched with epoch=35 offset=2");
        CHECK(contains(out, (uint32_t)n, "rewound to epoch=35 offset=2") &&
              contains(out, (uint32_t)n, "\"id\":2"),
              "rewind_to result text and id echoed");
    }
    {
        const char* req = "{\"id\":3,\"method\":\"tools/call\","
                          "\"params\":{\"name\":\"watch_last_write\",\"arguments\":{}}}";
        int n = mcp_handle(&ops, req, slen(req), out, sizeof(out));
        CHECK(mock.watches == 1 && contains(out, (uint32_t)n, "last write at epoch=40 offset=1"),
              "watch_last_write returns the write position");
    }
    {
        const char* req = "{\"id\":4,\"method\":\"tools/call\","
                          "\"params\":{\"name\":\"reverse_step\",\"arguments\":{}}}";
        int n = mcp_handle(&ops, req, slen(req), out, sizeof(out));
        CHECK(mock.steps == 1 && contains(out, (uint32_t)n, "stepped back to epoch=30 offset=3"),
              "reverse_step returns the new position");
    }
    {
        const char* req = "{\"id\":5,\"method\":\"tools/call\","
                          "\"params\":{\"name\":\"list_checkpoints\",\"arguments\":{}}}";
        int n = mcp_handle(&ops, req, slen(req), out, sizeof(out));
        CHECK(contains(out, (uint32_t)n, "count=3 oldest=20 newest=40"),
              "list_checkpoints returns the retained window");
    }
    {
        const char* req = "{\"id\":6,\"method\":\"tools/call\","
                          "\"params\":{\"name\":\"no_such_tool\",\"arguments\":{}}}";
        int n = mcp_handle(&ops, req, slen(req), out, sizeof(out));
        CHECK(contains(out, (uint32_t)n, "unknown tool") && contains(out, (uint32_t)n, "isError"),
              "an unknown tool is reported as an error");
    }
    {
        const char* req = "{\"id\":8,\"method\":\"nope\"}";
        int n = mcp_handle(&ops, req, slen(req), out, sizeof(out));
        CHECK(contains(out, (uint32_t)n, "-32601"), "an unknown method returns method-not-found");
    }

    if (failures == 0) {
        printf("PASSED: MCP tools dispatch record/rewind/reverse over JSON-RPC\n");
        return 0;
    }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
