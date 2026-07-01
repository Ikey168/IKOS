/* IKOS Orthogonal Persistence - GDB Reverse-Debugging Stub (#172, epic #159)
 *
 * See include/gdbstub.h and docs/testing/reverse-debugging.md.
 *
 * Pure RSP core: framing, checksum, and packet dispatch. No allocator, no
 * hardware; the reverse operations and the transport are injected/wired by the
 * adapter.
 */

#include "gdbstub.h"

static uint32_t lit_len(const char* s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

static bool eq_lit(const char* p, uint32_t plen, const char* lit) {
    uint32_t n = lit_len(lit);
    if (plen != n) return false;
    for (uint32_t i = 0; i < n; i++) if (p[i] != lit[i]) return false;
    return true;
}

static bool has_prefix(const char* p, uint32_t plen, const char* lit) {
    uint32_t n = lit_len(lit);
    if (plen < n) return false;
    for (uint32_t i = 0; i < n; i++) if (p[i] != lit[i]) return false;
    return true;
}

/* Copy a NUL-terminated literal into out; returns length or -1 if it overflows. */
static int emit(char* out, uint32_t cap, const char* s) {
    uint32_t n = lit_len(s);
    if (n > cap) return -1;
    for (uint32_t i = 0; i < n; i++) out[i] = s[i];
    return (int)n;
}

static char hex_digit(int v) {
    return (char)(v < 10 ? '0' + v : 'a' + (v - 10));
}
static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

uint8_t gdbstub_checksum(const char* data, uint32_t len) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < len; i++) sum += (uint8_t)data[i];
    return (uint8_t)(sum & 0xFF);
}

int gdbstub_frame(const char* payload, uint32_t plen, char* out, uint32_t outcap) {
    uint32_t need = plen + 4; /* '$' + payload + '#' + two hex */
    if (need > outcap) return -1;
    out[0] = '$';
    for (uint32_t i = 0; i < plen; i++) out[1 + i] = payload[i];
    out[1 + plen] = '#';
    uint8_t c = gdbstub_checksum(payload, plen);
    out[2 + plen] = hex_digit((c >> 4) & 0xF);
    out[3 + plen] = hex_digit(c & 0xF);
    return (int)need;
}

int gdbstub_unframe(const char* frame, uint32_t flen, char* out, uint32_t outcap,
                    bool* ok) {
    if (ok) *ok = false;
    if (flen < 4 || frame[0] != '$') return -1;

    /* Find the '#' that ends the payload. */
    uint32_t hash = 0;
    bool found = false;
    for (uint32_t i = 1; i < flen; i++) {
        if (frame[i] == '#') { hash = i; found = true; break; }
    }
    if (!found || hash + 2 >= flen) return -1;

    uint32_t plen = hash - 1;
    if (plen > outcap) return -1;
    for (uint32_t i = 0; i < plen; i++) out[i] = frame[1 + i];

    int hi = hex_val(frame[hash + 1]);
    int lo = hex_val(frame[hash + 2]);
    if (hi < 0 || lo < 0) return -1;
    uint8_t want = (uint8_t)((hi << 4) | lo);
    if (ok) *ok = (want == gdbstub_checksum(out, plen));
    return (int)plen;
}

int gdbstub_handle(const gdbstub_ops_t* ops, const char* packet, uint32_t plen,
                   char* out, uint32_t outcap) {
    /* Feature negotiation: advertise the reverse-execution packets so gdb will
     * send bs/bc for reverse-step / reverse-continue. */
    if (has_prefix(packet, plen, "qSupported"))
        return emit(out, outcap, "PacketSize=4000;ReverseStep+;ReverseContinue+");

    /* Halt reason. */
    if (eq_lit(packet, plen, "?"))
        return emit(out, outcap, "S05");

    /* Reverse single-step. */
    if (eq_lit(packet, plen, "bs")) {
        if (ops && ops->reverse_step) ops->reverse_step(ops->ctx);
        return emit(out, outcap, "S05");
    }

    /* Reverse continue. */
    if (eq_lit(packet, plen, "bc")) {
        if (ops && ops->reverse_continue) ops->reverse_continue(ops->ctx);
        return emit(out, outcap, "S05");
    }

    /* Anything else: empty response => gdb treats the packet as unsupported. */
    (void)outcap;
    return 0;
}
