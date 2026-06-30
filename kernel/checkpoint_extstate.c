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
