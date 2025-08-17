/* IKOS Logging & Debugging Service - Test Suite
 * Comprehensive tests for logging and debugging functionality
 */

#include "../include/logging_debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>

/* Test framework */
typedef struct {
    const char* name;
    int (*test_func)(void);
} test_case_t;

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            return -1; \
        } \
    } while(0)

#define RUN_TEST(test) \
    do { \
        printf("Running %s...", #test); \
        if (test() == 0) { \
            printf(" PASS\n"); \
            tests_passed++; \
        } else { \
            printf(" FAIL\n"); \
            tests_failed++; \
        } \
    } while(0)

/* ========================== Basic Logger Tests ========================== */

static int test_logger_init() {
    log_config_t config = {
        .min_level = LOG_LEVEL_DEBUG,
        .max_level = LOG_LEVEL_EMERG,
        .facility_mask = 0xFFFFFFFF,
        .flag_mask = 0xFFFF,
        .buffer_size = 4096,
        .max_outputs = 10,
        .default_format = LOG_FORMAT_TIMESTAMP | LOG_FORMAT_LEVEL | LOG_FORMAT_MESSAGE,
        .enable_colors = true,
        .thread_safe = true
    };
    
    int ret = logger_init(&config);
    TEST_ASSERT(ret == LOG_SUCCESS, "Logger initialization failed");
    
    /* Test re-initialization (should fail) */
    ret = logger_init(&config);
    TEST_ASSERT(ret == LOG_ERROR_ALREADY_INIT, "Re-initialization should fail");
    
    return 0;
}

static int test_basic_logging() {
    /* Test all log levels */
    int ret;
    
    ret = log_debug("Debug message: %d", 42);
    TEST_ASSERT(ret == LOG_SUCCESS, "Debug logging failed");
    
    ret = log_info("Info message: %s", "test");
    TEST_ASSERT(ret == LOG_SUCCESS, "Info logging failed");
    
    ret = log_warning("Warning message: %f", 3.14);
    TEST_ASSERT(ret == LOG_SUCCESS, "Warning logging failed");
    
    ret = log_error("Error message: %x", 0xDEADBEEF);
    TEST_ASSERT(ret == LOG_SUCCESS, "Error logging failed");
    
    ret = log_critical("Critical message");
    TEST_ASSERT(ret == LOG_SUCCESS, "Critical logging failed");
    
    return 0;
}

static int test_facility_logging() {
    int ret;
    
    ret = log_message(LOG_LEVEL_INFO, LOG_FACILITY_KERNEL, 0, "Kernel message");
    TEST_ASSERT(ret == LOG_SUCCESS, "Kernel facility logging failed");
    
    ret = log_message(LOG_LEVEL_INFO, LOG_FACILITY_MEMORY, 0, "Memory message");
    TEST_ASSERT(ret == LOG_SUCCESS, "Memory facility logging failed");
    
    ret = log_message(LOG_LEVEL_INFO, LOG_FACILITY_PROCESS, 0, "Process message");
    TEST_ASSERT(ret == LOG_SUCCESS, "Process facility logging failed");
    
    ret = log_message(LOG_LEVEL_INFO, LOG_FACILITY_VFS, 0, "VFS message");
    TEST_ASSERT(ret == LOG_SUCCESS, "VFS facility logging failed");
    
    return 0;
}

static int test_log_flags() {
    int ret;
    
    ret = log_message(LOG_LEVEL_INFO, LOG_FACILITY_KERNEL, LOG_FLAG_ASYNC, "Async message");
    TEST_ASSERT(ret == LOG_SUCCESS, "Async flag logging failed");
    
    ret = log_message(LOG_LEVEL_INFO, LOG_FACILITY_KERNEL, LOG_FLAG_ONCE, "Once message");
    TEST_ASSERT(ret == LOG_SUCCESS, "Once flag logging failed");
    
    /* Test ONCE flag - second call should be ignored */
    ret = log_message(LOG_LEVEL_INFO, LOG_FACILITY_KERNEL, LOG_FLAG_ONCE, "Once message");
    TEST_ASSERT(ret == LOG_SUCCESS, "Second ONCE flag call should succeed but be ignored");
    
    ret = log_message(LOG_LEVEL_INFO, LOG_FACILITY_KERNEL, LOG_FLAG_CONT, "Continuation");
    TEST_ASSERT(ret == LOG_SUCCESS, "Continuation flag logging failed");
    
    return 0;
}

/* ========================== Output Management Tests ========================== */

static int test_console_output() {
    log_console_config_t console_config = {
        .use_colors = true,
        .show_timestamp = true,
        .show_facility = true,
        .show_function = false,
        .max_line_length = 1024
    };
    
    int ret = logger_add_console_output(LOG_LEVEL_DEBUG, &console_config);
    TEST_ASSERT(ret == LOG_SUCCESS, "Console output addition failed");
    
    /* Test console logging */
    log_info("Console test message");
    
    return 0;
}

static int test_file_output() {
    log_file_config_t file_config = {
        .max_size = 1024 * 1024,  /* 1MB */
        .max_files = 5,
        .compress = false,
        .sync_interval = 10,
        .permissions = 0644
    };
    strncpy(file_config.path, "/tmp/ikos_test.log", sizeof(file_config.path) - 1);
    
    int ret = logger_add_file_output("/tmp/ikos_test.log", LOG_LEVEL_DEBUG, &file_config);
    TEST_ASSERT(ret == LOG_SUCCESS, "File output addition failed");
    
    /* Test file logging */
    log_info("File test message");
    
    /* Verify file exists */
    struct stat st;
    TEST_ASSERT(stat("/tmp/ikos_test.log", &st) == 0, "Log file was not created");
    
    return 0;
}

/* ========================== Debug Symbol Tests ========================== */

static int test_debug_symbols() {
    /* Initialize debug symbols */
    int ret = debug_init();
    TEST_ASSERT(ret == LOG_SUCCESS, "Debug initialization failed");
    
    /* Test symbol loading (simplified - would need actual binary) */
    ret = debug_load_symbols("/proc/self/exe", 0x400000);
    if (ret == LOG_SUCCESS) {
        /* Test symbol resolution */
        debug_symbol_t symbol;
        ret = debug_resolve_symbol((void*)test_debug_symbols, &symbol);
        if (ret == LOG_SUCCESS) {
            printf("Symbol resolved: %s at 0x%lx\n", symbol.name, symbol.address);
        }
    }
    
    return 0;
}

static int test_stack_trace() {
    /* Capture stack trace */
    void* trace[10];
    int count = debug_capture_stack_trace(trace, 10, 0);
    TEST_ASSERT(count > 0, "Stack trace capture failed");
    
    printf("Stack trace captured %d frames:\n", count);
    for (int i = 0; i < count; i++) {
        printf("  [%d] %p\n", i, trace[i]);
    }
    
    return 0;
}

/* ========================== Performance Tests ========================== */

static void* logging_thread(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 1000; i++) {
        log_info("Thread %d message %d", thread_id, i);
    }
    
    return NULL;
}

static int test_concurrent_logging() {
    const int num_threads = 4;
    pthread_t threads[num_threads];
    int thread_ids[num_threads];
    
    /* Start logging threads */
    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i;
        int ret = pthread_create(&threads[i], NULL, logging_thread, &thread_ids[i]);
        TEST_ASSERT(ret == 0, "Thread creation failed");
    }
    
    /* Wait for completion */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    return 0;
}

static int test_logging_performance() {
    const int num_messages = 10000;
    struct timespec start, end;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < num_messages; i++) {
        log_info("Performance test message %d", i);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_nsec - start.tv_nsec) / 1000000000.0;
    double rate = num_messages / elapsed;
    
    printf("Logged %d messages in %.3f seconds (%.0f msg/sec)\n", 
           num_messages, elapsed, rate);
    
    TEST_ASSERT(rate > 1000, "Logging rate too slow");
    
    return 0;
}

/* ========================== Filter Tests ========================== */

static int test_level_filtering() {
    /* Set minimum level to WARNING */
    int ret = logger_set_level(LOG_LEVEL_WARNING);
    TEST_ASSERT(ret == LOG_SUCCESS, "Setting log level failed");
    
    /* These should not appear in output */
    log_debug("This debug message should be filtered");
    log_info("This info message should be filtered");
    
    /* These should appear */
    log_warning("This warning message should appear");
    log_error("This error message should appear");
    
    /* Reset to DEBUG level */
    ret = logger_set_level(LOG_LEVEL_DEBUG);
    TEST_ASSERT(ret == LOG_SUCCESS, "Resetting log level failed");
    
    return 0;
}

static int test_facility_filtering() {
    /* Enable only kernel and memory facilities */
    uint32_t mask = (1 << LOG_FACILITY_KERNEL) | (1 << LOG_FACILITY_MEMORY);
    int ret = logger_set_facility_mask(mask);
    TEST_ASSERT(ret == LOG_SUCCESS, "Setting facility mask failed");
    
    /* These should appear */
    log_message(LOG_LEVEL_INFO, LOG_FACILITY_KERNEL, 0, "Kernel message - should appear");
    log_message(LOG_LEVEL_INFO, LOG_FACILITY_MEMORY, 0, "Memory message - should appear");
    
    /* These should be filtered */
    log_message(LOG_LEVEL_INFO, LOG_FACILITY_PROCESS, 0, "Process message - should be filtered");
    log_message(LOG_LEVEL_INFO, LOG_FACILITY_VFS, 0, "VFS message - should be filtered");
    
    /* Reset to all facilities */
    ret = logger_set_facility_mask(0xFFFFFFFF);
    TEST_ASSERT(ret == LOG_SUCCESS, "Resetting facility mask failed");
    
    return 0;
}

/* ========================== Stress Tests ========================== */

static int test_buffer_overflow() {
    /* Try to overflow the log buffer with large messages */
    char large_message[8192];
    memset(large_message, 'A', sizeof(large_message) - 1);
    large_message[sizeof(large_message) - 1] = '\0';
    
    int ret = log_info("Large message: %s", large_message);
    TEST_ASSERT(ret == LOG_SUCCESS || ret == LOG_ERROR_TRUNCATED, 
                "Large message handling failed");
    
    return 0;
}

static int test_rapid_logging() {
    /* Rapid fire logging to test buffer management */
    for (int i = 0; i < 1000; i++) {
        log_debug("Rapid message %d", i);
    }
    
    /* Give time for async processing */
    usleep(100000); /* 100ms */
    
    return 0;
}

/* ========================== Statistics Tests ========================== */

static int test_statistics() {
    log_statistics_t stats;
    int ret = logger_get_statistics(&stats);
    TEST_ASSERT(ret == LOG_SUCCESS, "Getting statistics failed");
    
    printf("Logger Statistics:\n");
    printf("  Messages logged: %lu\n", stats.messages_logged);
    printf("  Messages dropped: %lu\n", stats.messages_dropped);
    printf("  Bytes logged: %lu\n", stats.bytes_logged);
    printf("  Errors: %lu\n", stats.errors);
    printf("  Buffer overflows: %lu\n", stats.buffer_overflows);
    printf("  Uptime: %lu seconds\n", stats.uptime_seconds);
    
    TEST_ASSERT(stats.messages_logged > 0, "No messages logged");
    
    return 0;
}

/* ========================== Cleanup Tests ========================== */

static int test_logger_shutdown() {
    int ret = logger_shutdown();
    TEST_ASSERT(ret == LOG_SUCCESS, "Logger shutdown failed");
    
    /* Test logging after shutdown (should fail) */
    ret = log_info("This should fail");
    TEST_ASSERT(ret == LOG_ERROR_NOT_INIT, "Logging after shutdown should fail");
    
    return 0;
}

/* ========================== Test Suite ========================== */

static test_case_t test_cases[] = {
    {"Logger Initialization", test_logger_init},
    {"Basic Logging", test_basic_logging},
    {"Facility Logging", test_facility_logging},
    {"Log Flags", test_log_flags},
    {"Console Output", test_console_output},
    {"File Output", test_file_output},
    {"Debug Symbols", test_debug_symbols},
    {"Stack Trace", test_stack_trace},
    {"Level Filtering", test_level_filtering},
    {"Facility Filtering", test_facility_filtering},
    {"Concurrent Logging", test_concurrent_logging},
    {"Logging Performance", test_logging_performance},
    {"Buffer Overflow", test_buffer_overflow},
    {"Rapid Logging", test_rapid_logging},
    {"Statistics", test_statistics},
    {"Logger Shutdown", test_logger_shutdown},
    {NULL, NULL}
};

/* ========================== Main Test Runner ========================== */

int main(int argc, char* argv[]) {
    printf("IKOS Logging & Debugging Service Test Suite\n");
    printf("============================================\n\n");
    
    /* Run basic functionality tests */
    for (int i = 0; test_cases[i].name; i++) {
        RUN_TEST(test_cases[i].test_func);
    }
    
    printf("\n============================================\n");
    printf("Test Results: %d passed, %d failed\n", tests_passed, tests_failed);
    
    if (tests_failed == 0) {
        printf("All tests PASSED!\n");
        return 0;
    } else {
        printf("Some tests FAILED!\n");
        return 1;
    }
}

/* ========================== Additional Test Utilities ========================== */

/* Function to create test scenario with multiple outputs */
void setup_test_outputs(void) {
    /* Console output */
    log_console_config_t console_config = {
        .use_colors = true,
        .show_timestamp = true,
        .show_facility = true,
        .show_function = false,
        .max_line_length = 1024
    };
    logger_add_console_output(LOG_LEVEL_DEBUG, &console_config);
    
    /* File output */
    log_file_config_t file_config = {
        .max_size = 1024 * 1024,
        .max_files = 3,
        .compress = false,
        .sync_interval = 5,
        .permissions = 0644
    };
    strncpy(file_config.path, "/tmp/ikos_test_full.log", sizeof(file_config.path) - 1);
    logger_add_file_output("/tmp/ikos_test_full.log", LOG_LEVEL_INFO, &file_config);
}

/* Test message formatting */
void test_message_formats(void) {
    log_info("Standard message");
    log_warning("Message with number: %d", 42);
    log_error("Message with string: %s", "test string");
    log_debug("Message with multiple args: %d, %s, %f", 123, "hello", 3.14);
    
    /* Test different facilities */
    log_message(LOG_LEVEL_INFO, LOG_FACILITY_KERNEL, 0, "Kernel subsystem message");
    log_message(LOG_LEVEL_INFO, LOG_FACILITY_MEMORY, 0, "Memory management message");
    log_message(LOG_LEVEL_INFO, LOG_FACILITY_PROCESS, 0, "Process management message");
    log_message(LOG_LEVEL_INFO, LOG_FACILITY_VFS, 0, "Virtual file system message");
    log_message(LOG_LEVEL_INFO, LOG_FACILITY_NETWORK, 0, "Network subsystem message");
    log_message(LOG_LEVEL_INFO, LOG_FACILITY_HARDWARE, 0, "Hardware abstraction message");
    log_message(LOG_LEVEL_INFO, LOG_FACILITY_SECURITY, 0, "Security subsystem message");
    log_message(LOG_LEVEL_INFO, LOG_FACILITY_USER, 0, "User space message");
}
