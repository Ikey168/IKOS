/* IKOS Orthogonal Persistence - MCP Time-Travel Interface, kernel adapter
 * (#159 Milestone E)
 *
 * Wires the MCP tool operations (mcp.c) to the kernel's time-travel entry
 * points: list_checkpoints reads the keyframe ring (#168), rewind_to calls
 * krewind_to (#169), reverse_step calls kreverse_step (#170), and
 * watch_last_write calls krevbreak_watchpoint (#171) with a registered probe.
 *
 * A stdio JSON-RPC server loop reads a request, calls mcp_handle() with these
 * ops, and writes the response; that transport is the only remaining wiring to
 * expose the tools to a real MCP client.
 */

#include "mcp.h"
#include "keyframe_ring.h"
#include "rewind.h"
#include "reverse.h"
#include "revbreak.h"

static const keyframe_ring_t* g_ring;
static mcp_probe_fn           g_probe;
static void*                  g_probe_ctx;

static int op_list(void* c, uint64_t* oldest, uint64_t* newest, uint32_t* count) {
    (void)c;
    if (!g_ring) return -1;
    if (!keyframe_ring_oldest(g_ring, oldest)) return -1;
    keyframe_ring_newest(g_ring, newest);
    *count = keyframe_ring_count(g_ring);
    return 0;
}

static int op_rewind(void* c, uint64_t epoch, uint64_t offset) {
    (void)c;
    return krewind_to(epoch, offset) == REWIND_OK ? 0 : -1;
}

static int op_reverse_step(void* c, uint64_t* out_epoch, uint64_t* out_offset) {
    (void)c;
    int rc = kreverse_step();
    reverse_pos_t p = kreverse_position();
    if (out_epoch) *out_epoch = p.epoch;
    if (out_offset) *out_offset = p.offset;
    return rc == REVERSE_OK ? 0 : -1;
}

static int op_watch(void* c, uint64_t* out_epoch, uint64_t* out_offset) {
    (void)c;
    if (!g_probe) return -1;
    reverse_pos_t hit;
    if (krevbreak_watchpoint(g_probe, g_probe_ctx, &hit) != REVBREAK_OK) return -1;
    if (out_epoch) *out_epoch = hit.epoch;
    if (out_offset) *out_offset = hit.offset;
    return 0;
}

static mcp_ops_t g_ops;

void mcp_bind(void) {
    g_ops.list_checkpoints = op_list;
    g_ops.rewind_to = op_rewind;
    g_ops.reverse_step = op_reverse_step;
    g_ops.watch_last_write = op_watch;
    g_ops.ctx = 0;
}

void mcp_set_ring(const void* keyframe_ring) {
    g_ring = (const keyframe_ring_t*)keyframe_ring;
}

void mcp_set_watch_probe(mcp_probe_fn probe, void* ctx) {
    g_probe = probe;
    g_probe_ctx = ctx;
}

const mcp_ops_t* mcp_kernel_ops(void) {
    return &g_ops;
}
