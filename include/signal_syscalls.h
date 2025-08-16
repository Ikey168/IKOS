/* IKOS Signal System Calls - Issue #19
 * POSIX-compatible signal system call interface
 */

#ifndef SIGNAL_SYSCALLS_H
#define SIGNAL_SYSCALLS_H

#include <stdint.h>
#include <stdbool.h>
#include "signal_delivery.h"
#include "signal_mask.h"
#include "process.h"

/* ========================== System Call Numbers ========================== */

#define SYS_SIGNAL              48      /* signal() */
#define SYS_SIGACTION           49      /* sigaction() */
#define SYS_KILL                50      /* kill() */
#define SYS_SIGPROCMASK         51      /* sigprocmask() */
#define SYS_SIGPENDING          52      /* sigpending() */
#define SYS_SIGSUSPEND          53      /* sigsuspend() */
#define SYS_SIGQUEUE            54      /* sigqueue() - RT signals */
#define SYS_SIGTIMEDWAIT        55      /* sigtimedwait() - RT signals */
#define SYS_SIGWAITINFO         56      /* sigwaitinfo() - RT signals */
#define SYS_SIGALTSTACK         57      /* sigaltstack() */
#define SYS_ALARM               58      /* alarm() */
#define SYS_PAUSE               59      /* pause() */

/* ========================== Signal System Call Interface ========================== */

/**
 * System call: signal - Install signal handler (simple interface)
 * @param sig Signal number
 * @param handler Signal handler function
 * @return Previous handler on success, SIG_ERR on error
 */
long sys_signal(int sig, signal_handler_t handler);

/**
 * System call: sigaction - Install signal action (advanced interface)
 * @param sig Signal number
 * @param act New signal action (NULL to query)
 * @param oldact Buffer for previous action (can be NULL)
 * @return 0 on success, -1 on error
 */
long sys_sigaction(int sig, const struct sigaction* act, struct sigaction* oldact);

/**
 * System call: kill - Send signal to process or process group
 * @param pid Target process ID (special values for groups)
 * @param sig Signal number to send
 * @return 0 on success, -1 on error
 */
long sys_kill(pid_t pid, int sig);

/**
 * System call: sigprocmask - Change signal mask
 * @param how How to change mask (SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK)
 * @param set Signal set to apply (NULL to query)
 * @param oldset Buffer for old mask (can be NULL)
 * @return 0 on success, -1 on error
 */
long sys_sigprocmask(int how, const sigset_t* set, sigset_t* oldset);

/**
 * System call: sigpending - Get pending signals
 * @param set Buffer for pending signal set
 * @return 0 on success, -1 on error
 */
long sys_sigpending(sigset_t* set);

/**
 * System call: sigsuspend - Suspend until signal
 * @param mask Temporary signal mask during suspend
 * @return -1 (always interrupted), errno set to EINTR
 */
long sys_sigsuspend(const sigset_t* mask);

/**
 * System call: sigqueue - Send RT signal with value
 * @param pid Target process ID
 * @param sig Signal number (must be RT signal)
 * @param value Signal value to send
 * @return 0 on success, -1 on error
 */
long sys_sigqueue(pid_t pid, int sig, const union sigval* value);

/**
 * System call: sigtimedwait - Wait for signals with timeout
 * @param set Set of signals to wait for
 * @param info Buffer for signal information
 * @param timeout Timeout specification (NULL for no timeout)
 * @return Signal number on success, -1 on error/timeout
 */
long sys_sigtimedwait(const sigset_t* set, siginfo_t* info, const struct timespec* timeout);

/**
 * System call: sigwaitinfo - Wait for signals
 * @param set Set of signals to wait for
 * @param info Buffer for signal information
 * @return Signal number on success, -1 on error
 */
long sys_sigwaitinfo(const sigset_t* set, siginfo_t* info);

/**
 * System call: sigaltstack - Set/get signal stack
 * @param stack New signal stack (NULL to query)
 * @param oldstack Buffer for old stack (can be NULL)
 * @return 0 on success, -1 on error
 */
long sys_sigaltstack(const stack_t* stack, stack_t* oldstack);

/**
 * System call: alarm - Set alarm timer
 * @param seconds Alarm timeout in seconds (0 to cancel)
 * @return Previous alarm time remaining, 0 if no alarm set
 */
long sys_alarm(unsigned int seconds);

/**
 * System call: pause - Suspend until signal
 * @return -1 (always interrupted), errno set to EINTR
 */
long sys_pause(void);

/* ========================== Signal Validation Functions ========================== */

/**
 * Validate user pointer for signal system calls
 * @param ptr User pointer to validate
 * @param size Size of data structure
 * @return true if valid, false otherwise
 */
bool signal_syscall_validate_user_ptr(const void* ptr, size_t size);

/**
 * Copy signal set from user space
 * @param dest Kernel buffer
 * @param src User pointer
 * @return 0 on success, -1 on error
 */
int signal_syscall_copy_sigset_from_user(sigset_t* dest, const sigset_t* src);

/**
 * Copy signal set to user space
 * @param dest User pointer
 * @param src Kernel buffer
 * @return 0 on success, -1 on error
 */
int signal_syscall_copy_sigset_to_user(sigset_t* dest, const sigset_t* src);

/**
 * Copy signal action from user space
 * @param dest Kernel buffer
 * @param src User pointer
 * @return 0 on success, -1 on error
 */
int signal_syscall_copy_sigaction_from_user(struct sigaction* dest, const struct sigaction* src);

/**
 * Copy signal action to user space
 * @param dest User pointer
 * @param src Kernel buffer
 * @return 0 on success, -1 on error
 */
int signal_syscall_copy_sigaction_to_user(struct sigaction* dest, const struct sigaction* src);

/**
 * Copy signal info to user space
 * @param dest User pointer
 * @param src Kernel buffer
 * @return 0 on success, -1 on error
 */
int signal_syscall_copy_siginfo_to_user(siginfo_t* dest, const siginfo_t* src);

/**
 * Copy signal stack from user space
 * @param dest Kernel buffer
 * @param src User pointer
 * @return 0 on success, -1 on error
 */
int signal_syscall_copy_stack_from_user(stack_t* dest, const stack_t* src);

/**
 * Copy signal stack to user space
 * @param dest User pointer
 * @param src Kernel buffer
 * @return 0 on success, -1 on error
 */
int signal_syscall_copy_stack_to_user(stack_t* dest, const stack_t* src);

/* ========================== Signal Process Management ========================== */

/**
 * Send signal to process group
 * @param pgrp Process group ID
 * @param sig Signal number
 * @param sender_proc Sending process
 * @return Number of processes signaled, -1 on error
 */
int signal_syscall_kill_process_group(pid_t pgrp, int sig, process_t* sender_proc);

/**
 * Send signal to all processes (pid = -1)
 * @param sig Signal number
 * @param sender_proc Sending process
 * @return Number of processes signaled, -1 on error
 */
int signal_syscall_kill_all_processes(int sig, process_t* sender_proc);

/**
 * Check permission to send signal to process
 * @param sender Sending process
 * @param target Target process
 * @param sig Signal number
 * @return true if allowed, false otherwise
 */
bool signal_syscall_check_permission(process_t* sender, process_t* target, int sig);

/**
 * Find process by PID for signal delivery
 * @param pid Process ID
 * @return Process pointer, NULL if not found
 */
process_t* signal_syscall_find_process(pid_t pid);

/* ========================== Signal Waiting and Blocking ========================== */

/**
 * Wait for any signal in set
 * @param proc Waiting process
 * @param set Set of signals to wait for
 * @param info Buffer for signal information
 * @param timeout_ms Timeout in milliseconds (0 for infinite)
 * @return Signal number on success, 0 on timeout, -1 on error
 */
int signal_syscall_wait_for_signal(process_t* proc, const sigset_t* set, 
                                  siginfo_t* info, uint32_t timeout_ms);

/**
 * Block process until signal arrives
 * @param proc Process to block
 * @param mask Temporary signal mask
 * @return 0 on signal received, -1 on error
 */
int signal_syscall_suspend_process(process_t* proc, const sigset_t* mask);

/**
 * Wake up process blocked on signal
 * @param proc Process to wake up
 * @param signal Signal that caused wakeup
 * @return 0 on success, -1 on error
 */
int signal_syscall_wakeup_process(process_t* proc, int signal);

/* ========================== Signal Timer Integration ========================== */

/**
 * Set alarm timer for process
 * @param proc Target process
 * @param seconds Alarm timeout in seconds
 * @return Previous alarm time remaining
 */
unsigned int signal_syscall_set_alarm(process_t* proc, unsigned int seconds);

/**
 * Cancel alarm timer for process
 * @param proc Target process
 * @return Previous alarm time remaining
 */
unsigned int signal_syscall_cancel_alarm(process_t* proc);

/**
 * Handle alarm timer expiration
 * @param proc Process whose alarm expired
 */
void signal_syscall_alarm_expired(process_t* proc);

/* ========================== Error Codes ========================== */

/* Signal-specific error codes */
#define ESIGPERM                1       /* Permission denied for signal */
#define ESIGNOSYS               2       /* Signal system call not supported */
#define ESIGFAULT               3       /* Signal caused memory fault */
#define ESIGTIME                4       /* Signal operation timed out */
#define ESIGQUEUE               5       /* Signal queue full */
#define ESIGSTACK               6       /* Signal stack error */

/* ========================== Signal System Call Statistics ========================== */

/**
 * Signal system call statistics
 */
typedef struct signal_syscall_stats {
    uint64_t signal_calls;              /* signal() calls */
    uint64_t sigaction_calls;           /* sigaction() calls */
    uint64_t kill_calls;                /* kill() calls */
    uint64_t sigprocmask_calls;         /* sigprocmask() calls */
    uint64_t sigpending_calls;          /* sigpending() calls */
    uint64_t sigsuspend_calls;          /* sigsuspend() calls */
    uint64_t sigqueue_calls;            /* sigqueue() calls */
    uint64_t sigwait_calls;             /* sigtimedwait/sigwaitinfo calls */
    uint64_t sigaltstack_calls;         /* sigaltstack() calls */
    uint64_t alarm_calls;               /* alarm() calls */
    uint64_t pause_calls;               /* pause() calls */
    uint64_t permission_denied;         /* Permission denied count */
    uint64_t invalid_signals;           /* Invalid signal number count */
    uint64_t user_copy_errors;          /* User space copy errors */
} signal_syscall_stats_t;

/**
 * Get signal system call statistics
 * @param stats Buffer for statistics
 * @return 0 on success, -1 on error
 */
int signal_syscall_get_stats(signal_syscall_stats_t* stats);

/**
 * Reset signal system call statistics
 * @return 0 on success, -1 on error
 */
int signal_syscall_reset_stats(void);

/* ========================== Signal System Call Handler ========================== */

/**
 * Main signal system call handler
 * @param syscall_num System call number
 * @param arg1 First argument
 * @param arg2 Second argument
 * @param arg3 Third argument
 * @param arg4 Fourth argument
 * @return System call result
 */
long signal_syscall_handler(uint32_t syscall_num, uint64_t arg1, uint64_t arg2, 
                           uint64_t arg3, uint64_t arg4);

/* ========================== Signal Context Management ========================== */

/**
 * Signal context for handler execution
 */
typedef struct signal_context {
    /* General purpose registers */
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    
    /* Control registers */
    uint64_t rip;                       /* Instruction pointer */
    uint64_t rflags;                    /* CPU flags */
    
    /* Segment registers */
    uint16_t cs, ds, es, fs, gs, ss;
    
    /* FPU/SSE state */
    uint8_t fpu_state[512] __attribute__((aligned(16)));
    
    /* Signal-specific information */
    int signal_num;                     /* Signal being handled */
    siginfo_t signal_info;              /* Signal information */
    sigset_t old_mask;                  /* Signal mask before handler */
    
    /* Stack information */
    void* signal_stack_base;            /* Signal stack base */
    size_t signal_stack_size;           /* Signal stack size */
    bool on_signal_stack;               /* Whether using signal stack */
} signal_context_t;

/**
 * Save process context for signal handler
 * @param proc Target process
 * @param context Buffer for context
 * @return 0 on success, -1 on error
 */
int signal_context_save(process_t* proc, signal_context_t* context);

/**
 * Restore process context after signal handler
 * @param proc Target process
 * @param context Saved context
 * @return 0 on success, -1 on error
 */
int signal_context_restore(process_t* proc, const signal_context_t* context);

/**
 * Setup signal handler execution context
 * @param proc Target process
 * @param signal Signal number
 * @param handler Handler function
 * @param info Signal information
 * @param context Current process context
 * @return 0 on success, -1 on error
 */
int signal_context_setup_handler(process_t* proc, int signal, signal_handler_t handler,
                                const siginfo_t* info, signal_context_t* context);

#endif /* SIGNAL_SYSCALLS_H */
