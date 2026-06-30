/* IKOS Orthogonal Persistence v2 - Open-file table serialization core (#141)
 *
 * Pure serialize/deserialize/validate. No kernel deps. See
 * include/checkpoint_filetable.h.
 */

#include "checkpoint_filetable.h"

extern void* memcpy(void* dest, const void* src, unsigned long n);

typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t pid;
    uint32_t count;
    uint32_t reserved;
} filetable_blob_header_t;

uint32_t checkpoint_filetable_blob_size(uint32_t count) {
    return (uint32_t)sizeof(filetable_blob_header_t) +
           count * (uint32_t)sizeof(checkpoint_file_record_t);
}

uint32_t checkpoint_filetable_serialize(const checkpoint_filetable_t* t,
                                        void* buf, uint32_t bufsize) {
    if (!t || !buf || t->count > CHECKPOINT_FILETABLE_MAX) {
        return 0;
    }
    uint32_t need = checkpoint_filetable_blob_size(t->count);
    if (bufsize < need) {
        return 0;
    }

    filetable_blob_header_t h;
    h.magic = CHECKPOINT_FILETABLE_MAGIC;
    h.version = CHECKPOINT_FILETABLE_VERSION;
    h.pid = t->pid;
    h.count = t->count;
    h.reserved = 0;

    uint8_t* p = (uint8_t*)buf;
    memcpy(p, &h, sizeof(h));
    memcpy(p + sizeof(h), t->records, t->count * sizeof(checkpoint_file_record_t));
    return need;
}

bool checkpoint_filetable_deserialize(checkpoint_filetable_t* t,
                                      const void* buf, uint32_t size) {
    if (!t || !buf || size < sizeof(filetable_blob_header_t)) {
        return false;
    }
    filetable_blob_header_t h;
    memcpy(&h, buf, sizeof(h));
    if (h.magic != CHECKPOINT_FILETABLE_MAGIC ||
        h.version != CHECKPOINT_FILETABLE_VERSION ||
        h.count > CHECKPOINT_FILETABLE_MAX) {
        return false;
    }
    if (size < checkpoint_filetable_blob_size(h.count)) {
        return false;
    }

    const uint8_t* p = (const uint8_t*)buf;
    t->pid = h.pid;
    t->count = h.count;
    memcpy(t->records, p + sizeof(h), h.count * sizeof(checkpoint_file_record_t));
    return true;
}

const checkpoint_file_record_t* checkpoint_filetable_find(const checkpoint_filetable_t* t,
                                                          uint32_t fd) {
    if (!t) {
        return 0;
    }
    for (uint32_t i = 0; i < t->count; i++) {
        if (t->records[i].fd == fd) {
            return &t->records[i];
        }
    }
    return 0;
}

static bool path_is_terminated(const char* path) {
    for (uint32_t i = 0; i < CHECKPOINT_FILE_PATH_MAX; i++) {
        if (path[i] == '\0') {
            return true;
        }
    }
    return false;
}

bool checkpoint_filetable_validate(const checkpoint_filetable_t* t) {
    if (!t || t->count > CHECKPOINT_FILETABLE_MAX) {
        return false;
    }
    for (uint32_t i = 0; i < t->count; i++) {
        const checkpoint_file_record_t* r = &t->records[i];
        if (!path_is_terminated(r->path)) {
            return false; /* path must be a valid C string */
        }
        for (uint32_t j = i + 1; j < t->count; j++) {
            if (t->records[j].fd == r->fd) {
                return false; /* fds unique within a process */
            }
        }
    }
    return true;
}
