/* IKOS Input System Header
 * Unified input handling for keyboard and mouse devices
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Maximum number of input devices and applications */
#define INPUT_MAX_DEVICES       16
#define INPUT_MAX_APPLICATIONS  32
#define INPUT_EVENT_QUEUE_SIZE  256
#define INPUT_DEVICE_NAME_LEN   64

/* Input event types */
typedef enum {
    INPUT_EVENT_KEY_PRESS,
    INPUT_EVENT_KEY_RELEASE,
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_MOUSE_BUTTON_PRESS,
    INPUT_EVENT_MOUSE_BUTTON_RELEASE,
    INPUT_EVENT_MOUSE_WHEEL,
    INPUT_EVENT_DEVICE_CONNECT,
    INPUT_EVENT_DEVICE_DISCONNECT
} input_event_type_t;

/* Input device types */
typedef enum {
    INPUT_DEVICE_KEYBOARD,
    INPUT_DEVICE_MOUSE,
    INPUT_DEVICE_TOUCHPAD,
    INPUT_DEVICE_GAMEPAD,
    INPUT_DEVICE_TOUCHSCREEN
} input_device_type_t;

/* Input device capabilities */
#define INPUT_CAP_KEYS          0x01
#define INPUT_CAP_BUTTONS       0x02
#define INPUT_CAP_RELATIVE      0x04
#define INPUT_CAP_ABSOLUTE      0x08
#define INPUT_CAP_WHEEL         0x10

/* Keyboard modifier flags */
#define INPUT_MOD_SHIFT         0x01
#define INPUT_MOD_CTRL          0x02
#define INPUT_MOD_ALT           0x04
#define INPUT_MOD_SUPER         0x08
#define INPUT_MOD_CAPS          0x10
#define INPUT_MOD_NUM           0x20
#define INPUT_MOD_SCROLL        0x40

/* Mouse button flags */
#define INPUT_MOUSE_LEFT        0x01
#define INPUT_MOUSE_RIGHT       0x02
#define INPUT_MOUSE_MIDDLE      0x04
#define INPUT_MOUSE_SIDE1       0x08
#define INPUT_MOUSE_SIDE2       0x10

/* Event subscription masks */
#define INPUT_SUBSCRIBE_KEYBOARD    0x01
#define INPUT_SUBSCRIBE_MOUSE       0x02
#define INPUT_SUBSCRIBE_ALL         0xFF

/* Input event structure */
typedef struct {
    input_event_type_t type;
    uint64_t timestamp;
    uint32_t device_id;
    
    union {
        struct {
            uint32_t keycode;
            uint32_t modifiers;
            uint32_t unicode;
        } key;
        
        struct {
            int32_t x, y;
            int32_t delta_x, delta_y;
        } mouse_move;
        
        struct {
            uint32_t button;
            int32_t x, y;
        } mouse_button;
        
        struct {
            int32_t delta_x, delta_y;
            int32_t x, y;
        } mouse_wheel;
        
        struct {
            uint32_t device_id;
            input_device_type_t device_type;
        } device;
    } data;
} input_event_t;

/* Keyboard configuration structure */
typedef struct {
    uint32_t repeat_delay;   /* Key repeat delay in milliseconds */
    uint32_t repeat_rate;    /* Key repeat rate in Hz */
    uint32_t modifier_mask;  /* Active modifier mask */
} input_keyboard_config_t;

/* Mouse configuration structure */
typedef struct {
    uint32_t sensitivity;    /* Mouse sensitivity (1-1000, 100 = 1.0x) */
    uint32_t acceleration;   /* Mouse acceleration (1-1000, 100 = 1.0x) */
    bool invert_x;          /* Invert X axis */
    bool invert_y;          /* Invert Y axis */
    uint32_t button_mapping[8]; /* Button remapping */
} input_mouse_config_t;

/* Keyboard state structure */
typedef struct {
    uint32_t modifiers;      /* Current modifier state */
    uint32_t last_keycode;   /* Last pressed key */
    uint64_t last_press_time; /* Time of last key press */
    bool repeat_active;      /* Key repeat active */
} keyboard_state_t;

/* Key event structure (for compatibility with existing keyboard driver) */
typedef struct {
    uint32_t type;          /* KEY_EVENT_PRESS or KEY_EVENT_RELEASE */
    uint32_t scancode;      /* Hardware scancode */
    uint32_t modifiers;     /* Modifier state */
} key_event_t;

/* Key event types */
#define KEY_EVENT_PRESS     1
#define KEY_EVENT_RELEASE   2

/* Input device structure */
typedef struct input_device {
    uint32_t device_id;
    char name[INPUT_DEVICE_NAME_LEN];
    input_device_type_t type;
    uint32_t capabilities;
    void* device_data;
    bool connected;
    
    /* Device operations */
    int (*read_event)(struct input_device* dev, input_event_t* event);
    int (*configure)(struct input_device* dev, void* config);
    int (*reset)(struct input_device* dev);
    void (*cleanup)(struct input_device* dev);
    
    struct input_device* next;
} input_device_t;

/* Application input context */
typedef struct {
    uint32_t pid;
    uint32_t subscription_mask;
    input_event_t* event_queue;
    size_t queue_size;
    size_t queue_head, queue_tail;
    size_t queue_count;
    bool has_focus;
    bool blocking_wait;
    uint32_t wait_timeout;
    void* wait_queue; /* Pointer to kernel wait queue */
} input_context_t;

/* Input state structure */
typedef struct {
    /* Mouse state */
    int32_t mouse_x, mouse_y;
    uint32_t mouse_buttons;
    
    /* Keyboard state */
    uint32_t keyboard_modifiers;
    bool caps_lock, num_lock, scroll_lock;
    
    /* Focus information */
    uint32_t focused_pid;
    
    /* Statistics */
    uint64_t events_processed;
    uint64_t events_dropped;
    uint32_t active_devices;
    uint32_t registered_apps;
} input_state_t;

/* Error codes */
#define INPUT_SUCCESS               0
#define INPUT_ERROR_INVALID_PARAM   -1
#define INPUT_ERROR_NO_MEMORY       -2
#define INPUT_ERROR_DEVICE_EXISTS   -3
#define INPUT_ERROR_DEVICE_NOT_FOUND -4
#define INPUT_ERROR_APP_EXISTS      -5
#define INPUT_ERROR_APP_NOT_FOUND   -6
#define INPUT_ERROR_QUEUE_FULL      -7
#define INPUT_ERROR_NO_FOCUS        -8
#define INPUT_ERROR_TIMEOUT         -9

/* ================================
 * Core Input System Functions
 * ================================ */

/* Initialize input system */
int input_init(void);

/* Cleanup input system */
void input_cleanup(void);

/* Get current input state */
int input_get_state(input_state_t* state);

/* ================================
 * Device Management
 * ================================ */

/* Register input device */
int input_register_device(input_device_t* device);

/* Unregister input device */
int input_unregister_device(uint32_t device_id);

/* Find device by ID */
input_device_t* input_find_device(uint32_t device_id);

/* Report event from device */
int input_report_event(uint32_t device_id, input_event_t* event);

/* ================================
 * Application Interface
 * ================================ */

/* Register application for input events */
int input_register_app(uint32_t pid, uint32_t subscription_mask);

/* Unregister application */
int input_unregister_app(uint32_t pid);

/* Set application focus */
int input_set_focus(uint32_t pid);

/* Get focused application */
uint32_t input_get_focus(void);

/* Poll for events (non-blocking) */
int input_poll_events(uint32_t pid, input_event_t* events, size_t max_events);

/* Wait for events (blocking) */
int input_wait_events(uint32_t pid, input_event_t* events, size_t max_events, uint32_t timeout_ms);

/* Queue event for application */
int input_queue_event(uint32_t pid, input_event_t* event);

/* Distribute event to applications */
void input_distribute_event(const input_event_t* event);

/* ================================
 * Configuration and Control
 * ================================ */

/* Configure input device */
int input_configure_device(uint32_t device_id, void* config);

/* Get device capabilities */
uint32_t input_get_device_capabilities(uint32_t device_id);

/* Set mouse position */
int input_set_mouse_position(int32_t x, int32_t y);

/* Get mouse position */
int input_get_mouse_position(int32_t* x, int32_t* y);

/* ================================
 * Statistics and Debug
 * ================================ */

/* Get input system statistics */
void input_get_statistics(uint64_t* events_processed, uint64_t* events_dropped,
                         uint32_t* active_devices, uint32_t* registered_apps);

/* Debug print device list */
void input_debug_print_devices(void);

/* Debug print application contexts */
void input_debug_print_apps(void);

#endif /* INPUT_H */
