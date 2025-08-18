/* IKOS Input Events Implementation
 * Event processing utilities and queue management for the input system
 * 
 * This module provides:
 * - Event queue creation, management, and operations
 * - Event validation and sanitization
 * - Event filtering and type checking
 * - Event format conversion and debugging utilities
 * - Timestamp management and event batching
 * 
 * The event system uses a circular buffer design for efficient
 * memory usage and supports various filtering mechanisms to allow
 * applications to receive only the events they're interested in.
 * Events are validated for consistency and security before being
 * queued for delivery to applications.
 */

#include "input.h"
#include "memory.h"
#include <string.h>

/* Global timestamp counter */
static uint64_t g_timestamp_counter = 0;

/* ================================
 * Event Queue Management
 * ================================ */

input_event_t* input_event_queue_create(size_t size) {
    if (size == 0 || size > INPUT_EVENT_QUEUE_MAX_SIZE) {
        return NULL;
    }
    
    return (input_event_t*)kmalloc(sizeof(input_event_t) * size);
}

void input_event_queue_destroy(input_event_t* queue) {
    if (queue) {
        kfree(queue);
    }
}

bool input_event_queue_push(input_event_t* queue, size_t queue_size, 
                            size_t* head, size_t* tail, size_t* count,
                            const input_event_t* event) {
    if (!queue || !head || !tail || !count || !event) {
        return false;
    }
    
    /* Check if queue is full */
    if (*count >= queue_size) {
        return false;
    }
    
    /* Copy event to queue */
    queue[*tail] = *event;
    
    /* Update tail and count */
    *tail = (*tail + 1) % queue_size;
    (*count)++;
    
    return true;
}

bool input_event_queue_pop(input_event_t* queue, size_t queue_size,
                          size_t* head, size_t* tail, size_t* count,
                          input_event_t* event) {
    if (!queue || !head || !tail || !count || !event) {
        return false;
    }
    
    /* Check if queue is empty */
    if (*count == 0) {
        return false;
    }
    
    /* Copy event from queue */
    *event = queue[*head];
    
    /* Update head and count */
    *head = (*head + 1) % queue_size;
    (*count)--;
    
    return true;
}

bool input_event_queue_peek(input_event_t* queue, size_t queue_size,
                           size_t head, size_t tail, size_t count,
                           input_event_t* event) {
    if (!queue || !event) {
        return false;
    }
    
    /* Check if queue is empty */
    if (count == 0) {
        return false;
    }
    
    /* Copy event without removing */
    *event = queue[head];
    
    return true;
}

bool input_event_queue_is_empty(size_t count) {
    return count == 0;
}

bool input_event_queue_is_full(size_t count, size_t queue_size) {
    return count >= queue_size;
}

size_t input_event_queue_usage(size_t count, size_t queue_size) {
    if (queue_size == 0) return 0;
    return (count * 100) / queue_size;
}

/* ================================
 * Event Filtering and Processing
 * ================================ */

bool input_event_filter_by_type(const input_event_t* event, void* type_mask) {
    if (!event || !type_mask) {
        return false;
    }
    
    uint32_t mask = *(uint32_t*)type_mask;
    uint32_t event_bit = 1 << event->type;
    
    return (mask & event_bit) != 0;
}

bool input_event_filter_by_device(const input_event_t* event, void* device_id) {
    if (!event || !device_id) {
        return false;
    }
    
    uint32_t target_id = *(uint32_t*)device_id;
    return event->device_id == target_id;
}

bool input_event_filter_keyboard_only(const input_event_t* event, void* user_data) {
    (void)user_data; /* Unused */
    
    if (!event) return false;
    
    return (event->type == INPUT_EVENT_KEY_PRESS || 
            event->type == INPUT_EVENT_KEY_RELEASE);
}

bool input_event_filter_mouse_only(const input_event_t* event, void* user_data) {
    (void)user_data; /* Unused */
    
    if (!event) return false;
    
    return (event->type == INPUT_EVENT_MOUSE_MOVE ||
            event->type == INPUT_EVENT_MOUSE_BUTTON_PRESS ||
            event->type == INPUT_EVENT_MOUSE_BUTTON_RELEASE ||
            event->type == INPUT_EVENT_MOUSE_WHEEL);
}

bool input_event_filter_combine(const input_event_t* event, 
                               input_event_filter_t* filters, 
                               void** filter_data, 
                               size_t filter_count) {
    if (!event || !filters || filter_count == 0) {
        return false;
    }
    
    /* All filters must pass (AND logic) */
    for (size_t i = 0; i < filter_count; i++) {
        void* data = filter_data ? filter_data[i] : NULL;
        if (!filters[i](event, data)) {
            return false;
        }
    }
    
    return true;
}

/* ================================
 * Event Transformation
 * ================================ */

char input_event_key_to_char(const input_event_t* event) {
    if (!event || event->type != INPUT_EVENT_KEY_PRESS) {
        return 0;
    }
    
    /* Return Unicode character if available */
    if (event->data.key.unicode != 0 && event->data.key.unicode < 128) {
        return (char)event->data.key.unicode;
    }
    
    /* Basic ASCII conversion for common keys */
    uint32_t keycode = event->data.key.keycode;
    bool shift_pressed = (event->data.key.modifiers & INPUT_MOD_SHIFT) != 0;
    bool caps_lock = (event->data.key.modifiers & INPUT_MOD_CAPS) != 0;
    
    /* Letters */
    if (keycode >= 'a' && keycode <= 'z') {
        if (shift_pressed || caps_lock) {
            return keycode - 'a' + 'A';
        }
        return keycode;
    }
    
    /* Numbers and symbols */
    if (keycode >= '0' && keycode <= '9') {
        if (shift_pressed) {
            static const char shifted_numbers[] = ")!@#$%^&*(";
            return shifted_numbers[keycode - '0'];
        }
        return keycode;
    }
    
    /* Special characters */
    switch (keycode) {
        case ' ': return ' ';
        case '\t': return '\t';
        case '\n': return '\n';
        case '\b': return '\b';
        case 27: return 27; /* ESC */
        default: return 0;
    }
}

bool input_event_is_printable(const input_event_t* event) {
    if (!event || event->type != INPUT_EVENT_KEY_PRESS) {
        return false;
    }
    
    char c = input_event_key_to_char(event);
    return (c >= 32 && c <= 126); /* Printable ASCII range */
}

bool input_event_is_modifier(const input_event_t* event) {
    if (!event || (event->type != INPUT_EVENT_KEY_PRESS && 
                   event->type != INPUT_EVENT_KEY_RELEASE)) {
        return false;
    }
    
    uint32_t keycode = event->data.key.keycode;
    
    /* Common modifier keys */
    return (keycode == 16 ||  /* Shift */
            keycode == 17 ||  /* Ctrl */
            keycode == 18 ||  /* Alt */
            keycode == 91 ||  /* Super/Win */
            keycode == 20 ||  /* Caps Lock */
            keycode == 144 || /* Num Lock */
            keycode == 145);  /* Scroll Lock */
}

bool input_event_is_navigation(const input_event_t* event) {
    if (!event || event->type != INPUT_EVENT_KEY_PRESS) {
        return false;
    }
    
    uint32_t keycode = event->data.key.keycode;
    
    /* Arrow keys and navigation */
    return (keycode == 37 ||  /* Left */
            keycode == 38 ||  /* Up */
            keycode == 39 ||  /* Right */
            keycode == 40 ||  /* Down */
            keycode == 36 ||  /* Home */
            keycode == 35 ||  /* End */
            keycode == 33 ||  /* Page Up */
            keycode == 34);   /* Page Down */
}

bool input_event_is_function_key(const input_event_t* event) {
    if (!event || event->type != INPUT_EVENT_KEY_PRESS) {
        return false;
    }
    
    uint32_t keycode = event->data.key.keycode;
    
    /* Function keys F1-F12 */
    return (keycode >= 112 && keycode <= 123);
}

/* ================================
 * Event Validation
 * ================================ */

bool input_event_validate(const input_event_t* event) {
    if (!event) {
        return false;
    }
    
    /* Validate event type */
    if (event->type < INPUT_EVENT_KEY_PRESS || 
        event->type > INPUT_EVENT_DEVICE_DISCONNECT) {
        return false;
    }
    
    /* Validate device ID */
    if (event->device_id == 0) {
        return false;
    }
    
    /* Type-specific validation */
    switch (event->type) {
        case INPUT_EVENT_KEY_PRESS:
        case INPUT_EVENT_KEY_RELEASE:
            return input_event_validate_key(event);
            
        case INPUT_EVENT_MOUSE_MOVE:
        case INPUT_EVENT_MOUSE_BUTTON_PRESS:
        case INPUT_EVENT_MOUSE_BUTTON_RELEASE:
        case INPUT_EVENT_MOUSE_WHEEL:
            return input_event_validate_mouse(event);
            
        case INPUT_EVENT_DEVICE_CONNECT:
        case INPUT_EVENT_DEVICE_DISCONNECT:
            return true; /* Always valid */
            
        default:
            return false;
    }
}

bool input_event_validate_key(const input_event_t* event) {
    if (!event || (event->type != INPUT_EVENT_KEY_PRESS && 
                   event->type != INPUT_EVENT_KEY_RELEASE)) {
        return false;
    }
    
    /* Validate keycode range */
    if (event->data.key.keycode == 0 || event->data.key.keycode > 255) {
        return false;
    }
    
    /* Validate modifier flags */
    uint32_t valid_modifiers = INPUT_MOD_SHIFT | INPUT_MOD_CTRL | INPUT_MOD_ALT | 
                              INPUT_MOD_SUPER | INPUT_MOD_CAPS | INPUT_MOD_NUM | 
                              INPUT_MOD_SCROLL;
    if ((event->data.key.modifiers & ~valid_modifiers) != 0) {
        return false;
    }
    
    return true;
}

bool input_event_validate_mouse(const input_event_t* event) {
    if (!event) {
        return false;
    }
    
    switch (event->type) {
        case INPUT_EVENT_MOUSE_MOVE:
            /* Coordinates can be any value */
            return true;
            
        case INPUT_EVENT_MOUSE_BUTTON_PRESS:
        case INPUT_EVENT_MOUSE_BUTTON_RELEASE:
            /* Validate button flags */
            {
                uint32_t valid_buttons = INPUT_MOUSE_LEFT | INPUT_MOUSE_RIGHT | 
                                        INPUT_MOUSE_MIDDLE | INPUT_MOUSE_SIDE1 | 
                                        INPUT_MOUSE_SIDE2;
                return (event->data.mouse_button.button & ~valid_buttons) == 0;
            }
            
        case INPUT_EVENT_MOUSE_WHEEL:
            /* Wheel delta can be any value */
            return true;
            
        default:
            return false;
    }
}

/* ================================
 * Event Utilities
 * ================================ */

void input_event_copy(input_event_t* dest, const input_event_t* src) {
    if (dest && src) {
        *dest = *src;
    }
}

bool input_event_equal(const input_event_t* a, const input_event_t* b) {
    if (!a || !b) {
        return false;
    }
    
    return memcmp(a, b, sizeof(input_event_t)) == 0;
}

const char* input_event_type_name(input_event_type_t type) {
    switch (type) {
        case INPUT_EVENT_KEY_PRESS: return "KEY_PRESS";
        case INPUT_EVENT_KEY_RELEASE: return "KEY_RELEASE";
        case INPUT_EVENT_MOUSE_MOVE: return "MOUSE_MOVE";
        case INPUT_EVENT_MOUSE_BUTTON_PRESS: return "MOUSE_BUTTON_PRESS";
        case INPUT_EVENT_MOUSE_BUTTON_RELEASE: return "MOUSE_BUTTON_RELEASE";
        case INPUT_EVENT_MOUSE_WHEEL: return "MOUSE_WHEEL";
        case INPUT_EVENT_DEVICE_CONNECT: return "DEVICE_CONNECT";
        case INPUT_EVENT_DEVICE_DISCONNECT: return "DEVICE_DISCONNECT";
        default: return "UNKNOWN";
    }
}

const char* input_device_type_name(input_device_type_t type) {
    switch (type) {
        case INPUT_DEVICE_KEYBOARD: return "KEYBOARD";
        case INPUT_DEVICE_MOUSE: return "MOUSE";
        case INPUT_DEVICE_TOUCHPAD: return "TOUCHPAD";
        case INPUT_DEVICE_GAMEPAD: return "GAMEPAD";
        case INPUT_DEVICE_TOUCHSCREEN: return "TOUCHSCREEN";
        default: return "UNKNOWN";
    }
}

int input_event_format_debug(const input_event_t* event, char* buffer, size_t buffer_size) {
    if (!event || !buffer || buffer_size == 0) {
        return -1;
    }
    
    int written = 0;
    
    switch (event->type) {
        case INPUT_EVENT_KEY_PRESS:
        case INPUT_EVENT_KEY_RELEASE:
            written = snprintf(buffer, buffer_size, 
                "%s: device=%u keycode=%u modifiers=0x%x unicode=%u",
                input_event_type_name(event->type),
                event->device_id,
                event->data.key.keycode,
                event->data.key.modifiers,
                event->data.key.unicode);
            break;
            
        case INPUT_EVENT_MOUSE_MOVE:
            written = snprintf(buffer, buffer_size,
                "%s: device=%u x=%d y=%d dx=%d dy=%d",
                input_event_type_name(event->type),
                event->device_id,
                event->data.mouse_move.x,
                event->data.mouse_move.y,
                event->data.mouse_move.delta_x,
                event->data.mouse_move.delta_y);
            break;
            
        case INPUT_EVENT_MOUSE_BUTTON_PRESS:
        case INPUT_EVENT_MOUSE_BUTTON_RELEASE:
            written = snprintf(buffer, buffer_size,
                "%s: device=%u button=0x%x x=%d y=%d",
                input_event_type_name(event->type),
                event->device_id,
                event->data.mouse_button.button,
                event->data.mouse_button.x,
                event->data.mouse_button.y);
            break;
            
        default:
            written = snprintf(buffer, buffer_size,
                "%s: device=%u",
                input_event_type_name(event->type),
                event->device_id);
            break;
    }
    
    return written;
}

/* ================================
 * Timestamp and Timing
 * ================================ */

uint64_t input_get_timestamp(void) {
    /* Simple timestamp counter - in real implementation this would
       use the system timer or TSC */
    return ++g_timestamp_counter;
}

uint64_t input_event_age(const input_event_t* event) {
    if (!event) {
        return 0;
    }
    
    uint64_t current_time = input_get_timestamp();
    if (current_time >= event->timestamp) {
        return current_time - event->timestamp;
    }
    
    return 0;
}

bool input_event_is_stale(const input_event_t* event, uint64_t max_age_ms) {
    return input_event_age(event) > max_age_ms;
}

/* ================================
 * Event Batching
 * ================================ */

size_t input_event_batch_similar(input_event_t* events, size_t event_count, 
                                input_event_t* batched, size_t max_batched) {
    if (!events || !batched || event_count == 0 || max_batched == 0) {
        return 0;
    }
    
    size_t batched_count = 0;
    
    for (size_t i = 0; i < event_count && batched_count < max_batched; i++) {
        bool merged = false;
        
        /* Try to merge with existing batched events */
        for (size_t j = 0; j < batched_count; j++) {
            if (input_event_can_batch(&batched[j], &events[i])) {
                input_event_merge(&batched[j], &events[i]);
                merged = true;
                break;
            }
        }
        
        /* If not merged, add as new event */
        if (!merged) {
            batched[batched_count++] = events[i];
        }
    }
    
    return batched_count;
}

bool input_event_can_batch(const input_event_t* a, const input_event_t* b) {
    if (!a || !b) {
        return false;
    }
    
    /* Only batch events of the same type from the same device */
    if (a->type != b->type || a->device_id != b->device_id) {
        return false;
    }
    
    /* Only batch mouse move events */
    return a->type == INPUT_EVENT_MOUSE_MOVE;
}

bool input_event_merge(input_event_t* dest, const input_event_t* src) {
    if (!dest || !src || !input_event_can_batch(dest, src)) {
        return false;
    }
    
    /* Merge mouse move events */
    if (dest->type == INPUT_EVENT_MOUSE_MOVE) {
        /* Use latest position */
        dest->data.mouse_move.x = src->data.mouse_move.x;
        dest->data.mouse_move.y = src->data.mouse_move.y;
        
        /* Accumulate deltas */
        dest->data.mouse_move.delta_x += src->data.mouse_move.delta_x;
        dest->data.mouse_move.delta_y += src->data.mouse_move.delta_y;
        
        /* Use latest timestamp */
        dest->timestamp = src->timestamp;
        
        return true;
    }
    
    return false;
}
