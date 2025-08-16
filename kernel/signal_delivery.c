/* IKOS Signal Delivery Engine Implementation - Issue #19
 * Comprehensive signal delivery, queuing, and management system
 */

#include "signal_delivery.h"
#include "signal_mask.h"
#include "process.h"
#include "memory.h"
#include "string.h"
#include "assert.h"
#include "kernel_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Missing defines */
#ifndef KLOG_DEBUG
#define KLOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef KLOG_INFO
#define KLOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef KLOG_ERROR
#define KLOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif

/* Missing spinlock defines */
/* Define spinlock type if not available */
#ifndef _KERNEL_SPINLOCK_T_DEFINED
#define _KERNEL_SPINLOCK_T_DEFINED
typedef struct {
    volatile int locked;
} kernel_spinlock_t;
#endif

/* Define missing types if not available */
#ifndef _KERNEL_UID_T_DEFINED
#define _KERNEL_UID_T_DEFINED
typedef unsigned int kernel_uid_t;
#endif

#ifndef _KERNEL_CLOCK_T_DEFINED
#define _KERNEL_CLOCK_T_DEFINED
typedef long kernel_clock_t;
#endif

/* Forward declarations for functions implemented elsewhere */
static uint64_t get_current_time_us(void);
static void kernel_spin_lock(kernel_spinlock_t* lock);
static void kernel_spin_unlock(kernel_spinlock_t* lock);
static void kernel_spinlock_init(kernel_spinlock_t* lock);

/* Missing process states */
#ifndef PROCESS_RUNNING
#define PROCESS_RUNNING     1
#define PROCESS_READY       2
#define PROCESS_BLOCKED     3
#define PROCESS_TERMINATED  4
#define PROCESS_STOPPED     5
#endif

/* Signal definitions */
#ifndef SIGHUP
#define SIGHUP      1
#define SIGINT      2
#define SIGQUIT     3
#define SIGILL      4
#define SIGTRAP     5
#define SIGABRT     6
#define SIGBUS      7
#define SIGFPE      8
#define SIGKILL     9
#define SIGUSR1     10
#define SIGSEGV     11
#define SIGUSR2     12
#define SIGPIPE     13
#define SIGALRM     14
#define SIGTERM     15
#define SIGSTKFLT   16
#define SIGCHLD     17
#define SIGCONT     18
#define SIGSTOP     19
#define SIGTSTP     20
#define SIGTTIN     21
#define SIGTTOU     22
#define SIGURG      23
#define SIGXCPU     24
#define SIGXFSZ     25
#define SIGVTALRM   26
#define SIGPROF     27
#define SIGWINCH    28
#define SIGPOLL     29
#define SIGPWR      30
#define SIGSYS      31
#endif

/* ========================== Constants and Globals ========================== */

/* Global signal delivery manager */
static signal_delivery_manager_t g_signal_manager;

/* Signal name table */
static const char* g_signal_names[64] = {
    NULL,           /* 0 - Invalid */
    "SIGHUP",       /* 1 */
    "SIGINT",       /* 2 */
    "SIGQUIT",      /* 3 */
    "SIGILL",       /* 4 */
    "SIGTRAP",      /* 5 */
    "SIGABRT",      /* 6 */
    "SIGBUS",       /* 7 */
    "SIGFPE",       /* 8 */
    "SIGKILL",      /* 9 */
    "SIGUSR1",      /* 10 */
    "SIGSEGV",      /* 11 */
    "SIGUSR2",      /* 12 */
    "SIGPIPE",      /* 13 */
    "SIGALRM",      /* 14 */
    "SIGTERM",      /* 15 */
    "SIGSTKFLT",    /* 16 */
    "SIGCHLD",      /* 17 */
    "SIGCONT",      /* 18 */
    "SIGSTOP",      /* 19 */
    "SIGTSTP",      /* 20 */
    "SIGTTIN",      /* 21 */
    "SIGTTOU",      /* 22 */
    "SIGURG",       /* 23 */
    "SIGXCPU",      /* 24 */
    "SIGXFSZ",      /* 25 */
    "SIGVTALRM",    /* 26 */
    "SIGPROF",      /* 27 */
    "SIGWINCH",     /* 28 */
    "SIGPOLL",      /* 29 */
    "SIGPWR",       /* 30 */
    "SIGSYS",       /* 31 */
    /* RT signals 32-63 */
    "SIGRT0", "SIGRT1", "SIGRT2", "SIGRT3", "SIGRT4", "SIGRT5", "SIGRT6", "SIGRT7",
    "SIGRT8", "SIGRT9", "SIGRT10", "SIGRT11", "SIGRT12", "SIGRT13", "SIGRT14", "SIGRT15",
    "SIGRT16", "SIGRT17", "SIGRT18", "SIGRT19", "SIGRT20", "SIGRT21", "SIGRT22", "SIGRT23",
    "SIGRT24", "SIGRT25", "SIGRT26", "SIGRT27", "SIGRT28", "SIGRT29", "SIGRT30", "SIGRT31"
};

/* Signal priority table */
static const uint8_t g_signal_priorities[64] = {
    SIGNAL_PRIORITY_NORMAL,     /* 0 - Invalid */
    SIGNAL_PRIORITY_LOW,        /* SIGHUP */
    SIGNAL_PRIORITY_NORMAL,     /* SIGINT */
    SIGNAL_PRIORITY_NORMAL,     /* SIGQUIT */
    SIGNAL_PRIORITY_HIGH,       /* SIGILL */
    SIGNAL_PRIORITY_HIGH,       /* SIGTRAP */
    SIGNAL_PRIORITY_HIGH,       /* SIGABRT */
    SIGNAL_PRIORITY_HIGH,       /* SIGBUS */
    SIGNAL_PRIORITY_HIGH,       /* SIGFPE */
    SIGNAL_PRIORITY_CRITICAL,   /* SIGKILL */
    SIGNAL_PRIORITY_NORMAL,     /* SIGUSR1 */
    SIGNAL_PRIORITY_HIGH,       /* SIGSEGV */
    SIGNAL_PRIORITY_NORMAL,     /* SIGUSR2 */
    SIGNAL_PRIORITY_NORMAL,     /* SIGPIPE */
    SIGNAL_PRIORITY_NORMAL,     /* SIGALRM */
    SIGNAL_PRIORITY_NORMAL,     /* SIGTERM */
    SIGNAL_PRIORITY_HIGH,       /* SIGSTKFLT */
    SIGNAL_PRIORITY_LOW,        /* SIGCHLD */
    SIGNAL_PRIORITY_NORMAL,     /* SIGCONT */
    SIGNAL_PRIORITY_CRITICAL,   /* SIGSTOP */
    SIGNAL_PRIORITY_NORMAL,     /* SIGTSTP */
    SIGNAL_PRIORITY_NORMAL,     /* SIGTTIN */
    SIGNAL_PRIORITY_NORMAL,     /* SIGTTOU */
    SIGNAL_PRIORITY_LOW,        /* SIGURG */
    SIGNAL_PRIORITY_NORMAL,     /* SIGXCPU */
    SIGNAL_PRIORITY_NORMAL,     /* SIGXFSZ */
    SIGNAL_PRIORITY_NORMAL,     /* SIGVTALRM */
    SIGNAL_PRIORITY_NORMAL,     /* SIGPROF */
    SIGNAL_PRIORITY_LOW,        /* SIGWINCH */
    SIGNAL_PRIORITY_NORMAL,     /* SIGPOLL */
    SIGNAL_PRIORITY_NORMAL,     /* SIGPWR */
    SIGNAL_PRIORITY_HIGH,       /* SIGSYS */
    /* RT signals have priority SIGNAL_PRIORITY_RT_BASE + signal - 32 */
    SIGNAL_PRIORITY_RT_BASE, SIGNAL_PRIORITY_RT_BASE+1, SIGNAL_PRIORITY_RT_BASE+2, SIGNAL_PRIORITY_RT_BASE+3,
    SIGNAL_PRIORITY_RT_BASE+4, SIGNAL_PRIORITY_RT_BASE+5, SIGNAL_PRIORITY_RT_BASE+6, SIGNAL_PRIORITY_RT_BASE+7,
    SIGNAL_PRIORITY_RT_BASE+8, SIGNAL_PRIORITY_RT_BASE+9, SIGNAL_PRIORITY_RT_BASE+10, SIGNAL_PRIORITY_RT_BASE+11,
    SIGNAL_PRIORITY_RT_BASE+12, SIGNAL_PRIORITY_RT_BASE+13, SIGNAL_PRIORITY_RT_BASE+14, SIGNAL_PRIORITY_RT_BASE+15,
    SIGNAL_PRIORITY_RT_BASE+16, SIGNAL_PRIORITY_RT_BASE+17, SIGNAL_PRIORITY_RT_BASE+18, SIGNAL_PRIORITY_RT_BASE+19,
    SIGNAL_PRIORITY_RT_BASE+20, SIGNAL_PRIORITY_RT_BASE+21, SIGNAL_PRIORITY_RT_BASE+22, SIGNAL_PRIORITY_RT_BASE+23,
    SIGNAL_PRIORITY_RT_BASE+24, SIGNAL_PRIORITY_RT_BASE+25, SIGNAL_PRIORITY_RT_BASE+26, SIGNAL_PRIORITY_RT_BASE+27,
    SIGNAL_PRIORITY_RT_BASE+28, SIGNAL_PRIORITY_RT_BASE+29, SIGNAL_PRIORITY_RT_BASE+30, SIGNAL_PRIORITY_RT_BASE+31
};

/* Signals that can be coalesced (non-RT signals mostly) */
static const uint64_t g_coalescable_signals = 
    (1ULL << SIGHUP) | (1ULL << SIGINT) | (1ULL << SIGQUIT) | (1ULL << SIGTERM) |
    (1ULL << SIGPIPE) | (1ULL << SIGALRM) | (1ULL << SIGCHLD) | (1ULL << SIGWINCH) |
    (1ULL << SIGUSR1) | (1ULL << SIGUSR2) | (1ULL << SIGCONT) | (1ULL << SIGTSTP) |
    (1ULL << SIGTTIN) | (1ULL << SIGTTOU) | (1ULL << SIGURG) | (1ULL << SIGXCPU) |
    (1ULL << SIGXFSZ) | (1ULL << SIGVTALRM) | (1ULL << SIGPROF) | (1ULL << SIGPOLL) |
    (1ULL << SIGPWR);

/* Signals that cannot be blocked */
static const uint64_t g_unblockable_signals = (1ULL << SIGKILL) | (1ULL << SIGSTOP);

/* ========================== Signal Delivery Core Functions ========================== */

/**
 * Initialize signal delivery subsystem
 */
int signal_delivery_init(void) {
    memset(&g_signal_manager, 0, sizeof(g_signal_manager));
    
    /* Initialize manager lock */
    kernel_spinlock_init(&g_signal_manager.manager_lock);
    
    /* Set default limits */
    g_signal_manager.max_concurrent_deliveries = 100;
    g_signal_manager.delivery_enabled = true;
    
    KLOG_INFO("Signal delivery subsystem initialized");
    return 0;
}

/**
 * Shutdown signal delivery subsystem
 */
void signal_delivery_shutdown(void) {
    spin_lock(&g_signal_manager.manager_lock);
    g_signal_manager.delivery_enabled = false;
    spin_unlock(&g_signal_manager.manager_lock);
    
    KLOG_INFO("Signal delivery subsystem shutdown");
}

/**
 * Initialize signal delivery state for a process
 */
int signal_delivery_init_process(process_t* proc) {
    if (!proc) {
        return -1;
    }
    
    /* Allocate signal delivery state */
    signal_delivery_state_t* state = (signal_delivery_state_t*)kmalloc(sizeof(signal_delivery_state_t));
    if (!state) {
        return -1;
    }
    
    memset(state, 0, sizeof(signal_delivery_state_t));
    
    /* Initialize state */
    state->max_pending = SIGNAL_MAX_PENDING;
    kernel_spinlock_init(&state->state_lock);
    
    /* Create signal queues for all signals */
    for (int i = 1; i < 64; i++) {
        bool is_rt = signal_is_realtime(i);
        uint32_t max_size = is_rt ? SIGNAL_QUEUE_MAX_SIZE : 64;
        
        state->queues[i] = signal_queue_create(i, max_size, is_rt);
        if (!state->queues[i]) {
            /* Cleanup on failure */
            for (int j = 1; j < i; j++) {
                if (state->queues[j]) {
                    signal_queue_destroy(state->queues[j]);
                }
            }
            kfree(state);
            return -1;
        }
    }
    
    /* Attach to process */
    proc->signal_delivery_state = state;
    
    KLOG_DEBUG("Signal delivery state initialized for process %u", proc->pid);
    return 0;
}

/**
 * Cleanup signal delivery state for a process
 */
void signal_delivery_cleanup_process(process_t* proc) {
    if (!proc || !proc->signal_delivery_state) {
        return;
    }
    
    signal_delivery_state_t* state = (signal_delivery_state_t*)proc->signal_delivery_state;
    
    /* Destroy all signal queues */
    for (int i = 1; i < 64; i++) {
        if (state->queues[i]) {
            signal_queue_destroy(state->queues[i]);
        }
    }
    
    /* Free state structure */
    kfree(state);
    proc->signal_delivery_state = NULL;
    
    KLOG_DEBUG("Signal delivery state cleaned up for process %u", proc->pid);
}

/**
 * Generate and queue a signal for delivery
 */
int signal_generate(process_t* target_proc, int signal, const siginfo_t* info, 
                   int source, uint32_t flags) {
    if (!target_proc || !signal_is_valid(signal)) {
        return -1;
    }
    
    signal_delivery_state_t* state = (signal_delivery_state_t*)target_proc->signal_delivery_state;
    if (!state) {
        return -1;
    }
    
    /* Check if signal is blocked (unless forced or unblockable) */
    if (!(flags & SIGNAL_DELIVER_FORCE) && !signal_can_deliver(target_proc, signal)) {
        if (signal_is_blocked(target_proc, signal)) {
            /* Signal is blocked, add to pending mask but don't queue for non-RT signals */
            if (!signal_is_realtime(signal)) {
                spin_lock(&state->state_lock);
                state->pending_mask |= (1ULL << signal);
                spin_unlock(&state->state_lock);
                g_signal_manager.global_stats.signals_blocked++;
                return 0;
            }
        }
    }
    
    /* Create signal info if not provided */
    siginfo_t signal_info;
    if (info) {
        signal_info = *info;
    } else {
        signal_init_info(&signal_info, signal, source);
    }
    
    /* Set timestamp */
    signal_info.si_timestamp = get_current_time_us();
    
    /* Get signal priority */
    uint8_t priority = signal_get_priority(signal);
    
    /* Enqueue signal */
    signal_queue_t* queue = state->queues[signal];
    if (!queue) {
        return -1;
    }
    
    /* Check for coalescing */
    if ((flags & SIGNAL_DELIVER_COALESCE) && signal_can_coalesce(signal)) {
        /* For coalescable signals, check if already pending */
        spin_lock(&state->state_lock);
        if (state->pending_mask & (1ULL << signal)) {
            /* Signal already pending, coalesce */
            spin_unlock(&state->state_lock);
            g_signal_manager.global_stats.signals_coalesced++;
            return 0;
        }
        spin_unlock(&state->state_lock);
    }
    
    /* Enqueue the signal */
    int result = signal_queue_enqueue(queue, signal, &signal_info, priority, flags);
    if (result == 0) {
        /* Update pending mask and counts */
        spin_lock(&state->state_lock);
        state->pending_mask |= (1ULL << signal);
        state->total_pending++;
        spin_unlock(&state->state_lock);
        
        /* Update global statistics */
        g_signal_manager.global_stats.signals_generated++;
        
        /* Trigger delivery if process is ready */
        if (!(flags & SIGNAL_DELIVER_QUEUE) && target_proc->state == PROCESS_RUNNING) {
            signal_deliver_pending(target_proc);
        }
    } else {
        g_signal_manager.global_stats.signals_discarded++;
    }
    
    return result;
}

/**
 * Deliver pending signals to a process
 */
int signal_deliver_pending(process_t* proc) {
    if (!proc || !proc->signal_delivery_state) {
        return -1;
    }
    
    signal_delivery_state_t* state = (signal_delivery_state_t*)proc->signal_delivery_state;
    int delivered_count = 0;
    
    /* Check if delivery is already active */
    spin_lock(&state->state_lock);
    if (state->delivery_active) {
        spin_unlock(&state->state_lock);
        return 0;
    }
    state->delivery_active = true;
    spin_unlock(&state->state_lock);
    
    /* Deliver signals in priority order */
    for (int priority = SIGNAL_PRIORITY_CRITICAL; priority <= SIGNAL_PRIORITY_RT_BASE + 31; priority++) {
        /* Check each signal at this priority level */
        for (int signal = 1; signal < 64; signal++) {
            if (signal_get_priority(signal) != priority) {
                continue;
            }
            
            /* Skip if signal not pending */
            if (!(state->pending_mask & (1ULL << signal))) {
                continue;
            }
            
            /* Skip if signal is blocked */
            if (signal_is_blocked(proc, signal)) {
                continue;
            }
            
            /* Deliver the signal */
            siginfo_t info;
            if (signal_queue_dequeue(state->queues[signal], &info) > 0) {
                if (signal_deliver_immediate(proc, signal, &info, 0) == 0) {
                    delivered_count++;
                    
                    /* Update pending mask if queue is now empty */
                    uint32_t queue_count;
                    if (signal_queue_get_stats(state->queues[signal], &queue_count, NULL) == 0 && queue_count == 0) {
                        spin_lock(&state->state_lock);
                        state->pending_mask &= ~(1ULL << signal);
                        state->total_pending--;
                        spin_unlock(&state->state_lock);
                    }
                } else {
                    /* Delivery failed, re-queue the signal */
                    signal_queue_enqueue(state->queues[signal], signal, &info, 
                                        signal_get_priority(signal), 0);
                }
            }
        }
    }
    
    /* Clear delivery active flag */
    spin_lock(&state->state_lock);
    state->delivery_active = false;
    state->last_delivery_time = get_current_time_us();
    state->delivery_count += delivered_count;
    spin_unlock(&state->state_lock);
    
    return delivered_count;
}

/**
 * Deliver a specific signal immediately
 */
int signal_deliver_immediate(process_t* proc, int signal, const siginfo_t* info, uint32_t flags) {
    if (!proc || !signal_is_valid(signal)) {
        return -1;
    }
    
    uint64_t start_time = get_current_time_us();
    
    /* Set current signal being handled */
    signal_delivery_state_t* state = (signal_delivery_state_t*)proc->signal_delivery_state;
    if (state) {
        state->current_signal = signal;
    }
    
    /* Call signal handler (this will be implemented in signal_handlers.c) */
    int result = signal_execute_handler(proc, signal, info);
    
    /* Update statistics */
    uint64_t delivery_time = get_current_time_us() - start_time;
    g_signal_manager.global_stats.signals_delivered++;
    g_signal_manager.global_stats.average_delivery_time = 
        (g_signal_manager.global_stats.average_delivery_time + delivery_time) / 2;
    if (delivery_time > g_signal_manager.global_stats.max_delivery_time) {
        g_signal_manager.global_stats.max_delivery_time = delivery_time;
    }
    
    if (result != 0) {
        g_signal_manager.global_stats.delivery_failures++;
    }
    
    /* Clear current signal */
    if (state) {
        state->current_signal = 0;
    }
    
    return result;
}

/* ========================== Signal Queue Management ========================== */

/**
 * Create a signal queue for a specific signal
 */
signal_queue_t* signal_queue_create(int signal, uint32_t max_size, bool is_realtime) {
    signal_queue_t* queue = (signal_queue_t*)kmalloc(sizeof(signal_queue_t));
    if (!queue) {
        return NULL;
    }
    
    memset(queue, 0, sizeof(signal_queue_t));
    queue->max_size = max_size;
    queue->priority = signal_get_priority(signal);
    queue->is_realtime = is_realtime;
    kernel_spinlock_init(&queue->lock);
    
    return queue;
}

/**
 * Destroy a signal queue
 */
void signal_queue_destroy(signal_queue_t* queue) {
    if (!queue) {
        return;
    }
    
    /* Clear all entries */
    signal_queue_clear(queue);
    
    /* Free the queue */
    kfree(queue);
}

/**
 * Enqueue a signal entry
 */
int signal_queue_enqueue(signal_queue_t* queue, int signal, const siginfo_t* info,
                        uint8_t priority, uint32_t flags) {
    if (!queue || !info) {
        return -1;
    }
    
    spin_lock(&queue->lock);
    
    /* Check queue capacity */
    if (queue->count >= queue->max_size) {
        spin_unlock(&queue->lock);
        g_signal_manager.global_stats.queue_overflows++;
        return -1;
    }
    
    /* Allocate new entry */
    signal_queue_entry_t* entry = signal_alloc_entry();
    if (!entry) {
        spin_unlock(&queue->lock);
        return -1;
    }
    
    /* Initialize entry */
    entry->signal = signal;
    entry->info = *info;
    entry->priority = priority;
    entry->flags = flags;
    entry->timestamp = get_current_time_us();
    entry->next = NULL;
    entry->prev = NULL;
    
    /* Insert in priority order */
    int result = signal_queue_insert_ordered(queue, entry);
    if (result == 0) {
        queue->count++;
    } else {
        signal_free_entry(entry);
    }
    
    spin_unlock(&queue->lock);
    return result;
}

/**
 * Dequeue the next signal entry
 */
int signal_queue_dequeue(signal_queue_t* queue, siginfo_t* info) {
    if (!queue || !info) {
        return 0;
    }
    
    spin_lock(&queue->lock);
    
    if (!queue->head) {
        spin_unlock(&queue->lock);
        return 0;
    }
    
    /* Get first entry */
    signal_queue_entry_t* entry = queue->head;
    int signal = entry->signal;
    *info = entry->info;
    
    /* Remove from queue */
    queue->head = entry->next;
    if (queue->head) {
        queue->head->prev = NULL;
    } else {
        queue->tail = NULL;
    }
    queue->count--;
    
    /* Free entry */
    signal_free_entry(entry);
    
    spin_unlock(&queue->lock);
    return signal;
}

/**
 * Clear all entries from a signal queue
 */
int signal_queue_clear(signal_queue_t* queue) {
    if (!queue) {
        return 0;
    }
    
    spin_lock(&queue->lock);
    
    int cleared = 0;
    signal_queue_entry_t* entry = queue->head;
    while (entry) {
        signal_queue_entry_t* next = entry->next;
        signal_free_entry(entry);
        entry = next;
        cleared++;
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    
    spin_unlock(&queue->lock);
    return cleared;
}

/* ========================== Signal Priority and Utility Functions ========================== */

/**
 * Get signal priority level
 */
uint8_t signal_get_priority(int signal) {
    if (signal < 1 || signal >= 64) {
        return SIGNAL_PRIORITY_NORMAL;
    }
    return g_signal_priorities[signal];
}

/**
 * Check if signal can be coalesced
 */
bool signal_can_coalesce(int signal) {
    if (signal < 1 || signal >= 64) {
        return false;
    }
    return (g_coalescable_signals & (1ULL << signal)) != 0;
}

/**
 * Check if signal is blocked by process mask
 */
bool signal_is_blocked(process_t* proc, int signal) {
    if (!proc || signal < 1 || signal >= 64) {
        return false;
    }
    
    /* Unblockable signals */
    if (g_unblockable_signals & (1ULL << signal)) {
        return false;
    }
    
    /* Check process signal mask */
    signal_delivery_state_t* state = (signal_delivery_state_t*)proc->signal_delivery_state;
    if (!state) {
        return false;
    }
    
    return (state->blocked_mask & (1ULL << signal)) != 0;
}

/**
 * Check if signal can be delivered now
 */
bool signal_can_deliver(process_t* proc, int signal) {
    if (!proc || signal < 1 || signal >= 64) {
        return false;
    }
    
    /* Process must be in a deliverable state */
    if (proc->state != PROCESS_RUNNING && proc->state != PROCESS_READY) {
        return false;
    }
    
    /* Check if signal is blocked */
    return !signal_is_blocked(proc, signal);
}

/**
 * Validate signal number
 */
bool signal_is_valid(int signal) {
    return (signal >= 1 && signal < 64);
}

/**
 * Check if signal is a real-time signal
 */
bool signal_is_realtime(int signal) {
    return (signal >= 32 && signal < 64);
}

/**
 * Get signal name string
 */
const char* signal_get_name(int signal) {
    if (signal < 1 || signal >= 64) {
        return "INVALID";
    }
    return g_signal_names[signal] ? g_signal_names[signal] : "UNKNOWN";
}

/* ========================== Signal Information Management ========================== */

/**
 * Initialize signal information structure
 */
void signal_init_info(siginfo_t* info, int signal, int source) {
    if (!info) {
        return;
    }
    
    memset(info, 0, sizeof(siginfo_t));
    info->si_signo = signal;
    info->si_code = source;
    info->si_timestamp = get_current_time_us();
}

/**
 * Set signal sender information
 */
void signal_set_sender_info(siginfo_t* info, pid_t sender_pid, uid_t sender_uid) {
    if (!info) {
        return;
    }
    
    info->si_pid = sender_pid;
    info->si_uid = sender_uid;
}

/**
 * Set signal address information
 */
void signal_set_addr_info(siginfo_t* info, void* addr, uint32_t trapno) {
    if (!info) {
        return;
    }
    
    info->si_addr = addr;
    info->si_trapno = trapno;
}

/**
 * Set signal child information
 */
void signal_set_child_info(siginfo_t* info, pid_t child_pid, int exit_status,
                          clock_t utime, clock_t stime) {
    if (!info) {
        return;
    }
    
    info->si_pid = child_pid;
    info->si_status = exit_status;
    info->si_utime = utime;
    info->si_stime = stime;
}

/* ========================== Memory Management ========================== */

/**
 * Allocate a signal queue entry
 */
signal_queue_entry_t* signal_alloc_entry(void) {
    signal_queue_entry_t* entry = (signal_queue_entry_t*)kmalloc(sizeof(signal_queue_entry_t));
    if (entry) {
        memset(entry, 0, sizeof(signal_queue_entry_t));
    }
    return entry;
}

/**
 * Free a signal queue entry
 */
void signal_free_entry(signal_queue_entry_t* entry) {
    if (entry) {
        kfree(entry);
    }
}

/**
 * Insert entry into queue maintaining priority order
 */
int signal_queue_insert_ordered(signal_queue_t* queue, signal_queue_entry_t* entry) {
    if (!queue || !entry) {
        return -1;
    }
    
    /* Empty queue */
    if (!queue->head) {
        queue->head = entry;
        queue->tail = entry;
        return 0;
    }
    
    /* Insert at head if higher priority */
    if (entry->priority < queue->head->priority) {
        entry->next = queue->head;
        queue->head->prev = entry;
        queue->head = entry;
        return 0;
    }
    
    /* Find insertion point */
    signal_queue_entry_t* current = queue->head;
    while (current->next && current->next->priority <= entry->priority) {
        current = current->next;
    }
    
    /* Insert after current */
    entry->next = current->next;
    entry->prev = current;
    if (current->next) {
        current->next->prev = entry;
    } else {
        queue->tail = entry;
    }
    current->next = entry;
    
    return 0;
}

/* ========================== Statistics and Monitoring ========================== */

/**
 * Get global signal delivery statistics
 */
int signal_get_global_stats(signal_delivery_stats_t* stats) {
    if (!stats) {
        return -1;
    }
    
    spin_lock(&g_signal_manager.manager_lock);
    *stats = g_signal_manager.global_stats;
    spin_unlock(&g_signal_manager.manager_lock);
    
    return 0;
}

/**
 * Reset signal statistics
 */
int signal_reset_stats(bool global) {
    if (global) {
        spin_lock(&g_signal_manager.manager_lock);
        memset(&g_signal_manager.global_stats, 0, sizeof(signal_delivery_stats_t));
        spin_unlock(&g_signal_manager.manager_lock);
    }
    
    return 0;
}

/* ========================== Placeholder Functions ========================== */

/**
 * Execute signal handler (placeholder - will be implemented in signal_handlers.c)
 */
int signal_execute_handler(process_t* proc, int signal, const siginfo_t* info) {
    /* This is a placeholder - will be implemented in signal_handlers.c */
    KLOG_DEBUG("Signal %d (%s) delivered to process %u", 
               signal, signal_get_name(signal), proc->pid);
    return 0;
}

/**
 * Get current time in microseconds (placeholder)
 */
uint64_t get_current_time_us(void) {
    /* This should be implemented by the timer system */
    static uint64_t fake_time = 0;
    return fake_time++; /* Placeholder implementation */
}

/* Memory management placeholders */
void* kmalloc(size_t size) {
    /* Placeholder - should use real kernel allocator */
    return malloc(size);
}

void kfree(void* ptr) {
    /* Placeholder - should use real kernel deallocator */
    free(ptr);
}

/* Standard library placeholders for printf */
int printf(const char* format, ...) {
    /* Placeholder printf implementation */
    return 0;
}

/* Standard library placeholders */
void* malloc(size_t size) {
    /* This is just a placeholder - kernel should not use malloc */
    return (void*)0x1000000; /* Fake address */
}

void free(void* ptr) {
    /* Placeholder free */
}

/* Placeholder spinlock functions */
void spin_lock(spinlock_t* lock) {
    /* Placeholder spinlock */
}

void spin_unlock(spinlock_t* lock) {
    /* Placeholder spinlock */
}

/* Placeholder process functions */
process_t* get_current_process(void) {
    /* Placeholder - should return current process */
    static process_t fake_proc = {0};
    return &fake_proc;
}

process_t* find_process_by_pid(pid_t pid) {
    /* Placeholder - should find process by PID */
    static process_t fake_proc = {0};
    fake_proc.pid = pid;
    return &fake_proc;
}

/* Placeholder string functions */
void* memset(void* ptr, int value, size_t size) {
    /* Placeholder memset */
    unsigned char* p = (unsigned char*)ptr;
    for (size_t i = 0; i < size; i++) {
        p[i] = (unsigned char)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t size) {
    /* Placeholder memcpy */
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    return dest;
}
