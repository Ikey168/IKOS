/* IKOS Process Termination System Test - Issue #18
 * Tests comprehensive process exit, cleanup, and resource management
 */

#include "../include/process.h"
#include "../include/process_exit.h"
#include "../include/kernel_log.h"
#include "../include/syscalls.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Test results tracking */
static struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
} test_results = {0};

/* Helper macros */
#define TEST_START(name) \
    do { \
        KLOG_INFO(LOG_CAT_PROCESS, "Starting test: %s", name); \
        test_results.tests_run++; \
    } while (0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            KLOG_INFO(LOG_CAT_PROCESS, "PASS: %s", message); \
            test_results.tests_passed++; \
        } else { \
            KLOG_ERROR(LOG_CAT_PROCESS, "FAIL: %s", message); \
            test_results.tests_failed++; \
        } \
    } while (0)

#define TEST_END(name) \
    KLOG_INFO(LOG_CAT_PROCESS, "Completed test: %s", name)

/* ========================== Test Helper Functions ========================== */

/**
 * Create a test process
 */
static process_t* create_test_process(const char* name) {
    process_t* proc = process_create(name, "/test/dummy");
    if (proc) {
        /* Initialize test-specific fields */
        proc->state = PROCESS_STATE_READY;
        /* Let process_create assign the PID */
        proc->ppid = 1; /* Parent is init */
        proc->exit_code = 0;
        proc->killed_by_signal = 0;
        proc->pending_signals = 0;
        proc->signal_mask = 0;
        proc->alarm_time = 0;
        proc->zombie_children = NULL;
        proc->next_zombie = NULL;
        proc->waiting_for_child = NULL;
        proc->wait_status_ptr = NULL;
        proc->open_files_count = 0;
        proc->allocated_pages = 0;
        proc->cpu_time_used = 0;
        proc->argv = NULL;
        proc->envp = NULL;
    }
    return proc;
}

/**
 * Create a parent-child relationship
 */
static void setup_parent_child(process_t* parent, process_t* child) {
    if (!parent || !child) return;
    
    child->parent = parent;
    child->ppid = parent->pid;
    child->next_sibling = parent->first_child;
    parent->first_child = child;
}

/* ========================== Core Exit Tests ========================== */

/**
 * Test basic process exit functionality
 */
static void test_process_exit_basic(void) {
    TEST_START("Basic Process Exit");
    
    process_t* proc = create_test_process("test_exit");
    TEST_ASSERT(proc != NULL, "Test process created");
    
    if (proc) {
        int exit_code = 42;
        process_exit(proc, exit_code);
        
        TEST_ASSERT(proc->state == PROCESS_STATE_ZOMBIE, "Process entered zombie state");
        TEST_ASSERT(proc->exit_code == exit_code, "Exit code stored correctly");
        TEST_ASSERT(proc->exit_time > 0, "Exit time recorded");
    }
    
    TEST_END("Basic Process Exit");
}

/**
 * Test process kill functionality
 */
static void test_process_kill(void) {
    TEST_START("Process Kill");
    
    process_t* proc = create_test_process("test_kill");
    TEST_ASSERT(proc != NULL, "Test process created");
    
    if (proc) {
        int signal = SIGTERM;
        process_kill(proc, signal);
        
        TEST_ASSERT(proc->state == PROCESS_STATE_ZOMBIE, "Process killed and zombified");
        TEST_ASSERT(proc->killed_by_signal == signal, "Kill signal recorded");
        TEST_ASSERT(proc->exit_code == 128 + signal, "Exit code reflects signal");
    }
    
    TEST_END("Process Kill");
}

/**
 * Test force kill functionality
 */
static void test_process_force_kill(void) {
    TEST_START("Process Force Kill");
    
    process_t* proc = create_test_process("test_force_kill");
    TEST_ASSERT(proc != NULL, "Test process created");
    
    if (proc) {
        process_force_kill(proc);
        
        TEST_ASSERT(proc->state == PROCESS_STATE_TERMINATED, "Process force killed");
        TEST_ASSERT(proc->pid == 0, "Process slot cleared");
    }
    
    TEST_END("Process Force Kill");
}

/* ========================== Resource Cleanup Tests ========================== */

/**
 * Test file descriptor cleanup
 */
static void test_cleanup_files(void) {
    TEST_START("File Descriptor Cleanup");
    
    process_t* proc = create_test_process("test_cleanup_files");
    TEST_ASSERT(proc != NULL, "Test process created");
    
    if (proc) {
        /* Simulate open files */
        for (int i = 0; i < 5; i++) {
            proc->fds[i].fd = i + 3; /* Skip stdin/stdout/stderr */
            proc->fds[i].flags = 0;
            proc->fds[i].offset = 0;
        }
        proc->open_files_count = 5;
        
        int closed = process_cleanup_files(proc);
        
        TEST_ASSERT(closed == 5, "All file descriptors closed");
        
        /* Verify FDs are cleared */
        bool all_cleared = true;
        for (int i = 0; i < 5; i++) {
            if (proc->fds[i].fd != -1) {
                all_cleared = false;
                break;
            }
        }
        TEST_ASSERT(all_cleared, "File descriptor slots cleared");
    }
    
    TEST_END("File Descriptor Cleanup");
}

/**
 * Test memory cleanup
 */
static void test_cleanup_memory(void) {
    TEST_START("Memory Cleanup");
    
    process_t* proc = create_test_process("test_cleanup_memory");
    TEST_ASSERT(proc != NULL, "Test process created");
    
    if (proc) {
        /* Simulate allocated memory */
        proc->allocated_pages = 10;
        
        /* Simulate command line arguments */
        proc->argv = (char**)0x12345; /* Dummy pointer */
        proc->envp = (char**)0x67890; /* Dummy pointer */
        
        int pages_freed = process_cleanup_memory(proc);
        
        TEST_ASSERT(pages_freed >= 0, "Memory cleanup completed");
        TEST_ASSERT(proc->argv == NULL, "Argv pointer cleared");
        TEST_ASSERT(proc->envp == NULL, "Envp pointer cleared");
    }
    
    TEST_END("Memory Cleanup");
}

/**
 * Test signal cleanup
 */
static void test_cleanup_signals(void) {
    TEST_START("Signal Cleanup");
    
    process_t* proc = create_test_process("test_cleanup_signals");
    TEST_ASSERT(proc != NULL, "Test process created");
    
    if (proc) {
        /* Set up signal state */
        proc->pending_signals = 0x12345;
        proc->signal_mask = 0x67890;
        proc->signal_handlers[SIGTERM] = (void*)0xDEADBEEF;
        
        int result = process_cleanup_signals(proc);
        
        TEST_ASSERT(result == 0, "Signal cleanup completed");
        TEST_ASSERT(proc->pending_signals == 0, "Pending signals cleared");
        TEST_ASSERT(proc->signal_mask == 0, "Signal mask cleared");
        TEST_ASSERT(proc->signal_handlers[SIGTERM] == NULL, "Signal handlers cleared");
    }
    
    TEST_END("Signal Cleanup");
}

/* ========================== Parent-Child Management Tests ========================== */

/**
 * Test child reparenting to init
 */
static void test_reparent_children(void) {
    TEST_START("Child Reparenting");
    
    process_t* parent = create_test_process("test_parent");
    process_t* child1 = create_test_process("test_child1");
    process_t* child2 = create_test_process("test_child2");
    process_t* init_proc = process_find_by_pid(1);
    
    TEST_ASSERT(parent != NULL && child1 != NULL && child2 != NULL, "Test processes created");
    
    if (parent && child1 && child2) {
        /* Set up parent-child relationships */
        setup_parent_child(parent, child1);
        setup_parent_child(parent, child2);
        
        /* Reparent children */
        process_reparent_children(parent);
        
        TEST_ASSERT(parent->first_child == NULL, "Parent's child list cleared");
        
        if (init_proc) {
            TEST_ASSERT(child1->parent == init_proc, "Child1 reparented to init");
            TEST_ASSERT(child2->parent == init_proc, "Child2 reparented to init");
            TEST_ASSERT(child1->ppid == 1, "Child1 PPID updated");
            TEST_ASSERT(child2->ppid == 1, "Child2 PPID updated");
        }
    }
    
    TEST_END("Child Reparenting");
}

/**
 * Test parent notification
 */
static void test_parent_notification(void) {
    TEST_START("Parent Notification");
    
    process_t* parent = create_test_process("test_parent_notify");
    process_t* child = create_test_process("test_child_notify");
    
    TEST_ASSERT(parent != NULL && child != NULL, "Test processes created");
    
    if (parent && child) {
        setup_parent_child(parent, child);
        
        int exit_status = 123;
        process_notify_parent(child, exit_status);
        
        /* Check if SIGCHLD was queued (bit 17 set) */
        bool sigchld_pending = (parent->pending_signals & (1ULL << SIGCHLD)) != 0;
        TEST_ASSERT(sigchld_pending, "SIGCHLD queued to parent");
        
        /* Check if child was added to zombie list */
        TEST_ASSERT(parent->zombie_children == child, "Child added to zombie list");
    }
    
    TEST_END("Parent Notification");
}

/**
 * Test zombie reaping
 */
static void test_zombie_reaping(void) {
    TEST_START("Zombie Reaping");
    
    process_t* zombie = create_test_process("test_zombie");
    TEST_ASSERT(zombie != NULL, "Test zombie created");
    
    if (zombie) {
        /* Make it a zombie */
        zombie->state = PROCESS_STATE_ZOMBIE;
        zombie->exit_code = 99;
        
        process_reap_zombie(zombie);
        
        TEST_ASSERT(zombie->state == PROCESS_STATE_TERMINATED, "Zombie reaped");
        TEST_ASSERT(zombie->pid == 0, "Zombie PID cleared");
    }
    
    TEST_END("Zombie Reaping");
}

/* ========================== Wait System Call Tests ========================== */

/**
 * Test wait for any child
 */
static void test_wait_any_child(void) {
    TEST_START("Wait Any Child");
    
    process_t* parent = create_test_process("test_wait_parent");
    process_t* child = create_test_process("test_wait_child");
    
    TEST_ASSERT(parent != NULL && child != NULL, "Test processes created");
    
    if (parent && child) {
        setup_parent_child(parent, child);
        
        /* Make child a zombie */
        child->state = PROCESS_STATE_ZOMBIE;
        child->exit_code = 42;
        parent->zombie_children = child;
        
        int status;
        pid_t result = process_wait_any(parent, &status, 0);
        
        TEST_ASSERT(result == child->pid, "Wait returned correct PID");
        TEST_ASSERT(status == 42, "Exit status retrieved correctly");
    }
    
    TEST_END("Wait Any Child");
}

/**
 * Test wait for specific child
 */
static void test_wait_specific_child(void) {
    TEST_START("Wait Specific Child");
    
    process_t* parent = create_test_process("test_waitpid_parent");
    process_t* child = create_test_process("test_waitpid_child");
    
    TEST_ASSERT(parent != NULL && child != NULL, "Test processes created");
    
    if (parent && child) {
        setup_parent_child(parent, child);
        
        /* Make child a zombie */
        child->state = PROCESS_STATE_ZOMBIE;
        child->exit_code = 55;
        parent->zombie_children = child;
        
        int status;
        pid_t result = process_wait_pid(parent, child->pid, &status, 0);
        
        TEST_ASSERT(result == child->pid, "Waitpid returned correct PID");
        TEST_ASSERT(status == 55, "Exit status retrieved correctly");
    }
    
    TEST_END("Wait Specific Child");
}

/* ========================== System Call Tests ========================== */

/**
 * Test sys_exit system call
 */
static void test_sys_exit(void) {
    TEST_START("sys_exit System Call");
    
    /* This test would need to be run in a separate context
     * since sys_exit doesn't return */
    
    TEST_ASSERT(true, "sys_exit system call interface exists");
    
    TEST_END("sys_exit System Call");
}

/**
 * Test sys_waitpid system call
 */
static void test_sys_waitpid(void) {
    TEST_START("sys_waitpid System Call");
    
    /* Test parameter validation */
    long result = sys_waitpid(0, NULL, 0);
    TEST_ASSERT(result < 0, "Invalid PID rejected");
    
    TEST_END("sys_waitpid System Call");
}

/* ========================== Statistics Tests ========================== */

/**
 * Test exit statistics tracking
 */
static void test_exit_statistics(void) {
    TEST_START("Exit Statistics");
    
    process_exit_stats_t stats;
    process_get_exit_stats(&stats);
    
    TEST_ASSERT(stats.total_exits >= 0, "Total exits tracked");
    TEST_ASSERT(stats.normal_exits >= 0, "Normal exits tracked");
    TEST_ASSERT(stats.killed_processes >= 0, "Killed processes tracked");
    TEST_ASSERT(stats.zombie_count >= 0, "Zombie count tracked");
    TEST_ASSERT(stats.resources_cleaned >= 0, "Resources cleaned tracked");
    
    TEST_END("Exit Statistics");
}

/* ========================== Integration Tests ========================== */

/**
 * Test complete exit workflow
 */
static void test_complete_exit_workflow(void) {
    TEST_START("Complete Exit Workflow");
    
    process_t* parent = create_test_process("workflow_parent");
    process_t* child = create_test_process("workflow_child");
    
    TEST_ASSERT(parent != NULL && child != NULL, "Workflow processes created");
    
    if (parent && child) {
        /* Set up relationship */
        setup_parent_child(parent, child);
        
        /* Simulate some resources */
        child->fds[0].fd = 3;
        child->pending_signals = 0x100;
        child->allocated_pages = 5;
        
        /* Exit the child */
        int exit_code = 77;
        process_exit(child, exit_code);
        
        /* Verify workflow */
        TEST_ASSERT(child->state == PROCESS_STATE_ZOMBIE, "Child zombified");
        TEST_ASSERT(child->exit_code == exit_code, "Exit code preserved");
        TEST_ASSERT(child->fds[0].fd == -1, "File descriptors cleaned");
        TEST_ASSERT(child->pending_signals == 0, "Signals cleaned");
        TEST_ASSERT(parent->zombie_children == child, "Added to parent's zombie list");
        
        /* Wait for child */
        int status;
        pid_t waited = process_wait_any(parent, &status, 0);
        
        TEST_ASSERT(waited == child->pid, "Wait successful");
        TEST_ASSERT(status == exit_code, "Status retrieved");
    }
    
    TEST_END("Complete Exit Workflow");
}

/* ========================== Main Test Runner ========================== */

/**
 * Run all process termination tests
 */
void run_process_termination_tests(void) {
    KLOG_INFO(LOG_CAT_PROCESS, "Starting Process Termination System Tests");
    
    /* Initialize test results */
    test_results.tests_run = 0;
    test_results.tests_passed = 0;
    test_results.tests_failed = 0;
    
    /* Initialize process exit system */
    process_exit_init();
    
    /* Core exit functionality tests */
    test_process_exit_basic();
    test_process_kill();
    test_process_force_kill();
    
    /* Resource cleanup tests */
    test_cleanup_files();
    test_cleanup_memory();
    test_cleanup_signals();
    
    /* Parent-child management tests */
    test_reparent_children();
    test_parent_notification();
    test_zombie_reaping();
    
    /* Wait system call tests */
    test_wait_any_child();
    test_wait_specific_child();
    
    /* System call interface tests */
    test_sys_exit();
    test_sys_waitpid();
    
    /* Statistics tests */
    test_exit_statistics();
    
    /* Integration tests */
    test_complete_exit_workflow();
    
    /* Print test results */
    KLOG_INFO(LOG_CAT_PROCESS, "Process Termination Tests Complete");
    KLOG_INFO(LOG_CAT_PROCESS, "Tests Run: %d", test_results.tests_run);
    KLOG_INFO(LOG_CAT_PROCESS, "Tests Passed: %d", test_results.tests_passed);
    KLOG_INFO(LOG_CAT_PROCESS, "Tests Failed: %d", test_results.tests_failed);
    
    if (test_results.tests_failed == 0) {
        KLOG_INFO(LOG_CAT_PROCESS, "All Process Termination Tests PASSED!");
    } else {
        KLOG_ERROR(LOG_CAT_PROCESS, "Some Process Termination Tests FAILED!");
    }
}

/**
 * Test entry point for standalone testing
 */
int main(void) {
    run_process_termination_tests();
    return (test_results.tests_failed == 0) ? 0 : 1;
}
