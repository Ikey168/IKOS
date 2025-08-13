/* IKOS Preemptive Task Scheduler
 * Implements Round Robin and Priority-based preemptive scheduling
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

/* Task States */
typedef enum {
    TASK_RUNNING = 0,
    TASK_READY,
    TASK_BLOCKED,
    TASK_TERMINATED
} task_state_t;

/* Scheduling Policies */
typedef enum {
    SCHED_ROUND_ROBIN = 0,
    SCHED_PRIORITY,
    SCHED_FIFO
} sched_policy_t;

/* Task Priority Levels (0 = highest, 255 = lowest) */
#define PRIORITY_HIGHEST    0
#define PRIORITY_HIGH       64
#define PRIORITY_NORMAL     128
#define PRIORITY_LOW        192
#define PRIORITY_LOWEST     255

/* Time slice for Round Robin scheduling (in timer ticks) */
#define TIME_SLICE_DEFAULT  10
#define TIME_SLICE_MIN      1
#define TIME_SLICE_MAX      100

/* Maximum number of tasks */
#define MAX_TASKS           64

/* Task Control Block (TCB) */
typedef struct task {
    uint32_t pid;                   /* Process ID */
    char name[32];                  /* Task name */
    task_state_t state;             /* Current state */
    uint8_t priority;               /* Task priority (0-255) */
    uint32_t time_slice;            /* Remaining time slice */
    uint32_t quantum;               /* Original quantum size */
    
    /* CPU context */
    struct {
        uint64_t rax, rbx, rcx, rdx;
        uint64_t rsi, rdi, rbp, rsp;
        uint64_t r8, r9, r10, r11;
        uint64_t r12, r13, r14, r15;
        uint64_t rip, rflags;
        uint16_t cs, ds, es, fs, gs, ss;
        uint64_t cr3;               /* Page directory pointer */
    } context;
    
    /* Memory management */
    uint64_t stack_base;            /* Stack base address */
    uint64_t stack_size;            /* Stack size */
    uint64_t heap_base;             /* Heap base address */
    uint64_t heap_size;             /* Heap size */
    
    /* Scheduling statistics */
    uint64_t cpu_time;              /* Total CPU time used */
    uint64_t start_time;            /* Task start time */
    uint32_t switches;              /* Number of context switches */
    
    /* Linked list pointers */
    struct task *next;              /* Next task in queue */
    struct task *prev;              /* Previous task in queue */
} task_t;

/* Scheduler statistics */
typedef struct {
    uint64_t total_switches;        /* Total context switches */
    uint64_t total_interrupts;      /* Total timer interrupts */
    uint32_t active_tasks;          /* Number of active tasks */
    uint32_t ready_tasks;           /* Number of ready tasks */
    sched_policy_t policy;          /* Current scheduling policy */
    uint32_t time_slice;            /* Current time slice */
} scheduler_stats_t;

/* Function prototypes */

/* Scheduler initialization */
int scheduler_init(sched_policy_t policy, uint32_t time_slice);
void scheduler_start(void);
void scheduler_stop(void);

/* Task management */
task_t* task_create(const char* name, void* entry_point, uint8_t priority, uint32_t stack_size);
int task_destroy(uint32_t pid);
int task_suspend(uint32_t pid);
int task_resume(uint32_t pid);
task_t* task_get_current(void);
task_t* task_get_by_pid(uint32_t pid);

/* Scheduling functions */
void schedule(void);
void scheduler_tick(void);
void context_switch(task_t* prev, task_t* next);
task_t* scheduler_pick_next(void);

/* Queue management */
void ready_queue_add(task_t* task);
void ready_queue_remove(task_t* task);
task_t* ready_queue_next(void);

/* Priority management */
int task_set_priority(uint32_t pid, uint8_t priority);
uint8_t task_get_priority(uint32_t pid);
void priority_boost(void);

/* Round Robin functions */
void rr_schedule(void);
task_t* rr_pick_next(void);

/* Priority-based functions */
void priority_schedule(void);
task_t* priority_pick_next(void);

/* Timer interrupt handling */
void timer_interrupt_handler(void);
void setup_timer_interrupt(uint32_t frequency);

/* System calls */
void sys_yield(void);
void sys_sleep(uint32_t milliseconds);
void sys_exit(int status);

/* Statistics and debugging */
scheduler_stats_t* get_scheduler_stats(void);
void print_task_list(void);
void print_scheduler_stats(void);

/* Memory management for tasks */
void* task_alloc_stack(uint32_t size);
void task_free_stack(void* stack, uint32_t size);
int task_setup_memory(task_t* task, uint32_t stack_size);

#endif /* SCHEDULER_H */
