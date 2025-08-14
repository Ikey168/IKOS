#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include "ipc.h"

/* Keyboard Constants */
#define KBD_BUFFER_SIZE 256
#define KBD_MAX_SCANCODE 128
#define KBD_MAX_APPLICATIONS 16

/* Key States */
#define KEY_PRESSED  1
#define KEY_RELEASED 0

/* Special Keys */
#define KEY_ESC     0x01
#define KEY_ENTER   0x1C
#define KEY_SPACE   0x39
#define KEY_BACKSPACE 0x0E
#define KEY_TAB     0x0F
#define KEY_LSHIFT  0x2A
#define KEY_RSHIFT  0x36
#define KEY_LCTRL   0x1D
#define KEY_LALT    0x38
#define KEY_CAPS    0x3A

/* Function Keys */
#define KEY_F1      0x3B
#define KEY_F2      0x3C
#define KEY_F3      0x3D
#define KEY_F4      0x3E
#define KEY_F5      0x3F
#define KEY_F6      0x40
#define KEY_F7      0x41
#define KEY_F8      0x42
#define KEY_F9      0x43
#define KEY_F10     0x44

/* Arrow Keys */
#define KEY_UP      0x48
#define KEY_DOWN    0x50
#define KEY_LEFT    0x4B
#define KEY_RIGHT   0x4D

/* Keyboard Event Structure */
typedef struct {
    uint8_t scancode;
    uint8_t ascii;
    uint8_t state;      // KEY_PRESSED or KEY_RELEASED
    uint32_t timestamp;
    uint8_t modifiers;  // Shift, Ctrl, Alt flags
} kbd_event_t;

/* Keyboard Buffer Entry */
typedef struct {
    kbd_event_t event;
    bool valid;
} kbd_buffer_entry_t;

/* Keyboard Driver State */
typedef struct {
    kbd_buffer_entry_t buffer[KBD_BUFFER_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    
    bool shift_pressed;
    bool ctrl_pressed;
    bool alt_pressed;
    bool caps_lock;
    
    uint32_t registered_apps[KBD_MAX_APPLICATIONS];
    uint32_t app_count;
    
    bool driver_active;
    uint32_t events_processed;
    uint32_t events_dropped;
} kbd_driver_state_t;

/* Function Prototypes */
int kbd_init(void);
int kbd_cleanup(void);
int kbd_register_application(uint32_t pid);
int kbd_unregister_application(uint32_t pid);
int kbd_get_event(kbd_event_t* event, uint32_t flags);
int kbd_peek_event(kbd_event_t* event);
int kbd_handle_interrupt(uint8_t scancode);
uint8_t kbd_scancode_to_ascii(uint8_t scancode, bool shift, bool caps);
int kbd_set_led_state(uint8_t led_mask);
kbd_driver_state_t* kbd_get_state(void);

/* API Functions for Applications */
int kbd_api_init(void);
int kbd_api_read_key(kbd_event_t* event, uint32_t timeout_ms);
int kbd_api_check_key(void);
int kbd_api_subscribe_events(void);
int kbd_api_unsubscribe_events(void);

#endif /* KEYBOARD_H */
