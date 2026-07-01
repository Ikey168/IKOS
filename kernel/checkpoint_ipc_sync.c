/* IKOS Orthogonal Persistence v2 - IPC capture/restore adapter (#142)
 *
 * Bridges the process manager's pm_ipc_* API to the portable serialization core
 * (checkpoint_ipc.c) and the kernel-state blob mechanism (#139). Kernel only;
 * the host tests cover the pure core, this adapter is validated by the build.
 */

#include "checkpoint_ipc.h"
#include "checkpoint.h"
#include "process_manager.h"
#include <stddef.h>

extern void* kmalloc(size_t size);
extern void  kfree(void* ptr);
extern void* memcpy(void* dest, const void* src, size_t n);
extern void* memset(void* dest, int value, size_t n);

int checkpoint_capture_ipc(void) {
    checkpoint_ipc_t* t = (checkpoint_ipc_t*)kmalloc(sizeof(*t));
    if (!t) {
        return -1;
    }
    t->channel_count = 0;
    t->message_count = 0;

    /* Walk the active channels; drain each queue to a quiescent point (the
     * drained messages are persisted here and re-enqueued on restore). */
    for (uint32_t id = 1;
         id <= PM_MAX_IPC_CHANNELS && t->channel_count < CHECKPOINT_IPC_MAX_CHANNELS;
         id++) {
        pm_ipc_channel_t info;
        if (pm_ipc_get_channel_info(id, &info) != 0 || !info.is_active) {
            continue;
        }

        checkpoint_ipc_channel_t* c = &t->channels[t->channel_count++];
        c->channel_id = info.channel_id;
        c->owner_pid = info.owner_pid;
        c->permissions = info.permissions;
        c->active = 1;
        c->reserved[0] = c->reserved[1] = c->reserved[2] = 0;

        while (pm_ipc_channel_has_messages(id) &&
               t->message_count < CHECKPOINT_IPC_MAX_MESSAGES) {
            pm_ipc_message_t m;
            if (pm_ipc_receive_message(info.owner_pid, id, &m) != 0) {
                break;
            }
            checkpoint_ipc_message_t* r = &t->messages[t->message_count++];
            r->channel_id = m.channel_id;
            r->type = (uint32_t)m.type;
            r->src_pid = m.src_pid;
            r->dst_pid = m.dst_pid;
            r->message_id = m.message_id;
            r->data_size = m.data_size;
            r->flags = m.flags;
            r->reserved = 0;
            r->timestamp = m.timestamp;
            uint32_t n = m.data_size <= CHECKPOINT_IPC_DATA_MAX
                             ? m.data_size : CHECKPOINT_IPC_DATA_MAX;
            memset(r->data, 0, CHECKPOINT_IPC_DATA_MAX);
            memcpy(r->data, m.data, n);
        }
    }

    uint32_t bufsize = checkpoint_ipc_blob_size(t->channel_count, t->message_count);
    uint8_t* buf = (uint8_t*)kmalloc(bufsize);
    if (!buf) {
        kfree(t);
        return -1;
    }
    int rc = -1;
    uint32_t w = checkpoint_ipc_serialize(t, buf, bufsize);
    if (w) {
        rc = checkpoint_capture_kernel_blob(CHECKPOINT_TAG_IPC, buf, w);
    }
    kfree(buf);
    kfree(t);
    return rc;
}

int checkpoint_restore_ipc(const void* blob, uint32_t size) {
    checkpoint_ipc_t* t = (checkpoint_ipc_t*)kmalloc(sizeof(*t));
    if (!t) {
        return -1;
    }
    if (!checkpoint_ipc_deserialize(t, blob, size) || !checkpoint_ipc_validate(t)) {
        kfree(t);
        return -1;
    }

    /* Recreate the channels (id preservation is pm-dependent; best-effort). */
    for (uint32_t i = 0; i < t->channel_count; i++) {
        uint32_t new_id = 0;
        if (pm_ipc_create_channel(t->channels[i].owner_pid, &new_id) == 0) {
            pm_ipc_set_channel_permissions(new_id, t->channels[i].permissions);
        }
    }

    /* Re-enqueue the persisted messages. */
    for (uint32_t i = 0; i < t->message_count; i++) {
        checkpoint_ipc_message_t* r = &t->messages[i];
        pm_ipc_message_t m;
        memset(&m, 0, sizeof(m));
        m.type = (pm_ipc_type_t)r->type;
        m.src_pid = r->src_pid;
        m.dst_pid = r->dst_pid;
        m.channel_id = r->channel_id;
        m.message_id = r->message_id;
        m.data_size = r->data_size;
        m.timestamp = r->timestamp;
        m.flags = r->flags;
        uint32_t n = r->data_size <= PM_IPC_BUFFER_SIZE ? r->data_size : PM_IPC_BUFFER_SIZE;
        memcpy(m.data, r->data, n);
        pm_ipc_send_message(&m);
    }

    kfree(t);
    return 0;
}
