/* IKOS Input Mouse Driver Integration
 * Mouse input handling with PS/2 protocol support for the unified input system
 * 
 * This module provides:
 * - PS/2 mouse protocol packet processing and interpretation
 * - Mouse movement tracking with configurable sensitivity and acceleration
 * - Button state management with configurable button mapping
 * - Mouse bounds checking and constraint handling
 * - Device configuration for sensitivity, acceleration, and axis inversion
 * - Wheel scroll event processing and delta calculation
 * 
 * The mouse driver processes raw PS/2 packets and converts them into
 * standardized input events with proper scaling, acceleration, and
 * button mapping. It supports both relative movement tracking and
 * absolute position reporting for GUI applications.
 */

#include "input.h"
#include "input_events.h"
#include "memory.h"
#include <string.h>

/* Mouse device structure */
typedef struct {
    input_device_t device;
    input_mouse_config_t config;
    
    /* Mouse state */
    int32_t x, y;
    uint32_t buttons;
    int32_t wheel_x, wheel_y;
    
    /* Movement accumulation */
    int32_t accum_x, accum_y;
    
    /* Bounds */
    int32_t min_x, min_y;
    int32_t max_x, max_y;
    
    /* PS/2 mouse state */
    uint8_t packet_buffer[4];
    int packet_index;
    bool packet_ready;
} input_mouse_device_t;

/* Global mouse device instance */
static input_mouse_device_t* g_mouse_device = NULL;

/* Internal function prototypes */
static int mouse_device_read_event(input_device_t* dev, input_event_t* event);
static int mouse_device_configure(input_device_t* dev, void* config);
static int mouse_device_reset(input_device_t* dev);
static void mouse_device_cleanup(input_device_t* dev);
static void process_mouse_packet(input_mouse_device_t* mouse_dev);
static void apply_mouse_acceleration(input_mouse_device_t* mouse_dev, int32_t* dx, int32_t* dy);
static void clamp_mouse_position(input_mouse_device_t* mouse_dev);

/* ================================
 * Mouse Device Initialization
 * ================================ */

int input_mouse_init(void) {
    if (g_mouse_device) {
        return INPUT_SUCCESS; /* Already initialized */
    }
    
    /* Allocate mouse device */
    g_mouse_device = (input_mouse_device_t*)kmalloc(sizeof(input_mouse_device_t));
    if (!g_mouse_device) {
        return INPUT_ERROR_NO_MEMORY;
    }
    
    memset(g_mouse_device, 0, sizeof(input_mouse_device_t));
    
    /* Initialize device structure */
    g_mouse_device->device.device_id = 0; /* Will be assigned by input manager */
    strncpy(g_mouse_device->device.name, "PS/2 Mouse", INPUT_DEVICE_NAME_LEN - 1);
    g_mouse_device->device.type = INPUT_DEVICE_MOUSE;
    g_mouse_device->device.capabilities = INPUT_CAP_BUTTONS | INPUT_CAP_RELATIVE | INPUT_CAP_WHEEL;
    g_mouse_device->device.device_data = g_mouse_device;
    g_mouse_device->device.connected = false;
    
    /* Set device operations */
    g_mouse_device->device.read_event = mouse_device_read_event;
    g_mouse_device->device.configure = mouse_device_configure;
    g_mouse_device->device.reset = mouse_device_reset;
    g_mouse_device->device.cleanup = mouse_device_cleanup;
    
    /* Initialize default configuration */
    g_mouse_device->config.acceleration = 100;    /* 1.0x acceleration */
    g_mouse_device->config.sensitivity = 100;     /* 1.0x sensitivity */
    g_mouse_device->config.invert_x = false;
    g_mouse_device->config.invert_y = false;
    
    /* Initialize button mapping (1:1 by default) */
    for (int i = 0; i < 8; i++) {
        g_mouse_device->config.button_mapping[i] = i;
    }
    
    /* Initialize mouse state */
    g_mouse_device->x = 400; /* Default center position */
    g_mouse_device->y = 300;
    g_mouse_device->buttons = 0;
    g_mouse_device->wheel_x = 0;
    g_mouse_device->wheel_y = 0;
    g_mouse_device->accum_x = 0;
    g_mouse_device->accum_y = 0;
    
    /* Set default bounds (800x600 screen) */
    g_mouse_device->min_x = 0;
    g_mouse_device->min_y = 0;
    g_mouse_device->max_x = 799;
    g_mouse_device->max_y = 599;
    
    /* Initialize PS/2 packet state */
    g_mouse_device->packet_index = 0;
    g_mouse_device->packet_ready = false;
    
    /* Register with input manager */
    int result = input_register_device(&g_mouse_device->device);
    if (result != INPUT_SUCCESS) {
        kfree(g_mouse_device);
        g_mouse_device = NULL;
        return result;
    }
    
    return INPUT_SUCCESS;
}

void input_mouse_cleanup(void) {
    if (!g_mouse_device) {
        return;
    }
    
    /* Unregister from input manager */
    input_unregister_device(g_mouse_device->device.device_id);
    
    /* Cleanup */
    kfree(g_mouse_device);
    g_mouse_device = NULL;
}

/* ================================
 * Device Operation Implementations
 * ================================ */

static int mouse_device_read_event(input_device_t* dev, input_event_t* event) {
    if (!dev || !event) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    input_mouse_device_t* mouse_dev = (input_mouse_device_t*)dev->device_data;
    if (!mouse_dev) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* This function would be called by the input manager to poll for events */
    /* In our case, events are generated by the interrupt handler */
    return INPUT_SUCCESS;
}

static int mouse_device_configure(input_device_t* dev, void* config) {
    if (!dev || !config) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    input_mouse_device_t* mouse_dev = (input_mouse_device_t*)dev->device_data;
    if (!mouse_dev) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    input_mouse_config_t* mouse_config = (input_mouse_config_t*)config;
    
    /* Validate configuration */
    if (mouse_config->acceleration < 1 || mouse_config->acceleration > 1000) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    if (mouse_config->sensitivity < 1 || mouse_config->sensitivity > 1000) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Update configuration */
    mouse_dev->config = *mouse_config;
    
    return INPUT_SUCCESS;
}

static int mouse_device_reset(input_device_t* dev) {
    if (!dev) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    input_mouse_device_t* mouse_dev = (input_mouse_device_t*)dev->device_data;
    if (!mouse_dev) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    /* Reset mouse state */
    mouse_dev->buttons = 0;
    mouse_dev->wheel_x = 0;
    mouse_dev->wheel_y = 0;
    mouse_dev->accum_x = 0;
    mouse_dev->accum_y = 0;
    mouse_dev->packet_index = 0;
    mouse_dev->packet_ready = false;
    
    /* Reset to center position */
    mouse_dev->x = (mouse_dev->max_x - mouse_dev->min_x) / 2;
    mouse_dev->y = (mouse_dev->max_y - mouse_dev->min_y) / 2;
    
    return INPUT_SUCCESS;
}

static void mouse_device_cleanup(input_device_t* dev) {
    if (!dev) {
        return;
    }
    
    input_mouse_device_t* mouse_dev = (input_mouse_device_t*)dev->device_data;
    if (!mouse_dev) {
        return;
    }
    
    /* Cleanup resources */
    /* The device structure itself will be freed by the caller */
}

/* ================================
 * PS/2 Mouse Protocol Processing
 * ================================ */

void input_mouse_interrupt_handler(uint8_t data) {
    if (!g_mouse_device) {
        return;
    }
    
    /* Collect PS/2 mouse packet bytes */
    g_mouse_device->packet_buffer[g_mouse_device->packet_index] = data;
    g_mouse_device->packet_index++;
    
    /* Standard PS/2 mouse packet is 3 bytes */
    if (g_mouse_device->packet_index >= 3) {
        g_mouse_device->packet_ready = true;
        process_mouse_packet(g_mouse_device);
        g_mouse_device->packet_index = 0;
        g_mouse_device->packet_ready = false;
    }
}

static void process_mouse_packet(input_mouse_device_t* mouse_dev) {
    if (!mouse_dev || !mouse_dev->packet_ready) {
        return;
    }
    
    uint8_t* packet = mouse_dev->packet_buffer;
    
    /* Validate packet format */
    if ((packet[0] & 0x08) == 0) {
        /* Invalid packet - bit 3 should always be 1 */
        return;
    }
    
    /* Extract button states */
    uint32_t new_buttons = 0;
    if (packet[0] & 0x01) new_buttons |= INPUT_MOUSE_LEFT;
    if (packet[0] & 0x02) new_buttons |= INPUT_MOUSE_RIGHT;
    if (packet[0] & 0x04) new_buttons |= INPUT_MOUSE_MIDDLE;
    
    /* Extract movement data */
    int32_t dx = (int32_t)packet[1];
    int32_t dy = (int32_t)packet[2];
    
    /* Handle sign extension for 9-bit values */
    if (packet[0] & 0x10) dx |= 0xFFFFFF00; /* Sign extend X */
    if (packet[0] & 0x20) dy |= 0xFFFFFF00; /* Sign extend Y */
    
    /* Invert Y axis (PS/2 Y increases downward, we want upward) */
    dy = -dy;
    
    /* Apply configuration */
    if (mouse_dev->config.invert_x) dx = -dx;
    if (mouse_dev->config.invert_y) dy = -dy;
    
    /* Apply sensitivity */
    dx = (dx * (int32_t)mouse_dev->config.sensitivity) / 100;
    dy = (dy * (int32_t)mouse_dev->config.sensitivity) / 100;
    
    /* Apply acceleration */
    apply_mouse_acceleration(mouse_dev, &dx, &dy);
    
    /* Update accumulated movement */
    mouse_dev->accum_x += dx;
    mouse_dev->accum_y += dy;
    
    /* Update absolute position */
    int32_t old_x = mouse_dev->x;
    int32_t old_y = mouse_dev->y;
    
    mouse_dev->x += dx;
    mouse_dev->y += dy;
    
    /* Clamp to bounds */
    clamp_mouse_position(mouse_dev);
    
    /* Calculate actual movement after clamping */
    int32_t actual_dx = mouse_dev->x - old_x;
    int32_t actual_dy = mouse_dev->y - old_y;
    
    /* Generate movement event if there was movement */
    if (actual_dx != 0 || actual_dy != 0) {
        input_event_t move_event;
        memset(&move_event, 0, sizeof(input_event_t));
        
        move_event.type = INPUT_EVENT_MOUSE_MOVE;
        move_event.timestamp = input_get_timestamp();
        move_event.device_id = mouse_dev->device.device_id;
        move_event.data.mouse_move.x = mouse_dev->x;
        move_event.data.mouse_move.y = mouse_dev->y;
        move_event.data.mouse_move.delta_x = actual_dx;
        move_event.data.mouse_move.delta_y = actual_dy;
        
        input_report_event(mouse_dev->device.device_id, &move_event);
    }
    
    /* Generate button events if buttons changed */
    uint32_t button_changes = new_buttons ^ mouse_dev->buttons;
    
    for (int i = 0; i < 3; i++) {
        uint32_t button_mask = 1 << i;
        if (button_changes & button_mask) {
            input_event_t button_event;
            memset(&button_event, 0, sizeof(input_event_t));
            
            button_event.type = (new_buttons & button_mask) ? 
                               INPUT_EVENT_MOUSE_BUTTON_PRESS : 
                               INPUT_EVENT_MOUSE_BUTTON_RELEASE;
            button_event.timestamp = input_get_timestamp();
            button_event.device_id = mouse_dev->device.device_id;
            button_event.data.mouse_button.button = button_mask;
            button_event.data.mouse_button.x = mouse_dev->x;
            button_event.data.mouse_button.y = mouse_dev->y;
            
            input_report_event(mouse_dev->device.device_id, &button_event);
        }
    }
    
    /* Update button state */
    mouse_dev->buttons = new_buttons;
}

static void apply_mouse_acceleration(input_mouse_device_t* mouse_dev, int32_t* dx, int32_t* dy) {
    if (!mouse_dev || !dx || !dy) {
        return;
    }
    
    /* Simple acceleration based on movement magnitude */
    int32_t magnitude = (*dx * *dx) + (*dy * *dy);
    
    if (magnitude > 100) { /* Threshold for acceleration */
        int32_t accel_factor = mouse_dev->config.acceleration;
        *dx = (*dx * accel_factor) / 100;
        *dy = (*dy * accel_factor) / 100;
    }
}

static void clamp_mouse_position(input_mouse_device_t* mouse_dev) {
    if (!mouse_dev) {
        return;
    }
    
    if (mouse_dev->x < mouse_dev->min_x) {
        mouse_dev->x = mouse_dev->min_x;
    } else if (mouse_dev->x > mouse_dev->max_x) {
        mouse_dev->x = mouse_dev->max_x;
    }
    
    if (mouse_dev->y < mouse_dev->min_y) {
        mouse_dev->y = mouse_dev->min_y;
    } else if (mouse_dev->y > mouse_dev->max_y) {
        mouse_dev->y = mouse_dev->max_y;
    }
}

/* ================================
 * Public Interface Functions
 * ================================ */

int input_mouse_set_bounds(int32_t min_x, int32_t min_y, int32_t max_x, int32_t max_y) {
    if (!g_mouse_device) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    if (min_x >= max_x || min_y >= max_y) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    g_mouse_device->min_x = min_x;
    g_mouse_device->min_y = min_y;
    g_mouse_device->max_x = max_x;
    g_mouse_device->max_y = max_y;
    
    /* Clamp current position to new bounds */
    clamp_mouse_position(g_mouse_device);
    
    return INPUT_SUCCESS;
}

int input_mouse_get_position(int32_t* x, int32_t* y) {
    if (!g_mouse_device || !x || !y) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    *x = g_mouse_device->x;
    *y = g_mouse_device->y;
    
    return INPUT_SUCCESS;
}

int input_mouse_set_position(int32_t x, int32_t y) {
    if (!g_mouse_device) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    g_mouse_device->x = x;
    g_mouse_device->y = y;
    
    /* Clamp to bounds */
    clamp_mouse_position(g_mouse_device);
    
    /* Generate movement event */
    input_event_t move_event;
    memset(&move_event, 0, sizeof(input_event_t));
    
    move_event.type = INPUT_EVENT_MOUSE_MOVE;
    move_event.timestamp = input_get_timestamp();
    move_event.device_id = g_mouse_device->device.device_id;
    move_event.data.mouse_move.x = g_mouse_device->x;
    move_event.data.mouse_move.y = g_mouse_device->y;
    move_event.data.mouse_move.delta_x = 0; /* No relative movement */
    move_event.data.mouse_move.delta_y = 0;
    
    input_report_event(g_mouse_device->device.device_id, &move_event);
    
    return INPUT_SUCCESS;
}

uint32_t input_mouse_get_buttons(void) {
    if (!g_mouse_device) {
        return 0;
    }
    
    return g_mouse_device->buttons;
}

/* ================================
 * Mouse Wheel Support
 * ================================ */

void input_mouse_report_wheel(int32_t delta_x, int32_t delta_y) {
    if (!g_mouse_device) {
        return;
    }
    
    /* Generate wheel event */
    input_event_t wheel_event;
    memset(&wheel_event, 0, sizeof(input_event_t));
    
    wheel_event.type = INPUT_EVENT_MOUSE_WHEEL;
    wheel_event.timestamp = input_get_timestamp();
    wheel_event.device_id = g_mouse_device->device.device_id;
    wheel_event.data.mouse_wheel.delta_x = delta_x;
    wheel_event.data.mouse_wheel.delta_y = delta_y;
    wheel_event.data.mouse_wheel.x = g_mouse_device->x;
    wheel_event.data.mouse_wheel.y = g_mouse_device->y;
    
    input_report_event(g_mouse_device->device.device_id, &wheel_event);
}

/* ================================
 * Configuration Interface
 * ================================ */

int input_mouse_set_sensitivity(int32_t sensitivity) {
    if (!g_mouse_device) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    if (sensitivity < 1 || sensitivity > 1000) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    g_mouse_device->config.sensitivity = sensitivity;
    
    return INPUT_SUCCESS;
}

int input_mouse_set_acceleration(int32_t acceleration) {
    if (!g_mouse_device) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    if (acceleration < 1 || acceleration > 1000) {
        return INPUT_ERROR_INVALID_PARAM;
    }
    
    g_mouse_device->config.acceleration = acceleration;
    
    return INPUT_SUCCESS;
}

/* ================================
 * Status and Debug
 * ================================ */

bool input_mouse_is_initialized(void) {
    return g_mouse_device != NULL;
}

input_device_t* input_mouse_get_device(void) {
    return g_mouse_device ? &g_mouse_device->device : NULL;
}
