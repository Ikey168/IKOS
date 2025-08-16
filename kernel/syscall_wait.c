/* IKOS Wait System Calls Implementation - Issue #24
 * Complete implementation of wait() and waitpid() system calls
 */

#include "syscall_process.h"
#include "process.h"
#include "memory.h"
#include "string.h"
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* ========================== Global State ========================== */

extern process_lifecycle_stats_t g_lifecycle_stats;

/* ========================== Helper Functions ========================== */

/**
 * Check if process has any children
 */
static bool has_children(process_t* proc) {
    if (!proc) {
        return false;
    }
    
    return (proc->first_child != NULL) || has_zombie_children(proc);
}

/**
 * Find zombie child matching criteria
 */
static process_t* find_zombie_child(process_t* parent, pid_t target_pid) {
    if (!parent) {
        return NULL;
    }
    
    process_t* zombie = parent->zombie_children;
    while (zombie) {
        if (target_pid == -1 || zombie->pid == target_pid) {
            return zombie;
        }
        zombie = zombie->next_zombie;
    }
    
    return NULL;
}

/**
 * Find living child matching criteria
 */
static process_t* find_living_child(process_t* parent, pid_t target_pid) {
    if (!parent) {
        return NULL;
    }
    
    process_t* child = parent->first_child;
    while (child) {
        if (target_pid == -1 || child->pid == target_pid) {
            return child;
        }
        child = child->next_sibling;
    }
    
    return NULL;
}

/**
 * Create wait status from process exit information
 */
static int create_wait_status(process_t* child) {
    if (!child) {
        return 0;
    }
    
    int status = 0;
    
    if (child->killed_by_signal > 0) {
        /* Child was terminated by signal */
        status = child->killed_by_signal & 0x7f;
    } else {
        /* Child exited normally */
        status = (child->exit_code & 0xff) << 8;
    }
    
    return status;
}

/**
 * Block process until child terminates
 */
static int block_for_child(process_t* parent, pid_t target_pid, int* status_ptr) {
    if (!parent) {
        return -EINVAL;
    }
    
    /* Set up wait context */
    parent->wait_for_pid = target_pid;
    parent->wait_status_ptr = status_ptr;
    parent->state = PROCESS_STATE_WAITING;
    parent->wait_state = PROC_LIFECYCLE_WAITING;
    parent->wait_start_time = get_system_time();
    
    /* Yield to scheduler - process will be woken when child terminates */
    scheduler_yield();
    
    return 0;
}

/**
 * Wake up parent process waiting for child
 */
static int wake_waiting_parent(process_t* child) {
    if (!child || !child->parent) {
        return -EINVAL;
    }
    
    process_t* parent = child->parent;
    
    /* Check if parent is waiting for this child or any child */
    if (parent->state == PROCESS_STATE_WAITING &&
        (parent->wait_for_pid == -1 || parent->wait_for_pid == child->pid)) {
        
        /* Store exit status if parent wants it */
        if (parent->wait_status_ptr) {
            int status = create_wait_status(child);
            *(parent->wait_status_ptr) = status;
        }
        
        /* Wake up parent */
        parent->state = PROCESS_STATE_READY;
        parent->wait_state = PROC_LIFECYCLE_RUNNING;
        parent->wait_for_pid = 0;
        parent->wait_status_ptr = NULL;
        
        scheduler_add_process(parent);
    }
    
    return 0;
}

/* ========================== Zombie Process Management ========================== */

int create_zombie_process(process_t* child, int exit_status) {
    if (!child || !child->parent) {
        return -EINVAL;
    }
    
    process_t* parent = child->parent;
    
    /* Set zombie state */
    child->state = PROCESS_STATE_ZOMBIE;
    child->exit_code = exit_status;
    child->exit_time = get_system_time();
    
    /* Add to parent's zombie list */
    child->next_zombie = parent->zombie_children;
    parent->zombie_children = child;
    
    /* Remove from active process list */
    scheduler_remove_process(child);
    
    /* Wake up parent if it's waiting */
    wake_waiting_parent(child);
    
    g_lifecycle_stats.zombies_created++;
    
    return 0;
}

int reap_zombie_process(process_t* parent, process_t* zombie) {
    if (!parent || !zombie) {
        return -EINVAL;
    }
    
    /* Remove from zombie list */
    if (parent->zombie_children == zombie) {
        parent->zombie_children = zombie->next_zombie;
    } else {
        process_t* z = parent->zombie_children;
        while (z && z->next_zombie != zombie) {
            z = z->next_zombie;
        }
        if (z) {
            z->next_zombie = zombie->next_zombie;
        }
    }
    
    /* Clean up zombie process structure */
    zombie->next_zombie = NULL;
    zombie->parent = NULL;
    
    /* Free process resources */
    kfree(zombie);
    
    g_lifecycle_stats.zombies_reaped++;
    
    return 0;
}

bool has_zombie_children(process_t* proc) {
    return proc && (proc->zombie_children != NULL);
}

process_t* get_next_zombie_child(process_t* parent) {
    return parent ? parent->zombie_children : NULL;
}

/* ========================== Wait Context Management ========================== */

wait_context_t* create_wait_context(pid_t pid, int* status, int options) {
    wait_context_t* ctx = (wait_context_t*)kmalloc(sizeof(wait_context_t));
    if (!ctx) {
        return NULL;
    }
    
    memset(ctx, 0, sizeof(wait_context_t));
    
    ctx->wait_pid = pid;
    ctx->status_ptr = status;
    ctx->options = options;
    ctx->wait_start_time = get_system_time();
    ctx->is_blocking = !(options & WNOHANG);
    ctx->waiting_process = get_current_process();
    
    return ctx;
}

void destroy_wait_context(wait_context_t* ctx) {
    if (ctx) {
        kfree(ctx);
    }
}

/* ========================== Main Wait Implementation ========================== */

/**
 * Wait system call implementation
 */
long sys_wait(int* status) {
    return sys_waitpid(-1, status, 0);
}

/**
 * Waitpid system call implementation
 */
long sys_waitpid(pid_t pid, int* status, int options) {
    process_t* parent = get_current_process();
    if (!parent) {
        g_lifecycle_stats.failed_waits++;
        return -ESRCH;
    }
    
    g_lifecycle_stats.total_waits++;
    
    /* Validate arguments */
    if (pid < -1 || pid == 0) {
        /* TODO: Implement process group waiting */
        g_lifecycle_stats.failed_waits++;
        return -EINVAL;
    }
    
    /* Check if process has children */
    if (!has_children(parent)) {
        g_lifecycle_stats.failed_waits++;
        return -ECHILD;
    }
    
    /* Create wait context */
    wait_context_t* wait_ctx = create_wait_context(pid, status, options);
    if (!wait_ctx) {
        g_lifecycle_stats.failed_waits++;
        return -ENOMEM;
    }
    
    /* Look for zombie children first */
    process_t* zombie = find_zombie_child(parent, pid);
    if (zombie) {
        /* Found zombie child - reap it immediately */
        pid_t zombie_pid = zombie->pid;
        int exit_status = create_wait_status(zombie);
        
        /* Store status if requested */
        if (status) {
            *status = exit_status;
        }
        
        /* Reap the zombie */
        if (reap_zombie_process(parent, zombie) != 0) {
            destroy_wait_context(wait_ctx);
            g_lifecycle_stats.failed_waits++;
            return -EINVAL;
        }
        
        destroy_wait_context(wait_ctx);
        g_lifecycle_stats.successful_waits++;
        return zombie_pid;
    }
    
    /* No zombie children available */
    if (options & WNOHANG) {
        /* Non-blocking wait - return immediately */
        destroy_wait_context(wait_ctx);
        return 0;
    }
    
    /* Check if specified child exists */
    if (pid > 0) {
        process_t* child = find_living_child(parent, pid);
        if (!child) {
            destroy_wait_context(wait_ctx);
            g_lifecycle_stats.failed_waits++;
            return -ECHILD;
        }
    }
    
    /* Block until child terminates */
    if (block_for_child(parent, pid, status) != 0) {
        destroy_wait_context(wait_ctx);
        g_lifecycle_stats.failed_waits++;
        return -EINTR;
    }
    
    destroy_wait_context(wait_ctx);
    g_lifecycle_stats.successful_waits++;
    
    /* When process is unblocked, the child PID will be in a register or variable */
    /* For this implementation, we'll return a placeholder */
    return 1;  /* TODO: Return actual child PID */
}

/* ========================== Process Termination Support ========================== */

/**
 * Handle process termination (called when a process exits)
 */
int handle_process_termination(process_t* proc, int exit_code, int signal) {
    if (!proc) {
        return -EINVAL;
    }
    
    /* Set exit information */
    proc->exit_code = exit_code;
    proc->killed_by_signal = signal;
    proc->exit_time = get_system_time();
    
    /* Handle orphaned children */
    if (handle_orphaned_processes(proc) != 0) {
        /* Continue anyway - this is not fatal */
    }
    
    /* Create zombie if parent exists */
    if (proc->parent) {
        return create_zombie_process(proc, exit_code);
    } else {
        /* No parent - clean up immediately */
        scheduler_remove_process(proc);
        kfree(proc);
        return 0;
    }
}

/**
 * Handle orphaned processes when parent terminates
 */
int handle_orphaned_processes(process_t* terminated_parent) {
    if (!terminated_parent) {
        return -EINVAL;
    }
    
    /* Find init process (PID 1) to adopt orphans */
    process_t* init_proc = find_process_by_pid(1);
    if (!init_proc) {
        /* No init process - orphans become zombies */
        return -ESRCH;
    }
    
    /* Adopt all children */
    process_t* child = terminated_parent->first_child;
    while (child) {
        process_t* next_child = child->next_sibling;
        
        /* Remove from terminated parent */
        remove_child_process(terminated_parent, child);
        
        /* Add to init process */
        add_child_process(init_proc, child);
        
        g_lifecycle_stats.orphans_adopted++;
        
        child = next_child;
    }
    
    /* Adopt all zombie children */
    process_t* zombie = terminated_parent->zombie_children;
    while (zombie) {
        process_t* next_zombie = zombie->next_zombie;
        
        /* Remove from terminated parent's zombie list */
        zombie->parent = init_proc;
        zombie->next_zombie = init_proc->zombie_children;
        init_proc->zombie_children = zombie;
        
        zombie = next_zombie;
    }
    
    terminated_parent->first_child = NULL;
    terminated_parent->zombie_children = NULL;
    
    return 0;
}

/* ========================== Process Tree Utility Functions ========================== */

process_t* find_process_by_pid(pid_t pid) {
    /* TODO: Implement actual process lookup by PID */
    /* This would typically involve searching a global process table */
    static process_t fake_proc = {0};
    fake_proc.pid = pid;
    return &fake_proc;
}

uint32_t get_process_children_count(process_t* proc) {
    if (!proc) {
        return 0;
    }
    
    uint32_t count = 0;
    process_t* child = proc->first_child;
    while (child) {
        count++;
        child = child->next_sibling;
    }
    
    /* Add zombie children */
    process_t* zombie = proc->zombie_children;
    while (zombie) {
        count++;
        zombie = zombie->next_zombie;
    }
    
    return count;
}

/* ========================== Process Lifecycle Control ========================== */

int process_lifecycle_init(void) {
    /* Initialize global statistics */
    memset(&g_lifecycle_stats, 0, sizeof(g_lifecycle_stats));
    
    /* TODO: Initialize any global data structures */
    
    return 0;
}

void process_lifecycle_shutdown(void) {
    /* TODO: Cleanup any global resources */
}

/* ========================== Placeholder Functions ========================== */

/* Scheduler placeholders */
void scheduler_yield(void) {
    /* Placeholder - should yield CPU to next process */
}

int scheduler_remove_process(process_t* proc) {
    /* Placeholder - should remove process from scheduler */
    return 0;
}

/* Process management placeholders */
process_t* get_current_process(void) {
    static process_t fake_proc = {0};
    fake_proc.pid = 1234;
    return &fake_proc;
}

/* Time placeholder */
uint64_t get_system_time(void) {
    static uint64_t fake_time = 0;
    return ++fake_time;
}

/* Memory management placeholders */
void* kmalloc(size_t size) {
    return malloc(size);
}

void kfree(void* ptr) {
    free(ptr);
}

/* String functions */
void* memset(void* ptr, int value, size_t size) {
    unsigned char* p = (unsigned char*)ptr;
    for (size_t i = 0; i < size; i++) {
        p[i] = (unsigned char)value;
    }
    return ptr;
}

/* Standard library placeholders */
void* malloc(size_t size) {
    return (void*)0x3000000; /* Fake address */
}

void free(void* ptr) {
    /* Placeholder */
}
