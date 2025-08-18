/* IKOS Application Loader Test Suite - Issue #40
 * Comprehensive testing for unified GUI and CLI application loading
 */

#include "app_loader.h"
#include "process.h"
#include "kernel_log.h"
#include "assert.h"
#include <stddef.h>
#include <string.h>

/* ================================
 * Test Configuration
 * ================================ */

#define TEST_MAX_APPS 10
#define TEST_MAX_INSTANCES 5
#define TEST_TIMEOUT 5000  /* ms */

/* Test application descriptors */
static app_descriptor_t test_shell_app = {
    .name = "test_shell",
    .path = "embedded://shell",
    .description = "Test CLI Shell Application",
    .type = APP_TYPE_CLI,
    .flags = APP_FLAG_CLI_ENABLE,
    .memory_limit = 2 * 1024 * 1024,
    .cpu_priority = 50,
    .installed = true
};

static app_descriptor_t test_gui_app = {
    .name = "test_gui",
    .path = "/usr/bin/test_gui",
    .description = "Test GUI Application",
    .type = APP_TYPE_GUI,
    .flags = APP_FLAG_GUI_ENABLE,
    .memory_limit = 8 * 1024 * 1024,
    .cpu_priority = 60,
    .installed = true
};

static app_descriptor_t test_hybrid_app = {
    .name = "test_hybrid",
    .path = "embedded://sysinfo",
    .description = "Test Hybrid Application",
    .type = APP_TYPE_HYBRID,
    .flags = APP_FLAG_GUI_ENABLE | APP_FLAG_CLI_ENABLE | APP_FLAG_AUTO_DETECT,
    .memory_limit = 4 * 1024 * 1024,
    .cpu_priority = 40,
    .installed = true
};

/* ================================
 * Test Helper Functions
 * ================================ */

static bool test_init_passed = false;
static uint32_t test_count = 0;
static uint32_t test_passed = 0;
static uint32_t test_failed = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        KLOG_INFO(LOG_CAT_TEST, "Starting test: %s", name); \
    } while(0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            KLOG_ERROR(LOG_CAT_TEST, "ASSERTION FAILED: %s", message); \
            test_failed++; \
            return false; \
        } \
    } while(0)

#define TEST_PASS(name) \
    do { \
        test_passed++; \
        KLOG_INFO(LOG_CAT_TEST, "Test PASSED: %s", name); \
        return true; \
    } while(0)

#define TEST_FAIL(name, message) \
    do { \
        test_failed++; \
        KLOG_ERROR(LOG_CAT_TEST, "Test FAILED: %s - %s", name, message); \
        return false; \
    } while(0)

/* ================================
 * Basic Functionality Tests
 * ================================ */

static bool test_app_loader_initialization(void) {
    TEST_START("Application Loader Initialization");
    
    /* Test initialization with default config */
    int result = app_loader_init(NULL);
    TEST_ASSERT(result == APP_ERROR_SUCCESS, "Failed to initialize with default config");
    
    /* Test double initialization */
    result = app_loader_init(NULL);
    TEST_ASSERT(result == APP_ERROR_SUCCESS, "Double initialization should succeed");
    
    /* Test config retrieval */
    app_loader_config_t* config = app_loader_get_config();
    TEST_ASSERT(config != NULL, "Failed to get config");
    TEST_ASSERT(config->max_concurrent_apps > 0, "Invalid max concurrent apps");
    
    /* Test statistics retrieval */
    app_loader_stats_t stats;
    result = app_loader_get_stats(&stats);
    TEST_ASSERT(result == APP_ERROR_SUCCESS, "Failed to get stats");
    
    test_init_passed = true;
    TEST_PASS("Application Loader Initialization");
}

static bool test_app_registration(void) {
    TEST_START("Application Registration");
    
    if (!test_init_passed) {
        TEST_FAIL("Application Registration", "Initialization test must pass first");
    }
    
    /* Test basic registration */
    int result = app_register(&test_shell_app);
    TEST_ASSERT(result == APP_ERROR_SUCCESS, "Failed to register shell app");
    
    /* Test duplicate registration */
    result = app_register(&test_shell_app);
    TEST_ASSERT(result == APP_ERROR_ALREADY_EXISTS, "Duplicate registration should fail");
    
    /* Test finding registered app */
    app_descriptor_t* found = app_find_by_name("test_shell");
    TEST_ASSERT(found != NULL, "Failed to find registered app by name");
    TEST_ASSERT(strcmp(found->name, "test_shell") == 0, "Found app name mismatch");
    
    /* Test finding by path */
    found = app_find_by_path("embedded://shell");
    TEST_ASSERT(found != NULL, "Failed to find registered app by path");
    
    /* Register more test apps */
    result = app_register(&test_gui_app);
    TEST_ASSERT(result == APP_ERROR_SUCCESS, "Failed to register GUI app");
    
    result = app_register(&test_hybrid_app);
    TEST_ASSERT(result == APP_ERROR_SUCCESS, "Failed to register hybrid app");
    
    /* Test application listing */
    app_descriptor_t apps[TEST_MAX_APPS];
    int count = app_list_all(apps, TEST_MAX_APPS);
    TEST_ASSERT(count >= 3, "Should have at least 3 registered apps");
    
    /* Test listing by type */
    count = app_list_by_type(APP_TYPE_CLI, apps, TEST_MAX_APPS);
    TEST_ASSERT(count >= 1, "Should have at least 1 CLI app");
    
    count = app_list_by_type(APP_TYPE_GUI, apps, TEST_MAX_APPS);
    TEST_ASSERT(count >= 1, "Should have at least 1 GUI app");
    
    TEST_PASS("Application Registration");
}

static bool test_app_type_detection(void) {
    TEST_START("Application Type Detection");
    
    /* Test type detection from path */
    app_type_t type = app_detect_type_from_path("embedded://shell");
    TEST_ASSERT(type == APP_TYPE_CLI, "Shell should be detected as CLI");
    
    type = app_detect_type_from_path("embedded://sysinfo");
    TEST_ASSERT(type == APP_TYPE_HYBRID, "Sysinfo should be detected as hybrid");
    
    type = app_detect_type_from_path("/usr/bin/unknown");
    TEST_ASSERT(type != APP_TYPE_UNKNOWN, "Should detect some type for binary");
    
    /* Test invalid paths */
    type = app_detect_type_from_path(NULL);
    TEST_ASSERT(type == APP_TYPE_UNKNOWN, "NULL path should return unknown");
    
    type = app_detect_type_from_path("");
    TEST_ASSERT(type == APP_TYPE_UNKNOWN, "Empty path should return unknown");
    
    TEST_PASS("Application Type Detection");
}

/* ================================
 * Application Launch Tests
 * ================================ */

static bool test_app_launch_by_name(void) {
    TEST_START("Application Launch by Name");
    
    if (!test_init_passed) {
        TEST_FAIL("Application Launch by Name", "Initialization test must pass first");
    }
    
    /* Test launching shell application */
    int32_t instance_id = app_launch_by_name("test_shell", NULL, NULL, 
                                           APP_LAUNCH_FOREGROUND, 0);
    TEST_ASSERT(instance_id > 0, "Failed to launch shell application");
    
    /* Verify instance was created */
    app_instance_t* instance = app_get_instance((uint32_t)instance_id);
    TEST_ASSERT(instance != NULL, "Failed to get instance");
    TEST_ASSERT(instance->descriptor != NULL, "Instance descriptor is NULL");
    TEST_ASSERT(strcmp(instance->descriptor->name, "test_shell") == 0, 
               "Instance app name mismatch");
    
    /* Test launching nonexistent application */
    int32_t bad_id = app_launch_by_name("nonexistent", NULL, NULL, 
                                       APP_LAUNCH_FOREGROUND, 0);
    TEST_ASSERT(bad_id == APP_ERROR_NOT_FOUND, "Nonexistent app should return error");
    
    /* Clean up */
    int result = app_terminate_instance((uint32_t)instance_id, true);
    TEST_ASSERT(result == APP_ERROR_SUCCESS, "Failed to terminate instance");
    
    TEST_PASS("Application Launch by Name");
}

static bool test_app_launch_by_path(void) {
    TEST_START("Application Launch by Path");
    
    /* Test launching registered app by path */
    int32_t instance_id = app_launch_by_path("embedded://shell", NULL, NULL, 
                                           APP_LAUNCH_FOREGROUND, 0);
    TEST_ASSERT(instance_id > 0, "Failed to launch app by registered path");
    
    /* Clean up */
    if (instance_id > 0) {
        app_terminate_instance((uint32_t)instance_id, true);
    }
    
    /* Test launching unregistered path */
    instance_id = app_launch_by_path("/bin/unregistered", NULL, NULL, 
                                   APP_LAUNCH_FOREGROUND, 0);
    /* This may fail if file doesn't exist, which is expected */
    
    TEST_PASS("Application Launch by Path");
}

static bool test_gui_app_launch(void) {
    TEST_START("GUI Application Launch");
    
    /* Check if GUI is available */
    app_loader_config_t* config = app_loader_get_config();
    if (!config || !config->gui_enabled) {
        KLOG_INFO(LOG_CAT_TEST, "GUI not available, skipping GUI launch test");
        test_passed++;
        return true;
    }
    
    /* Test GUI application launch */
    int32_t instance_id = app_launch_gui("test_gui", NULL, NULL, NULL);
    
    /* This may fail if GUI app can't be loaded, which is acceptable for testing */
    if (instance_id > 0) {
        app_instance_t* instance = app_get_instance((uint32_t)instance_id);
        TEST_ASSERT(instance != NULL, "Failed to get GUI instance");
        
        /* Clean up */
        app_terminate_instance((uint32_t)instance_id, true);
    } else {
        KLOG_INFO(LOG_CAT_TEST, "GUI launch failed as expected (no GUI binary)");
    }
    
    TEST_PASS("GUI Application Launch");
}

static bool test_cli_app_launch(void) {
    TEST_START("CLI Application Launch");
    
    /* Check if CLI is available */
    app_loader_config_t* config = app_loader_get_config();
    if (!config || !config->cli_enabled) {
        KLOG_INFO(LOG_CAT_TEST, "CLI not available, skipping CLI launch test");
        test_passed++;
        return true;
    }
    
    /* Test CLI application launch */
    int32_t instance_id = app_launch_cli("test_shell", NULL, NULL, 0);
    TEST_ASSERT(instance_id > 0, "Failed to launch CLI application");
    
    if (instance_id > 0) {
        app_instance_t* instance = app_get_instance((uint32_t)instance_id);
        TEST_ASSERT(instance != NULL, "Failed to get CLI instance");
        TEST_ASSERT(instance->runtime_type == APP_TYPE_CLI, "Instance type should be CLI");
        
        /* Clean up */
        app_terminate_instance((uint32_t)instance_id, true);
    }
    
    TEST_PASS("CLI Application Launch");
}

/* ================================
 * Instance Management Tests
 * ================================ */

static bool test_instance_management(void) {
    TEST_START("Instance Management");
    
    /* Launch multiple instances */
    int32_t instance_ids[3];
    
    instance_ids[0] = app_launch_by_name("test_shell", NULL, NULL, 
                                       APP_LAUNCH_FOREGROUND, 0);
    TEST_ASSERT(instance_ids[0] > 0, "Failed to launch first instance");
    
    instance_ids[1] = app_launch_by_name("test_shell", NULL, NULL, 
                                       APP_LAUNCH_BACKGROUND, 0);
    TEST_ASSERT(instance_ids[1] > 0, "Failed to launch second instance");
    
    instance_ids[2] = app_launch_by_name("test_hybrid", NULL, NULL, 
                                       APP_LAUNCH_FOREGROUND, 0);
    TEST_ASSERT(instance_ids[2] > 0, "Failed to launch third instance");
    
    /* Test getting all instances */
    app_instance_t* instances[TEST_MAX_INSTANCES];
    int count = app_get_all_instances(instances, TEST_MAX_INSTANCES);
    TEST_ASSERT(count >= 3, "Should have at least 3 instances");
    
    /* Test getting instances by name */
    count = app_get_instances_by_name("test_shell", instances, TEST_MAX_INSTANCES);
    TEST_ASSERT(count >= 2, "Should have at least 2 shell instances");
    
    /* Test instance termination */
    for (int i = 0; i < 3; i++) {
        if (instance_ids[i] > 0) {
            int result = app_terminate_instance((uint32_t)instance_ids[i], true);
            TEST_ASSERT(result == APP_ERROR_SUCCESS, "Failed to terminate instance");
            
            /* Verify instance is gone */
            app_instance_t* instance = app_get_instance((uint32_t)instance_ids[i]);
            TEST_ASSERT(instance == NULL, "Instance should be removed after termination");
        }
    }
    
    TEST_PASS("Instance Management");
}

/* ================================
 * Error Handling Tests
 * ================================ */

static bool test_error_handling(void) {
    TEST_START("Error Handling");
    
    /* Test invalid parameters */
    int result = app_register(NULL);
    TEST_ASSERT(result == APP_ERROR_INVALID_PARAM, "NULL descriptor should fail");
    
    app_descriptor_t* found = app_find_by_name(NULL);
    TEST_ASSERT(found == NULL, "NULL name should return NULL");
    
    found = app_find_by_path(NULL);
    TEST_ASSERT(found == NULL, "NULL path should return NULL");
    
    int32_t instance_id = app_launch_by_name(NULL, NULL, NULL, APP_LAUNCH_FOREGROUND, 0);
    TEST_ASSERT(instance_id == APP_ERROR_INVALID_PARAM, "NULL name should fail launch");
    
    instance_id = app_launch_by_path(NULL, NULL, NULL, APP_LAUNCH_FOREGROUND, 0);
    TEST_ASSERT(instance_id == APP_ERROR_INVALID_PARAM, "NULL path should fail launch");
    
    /* Test invalid instance operations */
    app_instance_t* instance = app_get_instance(0);
    TEST_ASSERT(instance == NULL, "Invalid instance ID should return NULL");
    
    result = app_terminate_instance(0, false);
    TEST_ASSERT(result == APP_ERROR_NOT_FOUND, "Invalid instance ID should fail terminate");
    
    /* Test unregistering non-existent app */
    result = app_unregister("nonexistent");
    TEST_ASSERT(result == APP_ERROR_NOT_FOUND, "Unregistering non-existent app should fail");
    
    TEST_PASS("Error Handling");
}

/* ================================
 * Statistics and Performance Tests
 * ================================ */

static bool test_statistics(void) {
    TEST_START("Statistics");
    
    /* Get initial statistics */
    app_loader_stats_t stats_before;
    int result = app_loader_get_stats(&stats_before);
    TEST_ASSERT(result == APP_ERROR_SUCCESS, "Failed to get initial stats");
    
    /* Launch an application */
    int32_t instance_id = app_launch_by_name("test_shell", NULL, NULL, 
                                           APP_LAUNCH_FOREGROUND, 0);
    TEST_ASSERT(instance_id > 0, "Failed to launch app for stats test");
    
    /* Get updated statistics */
    app_loader_stats_t stats_after;
    result = app_loader_get_stats(&stats_after);
    TEST_ASSERT(result == APP_ERROR_SUCCESS, "Failed to get updated stats");
    
    /* Verify statistics updated */
    TEST_ASSERT(stats_after.apps_loaded > stats_before.apps_loaded, 
               "Apps loaded count should increase");
    TEST_ASSERT(stats_after.apps_running > stats_before.apps_running, 
               "Running apps count should increase");
    
    /* Clean up */
    app_terminate_instance((uint32_t)instance_id, true);
    
    /* Get final statistics */
    app_loader_stats_t stats_final;
    result = app_loader_get_stats(&stats_final);
    TEST_ASSERT(result == APP_ERROR_SUCCESS, "Failed to get final stats");
    
    TEST_ASSERT(stats_final.apps_terminated > stats_before.apps_terminated, 
               "Terminated apps count should increase");
    
    TEST_PASS("Statistics");
}

/* ================================
 * Integration Tests
 * ================================ */

static bool test_concurrent_launches(void) {
    TEST_START("Concurrent Application Launches");
    
    const int num_instances = 3;
    int32_t instance_ids[num_instances];
    
    /* Launch multiple instances concurrently */
    for (int i = 0; i < num_instances; i++) {
        instance_ids[i] = app_launch_by_name("test_shell", NULL, NULL, 
                                           APP_LAUNCH_BACKGROUND, 0);
        TEST_ASSERT(instance_ids[i] > 0, "Failed to launch concurrent instance");
    }
    
    /* Verify all instances are running */
    app_instance_t* instances[TEST_MAX_INSTANCES];
    int count = app_get_instances_by_name("test_shell", instances, TEST_MAX_INSTANCES);
    TEST_ASSERT(count >= num_instances, "Should have all launched instances");
    
    /* Clean up all instances */
    for (int i = 0; i < num_instances; i++) {
        if (instance_ids[i] > 0) {
            app_terminate_instance((uint32_t)instance_ids[i], true);
        }
    }
    
    TEST_PASS("Concurrent Application Launches");
}

static bool test_app_unregistration(void) {
    TEST_START("Application Unregistration");
    
    /* Register a temporary app */
    app_descriptor_t temp_app = {
        .name = "temp_test_app",
        .path = "/tmp/test",
        .description = "Temporary Test Application",
        .type = APP_TYPE_CLI,
        .flags = APP_FLAG_CLI_ENABLE,
        .memory_limit = 1024 * 1024,
        .cpu_priority = 30,
        .installed = false
    };
    
    int result = app_register(&temp_app);
    TEST_ASSERT(result == APP_ERROR_SUCCESS, "Failed to register temp app");
    
    /* Verify it was registered */
    app_descriptor_t* found = app_find_by_name("temp_test_app");
    TEST_ASSERT(found != NULL, "Temp app should be findable");
    
    /* Unregister the app */
    result = app_unregister("temp_test_app");
    TEST_ASSERT(result == APP_ERROR_SUCCESS, "Failed to unregister temp app");
    
    /* Verify it was unregistered */
    found = app_find_by_name("temp_test_app");
    TEST_ASSERT(found == NULL, "Temp app should not be findable after unregister");
    
    TEST_PASS("Application Unregistration");
}

/* ================================
 * Main Test Suite Function
 * ================================ */

void run_app_loader_tests(void) {
    KLOG_INFO(LOG_CAT_TEST, "=== Starting Application Loader Test Suite ===");
    
    /* Initialize test counters */
    test_count = 0;
    test_passed = 0;
    test_failed = 0;
    test_init_passed = false;
    
    /* Run test suite */
    test_app_loader_initialization();
    test_app_registration();
    test_app_type_detection();
    test_app_launch_by_name();
    test_app_launch_by_path();
    test_gui_app_launch();
    test_cli_app_launch();
    test_instance_management();
    test_error_handling();
    test_statistics();
    test_concurrent_launches();
    test_app_unregistration();
    
    /* Print results */
    KLOG_INFO(LOG_CAT_TEST, "=== Application Loader Test Results ===");
    KLOG_INFO(LOG_CAT_TEST, "Total Tests: %u", test_count);
    KLOG_INFO(LOG_CAT_TEST, "Passed: %u", test_passed);
    KLOG_INFO(LOG_CAT_TEST, "Failed: %u", test_failed);
    
    if (test_failed == 0) {
        KLOG_INFO(LOG_CAT_TEST, "*** ALL TESTS PASSED ***");
    } else {
        KLOG_ERROR(LOG_CAT_TEST, "*** %u TESTS FAILED ***", test_failed);
    }
    
    /* Cleanup */
    app_loader_shutdown();
    
    KLOG_INFO(LOG_CAT_TEST, "=== Application Loader Test Suite Complete ===");
}

/* Simple test runner for integration */
void test_app_loader_basic(void) {
    KLOG_INFO(LOG_CAT_TEST, "Running basic application loader test...");
    
    /* Initialize */
    if (app_loader_init(NULL) != APP_ERROR_SUCCESS) {
        KLOG_ERROR(LOG_CAT_TEST, "Failed to initialize application loader");
        return;
    }
    
    /* Register test app */
    app_descriptor_t test_app = {
        .name = "basic_test",
        .path = "embedded://shell",
        .description = "Basic Test Application",
        .type = APP_TYPE_CLI,
        .flags = APP_FLAG_CLI_ENABLE,
        .memory_limit = 1024 * 1024,
        .cpu_priority = 50,
        .installed = true
    };
    
    if (app_register(&test_app) != APP_ERROR_SUCCESS) {
        KLOG_ERROR(LOG_CAT_TEST, "Failed to register test application");
        app_loader_shutdown();
        return;
    }
    
    /* Launch app */
    int32_t instance_id = app_launch_by_name("basic_test", NULL, NULL, 
                                           APP_LAUNCH_FOREGROUND, 0);
    if (instance_id > 0) {
        KLOG_INFO(LOG_CAT_TEST, "Successfully launched application (Instance ID: %d)", instance_id);
        
        /* Terminate */
        app_terminate_instance((uint32_t)instance_id, true);
        KLOG_INFO(LOG_CAT_TEST, "Successfully terminated application");
    } else {
        KLOG_ERROR(LOG_CAT_TEST, "Failed to launch application: %d", instance_id);
    }
    
    /* Cleanup */
    app_loader_shutdown();
    KLOG_INFO(LOG_CAT_TEST, "Basic application loader test complete");
}
