/* IKOS Keyboard Driver Implementation
 * Handles keyboard input events and provides API for user-space applications
 */

#include "keyboard.h"
#include "interrupts.h"
#include "idt.h"
#include "process.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* External functions we need */
extern void* malloc(size_t size);
extern void free(void* ptr);

/* Implementations for interrupt and CPU functions */
int register_interrupt_handler(uint8_t irq, void (*handler)(void)) {
    /* For now, just use the IDT directly for keyboard IRQ */
    if (irq == 1) {  /* Keyboard IRQ */
        idt_set_gate(IRQ_BASE + IRQ_KEYBOARD, handler, 
                    0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
        pic_clear_mask(IRQ_KEYBOARD);
        return 0;
    }
    return -1;
}

int unregister_interrupt_handler(uint8_t irq) {
    /* For now, just mask the IRQ */
    if (irq == 1) {  /* Keyboard IRQ */
        pic_set_mask(IRQ_KEYBOARD);
        return 0;
    }
    return -1;
}

void yield_cpu(void) {
    /* Simple busy wait - in a real system this would be a scheduler call */
    for (volatile int i = 0; i < 1000; i++);
}

/* External functions */
extern void debug_print(const char* format, ...);
extern void* memcpy(void* dest, const void* src, size_t size);

/* Global keyboard state */
static keyboard_state_t g_keyboard_state;
static keyboard_listener_reg_t g_listeners[KEYBOARD_MAX_LISTENERS];
static bool g_keyboard_initialized = false;
static bool g_debug_enabled = false;

/* Scancode to keycode translation table */
static const uint8_t scancode_to_keycode[128] = {
    0x00, KEY_ESCAPE, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6,
    KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUALS, KEY_BACKSPACE, KEY_TAB,
    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I,
    KEY_O, KEY_P, KEY_LBRACKET, KEY_RBRACKET, KEY_ENTER, KEY_LCTRL, KEY_A, KEY_S,
    KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON,
    KEY_APOSTROPHE, KEY_GRAVE, KEY_LSHIFT, KEY_BACKSLASH, KEY_Z, KEY_X, KEY_C, KEY_V,
    KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_PERIOD, KEY_SLASH, KEY_RSHIFT, KEY_MULTIPLY,
    KEY_LALT, KEY_SPACE, KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
    KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* ASCII translation tables */
static const char keycode_to_ascii_normal[256] = {
    [KEY_Q] = 'q', [KEY_W] = 'w', [KEY_E] = 'e', [KEY_R] = 'r', [KEY_T] = 't',
    [KEY_Y] = 'y', [KEY_U] = 'u', [KEY_I] = 'i', [KEY_O] = 'o', [KEY_P] = 'p',
    [KEY_A] = 'a', [KEY_S] = 's', [KEY_D] = 'd', [KEY_F] = 'f', [KEY_G] = 'g',
    [KEY_H] = 'h', [KEY_J] = 'j', [KEY_K] = 'k', [KEY_L] = 'l',
    [KEY_Z] = 'z', [KEY_X] = 'x', [KEY_C] = 'c', [KEY_V] = 'v', [KEY_B] = 'b',
    [KEY_N] = 'n', [KEY_M] = 'm',
    [KEY_1] = '1', [KEY_2] = '2', [KEY_3] = '3', [KEY_4] = '4', [KEY_5] = '5',
    [KEY_6] = '6', [KEY_7] = '7', [KEY_8] = '8', [KEY_9] = '9', [KEY_0] = '0',
    [KEY_SPACE] = ' ', [KEY_ENTER] = '\n', [KEY_TAB] = '\t', [KEY_BACKSPACE] = '\b',
    [KEY_MINUS] = '-', [KEY_EQUALS] = '=', [KEY_LBRACKET] = '[', [KEY_RBRACKET] = ']',
    [KEY_SEMICOLON] = ';', [KEY_APOSTROPHE] = '\'', [KEY_GRAVE] = '`',
    [KEY_BACKSLASH] = '\\', [KEY_COMMA] = ',', [KEY_PERIOD] = '.', [KEY_SLASH] = '/'
};

static const char keycode_to_ascii_shift[256] = {
    [KEY_Q] = 'Q', [KEY_W] = 'W', [KEY_E] = 'E', [KEY_R] = 'R', [KEY_T] = 'T',
    [KEY_Y] = 'Y', [KEY_U] = 'U', [KEY_I] = 'I', [KEY_O] = 'O', [KEY_P] = 'P',
    [KEY_A] = 'A', [KEY_S] = 'S', [KEY_D] = 'D', [KEY_F] = 'F', [KEY_G] = 'G',
    [KEY_H] = 'H', [KEY_J] = 'J', [KEY_K] = 'K', [KEY_L] = 'L',
    [KEY_Z] = 'Z', [KEY_X] = 'X', [KEY_C] = 'C', [KEY_V] = 'V', [KEY_B] = 'B',
    [KEY_N] = 'N', [KEY_M] = 'M',
    [KEY_1] = '!', [KEY_2] = '@', [KEY_3] = '#', [KEY_4] = '$', [KEY_5] = '%',
    [KEY_6] = '^', [KEY_7] = '&', [KEY_8] = '*', [KEY_9] = '(', [KEY_0] = ')',
    [KEY_SPACE] = ' ', [KEY_ENTER] = '\n', [KEY_TAB] = '\t', [KEY_BACKSPACE] = '\b',
    [KEY_MINUS] = '_', [KEY_EQUALS] = '+', [KEY_LBRACKET] = '{', [KEY_RBRACKET] = '}',
    [KEY_SEMICOLON] = ':', [KEY_APOSTROPHE] = '"', [KEY_GRAVE] = '~',
    [KEY_BACKSLASH] = '|', [KEY_COMMA] = '<', [KEY_PERIOD] = '>', [KEY_SLASH] = '?'
};

/* Forward declarations */
static void keyboard_process_scancode(uint8_t scancode);
static void keyboard_add_event(const key_event_t* event);
static void keyboard_notify_listeners(const key_event_t* event);
static uint64_t keyboard_get_timestamp(void);

/* ================================
 * Core Keyboard Driver Functions
 * ================================ */

/**
 * Initialize keyboard driver
 */
int keyboard_init(void) {
    if (g_keyboard_initialized) {
        return KEYBOARD_SUCCESS;
    }
    
    debug_print("KEYBOARD: Initializing keyboard driver...\n");
    
    /* Initialize keyboard state */
    memset(&g_keyboard_state, 0, sizeof(keyboard_state_t));
    memset(g_listeners, 0, sizeof(g_listeners));
    
    /* Reset modifier state */
    g_keyboard_state.modifiers = 0;
    g_keyboard_state.caps_lock = false;
    g_keyboard_state.num_lock = false;
    g_keyboard_state.scroll_lock = false;
    
    /* Initialize buffer */
    g_keyboard_state.buffer_head = 0;
    g_keyboard_state.buffer_tail = 0;
    g_keyboard_state.buffer_count = 0;
    
    /* Disable keyboard during initialization */
    keyboard_write_command(KEYBOARD_CMD_DISABLE_KEYBOARD);
    keyboard_wait_ready();
    
    /* Flush any pending data */
    while (keyboard_read_status() & KEYBOARD_STATUS_OUTPUT_FULL) {
        keyboard_read_data();
    }
    
    /* Perform controller self-test */
    keyboard_write_command(KEYBOARD_CMD_SELF_TEST);
    keyboard_wait_ready();
    uint8_t result = keyboard_read_data();
    if (result != 0x55) {
        debug_print("KEYBOARD: Controller self-test failed (0x%02x)\n", result);
        return KEYBOARD_ERROR_HARDWARE;
    }
    
    /* Test keyboard interface */
    keyboard_write_command(KEYBOARD_CMD_TEST_KEYBOARD);
    keyboard_wait_ready();
    result = keyboard_read_data();
    if (result != 0x00) {
        debug_print("KEYBOARD: Keyboard interface test failed (0x%02x)\n", result);
        return KEYBOARD_ERROR_HARDWARE;
    }
    
    /* Enable keyboard */
    keyboard_write_command(KEYBOARD_CMD_ENABLE_KEYBOARD);
    keyboard_wait_ready();
    
    /* Set default configuration */
    keyboard_write_command(KEYBOARD_CMD_READ_CONFIG);
    keyboard_wait_ready();
    uint8_t config = keyboard_read_data();
    config |= 0x01;  /* Enable keyboard interrupt */
    config &= ~0x10; /* Enable keyboard */
    keyboard_write_command(KEYBOARD_CMD_WRITE_CONFIG);
    keyboard_wait_ready();
    keyboard_write_data(config);
    keyboard_wait_ready();
    
    /* Register interrupt handler */
    if (register_interrupt_handler(0x21, keyboard_interrupt_handler) != 0) {
        debug_print("KEYBOARD: Failed to register interrupt handler\n");
        return KEYBOARD_ERROR_INIT;
    }
    
    /* Set initial LED state */
    keyboard_set_leds(0);
    
    g_keyboard_initialized = true;
    debug_print("KEYBOARD: Keyboard driver initialized successfully\n");
    return KEYBOARD_SUCCESS;
}

/**
 * Cleanup keyboard driver
 */
void keyboard_cleanup(void) {
    if (!g_keyboard_initialized) {
        return;
    }
    
    debug_print("KEYBOARD: Cleaning up keyboard driver...\n");
    
    /* Disable keyboard */
    keyboard_write_command(KEYBOARD_CMD_DISABLE_KEYBOARD);
    keyboard_wait_ready();
    
    /* Unregister interrupt handler */
    unregister_interrupt_handler(0x21);
    
    /* Clear state */
    memset(&g_keyboard_state, 0, sizeof(keyboard_state_t));
    memset(g_listeners, 0, sizeof(g_listeners));
    
    g_keyboard_initialized = false;
    debug_print("KEYBOARD: Keyboard driver cleanup complete\n");
}

/**
 * Process raw keyboard interrupt
 */
void keyboard_interrupt_handler(void) {
    if (!g_keyboard_initialized) {
        return;
    }
    
    /* Read scancode from keyboard controller */
    uint8_t scancode = keyboard_read_data();
    
    if (g_debug_enabled) {
        debug_print("KEYBOARD: Received scancode: 0x%02x\n", scancode);
    }
    
    /* Process the scancode */
    keyboard_process_scancode(scancode);
}

/**
 * Get keyboard driver statistics
 */
void keyboard_get_stats(keyboard_state_t* stats) {
    if (!stats || !g_keyboard_initialized) {
        return;
    }
    
    /* Copy current state */
    memcpy(stats, &g_keyboard_state, sizeof(keyboard_state_t));
}

/**
 * Reset keyboard state
 */
void keyboard_reset(void) {
    if (!g_keyboard_initialized) {
        return;
    }
    
    debug_print("KEYBOARD: Resetting keyboard state...\n");
    
    /* Reset modifier state */
    g_keyboard_state.modifiers = 0;
    g_keyboard_state.caps_lock = false;
    g_keyboard_state.num_lock = false;
    g_keyboard_state.scroll_lock = false;
    
    /* Clear buffer */
    g_keyboard_state.buffer_head = 0;
    g_keyboard_state.buffer_tail = 0;
    g_keyboard_state.buffer_count = 0;
    
    /* Reset LEDs */
    keyboard_set_leds(0);
    
    debug_print("KEYBOARD: Keyboard state reset complete\n");
}

/* ================================
 * Input Buffer Management
 * ================================ */

/**
 * Check if keyboard buffer has data
 */
bool keyboard_has_data(void) {
    return g_keyboard_initialized && (g_keyboard_state.buffer_count > 0);
}

/**
 * Get next key event from buffer (blocking)
 */
int keyboard_get_event(key_event_t* event) {
    if (!event || !g_keyboard_initialized) {
        return KEYBOARD_ERROR_INVALID_PARAM;
    }
    
    /* Wait for data to be available */
    while (!keyboard_has_data()) {
        /* In a real implementation, this would use proper blocking/waiting */
        /* For now, just yield to other processes */
        yield_cpu();
    }
    
    return keyboard_get_event_nonblock(event);
}

/**
 * Get next key event from buffer (non-blocking)
 */
int keyboard_get_event_nonblock(key_event_t* event) {
    if (!event || !g_keyboard_initialized) {
        return KEYBOARD_ERROR_INVALID_PARAM;
    }
    
    if (g_keyboard_state.buffer_count == 0) {
        return KEYBOARD_ERROR_BUFFER_EMPTY;
    }
    
    /* Get event from buffer */
    *event = g_keyboard_state.buffer[g_keyboard_state.buffer_tail];
    
    /* Update buffer pointers */
    g_keyboard_state.buffer_tail = (g_keyboard_state.buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    g_keyboard_state.buffer_count--;
    
    return KEYBOARD_SUCCESS;
}

/**
 * Get ASCII character from buffer (blocking)
 */
char keyboard_getchar(void) {
    key_event_t event;
    
    while (true) {
        if (keyboard_get_event(&event) == KEYBOARD_SUCCESS) {
            if (event.type == KEY_EVENT_PRESS && event.ascii != 0) {
                return event.ascii;
            }
        }
    }
}

/**
 * Get ASCII character from buffer (non-blocking)
 */
int keyboard_getchar_nonblock(void) {
    key_event_t event;
    
    while (g_keyboard_state.buffer_count > 0) {
        if (keyboard_get_event_nonblock(&event) == KEYBOARD_SUCCESS) {
            if (event.type == KEY_EVENT_PRESS && event.ascii != 0) {
                return (int)event.ascii;
            }
        } else {
            break;
        }
    }
    
    return -1;  /* No character available */
}

/**
 * Peek at next event without removing it
 */
int keyboard_peek_event(key_event_t* event) {
    if (!event || !g_keyboard_initialized) {
        return KEYBOARD_ERROR_INVALID_PARAM;
    }
    
    if (g_keyboard_state.buffer_count == 0) {
        return KEYBOARD_ERROR_BUFFER_EMPTY;
    }
    
    /* Copy event without removing it */
    *event = g_keyboard_state.buffer[g_keyboard_state.buffer_tail];
    
    return KEYBOARD_SUCCESS;
}

/**
 * Clear input buffer
 */
void keyboard_clear_buffer(void) {
    if (!g_keyboard_initialized) {
        return;
    }
    
    g_keyboard_state.buffer_head = 0;
    g_keyboard_state.buffer_tail = 0;
    g_keyboard_state.buffer_count = 0;
    
    if (g_debug_enabled) {
        debug_print("KEYBOARD: Input buffer cleared\n");
    }
}

/* ================================
 * Event Listener System
 * ================================ */

/**
 * Register keyboard event listener
 */
int keyboard_register_listener(keyboard_listener_t callback, void* user_data) {
    if (!callback || !g_keyboard_initialized) {
        return KEYBOARD_ERROR_INVALID_PARAM;
    }
    
    /* Find free listener slot */
    for (int i = 0; i < KEYBOARD_MAX_LISTENERS; i++) {
        if (!g_listeners[i].active) {
            g_listeners[i].callback = callback;
            g_listeners[i].user_data = user_data;
            g_listeners[i].active = true;
            
            if (g_debug_enabled) {
                debug_print("KEYBOARD: Registered listener %d\n", i);
            }
            
            return i;
        }
    }
    
    return KEYBOARD_ERROR_LISTENER_FULL;
}

/**
 * Unregister keyboard event listener
 */
int keyboard_unregister_listener(int listener_id) {
    if (listener_id < 0 || listener_id >= KEYBOARD_MAX_LISTENERS || !g_keyboard_initialized) {
        return KEYBOARD_ERROR_LISTENER_INVALID;
    }
    
    g_listeners[listener_id].active = false;
    g_listeners[listener_id].callback = NULL;
    g_listeners[listener_id].user_data = NULL;
    
    if (g_debug_enabled) {
        debug_print("KEYBOARD: Unregistered listener %d\n", listener_id);
    }
    
    return KEYBOARD_SUCCESS;
}

/**
 * Enable/disable specific listener
 */
int keyboard_set_listener_enabled(int listener_id, bool enabled) {
    if (listener_id < 0 || listener_id >= KEYBOARD_MAX_LISTENERS || !g_keyboard_initialized) {
        return KEYBOARD_ERROR_LISTENER_INVALID;
    }
    
    if (g_listeners[listener_id].callback != NULL) {
        g_listeners[listener_id].active = enabled;
        return KEYBOARD_SUCCESS;
    }
    
    return KEYBOARD_ERROR_LISTENER_INVALID;
}

/* ================================
 * Key Mapping and Translation
 * ================================ */

/**
 * Translate scancode to keycode
 */
uint8_t keyboard_scancode_to_keycode(uint8_t scancode) {
    /* Handle release scancodes */
    if (scancode & SCANCODE_RELEASE_FLAG) {
        scancode &= ~SCANCODE_RELEASE_FLAG;
    }
    
    /* Handle extended scancodes */
    if (scancode == SCANCODE_EXTENDED_PREFIX) {
        return 0;  /* Extended prefix, wait for next byte */
    }
    
    if (scancode < 128) {
        return scancode_to_keycode[scancode];
    }
    
    return 0;  /* Invalid scancode */
}

/**
 * Translate keycode to ASCII character
 */
char keyboard_keycode_to_ascii(uint8_t keycode, uint8_t modifiers) {
    bool shift_pressed = (modifiers & MOD_SHIFT) != 0;
    bool caps_on = (modifiers & MOD_CAPS) != 0;
    
    /* For letters, consider both shift and caps lock */
    if (keycode >= KEY_A && keycode <= KEY_Z) {
        bool uppercase = (shift_pressed && !caps_on) || (!shift_pressed && caps_on);
        if (uppercase) {
            return keycode_to_ascii_shift[keycode];
        } else {
            return keycode_to_ascii_normal[keycode];
        }
    }
    
    /* For other keys, only consider shift */
    if (shift_pressed) {
        return keycode_to_ascii_shift[keycode];
    } else {
        return keycode_to_ascii_normal[keycode];
    }
}

/**
 * Check if key is a modifier key
 */
bool keyboard_is_modifier_key(uint8_t keycode) {
    return (keycode == KEY_LSHIFT || keycode == KEY_RSHIFT ||
            keycode == KEY_LCTRL || keycode == KEY_LALT ||
            keycode == KEY_CAPSLOCK);
}

/**
 * Get current modifier state
 */
uint8_t keyboard_get_modifiers(void) {
    return g_keyboard_initialized ? g_keyboard_state.modifiers : 0;
}

/**
 * Set modifier state
 */
void keyboard_set_modifiers(uint8_t modifiers) {
    if (g_keyboard_initialized) {
        g_keyboard_state.modifiers = modifiers;
    }
}

/* ================================
 * Hardware Interface
 * ================================ */

/**
 * Read from keyboard controller data port
 */
uint8_t keyboard_read_data(void) {
    return inb(KEYBOARD_DATA_PORT);
}

/**
 * Write to keyboard controller data port
 */
void keyboard_write_data(uint8_t data) {
    outb(KEYBOARD_DATA_PORT, data);
}

/**
 * Read keyboard controller status
 */
uint8_t keyboard_read_status(void) {
    return inb(KEYBOARD_STATUS_PORT);
}

/**
 * Write keyboard controller command
 */
void keyboard_write_command(uint8_t command) {
    outb(KEYBOARD_COMMAND_PORT, command);
}

/**
 * Wait for keyboard controller ready
 */
void keyboard_wait_ready(void) {
    int timeout = 10000;
    
    while (timeout-- > 0) {
        if (!(keyboard_read_status() & KEYBOARD_STATUS_INPUT_FULL)) {
            break;
        }
    }
}

/* ================================
 * LED Control
 * ================================ */

/**
 * Set keyboard LEDs
 */
void keyboard_set_leds(uint8_t led_state) {
    if (!g_keyboard_initialized) {
        return;
    }
    
    keyboard_write_data(KEYBOARD_CMD_SET_LEDS);
    keyboard_wait_ready();
    keyboard_write_data(led_state);
    keyboard_wait_ready();
    
    if (g_debug_enabled) {
        debug_print("KEYBOARD: Set LEDs to 0x%02x\n", led_state);
    }
}

/**
 * Get current LED state
 */
uint8_t keyboard_get_leds(void) {
    if (!g_keyboard_initialized) {
        return 0;
    }
    
    uint8_t leds = 0;
    if (g_keyboard_state.scroll_lock) leds |= LED_SCROLL_LOCK;
    if (g_keyboard_state.num_lock) leds |= LED_NUM_LOCK;
    if (g_keyboard_state.caps_lock) leds |= LED_CAPS_LOCK;
    
    return leds;
}

/* ================================
 * System Call Interface
 * ================================ */

/**
 * System call interface for keyboard read
 */
int sys_keyboard_read(void* buffer, size_t count) {
    if (!buffer || count == 0 || !g_keyboard_initialized) {
        return KEYBOARD_ERROR_INVALID_PARAM;
    }
    
    key_event_t* events = (key_event_t*)buffer;
    size_t max_events = count / sizeof(key_event_t);
    size_t events_read = 0;
    
    while (events_read < max_events && keyboard_has_data()) {
        if (keyboard_get_event_nonblock(&events[events_read]) == KEYBOARD_SUCCESS) {
            events_read++;
        } else {
            break;
        }
    }
    
    return events_read * sizeof(key_event_t);
}

/**
 * System call interface for keyboard poll
 */
int sys_keyboard_poll(void) {
    if (!g_keyboard_initialized) {
        return 0;
    }
    
    return keyboard_has_data() ? 1 : 0;
}

/**
 * System call interface for keyboard ioctl
 */
int sys_keyboard_ioctl(int cmd, void* arg) {
    if (!g_keyboard_initialized) {
        return KEYBOARD_ERROR_NOT_READY;
    }
    
    switch (cmd) {
        case KEYBOARD_IOCTL_GET_STATE:
            if (arg) {
                keyboard_get_stats((keyboard_state_t*)arg);
                return KEYBOARD_SUCCESS;
            }
            return KEYBOARD_ERROR_INVALID_PARAM;
            
        case KEYBOARD_IOCTL_SET_LEDS:
            if (arg) {
                uint8_t leds = *(uint8_t*)arg;
                keyboard_set_leds(leds);
                return KEYBOARD_SUCCESS;
            }
            return KEYBOARD_ERROR_INVALID_PARAM;
            
        case KEYBOARD_IOCTL_GET_MODIFIERS:
            if (arg) {
                *(uint8_t*)arg = keyboard_get_modifiers();
                return KEYBOARD_SUCCESS;
            }
            return KEYBOARD_ERROR_INVALID_PARAM;
            
        case KEYBOARD_IOCTL_CLEAR_BUFFER:
            keyboard_clear_buffer();
            return KEYBOARD_SUCCESS;
            
        case KEYBOARD_IOCTL_GET_STATS:
            if (arg) {
                keyboard_get_stats((keyboard_state_t*)arg);
                return KEYBOARD_SUCCESS;
            }
            return KEYBOARD_ERROR_INVALID_PARAM;
            
        default:
            return KEYBOARD_ERROR_INVALID_PARAM;
    }
}

/* ================================
 * Debugging and Diagnostics
 * ================================ */

/**
 * Enable/disable keyboard debugging
 */
void keyboard_set_debug(bool enabled) {
    g_debug_enabled = enabled;
    debug_print("KEYBOARD: Debug mode %s\n", enabled ? "enabled" : "disabled");
}

/**
 * Print keyboard state
 */
void keyboard_print_state(void) {
    if (!g_keyboard_initialized) {
        debug_print("KEYBOARD: Driver not initialized\n");
        return;
    }
    
    debug_print("KEYBOARD STATE:\n");
    debug_print("  Modifiers: 0x%02x\n", g_keyboard_state.modifiers);
    debug_print("  Caps Lock: %s\n", g_keyboard_state.caps_lock ? "ON" : "OFF");
    debug_print("  Num Lock: %s\n", g_keyboard_state.num_lock ? "ON" : "OFF");
    debug_print("  Scroll Lock: %s\n", g_keyboard_state.scroll_lock ? "ON" : "OFF");
    debug_print("  Buffer: %zu/%d events\n", g_keyboard_state.buffer_count, KEYBOARD_BUFFER_SIZE);
    debug_print("  Total Events: %llu\n", g_keyboard_state.total_events);
    debug_print("  Dropped Events: %llu\n", g_keyboard_state.dropped_events);
}

/**
 * Test keyboard functionality
 */
int keyboard_self_test(void) {
    debug_print("KEYBOARD: Starting self-test...\n");
    
    if (!g_keyboard_initialized) {
        debug_print("KEYBOARD: Self-test failed - driver not initialized\n");
        return KEYBOARD_ERROR_NOT_READY;
    }
    
    /* Test buffer operations */
    keyboard_clear_buffer();
    if (keyboard_has_data()) {
        debug_print("KEYBOARD: Self-test failed - buffer not empty after clear\n");
        return KEYBOARD_ERROR_HARDWARE;
    }
    
    /* Test LED control */
    keyboard_set_leds(LED_CAPS_LOCK | LED_NUM_LOCK | LED_SCROLL_LOCK);
    keyboard_set_leds(0);
    
    debug_print("KEYBOARD: Self-test completed successfully\n");
    return KEYBOARD_SUCCESS;
}

/* ================================
 * Internal Helper Functions
 * ================================ */

/**
 * Process raw scancode from keyboard
 */
static void keyboard_process_scancode(uint8_t scancode) {
    static bool extended_scancode = false;
    bool key_released = (scancode & SCANCODE_RELEASE_FLAG) != 0;
    
    /* Handle extended scancode prefix */
    if (scancode == 0xE0) {
        extended_scancode = true;
        return;
    }
    
    /* Remove release flag for processing */
    if (key_released) {
        scancode &= ~SCANCODE_RELEASE_FLAG;
    }
    
    /* Translate scancode to keycode (handle extended scancodes) */
    uint8_t keycode;
    if (extended_scancode) {
        /* Handle extended keycodes (arrow keys, etc.) */
        keycode = keyboard_scancode_to_keycode(scancode | 0x80);
        extended_scancode = false;
    } else {
        keycode = keyboard_scancode_to_keycode(scancode);
    }
    
    if (keycode == 0) {
        return;
    }
    
    /* Update modifier state */
    if (keyboard_is_modifier_key(keycode)) {
        if (keycode == KEY_LSHIFT || keycode == KEY_RSHIFT) {
            if (key_released) {
                g_keyboard_state.modifiers &= ~MOD_SHIFT;
            } else {
                g_keyboard_state.modifiers |= MOD_SHIFT;
            }
        } else if (keycode == KEY_LCTRL) {
            if (key_released) {
                g_keyboard_state.modifiers &= ~MOD_CTRL;
            } else {
                g_keyboard_state.modifiers |= MOD_CTRL;
            }
        } else if (keycode == KEY_LALT) {
            if (key_released) {
                g_keyboard_state.modifiers &= ~MOD_ALT;
            } else {
                g_keyboard_state.modifiers |= MOD_ALT;
            }
        } else if (keycode == KEY_CAPSLOCK && !key_released) {
            g_keyboard_state.caps_lock = !g_keyboard_state.caps_lock;
            if (g_keyboard_state.caps_lock) {
                g_keyboard_state.modifiers |= MOD_CAPS;
            } else {
                g_keyboard_state.modifiers &= ~MOD_CAPS;
            }
            keyboard_set_leds(keyboard_get_leds());
        }
    }
    
    /* Create key event */
    key_event_t event;
    event.scancode = scancode;
    event.keycode = keycode;
    event.ascii = keyboard_keycode_to_ascii(keycode, g_keyboard_state.modifiers);
    event.modifiers = g_keyboard_state.modifiers;
    event.type = key_released ? KEY_EVENT_RELEASE : KEY_EVENT_PRESS;
    event.timestamp = keyboard_get_timestamp();
    
    /* Add event to buffer */
    keyboard_add_event(&event);
    
    /* Notify listeners */
    keyboard_notify_listeners(&event);
    
    extended_scancode = false;
}

/**
 * Add event to keyboard buffer
 */
static void keyboard_add_event(const key_event_t* event) {
    if (g_keyboard_state.buffer_count >= KEYBOARD_BUFFER_SIZE) {
        g_keyboard_state.dropped_events++;
        if (g_debug_enabled) {
            debug_print("KEYBOARD: Buffer full, dropping event\n");
        }
        return;
    }
    
    /* Add event to buffer */
    g_keyboard_state.buffer[g_keyboard_state.buffer_head] = *event;
    g_keyboard_state.buffer_head = (g_keyboard_state.buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    g_keyboard_state.buffer_count++;
    g_keyboard_state.total_events++;
}

/**
 * Notify all registered listeners
 */
static void keyboard_notify_listeners(const key_event_t* event) {
    for (int i = 0; i < KEYBOARD_MAX_LISTENERS; i++) {
        if (g_listeners[i].active && g_listeners[i].callback) {
            g_listeners[i].callback(event, g_listeners[i].user_data);
        }
    }
}

/**
 * Get current timestamp
 */
static uint64_t keyboard_get_timestamp(void) {
    /* In a real implementation, this would get the system timer value */
    static uint64_t counter = 0;
    return ++counter;
}
