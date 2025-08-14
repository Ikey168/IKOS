/* IKOS Keyboard Driver Implementation
 * Handles keyboard input events and provides API for user-space applications
 */

#include "keyboard.h"
#include "ipc.h"
#include "memory.h"
#include "scheduler.h"
#include <string.h>

/* Global keyboard driver state */
static kbd_driver_state_t kbd_state;
static bool kbd_initialized = false;

/* Scancode to ASCII mapping tables */
static const uint8_t scancode_to_ascii_lower[KBD_MAX_SCANCODE] = {
    0,    0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*',  0,   ' ',
    [KEY_F1] = 0, [KEY_F2] = 0, [KEY_F3] = 0, [KEY_F4] = 0, [KEY_F5] = 0,
    [KEY_F6] = 0, [KEY_F7] = 0, [KEY_F8] = 0, [KEY_F9] = 0, [KEY_F10] = 0
};

static const uint8_t scancode_to_ascii_upper[KBD_MAX_SCANCODE] = {
    0,    0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*',  0,   ' ',
    [KEY_F1] = 0, [KEY_F2] = 0, [KEY_F3] = 0, [KEY_F4] = 0, [KEY_F5] = 0,
    [KEY_F6] = 0, [KEY_F7] = 0, [KEY_F8] = 0, [KEY_F9] = 0, [KEY_F10] = 0
};

/**
 * Initialize keyboard driver
 */
int kbd_init(void) {
    if (kbd_initialized) {
        return IPC_SUCCESS;
    }
    
    // Initialize driver state
    memset(&kbd_state, 0, sizeof(kbd_state));
    kbd_state.driver_active = true;
    
    // Create keyboard events channel
    uint32_t channel_id = ipc_create_channel("keyboard_events", true, true);
    if (channel_id == IPC_INVALID_CHANNEL) {
        return IPC_ERROR_CHANNEL_NOT_FOUND;
    }
    
    // Register with IPC system
    task_t* current_task = task_get_current();
    if (current_task) {
        ipc_register_keyboard_driver(current_task->pid);
    }
    
    kbd_initialized = true;
    return IPC_SUCCESS;
}

/**
 * Cleanup keyboard driver
 */
int kbd_cleanup(void) {
    if (!kbd_initialized) {
        return IPC_SUCCESS;
    }
    
    kbd_state.driver_active = false;
    
    // Unregister all applications
    for (uint32_t i = 0; i < kbd_state.app_count; i++) {
        kbd_unregister_application(kbd_state.registered_apps[i]);
    }
    
    // Unregister from IPC system
    task_t* current_task = task_get_current();
    if (current_task) {
        ipc_unregister_keyboard_driver(current_task->pid);
    }
    
    kbd_initialized = false;
    return IPC_SUCCESS;
}

/**
 * Register application for keyboard events
 */
int kbd_register_application(uint32_t pid) {
    if (!kbd_initialized || kbd_state.app_count >= KBD_MAX_APPLICATIONS) {
        return IPC_ERROR_QUEUE_FULL;
    }
    
    // Check if already registered
    for (uint32_t i = 0; i < kbd_state.app_count; i++) {
        if (kbd_state.registered_apps[i] == pid) {
            return IPC_SUCCESS;
        }
    }
    
    // Subscribe to keyboard events channel
    ipc_channel_t* kbd_channel = ipc_find_channel("keyboard_events");
    if (kbd_channel) {
        ipc_subscribe_channel(kbd_channel->channel_id, pid);
    }
    
    kbd_state.registered_apps[kbd_state.app_count++] = pid;
    return IPC_SUCCESS;
}

/**
 * Unregister application from keyboard events
 */
int kbd_unregister_application(uint32_t pid) {
    if (!kbd_initialized) {
        return IPC_ERROR_INVALID_PID;
    }
    
    // Find and remove application
    for (uint32_t i = 0; i < kbd_state.app_count; i++) {
        if (kbd_state.registered_apps[i] == pid) {
            // Shift remaining applications
            for (uint32_t j = i; j < kbd_state.app_count - 1; j++) {
                kbd_state.registered_apps[j] = kbd_state.registered_apps[j + 1];
            }
            kbd_state.app_count--;
            return IPC_SUCCESS;
        }
    }
    
    return IPC_ERROR_INVALID_PID;
}

/**
 * Handle keyboard interrupt and process scancode
 */
int kbd_handle_interrupt(uint8_t scancode) {
    if (!kbd_initialized || !kbd_state.driver_active) {
        return IPC_ERROR_INVALID_QUEUE;
    }
    
    kbd_event_t event;
    memset(&event, 0, sizeof(event));
    
    event.scancode = scancode & 0x7F;
    event.state = (scancode & 0x80) ? KEY_RELEASED : KEY_PRESSED;
    event.timestamp = ipc_get_timestamp();
    
    // Handle modifier keys
    switch (event.scancode) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            kbd_state.shift_pressed = (event.state == KEY_PRESSED);
            break;
        case KEY_LCTRL:
            kbd_state.ctrl_pressed = (event.state == KEY_PRESSED);
            break;
        case KEY_LALT:
            kbd_state.alt_pressed = (event.state == KEY_PRESSED);
            break;
        case KEY_CAPS:
            if (event.state == KEY_PRESSED) {
                kbd_state.caps_lock = !kbd_state.caps_lock;
            }
            break;
    }
    
    // Set modifier flags
    event.modifiers = 0;
    if (kbd_state.shift_pressed) event.modifiers |= 0x01;
    if (kbd_state.ctrl_pressed) event.modifiers |= 0x02;
    if (kbd_state.alt_pressed) event.modifiers |= 0x04;
    if (kbd_state.caps_lock) event.modifiers |= 0x08;
    
    // Convert to ASCII
    event.ascii = kbd_scancode_to_ascii(event.scancode, 
                                       kbd_state.shift_pressed, 
                                       kbd_state.caps_lock);
    
    // Add to buffer
    if (kbd_state.count < KBD_BUFFER_SIZE) {
        kbd_state.buffer[kbd_state.tail].event = event;
        kbd_state.buffer[kbd_state.tail].valid = true;
        kbd_state.tail = (kbd_state.tail + 1) % KBD_BUFFER_SIZE;
        kbd_state.count++;
        kbd_state.events_processed++;
        
        // Send event to registered applications
        ipc_message_t* msg = ipc_alloc_message(sizeof(kbd_event_t));
        if (msg) {
            msg->type = IPC_MSG_KEYBOARD_EVENT;
            memcpy(msg->data, &event, sizeof(kbd_event_t));
            ipc_send_keyboard_event(msg);
            ipc_free_message(msg);
        }
    } else {
        kbd_state.events_dropped++;
    }
    
    return IPC_SUCCESS;
}

/**
 * Get keyboard event from buffer
 */
int kbd_get_event(kbd_event_t* event, uint32_t flags) {
    if (!event || !kbd_initialized) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    if (kbd_state.count == 0) {
        if (flags & IPC_FLAG_NON_BLOCKING) {
            return IPC_ERROR_QUEUE_EMPTY;
        }
        // In full implementation, would block here
        return IPC_ERROR_QUEUE_EMPTY;
    }
    
    *event = kbd_state.buffer[kbd_state.head].event;
    kbd_state.buffer[kbd_state.head].valid = false;
    kbd_state.head = (kbd_state.head + 1) % KBD_BUFFER_SIZE;
    kbd_state.count--;
    
    return IPC_SUCCESS;
}

/**
 * Peek at next keyboard event without removing it
 */
int kbd_peek_event(kbd_event_t* event) {
    if (!event || !kbd_initialized) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    if (kbd_state.count == 0) {
        return IPC_ERROR_QUEUE_EMPTY;
    }
    
    *event = kbd_state.buffer[kbd_state.head].event;
    return IPC_SUCCESS;
}

/**
 * Convert scancode to ASCII character
 */
uint8_t kbd_scancode_to_ascii(uint8_t scancode, bool shift, bool caps) {
    if (scancode >= KBD_MAX_SCANCODE) {
        return 0;
    }
    
    bool use_upper = shift;
    
    // Handle caps lock for letters only
    if (caps && scancode >= 0x10 && scancode <= 0x32) {
        use_upper = !use_upper;
    }
    
    return use_upper ? scancode_to_ascii_upper[scancode] : 
                      scancode_to_ascii_lower[scancode];
}

/**
 * Set keyboard LED state (placeholder)
 */
int kbd_set_led_state(uint8_t led_mask) {
    // In full implementation, would control hardware LEDs
    return IPC_SUCCESS;
}

/**
 * Get keyboard driver state
 */
kbd_driver_state_t* kbd_get_state(void) {
    return kbd_initialized ? &kbd_state : NULL;
}

/* Application API Functions */

/**
 * Initialize keyboard API for application
 */
int kbd_api_init(void) {
    task_t* current_task = task_get_current();
    if (!current_task) {
        return IPC_ERROR_INVALID_PID;
    }
    
    return kbd_register_application(current_task->pid);
}

/**
 * Read keyboard event with timeout
 */
int kbd_api_read_key(kbd_event_t* event, uint32_t timeout_ms) {
    if (!event) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    task_t* current_task = task_get_current();
    if (!current_task) {
        return IPC_ERROR_INVALID_PID;
    }
    
    ipc_queue_t* my_queue = process_queues[current_task->pid];
    if (!my_queue) {
        return IPC_ERROR_INVALID_QUEUE;
    }
    
    // Poll for keyboard event message
    uint32_t timeout_counter = timeout_ms;
    while (timeout_counter > 0) {
        ipc_message_t* msg = my_queue->head;
        while (msg) {
            if (msg->type == IPC_MSG_KEYBOARD_EVENT) {
                memcpy(event, msg->data, sizeof(kbd_event_t));
                
                // Remove message from queue
                if (msg->prev) msg->prev->next = msg->next;
                if (msg->next) msg->next->prev = msg->prev;
                if (my_queue->head == msg) my_queue->head = msg->next;
                if (my_queue->tail == msg) my_queue->tail = msg->prev;
                my_queue->current_count--;
                ipc_free_message(msg);
                
                return IPC_SUCCESS;
            }
            msg = msg->next;
        }
        
        sys_yield();
        if (timeout_ms > 0) timeout_counter--;
    }
    
    return IPC_ERROR_TIMEOUT;
}

/**
 * Check if keyboard event is available
 */
int kbd_api_check_key(void) {
    task_t* current_task = task_get_current();
    if (!current_task) {
        return 0;
    }
    
    ipc_queue_t* my_queue = process_queues[current_task->pid];
    if (!my_queue) {
        return 0;
    }
    
    ipc_message_t* msg = my_queue->head;
    while (msg) {
        if (msg->type == IPC_MSG_KEYBOARD_EVENT) {
            return 1;
        }
        msg = msg->next;
    }
    
    return 0;
}

/**
 * Subscribe to keyboard events
 */
int kbd_api_subscribe_events(void) {
    return kbd_api_init();
}

/**
 * Unsubscribe from keyboard events
 */
int kbd_api_unsubscribe_events(void) {
    task_t* current_task = task_get_current();
    if (!current_task) {
        return IPC_ERROR_INVALID_PID;
    }
    
    return kbd_unregister_application(current_task->pid);
}
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
