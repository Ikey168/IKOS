/* IKOS Process Termination and Cleanup System - Issue #18
 * Implements comprehensive process exit, cleanup, and resource management
 */

#include "../include/process.h"
#include "../include/process_exit.h"
#include "../include/kernel_log.h"
#include "../include/vmm.h"
#include "../include/vfs.h"
#include "../include/syscalls.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* External variables from process.c */
extern process_t* current_process;
extern process_stats_t process_statistics;

/* Process exit statistics */
static struct {
    uint64_t total_exits;           /* Total processes that have exited */
    uint64_t normal_exits;          /* Processes that exited normally */
    uint64_t killed_processes;      /* Processes terminated by signals */
    uint64_t zombie_count;          /* Current zombie process count */
    uint64_t orphan_count;          /* Current orphaned process count */
    uint64_t resources_cleaned;     /* Total resources cleaned up */
} exit_statistics = {0};

/* Wait queue for processes waiting for children */
static process_wait_queue_t wait_queue = {0};

/* Init process (PID 1) for orphan reparenting */
static process_t* init_process = NULL;

/* ========================== Core Exit Functions ========================== */

/**
 * Complete process exit with comprehensive cleanup
 */
void process_exit(process_t* proc, int exit_code) {
    if (!proc) {
        KLOG_ERROR(LOG_CAT_PROCESS, "process_exit: Invalid process pointer");
        return;
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "Process %u (%s) exiting with code %d", 
              proc->pid, proc->name, exit_code);
    
    /* Prevent re-entry */
    if (proc->state == PROCESS_STATE_TERMINATING || proc->state == PROCESS_STATE_ZOMBIE) {
        KLOG_WARN(LOG_CAT_PROCESS, "Process %u already terminating", proc->pid);
        return;
    }
    
    /* Mark as terminating */
    proc->state = PROCESS_STATE_TERMINATING;
    proc->exit_code = exit_code;
    proc->exit_time = get_system_time();
    
    /* Remove from scheduler if currently running */
    if (current_process == proc) {
        current_process = NULL;
    }
    
    /* Step 1: Close all file descriptors */
    process_cleanup_files(proc);
    
    /* Step 2: Clean up IPC resources */
    process_cleanup_ipc(proc);
    
    /* Step 3: Clean up timers and signals */
    process_cleanup_timers(proc);
    process_cleanup_signals(proc);
    
    /* Step 4: Handle child processes */
    process_reparent_children(proc);
    
    /* Step 5: Notify parent */
    process_notify_parent(proc, exit_code);
    
    /* Step 6: Clean up memory (but keep process structure for zombie state) */
    process_cleanup_memory(proc);
    
    /* Step 7: Remove from ready queue */
    process_remove_from_ready_queue(proc);
    
    /* Step 8: Enter zombie state */
    proc->state = PROCESS_STATE_ZOMBIE;
    exit_statistics.zombie_count++;
    
    /* Update exit statistics */
    exit_statistics.total_exits++;
    if (exit_code == 0) {
        exit_statistics.normal_exits++;
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "Process %u entered zombie state", proc->pid);
    
    /* Schedule next process if this was the current process */
    if (proc == current_process) {
        schedule_next_process();
    }
}

/**
 * Force terminate a process (SIGKILL equivalent)
 */
void process_kill(process_t* proc, int signal) {
    if (!proc) {
        KLOG_ERROR(LOG_CAT_PROCESS, "process_kill: Invalid process pointer");
        return;
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "Killing process %u (%s) with signal %d", 
              proc->pid, proc->name, signal);
    
    /* Mark as killed */
    proc->killed_by_signal = signal;
    exit_statistics.killed_processes++;
    
    /* Force exit with signal number as exit code */
    process_exit(proc, 128 + signal);
}

/**
 * Emergency process termination (unrecoverable errors)
 */
void process_force_kill(process_t* proc) {
    if (!proc) {
        return;
    }
    
    KLOG_ERROR(LOG_CAT_PROCESS, "Force killing process %u (%s)", proc->pid, proc->name);
    
    /* Skip normal cleanup for emergency termination */
    proc->state = PROCESS_STATE_TERMINATED;
    
    /* Basic cleanup only */
    if (proc->address_space) {
        vmm_destroy_address_space(proc->address_space);
        proc->address_space = NULL;
    }
    
    /* Remove from all queues */
    process_remove_from_ready_queue(proc);
    process_remove_from_wait_queue(proc);
    
    /* Clear the process slot */
    process_free_slot(proc);
    
    KLOG_INFO(LOG_CAT_PROCESS, "Process %u force killed", proc->pid);
}

/* ========================== Resource Cleanup Functions ========================== */

/**
 * Clean up all file descriptors for a process
 */
int process_cleanup_files(process_t* proc) {
    if (!proc) {
        return -1;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Cleaning up files for process %u", proc->pid);
    
    int files_closed = 0;
    
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (proc->fds[i].fd >= 0) {
            /* Close the file descriptor */
            vfs_close(proc->fds[i].fd);
            
            /* Clear the descriptor */
            proc->fds[i].fd = -1;
            proc->fds[i].flags = 0;
            proc->fds[i].offset = 0;
            
            files_closed++;
        }
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Closed %d file descriptors for process %u", 
               files_closed, proc->pid);
    
    exit_statistics.resources_cleaned += files_closed;
    return files_closed;
}

/**
 * Clean up IPC resources for a process
 */
int process_cleanup_ipc(process_t* proc) {
    if (!proc) {
        return -1;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Cleaning up IPC resources for process %u", proc->pid);
    
    int resources_cleaned = 0;
    
    /* Clean up message queues owned by this process */
    resources_cleaned += ipc_cleanup_process_queues(proc->pid);
    
    /* Clean up shared memory segments */
    resources_cleaned += shm_cleanup_process_segments(proc->pid);
    
    /* Clean up semaphores */
    resources_cleaned += sem_cleanup_process_semaphores(proc->pid);
    
    /* Remove from all IPC wait queues */
    ipc_remove_from_all_queues(proc->pid);
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Cleaned up %d IPC resources for process %u", 
               resources_cleaned, proc->pid);
    
    exit_statistics.resources_cleaned += resources_cleaned;
    return resources_cleaned;
}

/**
 * Clean up memory for a process
 */
int process_cleanup_memory(process_t* proc) {
    if (!proc) {
        return -1;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Cleaning up memory for process %u", proc->pid);
    
    int pages_freed = 0;
    
    /* Free user space memory */
    if (proc->address_space) {
        pages_freed = vmm_cleanup_user_space(proc->address_space);
        
        /* Don't destroy address space yet - keep for zombie state */
        /* vmm_destroy_address_space() will be called when zombie is reaped */
    }
    
    /* Free any dynamically allocated process resources */
    if (proc->argv) {
        kfree(proc->argv);
        proc->argv = NULL;
    }
    
    if (proc->envp) {
        kfree(proc->envp);
        proc->envp = NULL;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Freed %d memory pages for process %u", 
               pages_freed, proc->pid);
    
    exit_statistics.resources_cleaned += pages_freed;
    return pages_freed;
}

/**
 * Clean up timers for a process
 */
int process_cleanup_timers(process_t* proc) {
    if (!proc) {
        return -1;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Cleaning up timers for process %u", proc->pid);
    
    int timers_cancelled = 0;
    
    /* Cancel all active timers */
    timers_cancelled += timer_cancel_all_for_process(proc->pid);
    
    /* Cancel alarm signals */
    if (proc->alarm_time > 0) {
        alarm_cancel(proc->pid);
        proc->alarm_time = 0;
        timers_cancelled++;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Cancelled %d timers for process %u", 
               timers_cancelled, proc->pid);
    
    exit_statistics.resources_cleaned += timers_cancelled;
    return timers_cancelled;
}

/**
 * Clean up signal state for a process
 */
int process_cleanup_signals(process_t* proc) {
    if (!proc) {
        return -1;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Cleaning up signals for process %u", proc->pid);
    
    /* Clear pending signals */
    proc->pending_signals = 0;
    proc->signal_mask = 0;
    
    /* Clear signal handlers */
    for (int i = 0; i < MAX_SIGNALS; i++) {
        proc->signal_handlers[i] = NULL;
    }
    
    /* Remove from signal delivery queues */
    signal_remove_from_delivery_queues(proc->pid);
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Cleaned up signals for process %u", proc->pid);
    
    return 0;
}

/* ========================== Parent-Child Management ========================== */

/**
 * Reparent all children of a dying process to init
 */
void process_reparent_children(process_t* parent) {
    if (!parent) {
        return;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Reparenting children of process %u", parent->pid);
    
    if (!init_process) {
        init_process = process_find_by_pid(1);
        if (!init_process) {
            KLOG_ERROR(LOG_CAT_PROCESS, "Init process not found for reparenting");
            return;
        }
    }
    
    process_t* child = parent->first_child;
    int children_reparented = 0;
    
    while (child) {
        process_t* next_child = child->next_sibling;
        
        KLOG_DEBUG(LOG_CAT_PROCESS, "Reparenting process %u from %u to init", 
                   child->pid, parent->pid);
        
        /* Remove from parent's child list */
        /* (Already being removed as we iterate) */
        
        /* Add to init's child list */
        child->parent = init_process;
        child->ppid = 1;
        child->next_sibling = init_process->first_child;
        init_process->first_child = child;
        
        children_reparented++;
        child = next_child;
    }
    
    /* Clear parent's child list */
    parent->first_child = NULL;
    
    if (children_reparented > 0) {
        exit_statistics.orphan_count += children_reparented;
        KLOG_INFO(LOG_CAT_PROCESS, "Reparented %d children of process %u to init", 
                  children_reparented, parent->pid);
    }
}

/**
 * Notify parent process of child exit
 */
void process_notify_parent(process_t* child, int exit_status) {
    if (!child || !child->parent) {
        return;
    }
    
    process_t* parent = child->parent;
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Notifying parent %u of child %u exit (status %d)", 
               parent->pid, child->pid, exit_status);
    
    /* Queue SIGCHLD signal to parent */
    signal_queue_to_process(parent, SIGCHLD, child->pid, exit_status);
    
    /* Wake up parent if waiting for this child */
    process_wake_waiting_parent(parent, child);
    
    /* Add child to parent's zombie list */
    process_add_to_zombie_list(parent, child);
}

/**
 * Reap a zombie process (final cleanup)
 */
int process_reap_zombie(process_t* zombie) {
    if (!zombie || zombie->state != PROCESS_STATE_ZOMBIE) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Attempt to reap non-zombie process %u", 
                   zombie ? zombie->pid : 0);
        return -1;
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "Reaping zombie process %u (%s)", zombie->pid, zombie->name);
    
    /* Final memory cleanup */
    if (zombie->address_space) {
        vmm_destroy_address_space(zombie->address_space);
        zombie->address_space = NULL;
    }
    
    /* Remove from parent's zombie list */
    process_remove_from_zombie_list(zombie->parent, zombie);
    
    /* Update statistics */
    exit_statistics.zombie_count--;
    process_statistics.active_processes--;
    
    /* Free the process slot */
    process_free_slot(zombie);
    
    KLOG_INFO(LOG_CAT_PROCESS, "Zombie process %u reaped successfully", zombie->pid);
    
    return 0;
}

/* ========================== Wait System Call Support ========================== */

/**
 * Wait for any child process to exit
 */
pid_t process_wait_any(process_t* parent, int* status, int options) {
    if (!parent) {
        return -1;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Process %u waiting for any child", parent->pid);
    
    /* Check for existing zombie children first */
    process_t* zombie = process_find_zombie_child(parent);
    if (zombie) {
        pid_t zombie_pid = zombie->pid;
        if (status) {
            *status = zombie->exit_code;
        }
        
        /* Reap the zombie */
        process_reap_zombie(zombie);
        
        return zombie_pid;
    }
    
    /* No zombie children, check if parent has any children */
    if (!parent->first_child) {
        return -1; /* No children */
    }
    
    /* Check for WNOHANG option */
    if (options & WNOHANG) {
        return 0; /* No zombie children available */
    }
    
    /* Block waiting for child to exit */
    return process_block_waiting_for_child(parent, 0, status);
}

/**
 * Wait for specific child process to exit
 */
pid_t process_wait_pid(process_t* parent, pid_t pid, int* status, int options) {
    if (!parent || pid <= 0) {
        return -1;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Process %u waiting for child %u", parent->pid, pid);
    
    /* Find the specific child */
    process_t* child = process_find_child_by_pid(parent, pid);
    if (!child) {
        return -1; /* Child not found */
    }
    
    /* Check if child is already a zombie */
    if (child->state == PROCESS_STATE_ZOMBIE) {
        if (status) {
            *status = child->exit_code;
        }
        
        /* Reap the zombie */
        process_reap_zombie(child);
        
        return pid;
    }
    
    /* Check for WNOHANG option */
    if (options & WNOHANG) {
        return 0; /* Child not ready */
    }
    
    /* Block waiting for specific child */
    return process_block_waiting_for_child(parent, pid, status);
}

/* ========================== System Call Implementations ========================== */

/**
 * System call: exit
 */
void sys_exit(int status) {
    process_t* proc = process_get_current();
    if (!proc) {
        KLOG_ERROR(LOG_CAT_PROCESS, "sys_exit: No current process");
        /* Force halt since we can't exit properly */
        while (1) {
            asm volatile("hlt");
        }
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "sys_exit called by process %u with status %d", 
              proc->pid, status);
    
    process_exit(proc, status);
    
    /* This should not return - force halt */
    KLOG_ERROR(LOG_CAT_PROCESS, "sys_exit returned unexpectedly");
    while (1) {
        asm volatile("hlt");
    }
}

/**
 * System call: waitpid
 */
long sys_waitpid(pid_t pid, int* status, int options) {
    process_t* parent = process_get_current();
    if (!parent) {
        return -ESRCH;
    }
    
    /* Validate status pointer if provided */
    if (status && !validate_user_pointer(status, sizeof(int))) {
        return -EFAULT;
    }
    
    if (pid == -1) {
        /* Wait for any child */
        return process_wait_any(parent, status, options);
    } else if (pid > 0) {
        /* Wait for specific child */
        return process_wait_pid(parent, pid, status, options);
    } else {
        /* Process group waiting not yet implemented */
        return -ENOSYS;
    }
}

/**
 * System call: wait
 */
long sys_wait(int* status) {
    return sys_waitpid(-1, status, 0);
}

/* ========================== Utility Functions ========================== */

/**
 * Get process exit statistics
 */
void process_get_exit_stats(process_exit_stats_t* stats) {
    if (!stats) {
        return;
    }
    
    stats->total_exits = exit_statistics.total_exits;
    stats->normal_exits = exit_statistics.normal_exits;
    stats->killed_processes = exit_statistics.killed_processes;
    stats->zombie_count = exit_statistics.zombie_count;
    stats->orphan_count = exit_statistics.orphan_count;
    stats->resources_cleaned = exit_statistics.resources_cleaned;
}

/**
 * Initialize process exit system
 */
int process_exit_init(void) {
    KLOG_INFO(LOG_CAT_PROCESS, "Initializing process exit system");
    
    /* Initialize wait queue */
    memset(&wait_queue, 0, sizeof(wait_queue));
    
    /* Clear statistics */
    memset(&exit_statistics, 0, sizeof(exit_statistics));
    
    /* Find init process */
    init_process = process_find_by_pid(1);
    if (!init_process) {
        KLOG_WARN(LOG_CAT_PROCESS, "Init process not found during exit system init");
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "Process exit system initialized");
    return 0;
}

/**
 * Cleanup zombie processes periodically
 */
void process_cleanup_zombies(void) {
    /* This function would be called periodically by the kernel
     * to clean up zombie processes whose parents haven't waited for them
     */
    
    static uint64_t last_cleanup = 0;
    uint64_t current_time = get_system_time();
    
    /* Only run cleanup every 5 seconds */
    if (current_time - last_cleanup < 5000) {
        return;
    }
    
    last_cleanup = current_time;
    
    /* Find and clean up old zombie processes */
    int zombies_cleaned = 0;
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = process_get_by_index(i);
        if (proc && proc->state == PROCESS_STATE_ZOMBIE) {
            /* If zombie is older than 30 seconds, force reap it */
            if (current_time - proc->exit_time > 30000) {
                KLOG_WARN(LOG_CAT_PROCESS, "Force reaping old zombie process %u", proc->pid);
                process_reap_zombie(proc);
                zombies_cleaned++;
            }
        }
    }
    
    if (zombies_cleaned > 0) {
        KLOG_INFO(LOG_CAT_PROCESS, "Cleaned up %d old zombie processes", zombies_cleaned);
    }
}
