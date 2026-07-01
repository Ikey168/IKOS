/* IKOS Orthogonal Persistence v2 - IPC serialization core (#142)
 *
 * Pure serialize/deserialize/validate. No kernel deps. See
 * include/checkpoint_ipc.h.
 */

#include "checkpoint_ipc.h"

extern void* memcpy(void* dest, const void* src, unsigned long n);

typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t channel_count;
    uint32_t message_count;
    uint32_t reserved;
} ipc_blob_header_t;

uint32_t checkpoint_ipc_blob_size(uint32_t channel_count, uint32_t message_count) {
    return (uint32_t)sizeof(ipc_blob_header_t) +
           channel_count * (uint32_t)sizeof(checkpoint_ipc_channel_t) +
           message_count * (uint32_t)sizeof(checkpoint_ipc_message_t);
}

uint32_t checkpoint_ipc_serialize(const checkpoint_ipc_t* t, void* buf, uint32_t bufsize) {
    if (!t || !buf ||
        t->channel_count > CHECKPOINT_IPC_MAX_CHANNELS ||
        t->message_count > CHECKPOINT_IPC_MAX_MESSAGES) {
        return 0;
    }
    uint32_t need = checkpoint_ipc_blob_size(t->channel_count, t->message_count);
    if (bufsize < need) {
        return 0;
    }

    ipc_blob_header_t h;
    h.magic = CHECKPOINT_IPC_MAGIC;
    h.version = CHECKPOINT_IPC_VERSION;
    h.channel_count = t->channel_count;
    h.message_count = t->message_count;
    h.reserved = 0;

    uint8_t* p = (uint8_t*)buf;
    memcpy(p, &h, sizeof(h));
    p += sizeof(h);
    memcpy(p, t->channels, t->channel_count * sizeof(checkpoint_ipc_channel_t));
    p += t->channel_count * sizeof(checkpoint_ipc_channel_t);
    memcpy(p, t->messages, t->message_count * sizeof(checkpoint_ipc_message_t));
    return need;
}

bool checkpoint_ipc_deserialize(checkpoint_ipc_t* t, const void* buf, uint32_t size) {
    if (!t || !buf || size < sizeof(ipc_blob_header_t)) {
        return false;
    }
    ipc_blob_header_t h;
    memcpy(&h, buf, sizeof(h));
    if (h.magic != CHECKPOINT_IPC_MAGIC ||
        h.version != CHECKPOINT_IPC_VERSION ||
        h.channel_count > CHECKPOINT_IPC_MAX_CHANNELS ||
        h.message_count > CHECKPOINT_IPC_MAX_MESSAGES) {
        return false;
    }
    if (size < checkpoint_ipc_blob_size(h.channel_count, h.message_count)) {
        return false;
    }

    const uint8_t* p = (const uint8_t*)buf + sizeof(h);
    t->channel_count = h.channel_count;
    t->message_count = h.message_count;
    memcpy(t->channels, p, h.channel_count * sizeof(checkpoint_ipc_channel_t));
    p += h.channel_count * sizeof(checkpoint_ipc_channel_t);
    memcpy(t->messages, p, h.message_count * sizeof(checkpoint_ipc_message_t));
    return true;
}

const checkpoint_ipc_channel_t* checkpoint_ipc_find_channel(const checkpoint_ipc_t* t,
                                                            uint32_t channel_id) {
    if (!t) {
        return 0;
    }
    for (uint32_t i = 0; i < t->channel_count; i++) {
        if (t->channels[i].channel_id == channel_id) {
            return &t->channels[i];
        }
    }
    return 0;
}

bool checkpoint_ipc_validate(const checkpoint_ipc_t* t) {
    if (!t ||
        t->channel_count > CHECKPOINT_IPC_MAX_CHANNELS ||
        t->message_count > CHECKPOINT_IPC_MAX_MESSAGES) {
        return false;
    }
    for (uint32_t i = 0; i < t->channel_count; i++) {
        if (t->channels[i].channel_id == 0) {
            return false;
        }
        for (uint32_t j = i + 1; j < t->channel_count; j++) {
            if (t->channels[j].channel_id == t->channels[i].channel_id) {
                return false; /* channel ids unique */
            }
        }
    }
    for (uint32_t i = 0; i < t->message_count; i++) {
        const checkpoint_ipc_message_t* m = &t->messages[i];
        if (m->data_size > CHECKPOINT_IPC_DATA_MAX) {
            return false;
        }
        if (!checkpoint_ipc_find_channel(t, m->channel_id)) {
            return false; /* every message belongs to a present channel */
        }
    }
    return true;
}
