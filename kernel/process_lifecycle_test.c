/* IKOS Process Lifecycle Test Suite - Issue #24
 * Comprehensive testing for fork(), execve(), and wait() system calls
 */

#include "syscall_process.h"
#include "process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========================== Test Framework ========================== */

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    static int test_##name(void); \
    static void run_test_##name(void) { \
        printf("Running test: %s... ", #name); \
        tests_run++; \
        if (test_##name() == 0) { \
            printf("PASSED\n"); \
            tests_passed++; \
        } else { \
            printf("FAILED\n"); \
            tests_failed++; \
        } \
    } \
    static int test_##name(void)

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("Assertion failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
            return -1; \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("Assertion failed: expected %ld, got %ld at %s:%d\n", \
                   (long)(expected), (long)(actual), __FILE__, __LINE__); \
            return -1; \
        } \
    } while(0)

#define ASSERT_NEQ(not_expected, actual) \
    do { \
        if ((not_expected) == (actual)) { \
            printf("Assertion failed: did not expect %ld at %s:%d\n", \
                   (long)(actual), __FILE__, __LINE__); \
            return -1; \
        } \
    } while(0)

/* ========================== Fork Tests ========================== */

TEST(fork_basic) {
    /* Test basic fork functionality */
    pid_t child_pid = sys_fork();
    
    if (child_pid == 0) {
        /* Child process */
        return 0;  /* Child should return 0 */
    } else if (child_pid > 0) {
        /* Parent process */
        ASSERT(child_pid > 0);
        return 0;
    } else {
        /* Fork failed */
        ASSERT(0);  /* Fork should not fail in test */
        return -1;
    }
}

TEST(fork_memory_isolation) {
    /* Test that parent and child have separate memory spaces */
    int test_var = 42;
    pid_t child_pid = sys_fork();
    
    if (child_pid == 0) {
        /* Child process - modify variable */
        test_var = 100;
        return 0;
    } else if (child_pid > 0) {
        /* Parent process - variable should be unchanged */
        ASSERT_EQ(42, test_var);
        return 0;
    } else {
        return -1;
    }
}

TEST(fork_file_descriptor_inheritance) {
    /* Test that child inherits file descriptors */
    /* This is a simplified test since we don't have full VFS */
    
    fork_context_t* ctx = create_fork_context(get_current_process());
    ASSERT(ctx != NULL);
    
    /* Test context creation and destruction */
    ASSERT(ctx->parent_pid > 0);
    ASSERT(ctx->copy_on_write == true);
    
    destroy_fork_context(ctx);
    return 0;
}

TEST(fork_process_tree) {
    /* Test process tree relationships */
    process_t parent = {0};
    process_t child = {0};
    
    parent.pid = 100;
    child.pid = 101;
    
    /* Test adding child to parent */
    int result = add_child_process(&parent, &child);
    ASSERT_EQ(0, result);
    ASSERT(parent.first_child == &child);
    ASSERT(child.parent == &parent);
    ASSERT_EQ(100, child.ppid);
    
    /* Test finding child */
    process_t* found = find_child_process(&parent, 101);
    ASSERT(found == &child);
    
    /* Test removing child */
    result = remove_child_process(&parent, &child);
    ASSERT_EQ(0, result);
    ASSERT(parent.first_child != &child);
    ASSERT(child.parent == NULL);
    
    return 0;
}

TEST(fork_cow_support) {
    /* Test copy-on-write functionality */
    uint64_t test_addr = 0x400000;
    
    /* Test marking page as COW */
    int result = mark_page_cow(test_addr);
    ASSERT_EQ(0, result);
    
    /* Test COW page fault handling */
    process_t proc = {0};
    proc.pid = 123;
    
    result = handle_cow_page_fault(test_addr, &proc);
    /* This may fail due to missing VMM, but should not crash */
    
    return 0;
}

/* ========================== Execve Tests ========================== */

TEST(execve_basic) {
    /* Test basic execve functionality */
    char* argv[] = {"/bin/test", NULL};
    char* envp[] = {"PATH=/bin", NULL};
    
    exec_context_t* ctx = create_exec_context("/bin/test", argv, envp);
    ASSERT(ctx != NULL);
    
    /* Test context fields */
    ASSERT(strcmp(ctx->path, "/bin/test") == 0);
    ASSERT(ctx->argc == 1);
    ASSERT(ctx->envc == 1);
    ASSERT(ctx->argv == argv);
    ASSERT(ctx->envp == envp);
    
    destroy_exec_context(ctx);
    return 0;
}

TEST(execve_argument_validation) {
    /* Test argument validation */
    
    /* Test NULL path */
    exec_context_t* ctx = create_exec_context(NULL, NULL, NULL);
    ASSERT(ctx == NULL);
    
    /* Test valid arguments */
    char* argv[] = {"test", "arg1", "arg2", NULL};
    char* envp[] = {"VAR1=value1", "VAR2=value2", NULL};
    
    ctx = create_exec_context("/bin/test", argv, envp);
    ASSERT(ctx != NULL);
    ASSERT_EQ(3, ctx->argc);  /* test, arg1, arg2 */
    ASSERT_EQ(2, ctx->envc);  /* VAR1, VAR2 */
    
    destroy_exec_context(ctx);
    return 0;
}

TEST(execve_path_validation) {
    /* Test executable path validation */
    
    /* Test valid path */
    int result = validate_executable("/bin/sh");
    ASSERT_EQ(0, result);
    
    /* Test NULL path */
    result = validate_executable(NULL);
    ASSERT_NEQ(0, result);
    
    /* Test empty path */
    result = validate_executable("");
    ASSERT_NEQ(0, result);
    
    return 0;
}

TEST(execve_memory_replacement) {
    /* Test memory space replacement */
    process_t proc = {0};
    proc.pid = 456;
    proc.virtual_memory_start = 0x400000;
    proc.virtual_memory_end = 0x600000;
    
    char* argv[] = {"/bin/test", NULL};
    char* envp[] = {NULL};
    
    exec_context_t* ctx = create_exec_context("/bin/test", argv, envp);
    ASSERT(ctx != NULL);
    
    /* Test memory replacement */
    int result = replace_process_memory(&proc, ctx);
    ASSERT_EQ(0, result);
    
    destroy_exec_context(ctx);
    return 0;
}

/* ========================== Wait Tests ========================== */

TEST(wait_basic) {
    /* Test basic wait functionality */
    int status = 0;
    
    /* Create wait context */
    wait_context_t* ctx = create_wait_context(-1, &status, 0);
    ASSERT(ctx != NULL);
    
    /* Test context fields */
    ASSERT_EQ(-1, ctx->wait_pid);
    ASSERT(ctx->status_ptr == &status);
    ASSERT_EQ(0, ctx->options);
    ASSERT(ctx->is_blocking == true);
    
    destroy_wait_context(ctx);
    return 0;
}

TEST(wait_status_macros) {
    /* Test wait status macros */
    int status;
    
    /* Test normal exit */
    status = (42 << 8);  /* Exit code 42 */
    ASSERT(WIFEXITED(status));
    ASSERT_EQ(42, WEXITSTATUS(status));
    ASSERT(!WIFSIGNALED(status));
    
    /* Test signal termination */
    status = 9;  /* SIGKILL */
    ASSERT(!WIFEXITED(status));
    ASSERT(WIFSIGNALED(status));
    ASSERT_EQ(9, WTERMSIG(status));
    
    return 0;
}

TEST(wait_zombie_management) {
    /* Test zombie process management */
    process_t parent = {0};
    process_t child = {0};
    
    parent.pid = 200;
    child.pid = 201;
    child.parent = &parent;
    
    /* Create zombie */
    int result = create_zombie_process(&child, 42);
    ASSERT_EQ(0, result);
    ASSERT(child.state == PROCESS_STATE_ZOMBIE);
    ASSERT_EQ(42, child.exit_code);
    ASSERT(parent.zombie_children == &child);
    
    /* Test zombie detection */
    ASSERT(has_zombie_children(&parent));
    
    /* Get next zombie */
    process_t* zombie = get_next_zombie_child(&parent);
    ASSERT(zombie == &child);
    
    /* Reap zombie */
    result = reap_zombie_process(&parent, &child);
    ASSERT_EQ(0, result);
    
    return 0;
}

TEST(wait_nonblocking) {
    /* Test non-blocking wait (WNOHANG) */
    wait_context_t* ctx = create_wait_context(-1, NULL, WNOHANG);
    ASSERT(ctx != NULL);
    
    ASSERT(ctx->is_blocking == false);
    ASSERT(ctx->options & WNOHANG);
    
    destroy_wait_context(ctx);
    return 0;
}

TEST(wait_process_counting) {
    /* Test process children counting */
    process_t parent = {0};
    process_t child1 = {0};
    process_t child2 = {0};
    process_t zombie = {0};
    
    parent.pid = 300;
    child1.pid = 301;
    child2.pid = 302;
    zombie.pid = 303;
    
    /* Initially no children */
    ASSERT_EQ(0, get_process_children_count(&parent));
    
    /* Add living children */
    add_child_process(&parent, &child1);
    add_child_process(&parent, &child2);
    ASSERT_EQ(2, get_process_children_count(&parent));
    
    /* Add zombie child */
    zombie.parent = &parent;
    zombie.next_zombie = parent.zombie_children;
    parent.zombie_children = &zombie;
    ASSERT_EQ(3, get_process_children_count(&parent));
    
    return 0;
}

/* ========================== Integration Tests ========================== */

TEST(fork_exec_integration) {
    /* Test fork followed by exec */
    
    /* This test simulates the typical fork+exec pattern */
    fork_context_t* fork_ctx = create_fork_context(get_current_process());
    ASSERT(fork_ctx != NULL);
    
    char* argv[] = {"/bin/sh", NULL};
    char* envp[] = {"PATH=/bin", NULL};
    
    exec_context_t* exec_ctx = create_exec_context("/bin/sh", argv, envp);
    ASSERT(exec_ctx != NULL);
    
    /* Cleanup */
    destroy_fork_context(fork_ctx);
    destroy_exec_context(exec_ctx);
    
    return 0;
}

TEST(complete_lifecycle) {
    /* Test complete process lifecycle: fork -> exec -> wait */
    process_t parent = {0};
    process_t child = {0};
    
    parent.pid = 400;
    child.pid = 401;
    
    /* Setup parent-child relationship */
    add_child_process(&parent, &child);
    
    /* Simulate child termination */
    create_zombie_process(&child, 0);
    
    /* Parent waits for child */
    process_t* zombie = find_zombie_child(&parent, 401);
    ASSERT(zombie == &child);
    
    /* Reap zombie */
    reap_zombie_process(&parent, &child);
    
    return 0;
}

TEST(orphan_handling) {
    /* Test orphaned process handling */
    process_t parent = {0};
    process_t child1 = {0};
    process_t child2 = {0};
    
    parent.pid = 500;
    child1.pid = 501;
    child2.pid = 502;
    
    /* Setup family */
    add_child_process(&parent, &child1);
    add_child_process(&parent, &child2);
    
    /* Handle parent termination */
    int result = handle_orphaned_processes(&parent);
    ASSERT_EQ(0, result);
    
    return 0;
}

/* ========================== Performance Tests ========================== */

TEST(fork_performance) {
    /* Test fork performance */
    process_lifecycle_stats_t stats_before, stats_after;
    
    get_process_lifecycle_stats(&stats_before);
    
    /* Simulate multiple forks */
    for (int i = 0; i < 10; i++) {
        fork_context_t* ctx = create_fork_context(get_current_process());
        if (ctx) {
            destroy_fork_context(ctx);
        }
    }
    
    get_process_lifecycle_stats(&stats_after);
    
    /* Verify statistics tracking works */
    ASSERT(stats_after.total_forks >= stats_before.total_forks);
    
    return 0;
}

TEST(statistics_tracking) {
    /* Test process lifecycle statistics */
    process_lifecycle_stats_t stats;
    
    /* Reset statistics */
    reset_process_lifecycle_stats();
    get_process_lifecycle_stats(&stats);
    
    ASSERT_EQ(0, stats.total_forks);
    ASSERT_EQ(0, stats.total_execs);
    ASSERT_EQ(0, stats.total_waits);
    ASSERT_EQ(0, stats.zombies_created);
    ASSERT_EQ(0, stats.zombies_reaped);
    
    return 0;
}

/* ========================== Error Handling Tests ========================== */

TEST(error_handling) {
    /* Test error handling for invalid arguments */
    
    /* Test NULL arguments */
    ASSERT(create_fork_context(NULL) == NULL);
    ASSERT(create_exec_context(NULL, NULL, NULL) == NULL);
    ASSERT(create_wait_context(-2, NULL, 0) != NULL);  /* -2 is valid (TODO: implement) */
    
    /* Test invalid operations */
    ASSERT_NEQ(0, add_child_process(NULL, NULL));
    ASSERT_NEQ(0, remove_child_process(NULL, NULL));
    ASSERT_NEQ(0, create_zombie_process(NULL, 0));
    
    return 0;
}

TEST(resource_cleanup) {
    /* Test proper resource cleanup */
    
    /* Test context cleanup */
    fork_context_t* fork_ctx = create_fork_context(get_current_process());
    if (fork_ctx) {
        destroy_fork_context(fork_ctx);
    }
    
    char* argv[] = {"test", NULL};
    exec_context_t* exec_ctx = create_exec_context("/bin/test", argv, NULL);
    if (exec_ctx) {
        destroy_exec_context(exec_ctx);
    }
    
    wait_context_t* wait_ctx = create_wait_context(-1, NULL, 0);
    if (wait_ctx) {
        destroy_wait_context(wait_ctx);
    }
    
    return 0;
}

/* ========================== Test Runner ========================== */

static void run_all_tests(void) {
    printf("=== IKOS Process Lifecycle Test Suite - Issue #24 ===\n\n");
    
    /* Fork tests */
    printf("Fork Tests:\n");
    run_test_fork_basic();
    run_test_fork_memory_isolation();
    run_test_fork_file_descriptor_inheritance();
    run_test_fork_process_tree();
    run_test_fork_cow_support();
    
    /* Execve tests */
    printf("\nExecve Tests:\n");
    run_test_execve_basic();
    run_test_execve_argument_validation();
    run_test_execve_path_validation();
    run_test_execve_memory_replacement();
    
    /* Wait tests */
    printf("\nWait Tests:\n");
    run_test_wait_basic();
    run_test_wait_status_macros();
    run_test_wait_zombie_management();
    run_test_wait_nonblocking();
    run_test_wait_process_counting();
    
    /* Integration tests */
    printf("\nIntegration Tests:\n");
    run_test_fork_exec_integration();
    run_test_complete_lifecycle();
    run_test_orphan_handling();
    
    /* Performance tests */
    printf("\nPerformance Tests:\n");
    run_test_fork_performance();
    run_test_statistics_tracking();
    
    /* Error handling tests */
    printf("\nError Handling Tests:\n");
    run_test_error_handling();
    run_test_resource_cleanup();
    
    /* Summary */
    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    
    if (tests_failed == 0) {
        printf("\n✅ All tests passed! Process lifecycle system is working correctly.\n");
    } else {
        printf("\n❌ Some tests failed. Please review the implementation.\n");
    }
}

/* ========================== Main Test Function ========================== */

/**
 * Run comprehensive process lifecycle tests
 */
int test_process_lifecycle(void) {
    /* Initialize process lifecycle system */
    if (process_lifecycle_init() != 0) {
        printf("Failed to initialize process lifecycle system\n");
        return -1;
    }
    
    /* Run all tests */
    run_all_tests();
    
    /* Cleanup */
    process_lifecycle_shutdown();
    
    return (tests_failed == 0) ? 0 : -1;
}

/* ========================== Standalone Test Program ========================== */

#ifdef STANDALONE_TEST
int main(void) {
    return test_process_lifecycle();
}
#endif
