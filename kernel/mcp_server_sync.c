/* IKOS Orthogonal Persistence - MCP server transport adapter (#199)
 *
 * See include/mcp_server.h. Wires the pure JSON-RPC server loop to the 16550
 * UART and to the MCP tool surface: byte I/O is the serial data/status
 * registers, and the handle step is mcp_handle() over mcp_kernel_ops().
 * mcp_server_init binds the ops (mcp_bind), registers the keyframe ring (from
 * the keyframe store, #195) and a watch probe, then mcp_server_run serves the
 * loop until the client disconnects.
 */

#include "mcp_server.h"
#include "mcp.h"             /* mcp_bind, mcp_handle, mcp_kernel_ops, mcp_set_* */
#include "keyframe_store.h"  /* keyframe_store_get, keyframe_store_ring */
#include "checkpoint.h"      /* checkpoint_current_epoch (watch probe) */
#include "boot.h"            /* SERIAL_* register offsets, SERIAL_READY_BIT */
#include "io.h"              /* inb, outb */

/* COM2 base, so the MCP server does not collide with the gdb stub on COM1. */
#define SERIAL_COM2_BASE      0x2F8
/* Line Status Register bit 0: receive data ready. */
#define SERIAL_LSR_DATA_READY 0x01

extern int klog_serial_init(uint16_t port, uint32_t baud_rate);

static uint16_t g_mcp_port;

static int serial_get_byte(void* ctx) {
    (void)ctx;
    while ((inb(g_mcp_port + SERIAL_STATUS_PORT) & SERIAL_LSR_DATA_READY) == 0) {
        /* busy-wait for the receiver */
    }
    return (int)inb(g_mcp_port + SERIAL_DATA_PORT);
}

static int serial_put_byte(void* ctx, uint8_t b) {
    (void)ctx;
    while ((inb(g_mcp_port + SERIAL_STATUS_PORT) & SERIAL_READY_BIT) == 0) {
        /* busy-wait for the transmitter holding register */
    }
    outb(g_mcp_port + SERIAL_DATA_PORT, b);
    return 0;
}

static const mcp_transport_t g_transport = {
    serial_get_byte, serial_put_byte, 0
};

/* The handle step: dispatch one request through the bound kernel ops. */
static int mcp_server_handle(const char* request, uint32_t reqlen,
                             char* out, uint32_t outcap) {
    return mcp_handle(mcp_kernel_ops(), request, reqlen, out, outcap);
}

/* Watch probe: a real, monotonically-advancing kernel value the agent can hunt
 * with watch_last_write (where the checkpoint epoch last changed). */
static uint64_t probe_current_epoch(void* ctx) {
    (void)ctx;
    return checkpoint_current_epoch();
}

void mcp_server_init(uint16_t port) {
    g_mcp_port = port ? port : SERIAL_COM2_BASE;
    klog_serial_init(g_mcp_port, 38400); /* 8N1, FIFO on */

    mcp_bind();                          /* wire the tools to the k* engines */

    /* list_checkpoints reads the retained keyframe window; the ring lives in
     * the keyframe store (NULL when persistence is disabled, handled gracefully). */
    keyframe_store_t* ks = keyframe_store_get();
    mcp_set_ring(ks ? keyframe_store_ring(ks) : 0);

    /* watch_last_write needs a probe for the watched value. */
    mcp_set_watch_probe(probe_current_epoch, 0);
}

int mcp_server_run(void) {
    return mcp_server_loop(&g_transport, mcp_server_handle);
}
