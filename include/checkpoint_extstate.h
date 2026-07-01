/* IKOS Orthogonal Persistence - External / non-persistable state policy
 *
 * Some resources cannot meaningfully be replayed across a power cycle: open
 * network sockets, in-flight DMA buffers, device-register handles. Per
 * docs/architecture/orthogonal-persistence.md, the policy is: do NOT serialize
 * these at checkpoint time; on restore, mark each such handle "severed" so the
 * next use returns a clean, defined error and the application re-establishes
 * the resource (reconnect, remap DMA, reopen device) instead of touching now
 * invalid kernel/hardware state.
 *
 * This module is the policy core: classification (which kinds survive),
 * severing (the restore-time transition), and the status an app observes.
 *
 * Issue #118 (epic #121).
 */

#ifndef CHECKPOINT_EXTSTATE_H
#define CHECKPOINT_EXTSTATE_H

#include <stdint.h>
#include <stdbool.h>

/* Kinds of resource a process handle can refer to. */
typedef enum {
    EXTSTATE_REGULAR_FILE = 0,  /* survives: reopen by path at the saved offset */
    EXTSTATE_SOCKET       = 1,  /* severed: connection state is gone */
    EXTSTATE_DMA_BUFFER   = 2,  /* severed: hardware mapping is gone */
    EXTSTATE_DEVICE       = 3,  /* severed: device register state is gone */
    EXTSTATE_PIPE         = 4,  /* severed: the peer endpoint is gone */
} extstate_kind_t;

/* Handle state flags. */
#define EXTSTATE_FLAG_OPEN        0x1   /* the handle is currently open/in use */
#define EXTSTATE_FLAG_SEVERED     0x2   /* severed across a checkpoint restore */
#define EXTSTATE_FLAG_RECONNECTED 0x4   /* owner re-established it after a sever */

/* Status codes an app/syscall observes when touching a handle. */
#define EXTSTATE_OK        0
#define EXTSTATE_SEVERED  -1   /* handle was severed; the app must re-establish it */

/* A minimal external-resource handle. The kernel maps a process's file
 * descriptors / DMA mappings onto these when applying the policy. */
typedef struct {
    extstate_kind_t kind;
    uint32_t        flags;
} extstate_resource_t;

/* Policy: does a resource of this kind survive a checkpoint/restore?
 * Regular files do (their bytes live in the persistent filesystem and the fd /
 * offset ride along in the process's memory image); everything external does
 * not. */
bool extstate_survives_checkpoint(extstate_kind_t kind);

/* Restore-time transition for one handle: if it is open and does not survive a
 * checkpoint, mark it severed and return true. Idempotent: an already-severed
 * or persistable or closed handle is left unchanged and returns false. */
bool extstate_sever(extstate_resource_t* res);

/* Apply extstate_sever() to every handle in an array; returns the number that
 * transitioned to severed. Use this when reconstructing a process on restore. */
int extstate_sever_all(extstate_resource_t* resources, int count);

/* Status an app/syscall observes for a handle: EXTSTATE_SEVERED if it was
 * severed across a restore, else EXTSTATE_OK. */
int extstate_status(const extstate_resource_t* res);

/* ----- File-descriptor integration (#128) -----
 *
 * The kernel tags a descriptor's flags word with its resource kind so that, on
 * restore, the registrar can sever the non-persistable ones. The kind lives in
 * the high bits of the flags so it does not collide with file open flags. */
#define EXTSTATE_FD_KIND_SHIFT 28
#define EXTSTATE_FD_KIND_MASK  (0x7u << EXTSTATE_FD_KIND_SHIFT)
#define EXTSTATE_FD_SEVERED    (0x1u << 31)

/* Store/read the resource kind in a descriptor's flags word. */
void            extstate_fd_set_kind(uint32_t* flags, extstate_kind_t kind);
extstate_kind_t extstate_kind_from_fd_flags(uint32_t flags);

/* Restore-time transition for one descriptor (by its flags word): if its kind
 * does not survive a checkpoint and it is not already severed, set the severed
 * bit and return true. Persistable or already-severed descriptors return false. */
bool extstate_sever_fd(uint32_t* flags);

/* Has this descriptor been severed across a restore? Syscalls consult this to
 * return a clean error so the app re-establishes the resource. */
bool extstate_fd_is_severed(uint32_t flags);

/* ----- DMA / IPC endpoints and the reconnection contract (v2, #146) -----
 *
 * Not every non-persistable resource is a file descriptor. An in-flight DMA
 * mapping or a non-drainable IPC/network endpoint is its own kernel object, so
 * v2 pushes the severing policy inward to those resources directly (v2 design
 * section 6). The kernel tracks each such resource as an extstate_endpoint_t;
 * at the checkpoint barrier the non-persistable ones are dropped from the
 * persisted set, and on restore they are severed so their next use returns a
 * defined error.
 *
 * Reconnection contract: an owner that observes EXTSTATE_SEVERED (via
 * extstate_endpoint_status, mirroring the socket reconnect path of #128)
 * re-establishes the resource out of band, then calls extstate_endpoint_reconnect
 * to clear the severed state. The generation counter bumps on every reconnect so
 * stale references held elsewhere can detect that the endpoint was rebuilt. */
typedef struct {
    extstate_kind_t kind;        /* DMA_BUFFER, DEVICE, SOCKET, PIPE, ... */
    uint32_t        flags;       /* EXTSTATE_FLAG_OPEN / _SEVERED / _RECONNECTED */
    uint32_t        owner;       /* owning pid (for the restore registrar) */
    uint32_t        generation;  /* bumped each time the owner re-establishes it */
} extstate_endpoint_t;

/* Does this endpoint's connection ride through a checkpoint, or must it be
 * severed? A thin wrapper over the kind classification (DMA/device/socket/pipe
 * do not survive; a regular-file-backed mapping would). */
bool extstate_endpoint_persistable(const extstate_endpoint_t* ep);

/* Restore-time transition for one endpoint: if it is open, non-persistable, and
 * not already severed, mark it severed and return true. Idempotent. */
bool extstate_endpoint_sever(extstate_endpoint_t* ep);

/* Sever every endpoint in an array; returns the number that transitioned. Use
 * when reconstructing a process's DMA mappings / IPC endpoints on restore. */
int  extstate_endpoint_sever_all(extstate_endpoint_t* eps, int count);

/* Status the owner observes: EXTSTATE_SEVERED while the endpoint is severed and
 * not yet reconnected, else EXTSTATE_OK. */
int  extstate_endpoint_status(const extstate_endpoint_t* ep);

/* Does this endpoint still need the owner to re-establish it? (Severed and not
 * yet reconnected.) */
bool extstate_endpoint_needs_reconnect(const extstate_endpoint_t* ep);

/* The reconnection contract. Call after the owner has re-established the
 * underlying resource: clears the severed state, marks it reconnected + open,
 * bumps the generation, and returns true. Returns false (no-op) if the endpoint
 * was not severed, so a double reconnect is safe. */
bool extstate_endpoint_reconnect(extstate_endpoint_t* ep);

#endif /* CHECKPOINT_EXTSTATE_H */
