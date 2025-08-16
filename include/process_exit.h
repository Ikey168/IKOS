/* IKOS Process Termination and Cleanup System Header - Issue #18
 * Comprehensive process exit, cleanup, and resource management
 */

#ifndef PROCESS_EXIT_H
#define PROCESS_EXIT_H

#include <stdint.h>
#include <stdbool.h>
#include "process.h"

/* ========================== Constants and Limits ========================== */

#define MAX_OPEN_FILES          64      /* Maximum open files per process */
#define MAX_SIGNALS             32      /* Maximum number of signals */
#define MAX_WAIT_QUEUE_SIZE     256     /* Maximum processes waiting for children */

/* Wait options for waitpid */
#define WNOHANG                 1       /* Return immediately if no child available */
#define WUNTRACED               2       /* Report stopped children */
#define WCONTINUED              4       /* Report continued children */

/* Exit status macros */
#define WEXITSTATUS(status)     (((status) & 0xff00) >> 8)
#define WTERMSIG(status)        ((status) & 0x7f)
#define WSTOPSIG(status)        (((status) & 0xff00) >> 8)
#define WIFEXITED(status)       (WTERMSIG(status) == 0)
#define WIFSIGNALED(status)     (((signed char) (((status) & 0x7f) + 1) >> 1) > 0)
#define WIFSTOPPED(status)      (((status) & 0xff) == 0x7f)
#define WIFCONTINUED(status)    ((status) == 0xffff)

/* Signal numbers */
#define SIGHUP                  1       /* Hangup */
#define SIGINT                  2       /* Interrupt */
#define SIGQUIT                 3       /* Quit */
#define SIGILL                  4       /* Illegal instruction */
#define SIGTRAP                 5       /* Trace trap */
#define SIGABRT                 6       /* Abort */
#define SIGBUS                  7       /* Bus error */
#define SIGFPE                  8       /* Floating point exception */
#define SIGKILL                 9       /* Kill (unblockable) */
#define SIGUSR1                 10      /* User signal 1 */
#define SIGSEGV                 11      /* Segmentation violation */
#define SIGUSR2                 12      /* User signal 2 */
#define SIGPIPE                 13      /* Broken pipe */
#define SIGALRM                 14      /* Alarm clock */
#define SIGTERM                 15      /* Termination */
#define SIGSTKFLT               16      /* Stack fault */
#define SIGCHLD                 17      /* Child status changed */
#define SIGCONT                 18      /* Continue */
#define SIGSTOP                 19      /* Stop (unblockable) */
#define SIGTSTP                 20      /* Keyboard stop */
#define SIGTTIN                 21      /* Background read from tty */
#define SIGTTOU                 22      /* Background write to tty */

/* Error codes */
#define ESRCH                   3       /* No such process */
#define EFAULT                  14      /* Bad address */
#define ECHILD                  10      /* No child processes */
#define ENOSYS                  38      /* Function not implemented */

/* ========================== Data Structures ========================== */

/**
 * File descriptor structure
 */
typedef struct {
    int fd;                 /* File descriptor number (-1 if unused) */
    int flags;              /* File descriptor flags */
    off_t offset;           /* Current file offset */
} process_fd_t;

/**
 * Process wait queue entry
 */
typedef struct process_wait_entry {
    process_t* parent;                      /* Parent process waiting */
    pid_t child_pid;                        /* Specific child PID (0 for any) */
    int* status_ptr;                        /* Where to store exit status */
    struct process_wait_entry* next;        /* Next in wait queue */
} process_wait_entry_t;

/**
 * Process wait queue
 */
typedef struct {
    process_wait_entry_t* head;             /* First waiting process */
    process_wait_entry_t* tail;             /* Last waiting process */
    int count;                              /* Number of waiting processes */
} process_wait_queue_t;

/**
 * Process exit statistics
 */
typedef struct {
    uint64_t total_exits;           /* Total processes that have exited */
    uint64_t normal_exits;          /* Processes that exited normally */
    uint64_t killed_processes;      /* Processes terminated by signals */
    uint64_t zombie_count;          /* Current zombie process count */
    uint64_t orphan_count;          /* Current orphaned process count */
    uint64_t resources_cleaned;     /* Total resources cleaned up */
} process_exit_stats_t;

/**
 * Signal handler function pointer
 */
typedef void (*signal_handler_t)(int signal);

/* ========================== Core Exit Functions ========================== */

/**
 * Complete process exit with comprehensive cleanup
 * @param proc Process to exit
 * @param exit_code Exit status code
 */
void process_exit(process_t* proc, int exit_code);

/**
 * Force terminate a process (SIGKILL equivalent)
 * @param proc Process to kill
 * @param signal Signal number that caused termination
 */
void process_kill(process_t* proc, int signal);

/**
 * Emergency process termination (unrecoverable errors)
 * @param proc Process to force kill
 */
void process_force_kill(process_t* proc);

/* ========================== Resource Cleanup Functions ========================== */

/**
 * Clean up all file descriptors for a process
 * @param proc Process to clean up
 * @return Number of file descriptors closed
 */
int process_cleanup_files(process_t* proc);

/**
 * Clean up IPC resources for a process
 * @param proc Process to clean up
 * @return Number of IPC resources cleaned
 */
int process_cleanup_ipc(process_t* proc);

/**
 * Clean up memory for a process
 * @param proc Process to clean up
 * @return Number of memory pages freed
 */
int process_cleanup_memory(process_t* proc);

/**
 * Clean up timers for a process
 * @param proc Process to clean up
 * @return Number of timers cancelled
 */
int process_cleanup_timers(process_t* proc);

/**
 * Clean up signal state for a process
 * @param proc Process to clean up
 * @return 0 on success
 */
int process_cleanup_signals(process_t* proc);

/* ========================== Parent-Child Management ========================== */

/**
 * Reparent all children of a dying process to init
 * @param parent Parent process that is dying
 */
void process_reparent_children(process_t* parent);

/**
 * Notify parent process of child exit
 * @param child Child process that exited
 * @param exit_status Exit status of child
 */
void process_notify_parent(process_t* child, int exit_status);

/**
 * Reap a zombie process (final cleanup)
 * @param zombie Zombie process to reap
 * @return 0 on success, -1 on error
 */
int process_reap_zombie(process_t* zombie);

/* ========================== Wait System Call Support ========================== */

/**
 * Wait for any child process to exit
 * @param parent Parent process
 * @param status Pointer to store exit status
 * @param options Wait options (WNOHANG, etc.)
 * @return PID of exited child, 0 if WNOHANG and no child ready, -1 on error
 */
pid_t process_wait_any(process_t* parent, int* status, int options);

/**
 * Wait for specific child process to exit
 * @param parent Parent process
 * @param pid PID of child to wait for
 * @param status Pointer to store exit status
 * @param options Wait options (WNOHANG, etc.)
 * @return PID if child exited, 0 if WNOHANG and child not ready, -1 on error
 */
pid_t process_wait_pid(process_t* parent, pid_t pid, int* status, int options);

/* ========================== Helper Functions ========================== */

/**
 * Find zombie child of a parent process
 * @param parent Parent process
 * @return Pointer to zombie child, or NULL if none
 */
process_t* process_find_zombie_child(process_t* parent);

/**
 * Find specific child by PID
 * @param parent Parent process
 * @param pid Child PID to find
 * @return Pointer to child process, or NULL if not found
 */
process_t* process_find_child_by_pid(process_t* parent, pid_t pid);

/**
 * Add process to wait queue
 * @param parent Parent process that is waiting
 * @param child_pid Child PID to wait for (0 for any)
 * @param status_ptr Where to store exit status
 * @return 0 on success, -1 on error
 */
int process_add_to_wait_queue(process_t* parent, pid_t child_pid, int* status_ptr);

/**
 * Remove process from wait queue
 * @param parent Parent process to remove
 */
void process_remove_from_wait_queue(process_t* parent);

/**
 * Wake up waiting parent when child exits
 * @param parent Parent process
 * @param child Child process that exited
 */
void process_wake_waiting_parent(process_t* parent, process_t* child);

/**
 * Add child to parent's zombie list
 * @param parent Parent process
 * @param child Zombie child process
 */
void process_add_to_zombie_list(process_t* parent, process_t* child);

/**
 * Remove child from parent's zombie list
 * @param parent Parent process
 * @param child Child to remove
 */
void process_remove_from_zombie_list(process_t* parent, process_t* child);

/**
 * Block process waiting for child to exit
 * @param parent Parent process
 * @param child_pid Child PID to wait for (0 for any)
 * @param status_ptr Where to store exit status
 * @return PID of exited child
 */
pid_t process_block_waiting_for_child(process_t* parent, pid_t child_pid, int* status_ptr);

/* ========================== System Call Implementations ========================== */

/**
 * System call: exit
 * @param status Exit status code
 */
void sys_exit(int status) __attribute__((noreturn));

/**
 * System call: waitpid
 * @param pid Process ID to wait for (-1 for any child)
 * @param status Pointer to store exit status
 * @param options Wait options
 * @return PID of exited child, 0 if WNOHANG, -1 on error
 */
long sys_waitpid(pid_t pid, int* status, int options);

/**
 * System call: wait
 * @param status Pointer to store exit status
 * @return PID of exited child, -1 on error
 */
long sys_wait(int* status);

/* ========================== Signal Functions ========================== */

/**
 * Queue a signal to a process
 * @param proc Target process
 * @param signal Signal number
 * @param sender_pid PID of sending process
 * @param exit_status Exit status (for SIGCHLD)
 * @return 0 on success, -1 on error
 */
int signal_queue_to_process(process_t* proc, int signal, pid_t sender_pid, int exit_status);

/**
 * Remove process from all signal delivery queues
 * @param pid Process ID to remove
 */
void signal_remove_from_delivery_queues(pid_t pid);

/* ========================== IPC Cleanup Functions ========================== */

/**
 * Clean up message queues owned by a process
 * @param pid Process ID
 * @return Number of queues cleaned
 */
int ipc_cleanup_process_queues(pid_t pid);

/**
 * Clean up shared memory segments for a process
 * @param pid Process ID
 * @return Number of segments cleaned
 */
int shm_cleanup_process_segments(pid_t pid);

/**
 * Clean up semaphores for a process
 * @param pid Process ID
 * @return Number of semaphores cleaned
 */
int sem_cleanup_process_semaphores(pid_t pid);

/**
 * Remove process from all IPC wait queues
 * @param pid Process ID
 */
void ipc_remove_from_all_queues(pid_t pid);

/* ========================== Timer Functions ========================== */

/**
 * Cancel all timers for a process
 * @param pid Process ID
 * @return Number of timers cancelled
 */
int timer_cancel_all_for_process(pid_t pid);

/**
 * Cancel alarm for a process
 * @param pid Process ID
 */
void alarm_cancel(pid_t pid);

/* ========================== Memory Validation ========================== */

/**
 * Validate user space pointer
 * @param ptr Pointer to validate
 * @param size Size of data
 * @return true if valid, false otherwise
 */
bool validate_user_pointer(void* ptr, size_t size);

/* ========================== System Utility Functions ========================== */

/**
 * Get process exit statistics
 * @param stats Pointer to store statistics
 */
void process_get_exit_stats(process_exit_stats_t* stats);

/**
 * Initialize process exit system
 * @return 0 on success, -1 on error
 */
int process_exit_init(void);

/**
 * Cleanup zombie processes periodically
 */
void process_cleanup_zombies(void);

/**
 * Get current system time in milliseconds
 * @return Current time
 */
uint64_t get_system_time(void);

/**
 * Get current process
 * @return Pointer to current process
 */
process_t* process_get_current(void);

/**
 * Find process by PID
 * @param pid Process ID
 * @return Pointer to process, or NULL if not found
 */
process_t* process_find_by_pid(pid_t pid);

/**
 * Get process by index
 * @param index Process table index
 * @return Pointer to process, or NULL if invalid
 */
process_t* process_get_by_index(int index);

/**
 * Free a process slot
 * @param proc Process to free
 */
void process_free_slot(process_t* proc);

/**
 * Remove process from ready queue
 * @param proc Process to remove
 */
void process_remove_from_ready_queue(process_t* proc);

/**
 * Schedule next process
 */
void schedule_next_process(void);

/**
 * Allocate kernel memory
 * @param size Size to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void* kalloc(size_t size);

/**
 * Free kernel memory
 * @param ptr Pointer to free
 */
void kfree(void* ptr);

/* ========================== Log Categories ========================== */

/* Note: LOG_CAT_PROCESS is defined in kernel_log.h */

/**
 * Kernel logging function
 */
void klog(int level, int category, const char* format, ...);

#define KLOG_ERROR(cat, fmt, ...)   klog(0, cat, fmt, ##__VA_ARGS__)
#define KLOG_WARN(cat, fmt, ...)    klog(1, cat, fmt, ##__VA_ARGS__)
#define KLOG_INFO(cat, fmt, ...)    klog(2, cat, fmt, ##__VA_ARGS__)
#define KLOG_DEBUG(cat, fmt, ...)   klog(3, cat, fmt, ##__VA_ARGS__)

#endif /* PROCESS_EXIT_H */
