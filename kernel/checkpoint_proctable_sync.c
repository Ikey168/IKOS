/* IKOS Orthogonal Persistence v2 - Process-table capture/restore adapter (#140)
 *
 * Bridges the live process manager to the portable serialization core
 * (checkpoint_proctable.c) and the kernel-state blob mechanism (#139). Kernel
 * only: references process_manager/process/scheduler. Compiled freestanding;
 * the host tests cover the pure core, this adapter is validated by the build.
 */

#include "checkpoint_proctable.h"
#include "checkpoint.h"
#include "process_manager.h"
#include <stddef.h>

extern void* kmalloc(size_t size);
extern void  kfree(void* ptr);
extern int   scheduler_add_process(process_t* proc);

int checkpoint_capture_proctable(void) {
    checkpoint_proctable_t* t = (checkpoint_proctable_t*)kmalloc(sizeof(*t));
    if (!t) {
        return -1;
    }
    t->count = 0;

    uint32_t pids[PM_MAX_PROCESSES];
    uint32_t n = 0;
    if (pm_get_process_list(pids, PM_MAX_PROCESSES, &n) != 0) {
        kfree(t);
        return -1;
    }

    for (uint32_t i = 0; i < n && t->count < CHECKPOINT_PROCTABLE_MAX; i++) {
        process_t* p = pm_get_process(pids[i]);
        if (!p) {
            continue;
        }
        checkpoint_proc_record_t* r = &t->procs[t->count++];
        r->pid = (uint32_t)p->pid;
        r->ppid = (uint32_t)p->ppid;
        r->state = (uint32_t)p->state;
        r->priority = (uint32_t)p->priority;
        r->alarm_time = p->alarm_time;
        r->first_child_pid = p->first_child ? (uint32_t)p->first_child->pid : 0;
        r->next_sibling_pid = p->next_sibling ? (uint32_t)p->next_sibling->pid : 0;
        r->on_ready_queue = (p->state == PROCESS_STATE_READY) ? 1 : 0;
        r->reserved[0] = r->reserved[1] = r->reserved[2] = 0;
        r->ready_order = 0;
    }

    uint32_t bufsize = checkpoint_proctable_blob_size(t->count);
    uint8_t* buf = (uint8_t*)kmalloc(bufsize);
    if (!buf) {
        kfree(t);
        return -1;
    }

    int rc = -1;
    uint32_t written = checkpoint_proctable_serialize(t, buf, bufsize);
    if (written) {
        rc = checkpoint_capture_kernel_blob(CHECKPOINT_TAG_PROCTABLE, buf, written);
    }
    kfree(buf);
    kfree(t);
    return rc;
}

int checkpoint_restore_proctable(const void* blob, uint32_t size) {
    checkpoint_proctable_t* t = (checkpoint_proctable_t*)kmalloc(sizeof(*t));
    if (!t) {
        return -1;
    }
    if (!checkpoint_proctable_deserialize(t, blob, size) ||
        !checkpoint_proctable_validate(t)) {
        kfree(t);
        return -1;
    }

    /* Pass 1: ensure a process_t exists per pid with its scalar fields. */
    for (uint32_t i = 0; i < t->count; i++) {
        checkpoint_proc_record_t* r = &t->procs[i];
        process_t* p = process_get_by_pid((pid_t)r->pid);
        if (!p) {
            p = process_create("restored", "");
            if (!p) {
                continue;
            }
            p->pid = (pid_t)r->pid;
            pm_table_add_process(p);
        }
        p->ppid = (pid_t)r->ppid;
        p->state = (process_state_t)r->state;
        p->priority = (process_priority_t)r->priority;
        p->alarm_time = r->alarm_time;
    }

    /* Pass 2: rebuild pointer links by pid, and re-add to the scheduler. */
    for (uint32_t i = 0; i < t->count; i++) {
        checkpoint_proc_record_t* r = &t->procs[i];
        process_t* p = process_get_by_pid((pid_t)r->pid);
        if (!p) {
            continue;
        }
        p->parent = r->ppid ? process_get_by_pid((pid_t)r->ppid) : NULL;
        p->first_child = r->first_child_pid
                             ? process_get_by_pid((pid_t)r->first_child_pid) : NULL;
        p->next_sibling = r->next_sibling_pid
                              ? process_get_by_pid((pid_t)r->next_sibling_pid) : NULL;
        if (r->on_ready_queue) {
            scheduler_add_process(p);
        }
    }

    kfree(t);
    return 0;
}
