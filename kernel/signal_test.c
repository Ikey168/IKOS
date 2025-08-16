/* IKOS Signal Handling Test Suite - Issue #19
 * Comprehensive testing for advanced signal handling system
 */

#include "signal_delivery.h"
#include "signal_mask.h"
#include "signal_syscalls.h"
#include "process.h"
#include "memory.h"
#include "string.h"
#include "assert.h"

/* ========================== Test Framework ========================== */

#define TEST_START(name) \
    do { \
        KLOG_INFO("=== Starting test: %s ===", name); \
        test_count++; \
    } while(0)

#define TEST_END(name) \
    do { \
        KLOG_INFO("=== Completed test: %s ===", name); \
    } while(0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            KLOG_ERROR("TEST FAILED: %s - %s", message, #condition); \
            test_failures++; \
            return; \
        } else { \
            KLOG_DEBUG("TEST PASSED: %s", message); \
        } \
    } while(0)

/* Test statistics */
static int test_count = 0;
static int test_failures = 0;

/* ========================== Test Helper Functions ========================== */

/**
 * Create a test process for signal testing
 */
static process_t* create_test_process(const char* name) {
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) {
        return NULL;
    }
    
    memset(proc, 0, sizeof(process_t));
    proc->pid = 1000 + test_count; /* Unique test PID */
    proc->state = PROCESS_RUNNING;
    proc->uid = 1000;
    proc->gid = 1000;
    
    /* Initialize signal delivery state */
    if (signal_delivery_init_process(proc) != 0) {
        kfree(proc);
        return NULL;
    }
    
    /* Initialize signal mask state */
    if (signal_mask_init_process(proc) != 0) {
        signal_delivery_cleanup_process(proc);
        kfree(proc);
        return NULL;
    }
    
    return proc;
}

/**
 * Cleanup test process
 */
static void cleanup_test_process(process_t* proc) {
    if (!proc) {
        return;
    }
    
    signal_delivery_cleanup_process(proc);
    signal_mask_cleanup_process(proc);
    kfree(proc);
}

/* ========================== Signal Set Tests ========================== */

/**
 * Test signal set operations
 */
static void test_signal_set_operations(void) {
    TEST_START("Signal Set Operations");
    
    sigset_t set1, set2, result;
    
    /* Test empty set */
    TEST_ASSERT(sigemptyset(&set1) == 0, "sigemptyset succeeded");
    TEST_ASSERT(sigset_is_empty(&set1), "Set is empty");
    TEST_ASSERT(sigset_count(&set1) == 0, "Empty set count is 0");
    
    /* Test adding signals */
    TEST_ASSERT(sigaddset(&set1, SIGINT) == 0, "Added SIGINT");
    TEST_ASSERT(sigaddset(&set1, SIGTERM) == 0, "Added SIGTERM");
    TEST_ASSERT(sigismember(&set1, SIGINT) == 1, "SIGINT is member");
    TEST_ASSERT(sigismember(&set1, SIGTERM) == 1, "SIGTERM is member");
    TEST_ASSERT(sigismember(&set1, SIGHUP) == 0, "SIGHUP is not member");
    TEST_ASSERT(sigset_count(&set1) == 2, "Set count is 2");
    
    /* Test removing signals */
    TEST_ASSERT(sigdelset(&set1, SIGINT) == 0, "Removed SIGINT");
    TEST_ASSERT(sigismember(&set1, SIGINT) == 0, "SIGINT is not member");
    TEST_ASSERT(sigismember(&set1, SIGTERM) == 1, "SIGTERM is still member");
    TEST_ASSERT(sigset_count(&set1) == 1, "Set count is 1");
    
    /* Test full set */
    TEST_ASSERT(sigfillset(&set2) == 0, "sigfillset succeeded");
    TEST_ASSERT(!sigset_is_empty(&set2), "Full set is not empty");
    TEST_ASSERT(sigismember(&set2, SIGINT) == 1, "SIGINT in full set");
    TEST_ASSERT(sigismember(&set2, SIGKILL) == 1, "SIGKILL in full set");
    
    /* Test set operations */
    sigemptyset(&set1);
    sigaddset(&set1, SIGINT);
    sigaddset(&set1, SIGTERM);
    
    sigemptyset(&set2);
    sigaddset(&set2, SIGTERM);
    sigaddset(&set2, SIGHUP);
    
    /* Test OR operation */
    TEST_ASSERT(sigset_or(&result, &set1, &set2) == 0, "sigset_or succeeded");
    TEST_ASSERT(sigismember(&result, SIGINT) == 1, "SIGINT in OR result");
    TEST_ASSERT(sigismember(&result, SIGTERM) == 1, "SIGTERM in OR result");
    TEST_ASSERT(sigismember(&result, SIGHUP) == 1, "SIGHUP in OR result");
    TEST_ASSERT(sigset_count(&result) == 3, "OR result count is 3");
    
    /* Test AND operation */
    TEST_ASSERT(sigset_and(&result, &set1, &set2) == 0, "sigset_and succeeded");
    TEST_ASSERT(sigismember(&result, SIGTERM) == 1, "SIGTERM in AND result");
    TEST_ASSERT(sigismember(&result, SIGINT) == 0, "SIGINT not in AND result");
    TEST_ASSERT(sigismember(&result, SIGHUP) == 0, "SIGHUP not in AND result");
    TEST_ASSERT(sigset_count(&result) == 1, "AND result count is 1");
    
    TEST_END("Signal Set Operations");
}

/* ========================== Signal Delivery Tests ========================== */

/**
 * Test basic signal generation and delivery
 */
static void test_signal_generation_and_delivery(void) {
    TEST_START("Signal Generation and Delivery");
    
    process_t* proc = create_test_process("test_signal_delivery");
    TEST_ASSERT(proc != NULL, "Test process created");
    
    if (proc) {
        /* Test signal generation */
        siginfo_t info;
        signal_init_info(&info, SIGTERM, SIGNAL_SOURCE_PROCESS);
        signal_set_sender_info(&info, 1, 0); /* From init */
        
        TEST_ASSERT(signal_generate(proc, SIGTERM, &info, SIGNAL_SOURCE_PROCESS, 0) == 0,
                   "Signal generated successfully");
        
        /* Check if signal is pending */
        TEST_ASSERT(signal_is_pending(proc, SIGTERM), "SIGTERM is pending");
        
        /* Test signal delivery */
        int delivered = signal_deliver_pending(proc);
        TEST_ASSERT(delivered >= 0, "Signal delivery completed");
        
        cleanup_test_process(proc);
    }
    
    TEST_END("Signal Generation and Delivery");
}

/**
 * Test signal masking and blocking
 */
static void test_signal_masking(void) {
    TEST_START("Signal Masking");
    
    process_t* proc = create_test_process("test_signal_masking");
    TEST_ASSERT(proc != NULL, "Test process created");
    
    if (proc) {
        sigset_t mask, oldmask;
        
        /* Test blocking signals */
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGTERM);
        
        TEST_ASSERT(signal_mask_change(proc, SIG_BLOCK, &mask, &oldmask) == 0,
                   "Blocked SIGINT and SIGTERM");
        
        /* Check if signals are blocked */
        TEST_ASSERT(signal_mask_is_blocked(proc, SIGINT), "SIGINT is blocked");
        TEST_ASSERT(signal_mask_is_blocked(proc, SIGTERM), "SIGTERM is blocked");
        TEST_ASSERT(!signal_mask_is_blocked(proc, SIGHUP), "SIGHUP is not blocked");
        
        /* Test unblocking signals */
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        
        TEST_ASSERT(signal_mask_change(proc, SIG_UNBLOCK, &mask, NULL) == 0,
                   "Unblocked SIGINT");
        
        TEST_ASSERT(!signal_mask_is_blocked(proc, SIGINT), "SIGINT is unblocked");
        TEST_ASSERT(signal_mask_is_blocked(proc, SIGTERM), "SIGTERM still blocked");
        
        /* Test setting mask */
        sigemptyset(&mask);
        sigaddset(&mask, SIGHUP);
        
        TEST_ASSERT(signal_mask_change(proc, SIG_SETMASK, &mask, NULL) == 0,
                   "Set new mask");
        
        TEST_ASSERT(signal_mask_is_blocked(proc, SIGHUP), "SIGHUP is blocked");
        TEST_ASSERT(!signal_mask_is_blocked(proc, SIGTERM), "SIGTERM is unblocked");
        
        cleanup_test_process(proc);
    }
    
    TEST_END("Signal Masking");
}

/**
 * Test signal actions and handlers
 */
static void test_signal_actions(void) {
    TEST_START("Signal Actions");
    
    process_t* proc = create_test_process("test_signal_actions");
    TEST_ASSERT(proc != NULL, "Test process created");
    
    if (proc) {
        struct sigaction act, oldact;
        
        /* Test getting default action */
        TEST_ASSERT(signal_action_get(proc, SIGINT, &act) == 0, "Got default SIGINT action");
        TEST_ASSERT(act.sa_handler == SIG_DFL, "Default action is SIG_DFL");
        
        /* Test setting custom handler */
        memset(&act, 0, sizeof(act));
        act.sa_handler = (signal_handler_t)0x12345678; /* Fake handler address */
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_RESTART;
        
        TEST_ASSERT(signal_action_set(proc, SIGINT, &act, &oldact) == 0,
                   "Set custom SIGINT handler");
        TEST_ASSERT(oldact.sa_handler == SIG_DFL, "Old action was SIG_DFL");
        
        /* Verify new action */
        TEST_ASSERT(signal_action_get(proc, SIGINT, &act) == 0, "Got new SIGINT action");
        TEST_ASSERT(act.sa_handler == (signal_handler_t)0x12345678, "Handler set correctly");
        TEST_ASSERT(act.sa_flags == SA_RESTART, "Flags set correctly");
        
        /* Test setting SIG_IGN */
        act.sa_handler = SIG_IGN;
        TEST_ASSERT(signal_action_set(proc, SIGTERM, &act, NULL) == 0,
                   "Set SIGTERM to SIG_IGN");
        TEST_ASSERT(signal_is_ignored(proc, SIGTERM), "SIGTERM is ignored");
        
        /* Test that SIGKILL cannot be changed */
        TEST_ASSERT(signal_action_set(proc, SIGKILL, &act, NULL) != 0,
                   "Cannot change SIGKILL action");
        
        cleanup_test_process(proc);
    }
    
    TEST_END("Signal Actions");
}

/**
 * Test real-time signals
 */
static void test_realtime_signals(void) {
    TEST_START("Real-Time Signals");
    
    process_t* proc = create_test_process("test_rt_signals");
    TEST_ASSERT(proc != NULL, "Test process created");
    
    if (proc) {
        /* Test RT signal properties */
        TEST_ASSERT(signal_is_realtime(32), "Signal 32 is RT signal");
        TEST_ASSERT(signal_is_realtime(63), "Signal 63 is RT signal");
        TEST_ASSERT(!signal_is_realtime(31), "Signal 31 is not RT signal");
        TEST_ASSERT(!signal_is_realtime(1), "Signal 1 is not RT signal");
        
        /* Test RT signal generation */
        siginfo_t info;
        signal_init_info(&info, 32, SIGNAL_SOURCE_PROCESS);
        info.si_value.sival_int = 42;
        
        TEST_ASSERT(signal_generate(proc, 32, &info, SIGNAL_SOURCE_PROCESS, 
                                   SIGNAL_DELIVER_QUEUE) == 0,
                   "RT signal generated");
        
        /* Generate multiple RT signals (should queue) */
        for (int i = 0; i < 5; i++) {
            info.si_value.sival_int = 100 + i;
            signal_generate(proc, 32, &info, SIGNAL_SOURCE_PROCESS, SIGNAL_DELIVER_QUEUE);
        }
        
        TEST_ASSERT(signal_is_pending(proc, 32), "RT signal is pending");
        
        cleanup_test_process(proc);
    }
    
    TEST_END("Real-Time Signals");
}

/**
 * Test signal priorities
 */
static void test_signal_priorities(void) {
    TEST_START("Signal Priorities");
    
    /* Test signal priority levels */
    TEST_ASSERT(signal_get_priority(SIGKILL) == SIGNAL_PRIORITY_CRITICAL,
               "SIGKILL has critical priority");
    TEST_ASSERT(signal_get_priority(SIGSTOP) == SIGNAL_PRIORITY_CRITICAL,
               "SIGSTOP has critical priority");
    TEST_ASSERT(signal_get_priority(SIGSEGV) == SIGNAL_PRIORITY_HIGH,
               "SIGSEGV has high priority");
    TEST_ASSERT(signal_get_priority(SIGCHLD) == SIGNAL_PRIORITY_LOW,
               "SIGCHLD has low priority");
    
    /* Test RT signal priorities */
    TEST_ASSERT(signal_get_priority(32) >= SIGNAL_PRIORITY_RT_BASE,
               "RT signal has RT priority");
    TEST_ASSERT(signal_get_priority(63) >= SIGNAL_PRIORITY_RT_BASE,
               "RT signal has RT priority");
    
    /* Test priority comparison */
    TEST_ASSERT(signal_compare_priority(SIGKILL, SIGTERM) < 0,
               "SIGKILL has higher priority than SIGTERM");
    TEST_ASSERT(signal_compare_priority(SIGCHLD, SIGSEGV) > 0,
               "SIGCHLD has lower priority than SIGSEGV");
    
    TEST_END("Signal Priorities");
}

/**
 * Test signal coalescing
 */
static void test_signal_coalescing(void) {
    TEST_START("Signal Coalescing");
    
    process_t* proc = create_test_process("test_signal_coalescing");
    TEST_ASSERT(proc != NULL, "Test process created");
    
    if (proc) {
        /* Test coalescable signals */
        TEST_ASSERT(signal_can_coalesce(SIGINT), "SIGINT can be coalesced");
        TEST_ASSERT(signal_can_coalesce(SIGTERM), "SIGTERM can be coalesced");
        TEST_ASSERT(signal_can_coalesce(SIGCHLD), "SIGCHLD can be coalesced");
        
        /* RT signals should not coalesce */
        TEST_ASSERT(!signal_can_coalesce(32), "RT signals don't coalesce");
        
        /* Generate multiple coalescable signals */
        siginfo_t info;
        signal_init_info(&info, SIGTERM, SIGNAL_SOURCE_PROCESS);
        
        /* Block SIGTERM to prevent immediate delivery */
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGTERM);
        signal_mask_change(proc, SIG_BLOCK, &mask, NULL);
        
        /* Generate multiple SIGTERM signals */
        for (int i = 0; i < 3; i++) {
            signal_generate(proc, SIGTERM, &info, SIGNAL_SOURCE_PROCESS, 
                           SIGNAL_DELIVER_COALESCE);
        }
        
        TEST_ASSERT(signal_is_pending(proc, SIGTERM), "SIGTERM is pending");
        
        cleanup_test_process(proc);
    }
    
    TEST_END("Signal Coalescing");
}

/* ========================== Main Test Function ========================== */

/**
 * Run all signal handling tests
 */
void signal_test_run_all(void) {
    KLOG_INFO("=== Starting Advanced Signal Handling Test Suite ===");
    
    test_count = 0;
    test_failures = 0;
    
    /* Initialize signal subsystems */
    if (signal_delivery_init() != 0) {
        KLOG_ERROR("Failed to initialize signal delivery system");
        return;
    }
    
    /* Run test suites */
    test_signal_set_operations();
    test_signal_generation_and_delivery();
    test_signal_masking();
    test_signal_actions();
    test_realtime_signals();
    test_signal_priorities();
    test_signal_coalescing();
    
    /* Print summary */
    KLOG_INFO("=== Signal Handling Test Suite Complete ===");
    KLOG_INFO("Tests run: %d", test_count);
    KLOG_INFO("Failures: %d", test_failures);
    
    if (test_failures == 0) {
        KLOG_INFO("🎉 ALL SIGNAL HANDLING TESTS PASSED! 🎉");
    } else {
        KLOG_ERROR("❌ %d SIGNAL HANDLING TESTS FAILED", test_failures);
    }
    
    /* Cleanup */
    signal_delivery_shutdown();
}

/**
 * Test signal system call interface
 */
void signal_test_syscalls(void) {
    KLOG_INFO("=== Testing Signal System Calls ===");
    
    /* This would test the system call interface */
    /* For now, just log that syscall testing is available */
    KLOG_INFO("Signal system call interface ready for testing");
    KLOG_INFO("Available syscalls: signal, sigaction, kill, sigprocmask, etc.");
}
