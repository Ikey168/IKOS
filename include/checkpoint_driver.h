/* IKOS Orthogonal Persistence v2 - Driver re-attach framework (#143)
 *
 * Device-register state is meaningless after a power cut, so each driver
 * declares how to quiesce its hardware before a checkpoint and re-initialize +
 * reconcile it on restore. This is the contract and the ordering engine from
 * docs/architecture/orthogonal-persistence-v2.md §4.
 *
 * Flow:
 *   - at the checkpoint barrier (#138): quiesce every registered device;
 *   - on restore, after cold-init and before user restore: reattach every one.
 *
 * The device is an opaque void* so this stays decoupled from device_manager and
 * is a pure, host-testable ordering core (like the barrier). Real drivers
 * register their ops in #144 (disk) and #145 (framebuffer).
 */

#ifndef CHECKPOINT_DRIVER_H
#define CHECKPOINT_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

#define CHECKPOINT_DRIVER_MAX 32

/* Per-driver persistence contract. */
typedef struct {
    int  (*quiesce)(void* dev);   /* reach a safe boundary before checkpoint; 0 = ok */
    int  (*reattach)(void* dev);  /* re-init hardware on restore, reconcile; 0 = ok */
    bool persistable;             /* false: the device's connections are severed */
} checkpoint_persist_ops_t;

typedef struct {
    void* dev;
    checkpoint_persist_ops_t ops;
    bool  severed;                /* set if reattach failed under CONTINUE mode */
} checkpoint_driver_entry_t;

typedef struct {
    checkpoint_driver_entry_t entries[CHECKPOINT_DRIVER_MAX];
    uint32_t count;
} checkpoint_driver_registry_t;

/* Failure policy for reattach. */
typedef enum {
    CHECKPOINT_REATTACH_ABORT = 0,   /* first failed reattach aborts (cold boot) */
    CHECKPOINT_REATTACH_CONTINUE,    /* a failed reattach severs that device, keep going */
} checkpoint_reattach_mode_t;

#define CHECKPOINT_DRIVER_ABORTED (-1)

/* ----- Registry ----- */

void     checkpoint_driver_registry_init(checkpoint_driver_registry_t* r);
/* Register a device + ops. Returns 0, or -1 if the registry is full or args are
 * bad. quiesce/reattach may be NULL (treated as a no-op success). */
int      checkpoint_driver_register(checkpoint_driver_registry_t* r,
                                    void* dev, const checkpoint_persist_ops_t* ops);
uint32_t checkpoint_driver_count(const checkpoint_driver_registry_t* r);

/* Quiesce every registered device, in registration order. Returns the number
 * that quiesced successfully; a driver whose quiesce returns nonzero is counted
 * as failed but does not stop the others (best effort to reach a safe boundary). */
int      checkpoint_driver_quiesce_all(checkpoint_driver_registry_t* r);

/* Reattach every registered device, in registration order.
 *  - CONTINUE: attempt all; a failed reattach marks that entry severed; returns
 *    the number reattached successfully.
 *  - ABORT: stop at the first failure and return CHECKPOINT_DRIVER_ABORTED
 *    (the caller cold-boots); otherwise returns the number reattached. */
int      checkpoint_driver_reattach_all(checkpoint_driver_registry_t* r,
                                        checkpoint_reattach_mode_t mode);

/* Was this entry severed by a failed reattach? */
bool     checkpoint_driver_is_severed(const checkpoint_driver_registry_t* r, uint32_t index);

/* Process-wide registry that real drivers register into at boot; the barrier
 * and restore path drive it in #147. */
checkpoint_driver_registry_t* checkpoint_driver_global(void);

#endif /* CHECKPOINT_DRIVER_H */
