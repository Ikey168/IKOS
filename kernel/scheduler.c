/* IKOS Preemptive Task Scheduler Implementation
 * Provides Round Robin and Priority-based preemptive scheduling
 */

#include "scheduler.h"
#include "memory.h"
#include "interrupts.h"
#include <string.h>

/* Global scheduler state */
static task_t* current_task = NULL;
static task_t* idle_task = NULL;
static task_t* task_list[MAX_TASKS];
static uint32_t next_pid = 1;
static bool scheduler_enabled = false;
static scheduler_stats_t stats;

/* Ready queues for different priorities */
static task_t* ready_queues[256] = {0};  /* One queue per priority level */
static task_t* round_robin_queue = NULL;

/* Scheduler policy and configuration */
static sched_policy_t current_policy = SCHED_ROUND_ROBIN;
static uint32_t default_time_slice = TIME_SLICE_DEFAULT;

/* Timer frequency (Hz) */
#define TIMER_FREQUENCY 1000

/* Forward declarations */
static void idle_task_func(void);
static task_t* create_idle_task(void);
static void save_context(task_t* task);
static void restore_context(task_t* task);

/**
 * Initialize the scheduler
 */
int scheduler_init(sched_policy_t policy, uint32_t time_slice) {
    // Initialize scheduler state
    memset(&stats, 0, sizeof(scheduler_stats_t));
    memset(task_list, 0, sizeof(task_list));
    memset(ready_queues, 0, sizeof(ready_queues));
    
    current_policy = policy;
    default_time_slice = time_slice;
    stats.policy = policy;
    stats.time_slice = time_slice;
    
    // Create idle task
    idle_task = create_idle_task();
    if (!idle_task) {
        return -1;
    }
    
    current_task = idle_task;
    scheduler_enabled = false;
    
    // Setup timer interrupt for preemption
    setup_timer_interrupt(TIMER_FREQUENCY);
    
    return 0;
}

/**
 * Start the scheduler
 */
void scheduler_start(void) {
    scheduler_enabled = true;
    // Enable timer interrupts
    enable_interrupts();
}

/**
 * Stop the scheduler
 */
void scheduler_stop(void) {
    scheduler_enabled = false;
    // Disable timer interrupts
    disable_interrupts();
}

/**
 * Create a new task
 */
task_t* task_create(const char* name, void* entry_point, uint8_t priority, uint32_t stack_size) {
    // Find free task slot
    uint32_t slot = 0;
    for (slot = 0; slot < MAX_TASKS; slot++) {
        if (task_list[slot] == NULL) {
            break;
        }
    }
    
    if (slot >= MAX_TASKS) {
        return NULL; // No free slots
    }
    
    // Allocate task structure
    task_t* task = (task_t*)kmalloc(sizeof(task_t));
    if (!task) {
        return NULL;
    }
    
    // Initialize task
    memset(task, 0, sizeof(task_t));
    task->pid = next_pid++;
    strncpy(task->name, name, sizeof(task->name) - 1);
    task->state = TASK_READY;
    task->priority = priority;
    task->quantum = default_time_slice;
    task->time_slice = default_time_slice;
    
    // Setup memory
    if (task_setup_memory(task, stack_size) != 0) {
        kfree(task);
        return NULL;
    }
    
    // Initialize context
    memset(&task->context, 0, sizeof(task->context));
    task->context.rip = (uint64_t)entry_point;
    task->context.rsp = task->stack_base + task->stack_size - 8; // Stack grows down
    task->context.rflags = 0x202; // Interrupts enabled
    task->context.cs = 0x18; // 64-bit code segment
    task->context.ds = task->context.es = task->context.fs = 
    task->context.gs = task->context.ss = 0x20; // 64-bit data segment
    
    // Add to task list
    task_list[slot] = task;
    stats.active_tasks++;
    
    // Add to ready queue
    ready_queue_add(task);
    
    return task;
}

/**
 * Destroy a task
 */
int task_destroy(uint32_t pid) {
    task_t* task = task_get_by_pid(pid);
    if (!task || task == idle_task) {
        return -1; // Can't destroy idle task or non-existent task
    }
    
    // Remove from queues
    ready_queue_remove(task);
    
    // Free memory
    task_free_stack((void*)task->stack_base, task->stack_size);
    
    // Remove from task list
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_list[i] == task) {
            task_list[i] = NULL;
            break;
        }
    }
    
    // If this is the current task, schedule next
    if (task == current_task) {
        task->state = TASK_TERMINATED;
        schedule();
    }
    
    kfree(task);
    stats.active_tasks--;
    
    return 0;
}

/**
 * Main scheduler function - picks next task to run
 */
void schedule(void) {
    if (!scheduler_enabled) {
        return;
    }
    
    task_t* prev_task = current_task;
    task_t* next_task = scheduler_pick_next();
    
    if (next_task != prev_task) {
        context_switch(prev_task, next_task);
    }
}

/**
 * Timer interrupt handler - called every timer tick
 */
void scheduler_tick(void) {
    stats.total_interrupts++;
    
    if (!scheduler_enabled || !current_task) {
        return;
    }
    
    // Update current task time
    current_task->cpu_time++;
    
    // Decrement time slice for current task
    if (current_task->time_slice > 0) {
        current_task->time_slice--;
    }
    
    // Check if time slice expired (for Round Robin)
    if (current_policy == SCHED_ROUND_ROBIN && current_task->time_slice == 0) {
        // Reset time slice
        current_task->time_slice = current_task->quantum;
        
        // Move to end of ready queue and schedule
        if (current_task->state == TASK_RUNNING) {
            current_task->state = TASK_READY;
            ready_queue_add(current_task);
        }
        
        schedule();
    }
}

/**
 * Pick next task to run based on scheduling policy
 */
task_t* scheduler_pick_next(void) {
    task_t* next_task = NULL;
    
    switch (current_policy) {
        case SCHED_ROUND_ROBIN:
            next_task = rr_pick_next();
            break;
        case SCHED_PRIORITY:
            next_task = priority_pick_next();
            break;
        default:
            next_task = rr_pick_next();
            break;
    }
    
    // Fallback to idle task if no other task available
    if (!next_task) {
        next_task = idle_task;
    }
    
    return next_task;
}

/**
 * Round Robin scheduler - pick next task in round robin fashion
 */
task_t* rr_pick_next(void) {
    return ready_queue_next();
}

/**
 * Priority-based scheduler - pick highest priority ready task
 */
task_t* priority_pick_next(void) {
    // Find highest priority ready task
    for (int priority = PRIORITY_HIGHEST; priority <= PRIORITY_LOWEST; priority++) {
        if (ready_queues[priority] != NULL) {
            task_t* task = ready_queues[priority];
            ready_queue_remove(task);
            return task;
        }
    }
    return NULL;
}

/**
 * Context switch between tasks
 */
void context_switch(task_t* prev, task_t* next) {
    if (prev == next) {
        return;
    }
    
    stats.total_switches++;
    
    // Save previous task context
    if (prev && prev->state == TASK_RUNNING) {
        save_context(prev);
        prev->state = TASK_READY;
        prev->switches++;
    }
    
    // Set new current task
    current_task = next;
    next->state = TASK_RUNNING;
    next->switches++;
    
    // Restore new task context
    restore_context(next);
}

/**
 * Add task to ready queue
 */
void ready_queue_add(task_t* task) {
    if (!task || task->state != TASK_READY) {
        return;
    }
    
    if (current_policy == SCHED_PRIORITY) {
        // Add to priority-specific queue
        uint8_t priority = task->priority;
        task->next = ready_queues[priority];
        if (ready_queues[priority]) {
            ready_queues[priority]->prev = task;
        }
        ready_queues[priority] = task;
        task->prev = NULL;
    } else {
        // Add to round robin queue
        if (round_robin_queue == NULL) {
            round_robin_queue = task;
            task->next = task->prev = task; // Circular list
        } else {
            task->next = round_robin_queue;
            task->prev = round_robin_queue->prev;
            round_robin_queue->prev->next = task;
            round_robin_queue->prev = task;
        }
    }
    
    stats.ready_tasks++;
}

/**
 * Remove task from ready queue
 */
void ready_queue_remove(task_t* task) {
    if (!task) {
        return;
    }
    
    if (current_policy == SCHED_PRIORITY) {
        // Remove from priority queue
        uint8_t priority = task->priority;
        if (ready_queues[priority] == task) {
            ready_queues[priority] = task->next;
        }
        if (task->prev) {
            task->prev->next = task->next;
        }
        if (task->next) {
            task->next->prev = task->prev;
        }
    } else {
        // Remove from round robin queue
        if (task->next == task) {
            // Only task in queue
            round_robin_queue = NULL;
        } else {
            task->prev->next = task->next;
            task->next->prev = task->prev;
            if (round_robin_queue == task) {
                round_robin_queue = task->next;
            }
        }
    }
    
    task->next = task->prev = NULL;
    stats.ready_tasks--;
}

/**
 * Get next task from ready queue
 */
task_t* ready_queue_next(void) {
    if (current_policy == SCHED_PRIORITY) {
        return priority_pick_next();
    }
    
    if (round_robin_queue == NULL) {
        return NULL;
    }
    
    task_t* task = round_robin_queue;
    ready_queue_remove(task);
    return task;
}

/**
 * Get current running task
 */
task_t* task_get_current(void) {
    return current_task;
}

/**
 * Get task by PID
 */
task_t* task_get_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_list[i] && task_list[i]->pid == pid) {
            return task_list[i];
        }
    }
    return NULL;
}

/**
 * System call: yield CPU voluntarily
 */
void sys_yield(void) {
    if (current_task && current_task != idle_task) {
        current_task->state = TASK_READY;
        ready_queue_add(current_task);
        schedule();
    }
}

/**
 * Get scheduler statistics
 */
scheduler_stats_t* get_scheduler_stats(void) {
    return &stats;
}

/**
 * Create idle task
 */
static task_t* create_idle_task(void) {
    task_t* task = (task_t*)kmalloc(sizeof(task_t));
    if (!task) {
        return NULL;
    }
    
    memset(task, 0, sizeof(task_t));
    task->pid = 0; // Idle task always has PID 0
    strcpy(task->name, "idle");
    task->state = TASK_RUNNING;
    task->priority = PRIORITY_LOWEST;
    task->quantum = 1;
    task->time_slice = 1;
    
    // Setup minimal stack for idle task
    task->stack_size = 4096; // 4KB stack
    task->stack_base = (uint64_t)kmalloc(task->stack_size);
    if (!task->stack_base) {
        kfree(task);
        return NULL;
    }
    
    // Initialize context for idle task
    task->context.rip = (uint64_t)idle_task_func;
    task->context.rsp = task->stack_base + task->stack_size - 8;
    task->context.rflags = 0x202;
    task->context.cs = 0x18;
    task->context.ds = task->context.es = task->context.fs = 
    task->context.gs = task->context.ss = 0x20;
    
    return task;
}

/**
 * Idle task function - runs when no other tasks are ready
 */
static void idle_task_func(void) {
    while (1) {
        // Halt CPU until next interrupt
        asm volatile("hlt");
    }
}

/**
 * Save CPU context to task structure
 */
static void save_context(task_t* task) {
    // This would be implemented in assembly
    // For now, this is a placeholder
    // The actual implementation would save all CPU registers
    // to the task's context structure
}

/**
 * Restore CPU context from task structure
 */
static void restore_context(task_t* task) {
    // This would be implemented in assembly
    // For now, this is a placeholder
    // The actual implementation would restore all CPU registers
    // from the task's context structure
}

/**
 * Timer interrupt handler (called from interrupt.asm)
 */
void timer_interrupt_handler(void) {
    scheduler_tick();
    // Send EOI to PIC
    outb(0x20, 0x20);
}
