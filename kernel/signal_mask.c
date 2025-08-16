/* IKOS Signal Masking and Control Implementation - Issue #19
 * Signal blocking, unblocking, and mask management
 */

#include <signal_mask.h>
#include <signal_delivery.h>
#include <process.h>
#include <string.h>
#include <errno.h>
#include <kernel.h>
#include <memory.h>

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

/* Define missing signal constants if not already defined */
#ifndef SIGKILL
#define SIGKILL 9
#endif

#ifndef SIGSTOP
#define SIGSTOP 19
#endif

/* Define KLOG macros if not available */
#ifndef KLOG_DEBUG
#define KLOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "
", ##__VA_ARGS__)
#endif

#ifndef KLOG_ERROR
#define KLOG_ERROR(fmt, ...) printf("[ERROR] " fmt "
", ##__VA_ARGS__)
#endif

#ifndef KLOG_INFO
#define KLOG_INFO(fmt, ...) printf("[INFO] " fmt "
", ##__VA_ARGS__)
#endif

/* Define spinlock type if not available */
typedef struct {
    volatile int locked;
} spinlock_t;

/* Define missing stack_t if not available */
#ifndef _SIGNAL_H
typedef struct {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
} stack_t;
#endif

/* ========================== Constants and Globals ========================== */

/* Signals that cannot be blocked */
static const uint64_t g_unblockable_signals = (1ULL << SIGKILL) | (1ULL << SIGSTOP);

/* Signals that are ignored by default */
static const uint64_t g_ignored_by_default = 
    (1ULL << SIGCHLD) | (1ULL << SIGURG) | (1ULL << SIGWINCH);

/* Signals that terminate process by default */
static const uint64_t g_fatal_by_default = 
    (1ULL << SIGHUP) | (1ULL << SIGINT) | (1ULL << SIGQUIT) | (1ULL << SIGILL) |
    (1ULL << SIGABRT) | (1ULL << SIGFPE) | (1ULL << SIGKILL) | (1ULL << SIGSEGV) |
    (1ULL << SIGPIPE) | (1ULL << SIGALRM) | (1ULL << SIGTERM) | (1ULL << SIGUSR1) |
    (1ULL << SIGUSR2) | (1ULL << SIGBUS) | (1ULL << SIGPOLL) | (1ULL << SIGPROF) |
    (1ULL << SIGSYS) | (1ULL << SIGTRAP) | (1ULL << SIGVTALRM) | (1ULL << SIGXCPU) |
    (1ULL << SIGXFSZ);

/* Signals that stop process by default */
static const uint64_t g_stop_by_default = 
    (1ULL << SIGSTOP) | (1ULL << SIGTSTP) | (1ULL << SIGTTIN) | (1ULL << SIGTTOU);

/* Signals that continue process by default */
static const uint64_t g_continue_by_default = (1ULL << SIGCONT);

/* ========================== Signal Set Operations ========================== */

/**
 * Initialize signal set to empty
 */
int sigemptyset(sigset_t* set) {
    if (!set) {
        return -1;
    }
    
    for (int i = 0; i < _SIGSET_NWORDS; i++) {
        set->sig[i] = 0;
    }
    
    return 0;
}

/**
 * Initialize signal set to full
 */
int sigfillset(sigset_t* set) {
    if (!set) {
        return -1;
    }
    
    for (int i = 0; i < _SIGSET_NWORDS; i++) {
        set->sig[i] = ~0ULL;
    }
    
    /* Clear invalid signal bits (signals > _NSIG) */
    if (_NSIG % 64 != 0) {
        int last_word = _SIGSET_NWORDS - 1;
        int valid_bits = _NSIG % 64;
        set->sig[last_word] &= (1ULL << valid_bits) - 1;
    }
    
    return 0;
}

/**
 * Add signal to signal set
 */
int sigaddset(sigset_t* set, int signum) {
    if (!set || !signal_mask_is_valid_signal(signum)) {
        return -1;
    }
    
    int word = (signum - 1) / 64;
    int bit = (signum - 1) % 64;
    
    set->sig[word] |= (1ULL << bit);
    return 0;
}

/**
 * Remove signal from signal set
 */
int sigdelset(sigset_t* set, int signum) {
    if (!set || !signal_mask_is_valid_signal(signum)) {
        return -1;
    }
    
    int word = (signum - 1) / 64;
    int bit = (signum - 1) % 64;
    
    set->sig[word] &= ~(1ULL << bit);
    return 0;
}

/**
 * Test if signal is in signal set
 */
int sigismember(const sigset_t* set, int signum) {
    if (!set || !signal_mask_is_valid_signal(signum)) {
        return -1;
    }
    
    int word = (signum - 1) / 64;
    int bit = (signum - 1) % 64;
    
    return (set->sig[word] & (1ULL << bit)) ? 1 : 0;
}

/**
 * Check if signal set is empty
 */
bool sigset_is_empty(const sigset_t* set) {
    if (!set) {
        return true;
    }
    
    for (int i = 0; i < _SIGSET_NWORDS; i++) {
        if (set->sig[i] != 0) {
            return false;
        }
    }
    
    return true;
}

/**
 * Count number of signals in set
 */
int sigset_count(const sigset_t* set) {
    if (!set) {
        return 0;
    }
    
    int count = 0;
    for (int i = 0; i < _SIGSET_NWORDS; i++) {
        uint64_t word = set->sig[i];
        while (word) {
            count += word & 1;
            word >>= 1;
        }
    }
    
    return count;
}

/**
 * Copy signal set
 */
int sigset_copy(sigset_t* dest, const sigset_t* src) {
    if (!dest || !src) {
        return -1;
    }
    
    for (int i = 0; i < _SIGSET_NWORDS; i++) {
        dest->sig[i] = src->sig[i];
    }
    
    return 0;
}

/**
 * Perform bitwise OR of two signal sets
 */
int sigset_or(sigset_t* dest, const sigset_t* src1, const sigset_t* src2) {
    if (!dest || !src1 || !src2) {
        return -1;
    }
    
    for (int i = 0; i < _SIGSET_NWORDS; i++) {
        dest->sig[i] = src1->sig[i] | src2->sig[i];
    }
    
    return 0;
}

/**
 * Perform bitwise AND of two signal sets
 */
int sigset_and(sigset_t* dest, const sigset_t* src1, const sigset_t* src2) {
    if (!dest || !src1 || !src2) {
        return -1;
    }
    
    for (int i = 0; i < _SIGSET_NWORDS; i++) {
        dest->sig[i] = src1->sig[i] & src2->sig[i];
    }
    
    return 0;
}

/**
 * Convert signal set to bitmask (first 64 signals)
 */
uint64_t sigset_to_mask(const sigset_t* set) {
    if (!set) {
        return 0;
    }
    
    return set->sig[0];
}

/**
 * Convert bitmask to signal set
 */
int mask_to_sigset(sigset_t* set, uint64_t mask) {
    if (!set) {
        return -1;
    }
    
    sigemptyset(set);
    set->sig[0] = mask;
    
    return 0;
}

/* ========================== Signal Masking Functions ========================== */

/**
 * Initialize signal masking state for a process
 */
int signal_mask_init_process(process_t* proc) {
    if (!proc) {
        return -1;
    }
    
    /* Allocate signal mask state */
    signal_mask_state_t* state = (signal_mask_state_t*)kmalloc(sizeof(signal_mask_state_t));
    if (!state) {
        return -1;
    }
    
    memset(state, 0, sizeof(signal_mask_state_t));
    
    /* Initialize empty signal mask */
    sigemptyset(&state->signal_mask);
    sigemptyset(&state->saved_mask);
    
    /* Initialize default signal actions */
    for (int i = 1; i < _NSIG; i++) {
        struct sigaction* action = &state->actions[i];
        memset(action, 0, sizeof(struct sigaction));
        
        action->sa_handler = signal_mask_get_default_action(i);
        sigemptyset(&action->sa_mask);
        action->sa_flags = 0;
    }
    
    /* Initialize signal stack to disabled */
    state->signal_stack.ss_sp = NULL;
    state->signal_stack.ss_size = 0;
    state->signal_stack.ss_flags = SS_DISABLE;
    
    /* Initialize lock */
    spinlock_init(&state->mask_lock);
    
    /* Attach to process */
    proc->signal_mask_state = state;
    
    KLOG_DEBUG("Signal mask state initialized for process %u", proc->pid);
    return 0;
}

/**
 * Cleanup signal masking state for a process
 */
void signal_mask_cleanup_process(process_t* proc) {
    if (!proc || !proc->signal_mask_state) {
        return;
    }
    
    signal_mask_state_t* state = (signal_mask_state_t*)proc->signal_mask_state;
    
    /* Free signal stack if allocated */
    if (state->signal_stack.ss_sp && !(state->signal_stack.ss_flags & SS_DISABLE)) {
        signal_stack_free(state->signal_stack.ss_sp, state->signal_stack.ss_size);
    }
    
    /* Free state structure */
    kfree(state);
    proc->signal_mask_state = NULL;
    
    KLOG_DEBUG("Signal mask state cleaned up for process %u", proc->pid);
}

/**
 * Change process signal mask
 */
int signal_mask_change(process_t* proc, int how, const sigset_t* set, sigset_t* oldset) {
    if (!proc || !proc->signal_mask_state) {
        return -1;
    }
    
    signal_mask_state_t* state = (signal_mask_state_t*)proc->signal_mask_state;
    
    spin_lock(&state->mask_lock);
    
    /* Save old mask if requested */
    if (oldset) {
        sigset_copy(oldset, &state->signal_mask);
    }
    
    /* Apply mask change based on how */
    switch (how) {
        case SIG_BLOCK:
            if (set) {
                sigset_t temp_mask;
                sigset_or(&temp_mask, &state->signal_mask, set);
                sigset_copy(&state->signal_mask, &temp_mask);
            }
            break;
            
        case SIG_UNBLOCK:
            if (set) {
                sigset_t temp_mask;
                sigset_not(&temp_mask, set);
                sigset_and(&state->signal_mask, &state->signal_mask, &temp_mask);
            }
            break;
            
        case SIG_SETMASK:
            if (set) {
                sigset_copy(&state->signal_mask, set);
            } else {
                sigemptyset(&state->signal_mask);
            }
            break;
            
        default:
            spin_unlock(&state->mask_lock);
            return -1;
    }
    
    /* Remove unblockable signals from mask */
    for (int i = 1; i < 64; i++) {
        if (!signal_mask_is_blockable(i)) {
            sigdelset(&state->signal_mask, i);
        }
    }
    
    state->mask_change_count++;
    spin_unlock(&state->mask_lock);
    
    /* Check for newly unblocked pending signals */
    if (how == SIG_UNBLOCK || how == SIG_SETMASK) {
        signal_deliver_pending(proc);
    }
    
    return 0;
}

/**
 * Get process signal mask
 */
int signal_mask_get(process_t* proc, sigset_t* mask) {
    if (!proc || !proc->signal_mask_state || !mask) {
        return -1;
    }
    
    signal_mask_state_t* state = (signal_mask_state_t*)proc->signal_mask_state;
    
    spin_lock(&state->mask_lock);
    sigset_copy(mask, &state->signal_mask);
    spin_unlock(&state->mask_lock);
    
    return 0;
}

/**
 * Check if a specific signal is blocked
 */
bool signal_mask_is_blocked(process_t* proc, int signal) {
    if (!proc || !proc->signal_mask_state || !signal_mask_is_valid_signal(signal)) {
        return false;
    }
    
    /* Unblockable signals are never blocked */
    if (!signal_mask_is_blockable(signal)) {
        return false;
    }
    
    signal_mask_state_t* state = (signal_mask_state_t*)proc->signal_mask_state;
    
    spin_lock(&state->mask_lock);
    bool blocked = (sigismember(&state->signal_mask, signal) == 1);
    spin_unlock(&state->mask_lock);
    
    return blocked;
}

/**
 * Temporarily change signal mask (for sigsuspend)
 */
int signal_mask_suspend(process_t* proc, const sigset_t* mask) {
    if (!proc || !proc->signal_mask_state) {
        return -1;
    }
    
    signal_mask_state_t* state = (signal_mask_state_t*)proc->signal_mask_state;
    
    spin_lock(&state->mask_lock);
    
    /* Save current mask */
    sigset_copy(&state->saved_mask, &state->signal_mask);
    state->mask_suspended = true;
    
    /* Set new mask */
    if (mask) {
        sigset_copy(&state->signal_mask, mask);
    } else {
        sigemptyset(&state->signal_mask);
    }
    
    /* Remove unblockable signals */
    for (int i = 1; i < 64; i++) {
        if (!signal_mask_is_blockable(i)) {
            sigdelset(&state->signal_mask, i);
        }
    }
    
    spin_unlock(&state->mask_lock);
    
    return 0;
}

/**
 * Restore signal mask after suspension
 */
int signal_mask_restore(process_t* proc) {
    if (!proc || !proc->signal_mask_state) {
        return -1;
    }
    
    signal_mask_state_t* state = (signal_mask_state_t*)proc->signal_mask_state;
    
    spin_lock(&state->mask_lock);
    
    if (state->mask_suspended) {
        /* Restore saved mask */
        sigset_copy(&state->signal_mask, &state->saved_mask);
        state->mask_suspended = false;
    }
    
    spin_unlock(&state->mask_lock);
    
    /* Check for newly unblocked pending signals */
    signal_deliver_pending(proc);
    
    return 0;
}

/* ========================== Signal Action Management ========================== */

/**
 * Set signal action for a process
 */
int signal_action_set(process_t* proc, int signal, const struct sigaction* act, 
                     struct sigaction* oldact) {
    if (!proc || !proc->signal_mask_state || !signal_mask_is_valid_signal(signal)) {
        return -1;
    }
    
    /* Cannot change action for SIGKILL and SIGSTOP */
    if (signal == SIGKILL || signal == SIGSTOP) {
        return -1;
    }
    
    signal_mask_state_t* state = (signal_mask_state_t*)proc->signal_mask_state;
    
    spin_lock(&state->mask_lock);
    
    /* Save old action if requested */
    if (oldact) {
        *oldact = state->actions[signal];
    }
    
    /* Set new action */
    if (act) {
        state->actions[signal] = *act;
        
        /* Ensure unblockable signals are not in sa_mask */
        for (int i = 1; i < 64; i++) {
            if (!signal_mask_is_blockable(i)) {
                sigdelset(&state->actions[signal].sa_mask, i);
            }
        }
    } else {
        /* Reset to default */
        memset(&state->actions[signal], 0, sizeof(struct sigaction));
        state->actions[signal].sa_handler = signal_mask_get_default_action(signal);
    }
    
    spin_unlock(&state->mask_lock);
    
    return 0;
}

/**
 * Get signal action for a process
 */
int signal_action_get(process_t* proc, int signal, struct sigaction* act) {
    if (!proc || !proc->signal_mask_state || !signal_mask_is_valid_signal(signal) || !act) {
        return -1;
    }
    
    signal_mask_state_t* state = (signal_mask_state_t*)proc->signal_mask_state;
    
    spin_lock(&state->mask_lock);
    *act = state->actions[signal];
    spin_unlock(&state->mask_lock);
    
    return 0;
}

/**
 * Set simple signal handler
 */
signal_handler_t signal_handler_set(process_t* proc, int signal, signal_handler_t handler) {
    if (!proc || !proc->signal_mask_state || !signal_mask_is_valid_signal(signal)) {
        return SIG_ERR;
    }
    
    signal_mask_state_t* state = (signal_mask_state_t*)proc->signal_mask_state;
    
    spin_lock(&state->mask_lock);
    
    signal_handler_t old_handler = state->actions[signal].sa_handler;
    state->actions[signal].sa_handler = handler;
    sigemptyset(&state->actions[signal].sa_mask);
    state->actions[signal].sa_flags = 0;
    
    spin_unlock(&state->mask_lock);
    
    return old_handler;
}

/**
 * Check if signal has custom handler
 */
bool signal_has_custom_handler(process_t* proc, int signal) {
    if (!proc || !proc->signal_mask_state || !signal_mask_is_valid_signal(signal)) {
        return false;
    }
    
    signal_mask_state_t* state = (signal_mask_state_t*)proc->signal_mask_state;
    
    spin_lock(&state->mask_lock);
    signal_handler_t handler = state->actions[signal].sa_handler;
    spin_unlock(&state->mask_lock);
    
    return (handler != SIG_DFL && handler != SIG_IGN);
}

/**
 * Check if signal is ignored
 */
bool signal_is_ignored(process_t* proc, int signal) {
    if (!proc || !proc->signal_mask_state || !signal_mask_is_valid_signal(signal)) {
        return false;
    }
    
    signal_mask_state_t* state = (signal_mask_state_t*)proc->signal_mask_state;
    
    spin_lock(&state->mask_lock);
    signal_handler_t handler = state->actions[signal].sa_handler;
    spin_unlock(&state->mask_lock);
    
    return (handler == SIG_IGN);
}

/* ========================== Signal Stack Management ========================== */

/**
 * Set alternative signal stack
 */
int signal_stack_set(process_t* proc, const stack_t* stack, stack_t* oldstack) {
    if (!proc || !proc->signal_mask_state) {
        return -1;
    }
    
    signal_mask_state_t* state = (signal_mask_state_t*)proc->signal_mask_state;
    
    spin_lock(&state->mask_lock);
    
    /* Save old stack if requested */
    if (oldstack) {
        *oldstack = state->signal_stack;
    }
    
    /* Set new stack */
    if (stack) {
        /* Validate stack parameters */
        if (stack->ss_size < MINSIGSTKSZ && !(stack->ss_flags & SS_DISABLE)) {
            spin_unlock(&state->mask_lock);
            return -1;
        }
        
        /* Free old stack if we allocated it */
        if (state->signal_stack.ss_sp && !(state->signal_stack.ss_flags & SS_DISABLE)) {
            signal_stack_free(state->signal_stack.ss_sp, state->signal_stack.ss_size);
        }
        
        state->signal_stack = *stack;
    }
    
    spin_unlock(&state->mask_lock);
    
    return 0;
}

/**
 * Allocate memory for signal stack
 */
void* signal_stack_alloc(size_t size) {
    if (size < MINSIGSTKSZ) {
        return NULL;
    }
    
    /* Allocate page-aligned memory for signal stack */
    return kmalloc(size);
}

/**
 * Free signal stack memory
 */
void signal_stack_free(void* stack, size_t size) {
    if (stack) {
        kfree(stack);
    }
}

/* ========================== Signal Mask Utilities ========================== */

/**
 * Validate signal number
 */
bool signal_mask_is_valid_signal(int signal) {
    return (signal >= 1 && signal < _NSIG);
}

/**
 * Check if signal can be blocked
 */
bool signal_mask_is_blockable(int signal) {
    if (!signal_mask_is_valid_signal(signal)) {
        return false;
    }
    
    return !(g_unblockable_signals & (1ULL << signal));
}

/**
 * Get default signal action
 */
signal_handler_t signal_mask_get_default_action(int signal) {
    if (!signal_mask_is_valid_signal(signal)) {
        return SIG_DFL;
    }
    
    if (g_ignored_by_default & (1ULL << signal)) {
        return SIG_IGN;
    }
    
    return SIG_DFL;
}

/**
 * Check if signal should terminate process by default
 */
bool signal_mask_is_fatal_by_default(int signal) {
    if (!signal_mask_is_valid_signal(signal)) {
        return false;
    }
    
    return (g_fatal_by_default & (1ULL << signal)) != 0;
}

/**
 * Check if signal should stop process by default
 */
bool signal_mask_is_stop_by_default(int signal) {
    if (!signal_mask_is_valid_signal(signal)) {
        return false;
    }
    
    return (g_stop_by_default & (1ULL << signal)) != 0;
}

/**
 * Check if signal is ignored by default
 */
bool signal_mask_is_ignored_by_default(int signal) {
    if (!signal_mask_is_valid_signal(signal)) {
        return false;
    }
    
    return (g_ignored_by_default & (1ULL << signal)) != 0;
}

/* ========================== Debug Functions ========================== */

/**
 * Print signal set (for debugging)
 */
void signal_mask_print_set(const sigset_t* set, const char* name) {
    if (!set || !name) {
        return;
    }
    
    KLOG_DEBUG("Signal set %s: ", name);
    
    bool first = true;
    for (int i = 1; i < _NSIG; i++) {
        if (sigismember(set, i) == 1) {
            if (!first) {
                KLOG_DEBUG_CONT(", ");
            }
            KLOG_DEBUG_CONT("%d", i);
            first = false;
        }
    }
    
    if (first) {
        KLOG_DEBUG_CONT("(empty)");
    }
    
    KLOG_DEBUG_CONT("\n");
}

/**
 * Validate signal mask state consistency
 */
bool signal_mask_validate_state(process_t* proc) {
    if (!proc || !proc->signal_mask_state) {
        return false;
    }
    
    signal_mask_state_t* state = (signal_mask_state_t*)proc->signal_mask_state;
    
    /* Check that unblockable signals are not in mask */
    for (int i = 1; i < 64; i++) {
        if (!signal_mask_is_blockable(i)) {
            if (sigismember(&state->signal_mask, i) == 1) {
                KLOG_ERROR("Unblockable signal %d found in mask for process %u", i, proc->pid);
                return false;
            }
        }
    }
    
    return true;
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
