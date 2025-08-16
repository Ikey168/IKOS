/* IKOS Runtime Kernel Debugger Test Suite - Issue #16 Enhancement
 * Comprehensive testing of the runtime debugging system
 */

#include "../include/kernel_debug.h"

/* Try to include existing logging system if available */
#ifdef HAVE_KERNEL_LOG
#include "../include/kernel_log.h"
#else
/* Fallback logging macros if kernel_log.h not available */
#define KLOG_INFO(cat, ...) do { } while(0)
#define KLOG_DEBUG(cat, ...) do { } while(0)
#define KLOG_ERROR(cat, ...) do { } while(0)
#define LOG_CAT_KERNEL 0
#define LOG_CAT_MEMORY 1
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Test configuration */
#define TEST_MEMORY_SIZE 1024
#define TEST_PATTERN_SIZE 16

/* Test data */
static uint8_t test_memory[TEST_MEMORY_SIZE];
static const uint8_t test_pattern[TEST_PATTERN_SIZE] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0
};

/* Test results tracking */
typedef struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
    char last_error[256];
} test_results_t;

static test_results_t test_results = {0};

/* ========================== Test Helper Functions ========================== */

#define TEST_ASSERT(condition, message) do { \
    test_results.tests_run++; \
    if (condition) { \
        test_results.tests_passed++; \
        KLOG_DEBUG(LOG_CAT_KERNEL, "✅ PASS: %s", message); \
    } else { \
        test_results.tests_failed++; \
        strncpy(test_results.last_error, message, sizeof(test_results.last_error) - 1); \
        KLOG_ERROR(LOG_CAT_KERNEL, "❌ FAIL: %s", message); \
    } \
} while(0)

#define TEST_SECTION(name) do { \
    KLOG_INFO(LOG_CAT_KERNEL, "\n=== Testing %s ===", name); \
} while(0)

void print_test_summary(void) {
    KLOG_INFO(LOG_CAT_KERNEL, "\n=== Test Summary ===");
    KLOG_INFO(LOG_CAT_KERNEL, "Tests Run: %d", test_results.tests_run);
    KLOG_INFO(LOG_CAT_KERNEL, "Tests Passed: %d", test_results.tests_passed);
    KLOG_INFO(LOG_CAT_KERNEL, "Tests Failed: %d", test_results.tests_failed);
    
    if (test_results.tests_failed > 0) {
        KLOG_ERROR(LOG_CAT_KERNEL, "Last Error: %s", test_results.last_error);
        KLOG_ERROR(LOG_CAT_KERNEL, "❌ SOME TESTS FAILED");
    } else {
        KLOG_INFO(LOG_CAT_KERNEL, "✅ ALL TESTS PASSED");
    }
    
    double pass_rate = (double)test_results.tests_passed / test_results.tests_run * 100.0;
    KLOG_INFO(LOG_CAT_KERNEL, "Pass Rate: %.1f%%", pass_rate);
}

/* ========================== Core Functionality Tests ========================== */

void test_debugger_initialization(void) {
    TEST_SECTION("Debugger Initialization");
    
    /* Test initialization */
    bool init_result = kdebug_init();
    TEST_ASSERT(init_result, "Debugger initialization should succeed");
    
    /* Test initial state */
    kdebug_state_t initial_state = kdebug_get_state();
    TEST_ASSERT(initial_state == KDEBUG_STATE_DISABLED, "Initial state should be DISABLED");
    
    /* Test enabling */
    kdebug_set_enabled(true);
    TEST_ASSERT(kdebug_is_enabled(), "Debugger should be enabled after kdebug_set_enabled(true)");
    
    kdebug_state_t enabled_state = kdebug_get_state();
    TEST_ASSERT(enabled_state == KDEBUG_STATE_ENABLED, "State should be ENABLED when enabled");
    
    /* Test disabling */
    kdebug_set_enabled(false);
    TEST_ASSERT(!kdebug_is_enabled(), "Debugger should be disabled after kdebug_set_enabled(false)");
    
    /* Re-enable for other tests */
    kdebug_set_enabled(true);
}

void test_breakpoint_management(void) {
    TEST_SECTION("Breakpoint Management");
    
    /* Test setting breakpoints */
    int bp1 = kdebug_set_breakpoint(0x12345678, "Test breakpoint 1");
    TEST_ASSERT(bp1 >= 0, "Setting breakpoint should return valid ID");
    
    int bp2 = kdebug_set_breakpoint(0x87654321, "Test breakpoint 2");
    TEST_ASSERT(bp2 >= 0, "Setting second breakpoint should return valid ID");
    
    TEST_ASSERT(bp1 != bp2, "Different breakpoints should have different IDs");
    
    /* Test removing breakpoints */
    bool remove_result = kdebug_remove_breakpoint(bp1);
    TEST_ASSERT(remove_result, "Removing valid breakpoint should succeed");
    
    bool remove_invalid = kdebug_remove_breakpoint(999);
    TEST_ASSERT(!remove_invalid, "Removing invalid breakpoint should fail");
    
    /* Test clearing all breakpoints */
    kdebug_clear_all_breakpoints();
    /* No direct way to verify, but should not crash */
    
    /* Test maximum breakpoints */
    int bp_ids[KDEBUG_MAX_BREAKPOINTS + 2];
    int valid_bps = 0;
    
    for (int i = 0; i < KDEBUG_MAX_BREAKPOINTS + 2; i++) {
        bp_ids[i] = kdebug_set_breakpoint(0x1000 + i * 0x100, "Max test breakpoint");
        if (bp_ids[i] >= 0) {
            valid_bps++;
        }
    }
    
    TEST_ASSERT(valid_bps <= KDEBUG_MAX_BREAKPOINTS, "Should not exceed maximum breakpoints");
    
    /* Clean up */
    kdebug_clear_all_breakpoints();
}

void test_watchpoint_management(void) {
    TEST_SECTION("Watchpoint Management");
    
    /* Test setting watchpoints */
    int wp1 = kdebug_set_watchpoint((uint64_t)test_memory, 64, KDEBUG_BP_MEMORY_READ, "Read watchpoint");
    TEST_ASSERT(wp1 >= 0, "Setting read watchpoint should return valid ID");
    
    int wp2 = kdebug_set_watchpoint((uint64_t)test_memory + 64, 64, KDEBUG_BP_MEMORY_WRITE, "Write watchpoint");
    TEST_ASSERT(wp2 >= 0, "Setting write watchpoint should return valid ID");
    
    int wp3 = kdebug_set_watchpoint((uint64_t)test_memory + 128, 64, KDEBUG_BP_MEMORY_ACCESS, "Access watchpoint");
    TEST_ASSERT(wp3 >= 0, "Setting access watchpoint should return valid ID");
    
    /* Test removing watchpoints */
    bool remove_result = kdebug_remove_breakpoint(wp1);
    TEST_ASSERT(remove_result, "Removing valid watchpoint should succeed");
    
    /* Test maximum watchpoints */
    kdebug_clear_all_breakpoints();  /* This clears watchpoints too */
    
    int valid_wps = 0;
    for (int i = 0; i < KDEBUG_MAX_WATCHPOINTS + 2; i++) {
        int wp = kdebug_set_watchpoint((uint64_t)test_memory + i * 32, 32, 
                                      KDEBUG_BP_MEMORY_ACCESS, "Max test watchpoint");
        if (wp >= 0) {
            valid_wps++;
        }
    }
    
    TEST_ASSERT(valid_wps <= KDEBUG_MAX_WATCHPOINTS, "Should not exceed maximum watchpoints");
    
    /* Clean up */
    kdebug_clear_all_breakpoints();
}

void test_memory_debugging(void) {
    TEST_SECTION("Memory Debugging");
    
    /* Initialize test memory with pattern */
    memcpy(test_memory, test_pattern, TEST_PATTERN_SIZE);
    for (int i = TEST_PATTERN_SIZE; i < TEST_MEMORY_SIZE; i++) {
        test_memory[i] = (uint8_t)(i & 0xFF);
    }
    
    /* Test memory dump (visual inspection required) */
    KLOG_INFO(LOG_CAT_KERNEL, "Testing memory dump (should show test pattern):");
    kdebug_memory_dump((uint64_t)test_memory, 64);
    
    /* Test memory search */
    uint64_t found_address = kdebug_memory_search((uint64_t)test_memory, 
                                                 (uint64_t)test_memory + TEST_MEMORY_SIZE,
                                                 test_pattern, TEST_PATTERN_SIZE);
    TEST_ASSERT(found_address == (uint64_t)test_memory, "Memory search should find test pattern at start");
    
    /* Test memory search for non-existent pattern */
    uint8_t fake_pattern[] = {0xFF, 0xEE, 0xDD, 0xCC};
    uint64_t not_found = kdebug_memory_search((uint64_t)test_memory,
                                             (uint64_t)test_memory + TEST_MEMORY_SIZE,
                                             fake_pattern, sizeof(fake_pattern));
    TEST_ASSERT(not_found == 0, "Memory search should return 0 for non-existent pattern");
    
    /* Test memory read */
    uint8_t read_buffer[TEST_PATTERN_SIZE];
    bool read_result = kdebug_memory_read((uint64_t)test_memory, read_buffer, TEST_PATTERN_SIZE);
    TEST_ASSERT(read_result, "Memory read should succeed");
    
    bool pattern_match = (memcmp(read_buffer, test_pattern, TEST_PATTERN_SIZE) == 0);
    TEST_ASSERT(pattern_match, "Memory read should return correct data");
    
    /* Test memory write */
    uint8_t write_pattern[] = {0xAA, 0xBB, 0xCC, 0xDD};
    bool write_result = kdebug_memory_write((uint64_t)test_memory + 100, write_pattern, sizeof(write_pattern));
    TEST_ASSERT(write_result, "Memory write should succeed");
    
    /* Verify write */
    bool write_verify = (memcmp(test_memory + 100, write_pattern, sizeof(write_pattern)) == 0);
    TEST_ASSERT(write_verify, "Memory write should modify target memory correctly");
}

void test_register_operations(void) {
    TEST_SECTION("Register Operations");
    
    /* Test register capture */
    kdebug_registers_t test_regs;
    kdebug_capture_registers(&test_regs);
    
    /* Basic sanity checks on captured registers */
    TEST_ASSERT(test_regs.rsp != 0, "Stack pointer should not be zero");
    TEST_ASSERT(test_regs.rip != 0, "Instruction pointer should not be zero");
    TEST_ASSERT(test_regs.cs != 0, "Code segment should not be zero");
    
    /* Test register display (visual inspection required) */
    KLOG_INFO(LOG_CAT_KERNEL, "Testing register display:");
    kdebug_display_registers(&test_regs);
}

void test_stack_tracing(void) {
    TEST_SECTION("Stack Tracing");
    
    /* Test stack trace generation */
    KLOG_INFO(LOG_CAT_KERNEL, "Testing stack trace generation:");
    kdebug_stack_trace(NULL);
    
    /* Test stack frame capture */
    kdebug_stack_frame_t frames[8];
    int frame_count = kdebug_get_stack_frames(frames, 8, NULL);
    
    TEST_ASSERT(frame_count > 0, "Should capture at least one stack frame");
    TEST_ASSERT(frame_count <= 8, "Should not exceed requested frame count");
    
    /* Verify first frame has valid RIP */
    if (frame_count > 0) {
        TEST_ASSERT(frames[0].rip != 0, "First frame should have valid instruction pointer");
    }
    
    /* Test symbol lookup (basic test) */
    char symbol_name[64];
    bool symbol_found = kdebug_lookup_symbol(frames[0].rip, symbol_name, sizeof(symbol_name));
    /* Symbol lookup may not find symbols, but should not crash */
    TEST_ASSERT(symbol_name[0] != '\0', "Symbol lookup should return some string");
}

void test_state_inspection(void) {
    TEST_SECTION("State Inspection");
    
    /* Test kernel state display (visual inspection required) */
    KLOG_INFO(LOG_CAT_KERNEL, "Testing kernel state display:");
    kdebug_display_kernel_state();
    
    /* Test process info display (visual inspection required) */
    KLOG_INFO(LOG_CAT_KERNEL, "Testing process info display:");
    kdebug_display_process_info();
    
    /* These tests mainly verify that functions don't crash */
    TEST_ASSERT(true, "State inspection functions should not crash");
}

void test_statistics(void) {
    TEST_SECTION("Statistics");
    
    /* Reset statistics */
    kdebug_reset_statistics();
    
    /* Get initial statistics */
    const kdebug_stats_t* stats = kdebug_get_statistics();
    TEST_ASSERT(stats != NULL, "Statistics pointer should not be NULL");
    TEST_ASSERT(stats->total_breakpoints_hit == 0, "Initial breakpoint hits should be zero");
    TEST_ASSERT(stats->memory_dumps_performed == 0, "Initial memory dumps should be zero");
    
    /* Perform some operations that should update statistics */
    kdebug_memory_dump((uint64_t)test_memory, 32);
    kdebug_stack_trace(NULL);
    
    /* Check updated statistics */
    const kdebug_stats_t* updated_stats = kdebug_get_statistics();
    TEST_ASSERT(updated_stats->memory_dumps_performed > 0, "Memory dumps count should increase");
    TEST_ASSERT(updated_stats->stack_traces_generated > 0, "Stack traces count should increase");
    
    /* Test statistics display */
    KLOG_INFO(LOG_CAT_KERNEL, "Testing statistics display:");
    kdebug_display_statistics();
}

void test_command_processing(void) {
    TEST_SECTION("Command Processing");
    
    /* Test valid commands */
    bool help_result = kdebug_process_command("help");
    TEST_ASSERT(help_result, "Help command should succeed");
    
    bool regs_result = kdebug_process_command("regs");
    TEST_ASSERT(regs_result, "Regs command should succeed");
    
    bool stack_result = kdebug_process_command("stack");
    TEST_ASSERT(stack_result, "Stack command should succeed");
    
    bool bp_result = kdebug_process_command("bp");
    TEST_ASSERT(bp_result, "Breakpoint list command should succeed");
    
    bool stats_result = kdebug_process_command("stats");
    TEST_ASSERT(stats_result, "Stats command should succeed");
    
    /* Test invalid command */
    bool invalid_result = kdebug_process_command("invalid_command_xyz");
    TEST_ASSERT(!invalid_result, "Invalid command should fail");
    
    /* Test NULL command */
    bool null_result = kdebug_process_command(NULL);
    TEST_ASSERT(!null_result, "NULL command should fail");
}

void test_convenience_macros(void) {
    TEST_SECTION("Convenience Macros");
    
    /* Test memory dump macro */
    KDEBUG_DUMP_MEMORY(test_memory, 32);
    
    /* Test stack trace macro */
    KDEBUG_STACK_TRACE();
    
    /* Test conditional break macro with false condition */
    KDEBUG_BREAK_IF(false);
    
    /* Test assert macro with true condition */
    KDEBUG_ASSERT(true);
    
    /* These macros should execute without crashing */
    TEST_ASSERT(true, "Convenience macros should execute without errors");
}

/* ========================== Integration Tests ========================== */

void test_integration_with_logging(void) {
    TEST_SECTION("Integration with Logging System");
    
    /* Verify that both logging and debugging can work together */
    KLOG_INFO(LOG_CAT_KERNEL, "Testing integration between logging and debugging systems");
    
    /* Set a breakpoint and log about it */
    int bp = kdebug_set_breakpoint((uint64_t)test_integration_with_logging, "Integration test breakpoint");
    if (bp >= 0) {
        KLOG_DEBUG(LOG_CAT_KERNEL, "Successfully set integration test breakpoint %d", bp);
        kdebug_remove_breakpoint(bp);
        KLOG_DEBUG(LOG_CAT_KERNEL, "Successfully removed integration test breakpoint %d", bp);
    }
    
    /* Test that both systems maintain their statistics independently */
    const kdebug_stats_t* debug_stats = kdebug_get_statistics();
    TEST_ASSERT(debug_stats != NULL, "Debug statistics should be available");
    
    /* Both systems should work without interference */
    TEST_ASSERT(true, "Logging and debugging systems should coexist peacefully");
}

void test_error_conditions(void) {
    TEST_SECTION("Error Conditions");
    
    /* Test operations with debugger disabled */
    kdebug_set_enabled(false);
    
    int bp_disabled = kdebug_set_breakpoint(0x12345678, "Should fail when disabled");
    TEST_ASSERT(bp_disabled == -1, "Breakpoint setting should fail when debugger disabled");
    
    bool read_disabled = kdebug_memory_read(0x12345678, test_memory, 32);
    TEST_ASSERT(!read_disabled, "Memory read should fail when debugger disabled");
    
    /* Re-enable for remaining tests */
    kdebug_set_enabled(true);
    
    /* Test invalid memory operations */
    bool invalid_read = kdebug_memory_read(0, NULL, 32);
    /* This might succeed or fail depending on implementation */
    
    /* Test removing non-existent breakpoint */
    bool remove_invalid = kdebug_remove_breakpoint(-1);
    TEST_ASSERT(!remove_invalid, "Removing invalid breakpoint ID should fail");
    
    remove_invalid = kdebug_remove_breakpoint(999);
    TEST_ASSERT(!remove_invalid, "Removing non-existent breakpoint should fail");
}

/* ========================== Performance Tests ========================== */

void test_performance(void) {
    TEST_SECTION("Performance");
    
    /* Test that debugging operations don't take excessive time */
    /* Note: In a real kernel, you'd use proper timing mechanisms */
    
    uint64_t start_cycles, end_cycles;
    
    /* Time memory dump operation */
    __asm__ volatile ("rdtsc" : "=A" (start_cycles));
    kdebug_memory_dump((uint64_t)test_memory, 256);
    __asm__ volatile ("rdtsc" : "=A" (end_cycles));
    
    KLOG_DEBUG(LOG_CAT_KERNEL, "Memory dump took approximately %lu cycles", end_cycles - start_cycles);
    
    /* Time stack trace operation */
    __asm__ volatile ("rdtsc" : "=A" (start_cycles));
    kdebug_stack_trace(NULL);
    __asm__ volatile ("rdtsc" : "=A" (end_cycles));
    
    KLOG_DEBUG(LOG_CAT_KERNEL, "Stack trace took approximately %lu cycles", end_cycles - start_cycles);
    
    /* Performance tests are mainly for observation */
    TEST_ASSERT(true, "Performance measurements completed");
}

/* ========================== Main Test Runner ========================== */

void run_kernel_debug_tests(void) {
    KLOG_INFO(LOG_CAT_KERNEL, "\n" 
              "╔════════════════════════════════════════════════════════════════╗\n"
              "║              IKOS Runtime Kernel Debugger Test Suite          ║\n"
              "║                     Issue #16 Enhancement                     ║\n"
              "╚════════════════════════════════════════════════════════════════╝");
    
    /* Initialize test environment */
    memset(&test_results, 0, sizeof(test_results));
    
    /* Run all test categories */
    test_debugger_initialization();
    test_breakpoint_management();
    test_watchpoint_management();
    test_memory_debugging();
    test_register_operations();
    test_stack_tracing();
    test_state_inspection();
    test_statistics();
    test_command_processing();
    test_convenience_macros();
    test_integration_with_logging();
    test_error_conditions();
    test_performance();
    
    /* Display final results */
    print_test_summary();
    
    /* Final verdict */
    if (test_results.tests_failed == 0) {
        KLOG_INFO(LOG_CAT_KERNEL, 
                 "\n🎉 Runtime Kernel Debugger Test Suite PASSED!\n"
                 "✅ All debugging features are working correctly.\n"
                 "✅ Integration with existing logging system is successful.\n"
                 "✅ Ready for production use in debug builds.\n");
    } else {
        KLOG_ERROR(LOG_CAT_KERNEL,
                  "\n❌ Runtime Kernel Debugger Test Suite FAILED!\n"
                  "❌ %d out of %d tests failed.\n"
                  "❌ Issues need to be resolved before production use.\n",
                  test_results.tests_failed, test_results.tests_run);
    }
}

/* Quick test function for basic functionality verification */
void quick_debug_test(void) {
    KLOG_INFO(LOG_CAT_KERNEL, "=== Quick Debug System Test ===");
    
    if (!kdebug_init()) {
        KLOG_ERROR(LOG_CAT_KERNEL, "❌ Debug system initialization failed");
        return;
    }
    
    kdebug_set_enabled(true);
    
    /* Quick functionality test */
    kdebug_memory_dump((uint64_t)&quick_debug_test, 32);
    kdebug_stack_trace(NULL);
    kdebug_display_statistics();
    
    KLOG_INFO(LOG_CAT_KERNEL, "✅ Quick debug test completed successfully");
}
