/* IKOS Orthogonal Persistence - MCP Time-Travel Interface (#159 Milestone E)
 *
 * Exposes IKOS's time-travel primitives as Model Context Protocol tools, so an
 * AI agent can drive record / rewind / reverse execution over JSON-RPC. Agents
 * are poor at exactly what this stack is good at: reproducing a non-deterministic
 * bug, stepping backward, and keeping the world stable between runs. Giving the
 * agent rewind_to / reverse_step / watch_last_write as callable tools turns it
 * into a time-traveling debugger.
 *
 * The tool surface:
 *   - list_checkpoints : the retained keyframe window (rewind horizon)
 *   - rewind_to        : restore the nearest keyframe and replay to (epoch,offset)
 *   - reverse_step     : step one unit backward
 *   - watch_last_write : find where a watched value last changed
 *
 * The core is pure and host-testable: it parses a JSON-RPC request, dispatches
 * to injected operations (backed by the rewind #169 / reverse #170 / reverse
 * watchpoint #171 engines), and formats the JSON-RPC response. The kernel
 * adapter (mcp_sync.c) wires the operations to the k* time-travel entry points;
 * the stdio JSON-RPC server loop is the thin transport documented alongside it.
 *
 * See docs/architecture/time-travel.md (Plan 2).
 */

#ifndef MCP_H
#define MCP_H

#include <stdint.h>
#include <stdbool.h>

/* Time-travel operations the tools drive. Each returns 0 on success. Position
 * results are written to *out_epoch / *out_offset. */
typedef struct {
    int (*list_checkpoints)(void* ctx, uint64_t* oldest, uint64_t* newest, uint32_t* count);
    int (*rewind_to)(void* ctx, uint64_t epoch, uint64_t offset);
    int (*reverse_step)(void* ctx, uint64_t* out_epoch, uint64_t* out_offset);
    int (*watch_last_write)(void* ctx, uint64_t* out_epoch, uint64_t* out_offset);
    void* ctx;
} mcp_ops_t;

/* ---- JSON helpers (exposed for tests) ---- */

/* Find integer field "key": <digits> in a JSON object. Returns true if found. */
bool mcp_json_int(const char* json, uint32_t len, const char* key, uint64_t* out);

/* Find string field "key": "value"; copies value into out. Returns true. */
bool mcp_json_str(const char* json, uint32_t len, const char* key,
                  char* out, uint32_t outcap);

/* Handle one JSON-RPC request and write the JSON-RPC response into out.
 * Recognizes the "tools/list" method (returns the tool schema) and "tools/call"
 * (dispatches params.name to the matching operation). Returns the response
 * length, or -1 if it does not fit in out. */
int mcp_handle(const mcp_ops_t* ops, const char* request, uint32_t reqlen,
               char* out, uint32_t outcap);

/* ---- Kernel adapter (mcp_sync.c) ----
 * Build an mcp_ops_t wired to the kernel's time-travel entry points. The
 * watchpoint needs a probe for the watched value, registered separately. */
typedef uint64_t (*mcp_probe_fn)(void* ctx);
void          mcp_bind(void);
void          mcp_set_ring(const void* keyframe_ring);   /* for list_checkpoints */
void          mcp_set_watch_probe(mcp_probe_fn probe, void* ctx);
const mcp_ops_t* mcp_kernel_ops(void);

#endif /* MCP_H */
