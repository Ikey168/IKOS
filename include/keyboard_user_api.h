/* IKOS Keyboard User-Space API Library
 * Provides convenient functions for user-space applications to access keyboard
 */

#ifndef KEYBOARD_USER_API_H
#define KEYBOARD_USER_API_H

#include "keyboard.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* System call interface (implemented by libc) */
extern long syscall(long number, ...);

/* System call numbers */
#define SYS_KEYBOARD_READ       140
#define SYS_KEYBOARD_POLL       141
#define SYS_KEYBOARD_IOCTL      142
#define SYS_KEYBOARD_GETCHAR    143

/* ================================
 * User-Space Keyboard API
 * ================================ */

/**
 * Read keyboard events from kernel driver
 * @param buffer: Buffer to store key events
 * @param count: Size of buffer in bytes
 * @return: Number of bytes read, or negative error code
 */
static inline int keyboard_read_events(key_event_t* buffer, size_t count) {
    return (int)syscall(SYS_KEYBOARD_READ, (long)buffer, (long)count, 0, 0, 0);
}

/**
 * Poll keyboard for available data
 * @return: 1 if data available, 0 if no data, negative on error
 */
static inline int keyboard_poll_data(void) {
    return (int)syscall(SYS_KEYBOARD_POLL, 0, 0, 0, 0, 0);
}

/**
 * Perform keyboard control operations
 * @param cmd: Control command
 * @param arg: Command argument
 * @return: 0 on success, negative error code on failure
 */
static inline int keyboard_control(int cmd, void* arg) {
    return (int)syscall(SYS_KEYBOARD_IOCTL, (long)cmd, (long)arg, 0, 0, 0);
}

/**
 * Get character from keyboard (blocking)
 * @return: Character read from keyboard
 */
static inline char keyboard_get_char(void) {
    return (char)syscall(SYS_KEYBOARD_GETCHAR, 0, 0, 0, 0, 0);
}

/* ================================
 * Convenience Functions
 * ================================ */

/**
 * Get next key event (blocking)
 * @param event: Pointer to store the event
 * @return: 0 on success, negative error code on failure
 */
static inline int keyboard_get_next_event(key_event_t* event) {
    if (!event) {
        return -1;
    }
    
    int result = keyboard_read_events(event, sizeof(key_event_t));
    return (result == sizeof(key_event_t)) ? 0 : -1;
}

/**
 * Get next key event (non-blocking)
 * @param event: Pointer to store the event
 * @return: 0 on success, -1 if no event available, negative error code on failure
 */
static inline int keyboard_get_next_event_nonblock(key_event_t* event) {
    if (!event) {
        return -1;
    }
    
    if (keyboard_poll_data() <= 0) {
        return -1;  /* No data available */
    }
    
    int result = keyboard_read_events(event, sizeof(key_event_t));
    return (result == sizeof(key_event_t)) ? 0 : -1;
}

/**
 * Get character from keyboard (non-blocking)
 * @return: Character if available, -1 if no character available
 */
static inline int keyboard_get_char_nonblock(void) {
    key_event_t event;
    
    while (keyboard_get_next_event_nonblock(&event) == 0) {
        if (event.type == KEY_EVENT_PRESS && event.ascii != 0) {
            return event.ascii;
        }
    }
    
    return -1;  /* No character available */
}

/**
 * Wait for a specific key to be pressed
 * @param keycode: Key code to wait for
 * @return: 0 when key is pressed, negative error code on failure
 */
static inline int keyboard_wait_for_keypress(uint8_t keycode) {
    key_event_t event;
    
    while (keyboard_get_next_event(&event) == 0) {
        if (event.type == KEY_EVENT_PRESS && event.keycode == keycode) {
            return 0;
        }
    }
    
    return -1;
}

/**
 * Wait for any key to be pressed
 * @return: Key code of pressed key, negative error code on failure
 */
static inline int keyboard_wait_for_any_key(void) {
    key_event_t event;
    
    while (keyboard_get_next_event(&event) == 0) {
        if (event.type == KEY_EVENT_PRESS) {
            return event.keycode;
        }
    }
    
    return -1;
}

/**
 * Check if a specific modifier key is currently pressed
 * @param modifier: Modifier flag to check (MOD_SHIFT, MOD_CTRL, etc.)
 * @return: true if modifier is pressed, false otherwise
 */
static inline bool keyboard_is_modifier_active(uint8_t modifier) {
    uint8_t modifiers;
    if (keyboard_control(KEYBOARD_IOCTL_GET_MODIFIERS, &modifiers) == 0) {
        return (modifiers & modifier) != 0;
    }
    return false;
}

/**
 * Get current keyboard state
 * @param state: Pointer to store keyboard state
 * @return: 0 on success, negative error code on failure
 */
static inline int keyboard_get_current_state(keyboard_state_t* state) {
    if (!state) {
        return -1;
    }
    
    return keyboard_control(KEYBOARD_IOCTL_GET_STATE, state);
}

/**
 * Clear keyboard input buffer
 * @return: 0 on success, negative error code on failure
 */
static inline int keyboard_clear_input_buffer(void) {
    return keyboard_control(KEYBOARD_IOCTL_CLEAR_BUFFER, NULL);
}

/**
 * Set keyboard LED state
 * @param led_state: LED state flags (LED_CAPS_LOCK, LED_NUM_LOCK, etc.)
 * @return: 0 on success, negative error code on failure
 */
static inline int keyboard_set_led_state(uint8_t led_state) {
    return keyboard_control(KEYBOARD_IOCTL_SET_LEDS, &led_state);
}

/**
 * Get keyboard statistics
 * @param stats: Pointer to store statistics
 * @return: 0 on success, negative error code on failure
 */
static inline int keyboard_get_statistics(keyboard_state_t* stats) {
    if (!stats) {
        return -1;
    }
    
    return keyboard_control(KEYBOARD_IOCTL_GET_STATS, stats);
}

/* ================================
 * High-Level Input Functions
 * ================================ */

/**
 * Read a line of text from keyboard
 * @param buffer: Buffer to store the line
 * @param size: Size of buffer
 * @return: Number of characters read, or negative error code
 */
int keyboard_read_line(char* buffer, size_t size);

/**
 * Read a string with echo
 * @param prompt: Prompt to display
 * @param buffer: Buffer to store input
 * @param size: Size of buffer
 * @return: Number of characters read, or negative error code
 */
int keyboard_read_string(const char* prompt, char* buffer, size_t size);

/**
 * Read a password (no echo)
 * @param prompt: Prompt to display
 * @param buffer: Buffer to store password
 * @param size: Size of buffer
 * @return: Number of characters read, or negative error code
 */
int keyboard_read_password(const char* prompt, char* buffer, size_t size);

/**
 * Read an integer from keyboard
 * @param prompt: Prompt to display
 * @param value: Pointer to store the integer
 * @return: 0 on success, negative error code on failure
 */
int keyboard_read_integer(const char* prompt, int* value);

/**
 * Present a menu and get user selection
 * @param title: Menu title
 * @param options: Array of menu option strings
 * @param count: Number of options
 * @return: Selected option index (0-based), or negative error code
 */
int keyboard_menu_select(const char* title, const char** options, int count);

/**
 * Wait for user confirmation (Y/N)
 * @param prompt: Confirmation prompt
 * @return: 1 for yes, 0 for no, negative error code on failure
 */
int keyboard_confirm(const char* prompt);

/* ================================
 * Key Combination Handling
 * ================================ */

/**
 * Check for specific key combination
 * @param keycode: Main key code
 * @param modifiers: Required modifier flags
 * @return: true if combination is currently active, false otherwise
 */
bool keyboard_check_combination(uint8_t keycode, uint8_t modifiers);

/**
 * Wait for specific key combination
 * @param keycode: Main key code
 * @param modifiers: Required modifier flags
 * @return: 0 when combination is detected, negative error code on failure
 */
int keyboard_wait_combination(uint8_t keycode, uint8_t modifiers);

/**
 * Register hotkey callback (if supported by system)
 * @param keycode: Key code
 * @param modifiers: Modifier flags
 * @param callback: Callback function
 * @return: Hotkey ID on success, negative error code on failure
 */
int keyboard_register_hotkey(uint8_t keycode, uint8_t modifiers, void (*callback)(void));

/**
 * Unregister hotkey callback
 * @param hotkey_id: Hotkey ID returned by keyboard_register_hotkey
 * @return: 0 on success, negative error code on failure
 */
int keyboard_unregister_hotkey(int hotkey_id);

/* ================================
 * Utility Functions
 * ================================ */

/**
 * Convert key event to string representation
 * @param event: Key event
 * @param buffer: Buffer to store string
 * @param size: Size of buffer
 * @return: Number of characters written, or negative error code
 */
int keyboard_event_to_string(const key_event_t* event, char* buffer, size_t size);

/**
 * Get key name string
 * @param keycode: Key code
 * @return: String representation of key name, or NULL if unknown
 */
const char* keyboard_get_key_name(uint8_t keycode);

/**
 * Get modifier string
 * @param modifiers: Modifier flags
 * @param buffer: Buffer to store string
 * @param size: Size of buffer
 * @return: Number of characters written
 */
int keyboard_get_modifier_string(uint8_t modifiers, char* buffer, size_t size);

/**
 * Parse key combination string
 * @param str: String like "Ctrl+Alt+A"
 * @param keycode: Pointer to store key code
 * @param modifiers: Pointer to store modifier flags
 * @return: 0 on success, negative error code on failure
 */
int keyboard_parse_combination(const char* str, uint8_t* keycode, uint8_t* modifiers);

/* ================================
 * Error Handling
 * ================================ */

/**
 * Get error string for keyboard error code
 * @param error_code: Error code
 * @return: String description of error
 */
const char* keyboard_get_error_string(int error_code);

/**
 * Print keyboard error message
 * @param error_code: Error code
 * @param prefix: Optional prefix for error message
 */
void keyboard_print_error(int error_code, const char* prefix);

#endif /* KEYBOARD_USER_API_H */
