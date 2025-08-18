/* IKOS Input Manager Implementation
 * Central management service for the unified input handling system
 * 
 * Features:
 * - Device registration and management
 * - Application input contexts and focus handling
 * - Event distribution and routing
 * - Input state tracking and statistics
 * - Device capability detection and configuration
 * 
 * The input manager serves as the central hub that coordinates input
 * devices (keyboards, mice, etc.) and applications that consume input.
 * It provides device abstraction, event validation, and ensures proper
 * delivery of input events to the focused application or interested
 * background applications based on their subscription preferences.
 */

#include "input.h"
#include "input_events.h"
#include "memory.h"
#include "scheduler.h"
#include "process.h"
#include <string.h>

/* Global input manager state */
typedef struct {
    input_device_t* device_list;
    input_context_t application_contexts[INPUT_MAX_APPLICATIONS];
    uint32_t next_device_id;
    uint32_t focused_pid;
    input_state_t current_state;
    bool initialized;
    
    /* Statistics */
    uint64_t total_events_processed;
    uint64_t total_events_dropped;
    uint32_t active_device_count;
    uint32_t registered_app_count;
} input_manager_t;

static input_manager_t g_input_manager;

/* Internal function prototypes */
static input_context_t* find_app_context(uint32_t pid);
static void update_input_state(const input_event_t* event);
static bool should_deliver_event(const input_context_t* context, const input_event_t* event);
static void distribute_event(const input_event_t* event);

/* ================================
 * Core Input System Functions
 * ================================ */

int input_init(void) {
    if (g_input_manager.initialized) {
        return INPUT_SUCCESS;
    }
    
    /* Initialize manager structure */
    memset(&g_input_manager, 0, sizeof(input_manager_t));
    
    /* Initialize application contexts */
    for (int i = 0; i < INPUT_MAX_APPLICATIONS; i++) {
        g_input_manager.application_contexts[i].pid = 0; /* Mark as unused */
        g_input_manager.application_contexts[i].event_queue = NULL;
    }
    
    /* Initialize state */
    g_input_manager.next_device_id = 1;
    g_input_manager.focused_pid = 0;
    g_input_manager.current_state.mouse_x = 400; /* Default center position */
    g_input_manager.current_state.mouse_y = 300;
    g_input_manager.current_state.mouse_buttons = 0;
    g_input_manager.current_state.keyboard_modifiers = 0;
    g_input_manager.current_state.caps_lock = false;
    g_input_manager.current_state.num_lock = false;
    g_input_manager.current_state.scroll_lock = false;
    
    g_input_manager.initialized = true;
    
    return INPUT_SUCCESS;
}

void input_cleanup(void) {
    if (!g_input_manager.initialized) {
        return;
    }
    
    /* Cleanup all devices */
    input_device_t* device = g_input_manager.device_list;
    while (device) {
        input_device_t* next = device->next;
        if (device->cleanup) {
            device->cleanup(device);
        }
        kfree(device);
        device = next;
    }
    
    /* Cleanup application contexts */
    for (int i = 0; i < INPUT_MAX_APPLICATIONS; i++) {
        input_context_t* context = &g_input_manager.application_contexts[i];
        if (context->pid != 0 && context->event_queue) {
            kfree(context->event_queue);
            context->event_queue = NULL;
        }
    }
    
    memset(&g_input_manager, 0, sizeof(input_manager_t));
}

int input_get_state(input_state_t* state) {
    if (!state || !g_input_manager.initialized) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Copy current state */
    *state = g_input_manager.current_state;
    state->focused_pid = g_input_manager.focused_pid;
    state->events_processed = g_input_manager.total_events_processed;
    state->events_dropped = g_input_manager.total_events_dropped;
    state->active_devices = g_input_manager.active_device_count;
    state->registered_apps = g_input_manager.registered_app_count;
    
    return INPUT_SUCCESS;
}

/* ================================
 * Device Management
 * ================================ */

int input_register_device(input_device_t* device) {
    if (!device || !g_input_manager.initialized) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Check if device already exists */
    input_device_t* existing = g_input_manager.device_list;
    while (existing) {
        if (existing->device_id == device->device_id) {
            return INPUT_ERROR_DEVICE_EXISTS;
        }
        existing = existing->next;
    }
    
    /* Assign device ID if not set */
    if (device->device_id == 0) {
        device->device_id = g_input_manager.next_device_id++;
    }
    
    /* Add to device list */
    device->next = g_input_manager.device_list;
    g_input_manager.device_list = device;
    device->connected = true;
    
    g_input_manager.active_device_count++;
    
    /* Generate device connect event */
    input_event_t connect_event;
    memset(&connect_event, 0, sizeof(input_event_t));
    connect_event.type = INPUT_EVENT_DEVICE_CONNECT;
    connect_event.timestamp = input_get_timestamp();
    connect_event.device_id = device->device_id;
    connect_event.data.device.device_id = device->device_id;
    connect_event.data.device.device_type = device->type;
    
    distribute_event(&connect_event);
    
    return INPUT_SUCCESS;
}

int input_unregister_device(uint32_t device_id) {
    if (!g_input_manager.initialized) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    input_device_t** current = &g_input_manager.device_list;
    while (*current) {
        if ((*current)->device_id == device_id) {
            input_device_t* device = *current;
            *current = device->next;
            
            /* Generate device disconnect event */
            input_event_t disconnect_event;
            memset(&disconnect_event, 0, sizeof(input_event_t));
            disconnect_event.type = INPUT_EVENT_DEVICE_DISCONNECT;
            disconnect_event.timestamp = input_get_timestamp();
            disconnect_event.device_id = device_id;
            disconnect_event.data.device.device_id = device_id;
            disconnect_event.data.device.device_type = device->type;
            
            distribute_event(&disconnect_event);
            
            /* Cleanup device */
            if (device->cleanup) {
                device->cleanup(device);
            }
            kfree(device);
            
            g_input_manager.active_device_count--;
            return INPUT_SUCCESS;
        }
        current = &(*current)->next;
    }
    
    return INPUT_ERROR_DEVICE_NOT_FOUND;
}

input_device_t* input_find_device(uint32_t device_id) {
    if (!g_input_manager.initialized) {
        return NULL;
    }
    
    input_device_t* device = g_input_manager.device_list;
    while (device) {
        if (device->device_id == device_id) {
            return device;
        }
        device = device->next;
    }
    
    return NULL;
}

int input_report_event(uint32_t device_id, input_event_t* event) {
    if (!event || !g_input_manager.initialized) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Find the device */
    input_device_t* device = input_find_device(device_id);
    if (!device || !device->connected) {
        return INPUT_ERROR_DEVICE_NOT_FOUND;
    }
    
    /* Set device ID and timestamp */
    event->device_id = device_id;
    if (event->timestamp == 0) {
        event->timestamp = input_get_timestamp();
    }
    
    /* Validate event */
    if (!input_event_validate(event)) {
        g_input_manager.total_events_dropped++;
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Update input state */
    update_input_state(event);
    
    /* Distribute event to applications */
    distribute_event(event);
    
    g_input_manager.total_events_processed++;
    
    return INPUT_SUCCESS;
}

/* ================================
 * Application Interface
 * ================================ */

int input_register_app(uint32_t pid, uint32_t subscription_mask) {
    if (!g_input_manager.initialized || pid == 0) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Check if app is already registered */
    input_context_t* existing = find_app_context(pid);
    if (existing) {
        /* Update subscription mask */
        existing->subscription_mask = subscription_mask;
        return INPUT_SUCCESS;
    }
    
    /* Find free context slot */
    for (int i = 0; i < INPUT_MAX_APPLICATIONS; i++) {
        input_context_t* context = &g_input_manager.application_contexts[i];
        if (context->pid == 0) {
            /* Initialize context */
            context->pid = pid;
            context->subscription_mask = subscription_mask;
            context->queue_size = INPUT_EVENT_QUEUE_SIZE;
            context->queue_head = 0;
            context->queue_tail = 0;
            context->queue_count = 0;
            context->has_focus = false;
            context->blocking_wait = false;
            context->wait_timeout = 0;
            context->wait_queue = NULL; /* TODO: Initialize wait queue */
            
            /* Allocate event queue */
            context->event_queue = (input_event_t*)kmalloc(sizeof(input_event_t) * context->queue_size);
            if (!context->event_queue) {
                context->pid = 0; /* Mark as unused */
                return INPUT_ERROR_NO_MEMORY;
            }
            
            g_input_manager.registered_app_count++;
            
            /* Give focus to first registered app */
            if (g_input_manager.focused_pid == 0) {
                input_set_focus(pid);
            }
            
            return INPUT_SUCCESS;
        }
    }
    
    return INPUT_ERROR_NO_MEMORY;
}

int input_unregister_app(uint32_t pid) {
    if (!g_input_manager.initialized || pid == 0) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    input_context_t* context = find_app_context(pid);
    if (!context) {
        return INPUT_ERROR_APP_NOT_FOUND;
    }
    
    /* Release focus if this app has it */
    if (g_input_manager.focused_pid == pid) {
        g_input_manager.focused_pid = 0;
    }
    
    /* Cleanup context */
    if (context->event_queue) {
        kfree(context->event_queue);
    }
    
    memset(context, 0, sizeof(input_context_t));
    g_input_manager.registered_app_count--;
    
    return INPUT_SUCCESS;
}

int input_set_focus(uint32_t pid) {
    if (!g_input_manager.initialized) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Validate that app is registered */
    if (pid != 0) {
        input_context_t* context = find_app_context(pid);
        if (!context) {
            return INPUT_ERROR_APP_NOT_FOUND;
        }
    }
    
    /* Update focus */
    uint32_t old_focus = g_input_manager.focused_pid;
    g_input_manager.focused_pid = pid;
    
    /* Update focus flags in contexts */
    for (int i = 0; i < INPUT_MAX_APPLICATIONS; i++) {
        input_context_t* context = &g_input_manager.application_contexts[i];
        if (context->pid != 0) {
            context->has_focus = (context->pid == pid);
        }
    }
    
    return INPUT_SUCCESS;
}

uint32_t input_get_focus(void) {
    return g_input_manager.focused_pid;
}

int input_poll_events(uint32_t pid, input_event_t* events, size_t max_events) {
    if (!events || max_events == 0 || !g_input_manager.initialized) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    input_context_t* context = find_app_context(pid);
    if (!context) {
        return INPUT_ERROR_APP_NOT_FOUND;
    }
    
    size_t events_returned = 0;
    
    /* Get events from queue */
    while (events_returned < max_events && !input_event_queue_is_empty(context->queue_count)) {
        if (input_event_queue_pop(context->event_queue, context->queue_size,
                                 &context->queue_head, &context->queue_tail, 
                                 &context->queue_count, &events[events_returned])) {
            events_returned++;
        }
    }
    
    return (int)events_returned;
}

int input_wait_events(uint32_t pid, input_event_t* events, size_t max_events, uint32_t timeout_ms) {
    if (!events || max_events == 0 || !g_input_manager.initialized) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    input_context_t* context = find_app_context(pid);
    if (!context) {
        return INPUT_ERROR_APP_NOT_FOUND;
    }
    
    /* First try non-blocking poll */
    int result = input_poll_events(pid, events, max_events);
    if (result > 0) {
        return result;
    }
    
    /* TODO: Implement blocking wait with timeout */
    /* This would involve sleeping the process and waking it when events arrive */
    
    /* For now, return timeout */
    return INPUT_ERROR_TIMEOUT;
}

int input_queue_event(uint32_t pid, input_event_t* event) {
    if (!event || !g_input_manager.initialized) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    input_context_t* context = find_app_context(pid);
    if (!context) {
        return INPUT_ERROR_APP_NOT_FOUND;
    }
    
    /* Add event to queue */
    if (input_event_queue_push(context->event_queue, context->queue_size,
                              &context->queue_head, &context->queue_tail,
                              &context->queue_count, event)) {
        return INPUT_SUCCESS;
    }
    
    g_input_manager.total_events_dropped++;
    return INPUT_ERROR_QUEUE_FULL;
}

/* ================================
 * Configuration and Control
 * ================================ */

int input_configure_device(uint32_t device_id, void* config) {
    if (!config || !g_input_manager.initialized) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    input_device_t* device = input_find_device(device_id);
    if (!device) {
        return INPUT_ERROR_DEVICE_NOT_FOUND;
    }
    
    if (device->configure) {
        return device->configure(device, config);
    }
    
    return INPUT_SUCCESS;
}

uint32_t input_get_device_capabilities(uint32_t device_id) {
    input_device_t* device = input_find_device(device_id);
    if (!device) {
        return 0;
    }
    
    return device->capabilities;
}

int input_set_mouse_position(int32_t x, int32_t y) {
    if (!g_input_manager.initialized) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    g_input_manager.current_state.mouse_x = x;
    g_input_manager.current_state.mouse_y = y;
    
    return INPUT_SUCCESS;
}

int input_get_mouse_position(int32_t* x, int32_t* y) {
    if (!x || !y || !g_input_manager.initialized) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    *x = g_input_manager.current_state.mouse_x;
    *y = g_input_manager.current_state.mouse_y;
    
    return INPUT_SUCCESS;
}

/* ================================
 * Statistics and Debug
 * ================================ */

void input_get_statistics(uint64_t* events_processed, uint64_t* events_dropped,
                         uint32_t* active_devices, uint32_t* registered_apps) {
    if (events_processed) *events_processed = g_input_manager.total_events_processed;
    if (events_dropped) *events_dropped = g_input_manager.total_events_dropped;
    if (active_devices) *active_devices = g_input_manager.active_device_count;
    if (registered_apps) *registered_apps = g_input_manager.registered_app_count;
}

void input_debug_print_devices(void) {
    /* TODO: Implement debug output */
}

void input_debug_print_apps(void) {
    /* TODO: Implement debug output */
}

/* ================================
 * Internal Helper Functions
 * ================================ */

static input_context_t* find_app_context(uint32_t pid) {
    for (int i = 0; i < INPUT_MAX_APPLICATIONS; i++) {
        input_context_t* context = &g_input_manager.application_contexts[i];
        if (context->pid == pid) {
            return context;
        }
    }
    return NULL;
}

static void update_input_state(const input_event_t* event) {
    switch (event->type) {
        case INPUT_EVENT_MOUSE_MOVE:
            g_input_manager.current_state.mouse_x = event->data.mouse_move.x;
            g_input_manager.current_state.mouse_y = event->data.mouse_move.y;
            break;
            
        case INPUT_EVENT_MOUSE_BUTTON_PRESS:
            g_input_manager.current_state.mouse_buttons |= event->data.mouse_button.button;
            break;
            
        case INPUT_EVENT_MOUSE_BUTTON_RELEASE:
            g_input_manager.current_state.mouse_buttons &= ~event->data.mouse_button.button;
            break;
            
        case INPUT_EVENT_KEY_PRESS:
            /* Update modifier state */
            g_input_manager.current_state.keyboard_modifiers |= event->data.key.modifiers;
            break;
            
        case INPUT_EVENT_KEY_RELEASE:
            /* Update modifier state */
            g_input_manager.current_state.keyboard_modifiers &= ~event->data.key.modifiers;
            break;
            
        default:
            break;
    }
}

static bool should_deliver_event(const input_context_t* context, const input_event_t* event) {
    /* Check subscription mask */
    switch (event->type) {
        case INPUT_EVENT_KEY_PRESS:
        case INPUT_EVENT_KEY_RELEASE:
            return (context->subscription_mask & INPUT_SUBSCRIBE_KEYBOARD) != 0;
            
        case INPUT_EVENT_MOUSE_MOVE:
        case INPUT_EVENT_MOUSE_BUTTON_PRESS:
        case INPUT_EVENT_MOUSE_BUTTON_RELEASE:
        case INPUT_EVENT_MOUSE_WHEEL:
            return (context->subscription_mask & INPUT_SUBSCRIBE_MOUSE) != 0;
            
        case INPUT_EVENT_DEVICE_CONNECT:
        case INPUT_EVENT_DEVICE_DISCONNECT:
            return true; /* Always deliver device events */
            
        default:
            return false;
    }
}

static void distribute_event(const input_event_t* event) {
    /* Distribute to focused application first */
    if (g_input_manager.focused_pid != 0) {
        input_context_t* focused_context = find_app_context(g_input_manager.focused_pid);
        if (focused_context && should_deliver_event(focused_context, event)) {
            input_queue_event(g_input_manager.focused_pid, (input_event_t*)event);
            return;
        }
    }
    
    /* If no focused app or focused app doesn't want the event, 
       distribute to other interested applications */
    for (int i = 0; i < INPUT_MAX_APPLICATIONS; i++) {
        input_context_t* context = &g_input_manager.application_contexts[i];
        if (context->pid != 0 && context->pid != g_input_manager.focused_pid) {
            if (should_deliver_event(context, event)) {
                input_queue_event(context->pid, (input_event_t*)event);
            }
        }
    }
}

/* Public wrapper for event distribution (for testing) */
void input_distribute_event(const input_event_t* event) {
    distribute_event(event);
}
