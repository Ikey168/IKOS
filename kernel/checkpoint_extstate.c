/* IKOS Orthogonal Persistence - External / non-persistable state policy
 *
 * See include/checkpoint_extstate.h and
 * docs/architecture/orthogonal-persistence.md. Issue #118 (epic #121).
 *
 * Dependency-free policy core: no kernel headers, so it builds both in the
 * freestanding kernel and in the host test harness unchanged.
 */

#include "checkpoint_extstate.h"

bool extstate_survives_checkpoint(extstate_kind_t kind) {
    switch (kind) {
        case EXTSTATE_REGULAR_FILE:
            return true;  /* bytes live in the persistent FS; reopen + seek */
        case EXTSTATE_SOCKET:
        case EXTSTATE_DMA_BUFFER:
        case EXTSTATE_DEVICE:
        case EXTSTATE_PIPE:
            return false; /* external/hardware state cannot be replayed */
    }
    /* Unknown kinds are treated as non-persistable: fail safe (sever) rather
     * than hand a process a stale handle to state we cannot vouch for. */
    return false;
}

bool extstate_sever(extstate_resource_t* res) {
    if (!res) {
        return false;
    }
    /* Only open handles matter, and only if their kind cannot survive. */
    if (!(res->flags & EXTSTATE_FLAG_OPEN)) {
        return false;
    }
    if (res->flags & EXTSTATE_FLAG_SEVERED) {
        return false; /* already severed: idempotent */
    }
    if (extstate_survives_checkpoint(res->kind)) {
        return false; /* persistable: leave intact */
    }
    res->flags |= EXTSTATE_FLAG_SEVERED;
    return true;
}

int extstate_sever_all(extstate_resource_t* resources, int count) {
    if (!resources || count <= 0) {
        return 0;
    }
    int severed = 0;
    for (int i = 0; i < count; i++) {
        if (extstate_sever(&resources[i])) {
            severed++;
        }
    }
    return severed;
}

int extstate_status(const extstate_resource_t* res) {
    if (res && (res->flags & EXTSTATE_FLAG_SEVERED)) {
        return EXTSTATE_SEVERED;
    }
    return EXTSTATE_OK;
}

/* ----- File-descriptor integration (#128) ----- */

void extstate_fd_set_kind(uint32_t* flags, extstate_kind_t kind) {
    if (!flags) {
        return;
    }
    *flags = (*flags & ~EXTSTATE_FD_KIND_MASK) |
             (((uint32_t)kind << EXTSTATE_FD_KIND_SHIFT) & EXTSTATE_FD_KIND_MASK);
}

extstate_kind_t extstate_kind_from_fd_flags(uint32_t flags) {
    return (extstate_kind_t)((flags & EXTSTATE_FD_KIND_MASK) >> EXTSTATE_FD_KIND_SHIFT);
}

bool extstate_sever_fd(uint32_t* flags) {
    if (!flags) {
        return false;
    }
    if (*flags & EXTSTATE_FD_SEVERED) {
        return false; /* already severed: idempotent */
    }
    if (extstate_survives_checkpoint(extstate_kind_from_fd_flags(*flags))) {
        return false; /* persistable (e.g. a regular file): leave intact */
    }
    *flags |= EXTSTATE_FD_SEVERED;
    return true;
}

bool extstate_fd_is_severed(uint32_t flags) {
    return (flags & EXTSTATE_FD_SEVERED) != 0;
}

/* ----- DMA / IPC endpoints and the reconnection contract (v2, #146) ----- */

bool extstate_endpoint_persistable(const extstate_endpoint_t* ep) {
    if (!ep) {
        return false; /* fail safe: nothing to vouch for */
    }
    return extstate_survives_checkpoint(ep->kind);
}

bool extstate_endpoint_sever(extstate_endpoint_t* ep) {
    if (!ep) {
        return false;
    }
    if (!(ep->flags & EXTSTATE_FLAG_OPEN)) {
        return false; /* not in use */
    }
    if (ep->flags & EXTSTATE_FLAG_SEVERED) {
        return false; /* already severed: idempotent */
    }
    if (extstate_survives_checkpoint(ep->kind)) {
        return false; /* persistable: leave intact */
    }
    ep->flags |= EXTSTATE_FLAG_SEVERED;
    ep->flags &= ~EXTSTATE_FLAG_RECONNECTED; /* a fresh sever supersedes any prior reconnect */
    return true;
}

int extstate_endpoint_sever_all(extstate_endpoint_t* eps, int count) {
    if (!eps || count <= 0) {
        return 0;
    }
    int severed = 0;
    for (int i = 0; i < count; i++) {
        if (extstate_endpoint_sever(&eps[i])) {
            severed++;
        }
    }
    return severed;
}

int extstate_endpoint_status(const extstate_endpoint_t* ep) {
    if (ep && (ep->flags & EXTSTATE_FLAG_SEVERED)) {
        return EXTSTATE_SEVERED;
    }
    return EXTSTATE_OK;
}

bool extstate_endpoint_needs_reconnect(const extstate_endpoint_t* ep) {
    return ep && (ep->flags & EXTSTATE_FLAG_SEVERED) != 0;
}

bool extstate_endpoint_reconnect(extstate_endpoint_t* ep) {
    if (!ep) {
        return false;
    }
    if (!(ep->flags & EXTSTATE_FLAG_SEVERED)) {
        return false; /* nothing to re-establish: safe no-op */
    }
    ep->flags &= ~EXTSTATE_FLAG_SEVERED;
    ep->flags |= EXTSTATE_FLAG_OPEN | EXTSTATE_FLAG_RECONNECTED;
    ep->generation++;
    return true;
}
