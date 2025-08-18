/*
 * IKOS Operating System - Notification System Test Suite
 * Issue #42: Notifications & System Alerts
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "notifications.h"

/* Test framework macros */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            test_failures++; \
        } else { \
            printf("PASS: %s\n", message); \
            test_passes++; \
        } \
    } while (0)

#define TEST_BEGIN(name) \
    printf("\n=== Running Test: %s ===\n", name)

static int test_passes = 0;
static int test_failures = 0;

/* ================================
 * Test Helper Functions  
 * ================================ */

static void reset_test_counts(void) {
    test_passes = 0;
    test_failures = 0;
}

static void print_test_summary(const char* suite_name) {
    printf("\n=== %s Test Summary ===\n", suite_name);
    printf("Passed: %d\n", test_passes);
    printf("Failed: %d\n", test_failures);
    printf("Total:  %d\n", test_passes + test_failures);
    printf("Success Rate: %.1f%%\n", 
           (test_passes + test_failures > 0) ? 
           (100.0 * test_passes / (test_passes + test_failures)) : 0.0);
}

/* Test callback functions */
static bool callback_triggered = false;
static notification_t* callback_notification = NULL;
static notification_state_t callback_old_state = NOTIFICATION_STATE_PENDING;
static notification_state_t callback_new_state = NOTIFICATION_STATE_PENDING;

static void test_event_callback(notification_t* notification, 
                               notification_state_t old_state,
                               notification_state_t new_state,
                               void* user_data) {
    callback_triggered = true;
    callback_notification = notification;
    callback_old_state = old_state;
    callback_new_state = new_state;
    (void)user_data;
}

static bool alert_callback_triggered = false;
static system_alert_type_t alert_callback_type = SYSTEM_ALERT_CUSTOM;
static char alert_callback_message[256] = {0};

static void test_alert_callback(system_alert_type_t alert_type,
                               const char* message,
                               void* user_data) {
    alert_callback_triggered = true;
    alert_callback_type = alert_type;
    if (message) {
        strncpy(alert_callback_message, message, sizeof(alert_callback_message) - 1);
    }
    (void)user_data;
}

static bool action_callback_triggered = false;
static uint32_t action_callback_notification_id = 0;
static char action_callback_action_name[64] = {0};

static void test_action_callback(uint32_t notification_id, 
                                const char* action_name,
                                void* user_data) {
    action_callback_triggered = true;
    action_callback_notification_id = notification_id;
    if (action_name) {
        strncpy(action_callback_action_name, action_name, sizeof(action_callback_action_name) - 1);
    }
    (void)user_data;
}

/* ================================
 * Basic API Tests
 * ================================ */

static void test_notification_system_init(void) {
    TEST_BEGIN("Notification System Initialization");
    
    /* Test basic initialization */
    int result = notification_system_init(NULL);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Basic initialization should succeed");
    
    /* Test double initialization */
    result = notification_system_init(NULL);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Double initialization should not fail");
    
    /* Test with custom config */
    notification_config_t config = {0};
    config.notifications_enabled = true;
    config.sounds_enabled = false;
    config.max_visible_notifications = 5;
    config.default_timeout_ms = 3000;
    
    notification_system_shutdown();
    result = notification_system_init(&config);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Initialization with config should succeed");
}

static void test_basic_notification_sending(void) {
    TEST_BEGIN("Basic Notification Sending");
    
    /* Test simple notification */
    uint32_t id1 = notification_send("Test Title", "Test Message", NOTIFICATION_TYPE_INFO);
    TEST_ASSERT(id1 > 0, "Simple notification should return valid ID");
    
    /* Test advanced notification */
    uint32_t id2 = notification_send_advanced("Advanced Title", "Advanced Message", 
                                              "TestApp", "/icon.png",
                                              NOTIFICATION_TYPE_WARNING,
                                              NOTIFICATION_PRIORITY_HIGH, 5000);
    TEST_ASSERT(id2 > 0, "Advanced notification should return valid ID");
    TEST_ASSERT(id2 != id1, "Notification IDs should be unique");
    
    /* Test invalid parameters */
    uint32_t id3 = notification_send(NULL, "Message", NOTIFICATION_TYPE_INFO);
    TEST_ASSERT(id3 == 0, "NULL title should return 0");
    
    uint32_t id4 = notification_send("Title", NULL, NOTIFICATION_TYPE_INFO);
    TEST_ASSERT(id4 == 0, "NULL message should return 0");
}

static void test_notification_retrieval(void) {
    TEST_BEGIN("Notification Retrieval");
    
    /* Send test notification */
    uint32_t id = notification_send("Retrieval Test", "Test Message", NOTIFICATION_TYPE_SUCCESS);
    TEST_ASSERT(id > 0, "Test notification should be sent successfully");
    
    /* Retrieve notification */
    notification_t* notification = notification_get_by_id(id);
    TEST_ASSERT(notification != NULL, "Should be able to retrieve notification by ID");
    
    if (notification) {
        TEST_ASSERT(notification->id == id, "Retrieved notification should have correct ID");
        TEST_ASSERT(strcmp(notification->title, "Retrieval Test") == 0, "Title should match");
        TEST_ASSERT(strcmp(notification->message, "Test Message") == 0, "Message should match");
        TEST_ASSERT(notification->type == NOTIFICATION_TYPE_SUCCESS, "Type should match");
    }
    
    /* Test invalid ID */
    notification_t* invalid = notification_get_by_id(99999);
    TEST_ASSERT(invalid == NULL, "Invalid ID should return NULL");
}

static void test_notification_dismissal(void) {
    TEST_BEGIN("Notification Dismissal");
    
    /* Send test notification */
    uint32_t id = notification_send("Dismissal Test", "Test Message", NOTIFICATION_TYPE_ERROR);
    TEST_ASSERT(id > 0, "Test notification should be sent successfully");
    
    /* Dismiss notification */
    int result = notification_dismiss(id);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Dismissal should succeed");
    
    /* Verify state change */
    notification_t* notification = notification_get_by_id(id);
    TEST_ASSERT(notification != NULL, "Notification should still exist after dismissal");
    if (notification) {
        TEST_ASSERT(notification->state == NOTIFICATION_STATE_DISMISSED, 
                   "State should be DISMISSED");
    }
    
    /* Test invalid ID dismissal */
    result = notification_dismiss(99999);
    TEST_ASSERT(result == NOTIFICATION_ERROR_NOT_FOUND, "Invalid ID dismissal should fail");
}

/* ================================
 * Advanced Feature Tests
 * ================================ */

static void test_notification_with_actions(void) {
    TEST_BEGIN("Notification with Actions");
    
    /* Create actions */
    notification_action_t actions[2];
    strcpy(actions[0].name, "approve");
    strcpy(actions[0].label, "Approve");
    actions[0].is_destructive = false;
    actions[0].callback = test_action_callback;
    actions[0].user_data = NULL;
    
    strcpy(actions[1].name, "reject");
    strcpy(actions[1].label, "Reject");
    actions[1].is_destructive = true;
    actions[1].callback = test_action_callback;
    actions[1].user_data = NULL;
    
    /* Send notification with actions */
    uint32_t id = notification_send_with_actions("Action Test", "Choose an action",
                                                "TestApp", NOTIFICATION_TYPE_INFO,
                                                actions, 2);
    TEST_ASSERT(id > 0, "Notification with actions should be sent successfully");
    
    /* Verify actions */
    notification_t* notification = notification_get_by_id(id);
    TEST_ASSERT(notification != NULL, "Should be able to retrieve notification");
    if (notification) {
        TEST_ASSERT(notification->action_count == 2, "Should have 2 actions");
        TEST_ASSERT(strcmp(notification->actions[0].label, "Approve") == 0, "First action label should match");
        TEST_ASSERT(strcmp(notification->actions[1].label, "Reject") == 0, "Second action label should match");
    }
}

static void test_system_alerts(void) {
    TEST_BEGIN("System Alerts");
    
    /* Test generic system alert */
    uint32_t id1 = notification_send_system_alert(SYSTEM_ALERT_LOW_MEMORY,
                                                  "Memory Alert", "System is low on memory");
    TEST_ASSERT(id1 > 0, "Generic system alert should be sent successfully");
    
    /* Test specific alerts */
    uint32_t id2 = notification_alert_low_memory(1024 * 1024 * 512, 1024 * 1024 * 1024 * 4); /* 512MB available, 4GB total */
    TEST_ASSERT(id2 > 0, "Low memory alert should be sent successfully");
    
    uint32_t id3 = notification_alert_low_battery(15);
    TEST_ASSERT(id3 > 0, "Low battery alert should be sent successfully");
    
    uint32_t id4 = notification_alert_service_failed("TestService", "Connection timeout");
    TEST_ASSERT(id4 > 0, "Service failed alert should be sent successfully");
    
    /* Verify alert properties */
    notification_t* notification = notification_get_by_id(id1);
    TEST_ASSERT(notification != NULL, "Should be able to retrieve system alert");
    if (notification) {
        TEST_ASSERT(strcmp(notification->app_name, "System Alert") == 0, "App name should be 'System Alert'");
    }
}

static void test_event_callbacks(void) {
    TEST_BEGIN("Event Callbacks");
    
    /* Reset callback state */
    callback_triggered = false;
    callback_notification = NULL;
    
    /* Register callback */
    int result = notification_register_event_callback(test_event_callback, NULL);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Callback registration should succeed");
    
    /* Send notification to trigger callback */
    uint32_t id = notification_send("Callback Test", "Test Message", NOTIFICATION_TYPE_INFO);
    TEST_ASSERT(id > 0, "Test notification should be sent");
    
    /* Update display to trigger state changes */
    notification_update_display();
    
    /* Dismiss to trigger callback */
    notification_dismiss(id);
    
    /* Verify callback was triggered */
    TEST_ASSERT(callback_triggered, "Event callback should be triggered");
    TEST_ASSERT(callback_notification != NULL, "Callback should receive notification");
    
    if (callback_notification) {
        TEST_ASSERT(callback_notification->id == id, "Callback notification ID should match");
    }
    
    /* Unregister callback */
    result = notification_unregister_event_callback(test_event_callback);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Callback unregistration should succeed");
}

static void test_alert_callbacks(void) {
    TEST_BEGIN("Alert Callbacks");
    
    /* Reset callback state */
    alert_callback_triggered = false;
    alert_callback_type = SYSTEM_ALERT_CUSTOM;
    memset(alert_callback_message, 0, sizeof(alert_callback_message));
    
    /* Register callback */
    int result = notification_register_alert_callback(test_alert_callback, NULL);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Alert callback registration should succeed");
    
    /* Send system alert to trigger callback */
    notification_send_system_alert(SYSTEM_ALERT_LOW_BATTERY, "Test Alert", "Test alert message");
    
    /* Verify callback was triggered */
    TEST_ASSERT(alert_callback_triggered, "Alert callback should be triggered");
    TEST_ASSERT(alert_callback_type == SYSTEM_ALERT_LOW_BATTERY, "Alert type should match");
    TEST_ASSERT(strlen(alert_callback_message) > 0, "Alert message should be received");
    
    /* Unregister callback */
    result = notification_unregister_alert_callback(test_alert_callback);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Alert callback unregistration should succeed");
}

/* ================================
 * Configuration Tests
 * ================================ */

static void test_configuration_management(void) {
    TEST_BEGIN("Configuration Management");
    
    /* Get current config */
    notification_config_t config;
    int result = notification_get_config(&config);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Should be able to get config");
    
    /* Modify config */
    config.notifications_enabled = false;
    config.sounds_enabled = false;
    config.max_visible_notifications = 3;
    
    result = notification_set_config(&config);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Should be able to set config");
    
    /* Verify config change */
    notification_config_t new_config;
    result = notification_get_config(&new_config);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Should be able to get updated config");
    TEST_ASSERT(new_config.notifications_enabled == false, "Notifications should be disabled");
    TEST_ASSERT(new_config.sounds_enabled == false, "Sounds should be disabled");
    TEST_ASSERT(new_config.max_visible_notifications == 3, "Max notifications should be 3");
    
    /* Test notification blocking when disabled */
    uint32_t id = notification_send("Blocked Test", "Should not appear", NOTIFICATION_TYPE_INFO);
    TEST_ASSERT(id == 0, "Notification should be blocked when disabled");
    
    /* Re-enable notifications */
    config.notifications_enabled = true;
    notification_set_config(&config);
}

static void test_statistics(void) {
    TEST_BEGIN("Statistics");
    
    /* Get initial stats */
    notification_stats_t stats;
    int result = notification_get_stats(&stats);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Should be able to get stats");
    
    uint32_t initial_sent = stats.total_notifications_sent;
    uint32_t initial_dismissed = stats.total_notifications_dismissed;
    
    /* Send and dismiss some notifications */
    uint32_t id1 = notification_send("Stats Test 1", "Message 1", NOTIFICATION_TYPE_INFO);
    uint32_t id2 = notification_send("Stats Test 2", "Message 2", NOTIFICATION_TYPE_SUCCESS);
    
    notification_dismiss(id1);
    notification_dismiss(id2);
    
    /* Get updated stats */
    result = notification_get_stats(&stats);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Should be able to get updated stats");
    TEST_ASSERT(stats.total_notifications_sent == initial_sent + 2, "Sent count should increase by 2");
    TEST_ASSERT(stats.total_notifications_dismissed == initial_dismissed + 2, "Dismissed count should increase by 2");
}

/* ================================
 * Utility Function Tests
 * ================================ */

static void test_utility_functions(void) {
    TEST_BEGIN("Utility Functions");
    
    /* Test type to string conversion */
    const char* type_str = notification_type_to_string(NOTIFICATION_TYPE_WARNING);
    TEST_ASSERT(strcmp(type_str, "warning") == 0, "Type to string conversion should work");
    
    /* Test priority to string conversion */
    const char* priority_str = notification_priority_to_string(NOTIFICATION_PRIORITY_HIGH);
    TEST_ASSERT(strcmp(priority_str, "high") == 0, "Priority to string conversion should work");
    
    /* Test state to string conversion */
    const char* state_str = notification_state_to_string(NOTIFICATION_STATE_VISIBLE);
    TEST_ASSERT(strcmp(state_str, "visible") == 0, "State to string conversion should work");
    
    /* Test type to color conversion */
    gui_color_t color = notification_type_to_color(NOTIFICATION_TYPE_ERROR);
    TEST_ASSERT(color != 0, "Type to color conversion should return valid color");
    
    /* Test sound playing */
    int result = notification_play_sound(NOTIFICATION_TYPE_SUCCESS);
    TEST_ASSERT(result == NOTIFICATION_SUCCESS, "Sound playing should not fail");
}

/* ================================
 * GUI Integration Tests
 * ================================ */

static void test_gui_integration(void) {
    TEST_BEGIN("GUI Integration");
    
    /* Test panel visibility */
    notification_show_panel(true);
    TEST_ASSERT(notification_is_panel_visible() == true, "Panel should be visible after showing");
    
    notification_show_panel(false);
    TEST_ASSERT(notification_is_panel_visible() == false, "Panel should be hidden after hiding");
    
    /* Test display update */
    uint32_t id = notification_send("GUI Test", "Test Message", NOTIFICATION_TYPE_INFO);
    notification_update_display();
    
    notification_t* notification = notification_get_by_id(id);
    TEST_ASSERT(notification != NULL, "Notification should exist");
    if (notification) {
        TEST_ASSERT(notification->state == NOTIFICATION_STATE_VISIBLE, "Notification should be visible after update");
    }
}

/* ================================
 * Stress Tests
 * ================================ */

static void test_maximum_notifications(void) {
    TEST_BEGIN("Maximum Notifications");
    
    /* Send maximum number of notifications */
    uint32_t ids[NOTIFICATION_MAX_ACTIVE];
    int successful_sends = 0;
    
    for (int i = 0; i < NOTIFICATION_MAX_ACTIVE + 5; i++) {
        char title[64], message[128];
        snprintf(title, sizeof(title), "Test Notification %d", i);
        snprintf(message, sizeof(message), "This is test message number %d", i);
        
        uint32_t id = notification_send(title, message, NOTIFICATION_TYPE_INFO);
        if (id > 0 && successful_sends < NOTIFICATION_MAX_ACTIVE) {
            ids[successful_sends] = id;
            successful_sends++;
        }
    }
    
    TEST_ASSERT(successful_sends == NOTIFICATION_MAX_ACTIVE, "Should be able to send up to maximum notifications");
    
    /* Clean up */
    for (int i = 0; i < successful_sends; i++) {
        notification_dismiss(ids[i]);
    }
}

static void test_rapid_notification_sending(void) {
    TEST_BEGIN("Rapid Notification Sending");
    
    const int rapid_count = 50;
    uint32_t ids[rapid_count];
    int successful_sends = 0;
    
    /* Send notifications rapidly */
    for (int i = 0; i < rapid_count; i++) {
        char title[64];
        snprintf(title, sizeof(title), "Rapid Test %d", i);
        
        uint32_t id = notification_send(title, "Rapid test message", NOTIFICATION_TYPE_INFO);
        if (id > 0) {
            ids[successful_sends] = id;
            successful_sends++;
        }
    }
    
    TEST_ASSERT(successful_sends > 0, "Should be able to send notifications rapidly");
    
    /* Clean up */
    notification_dismiss_all();
}

/* ================================
 * Main Test Runner
 * ================================ */

static void run_all_tests(void) {
    printf("IKOS Notification System Test Suite\n");
    printf("===================================\n");
    
    reset_test_counts();
    
    /* Initialize notification system for testing */
    notification_system_init(NULL);
    
    /* Run test suites */
    test_notification_system_init();
    test_basic_notification_sending();
    test_notification_retrieval();
    test_notification_dismissal();
    test_notification_with_actions();
    test_system_alerts();
    test_event_callbacks();
    test_alert_callbacks();
    test_configuration_management();
    test_statistics();
    test_utility_functions();
    test_gui_integration();
    test_maximum_notifications();
    test_rapid_notification_sending();
    
    /* Print final results */
    print_test_summary("Notification System");
    
    /* Cleanup */
    notification_system_shutdown();
}

/* ================================
 * Simple implementations for missing functions
 * ================================ */

static int snprintf(char* buffer, size_t size, const char* format, ...) {
    /* Simple implementation for testing */
    if (buffer && size > 0 && format) {
        strncpy(buffer, format, size - 1);
        buffer[size - 1] = '\0';
        return strlen(buffer);
    }
    return 0;
}

/* Test main function */
int main(void) {
    run_all_tests();
    return (test_failures == 0) ? 0 : 1;
}

/* For integration with IKOS test framework */
int test_notifications(void) {
    run_all_tests();
    return (test_failures == 0) ? NOTIFICATION_SUCCESS : NOTIFICATION_ERROR_UNKNOWN;
}
