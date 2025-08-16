/* IKOS Signal System Calls Implementation - Issue #19
 * POSIX-compatible signal system call interface
 */

#include "signal_syscalls.h"
#include "signal_delivery.h"
#include "signal_mask.h"
#include "process.h"
#include "memory.h"
#include "string.h"
#include "assert.h"

/* Define missing constants */
#ifndef ENOMEM
#define ENOMEM 12
#endif

#ifndef EINVAL
#define EINVAL 22
#endif

#ifndef EPERM
#define EPERM 1
#endif

#ifndef ESRCH
#define ESRCH 3
#endif

#ifndef EFAULT
#define EFAULT 14
#endif

/* Define KLOG macros if not available */
#ifndef KLOG_DEBUG
#define KLOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#endif

#ifndef KLOG_ERROR
#define KLOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif

#ifndef KLOG_INFO
#define KLOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#endif

/* Define missing signal constants if not already defined */
#ifndef SIGKILL
#define SIGKILL 9
#endif

#ifndef SIGSTOP
#define SIGSTOP 19
#endif

#ifndef SIGCHLD
#define SIGCHLD 17
#endif

#ifndef SIGCONT
#define SIGCONT 18
#endif

/* Define spinlock type if not available */
typedef struct {
    volatile int locked;
} spinlock_t;

/* ========================== Global State ========================== */

/* Signal system call statistics */
static signal_syscall_stats_t g_signal_syscall_stats;
static spinlock_t g_stats_lock;

/* Process wait queue for signal suspension */
typedef struct signal_wait_entry {
    process_t* proc;
    sigset_t wait_set;
    siginfo_t* info_buffer;
    uint64_t timeout_time;
    struct signal_wait_entry* next;
} signal_wait_entry_t;

static signal_wait_entry_t* g_signal_wait_queue = NULL;
static spinlock_t g_wait_queue_lock;

/* ========================== System Call Implementations ========================== */

/**
 * System call: signal - Install signal handler (simple interface)
 */
long sys_signal(int sig, signal_handler_t handler) {
    process_t* current = get_current_process();
    if (!current) {
        return (long)SIG_ERR;
    }
    
    spin_lock(&g_stats_lock);
    g_signal_syscall_stats.signal_calls++;
    spin_unlock(&g_stats_lock);
    
    /* Validate signal number */
    if (!signal_mask_is_valid_signal(sig) || sig == SIGKILL || sig == SIGSTOP) {
        spin_lock(&g_stats_lock);
        g_signal_syscall_stats.invalid_signals++;
        spin_unlock(&g_stats_lock);
        return (long)SIG_ERR;
    }
    
    /* Set simple signal handler */
    signal_handler_t old_handler = signal_handler_set(current, sig, handler);
    
    return (long)old_handler;
}

/**
 * System call: sigaction - Install signal action (advanced interface)
 */
long sys_sigaction(int sig, const struct sigaction* act, struct sigaction* oldact) {
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    spin_lock(&g_stats_lock);
    g_signal_syscall_stats.sigaction_calls++;
    spin_unlock(&g_stats_lock);
    
    /* Validate signal number */
    if (!signal_mask_is_valid_signal(sig)) {
        spin_lock(&g_stats_lock);
        g_signal_syscall_stats.invalid_signals++;
        spin_unlock(&g_stats_lock);
        return -1;
    }
    
    /* Copy action from user space if provided */
    struct sigaction kernel_act;
    if (act) {
        if (signal_syscall_copy_sigaction_from_user(&kernel_act, act) != 0) {
            spin_lock(&g_stats_lock);
            g_signal_syscall_stats.user_copy_errors++;
            spin_unlock(&g_stats_lock);
            return -1;
        }
    }
    
    /* Get old action if requested */
    struct sigaction old_action;
    if (oldact) {
        if (signal_action_get(current, sig, &old_action) != 0) {
            return -1;
        }
    }
    
    /* Set new action */
    if (act) {
        if (signal_action_set(current, sig, &kernel_act, NULL) != 0) {
            return -1;
        }
    }
    
    /* Copy old action to user space */
    if (oldact) {
        if (signal_syscall_copy_sigaction_to_user(oldact, &old_action) != 0) {
            spin_lock(&g_stats_lock);
            g_signal_syscall_stats.user_copy_errors++;
            spin_unlock(&g_stats_lock);
            return -1;
        }
    }
    
    return 0;
}

/**
 * System call: kill - Send signal to process or process group
 */
long sys_kill(pid_t pid, int sig) {
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    spin_lock(&g_stats_lock);
    g_signal_syscall_stats.kill_calls++;
    spin_unlock(&g_stats_lock);
    
    /* Validate signal number (0 is valid for permission check) */
    if (sig != 0 && !signal_mask_is_valid_signal(sig)) {
        spin_lock(&g_stats_lock);
        g_signal_syscall_stats.invalid_signals++;
        spin_unlock(&g_stats_lock);
        return -1;
    }
    
    /* Handle special PID values */
    if (pid == 0) {
        /* Send to process group of current process */
        return signal_syscall_kill_process_group(current->pgid, sig, current);
    } else if (pid == -1) {
        /* Send to all processes (except init) */
        return signal_syscall_kill_all_processes(sig, current);
    } else if (pid < -1) {
        /* Send to process group -pid */
        return signal_syscall_kill_process_group(-pid, sig, current);
    }
    
    /* Send to specific process */
    process_t* target = signal_syscall_find_process(pid);
    if (!target) {
        return -1;
    }
    
    /* Check permission */
    if (!signal_syscall_check_permission(current, target, sig)) {
        spin_lock(&g_stats_lock);
        g_signal_syscall_stats.permission_denied++;
        spin_unlock(&g_stats_lock);
        return -1;
    }
    
    /* If sig is 0, just check permission */
    if (sig == 0) {
        return 0;
    }
    
    /* Generate signal */
    siginfo_t info;
    signal_init_info(&info, sig, SIGNAL_SOURCE_PROCESS);
    signal_set_sender_info(&info, current->pid, current->uid);
    
    if (signal_generate(target, sig, &info, SIGNAL_SOURCE_PROCESS, 0) != 0) {
        return -1;
    }
    
    return 0;
}

/**
 * System call: sigprocmask - Change signal mask
 */
long sys_sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    spin_lock(&g_stats_lock);
    g_signal_syscall_stats.sigprocmask_calls++;
    spin_unlock(&g_stats_lock);
    
    /* Copy signal set from user space if provided */
    sigset_t kernel_set;
    if (set) {
        if (signal_syscall_copy_sigset_from_user(&kernel_set, set) != 0) {
            spin_lock(&g_stats_lock);
            g_signal_syscall_stats.user_copy_errors++;
            spin_unlock(&g_stats_lock);
            return -1;
        }
    }
    
    /* Get old mask if requested */
    sigset_t old_mask;
    if (oldset) {
        if (signal_mask_get(current, &old_mask) != 0) {
            return -1;
        }
    }
    
    /* Change mask */
    if (signal_mask_change(current, how, set ? &kernel_set : NULL, NULL) != 0) {
        return -1;
    }
    
    /* Copy old mask to user space */
    if (oldset) {
        if (signal_syscall_copy_sigset_to_user(oldset, &old_mask) != 0) {
            spin_lock(&g_stats_lock);
            g_signal_syscall_stats.user_copy_errors++;
            spin_unlock(&g_stats_lock);
            return -1;
        }
    }
    
    return 0;
}

/**
 * System call: sigpending - Get pending signals
 */
long sys_sigpending(sigset_t* set) {
    process_t* current = get_current_process();
    if (!current || !set) {
        return -1;
    }
    
    spin_lock(&g_stats_lock);
    g_signal_syscall_stats.sigpending_calls++;
    spin_unlock(&g_stats_lock);
    
    /* Get pending signals */
    sigset_t pending;
    if (signal_mask_get_pending(current, &pending) != 0) {
        return -1;
    }
    
    /* Copy to user space */
    if (signal_syscall_copy_sigset_to_user(set, &pending) != 0) {
        spin_lock(&g_stats_lock);
        g_signal_syscall_stats.user_copy_errors++;
        spin_unlock(&g_stats_lock);
        return -1;
    }
    
    return 0;
}

/**
 * System call: sigsuspend - Suspend until signal
 */
long sys_sigsuspend(const sigset_t* mask) {
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    spin_lock(&g_stats_lock);
    g_signal_syscall_stats.sigsuspend_calls++;
    spin_unlock(&g_stats_lock);
    
    /* Copy mask from user space */
    sigset_t kernel_mask;
    if (mask) {
        if (signal_syscall_copy_sigset_from_user(&kernel_mask, mask) != 0) {
            spin_lock(&g_stats_lock);
            g_signal_syscall_stats.user_copy_errors++;
            spin_unlock(&g_stats_lock);
            return -1;
        }
    } else {
        sigemptyset(&kernel_mask);
    }
    
    /* Suspend process with temporary mask */
    return signal_syscall_suspend_process(current, &kernel_mask);
}

/**
 * System call: sigqueue - Send RT signal with value
 */
long sys_sigqueue(pid_t pid, int sig, const union sigval* value) {
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    spin_lock(&g_stats_lock);
    g_signal_syscall_stats.sigqueue_calls++;
    spin_unlock(&g_stats_lock);
    
    /* Validate signal number (must be RT signal) */
    if (!signal_is_realtime(sig)) {
        spin_lock(&g_stats_lock);
        g_signal_syscall_stats.invalid_signals++;
        spin_unlock(&g_stats_lock);
        return -1;
    }
    
    /* Find target process */
    process_t* target = signal_syscall_find_process(pid);
    if (!target) {
        return -1;
    }
    
    /* Check permission */
    if (!signal_syscall_check_permission(current, target, sig)) {
        spin_lock(&g_stats_lock);
        g_signal_syscall_stats.permission_denied++;
        spin_unlock(&g_stats_lock);
        return -1;
    }
    
    /* Create signal info with value */
    siginfo_t info;
    signal_init_info(&info, sig, SIGNAL_SOURCE_PROCESS);
    signal_set_sender_info(&info, current->pid, current->uid);
    
    if (value) {
        info.si_value = *value;
    }
    
    /* Generate RT signal */
    if (signal_generate(target, sig, &info, SIGNAL_SOURCE_PROCESS, 
                       SIGNAL_DELIVER_QUEUE) != 0) {
        return -1;
    }
    
    return 0;
}

/**
 * System call: sigwaitinfo - Wait for signals
 */
long sys_sigwaitinfo(const sigset_t* set, siginfo_t* info) {
    return sys_sigtimedwait(set, info, NULL);
}

/**
 * System call: sigtimedwait - Wait for signals with timeout
 */
long sys_sigtimedwait(const sigset_t* set, siginfo_t* info, const struct timespec* timeout) {
    process_t* current = get_current_process();
    if (!current || !set) {
        return -1;
    }
    
    spin_lock(&g_stats_lock);
    g_signal_syscall_stats.sigwait_calls++;
    spin_unlock(&g_stats_lock);
    
    /* Copy signal set from user space */
    sigset_t wait_set;
    if (signal_syscall_copy_sigset_from_user(&wait_set, set) != 0) {
        spin_lock(&g_stats_lock);
        g_signal_syscall_stats.user_copy_errors++;
        spin_unlock(&g_stats_lock);
        return -1;
    }
    
    /* Convert timeout to milliseconds */
    uint32_t timeout_ms = 0;
    if (timeout) {
        timeout_ms = timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000;
    }
    
    /* Wait for signal */
    siginfo_t kernel_info;
    int result = signal_syscall_wait_for_signal(current, &wait_set, &kernel_info, timeout_ms);
    
    if (result > 0 && info) {
        /* Copy signal info to user space */
        if (signal_syscall_copy_siginfo_to_user(info, &kernel_info) != 0) {
            spin_lock(&g_stats_lock);
            g_signal_syscall_stats.user_copy_errors++;
            spin_unlock(&g_stats_lock);
            return -1;
        }
    }
    
    return result;
}

/**
 * System call: sigaltstack - Set/get signal stack
 */
long sys_sigaltstack(const stack_t* stack, stack_t* oldstack) {
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    spin_lock(&g_stats_lock);
    g_signal_syscall_stats.sigaltstack_calls++;
    spin_unlock(&g_stats_lock);
    
    /* Copy new stack from user space if provided */
    stack_t kernel_stack;
    if (stack) {
        if (signal_syscall_copy_stack_from_user(&kernel_stack, stack) != 0) {
            spin_lock(&g_stats_lock);
            g_signal_syscall_stats.user_copy_errors++;
            spin_unlock(&g_stats_lock);
            return -1;
        }
    }
    
    /* Get old stack if requested */
    stack_t old_stack_info;
    if (oldstack) {
        if (signal_stack_get(current, &old_stack_info) != 0) {
            return -1;
        }
    }
    
    /* Set new stack */
    if (stack) {
        if (signal_stack_set(current, &kernel_stack, NULL) != 0) {
            return -1;
        }
    }
    
    /* Copy old stack to user space */
    if (oldstack) {
        if (signal_syscall_copy_stack_to_user(oldstack, &old_stack_info) != 0) {
            spin_lock(&g_stats_lock);
            g_signal_syscall_stats.user_copy_errors++;
            spin_unlock(&g_stats_lock);
            return -1;
        }
    }
    
    return 0;
}

/**
 * System call: alarm - Set alarm timer
 */
long sys_alarm(unsigned int seconds) {
    process_t* current = get_current_process();
    if (!current) {
        return 0;
    }
    
    spin_lock(&g_stats_lock);
    g_signal_syscall_stats.alarm_calls++;
    spin_unlock(&g_stats_lock);
    
    return signal_syscall_set_alarm(current, seconds);
}

/**
 * System call: pause - Suspend until signal
 */
long sys_pause(void) {
    process_t* current = get_current_process();
    if (!current) {
        return -1;
    }
    
    spin_lock(&g_stats_lock);
    g_signal_syscall_stats.pause_calls++;
    spin_unlock(&g_stats_lock);
    
    /* Pause is equivalent to sigsuspend with current mask */
    sigset_t current_mask;
    if (signal_mask_get(current, &current_mask) != 0) {
        return -1;
    }
    
    return signal_syscall_suspend_process(current, &current_mask);
}

/* ========================== Helper Functions ========================== */

/**
 * Validate user pointer for signal system calls
 */
bool signal_syscall_validate_user_ptr(const void* ptr, size_t size) {
    /* This should validate that the pointer is in user space and accessible */
    /* Placeholder implementation - should be replaced with proper validation */
    return (ptr != NULL && (uintptr_t)ptr >= 0x10000 && 
            (uintptr_t)ptr + size < 0x800000000000ULL);
}

/**
 * Copy signal set from user space
 */
int signal_syscall_copy_sigset_from_user(sigset_t* dest, const sigset_t* src) {
    if (!dest || !src) {
        return -1;
    }
    
    if (!signal_syscall_validate_user_ptr(src, sizeof(sigset_t))) {
        return -1;
    }
    
    /* Copy from user space - this should use proper copy_from_user function */
    memcpy(dest, src, sizeof(sigset_t));
    return 0;
}

/**
 * Copy signal set to user space
 */
int signal_syscall_copy_sigset_to_user(sigset_t* dest, const sigset_t* src) {
    if (!dest || !src) {
        return -1;
    }
    
    if (!signal_syscall_validate_user_ptr(dest, sizeof(sigset_t))) {
        return -1;
    }
    
    /* Copy to user space - this should use proper copy_to_user function */
    memcpy(dest, src, sizeof(sigset_t));
    return 0;
}

/**
 * Copy signal action from user space
 */
int signal_syscall_copy_sigaction_from_user(struct sigaction* dest, const struct sigaction* src) {
    if (!dest || !src) {
        return -1;
    }
    
    if (!signal_syscall_validate_user_ptr(src, sizeof(struct sigaction))) {
        return -1;
    }
    
    /* Copy from user space */
    memcpy(dest, src, sizeof(struct sigaction));
    return 0;
}

/**
 * Copy signal action to user space
 */
int signal_syscall_copy_sigaction_to_user(struct sigaction* dest, const struct sigaction* src) {
    if (!dest || !src) {
        return -1;
    }
    
    if (!signal_syscall_validate_user_ptr(dest, sizeof(struct sigaction))) {
        return -1;
    }
    
    /* Copy to user space */
    memcpy(dest, src, sizeof(struct sigaction));
    return 0;
}

/**
 * Copy signal info to user space
 */
int signal_syscall_copy_siginfo_to_user(siginfo_t* dest, const siginfo_t* src) {
    if (!dest || !src) {
        return -1;
    }
    
    if (!signal_syscall_validate_user_ptr(dest, sizeof(siginfo_t))) {
        return -1;
    }
    
    /* Copy to user space */
    memcpy(dest, src, sizeof(siginfo_t));
    return 0;
}

/**
 * Copy signal stack from user space
 */
int signal_syscall_copy_stack_from_user(stack_t* dest, const stack_t* src) {
    if (!dest || !src) {
        return -1;
    }
    
    if (!signal_syscall_validate_user_ptr(src, sizeof(stack_t))) {
        return -1;
    }
    
    /* Copy from user space */
    memcpy(dest, src, sizeof(stack_t));
    return 0;
}

/**
 * Copy signal stack to user space
 */
int signal_syscall_copy_stack_to_user(stack_t* dest, const stack_t* src) {
    if (!dest || !src) {
        return -1;
    }
    
    if (!signal_syscall_validate_user_ptr(dest, sizeof(stack_t))) {
        return -1;
    }
    
    /* Copy to user space */
    memcpy(dest, src, sizeof(stack_t));
    return 0;
}

/**
 * Check permission to send signal to process
 */
bool signal_syscall_check_permission(process_t* sender, process_t* target, int sig) {
    if (!sender || !target) {
        return false;
    }
    
    /* Root can send any signal to any process */
    if (sender->uid == 0) {
        return true;
    }
    
    /* Processes can always send signals to themselves */
    if (sender->pid == target->pid) {
        return true;
    }
    
    /* Processes can send signals to processes with same effective UID */
    if (sender->uid == target->uid) {
        return true;
    }
    
    /* SIGCONT can be sent to processes in same session */
    if (sig == SIGCONT && sender->sid == target->sid) {
        return true;
    }
    
    /* Otherwise, permission denied */
    return false;
}

/**
 * Find process by PID
 */
process_t* signal_syscall_find_process(pid_t pid) {
    /* This should search the process table for the given PID */
    /* Placeholder implementation - should be replaced with proper lookup */
    return find_process_by_pid(pid);
}

/**
 * Suspend process until signal
 */
int signal_syscall_suspend_process(process_t* proc, const sigset_t* mask) {
    if (!proc) {
        return -1;
    }
    
    /* Set temporary mask */
    signal_mask_suspend(proc, mask);
    
    /* Block process until signal arrives */
    proc->state = PROCESS_BLOCKED;
    
    /* This should trigger a context switch to another process */
    /* The process will be woken up when a signal is delivered */
    schedule_next_process();
    
    /* When we get here, a signal was delivered */
    /* Restore original mask */
    signal_mask_restore(proc);
    
    /* Return -1 with EINTR (interrupted by signal) */
    return -1;
}

/**
 * Set alarm timer for process
 */
unsigned int signal_syscall_set_alarm(process_t* proc, unsigned int seconds) {
    if (!proc) {
        return 0;
    }
    
    /* Get remaining time from previous alarm */
    unsigned int remaining = 0;
    if (proc->alarm_time > 0) {
        uint64_t current_time = get_current_time_us() / 1000000; /* Convert to seconds */
        if (proc->alarm_time > current_time) {
            remaining = (unsigned int)(proc->alarm_time - current_time);
        }
        
        /* Cancel previous alarm */
        alarm_cancel(proc->pid);
    }
    
    /* Set new alarm */
    if (seconds > 0) {
        uint64_t alarm_time = get_current_time_us() / 1000000 + seconds;
        proc->alarm_time = alarm_time;
        
        /* Schedule alarm with timer system */
        timer_set_alarm(proc->pid, seconds);
    } else {
        proc->alarm_time = 0;
    }
    
    return remaining;
}

/**
 * Get current process (placeholder)
 */
process_t* get_current_process(void) {
    /* This should return the currently executing process */
    /* Placeholder implementation */
    extern process_t* current_process;
    return current_process;
}

/**
 * Find process by PID (placeholder)
 */
process_t* find_process_by_pid(pid_t pid) {
    /* This should search the process table */
    /* Placeholder implementation */
    return NULL;
}

/**
 * Schedule next process (placeholder)
 */
void schedule_next_process(void) {
    /* This should trigger the scheduler */
    /* Placeholder implementation */
}

/**
 * Timer functions (placeholders)
 */
void timer_set_alarm(pid_t pid, unsigned int seconds) {
    /* Placeholder */
}

void alarm_cancel(pid_t pid) {
    /* Placeholder */
}

uint64_t get_current_time_us(void) {
    /* Placeholder */
    static uint64_t fake_time = 0;
    return fake_time++;
}

/* ========================== Statistics Functions ========================== */

/**
 * Get signal system call statistics
 */
int signal_syscall_get_stats(signal_syscall_stats_t* stats) {
    if (!stats) {
        return -1;
    }
    
    spin_lock(&g_stats_lock);
    *stats = g_signal_syscall_stats;
    spin_unlock(&g_stats_lock);
    
    return 0;
}

/**
 * Reset signal system call statistics
 */
int signal_syscall_reset_stats(void) {
    spin_lock(&g_stats_lock);
    memset(&g_signal_syscall_stats, 0, sizeof(g_signal_syscall_stats));
    spin_unlock(&g_stats_lock);
    
    return 0;
}

/* ========================== Placeholder Functions ========================== */

/* Memory management placeholders */
void* kmalloc(size_t size) {
    /* Placeholder - should use real kernel allocator */
    return malloc(size);
}

void kfree(void* ptr) {
    /* Placeholder - should use real kernel deallocator */
    free(ptr);
}

/* Standard library placeholders */
int printf(const char* format, ...) {
    /* Placeholder printf implementation */
    return 0;
}

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

/* Placeholder user space access functions */
int copy_from_user(void* dest, const void* src, size_t size) {
    /* Placeholder - should validate and copy from user space */
    memcpy(dest, src, size);
    return 0;
}

int copy_to_user(void* dest, const void* src, size_t size) {
    /* Placeholder - should validate and copy to user space */
    memcpy(dest, src, size);
    return 0;
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
