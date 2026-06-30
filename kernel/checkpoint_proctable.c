/* IKOS Orthogonal Persistence v2 - Process-table serialization core (#140)
 *
 * Pure serialize/deserialize/validate. No kernel deps. See
 * include/checkpoint_proctable.h.
 */

#include "checkpoint_proctable.h"

extern void* memcpy(void* dest, const void* src, unsigned long n);

/* On-disk blob header: magic + version + record count, followed by `count`
 * checkpoint_proc_record_t. Both serialize and deserialize use the same struct
 * layout (IKOS is x86_64-only; the superblock kernel-version stamp guards format
 * changes). */
typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t count;
} proctable_blob_header_t;

uint32_t checkpoint_proctable_blob_size(uint32_t count) {
    return (uint32_t)sizeof(proctable_blob_header_t) +
           count * (uint32_t)sizeof(checkpoint_proc_record_t);
}

uint32_t checkpoint_proctable_serialize(const checkpoint_proctable_t* t,
                                        void* buf, uint32_t bufsize) {
    if (!t || !buf || t->count > CHECKPOINT_PROCTABLE_MAX) {
        return 0;
    }
    uint32_t need = checkpoint_proctable_blob_size(t->count);
    if (bufsize < need) {
        return 0;
    }

    proctable_blob_header_t h;
    h.magic = CHECKPOINT_PROCTABLE_MAGIC;
    h.version = CHECKPOINT_PROCTABLE_VERSION;
    h.count = t->count;

    uint8_t* p = (uint8_t*)buf;
    memcpy(p, &h, sizeof(h));
    memcpy(p + sizeof(h), t->procs, t->count * sizeof(checkpoint_proc_record_t));
    return need;
}

bool checkpoint_proctable_deserialize(checkpoint_proctable_t* t,
                                      const void* buf, uint32_t size) {
    if (!t || !buf || size < sizeof(proctable_blob_header_t)) {
        return false;
    }
    proctable_blob_header_t h;
    memcpy(&h, buf, sizeof(h));
    if (h.magic != CHECKPOINT_PROCTABLE_MAGIC ||
        h.version != CHECKPOINT_PROCTABLE_VERSION ||
        h.count > CHECKPOINT_PROCTABLE_MAX) {
        return false;
    }
    if (size < checkpoint_proctable_blob_size(h.count)) {
        return false;
    }

    const uint8_t* p = (const uint8_t*)buf;
    memcpy(t->procs, p + sizeof(h), h.count * sizeof(checkpoint_proc_record_t));
    t->count = h.count;
    return true;
}

const checkpoint_proc_record_t* checkpoint_proctable_find(const checkpoint_proctable_t* t,
                                                          uint32_t pid) {
    if (!t) {
        return 0;
    }
    for (uint32_t i = 0; i < t->count; i++) {
        if (t->procs[i].pid == pid) {
            return &t->procs[i];
        }
    }
    return 0;
}

bool checkpoint_proctable_validate(const checkpoint_proctable_t* t) {
    if (!t || t->count > CHECKPOINT_PROCTABLE_MAX) {
        return false;
    }
    for (uint32_t i = 0; i < t->count; i++) {
        const checkpoint_proc_record_t* r = &t->procs[i];
        if (r->pid == 0) {
            return false; /* pids are nonzero */
        }
        /* unique pids */
        for (uint32_t j = i + 1; j < t->count; j++) {
            if (t->procs[j].pid == r->pid) {
                return false;
            }
        }
        /* every nonzero relationship resolves to a present pid */
        if (r->ppid && !checkpoint_proctable_find(t, r->ppid)) {
            return false;
        }
        if (r->first_child_pid && !checkpoint_proctable_find(t, r->first_child_pid)) {
            return false;
        }
        if (r->next_sibling_pid && !checkpoint_proctable_find(t, r->next_sibling_pid)) {
            return false;
        }
    }
    return true;
}
