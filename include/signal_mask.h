/* IKOS Signal Masking and Control System - Issue #19
 * Signal blocking, unblocking, and mask management
 */

#ifndef SIGNAL_MASK_H
#define SIGNAL_MASK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <sys/types.h>
#include "process.h"

/* Define missing types if not available */
#ifndef _UID_T_DEFINED
#define _UID_T_DEFINED
typedef unsigned int uid_t;
#endif

#ifndef _CLOCK_T_DEFINED
#define _CLOCK_T_DEFINED
typedef long clock_t;
#endif

/* Define spinlock type if not available */
#ifndef _SPINLOCK_T_DEFINED
#define _SPINLOCK_T_DEFINED
typedef struct {
    volatile int locked;
} spinlock_t;
#endif

/* ========================== Constants and Defines ========================== */

/* Signal set operations */
#define SIG_BLOCK               0       /* Block signals in set */
#define SIG_UNBLOCK             1       /* Unblock signals in set */
#define SIG_SETMASK             2       /* Set signal mask to set */

/* Signal set size */
#define _NSIG                   64      /* Number of signals */
#define _SIGSET_NWORDS          ((_NSIG + 63) / 64)

/* Special signal values */
#define SIG_DFL                 ((signal_handler_t) 0)   /* Default handler */
#define SIG_IGN                 ((signal_handler_t) 1)   /* Ignore signal */
#define SIG_ERR                 ((signal_handler_t) -1)  /* Error return */

/* Signal action flags */
#define SA_NOCLDSTOP            0x0001  /* Don't send SIGCHLD on child stop */
#define SA_NOCLDWAIT            0x0002  /* Don't create zombie on child death */
#define SA_SIGINFO              0x0004  /* Use sa_sigaction instead of sa_handler */
#define SA_ONSTACK              0x0008  /* Use signal stack */
#define SA_RESTART              0x0010  /* Restart interrupted system calls */
#define SA_NODEFER              0x0040  /* Don't block signal during handler */
#define SA_RESETHAND            0x0080  /* Reset handler after signal */

/* Signal stack flags */
#define SS_ONSTACK              0x0001  /* Currently using signal stack */
#define SS_DISABLE              0x0002  /* Disable signal stack */

/* Minimum signal stack size */
#define SIGSTKSZ                8192    /* Standard signal stack size */
#define MINSIGSTKSZ             2048    /* Minimum signal stack size */

/* ========================== Data Structures ========================== */

/**
 * Signal set type
 */
typedef struct {
    uint64_t sig[_SIGSET_NWORDS];
} sigset_t;

/**
 * Signal handler function pointer
 */
typedef void (*signal_handler_t)(int signal);

/**
 * Extended signal handler function pointer
 */
typedef void (*signal_action_t)(int signal, siginfo_t* info, void* context);

/**
 * Signal action structure (POSIX sigaction)
 */
struct sigaction {
    union {
        signal_handler_t sa_handler;    /* Simple signal handler */
        signal_action_t sa_sigaction;   /* Extended signal handler */
    };
    sigset_t sa_mask;                   /* Additional signals to block */
    int sa_flags;                       /* Signal action flags */
    void (*sa_restorer)(void);          /* Signal return function (internal) */
};

/**
 * Signal stack structure
 */
typedef struct {
    void* ss_sp;                        /* Stack pointer */
    int ss_flags;                       /* Stack flags */
    size_t ss_size;                     /* Stack size */
} stack_t;

/**
 * Signal masking state for a process
 */
typedef struct signal_mask_state {
    sigset_t signal_mask;               /* Currently blocked signals */
    sigset_t saved_mask;                /* Saved mask (for sigsuspend) */
    struct sigaction actions[_NSIG];    /* Signal actions */
    stack_t signal_stack;               /* Alternative signal stack */
    bool mask_suspended;                /* Whether mask is temporarily changed */
    uint32_t mask_change_count;         /* Number of mask changes */
    spinlock_t mask_lock;               /* Mask modification lock */
} signal_mask_state_t;

/* ========================== Signal Set Operations ========================== */

/**
 * Initialize signal set to empty
 * @param set Signal set to initialize
 * @return 0 on success, -1 on error
 */
int sigemptyset(sigset_t* set);

/**
 * Initialize signal set to full (all signals)
 * @param set Signal set to initialize
 * @return 0 on success, -1 on error
 */
int sigfillset(sigset_t* set);

/**
 * Add signal to signal set
 * @param set Signal set to modify
 * @param signum Signal number to add
 * @return 0 on success, -1 on error
 */
int sigaddset(sigset_t* set, int signum);

/**
 * Remove signal from signal set
 * @param set Signal set to modify
 * @param signum Signal number to remove
 * @return 0 on success, -1 on error
 */
int sigdelset(sigset_t* set, int signum);

/**
 * Test if signal is in signal set
 * @param set Signal set to test
 * @param signum Signal number to test
 * @return 1 if signal is in set, 0 if not, -1 on error
 */
int sigismember(const sigset_t* set, int signum);

/**
 * Check if signal set is empty
 * @param set Signal set to check
 * @return true if empty, false otherwise
 */
bool sigset_is_empty(const sigset_t* set);

/**
 * Check if signal set is full
 * @param set Signal set to check
 * @return true if full, false otherwise
 */
bool sigset_is_full(const sigset_t* set);

/**
 * Count number of signals in set
 * @param set Signal set to count
 * @return Number of signals in set
 */
int sigset_count(const sigset_t* set);

/**
 * Copy signal set
 * @param dest Destination set
 * @param src Source set
 * @return 0 on success, -1 on error
 */
int sigset_copy(sigset_t* dest, const sigset_t* src);

/**
 * Perform bitwise OR of two signal sets
 * @param dest Destination set (result)
 * @param src1 First source set
 * @param src2 Second source set
 * @return 0 on success, -1 on error
 */
int sigset_or(sigset_t* dest, const sigset_t* src1, const sigset_t* src2);

/**
 * Perform bitwise AND of two signal sets
 * @param dest Destination set (result)
 * @param src1 First source set
 * @param src2 Second source set
 * @return 0 on success, -1 on error
 */
int sigset_and(sigset_t* dest, const sigset_t* src1, const sigset_t* src2);

/**
 * Perform bitwise NOT of signal set
 * @param dest Destination set (result)
 * @param src Source set
 * @return 0 on success, -1 on error
 */
int sigset_not(sigset_t* dest, const sigset_t* src);

/**
 * Convert signal set to bitmask
 * @param set Signal set
 * @return 64-bit bitmask representation
 */
uint64_t sigset_to_mask(const sigset_t* set);

/**
 * Convert bitmask to signal set
 * @param set Signal set to fill
 * @param mask 64-bit bitmask
 * @return 0 on success, -1 on error
 */
int mask_to_sigset(sigset_t* set, uint64_t mask);

/* ========================== Signal Masking Functions ========================== */

/**
 * Initialize signal masking state for a process
 * @param proc Process to initialize
 * @return 0 on success, -1 on error
 */
int signal_mask_init_process(process_t* proc);

/**
 * Cleanup signal masking state for a process
 * @param proc Process to cleanup
 */
void signal_mask_cleanup_process(process_t* proc);

/**
 * Change process signal mask
 * @param proc Target process
 * @param how How to change mask (SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK)
 * @param set Signal set to apply
 * @param oldset Buffer for old mask (can be NULL)
 * @return 0 on success, -1 on error
 */
int signal_mask_change(process_t* proc, int how, const sigset_t* set, sigset_t* oldset);

/**
 * Get process signal mask
 * @param proc Target process
 * @param mask Buffer for current mask
 * @return 0 on success, -1 on error
 */
int signal_mask_get(process_t* proc, sigset_t* mask);

/**
 * Set process signal mask
 * @param proc Target process
 * @param mask New signal mask
 * @param oldmask Buffer for old mask (can be NULL)
 * @return 0 on success, -1 on error
 */
int signal_mask_set(process_t* proc, const sigset_t* mask, sigset_t* oldmask);

/**
 * Block signals in the given set
 * @param proc Target process
 * @param set Signals to block
 * @return 0 on success, -1 on error
 */
int signal_mask_block(process_t* proc, const sigset_t* set);

/**
 * Unblock signals in the given set
 * @param proc Target process
 * @param set Signals to unblock
 * @return 0 on success, -1 on error
 */
int signal_mask_unblock(process_t* proc, const sigset_t* set);

/**
 * Check if a specific signal is blocked
 * @param proc Target process
 * @param signal Signal number to check
 * @return true if blocked, false otherwise
 */
bool signal_mask_is_blocked(process_t* proc, int signal);

/**
 * Get set of pending signals (blocked and undelivered)
 * @param proc Target process
 * @param pending Buffer for pending signal set
 * @return 0 on success, -1 on error
 */
int signal_mask_get_pending(process_t* proc, sigset_t* pending);

/**
 * Temporarily change signal mask (for sigsuspend)
 * @param proc Target process
 * @param mask New temporary mask
 * @return 0 on success, -1 on error
 */
int signal_mask_suspend(process_t* proc, const sigset_t* mask);

/**
 * Restore signal mask after suspension
 * @param proc Target process
 * @return 0 on success, -1 on error
 */
int signal_mask_restore(process_t* proc);

/* ========================== Signal Action Management ========================== */

/**
 * Set signal action for a process
 * @param proc Target process
 * @param signal Signal number
 * @param act New signal action
 * @param oldact Buffer for old action (can be NULL)
 * @return 0 on success, -1 on error
 */
int signal_action_set(process_t* proc, int signal, const struct sigaction* act, 
                     struct sigaction* oldact);

/**
 * Get signal action for a process
 * @param proc Target process
 * @param signal Signal number
 * @param act Buffer for signal action
 * @return 0 on success, -1 on error
 */
int signal_action_get(process_t* proc, int signal, struct sigaction* act);

/**
 * Set simple signal handler
 * @param proc Target process
 * @param signal Signal number
 * @param handler Signal handler function
 * @return Previous handler, SIG_ERR on error
 */
signal_handler_t signal_handler_set(process_t* proc, int signal, signal_handler_t handler);

/**
 * Get signal handler
 * @param proc Target process
 * @param signal Signal number
 * @return Signal handler, SIG_ERR on error
 */
signal_handler_t signal_handler_get(process_t* proc, int signal);

/**
 * Reset signal action to default
 * @param proc Target process
 * @param signal Signal number
 * @return 0 on success, -1 on error
 */
int signal_action_reset(process_t* proc, int signal);

/**
 * Reset all signal actions to default
 * @param proc Target process
 * @return 0 on success, -1 on error
 */
int signal_action_reset_all(process_t* proc);

/**
 * Check if signal has custom handler
 * @param proc Target process
 * @param signal Signal number
 * @return true if custom handler, false if default/ignore
 */
bool signal_has_custom_handler(process_t* proc, int signal);

/**
 * Check if signal is ignored
 * @param proc Target process
 * @param signal Signal number
 * @return true if ignored, false otherwise
 */
bool signal_is_ignored(process_t* proc, int signal);

/* ========================== Signal Stack Management ========================== */

/**
 * Set alternative signal stack
 * @param proc Target process
 * @param stack New signal stack
 * @param oldstack Buffer for old stack (can be NULL)
 * @return 0 on success, -1 on error
 */
int signal_stack_set(process_t* proc, const stack_t* stack, stack_t* oldstack);

/**
 * Get current signal stack
 * @param proc Target process
 * @param stack Buffer for stack information
 * @return 0 on success, -1 on error
 */
int signal_stack_get(process_t* proc, stack_t* stack);

/**
 * Check if currently executing on signal stack
 * @param proc Target process
 * @return true if on signal stack, false otherwise
 */
bool signal_stack_is_active(process_t* proc);

/**
 * Allocate memory for signal stack
 * @param size Stack size in bytes
 * @return Pointer to allocated stack, NULL on error
 */
void* signal_stack_alloc(size_t size);

/**
 * Free signal stack memory
 * @param stack Stack pointer to free
 * @param size Stack size in bytes
 */
void signal_stack_free(void* stack, size_t size);

/* ========================== Signal Mask Utilities ========================== */

/**
 * Validate signal number
 * @param signal Signal number to validate
 * @return true if valid, false otherwise
 */
bool signal_mask_is_valid_signal(int signal);

/**
 * Check if signal can be blocked
 * @param signal Signal number
 * @return true if blockable, false otherwise
 */
bool signal_mask_is_blockable(int signal);

/**
 * Get default signal action
 * @param signal Signal number
 * @return Default action (SIG_DFL, SIG_IGN, or specific handler)
 */
signal_handler_t signal_mask_get_default_action(int signal);

/**
 * Check if signal should terminate process by default
 * @param signal Signal number
 * @return true if signal terminates by default, false otherwise
 */
bool signal_mask_is_fatal_by_default(int signal);

/**
 * Check if signal should stop process by default
 * @param signal Signal number
 * @return true if signal stops by default, false otherwise
 */
bool signal_mask_is_stop_by_default(int signal);

/**
 * Check if signal should continue process by default
 * @param signal Signal number
 * @return true if signal continues by default, false otherwise
 */
bool signal_mask_is_continue_by_default(int signal);

/**
 * Check if signal should be ignored by default
 * @param signal Signal number
 * @return true if signal is ignored by default, false otherwise
 */
bool signal_mask_is_ignored_by_default(int signal);

/* ========================== Signal Mask Statistics ========================== */

/**
 * Signal mask statistics
 */
typedef struct signal_mask_stats {
    uint64_t mask_changes;              /* Number of mask changes */
    uint64_t signals_blocked;           /* Signals blocked by mask */
    uint64_t signals_unblocked;         /* Signals unblocked */
    uint64_t action_changes;            /* Signal action changes */
    uint64_t stack_changes;             /* Signal stack changes */
    uint64_t suspend_operations;        /* sigsuspend operations */
    uint64_t invalid_operations;        /* Invalid operations attempted */
} signal_mask_stats_t;

/**
 * Get signal mask statistics for a process
 * @param proc Target process
 * @param stats Buffer for statistics
 * @return 0 on success, -1 on error
 */
int signal_mask_get_stats(process_t* proc, signal_mask_stats_t* stats);

/**
 * Reset signal mask statistics for a process
 * @param proc Target process
 * @return 0 on success, -1 on error
 */
int signal_mask_reset_stats(process_t* proc);

/* ========================== Debug and Tracing ========================== */

/**
 * Print signal set (for debugging)
 * @param set Signal set to print
 * @param name Name/description of the set
 */
void signal_mask_print_set(const sigset_t* set, const char* name);

/**
 * Print signal action (for debugging)
 * @param act Signal action to print
 * @param signal Signal number
 */
void signal_mask_print_action(const struct sigaction* act, int signal);

/**
 * Print process signal state (for debugging)
 * @param proc Target process
 */
void signal_mask_print_process_state(process_t* proc);

/**
 * Validate signal mask state consistency
 * @param proc Target process
 * @return true if consistent, false if corrupted
 */
bool signal_mask_validate_state(process_t* proc);

#endif /* SIGNAL_MASK_H */
