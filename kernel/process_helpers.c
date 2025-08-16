/* IKOS Process Helper Functions - Issue #18
 * Utility functions supporting comprehensive process termination
 */

#include "../include/process.h"
#include "../include/process_exit.h"
#include "../include/kernel_log.h"
#include "../include/scheduler.h"
#include "../include/vmm.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* External variables from process.c */
extern process_t processes[MAX_PROCESSES];
extern process_t* current_process;
extern process_t* ready_queue_head;
extern process_t* ready_queue_tail;
extern process_stats_t process_statistics;

/* System time counter (updated by timer interrupt) */
extern volatile uint64_t system_time_ms;

/* ========================== Process Lookup Functions ========================== */

/**
 * Find process by PID
 */
process_t* process_find_by_pid(pid_t pid) {
    if (pid <= 0 || pid >= MAX_PROCESSES) {
        return NULL;
    }
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid && processes[i].state != PROCESS_STATE_TERMINATED) {
            return &processes[i];
        }
    }
    
    return NULL;
}

/**
 * Get process by index
 */
process_t* process_get_by_index(int index) {
    if (index < 0 || index >= MAX_PROCESSES) {
        return NULL;
    }
    
    return &processes[index];
}

/**
 * Get current process
 */
process_t* process_get_current(void) {
    return current_process;
}

/**
 * Find zombie child of a parent process
 */
process_t* process_find_zombie_child(process_t* parent) {
    if (!parent) {
        return NULL;
    }
    
    return parent->zombie_children;
}

/**
 * Find specific child by PID
 */
process_t* process_find_child_by_pid(process_t* parent, pid_t pid) {
    if (!parent || pid <= 0) {
        return NULL;
    }
    
    /* Check living children */
    process_t* child = parent->first_child;
    while (child) {
        if (child->pid == pid) {
            return child;
        }
        child = child->next_sibling;
    }
    
    /* Check zombie children */
    child = parent->zombie_children;
    while (child) {
        if (child->pid == pid) {
            return child;
        }
        child = child->next_zombie;
    }
    
    return NULL;
}

/* ========================== Process Slot Management ========================== */

/**
 * Free a process slot
 */
void process_free_slot(process_t* proc) {
    if (!proc) {
        return;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Freeing process slot for PID %u", proc->pid);
    
    /* Clear the entire process structure */
    uint32_t pid = proc->pid; /* Save PID for logging */
    memset(proc, 0, sizeof(process_t));
    
    /* Mark as terminated */
    proc->state = PROCESS_STATE_TERMINATED;
    proc->pid = 0;
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Process slot %u freed", pid);
}

/* ========================== Queue Management ========================== */

/**
 * Remove process from ready queue
 */
void process_remove_from_ready_queue(process_t* proc) {
    if (!proc) {
        return;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Removing process %u from ready queue", proc->pid);
    
    /* Handle single-item queue */
    if (ready_queue_head == proc && ready_queue_tail == proc) {
        ready_queue_head = NULL;
        ready_queue_tail = NULL;
    }
    /* Handle head of queue */
    else if (ready_queue_head == proc) {
        ready_queue_head = proc->next;
        if (ready_queue_head) {
            ready_queue_head->prev = NULL;
        }
    }
    /* Handle tail of queue */
    else if (ready_queue_tail == proc) {
        ready_queue_tail = proc->prev;
        if (ready_queue_tail) {
            ready_queue_tail->next = NULL;
        }
    }
    /* Handle middle of queue */
    else {
        if (proc->prev) {
            proc->prev->next = proc->next;
        }
        if (proc->next) {
            proc->next->prev = proc->prev;
        }
    }
    
    /* Clear queue pointers */
    proc->next = NULL;
    proc->prev = NULL;
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Process %u removed from ready queue", proc->pid);
}

/**
 * Add process to zombie list of parent
 */
void process_add_to_zombie_list(process_t* parent, process_t* child) {
    if (!parent || !child) {
        return;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Adding process %u to zombie list of parent %u", 
               child->pid, parent->pid);
    
    /* Add to head of zombie list */
    child->next_zombie = parent->zombie_children;
    parent->zombie_children = child;
    
    /* Remove from living children list */
    if (parent->first_child == child) {
        parent->first_child = child->next_sibling;
    } else {
        /* Find child in sibling list and remove */
        process_t* sibling = parent->first_child;
        while (sibling && sibling->next_sibling != child) {
            sibling = sibling->next_sibling;
        }
        if (sibling) {
            sibling->next_sibling = child->next_sibling;
        }
    }
    
    child->next_sibling = NULL;
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Process %u added to zombie list", child->pid);
}

/**
 * Remove child from parent's zombie list
 */
void process_remove_from_zombie_list(process_t* parent, process_t* child) {
    if (!parent || !child) {
        return;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Removing process %u from zombie list of parent %u", 
               child->pid, parent->pid);
    
    if (parent->zombie_children == child) {
        /* Child is at head of zombie list */
        parent->zombie_children = child->next_zombie;
    } else {
        /* Find child in zombie list and remove */
        process_t* zombie = parent->zombie_children;
        while (zombie && zombie->next_zombie != child) {
            zombie = zombie->next_zombie;
        }
        if (zombie) {
            zombie->next_zombie = child->next_zombie;
        }
    }
    
    child->next_zombie = NULL;
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Process %u removed from zombie list", child->pid);
}

/* ========================== Wait Queue Management ========================== */

/**
 * Block process waiting for child to exit
 */
pid_t process_block_waiting_for_child(process_t* parent, pid_t child_pid, int* status_ptr) {
    if (!parent) {
        return -1;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Blocking process %u waiting for child %u", 
               parent->pid, child_pid);
    
    /* Set up wait state */
    parent->waiting_for_child = (process_t*)(uintptr_t)child_pid; /* Use as PID storage */
    parent->wait_status_ptr = status_ptr;
    parent->state = PROCESS_STATE_BLOCKED;
    
    /* Remove from ready queue */
    process_remove_from_ready_queue(parent);
    
    /* Schedule next process */
    schedule_next_process();
    
    /* When we return here, a child has exited */
    /* The scheduler will have updated our status and unblocked us */
    
    /* Clear wait state */
    parent->waiting_for_child = NULL;
    parent->wait_status_ptr = NULL;
    
    /* Return PID of child that exited (set by signal handler) */
    /* For simplicity, we'll return the child_pid for now */
    return child_pid;
}

/**
 * Wake up waiting parent when child exits
 */
void process_wake_waiting_parent(process_t* parent, process_t* child) {
    if (!parent || !child) {
        return;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Waking up parent %u for child %u exit", 
               parent->pid, child->pid);
    
    /* Check if parent is waiting for this specific child or any child */
    if (parent->state == PROCESS_STATE_BLOCKED && parent->waiting_for_child) {
        pid_t waiting_for = (pid_t)(uintptr_t)parent->waiting_for_child;
        
        if (waiting_for == 0 || waiting_for == child->pid) {
            /* Parent is waiting for this child */
            
            /* Store exit status if parent wants it */
            if (parent->wait_status_ptr) {
                *parent->wait_status_ptr = child->exit_code;
            }
            
            /* Unblock parent */
            parent->state = PROCESS_STATE_READY;
            parent->waiting_for_child = NULL;
            parent->wait_status_ptr = NULL;
            
            /* Add parent back to ready queue */
            process_add_to_ready_queue(parent);
            
            KLOG_DEBUG(LOG_CAT_PROCESS, "Parent %u woken up", parent->pid);
        }
    }
}

/**
 * Remove process from wait queue
 */
void process_remove_from_wait_queue(process_t* proc) {
    if (!proc) {
        return;
    }
    
    /* Clear wait state if process was waiting */
    if (proc->state == PROCESS_STATE_BLOCKED && proc->waiting_for_child) {
        proc->waiting_for_child = NULL;
        proc->wait_status_ptr = NULL;
        proc->state = PROCESS_STATE_READY;
    }
}

/* ========================== Scheduling Support ========================== */

/**
 * Schedule next process
 */
void schedule_next_process(void) {
    /* This should call the scheduler to switch to the next ready process */
    scheduler_switch_to_next();
}

/**
 * Add process to ready queue
 */
void process_add_to_ready_queue(process_t* proc) {
    if (!proc) {
        return;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Adding process %u to ready queue", proc->pid);
    
    /* Clear queue pointers */
    proc->next = NULL;
    proc->prev = NULL;
    
    if (!ready_queue_head) {
        /* Empty queue */
        ready_queue_head = proc;
        ready_queue_tail = proc;
    } else {
        /* Add to tail */
        ready_queue_tail->next = proc;
        proc->prev = ready_queue_tail;
        ready_queue_tail = proc;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Process %u added to ready queue", proc->pid);
}

/* ========================== System Time ========================== */

/**
 * Get current system time in milliseconds
 */
uint64_t get_system_time(void) {
    return system_time_ms;
}

/* ========================== Memory Management Stubs ========================== */

/**
 * Allocate kernel memory
 */
void* kalloc(size_t size) {
    /* For now, return NULL - this should be implemented by memory manager */
    KLOG_WARN(LOG_CAT_PROCESS, "kalloc not yet implemented, requested size %zu", size);
    return NULL;
}

/**
 * Free kernel memory
 */
void kfree(void* ptr) {
    /* For now, do nothing - this should be implemented by memory manager */
    if (ptr) {
        KLOG_WARN(LOG_CAT_PROCESS, "kfree not yet implemented");
    }
}

/**
 * Validate user space pointer
 */
bool validate_user_pointer(void* ptr, size_t size) {
    if (!ptr) {
        return false;
    }
    
    uint64_t addr = (uint64_t)ptr;
    
    /* Check if pointer is in user space range */
    if (addr < USER_SPACE_START || addr + size > USER_SPACE_END) {
        return false;
    }
    
    /* Additional validation could be added here */
    return true;
}

/* ========================== Resource Cleanup Stubs ========================== */

/* These functions would be implemented by their respective subsystems */

int ipc_cleanup_process_queues(pid_t pid) {
    KLOG_DEBUG(LOG_CAT_PROCESS, "IPC cleanup for process %u (stub)", pid);
    return 0;
}

int shm_cleanup_process_segments(pid_t pid) {
    KLOG_DEBUG(LOG_CAT_PROCESS, "SHM cleanup for process %u (stub)", pid);
    return 0;
}

int sem_cleanup_process_semaphores(pid_t pid) {
    KLOG_DEBUG(LOG_CAT_PROCESS, "Semaphore cleanup for process %u (stub)", pid);
    return 0;
}

void ipc_remove_from_all_queues(pid_t pid) {
    KLOG_DEBUG(LOG_CAT_PROCESS, "IPC queue removal for process %u (stub)", pid);
}

int timer_cancel_all_for_process(pid_t pid) {
    KLOG_DEBUG(LOG_CAT_PROCESS, "Timer cleanup for process %u (stub)", pid);
    return 0;
}

void alarm_cancel(pid_t pid) {
    KLOG_DEBUG(LOG_CAT_PROCESS, "Alarm cancel for process %u (stub)", pid);
}

int signal_queue_to_process(process_t* proc, int signal, pid_t sender_pid, int exit_status) {
    if (!proc) {
        return -1;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Queuing signal %d to process %u (stub)", signal, proc->pid);
    
    /* Set pending signal bit */
    proc->pending_signals |= (1ULL << signal);
    
    return 0;
}

void signal_remove_from_delivery_queues(pid_t pid) {
    KLOG_DEBUG(LOG_CAT_PROCESS, "Signal queue removal for process %u (stub)", pid);
}

int vmm_cleanup_user_space(vm_space_t* address_space) {
    if (!address_space) {
        return 0;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "VMM user space cleanup (stub)");
    
    /* Return number of pages freed (stub) */
    return 0;
}

/* ========================== VFS Integration Stubs ========================== */

/* These would be implemented by the VFS subsystem */
int vfs_close(int fd) {
    KLOG_DEBUG(LOG_CAT_PROCESS, "VFS close fd %d (stub)", fd);
    return 0;
}

/* ========================== Kernel Logging Stub ========================== */

void klog(int level, int category, const char* format, ...) {
    /* For now, do nothing - this should be implemented by logging system */
    /* In a real implementation, this would format and output the log message */
}

/* ========================== Scheduler Integration Stub ========================== */

void scheduler_switch_to_next(void) {
    KLOG_DEBUG(LOG_CAT_PROCESS, "Scheduler switch (stub)");
    
    /* For now, just set current_process to next ready process */
    if (ready_queue_head && ready_queue_head != current_process) {
        current_process = ready_queue_head;
    }
}
