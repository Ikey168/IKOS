/* IKOS Orthogonal Persistence - GDB serial transport adapter (#198)
 *
 * See include/gdb_serial.h. Wires the pure transport loop to the 16550 UART and
 * to the RSP stub: byte I/O is the serial data/status registers, and the serve
 * step is gdbstub_serve. gdbstub_serial_init registers the reverse ops
 * (gdbstub_bind_reverse) so bs/bc from a gdb session reach the reverse engine,
 * then gdbstub_serial_run serves the loop until gdb disconnects.
 */

#include "gdb_serial.h"
#include "gdbstub.h"   /* gdbstub_bind_reverse, gdbstub_serve */
#include "boot.h"      /* SERIAL_COM1_BASE, SERIAL_DATA_PORT, SERIAL_STATUS_PORT,
                          SERIAL_READY_BIT */
#include "io.h"        /* inb, outb */

/* Line Status Register bit 0: receive data ready. */
#define SERIAL_LSR_DATA_READY 0x01

/* UART configuration helper provided by kernel_log.c (8N1, FIFO enabled). */
extern int klog_serial_init(uint16_t port, uint32_t baud_rate);

static uint16_t g_gdb_port;

/* Block until a byte is available, then read it from the data register. */
static int serial_get_byte(void* ctx) {
    (void)ctx;
    while ((inb(g_gdb_port + SERIAL_STATUS_PORT) & SERIAL_LSR_DATA_READY) == 0) {
        /* busy-wait for the receiver */
    }
    return (int)inb(g_gdb_port + SERIAL_DATA_PORT);
}

/* Block until the transmitter is ready, then write one byte. */
static int serial_put_byte(void* ctx, uint8_t b) {
    (void)ctx;
    while ((inb(g_gdb_port + SERIAL_STATUS_PORT) & SERIAL_READY_BIT) == 0) {
        /* busy-wait for the transmitter holding register */
    }
    outb(g_gdb_port + SERIAL_DATA_PORT, b);
    return 0;
}

static const gdb_serial_ops_t g_serial_ops = {
    serial_get_byte, serial_put_byte, 0
};

void gdbstub_serial_init(uint16_t port) {
    g_gdb_port = port ? port : SERIAL_COM1_BASE;
    klog_serial_init(g_gdb_port, 38400); /* 8N1, FIFO on */
    gdbstub_bind_reverse();              /* bs/bc -> reverse engine (#172) */
}

int gdbstub_serial_run(void) {
    return gdb_serial_loop(&g_serial_ops, gdbstub_serve);
}
