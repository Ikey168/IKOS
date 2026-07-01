/* IKOS Orthogonal Persistence - GDB Reverse-Debugging Stub (#172, epic #159)
 *
 * A GDB Remote Serial Protocol (RSP) stub that exposes IKOS's reverse execution
 * (#170) to a normal gdb session. GDB drives reverse debugging with two packets:
 *   - "bs": reverse single-step
 *   - "bc": reverse continue
 * which gdb only sends after the target advertises ReverseStep+ / ReverseContinue+
 * in its qSupported reply. This stub advertises them and maps bs/bc onto the
 * reverse engine, so `reverse-step` and `reverse-continue` at the gdb prompt walk
 * IKOS backward through its recorded history.
 *
 * Note this is distinct from QEMU's own gdb server (`make debug`, qemu -s): that
 * debugs the emulated CPU forward. IKOS's reverse is a property of its replay
 * engine, so gdb connects to this in-kernel stub over the serial port instead.
 * See docs/testing/reverse-debugging.md.
 *
 * The core is pure and host-testable: RSP framing (checksum, ack) and packet
 * dispatch are here, and the reverse operations are injected. The kernel adapter
 * (gdbstub_sync.c) wires bs/bc to kreverse_step / kreverse_continue and the
 * transport to the serial port.
 */

#ifndef GDBSTUB_H
#define GDBSTUB_H

#include <stdint.h>
#include <stdbool.h>

/* Reverse operations the stub drives. Each returns 0 on success (the target is
 * reported stopped either way; bounds are handled inside). */
typedef struct {
    int (*reverse_step)(void* ctx);
    int (*reverse_continue)(void* ctx);
    void* ctx;
} gdbstub_ops_t;

/* RSP checksum: the 8-bit sum of the payload bytes. */
uint8_t gdbstub_checksum(const char* data, uint32_t len);

/* Frame a payload as "$<payload>#<cc>". Returns the framed length, or -1 if it
 * does not fit in `out`. */
int gdbstub_frame(const char* payload, uint32_t plen, char* out, uint32_t outcap);

/* Parse a framed packet "$<payload>#<cc>": copies the payload into `out` and
 * sets *ok to whether the checksum matched. Returns the payload length, or -1 on
 * a malformed frame. */
int gdbstub_unframe(const char* frame, uint32_t flen, char* out, uint32_t outcap,
                    bool* ok);

/* Handle one RSP packet payload (unframed) and produce the response payload
 * (unframed) in `out`. Recognizes qSupported (advertising the reverse packets),
 * "?", "bs" (reverse-step), and "bc" (reverse-continue); any other packet gets
 * an empty response, which gdb reads as "unsupported". Returns the response
 * length, or -1 if it does not fit. */
int gdbstub_handle(const gdbstub_ops_t* ops, const char* packet, uint32_t plen,
                   char* out, uint32_t outcap);

/* ---- Kernel adapter (gdbstub_sync.c) ---- */
void gdbstub_bind_reverse(void);
/* Serve one framed request: unframe, dispatch, and write the framed reply into
 * `out`. Returns the framed reply length, or -1. */
int  gdbstub_serve(const char* frame, uint32_t flen, char* out, uint32_t outcap);

#endif /* GDBSTUB_H */
