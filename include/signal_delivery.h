/* IKOS Signal Delivery Engine - Issue #19
 * Comprehensive signal delivery, queuing, and management system
 */

#ifndef SIGNAL_DELIVERY_H
#define SIGNAL_DELIVERY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "process.h"

/* Avoid conflicts with system headers by using kernel-specific types */

/* Define missing types if not available */
#ifndef _KERNEL_UID_T_DEFINED
#define _KERNEL_UID_T_DEFINED
typedef unsigned int kernel_uid_t;
#endif

#ifndef _KERNEL_CLOCK_T_DEFINED
#define _KERNEL_CLOCK_T_DEFINED
typedef long kernel_clock_t;
#endif

/* Define kernel spinlock type */
#ifndef _KERNEL_SPINLOCK_T_DEFINED
#define _KERNEL_SPINLOCK_T_DEFINED
typedef struct {
    volatile int locked;
} kernel_spinlock_t;
#endif

/* Define kernel union sigval */
#ifndef _KERNEL_SIGVAL_DEFINED
#define _KERNEL_SIGVAL_DEFINED
union kernel_sigval {
    int sival_int;
    void *sival_ptr;
};
#endif

/* Use kernel-specific sigset_t to avoid conflicts */
#ifndef _KERNEL_SIGSET_T_DEFINED
#define _KERNEL_SIGSET_T_DEFINED
#define KERNEL_NSIG 64
#define KERNEL_SIGSET_NWORDS ((KERNEL_NSIG + 63) / 64)

typedef struct {
    uint64_t sig[KERNEL_SIGSET_NWORDS];
} kernel_sigset_t;
#endif

/* ========================== Constants and Limits ========================== */

#define SIGNAL_QUEUE_MAX_SIZE       1024    /* Maximum signals per queue */
#define SIGNAL_MAX_RT_SIGNALS       32      /* Maximum RT signals (32-63) */
#define SIGNAL_MAX_PENDING          10000   /* Maximum pending signals per process */
#define SIGNAL_DELIVERY_TIMEOUT_MS  5000    /* Signal delivery timeout */

/* Signal delivery flags */
#define SIGNAL_DELIVER_ASYNC        0x01    /* Asynchronous delivery */
#define SIGNAL_DELIVER_SYNC         0x02    /* Synchronous delivery */
#define SIGNAL_DELIVER_FORCE        0x04    /* Force delivery (ignore mask) */
#define SIGNAL_DELIVER_COALESCE     0x08    /* Allow signal coalescing */
#define SIGNAL_DELIVER_QUEUE        0x10    /* Queue if process busy */

/* Signal generation sources */
#define SIGNAL_SOURCE_HARDWARE      1       /* Hardware exception */
#define SIGNAL_SOURCE_TIMER         2       /* Timer expiration */
#define SIGNAL_SOURCE_PROCESS       3       /* From another process */
#define SIGNAL_SOURCE_KERNEL        4       /* Kernel-generated */
#define SIGNAL_SOURCE_INTERRUPT     5       /* Interrupt handler */

/* Signal priority levels */
#define SIGNAL_PRIORITY_CRITICAL    0       /* SIGKILL, SIGSTOP */
#define SIGNAL_PRIORITY_HIGH        1       /* SIGSEGV, SIGBUS, SIGFPE */
#define SIGNAL_PRIORITY_NORMAL      2       /* Most signals */
#define SIGNAL_PRIORITY_LOW         3       /* SIGCHLD, SIGWINCH */
#define SIGNAL_PRIORITY_RT_BASE     10      /* Base for RT signals */

/* ========================== Data Structures ========================== */

/**
 * Signal information structure (POSIX siginfo_t)
 */
typedef struct siginfo {
    int si_signo;           /* Signal number */
    int si_errno;           /* Error number */
    int si_code;            /* Signal code */
    pid_t si_pid;           /* Sending process ID */
    kernel_uid_t si_uid;    /* Sending user ID */
    int si_status;          /* Exit status or signal */
    kernel_clock_t si_utime; /* User time consumed */
    kernel_clock_t si_stime; /* System time consumed */
    union kernel_sigval si_value;  /* Signal value */
    void* si_addr;          /* Memory address (SIGSEGV, SIGBUS) */
    int si_band;            /* SIGPOLL band event */
    int si_fd;              /* File descriptor (SIGPOLL) */
    int si_overrun;         /* Timer overrun count */
    uint32_t si_trapno;     /* Trap number that caused signal */
    uint64_t si_timestamp;  /* Signal generation timestamp */
} siginfo_t;

/**
 * Signal value union (POSIX union sigval)
 */
union sigval {
    int sival_int;          /* Integer value */
    void* sival_ptr;        /* Pointer value */
};

/**
 * Signal queue entry
 */
typedef struct signal_queue_entry {
    int signal;                             /* Signal number */
    siginfo_t info;                         /* Signal information */
    uint8_t priority;                       /* Signal priority */
    uint32_t flags;                         /* Delivery flags */
    uint64_t timestamp;                     /* Enqueue timestamp */
    struct signal_queue_entry* next;        /* Next in queue */
    struct signal_queue_entry* prev;        /* Previous in queue */
} signal_queue_entry_t;

/**
 * Per-signal queue with priority ordering
 */
typedef struct signal_queue {
    signal_queue_entry_t* head;             /* First entry */
    signal_queue_entry_t* tail;             /* Last entry */
    uint32_t count;                         /* Number of entries */
    uint32_t max_size;                      /* Maximum queue size */
    uint8_t priority;                       /* Queue priority */
    bool is_realtime;                       /* RT signal queue */
    kernel_spinlock_t lock;                        /* Queue protection */
} signal_queue_t;

/**
 * Process signal delivery state
 */
typedef struct signal_delivery_state {
    signal_queue_t* queues[64];             /* Per-signal queues (1-63) */
    uint64_t pending_mask;                  /* Bitmask of pending signals */
    uint64_t blocked_mask;                  /* Bitmask of blocked signals */
    uint32_t total_pending;                 /* Total pending signals */
    uint32_t max_pending;                   /* Maximum allowed pending */
    uint64_t last_delivery_time;            /* Last signal delivery */
    uint32_t delivery_count;                /* Signals delivered */
    kernel_spinlock_t state_lock;                  /* State protection */
    bool delivery_active;                   /* Delivery in progress */
    int current_signal;                     /* Currently handling signal */
} signal_delivery_state_t;

/**
 * Signal delivery statistics
 */
typedef struct signal_delivery_stats {
    uint64_t signals_generated;             /* Total signals generated */
    uint64_t signals_delivered;             /* Total signals delivered */
    uint64_t signals_blocked;               /* Signals blocked by mask */
    uint64_t signals_discarded;             /* Signals discarded (queue full) */
    uint64_t signals_coalesced;             /* Signals coalesced */
    uint64_t delivery_failures;             /* Delivery failures */
    uint64_t average_delivery_time;         /* Average delivery time (μs) */
    uint64_t max_delivery_time;             /* Maximum delivery time (μs) */
    uint64_t queue_overflows;               /* Queue overflow events */
    uint64_t priority_inversions;           /* Priority inversion events */
} signal_delivery_stats_t;

/**
 * Global signal delivery manager
 */
typedef struct signal_delivery_manager {
    signal_delivery_stats_t global_stats;   /* Global statistics */
    uint32_t active_deliveries;             /* Active delivery count */
    uint64_t next_sequence_number;          /* Signal sequence numbers */
    kernel_spinlock_t manager_lock;                /* Manager protection */
    bool delivery_enabled;                  /* Global delivery enable */
    uint32_t max_concurrent_deliveries;     /* Delivery concurrency limit */
} signal_delivery_manager_t;

/* ========================== Core Signal Delivery Functions ========================== */

/**
 * Initialize signal delivery subsystem
 * @return 0 on success, -1 on error
 */
int signal_delivery_init(void);

/**
 * Shutdown signal delivery subsystem
 */
void signal_delivery_shutdown(void);

/**
 * Initialize signal delivery state for a process
 * @param proc Process to initialize
 * @return 0 on success, -1 on error
 */
int signal_delivery_init_process(process_t* proc);

/**
 * Cleanup signal delivery state for a process
 * @param proc Process to cleanup
 */
void signal_delivery_cleanup_process(process_t* proc);

/**
 * Generate and queue a signal for delivery
 * @param target_proc Target process
 * @param signal Signal number (1-63)
 * @param info Signal information (NULL for simple signals)
 * @param source Signal generation source
 * @param flags Delivery flags
 * @return 0 on success, -1 on error
 */
int signal_generate(process_t* target_proc, int signal, const siginfo_t* info, 
                   int source, uint32_t flags);

/**
 * Deliver pending signals to a process
 * @param proc Target process
 * @return Number of signals delivered, -1 on error
 */
int signal_deliver_pending(process_t* proc);

/**
 * Deliver a specific signal immediately
 * @param proc Target process
 * @param signal Signal number
 * @param info Signal information
 * @param flags Delivery flags
 * @return 0 on success, -1 on error
 */
int signal_deliver_immediate(process_t* proc, int signal, const siginfo_t* info, uint32_t flags);

/**
 * Check if a signal is pending for a process
 * @param proc Target process
 * @param signal Signal number (0 for any signal)
 * @return true if signal is pending, false otherwise
 */
bool signal_is_pending(process_t* proc, int signal);

/**
 * Get next pending signal for delivery
 * @param proc Target process
 * @param info Buffer for signal information
 * @return Signal number, 0 if no pending signals
 */
int signal_get_next_pending(process_t* proc, siginfo_t* info);

/* ========================== Signal Queue Management ========================== */

/**
 * Create a signal queue for a specific signal
 * @param signal Signal number
 * @param max_size Maximum queue size
 * @param is_realtime Whether this is an RT signal queue
 * @return Pointer to queue, NULL on error
 */
signal_queue_t* signal_queue_create(int signal, uint32_t max_size, bool is_realtime);

/**
 * Destroy a signal queue
 * @param queue Queue to destroy
 */
void signal_queue_destroy(signal_queue_t* queue);

/**
 * Enqueue a signal entry
 * @param queue Target queue
 * @param signal Signal number
 * @param info Signal information
 * @param priority Signal priority
 * @param flags Delivery flags
 * @return 0 on success, -1 on error
 */
int signal_queue_enqueue(signal_queue_t* queue, int signal, const siginfo_t* info,
                        uint8_t priority, uint32_t flags);

/**
 * Dequeue the next signal entry
 * @param queue Source queue
 * @param info Buffer for signal information
 * @return Signal number, 0 if queue empty
 */
int signal_queue_dequeue(signal_queue_t* queue, siginfo_t* info);

/**
 * Peek at the next signal without removing it
 * @param queue Source queue
 * @param info Buffer for signal information
 * @return Signal number, 0 if queue empty
 */
int signal_queue_peek(signal_queue_t* queue, siginfo_t* info);

/**
 * Clear all entries from a signal queue
 * @param queue Queue to clear
 * @return Number of entries removed
 */
int signal_queue_clear(signal_queue_t* queue);

/**
 * Get queue statistics
 * @param queue Target queue
 * @param count Current entry count
 * @param max_size Maximum queue size
 * @return 0 on success, -1 on error
 */
int signal_queue_get_stats(signal_queue_t* queue, uint32_t* count, uint32_t* max_size);

/* ========================== Signal Priority and Ordering ========================== */

/**
 * Get signal priority level
 * @param signal Signal number
 * @return Priority level (0 = highest)
 */
uint8_t signal_get_priority(int signal);

/**
 * Compare signal priorities for ordering
 * @param sig1 First signal
 * @param sig2 Second signal
 * @return <0 if sig1 has higher priority, >0 if sig2 higher, 0 if equal
 */
int signal_compare_priority(int sig1, int sig2);

/**
 * Check if signal can be coalesced
 * @param signal Signal number
 * @return true if signal can be coalesced, false otherwise
 */
bool signal_can_coalesce(int signal);

/**
 * Coalesce identical signals in queue
 * @param queue Target queue
 * @param signal Signal number
 * @return Number of signals coalesced
 */
int signal_coalesce_in_queue(signal_queue_t* queue, int signal);

/* ========================== Signal Masking and Blocking ========================== */

/**
 * Check if signal is blocked by process mask
 * @param proc Target process
 * @param signal Signal number
 * @return true if signal is blocked, false otherwise
 */
bool signal_is_blocked(process_t* proc, int signal);

/**
 * Check if signal can be delivered now
 * @param proc Target process
 * @param signal Signal number
 * @return true if signal can be delivered, false otherwise
 */
bool signal_can_deliver(process_t* proc, int signal);

/**
 * Update process signal mask
 * @param proc Target process
 * @param new_mask New signal mask
 * @param old_mask Buffer for old mask (can be NULL)
 * @return 0 on success, -1 on error
 */
int signal_update_mask(process_t* proc, uint64_t new_mask, uint64_t* old_mask);

/* ========================== Signal Information Management ========================== */

/**
 * Initialize signal information structure
 * @param info Signal info structure to initialize
 * @param signal Signal number
 * @param source Signal generation source
 */
void signal_init_info(siginfo_t* info, int signal, int source);

/**
 * Set signal sender information
 * @param info Signal info structure
 * @param sender_pid Sending process ID
 * @param sender_uid Sending user ID
 */
void signal_set_sender_info(siginfo_t* info, pid_t sender_pid, kernel_uid_t sender_uid);

/**
 * Set signal address information (for SIGSEGV, SIGBUS)
 * @param info Signal info structure
 * @param addr Memory address that caused signal
 * @param trapno Trap number
 */
void signal_set_addr_info(siginfo_t* info, void* addr, uint32_t trapno);

/**
 * Set signal timer information (for SIGALRM, etc.)
 * @param info Signal info structure
 * @param overrun Timer overrun count
 */
void signal_set_timer_info(siginfo_t* info, int overrun);

/**
 * Set signal child information (for SIGCHLD)
 * @param info Signal info structure
 * @param child_pid Child process ID
 * @param exit_status Child exit status
 * @param utime User CPU time
 * @param stime System CPU time
 */
void signal_set_child_info(siginfo_t* info, pid_t child_pid, int exit_status,
                          kernel_clock_t utime, kernel_clock_t stime);

/* ========================== Signal Statistics and Monitoring ========================== */

/**
 * Get global signal delivery statistics
 * @param stats Buffer for statistics
 * @return 0 on success, -1 on error
 */
int signal_get_global_stats(signal_delivery_stats_t* stats);

/**
 * Get process signal statistics
 * @param proc Target process
 * @param stats Buffer for statistics
 * @return 0 on success, -1 on error
 */
int signal_get_process_stats(process_t* proc, signal_delivery_stats_t* stats);

/**
 * Reset signal statistics
 * @param global Whether to reset global statistics
 * @return 0 on success, -1 on error
 */
int signal_reset_stats(bool global);

/**
 * Enable or disable signal delivery tracing
 * @param enabled Whether to enable tracing
 * @param trace_mask Bitmask of signals to trace (0 for all)
 */
void signal_set_tracing(bool enabled, uint64_t trace_mask);

/* ========================== Internal Helper Functions ========================== */

/**
 * Allocate a signal queue entry
 * @return Pointer to entry, NULL on error
 */
signal_queue_entry_t* signal_alloc_entry(void);

/**
 * Free a signal queue entry
 * @param entry Entry to free
 */
void signal_free_entry(signal_queue_entry_t* entry);

/**
 * Insert entry into queue maintaining priority order
 * @param queue Target queue
 * @param entry Entry to insert
 * @return 0 on success, -1 on error
 */
int signal_queue_insert_ordered(signal_queue_t* queue, signal_queue_entry_t* entry);

/**
 * Remove specific entry from queue
 * @param queue Source queue
 * @param entry Entry to remove
 * @return 0 on success, -1 on error
 */
int signal_queue_remove_entry(signal_queue_t* queue, signal_queue_entry_t* entry);

/**
 * Validate signal number
 * @param signal Signal number to validate
 * @return true if valid, false otherwise
 */
bool signal_is_valid(int signal);

/**
 * Check if signal is a real-time signal
 * @param signal Signal number
 * @return true if RT signal, false otherwise
 */
bool signal_is_realtime(int signal);

/**
 * Get signal name string
 * @param signal Signal number
 * @return Signal name string
 */
const char* signal_get_name(int signal);

#endif /* SIGNAL_DELIVERY_H */
