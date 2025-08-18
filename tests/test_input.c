/* IKOS Input System Test
 * Comprehensive test suite for the unified input system
 */

#include "input.h"
#include "input_events.h"
#include "input_syscalls.h"
#include "memory.h"
#include <string.h>
#include <stdio.h>

/* Test configuration */
#define TEST_MAX_EVENTS 16
#define TEST_QUEUE_SIZE 32

/* Test statistics */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test macros */
#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("✅ PASS: %s\n", message); \
        } else { \
            tests_failed++; \
            printf("❌ FAIL: %s\n", message); \
        } \
    } while(0)

#define TEST_SECTION(name) \
    printf("\n📋 Testing %s\n", name); \
    printf("==========================================\n")

/* ================================
 * Test Helper Functions
 * ================================ */

static void test_input_manager_basic(void);
static void test_event_queue_operations(void);
static void test_event_validation(void);
static void test_device_registration(void);
static void test_application_registration(void);
static void test_focus_management(void);
static void test_event_filtering(void);
static void test_system_calls(void);
static void print_test_summary(void);

/* Mock device for testing */
typedef struct {
    input_device_t device;
    bool configured;
    bool reset_called;
    int events_generated;
} test_device_t;

static int test_device_read_event(input_device_t* dev, input_event_t* event);
static int test_device_configure(input_device_t* dev, void* config);
static int test_device_reset(input_device_t* dev);
static void test_device_cleanup(input_device_t* dev);

/* ================================
 * Main Test Function
 * ================================ */

int test_input_system(void) {
    printf("\n🧪 IKOS Input System Test Suite\n");
    printf("============================================\n");
    
    /* Initialize input system */
    int result = input_init();
    TEST_ASSERT(result == INPUT_SUCCESS, "Input system initialization");
    
    /* Run test suites */
    test_input_manager_basic();
    test_event_queue_operations();
    test_event_validation();
    test_device_registration();
    test_application_registration();
    test_focus_management();
    test_event_filtering();
    test_system_calls();
    
    /* Cleanup */
    input_cleanup();
    
    /* Print summary */
    print_test_summary();
    
    return (tests_failed == 0) ? 0 : -1;
}

/* ================================
 * Test Implementations
 * ================================ */

static void test_input_manager_basic(void) {
    TEST_SECTION("Input Manager Basic Operations");
    
    /* Test getting initial state */
    input_state_t state;
    int result = input_get_state(&state);
    TEST_ASSERT(result == INPUT_SUCCESS, "Get initial input state");
    TEST_ASSERT(state.events_processed == 0, "Initial events processed count");
    TEST_ASSERT(state.active_devices == 0, "Initial active devices count");
    TEST_ASSERT(state.registered_apps == 0, "Initial registered apps count");
    
    /* Test mouse position functions */
    int32_t x, y;
    result = input_get_mouse_position(&x, &y);
    TEST_ASSERT(result == INPUT_SUCCESS, "Get mouse position");
    TEST_ASSERT(x == 400 && y == 300, "Default mouse position");
    
    result = input_set_mouse_position(100, 200);
    TEST_ASSERT(result == INPUT_SUCCESS, "Set mouse position");
    
    result = input_get_mouse_position(&x, &y);
    TEST_ASSERT(result == INPUT_SUCCESS && x == 100 && y == 200, "Verify mouse position set");
}

static void test_event_queue_operations(void) {
    TEST_SECTION("Event Queue Operations");
    
    /* Create test queue */
    input_event_t* queue = input_event_queue_create(TEST_QUEUE_SIZE);
    TEST_ASSERT(queue != NULL, "Event queue creation");
    
    size_t head = 0, tail = 0, count = 0;
    
    /* Test empty queue */
    TEST_ASSERT(input_event_queue_is_empty(count), "Empty queue detection");
    TEST_ASSERT(!input_event_queue_is_full(count, TEST_QUEUE_SIZE), "Non-full queue detection");
    
    /* Create test event */
    input_event_t test_event;
    memset(&test_event, 0, sizeof(input_event_t));
    test_event.type = INPUT_EVENT_KEY_PRESS;
    test_event.device_id = 1;
    test_event.data.key.keycode = 'A';
    test_event.data.key.modifiers = INPUT_MOD_SHIFT;
    test_event.timestamp = input_get_timestamp();
    
    /* Test push operation */
    bool success = input_event_queue_push(queue, TEST_QUEUE_SIZE, &head, &tail, &count, &test_event);
    TEST_ASSERT(success, "Event queue push");
    TEST_ASSERT(count == 1, "Queue count after push");
    TEST_ASSERT(!input_event_queue_is_empty(count), "Non-empty queue after push");
    
    /* Test peek operation */
    input_event_t peeked_event;
    success = input_event_queue_peek(queue, TEST_QUEUE_SIZE, head, tail, count, &peeked_event);
    TEST_ASSERT(success, "Event queue peek");
    TEST_ASSERT(peeked_event.type == test_event.type, "Peeked event type matches");
    TEST_ASSERT(count == 1, "Queue count unchanged after peek");
    
    /* Test pop operation */
    input_event_t popped_event;
    success = input_event_queue_pop(queue, TEST_QUEUE_SIZE, &head, &tail, &count, &popped_event);
    TEST_ASSERT(success, "Event queue pop");
    TEST_ASSERT(popped_event.type == test_event.type, "Popped event type matches");
    TEST_ASSERT(popped_event.data.key.keycode == test_event.data.key.keycode, "Popped event data matches");
    TEST_ASSERT(count == 0, "Queue count after pop");
    TEST_ASSERT(input_event_queue_is_empty(count), "Empty queue after pop");
    
    /* Test queue full condition */
    for (size_t i = 0; i < TEST_QUEUE_SIZE; i++) {
        input_event_queue_push(queue, TEST_QUEUE_SIZE, &head, &tail, &count, &test_event);
    }
    TEST_ASSERT(input_event_queue_is_full(count, TEST_QUEUE_SIZE), "Full queue detection");
    
    /* Test push to full queue fails */
    success = input_event_queue_push(queue, TEST_QUEUE_SIZE, &head, &tail, &count, &test_event);
    TEST_ASSERT(!success, "Push to full queue fails");
    
    /* Cleanup */
    input_event_queue_destroy(queue);
}

static void test_event_validation(void) {
    TEST_SECTION("Event Validation");
    
    /* Test valid key event */
    input_event_t key_event;
    memset(&key_event, 0, sizeof(input_event_t));
    key_event.type = INPUT_EVENT_KEY_PRESS;
    key_event.device_id = 1;
    key_event.data.key.keycode = 'A';
    key_event.data.key.modifiers = INPUT_MOD_SHIFT;
    
    TEST_ASSERT(input_event_validate(&key_event), "Valid key event");
    TEST_ASSERT(input_event_validate_key(&key_event), "Valid key event data");
    
    /* Test invalid key event (no device ID) */
    key_event.device_id = 0;
    TEST_ASSERT(!input_event_validate(&key_event), "Invalid key event (no device ID)");
    key_event.device_id = 1;
    
    /* Test valid mouse event */
    input_event_t mouse_event;
    memset(&mouse_event, 0, sizeof(input_event_t));
    mouse_event.type = INPUT_EVENT_MOUSE_MOVE;
    mouse_event.device_id = 2;
    mouse_event.data.mouse_move.x = 100;
    mouse_event.data.mouse_move.y = 200;
    mouse_event.data.mouse_move.delta_x = 10;
    mouse_event.data.mouse_move.delta_y = 20;
    
    TEST_ASSERT(input_event_validate(&mouse_event), "Valid mouse move event");
    TEST_ASSERT(input_event_validate_mouse(&mouse_event), "Valid mouse event data");
    
    /* Test character conversion */
    key_event.data.key.keycode = 'a';
    key_event.data.key.modifiers = 0;
    char c = input_event_key_to_char(&key_event);
    TEST_ASSERT(c == 'a', "Key to character conversion (lowercase)");
    
    key_event.data.key.modifiers = INPUT_MOD_SHIFT;
    c = input_event_key_to_char(&key_event);
    TEST_ASSERT(c == 'A', "Key to character conversion (uppercase)");
    
    /* Test event classification */
    key_event.data.key.keycode = 'x';
    TEST_ASSERT(input_event_is_printable(&key_event), "Printable character detection");
    
    key_event.data.key.keycode = 16; /* Shift */
    TEST_ASSERT(input_event_is_modifier(&key_event), "Modifier key detection");
    
    key_event.data.key.keycode = 37; /* Left arrow */
    TEST_ASSERT(input_event_is_navigation(&key_event), "Navigation key detection");
    
    key_event.data.key.keycode = 112; /* F1 */
    TEST_ASSERT(input_event_is_function_key(&key_event), "Function key detection");
}

static void test_device_registration(void) {
    TEST_SECTION("Device Registration");
    
    /* Create test device */
    test_device_t* test_dev = (test_device_t*)kmalloc(sizeof(test_device_t));
    TEST_ASSERT(test_dev != NULL, "Test device allocation");
    
    memset(test_dev, 0, sizeof(test_device_t));
    strncpy(test_dev->device.name, "Test Device", INPUT_DEVICE_NAME_LEN - 1);
    test_dev->device.type = INPUT_DEVICE_KEYBOARD;
    test_dev->device.capabilities = INPUT_CAP_KEYS;
    test_dev->device.device_data = test_dev;
    test_dev->device.read_event = test_device_read_event;
    test_dev->device.configure = test_device_configure;
    test_dev->device.reset = test_device_reset;
    test_dev->device.cleanup = test_device_cleanup;
    
    /* Test device registration */
    int result = input_register_device(&test_dev->device);
    TEST_ASSERT(result == INPUT_SUCCESS, "Device registration");
    TEST_ASSERT(test_dev->device.device_id != 0, "Device ID assigned");
    
    /* Test finding registered device */
    input_device_t* found_device = input_find_device(test_dev->device.device_id);
    TEST_ASSERT(found_device == &test_dev->device, "Find registered device");
    
    /* Test device capabilities */
    uint32_t capabilities = input_get_device_capabilities(test_dev->device.device_id);
    TEST_ASSERT(capabilities == INPUT_CAP_KEYS, "Device capabilities");
    
    /* Test device configuration */
    result = test_dev->device.configure(&test_dev->device, NULL);
    TEST_ASSERT(result == INPUT_SUCCESS && test_dev->configured, "Device configuration");
    
    /* Test device reset */
    result = test_dev->device.reset(&test_dev->device);
    TEST_ASSERT(result == INPUT_SUCCESS && test_dev->reset_called, "Device reset");
    
    /* Test device unregistration */
    result = input_unregister_device(test_dev->device.device_id);
    TEST_ASSERT(result == INPUT_SUCCESS, "Device unregistration");
    
    /* Test finding unregistered device fails */
    found_device = input_find_device(test_dev->device.device_id);
    TEST_ASSERT(found_device == NULL, "Find unregistered device fails");
    
    /* Note: test_dev is freed by input_unregister_device via cleanup callback */
}

static void test_application_registration(void) {
    TEST_SECTION("Application Registration");
    
    uint32_t test_pid = 100;
    uint32_t subscription_mask = INPUT_SUBSCRIBE_KEYBOARD | INPUT_SUBSCRIBE_MOUSE;
    
    /* Test application registration */
    int result = input_register_app(test_pid, subscription_mask);
    TEST_ASSERT(result == INPUT_SUCCESS, "Application registration");
    
    /* Test getting state with registered app */
    input_state_t state;
    result = input_get_state(&state);
    TEST_ASSERT(result == INPUT_SUCCESS && state.registered_apps == 1, "Registered apps count");
    
    /* Test re-registration (should update subscription) */
    result = input_register_app(test_pid, INPUT_SUBSCRIBE_KEYBOARD);
    TEST_ASSERT(result == INPUT_SUCCESS, "Application re-registration");
    
    /* Test polling with no events */
    input_event_t events[TEST_MAX_EVENTS];
    result = input_poll_events(test_pid, events, TEST_MAX_EVENTS);
    TEST_ASSERT(result == 0, "Poll with no events");
    
    /* Test application unregistration */
    result = input_unregister_app(test_pid);
    TEST_ASSERT(result == INPUT_SUCCESS, "Application unregistration");
    
    /* Test operations on unregistered app fail */
    result = input_poll_events(test_pid, events, TEST_MAX_EVENTS);
    TEST_ASSERT(result == INPUT_ERROR_APP_NOT_FOUND, "Poll unregistered app fails");
}

static void test_focus_management(void) {
    TEST_SECTION("Focus Management");
    
    uint32_t app1_pid = 101;
    uint32_t app2_pid = 102;
    
    /* Register two applications */
    int result = input_register_app(app1_pid, INPUT_SUBSCRIBE_ALL);
    TEST_ASSERT(result == INPUT_SUCCESS, "Register app1");
    
    result = input_register_app(app2_pid, INPUT_SUBSCRIBE_ALL);
    TEST_ASSERT(result == INPUT_SUCCESS, "Register app2");
    
    /* First app should get focus automatically */
    uint32_t focused = input_get_focus();
    TEST_ASSERT(focused == app1_pid, "First app gets focus");
    
    /* Test focus switching */
    result = input_set_focus(app2_pid);
    TEST_ASSERT(result == INPUT_SUCCESS, "Switch focus to app2");
    
    focused = input_get_focus();
    TEST_ASSERT(focused == app2_pid, "Focus switched to app2");
    
    /* Test setting focus to unregistered app fails */
    result = input_set_focus(999);
    TEST_ASSERT(result == INPUT_ERROR_APP_NOT_FOUND, "Focus unregistered app fails");
    
    /* Test releasing focus */
    result = input_set_focus(0);
    TEST_ASSERT(result == INPUT_SUCCESS, "Release focus");
    
    focused = input_get_focus();
    TEST_ASSERT(focused == 0, "Focus released");
    
    /* Cleanup */
    input_unregister_app(app1_pid);
    input_unregister_app(app2_pid);
}

static void test_event_filtering(void) {
    TEST_SECTION("Event Filtering");
    
    /* Test type filtering */
    input_event_t key_event;
    memset(&key_event, 0, sizeof(input_event_t));
    key_event.type = INPUT_EVENT_KEY_PRESS;
    key_event.device_id = 1;
    
    TEST_ASSERT(input_event_filter_keyboard_only(&key_event, NULL), "Keyboard filter accepts key event");
    TEST_ASSERT(!input_event_filter_mouse_only(&key_event, NULL), "Mouse filter rejects key event");
    
    input_event_t mouse_event;
    memset(&mouse_event, 0, sizeof(input_event_t));
    mouse_event.type = INPUT_EVENT_MOUSE_MOVE;
    mouse_event.device_id = 2;
    
    TEST_ASSERT(!input_event_filter_keyboard_only(&mouse_event, NULL), "Keyboard filter rejects mouse event");
    TEST_ASSERT(input_event_filter_mouse_only(&mouse_event, NULL), "Mouse filter accepts mouse event");
    
    /* Test device filtering */
    uint32_t target_device = 1;
    TEST_ASSERT(input_event_filter_by_device(&key_event, &target_device), "Device filter matches");
    
    target_device = 2;
    TEST_ASSERT(!input_event_filter_by_device(&key_event, &target_device), "Device filter rejects");
    
    /* Test type mask filtering */
    uint32_t type_mask = (1 << INPUT_EVENT_KEY_PRESS) | (1 << INPUT_EVENT_KEY_RELEASE);
    TEST_ASSERT(input_event_filter_by_type(&key_event, &type_mask), "Type mask accepts key event");
    
    type_mask = (1 << INPUT_EVENT_MOUSE_MOVE);
    TEST_ASSERT(!input_event_filter_by_type(&key_event, &type_mask), "Type mask rejects key event");
}

static void test_system_calls(void) {
    TEST_SECTION("System Call Interface");
    
    /* Note: These tests use the system call functions directly
       In a real system, they would go through the syscall dispatcher */
    
    /* Test registration syscall */
    long result = sys_input_register(INPUT_SUBSCRIBE_ALL);
    TEST_ASSERT(result == INPUT_SUCCESS, "Syscall input register");
    
    /* Test getting state syscall */
    input_state_t state;
    result = sys_input_get_state(&state);
    TEST_ASSERT(result == INPUT_SUCCESS, "Syscall get state");
    
    /* Test focus request syscall */
    result = sys_input_request_focus();
    TEST_ASSERT(result == INPUT_SUCCESS, "Syscall request focus");
    
    /* Test polling syscall */
    input_event_t events[TEST_MAX_EVENTS];
    result = sys_input_poll(events, TEST_MAX_EVENTS);
    TEST_ASSERT(result >= 0, "Syscall poll events");
    
    /* Test focus release syscall */
    result = sys_input_release_focus();
    TEST_ASSERT(result == INPUT_SUCCESS, "Syscall release focus");
    
    /* Test unregistration syscall */
    result = sys_input_unregister();
    TEST_ASSERT(result == INPUT_SUCCESS, "Syscall input unregister");
}

static void print_test_summary(void) {
    printf("\n📊 Test Summary\n");
    printf("============================================\n");
    printf("Tests Run:    %d\n", tests_run);
    printf("Tests Passed: %d\n", tests_passed);
    printf("Tests Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("🎉 All tests passed!\n");
    } else {
        printf("⚠️  %d test(s) failed\n", tests_failed);
    }
    
    printf("Success Rate: %.1f%%\n", tests_run ? (100.0 * tests_passed / tests_run) : 0.0);
}

/* ================================
 * Mock Device Implementation
 * ================================ */

static int test_device_read_event(input_device_t* dev, input_event_t* event) {
    (void)dev; (void)event; /* Unused */
    return INPUT_SUCCESS;
}

static int test_device_configure(input_device_t* dev, void* config) {
    (void)config; /* Unused */
    
    test_device_t* test_dev = (test_device_t*)dev->device_data;
    if (test_dev) {
        test_dev->configured = true;
    }
    return INPUT_SUCCESS;
}

static int test_device_reset(input_device_t* dev) {
    test_device_t* test_dev = (test_device_t*)dev->device_data;
    if (test_dev) {
        test_dev->reset_called = true;
    }
    return INPUT_SUCCESS;
}

static void test_device_cleanup(input_device_t* dev) {
    test_device_t* test_dev = (test_device_t*)dev->device_data;
    if (test_dev) {
        kfree(test_dev);
    }
}
