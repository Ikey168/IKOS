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
#define EXTSTATE_FLAG_OPEN     0x1   /* the handle is currently open/in use */
#define EXTSTATE_FLAG_SEVERED  0x2   /* severed across a checkpoint restore */

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

#endif /* CHECKPOINT_EXTSTATE_H */
