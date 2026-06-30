/* IKOS Orthogonal Persistence v2 - Process-table serialization (#140)
 *
 * Serializes the process table and per-process scheduler state into a portable
 * blob (and back), so a whole-machine checkpoint can resume the exact set of
 * processes and their relationships. Pointers are never persisted: parent /
 * child / sibling links and scheduler membership are stored by pid and rebuilt
 * on restore.
 *
 * This header has two layers:
 *   - a pure, dependency-free serialize/deserialize/validate core (host-tested);
 *   - kernel capture/restore adapters that snapshot the live process manager
 *     into the portable form and apply it back (declared here, defined in the
 *     kernel-only sync module).
 *
 * See docs/architecture/orthogonal-persistence-v2.md §3.
 */

#ifndef CHECKPOINT_PROCTABLE_H
#define CHECKPOINT_PROCTABLE_H

#include <stdint.h>
#include <stdbool.h>

#define CHECKPOINT_PROCTABLE_MAGIC   0x494B50524F435431ULL /* "IKPROCT1" */
#define CHECKPOINT_PROCTABLE_VERSION 1
#define CHECKPOINT_PROCTABLE_MAX     256 /* PM_MAX_PROCESSES */

/* Portable, pointer-free record for one process. Relationships and scheduler
 * membership are by pid (0 = none). */
typedef struct {
    uint32_t pid;
    uint32_t ppid;
    uint32_t state;            /* process_state_t */
    uint32_t priority;         /* process_priority_t */
    uint64_t alarm_time;
    uint32_t first_child_pid;  /* 0 = none */
    uint32_t next_sibling_pid; /* 0 = none */
    uint8_t  on_ready_queue;   /* 1 if in the scheduler ready queue */
    uint8_t  reserved[3];
    uint32_t ready_order;      /* position in the ready queue */
} checkpoint_proc_record_t;

typedef struct {
    checkpoint_proc_record_t procs[CHECKPOINT_PROCTABLE_MAX];
    uint32_t count;
} checkpoint_proctable_t;

/* ----- Pure serialization core ----- */

/* Serialize `t` into `buf`. Returns the number of bytes written, or 0 if the
 * buffer is too small / inputs are invalid. */
uint32_t checkpoint_proctable_serialize(const checkpoint_proctable_t* t,
                                        void* buf, uint32_t bufsize);

/* Deserialize `buf` into `t`. Returns true on a valid magic/version and a
 * self-consistent length; false otherwise. */
bool checkpoint_proctable_deserialize(checkpoint_proctable_t* t,
                                      const void* buf, uint32_t size);

/* Referential-integrity check: every nonzero ppid / first_child_pid /
 * next_sibling_pid refers to a pid present in the table, and pids are unique
 * and nonzero. Returns true if consistent. */
bool checkpoint_proctable_validate(const checkpoint_proctable_t* t);

/* Find a record by pid, or NULL. */
const checkpoint_proc_record_t* checkpoint_proctable_find(const checkpoint_proctable_t* t,
                                                          uint32_t pid);

/* Serialized size of a table with `count` records. */
uint32_t checkpoint_proctable_blob_size(uint32_t count);

/* ----- Kernel capture/restore adapters (defined in the kernel sync module) -----
 * Kernel-state blob tag for the process table. */
#define CHECKPOINT_TAG_PROCTABLE 1

/* Snapshot the live process table into a kernel-state blob (#139) at checkpoint
 * time. Returns 0 on success, negative on error. */
int checkpoint_capture_proctable(void);

/* Rebuild the live process table + scheduler links from a serialized blob on
 * restore. Returns 0 on success, negative on error. */
int checkpoint_restore_proctable(const void* blob, uint32_t size);

#endif /* CHECKPOINT_PROCTABLE_H */
