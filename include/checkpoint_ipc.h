/* IKOS Orthogonal Persistence v2 - IPC channel/message serialization (#142)
 *
 * Persists IPC across a checkpoint without losing or duplicating messages. At
 * the checkpoint barrier the message queues are drained to a quiescent point,
 * then the settled channels and their queued messages are serialized as kernel
 * state; restore recreates the channels and re-enqueues the messages.
 *
 * Two layers, mirroring #140/#141:
 *   - a pure, dependency-free serialize/deserialize/validate core (host-tested);
 *   - kernel capture/restore adapters (declared here, defined in the sync module)
 *     over the process manager's pm_ipc_* API.
 *
 * See docs/architecture/orthogonal-persistence-v2.md §6.
 */

#ifndef CHECKPOINT_IPC_H
#define CHECKPOINT_IPC_H

#include <stdint.h>
#include <stdbool.h>

#define CHECKPOINT_IPC_MAGIC        0x494B49504331ULL /* "IKIPC1" */
#define CHECKPOINT_IPC_VERSION      1
#define CHECKPOINT_IPC_MAX_CHANNELS 32
#define CHECKPOINT_IPC_MAX_MESSAGES 16
#define CHECKPOINT_IPC_DATA_MAX     4096 /* PM_IPC_BUFFER_SIZE */

typedef struct {
    uint32_t channel_id;
    uint32_t owner_pid;
    uint32_t permissions;
    uint8_t  active;
    uint8_t  reserved[3];
} checkpoint_ipc_channel_t;

typedef struct {
    uint32_t channel_id;
    uint32_t type;
    uint32_t src_pid;
    uint32_t dst_pid;
    uint32_t message_id;
    uint32_t data_size;   /* valid bytes of data[] */
    uint32_t flags;
    uint32_t reserved;
    uint64_t timestamp;
    uint8_t  data[CHECKPOINT_IPC_DATA_MAX];
} checkpoint_ipc_message_t;

typedef struct {
    uint32_t channel_count;
    uint32_t message_count;
    checkpoint_ipc_channel_t channels[CHECKPOINT_IPC_MAX_CHANNELS];
    checkpoint_ipc_message_t messages[CHECKPOINT_IPC_MAX_MESSAGES];
} checkpoint_ipc_t;

/* ----- Pure serialization core ----- */

uint32_t checkpoint_ipc_serialize(const checkpoint_ipc_t* t, void* buf, uint32_t bufsize);
bool     checkpoint_ipc_deserialize(checkpoint_ipc_t* t, const void* buf, uint32_t size);

/* channel_count/message_count within bounds; channel ids unique + nonzero;
 * every message's channel_id resolves to a present channel; data_size bounded. */
bool checkpoint_ipc_validate(const checkpoint_ipc_t* t);

const checkpoint_ipc_channel_t* checkpoint_ipc_find_channel(const checkpoint_ipc_t* t,
                                                            uint32_t channel_id);

uint32_t checkpoint_ipc_blob_size(uint32_t channel_count, uint32_t message_count);

/* ----- Kernel capture/restore adapters (kernel sync module) ----- */
#define CHECKPOINT_TAG_IPC 3

/* Drain the live IPC channels and persist the settled channels + messages as a
 * kernel-state blob (#139). Returns 0 on success, negative on error. */
int checkpoint_capture_ipc(void);

/* Recreate channels and re-enqueue the persisted messages from a blob on
 * restore. Returns 0 on success, negative on error. */
int checkpoint_restore_ipc(const void* blob, uint32_t size);

#endif /* CHECKPOINT_IPC_H */
