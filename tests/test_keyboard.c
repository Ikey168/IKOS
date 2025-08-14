/* IKOS Keyboard Driver Test Suite
 * Comprehensive tests for keyboard driver functionality
 */

#include "keyboard.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

/* Test framework */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        printf("✓ PASS: %s\n", message); \
        tests_passed++; \
    } else { \
        printf("✗ FAIL: %s\n", message); \
        tests_failed++; \
    } \
} while(0)

#define TEST_START(test_name) \
    printf("\n=== Running %s ===\n", test_name)

#define TEST_END() \
    printf("--- Test completed ---\n")

/* Test data and state */
static bool test_listener_called = false;
static key_event_t test_listener_event;

/* Forward declarations */
static void test_keyboard_initialization(void);
static void test_keyboard_hardware_interface(void);
static void test_scancode_translation(void);
static void test_key_mapping(void);
static void test_modifier_handling(void);
static void test_buffer_management(void);
static void test_event_listeners(void);
static void test_led_control(void);
static void test_system_calls(void);
static void test_error_conditions(void);
static void test_keyboard_cleanup_test(void);

/* Test event listener callback */
static void test_event_listener(const key_event_t* event, void* user_data) {
    (void)user_data;
    test_listener_called = true;
    test_listener_event = *event;
}

/* ================================
 * Test Implementation
 * ================================ */

/**
 * Main test runner
 */
int main(void) {
    printf("IKOS Keyboard Driver Test Suite\n");
    printf("===============================\n");
    
    /* Run all tests */
    test_keyboard_initialization();
    test_keyboard_hardware_interface();
    test_scancode_translation();
    test_key_mapping();
    test_modifier_handling();
    test_buffer_management();
    test_event_listeners();
    test_led_control();
    test_system_calls();
    test_error_conditions();
    test_keyboard_cleanup_test();
    
    /* Print summary */
    printf("\n===============================\n");
    printf("Test Summary:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Success rate: %.1f%%\n", 
           tests_run > 0 ? (float)tests_passed / tests_run * 100.0f : 0.0f);
    
    return tests_failed > 0 ? 1 : 0;
}

/**
 * Test keyboard driver initialization
 */
static void test_keyboard_initialization(void) {
    TEST_START("Keyboard Initialization");
    
    /* Test initial state (should not be initialized) */
    keyboard_state_t state;
    keyboard_get_stats(&state);
    TEST_ASSERT(true, "Getting stats before init should not crash");
    
    /* Test initialization */
    int result = keyboard_init();
    TEST_ASSERT(result == KEYBOARD_SUCCESS, "Keyboard initialization should succeed");
    
    /* Test double initialization */
    result = keyboard_init();
    TEST_ASSERT(result == KEYBOARD_SUCCESS, "Double initialization should be safe");
    
    /* Test self-test */
    result = keyboard_self_test();
    TEST_ASSERT(result == KEYBOARD_SUCCESS, "Keyboard self-test should pass");
    
    /* Test getting stats after init */
    keyboard_get_stats(&state);
    TEST_ASSERT(state.buffer_count == 0, "Buffer should be empty after init");
    TEST_ASSERT(state.modifiers == 0, "Modifiers should be clear after init");
    TEST_ASSERT(state.total_events == 0, "Event counter should be zero after init");
    
    TEST_END();
}

/**
 * Test keyboard hardware interface
 */
static void test_keyboard_hardware_interface(void) {
    TEST_START("Keyboard Hardware Interface");
    
    /* Test hardware interface functions exist */
    TEST_ASSERT(keyboard_read_data != NULL, "keyboard_read_data function should exist");
    TEST_ASSERT(keyboard_write_data != NULL, "keyboard_write_data function should exist");
    TEST_ASSERT(keyboard_read_status != NULL, "keyboard_read_status function should exist");
    TEST_ASSERT(keyboard_write_command != NULL, "keyboard_write_command function should exist");
    TEST_ASSERT(keyboard_wait_ready != NULL, "keyboard_wait_ready function should exist");
    
    /* Test status reading (should not crash) */
    uint8_t status = keyboard_read_status();
    TEST_ASSERT(true, "Reading keyboard status should not crash");
    (void)status;  /* Suppress unused variable warning */
    
    /* Test wait function (should not hang indefinitely) */
    keyboard_wait_ready();
    TEST_ASSERT(true, "keyboard_wait_ready should complete");
    
    TEST_END();
}

/**
 * Test scancode translation
 */
static void test_scancode_translation(void) {
    TEST_START("Scancode Translation");
    
    /* Test basic scancode translation */
    uint8_t keycode = keyboard_scancode_to_keycode(0x1E);  /* 'A' key */
    TEST_ASSERT(keycode == KEY_A, "Scancode 0x1E should translate to KEY_A");
    
    keycode = keyboard_scancode_to_keycode(0x10);  /* 'Q' key */
    TEST_ASSERT(keycode == KEY_Q, "Scancode 0x10 should translate to KEY_Q");
    
    keycode = keyboard_scancode_to_keycode(0x39);  /* Space key */
    TEST_ASSERT(keycode == KEY_SPACE, "Scancode 0x39 should translate to KEY_SPACE");
    
    keycode = keyboard_scancode_to_keycode(0x1C);  /* Enter key */
    TEST_ASSERT(keycode == KEY_ENTER, "Scancode 0x1C should translate to KEY_ENTER");
    
    /* Test release scancode */
    keycode = keyboard_scancode_to_keycode(0x9E);  /* 'A' key release */
    TEST_ASSERT(keycode == KEY_A, "Release scancode 0x9E should translate to KEY_A");
    
    /* Test invalid scancode */
    keycode = keyboard_scancode_to_keycode(0xFF);
    TEST_ASSERT(keycode == 0, "Invalid scancode should return 0");
    
    /* Test extended scancode prefix */
    keycode = keyboard_scancode_to_keycode(SCANCODE_EXTENDED_PREFIX);
    TEST_ASSERT(keycode == 0, "Extended prefix should return 0");
    
    TEST_END();
}

/**
 * Test key mapping and ASCII translation
 */
static void test_key_mapping(void) {
    TEST_START("Key Mapping");
    
    /* Test normal key mapping */
    char ascii = keyboard_keycode_to_ascii(KEY_A, 0);
    TEST_ASSERT(ascii == 'a', "KEY_A without modifiers should give 'a'");
    
    ascii = keyboard_keycode_to_ascii(KEY_A, MOD_SHIFT);
    TEST_ASSERT(ascii == 'A', "KEY_A with shift should give 'A'");
    
    ascii = keyboard_keycode_to_ascii(KEY_A, MOD_CAPS);
    TEST_ASSERT(ascii == 'A', "KEY_A with caps lock should give 'A'");
    
    ascii = keyboard_keycode_to_ascii(KEY_A, MOD_SHIFT | MOD_CAPS);
    TEST_ASSERT(ascii == 'a', "KEY_A with shift and caps should give 'a'");
    
    /* Test number keys */
    ascii = keyboard_keycode_to_ascii(KEY_1, 0);
    TEST_ASSERT(ascii == '1', "KEY_1 without modifiers should give '1'");
    
    ascii = keyboard_keycode_to_ascii(KEY_1, MOD_SHIFT);
    TEST_ASSERT(ascii == '!', "KEY_1 with shift should give '!'");
    
    /* Test special keys */
    ascii = keyboard_keycode_to_ascii(KEY_SPACE, 0);
    TEST_ASSERT(ascii == ' ', "KEY_SPACE should give space character");
    
    ascii = keyboard_keycode_to_ascii(KEY_ENTER, 0);
    TEST_ASSERT(ascii == '\n', "KEY_ENTER should give newline character");
    
    ascii = keyboard_keycode_to_ascii(KEY_TAB, 0);
    TEST_ASSERT(ascii == '\t', "KEY_TAB should give tab character");
    
    ascii = keyboard_keycode_to_ascii(KEY_BACKSPACE, 0);
    TEST_ASSERT(ascii == '\b', "KEY_BACKSPACE should give backspace character");
    
    /* Test modifier key detection */
    bool is_modifier = keyboard_is_modifier_key(KEY_LSHIFT);
    TEST_ASSERT(is_modifier, "KEY_LSHIFT should be detected as modifier");
    
    is_modifier = keyboard_is_modifier_key(KEY_LCTRL);
    TEST_ASSERT(is_modifier, "KEY_LCTRL should be detected as modifier");
    
    is_modifier = keyboard_is_modifier_key(KEY_A);
    TEST_ASSERT(!is_modifier, "KEY_A should not be detected as modifier");
    
    TEST_END();
}

/**
 * Test modifier key handling
 */
static void test_modifier_handling(void) {
    TEST_START("Modifier Handling");
    
    /* Reset modifier state */
    keyboard_set_modifiers(0);
    uint8_t modifiers = keyboard_get_modifiers();
    TEST_ASSERT(modifiers == 0, "Modifiers should be clear after reset");
    
    /* Test setting modifiers */
    keyboard_set_modifiers(MOD_SHIFT | MOD_CTRL);
    modifiers = keyboard_get_modifiers();
    TEST_ASSERT(modifiers == (MOD_SHIFT | MOD_CTRL), "Should be able to set multiple modifiers");
    
    /* Test individual modifier flags */
    keyboard_set_modifiers(MOD_SHIFT);
    modifiers = keyboard_get_modifiers();
    TEST_ASSERT((modifiers & MOD_SHIFT) != 0, "Shift modifier should be set");
    TEST_ASSERT((modifiers & MOD_CTRL) == 0, "Ctrl modifier should not be set");
    
    keyboard_set_modifiers(MOD_CTRL);
    modifiers = keyboard_get_modifiers();
    TEST_ASSERT((modifiers & MOD_CTRL) != 0, "Ctrl modifier should be set");
    TEST_ASSERT((modifiers & MOD_SHIFT) == 0, "Shift modifier should not be set");
    
    keyboard_set_modifiers(MOD_ALT);
    modifiers = keyboard_get_modifiers();
    TEST_ASSERT((modifiers & MOD_ALT) != 0, "Alt modifier should be set");
    
    keyboard_set_modifiers(MOD_CAPS);
    modifiers = keyboard_get_modifiers();
    TEST_ASSERT((modifiers & MOD_CAPS) != 0, "Caps modifier should be set");
    
    /* Reset for next tests */
    keyboard_set_modifiers(0);
    
    TEST_END();
}

/**
 * Test buffer management
 */
static void test_buffer_management(void) {
    TEST_START("Buffer Management");
    
    /* Clear buffer and test empty state */
    keyboard_clear_buffer();
    TEST_ASSERT(!keyboard_has_data(), "Buffer should be empty after clear");
    
    key_event_t event;
    int result = keyboard_get_event_nonblock(&event);
    TEST_ASSERT(result == KEYBOARD_ERROR_BUFFER_EMPTY, "Should get buffer empty error");
    
    /* Test peek on empty buffer */
    result = keyboard_peek_event(&event);
    TEST_ASSERT(result == KEYBOARD_ERROR_BUFFER_EMPTY, "Peek should fail on empty buffer");
    
    /* Test non-blocking character read on empty buffer */
    int ch = keyboard_getchar_nonblock();
    TEST_ASSERT(ch == -1, "Should get -1 for no character available");
    
    /* Test buffer state after operations */
    keyboard_state_t state;
    keyboard_get_stats(&state);
    TEST_ASSERT(state.buffer_count == 0, "Buffer count should be 0");
    TEST_ASSERT(state.buffer_head == 0, "Buffer head should be 0");
    TEST_ASSERT(state.buffer_tail == 0, "Buffer tail should be 0");
    
    TEST_END();
}

/**
 * Test event listener system
 */
static void test_event_listeners(void) {
    TEST_START("Event Listeners");
    
    /* Reset test state */
    test_listener_called = false;
    memset(&test_listener_event, 0, sizeof(test_listener_event));
    
    /* Test registering listener */
    int listener_id = keyboard_register_listener(test_event_listener, NULL);
    TEST_ASSERT(listener_id >= 0, "Should be able to register event listener");
    
    /* Test invalid listener registration */
    int invalid_id = keyboard_register_listener(NULL, NULL);
    TEST_ASSERT(invalid_id == KEYBOARD_ERROR_INVALID_PARAM, "Should reject NULL callback");
    
    /* Test listener enable/disable */
    int result = keyboard_set_listener_enabled(listener_id, false);
    TEST_ASSERT(result == KEYBOARD_SUCCESS, "Should be able to disable listener");
    
    result = keyboard_set_listener_enabled(listener_id, true);
    TEST_ASSERT(result == KEYBOARD_SUCCESS, "Should be able to enable listener");
    
    /* Test invalid listener operations */
    result = keyboard_set_listener_enabled(-1, true);
    TEST_ASSERT(result == KEYBOARD_ERROR_LISTENER_INVALID, "Should reject invalid listener ID");
    
    result = keyboard_set_listener_enabled(999, true);
    TEST_ASSERT(result == KEYBOARD_ERROR_LISTENER_INVALID, "Should reject out-of-range listener ID");
    
    /* Test unregistering listener */
    result = keyboard_unregister_listener(listener_id);
    TEST_ASSERT(result == KEYBOARD_SUCCESS, "Should be able to unregister listener");
    
    /* Test unregistering invalid listener */
    result = keyboard_unregister_listener(-1);
    TEST_ASSERT(result == KEYBOARD_ERROR_LISTENER_INVALID, "Should reject invalid listener ID for unregister");
    
    TEST_END();
}

/**
 * Test LED control
 */
static void test_led_control(void) {
    TEST_START("LED Control");
    
    /* Test setting LEDs */
    keyboard_set_leds(0);
    uint8_t leds = keyboard_get_leds();
    TEST_ASSERT(leds == 0, "All LEDs should be off after setting to 0");
    
    keyboard_set_leds(LED_CAPS_LOCK);
    TEST_ASSERT(true, "Setting caps lock LED should not crash");
    
    keyboard_set_leds(LED_NUM_LOCK);
    TEST_ASSERT(true, "Setting num lock LED should not crash");
    
    keyboard_set_leds(LED_SCROLL_LOCK);
    TEST_ASSERT(true, "Setting scroll lock LED should not crash");
    
    keyboard_set_leds(LED_CAPS_LOCK | LED_NUM_LOCK | LED_SCROLL_LOCK);
    TEST_ASSERT(true, "Setting all LEDs should not crash");
    
    /* Reset LEDs */
    keyboard_set_leds(0);
    
    TEST_END();
}

/**
 * Test system call interface
 */
static void test_system_calls(void) {
    TEST_START("System Calls");
    
    /* Test system call functions exist */
    TEST_ASSERT(sys_keyboard_read != NULL, "sys_keyboard_read should exist");
    TEST_ASSERT(sys_keyboard_poll != NULL, "sys_keyboard_poll should exist");
    TEST_ASSERT(sys_keyboard_ioctl != NULL, "sys_keyboard_ioctl should exist");
    
    /* Test polling */
    int poll_result = sys_keyboard_poll();
    TEST_ASSERT(poll_result >= 0, "Keyboard poll should return valid result");
    
    /* Test reading with invalid parameters */
    int read_result = sys_keyboard_read(NULL, 0);
    TEST_ASSERT(read_result == KEYBOARD_ERROR_INVALID_PARAM, "Should reject NULL buffer");
    
    /* Test ioctl with get modifiers */
    uint8_t modifiers;
    int ioctl_result = sys_keyboard_ioctl(KEYBOARD_IOCTL_GET_MODIFIERS, &modifiers);
    TEST_ASSERT(ioctl_result == KEYBOARD_SUCCESS, "Get modifiers ioctl should succeed");
    
    /* Test ioctl with clear buffer */
    ioctl_result = sys_keyboard_ioctl(KEYBOARD_IOCTL_CLEAR_BUFFER, NULL);
    TEST_ASSERT(ioctl_result == KEYBOARD_SUCCESS, "Clear buffer ioctl should succeed");
    
    /* Test ioctl with invalid command */
    ioctl_result = sys_keyboard_ioctl(999, NULL);
    TEST_ASSERT(ioctl_result == KEYBOARD_ERROR_INVALID_PARAM, "Should reject invalid ioctl command");
    
    /* Test ioctl with get state */
    keyboard_state_t state;
    ioctl_result = sys_keyboard_ioctl(KEYBOARD_IOCTL_GET_STATE, &state);
    TEST_ASSERT(ioctl_result == KEYBOARD_SUCCESS, "Get state ioctl should succeed");
    
    /* Test ioctl with set LEDs */
    uint8_t led_state = 0;
    ioctl_result = sys_keyboard_ioctl(KEYBOARD_IOCTL_SET_LEDS, &led_state);
    TEST_ASSERT(ioctl_result == KEYBOARD_SUCCESS, "Set LEDs ioctl should succeed");
    
    TEST_END();
}

/**
 * Test error conditions and edge cases
 */
static void test_error_conditions(void) {
    TEST_START("Error Conditions");
    
    /* Test operations with NULL parameters */
    keyboard_state_t* null_state = NULL;
    keyboard_get_stats(null_state);
    TEST_ASSERT(true, "Getting stats with NULL should not crash");
    
    key_event_t* null_event = NULL;
    int result = keyboard_get_event_nonblock(null_event);
    TEST_ASSERT(result == KEYBOARD_ERROR_INVALID_PARAM, "Should reject NULL event pointer");
    
    result = keyboard_peek_event(null_event);
    TEST_ASSERT(result == KEYBOARD_ERROR_INVALID_PARAM, "Should reject NULL event pointer for peek");
    
    /* Test invalid scancode translation */
    uint8_t keycode = keyboard_scancode_to_keycode(0xFF);
    TEST_ASSERT(keycode == 0, "Invalid scancode should return 0");
    
    /* Test ASCII translation with invalid keycode */
    char ascii = keyboard_keycode_to_ascii(0xFF, 0);
    TEST_ASSERT(ascii == 0, "Invalid keycode should return 0");
    
    /* Test modifier operations */
    bool is_modifier = keyboard_is_modifier_key(0xFF);
    TEST_ASSERT(!is_modifier, "Invalid keycode should not be modifier");
    
    /* Test hardware interface edge cases */
    keyboard_wait_ready();  /* Should not hang */
    TEST_ASSERT(true, "keyboard_wait_ready should handle timeout");
    
    TEST_END();
}

/**
 * Test keyboard cleanup
 */
static void test_keyboard_cleanup_test(void) {
    TEST_START("Keyboard Cleanup");
    
    /* Test cleanup */
    keyboard_cleanup();
    TEST_ASSERT(true, "Keyboard cleanup should complete without error");
    
    /* Test operations after cleanup */
    keyboard_state_t state;
    keyboard_get_stats(&state);
    TEST_ASSERT(true, "Getting stats after cleanup should not crash");
    
    /* Test re-initialization after cleanup */
    int result = keyboard_init();
    TEST_ASSERT(result == KEYBOARD_SUCCESS, "Should be able to re-initialize after cleanup");
    
    /* Final cleanup */
    keyboard_cleanup();
    
    TEST_END();
}

/* Simple memory and string functions for testing */
void* memset(void* ptr, int value, size_t size) {
    unsigned char* p = (unsigned char*)ptr;
    while (size--) *p++ = (unsigned char)value;
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t size) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (size--) *d++ = *s++;
    return dest;
}
