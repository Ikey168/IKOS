/* IKOS Orthogonal Persistence - MCP Time-Travel Interface (#159 Milestone E)
 *
 * See include/mcp.h and docs/architecture/time-travel.md (Plan 2).
 *
 * Pure JSON-RPC dispatch core: focused scanners for the known tool schema (not a
 * general JSON parser, in the spirit of the gdb RSP stub), plus a small string
 * builder for the response. No allocator, no I/O; the operations and the stdio
 * transport are injected/wired by the adapter.
 */

#include "mcp.h"

/* ---------------- small string builder ---------------- */

typedef struct { char* buf; uint32_t cap; uint32_t len; bool ovf; } sb_t;

static void sb_init(sb_t* s, char* buf, uint32_t cap) {
    s->buf = buf; s->cap = cap; s->len = 0; s->ovf = false;
}
static void sb_putc(sb_t* s, char c) {
    if (s->len < s->cap) s->buf[s->len++] = c; else s->ovf = true;
}
static void sb_put(sb_t* s, const char* str) {
    while (*str) sb_putc(s, *str++);
}
static void sb_put_u64(sb_t* s, uint64_t v) {
    char tmp[20]; int n = 0;
    if (v == 0) { sb_putc(s, '0'); return; }
    while (v) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n--) sb_putc(s, tmp[n]);
}

/* ---------------- focused JSON scanners ---------------- */

static bool str_eq(const char* a, const char* b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) if (a[i] != b[i]) return false;
    return true;
}
static uint32_t lit_len(const char* s) { uint32_t n = 0; while (s[n]) n++; return n; }

/* Locate `"key"` in json; returns the index just past the closing quote, or -1. */
static int find_key(const char* json, uint32_t len, const char* key) {
    uint32_t klen = lit_len(key);
    if (klen + 2 > len) return -1;
    for (uint32_t i = 0; i + klen + 2 <= len; i++) {
        if (json[i] == '"' && str_eq(json + i + 1, key, klen) && json[i + 1 + klen] == '"')
            return (int)(i + 1 + klen + 1);
    }
    return -1;
}

/* Skip spaces and a single ':'. */
static uint32_t skip_colon(const char* json, uint32_t len, uint32_t i) {
    while (i < len && (json[i] == ' ' || json[i] == ':' || json[i] == '\t')) i++;
    return i;
}

bool mcp_json_int(const char* json, uint32_t len, const char* key, uint64_t* out) {
    int k = find_key(json, len, key);
    if (k < 0) return false;
    uint32_t i = skip_colon(json, len, (uint32_t)k);
    if (i >= len || json[i] < '0' || json[i] > '9') return false;
    uint64_t v = 0;
    while (i < len && json[i] >= '0' && json[i] <= '9') { v = v * 10 + (uint64_t)(json[i] - '0'); i++; }
    if (out) *out = v;
    return true;
}

bool mcp_json_str(const char* json, uint32_t len, const char* key,
                  char* out, uint32_t outcap) {
    int k = find_key(json, len, key);
    if (k < 0) return false;
    uint32_t i = skip_colon(json, len, (uint32_t)k);
    if (i >= len || json[i] != '"') return false;
    i++;
    uint32_t o = 0;
    while (i < len && json[i] != '"') {
        if (o + 1 < outcap) out[o++] = json[i];
        i++;
    }
    if (o < outcap) out[o] = 0;
    return true;
}

/* ---------------- response helpers ---------------- */

/* Open a JSON-RPC result envelope with the echoed id and a text content item. */
static void begin_text_result(sb_t* s, uint64_t id) {
    sb_put(s, "{\"jsonrpc\":\"2.0\",\"id\":");
    sb_put_u64(s, id);
    sb_put(s, ",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"");
}
static void end_text_result(sb_t* s, bool is_error) {
    sb_put(s, "\"}]");
    if (is_error) sb_put(s, ",\"isError\":true");
    sb_put(s, "}}");
}

/* ---------------- dispatch ---------------- */

static int finish(sb_t* s) { return s->ovf ? -1 : (int)s->len; }

int mcp_handle(const mcp_ops_t* ops, const char* request, uint32_t reqlen,
               char* out, uint32_t outcap) {
    uint64_t id = 0;
    mcp_json_int(request, reqlen, "id", &id);

    char method[32];
    if (!mcp_json_str(request, reqlen, "method", method, sizeof(method)))
        return -1;

    sb_t s; sb_init(&s, out, outcap);

    /* tools/list: advertise the time-travel tools. */
    if (str_eq(method, "tools/list", lit_len("tools/list")) &&
        lit_len(method) == lit_len("tools/list")) {
        sb_put(&s, "{\"jsonrpc\":\"2.0\",\"id\":");
        sb_put_u64(&s, id);
        sb_put(&s, ",\"result\":{\"tools\":[");
        sb_put(&s, "{\"name\":\"list_checkpoints\",\"description\":\"Retained keyframe window (rewind horizon).\"},");
        sb_put(&s, "{\"name\":\"rewind_to\",\"description\":\"Restore the nearest keyframe and replay to (epoch, offset).\"},");
        sb_put(&s, "{\"name\":\"reverse_step\",\"description\":\"Step one unit backward through recorded history.\"},");
        sb_put(&s, "{\"name\":\"watch_last_write\",\"description\":\"Find where a watched value last changed.\"}");
        sb_put(&s, "]}}");
        return finish(&s);
    }

    /* tools/call: dispatch on the tool name. */
    if (str_eq(method, "tools/call", lit_len("tools/call")) &&
        lit_len(method) == lit_len("tools/call")) {
        char name[32];
        if (!mcp_json_str(request, reqlen, "name", name, sizeof(name))) {
            begin_text_result(&s, id);
            sb_put(&s, "missing tool name");
            end_text_result(&s, true);
            return finish(&s);
        }
        uint32_t nlen = lit_len(name);

        if (nlen == lit_len("list_checkpoints") && str_eq(name, "list_checkpoints", nlen)) {
            uint64_t oldest = 0, newest = 0; uint32_t count = 0;
            int rc = ops && ops->list_checkpoints
                     ? ops->list_checkpoints(ops->ctx, &oldest, &newest, &count) : -1;
            begin_text_result(&s, id);
            if (rc == 0) {
                sb_put(&s, "checkpoints: count="); sb_put_u64(&s, count);
                sb_put(&s, " oldest="); sb_put_u64(&s, oldest);
                sb_put(&s, " newest="); sb_put_u64(&s, newest);
                end_text_result(&s, false);
            } else { sb_put(&s, "no checkpoints"); end_text_result(&s, true); }
            return finish(&s);
        }

        if (nlen == lit_len("rewind_to") && str_eq(name, "rewind_to", nlen)) {
            uint64_t epoch = 0, offset = 0;
            bool have = mcp_json_int(request, reqlen, "epoch", &epoch);
            mcp_json_int(request, reqlen, "offset", &offset);
            int rc = (have && ops && ops->rewind_to)
                     ? ops->rewind_to(ops->ctx, epoch, offset) : -1;
            begin_text_result(&s, id);
            if (rc == 0) {
                sb_put(&s, "rewound to epoch="); sb_put_u64(&s, epoch);
                sb_put(&s, " offset="); sb_put_u64(&s, offset);
                end_text_result(&s, false);
            } else { sb_put(&s, "rewind out of range"); end_text_result(&s, true); }
            return finish(&s);
        }

        if (nlen == lit_len("reverse_step") && str_eq(name, "reverse_step", nlen)) {
            uint64_t e = 0, o = 0;
            int rc = ops && ops->reverse_step ? ops->reverse_step(ops->ctx, &e, &o) : -1;
            begin_text_result(&s, id);
            if (rc == 0) {
                sb_put(&s, "stepped back to epoch="); sb_put_u64(&s, e);
                sb_put(&s, " offset="); sb_put_u64(&s, o);
                end_text_result(&s, false);
            } else { sb_put(&s, "at start of history"); end_text_result(&s, true); }
            return finish(&s);
        }

        if (nlen == lit_len("watch_last_write") && str_eq(name, "watch_last_write", nlen)) {
            uint64_t e = 0, o = 0;
            int rc = ops && ops->watch_last_write ? ops->watch_last_write(ops->ctx, &e, &o) : -1;
            begin_text_result(&s, id);
            if (rc == 0) {
                sb_put(&s, "last write at epoch="); sb_put_u64(&s, e);
                sb_put(&s, " offset="); sb_put_u64(&s, o);
                end_text_result(&s, false);
            } else { sb_put(&s, "no write within the retained window"); end_text_result(&s, true); }
            return finish(&s);
        }

        begin_text_result(&s, id);
        sb_put(&s, "unknown tool");
        end_text_result(&s, true);
        return finish(&s);
    }

    /* Unknown method: JSON-RPC method-not-found error. */
    sb_put(&s, "{\"jsonrpc\":\"2.0\",\"id\":");
    sb_put_u64(&s, id);
    sb_put(&s, ",\"error\":{\"code\":-32601,\"message\":\"method not found\"}}");
    return finish(&s);
}
