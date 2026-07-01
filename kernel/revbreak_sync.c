/* IKOS Orthogonal Persistence - Reverse Breakpoints/Watchpoints adapter (#171)
 *
 * Binds the reverse breakpoint / watchpoint operations (revbreak.c) to a reverse
 * context for the debugger front end (the GDB bridge in #172).
 */

#include "revbreak.h"

static reverse_ctx_t* g_rv;

void krevbreak_bind(reverse_ctx_t* rv) {
    g_rv = rv;
}

int krevbreak_breakpoint(revbreak_cond_fn cond, void* ctx, reverse_pos_t* hit) {
    if (!g_rv) return REVBREAK_ERR_PARAM;
    return reverse_breakpoint(g_rv, cond, ctx, hit);
}

int krevbreak_watchpoint(revbreak_probe_fn probe, void* ctx, reverse_pos_t* hit) {
    if (!g_rv) return REVBREAK_ERR_PARAM;
    return reverse_watchpoint(g_rv, probe, ctx, hit);
}
