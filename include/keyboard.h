/* IKOS Keyboard Driver Header
 * Provides keyboard input handling and API for user-space applications
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Keyboard driver configuration */
#define KEYBOARD_BUFFER_SIZE    256     /* Input buffer size */
#define KEYBOARD_MAX_LISTENERS  16      /* Maximum event listeners */

/* Key codes and modifiers */
#define KEY_ESCAPE      0x01
#define KEY_1           0x02
#define KEY_2           0x03
#define KEY_3           0x04
#define KEY_4           0x05
#define KEY_5           0x06
#define KEY_6           0x07
#define KEY_7           0x08
#define KEY_8           0x09
#define KEY_9           0x0A
#define KEY_0           0x0B
#define KEY_MINUS       0x0C
#define KEY_EQUALS      0x0D
#define KEY_BACKSPACE   0x0E
#define KEY_TAB         0x0F
#define KEY_Q           0x10
#define KEY_W           0x11
#define KEY_E           0x12
#define KEY_R           0x13
#define KEY_T           0x14
#define KEY_Y           0x15
#define KEY_U           0x16
#define KEY_I           0x17
#define KEY_O           0x18
#define KEY_P           0x19
#define KEY_LBRACKET    0x1A
#define KEY_RBRACKET    0x1B
#define KEY_ENTER       0x1C
#define KEY_LCTRL       0x1D
#define KEY_A           0x1E
#define KEY_S           0x1F
#define KEY_D           0x20
#define KEY_F           0x21
#define KEY_G           0x22
#define KEY_H           0x23
#define KEY_J           0x24
#define KEY_K           0x25
#define KEY_L           0x26
#define KEY_SEMICOLON   0x27
#define KEY_APOSTROPHE  0x28
#define KEY_GRAVE       0x29
#define KEY_LSHIFT      0x2A
#define KEY_BACKSLASH   0x2B
#define KEY_Z           0x2C
#define KEY_X           0x2D
#define KEY_C           0x2E
#define KEY_V           0x2F
#define KEY_B           0x30
#define KEY_N           0x31
#define KEY_M           0x32
#define KEY_COMMA       0x33
#define KEY_PERIOD      0x34
#define KEY_SLASH       0x35
#define KEY_RSHIFT      0x36
#define KEY_MULTIPLY    0x37
#define KEY_LALT        0x38
#define KEY_SPACE       0x39
#define KEY_CAPSLOCK    0x3A

/* Function keys */
#define KEY_F1          0x3B
#define KEY_F2          0x3C
#define KEY_F3          0x3D
#define KEY_F4          0x3E
#define KEY_F5          0x3F
#define KEY_F6          0x40
#define KEY_F7          0x41
#define KEY_F8          0x42
#define KEY_F9          0x43
#define KEY_F10         0x44

/* Modifier flags */
#define MOD_SHIFT       0x01
#define MOD_CTRL        0x02
#define MOD_ALT         0x04
#define MOD_CAPS        0x08

/* Key event types */
typedef enum {
    KEY_EVENT_PRESS = 0,
    KEY_EVENT_RELEASE = 1
} key_event_type_t;

/* Key event structure */
typedef struct {
    uint8_t scancode;           /* Raw scancode */
    uint8_t keycode;            /* Translated keycode */
    char ascii;                 /* ASCII character (if printable) */
    uint8_t modifiers;          /* Modifier flags */
    key_event_type_t type;      /* Press or release */
    uint64_t timestamp;         /* Event timestamp */
} key_event_t;

/* Keyboard state structure */
typedef struct {
    uint8_t modifiers;          /* Current modifier state */
    bool caps_lock;             /* Caps lock state */
    bool num_lock;              /* Num lock state */
    bool scroll_lock;           /* Scroll lock state */
    
    /* Input buffer */
    key_event_t buffer[KEYBOARD_BUFFER_SIZE];
    size_t buffer_head;         /* Write position */
    size_t buffer_tail;         /* Read position */
    size_t buffer_count;        /* Number of events in buffer */
    
    /* Statistics */
    uint64_t total_events;      /* Total events processed */
    uint64_t dropped_events;    /* Events dropped due to full buffer */
} keyboard_state_t;

/* Keyboard event listener callback */
typedef void (*keyboard_listener_t)(const key_event_t* event, void* user_data);

/* Keyboard listener registration */
typedef struct {
    keyboard_listener_t callback;
    void* user_data;
    bool active;
} keyboard_listener_reg_t;

/* ================================
 * Core Keyboard Driver Functions
 * ================================ */

/* Initialize keyboard driver */
int keyboard_init(void);

/* Cleanup keyboard driver */
void keyboard_cleanup(void);

/* Process raw keyboard interrupt */
void keyboard_interrupt_handler(void);

/* Get keyboard driver statistics */
void keyboard_get_stats(keyboard_state_t* stats);

/* Reset keyboard state */
void keyboard_reset(void);

/* ================================
 * Input Buffer Management
 * ================================ */

/* Check if keyboard buffer has data */
bool keyboard_has_data(void);

/* Get next key event from buffer (blocking) */
int keyboard_get_event(key_event_t* event);

/* Get next key event from buffer (non-blocking) */
int keyboard_get_event_nonblock(key_event_t* event);

/* Get ASCII character from buffer (blocking) */
char keyboard_getchar(void);

/* Get ASCII character from buffer (non-blocking) */
int keyboard_getchar_nonblock(void);

/* Peek at next event without removing it */
int keyboard_peek_event(key_event_t* event);

/* Clear input buffer */
void keyboard_clear_buffer(void);

/* ================================
 * Event Listener System
 * ================================ */

/* Register keyboard event listener */
int keyboard_register_listener(keyboard_listener_t callback, void* user_data);

/* Unregister keyboard event listener */
int keyboard_unregister_listener(int listener_id);

/* Enable/disable specific listener */
int keyboard_set_listener_enabled(int listener_id, bool enabled);

/* ================================
 * Key Mapping and Translation
 * ================================ */

/* Translate scancode to keycode */
uint8_t keyboard_scancode_to_keycode(uint8_t scancode);

/* Translate keycode to ASCII character */
char keyboard_keycode_to_ascii(uint8_t keycode, uint8_t modifiers);

/* Check if key is a modifier key */
bool keyboard_is_modifier_key(uint8_t keycode);

/* Get current modifier state */
uint8_t keyboard_get_modifiers(void);

/* Set modifier state */
void keyboard_set_modifiers(uint8_t modifiers);

/* ================================
 * Hardware Interface
 * ================================ */

/* Read from keyboard controller */
uint8_t keyboard_read_data(void);

/* Write to keyboard controller */
void keyboard_write_data(uint8_t data);

/* Read keyboard controller status */
uint8_t keyboard_read_status(void);

/* Write keyboard controller command */
void keyboard_write_command(uint8_t command);

/* Wait for keyboard controller ready */
void keyboard_wait_ready(void);

/* ================================
 * User-Space API
 * ================================ */

/* System call interface for keyboard access */
int sys_keyboard_read(void* buffer, size_t count);
int sys_keyboard_poll(void);
int sys_keyboard_ioctl(int cmd, void* arg);

/* Keyboard configuration commands for ioctl */
#define KEYBOARD_IOCTL_GET_STATE        0x01
#define KEYBOARD_IOCTL_SET_LEDS         0x02
#define KEYBOARD_IOCTL_GET_MODIFIERS    0x03
#define KEYBOARD_IOCTL_CLEAR_BUFFER     0x04
#define KEYBOARD_IOCTL_GET_STATS        0x05

/* ================================
 * LED Control
 * ================================ */

/* LED flags */
#define LED_SCROLL_LOCK     0x01
#define LED_NUM_LOCK        0x02
#define LED_CAPS_LOCK       0x04

/* Set keyboard LEDs */
void keyboard_set_leds(uint8_t led_state);

/* Get current LED state */
uint8_t keyboard_get_leds(void);

/* ================================
 * Debugging and Diagnostics
 * ================================ */

/* Enable/disable keyboard debugging */
void keyboard_set_debug(bool enabled);

/* Print keyboard state */
void keyboard_print_state(void);

/* Print key mapping table */
void keyboard_print_keymap(void);

/* Test keyboard functionality */
int keyboard_self_test(void);

/* ================================
 * Error Codes
 * ================================ */

#define KEYBOARD_SUCCESS                0
#define KEYBOARD_ERROR_INIT             -1
#define KEYBOARD_ERROR_TIMEOUT          -2
#define KEYBOARD_ERROR_BUFFER_FULL      -3
#define KEYBOARD_ERROR_BUFFER_EMPTY     -4
#define KEYBOARD_ERROR_INVALID_PARAM    -5
#define KEYBOARD_ERROR_NOT_READY        -6
#define KEYBOARD_ERROR_HARDWARE         -7
#define KEYBOARD_ERROR_LISTENER_FULL    -8
#define KEYBOARD_ERROR_LISTENER_INVALID -9

/* ================================
 * Hardware Constants
 * ================================ */

/* Keyboard controller ports */
#define KEYBOARD_DATA_PORT      0x60
#define KEYBOARD_STATUS_PORT    0x64
#define KEYBOARD_COMMAND_PORT   0x64

/* Keyboard controller status flags */
#define KEYBOARD_STATUS_OUTPUT_FULL     0x01
#define KEYBOARD_STATUS_INPUT_FULL      0x02
#define KEYBOARD_STATUS_SYSTEM_FLAG     0x04
#define KEYBOARD_STATUS_COMMAND_DATA    0x08
#define KEYBOARD_STATUS_KEYBOARD_LOCK   0x10
#define KEYBOARD_STATUS_MOUSE_DATA      0x20
#define KEYBOARD_STATUS_TIMEOUT_ERROR   0x40
#define KEYBOARD_STATUS_PARITY_ERROR    0x80

/* Keyboard controller commands */
#define KEYBOARD_CMD_READ_CONFIG        0x20
#define KEYBOARD_CMD_WRITE_CONFIG       0x60
#define KEYBOARD_CMD_DISABLE_MOUSE      0xA7
#define KEYBOARD_CMD_ENABLE_MOUSE       0xA8
#define KEYBOARD_CMD_TEST_MOUSE         0xA9
#define KEYBOARD_CMD_SELF_TEST          0xAA
#define KEYBOARD_CMD_TEST_KEYBOARD      0xAB
#define KEYBOARD_CMD_DISABLE_KEYBOARD   0xAD
#define KEYBOARD_CMD_ENABLE_KEYBOARD    0xAE

/* Keyboard commands */
#define KEYBOARD_CMD_SET_LEDS           0xED
#define KEYBOARD_CMD_ECHO               0xEE
#define KEYBOARD_CMD_SET_SCANCODE       0xF0
#define KEYBOARD_CMD_GET_ID             0xF2
#define KEYBOARD_CMD_SET_REPEAT         0xF3
#define KEYBOARD_CMD_ENABLE             0xF4
#define KEYBOARD_CMD_DISABLE            0xF5
#define KEYBOARD_CMD_RESET              0xFF

/* Special scancodes */
#define SCANCODE_EXTENDED_PREFIX        0xE0
#define SCANCODE_RELEASE_FLAG           0x80

#endif /* KEYBOARD_H */
