/* IKOS Orthogonal Persistence v2 - Driver re-attach framework (#143)
 *
 * Pure ordering engine + registry. No kernel deps. See
 * include/checkpoint_driver.h.
 */

#include "checkpoint_driver.h"

void checkpoint_driver_registry_init(checkpoint_driver_registry_t* r) {
    if (!r) {
        return;
    }
    r->count = 0;
    for (uint32_t i = 0; i < CHECKPOINT_DRIVER_MAX; i++) {
        r->entries[i].dev = 0;
        r->entries[i].ops.quiesce = 0;
        r->entries[i].ops.reattach = 0;
        r->entries[i].ops.persistable = false;
        r->entries[i].severed = false;
    }
}

int checkpoint_driver_register(checkpoint_driver_registry_t* r,
                               void* dev, const checkpoint_persist_ops_t* ops) {
    if (!r || !ops || r->count >= CHECKPOINT_DRIVER_MAX) {
        return -1;
    }
    checkpoint_driver_entry_t* e = &r->entries[r->count++];
    e->dev = dev;
    e->ops = *ops;
    e->severed = false;
    return 0;
}

uint32_t checkpoint_driver_count(const checkpoint_driver_registry_t* r) {
    return r ? r->count : 0;
}

int checkpoint_driver_quiesce_all(checkpoint_driver_registry_t* r) {
    if (!r) {
        return 0;
    }
    int ok = 0;
    for (uint32_t i = 0; i < r->count; i++) {
        checkpoint_persist_ops_t* o = &r->entries[i].ops;
        if (!o->quiesce) {
            ok++; /* nothing to do counts as success */
            continue;
        }
        if (o->quiesce(r->entries[i].dev) == 0) {
            ok++;
        }
        /* A failed quiesce does not stop the others: we still try to reach a
         * safe boundary for the rest. */
    }
    return ok;
}

int checkpoint_driver_reattach_all(checkpoint_driver_registry_t* r,
                                   checkpoint_reattach_mode_t mode) {
    if (!r) {
        return 0;
    }
    int reattached = 0;
    for (uint32_t i = 0; i < r->count; i++) {
        checkpoint_driver_entry_t* e = &r->entries[i];
        e->severed = false;

        int rc = e->ops.reattach ? e->ops.reattach(e->dev) : 0;
        if (rc == 0) {
            reattached++;
            continue;
        }

        if (mode == CHECKPOINT_REATTACH_ABORT) {
            return CHECKPOINT_DRIVER_ABORTED; /* remaining devices not attempted */
        }
        /* CONTINUE: sever this device and keep going. */
        e->severed = true;
        e->ops.persistable = false;
    }
    return reattached;
}

bool checkpoint_driver_is_severed(const checkpoint_driver_registry_t* r, uint32_t index) {
    if (!r || index >= r->count) {
        return false;
    }
    return r->entries[index].severed;
}

checkpoint_driver_registry_t* checkpoint_driver_global(void) {
    static checkpoint_driver_registry_t g_registry;
    static bool initialized = false;
    if (!initialized) {
        checkpoint_driver_registry_init(&g_registry);
        initialized = true;
    }
    return &g_registry;
}
