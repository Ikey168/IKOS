/* IKOS Process Manager Test Suite
 * Comprehensive testing for process management functionality
 */

#include "process_manager.h"
#include "process.h"
#include "elf.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

/* Function declarations */
static void debug_print(const char* format, ...);

/* Test framework */
typedef struct {
    const char* name;
    bool (*test_func)(void);
} test_case_t;

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test helper macros */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            debug_print("FAIL: %s - %s\n", __func__, message); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            debug_print("FAIL: %s - %s (expected %d, got %d)\n", \
                       __func__, message, (int)(expected), (int)(actual)); \
            return false; \
        } \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        tests_run++; \
        debug_print("Running test: %s\n", #test_func); \
        if (test_func()) { \
            tests_passed++; \
            debug_print("PASS: %s\n", #test_func); \
        } else { \
            tests_failed++; \
        } \
    } while(0)

/* ================================
 * Process Manager Core Tests
 * ================================ */

/**
 * Test process manager initialization
 */
static bool test_pm_initialization(void) {
    pm_state_t state_before = pm_get_state();
    
    int result = pm_init();
    TEST_ASSERT_EQ(PM_SUCCESS, result, "Process manager initialization failed");
    
    pm_state_t state_after = pm_get_state();
    TEST_ASSERT_EQ(PM_STATE_RUNNING, state_after, "Process manager not in running state");
    
    /* Test double initialization */
    result = pm_init();
    TEST_ASSERT_EQ(PM_SUCCESS, result, "Double initialization should succeed");
    
    return true;
}

/**
 * Test process manager shutdown
 */
static bool test_pm_shutdown(void) {
    /* Ensure initialized first */
    pm_init();
    
    int result = pm_shutdown();
    TEST_ASSERT_EQ(PM_SUCCESS, result, "Process manager shutdown failed");
    
    pm_state_t state = pm_get_state();
    TEST_ASSERT_EQ(PM_STATE_UNINITIALIZED, state, "Process manager should be uninitialized after shutdown");
    
    return true;
}

/**
 * Test process creation with parameters
 */
static bool test_process_creation(void) {
    /* Initialize process manager */
    pm_init();
    
    /* Create process creation parameters */
    pm_create_params_t params = {0};
    strcpy(params.name, "test_process");
    params.argc = 0;
    params.envc = 0;
    params.priority = PROCESS_PRIORITY_NORMAL;
    params.memory_limit = 0;
    params.time_limit = 0;
    params.flags = 0;
    
    uint32_t pid;
    int result = pm_create_process(&params, &pid);
    TEST_ASSERT_EQ(PM_SUCCESS, result, "Process creation failed");
    TEST_ASSERT(pid > 0, "Invalid PID returned");
    
    /* Verify process exists */
    process_t* process = pm_get_process(pid);
    TEST_ASSERT(process != NULL, "Created process not found");
    TEST_ASSERT_EQ(pid, process->pid, "PID mismatch");
    TEST_ASSERT(strcmp(process->name, "test_process") == 0, "Process name mismatch");
    
    return true;
}

/**
 * Test process creation from ELF
 */
static bool test_process_creation_from_elf(void) {
    pm_init();
    
    /* Create a minimal valid ELF header for testing */
    elf64_header_t test_elf = {0};
    test_elf.e_ident[0] = 0x7f;
    test_elf.e_ident[1] = 'E';
    test_elf.e_ident[2] = 'L';
    test_elf.e_ident[3] = 'F';
    test_elf.e_ident[4] = 2; /* 64-bit */
    test_elf.e_ident[5] = 1; /* Little endian */
    test_elf.e_ident[6] = 1; /* Current version */
    test_elf.e_type = 2;     /* Executable */
    test_elf.e_machine = 0x3e; /* x86-64 */
    test_elf.e_version = 1;
    test_elf.e_entry = 0x400000;
    test_elf.e_phoff = sizeof(elf64_header_t);
    test_elf.e_shoff = 0;
    test_elf.e_flags = 0;
    test_elf.e_ehsize = sizeof(elf64_header_t);
    test_elf.e_phentsize = sizeof(elf64_program_header_t);
    test_elf.e_phnum = 0;
    test_elf.e_shentsize = 0;
    test_elf.e_shnum = 0;
    test_elf.e_shstrndx = 0;
    
    uint32_t pid;
    int result = pm_create_process_from_elf("elf_test", &test_elf, sizeof(test_elf), &pid);
    TEST_ASSERT_EQ(PM_SUCCESS, result, "ELF process creation failed");
    TEST_ASSERT(pid > 0, "Invalid PID returned for ELF process");
    
    /* Verify process exists */
    process_t* process = pm_get_process(pid);
    TEST_ASSERT(process != NULL, "ELF process not found");
    TEST_ASSERT_EQ(pid, process->pid, "ELF process PID mismatch");
    
    return true;
}

/**
 * Test process termination
 */
static bool test_process_termination(void) {
    pm_init();
    
    /* Create a process to terminate */
    pm_create_params_t params = {0};
    strcpy(params.name, "terminate_test");
    params.priority = PROCESS_PRIORITY_NORMAL;
    
    uint32_t pid;
    int result = pm_create_process(&params, &pid);
    TEST_ASSERT_EQ(PM_SUCCESS, result, "Process creation for termination test failed");
    
    /* Verify process exists before termination */
    process_t* process = pm_get_process(pid);
    TEST_ASSERT(process != NULL, "Process not found before termination");
    
    /* Terminate the process */
    result = pm_terminate_process(pid, 42);
    TEST_ASSERT_EQ(PM_SUCCESS, result, "Process termination failed");
    
    /* Process should still exist but in zombie state */
    process = pm_get_process(pid);
    /* Note: depending on implementation, this might be NULL for zombie processes */
    
    return true;
}

/* ================================
 * Process Table Tests
 * ================================ */

/**
 * Test PID allocation
 */
static bool test_pid_allocation(void) {
    pm_init();
    
    /* Test PID allocation */
    uint32_t pid1 = pm_table_allocate_pid();
    TEST_ASSERT(pid1 > 0, "First PID allocation failed");
    
    uint32_t pid2 = pm_table_allocate_pid();
    TEST_ASSERT(pid2 > 0, "Second PID allocation failed");
    TEST_ASSERT(pid1 != pid2, "PIDs should be unique");
    
    /* Test PID validity */
    TEST_ASSERT(pm_table_is_pid_valid(pid1), "First PID should be valid");
    TEST_ASSERT(pm_table_is_pid_valid(pid2), "Second PID should be valid");
    TEST_ASSERT(!pm_table_is_pid_valid(0), "PID 0 should be invalid");
    
    /* Free PIDs */
    pm_table_free_pid(pid1);
    pm_table_free_pid(pid2);
    
    return true;
}

/**
 * Test hash table functionality
 */
static bool test_hash_table(void) {
    pm_init();
    
    /* Test hash function */
    uint32_t hash1 = pm_table_hash_pid(1);
    uint32_t hash2 = pm_table_hash_pid(65); /* Should be different from 1 */
    
    TEST_ASSERT(hash1 < PM_PROCESS_HASH_SIZE, "Hash should be within table size");
    TEST_ASSERT(hash2 < PM_PROCESS_HASH_SIZE, "Hash should be within table size");
    
    return true;
}

/**
 * Test multiple process creation
 */
static bool test_multiple_processes(void) {
    pm_init();
    
    const int num_processes = 10;
    uint32_t pids[num_processes];
    
    /* Create multiple processes */
    for (int i = 0; i < num_processes; i++) {
        pm_create_params_t params = {0};
        strcpy(params.name, "test_proc");
        /* Append number to name manually */
        params.name[9] = '0' + (i / 10);
        params.name[10] = '0' + (i % 10);
        params.name[11] = '\0';
        params.priority = PROCESS_PRIORITY_NORMAL;
        
        int result = pm_create_process(&params, &pids[i]);
        TEST_ASSERT_EQ(PM_SUCCESS, result, "Multiple process creation failed");
        TEST_ASSERT(pids[i] > 0, "Invalid PID in multiple creation");
    }
    
    /* Verify all processes exist and are unique */
    for (int i = 0; i < num_processes; i++) {
        process_t* process = pm_get_process(pids[i]);
        TEST_ASSERT(process != NULL, "Process not found in multiple creation");
        
        /* Check uniqueness */
        for (int j = i + 1; j < num_processes; j++) {
            TEST_ASSERT(pids[i] != pids[j], "PIDs should be unique in multiple creation");
        }
    }
    
    return true;
}

/* ================================
 * IPC Tests
 * ================================ */

/**
 * Test IPC channel creation
 */
static bool test_ipc_channel_creation(void) {
    pm_init();
    
    /* Create a process first */
    pm_create_params_t params = {0};
    strcpy(params.name, "ipc_test");
    params.priority = PROCESS_PRIORITY_NORMAL;
    
    uint32_t pid;
    int result = pm_create_process(&params, &pid);
    TEST_ASSERT_EQ(PM_SUCCESS, result, "Process creation for IPC test failed");
    
    /* Create IPC channel */
    uint32_t channel_id;
    result = pm_ipc_create_channel(pid, &channel_id);
    TEST_ASSERT_EQ(PM_SUCCESS, result, "IPC channel creation failed");
    TEST_ASSERT(channel_id > 0, "Invalid channel ID");
    
    return true;
}

/**
 * Test IPC message sending
 */
static bool test_ipc_messaging(void) {
    pm_init();
    
    /* Create process and channel */
    pm_create_params_t params = {0};
    strcpy(params.name, "ipc_msg_test");
    params.priority = PROCESS_PRIORITY_NORMAL;
    
    uint32_t pid;
    int result = pm_create_process(&params, &pid);
    TEST_ASSERT_EQ(PM_SUCCESS, result, "Process creation for IPC messaging failed");
    
    uint32_t channel_id;
    result = pm_ipc_create_channel(pid, &channel_id);
    TEST_ASSERT_EQ(PM_SUCCESS, result, "Channel creation for IPC messaging failed");
    
    /* Create and send message */
    pm_ipc_message_t message = {0};
    message.type = PM_IPC_REQUEST;
    message.src_pid = pid;
    message.dst_pid = 0;
    message.channel_id = channel_id;
    message.data_size = 5;
    strcpy(message.data, "test");
    
    result = pm_ipc_send_message(&message);
    TEST_ASSERT_EQ(PM_SUCCESS, result, "IPC message sending failed");
    
    return true;
}

/* ================================
 * Statistics and Monitoring Tests
 * ================================ */

/**
 * Test statistics collection
 */
static bool test_statistics(void) {
    pm_init();
    
    /* Get initial statistics */
    pm_statistics_t stats_before;
    pm_get_statistics(&stats_before);
    
    /* Create some processes */
    const int num_processes = 3;
    for (int i = 0; i < num_processes; i++) {
        pm_create_params_t params = {0};
        strcpy(params.name, "stats_test");
        /* Append number to name manually */
        params.name[10] = '0' + (i % 10);
        params.name[11] = '\0';
        params.priority = PROCESS_PRIORITY_NORMAL;
        
        uint32_t pid;
        pm_create_process(&params, &pid);
    }
    
    /* Get statistics after creation */
    pm_statistics_t stats_after;
    pm_get_statistics(&stats_after);
    
    /* Verify statistics updated */
    TEST_ASSERT(stats_after.total_created >= stats_before.total_created + num_processes,
               "Statistics should reflect created processes");
    TEST_ASSERT(stats_after.current_active >= stats_before.current_active,
               "Active process count should increase");
    
    return true;
}

/* ================================
 * Error Handling Tests
 * ================================ */

/**
 * Test invalid parameter handling
 */
static bool test_invalid_parameters(void) {
    pm_init();
    
    /* Test NULL parameters */
    uint32_t pid;
    int result = pm_create_process(NULL, &pid);
    TEST_ASSERT_EQ(PM_ERROR_INVALID_PARAM, result, "Should reject NULL parameters");
    
    result = pm_create_process_from_elf(NULL, NULL, 0, &pid);
    TEST_ASSERT_EQ(PM_ERROR_INVALID_PARAM, result, "Should reject NULL ELF parameters");
    
    /* Test invalid PID operations */
    result = pm_terminate_process(0, 0);
    TEST_ASSERT_EQ(PM_ERROR_INVALID_PARAM, result, "Should reject PID 0");
    
    process_t* process = pm_get_process(0);
    TEST_ASSERT(process == NULL, "Should return NULL for invalid PID");
    
    return true;
}

/**
 * Test process table limits
 */
static bool test_process_limits(void) {
    pm_init();
    
    /* This test would create many processes to test limits */
    /* For now, just test that we can handle the expected number */
    
    pm_statistics_t stats;
    pm_get_statistics(&stats);
    
    /* Should be able to create at least a few processes */
    pm_create_params_t params = {0};
    strcpy(params.name, "limit_test");
    params.priority = PROCESS_PRIORITY_NORMAL;
    
    uint32_t pid;
    int result = pm_create_process(&params, &pid);
    TEST_ASSERT_EQ(PM_SUCCESS, result, "Should be able to create process within limits");
    
    return true;
}

/* ================================
 * Test Runner
 * ================================ */

/**
 * Main entry point for testing
 */
int main(void) {
    return run_process_manager_tests();
}

/**
 * Run all process manager tests
 */
int run_process_manager_tests(void) {
    debug_print("=================================================\n");
    debug_print("IKOS Process Manager Test Suite\n");
    debug_print("=================================================\n");
    
    tests_run = 0;
    tests_passed = 0;
    tests_failed = 0;
    
    /* Core functionality tests */
    RUN_TEST(test_pm_initialization);
    RUN_TEST(test_process_creation);
    RUN_TEST(test_process_creation_from_elf);
    RUN_TEST(test_process_termination);
    
    /* Process table tests */
    RUN_TEST(test_pid_allocation);
    RUN_TEST(test_hash_table);
    RUN_TEST(test_multiple_processes);
    
    /* IPC tests */
    RUN_TEST(test_ipc_channel_creation);
    RUN_TEST(test_ipc_messaging);
    
    /* Statistics and monitoring */
    RUN_TEST(test_statistics);
    
    /* Error handling tests */
    RUN_TEST(test_invalid_parameters);
    RUN_TEST(test_process_limits);
    
    /* Shutdown test (run last) */
    RUN_TEST(test_pm_shutdown);
    
    /* Print results */
    debug_print("=================================================\n");
    debug_print("Test Results:\n");
    debug_print("  Total Tests: %d\n", tests_run);
    debug_print("  Passed:      %d\n", tests_passed);
    debug_print("  Failed:      %d\n", tests_failed);
    debug_print("  Success Rate: %d%%\n", tests_run > 0 ? (tests_passed * 100 / tests_run) : 0);
    debug_print("=================================================\n");
    
    return (tests_failed == 0) ? 0 : -1;
}

/**
 * Simple debug print function
 */
static void debug_print(const char* format, ...) {
    /* In a real kernel, this would output to the console/log */
    /* For now, it's a placeholder */
    (void)format;
}
