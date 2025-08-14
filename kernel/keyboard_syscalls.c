/* IKOS Keyboard System Calls
 * System call interface for keyboard driver access from user-space
 */

#include "keyboard.h"
#include "syscalls.h"
#include "process.h"
#include <stdint.h>
#include <stddef.h>

/* External functions */
extern int is_user_address(void* addr, size_t size);

/* Simple implementation for is_user_address if not available */
int is_user_address(void* addr, size_t size) {
    /* Simple check - in a real system this would validate user-space addresses */
    (void)size;
    return (addr != NULL && (uintptr_t)addr >= 0x400000);  /* Typical user-space start */
}

/* System call numbers for keyboard operations */
#define SYS_KEYBOARD_READ       140
#define SYS_KEYBOARD_POLL       141
#define SYS_KEYBOARD_IOCTL      142
#define SYS_KEYBOARD_GETCHAR    143

/* External syscall registration function */
extern int register_syscall(int syscall_num, void* handler);

/* Forward declarations */
static long sys_keyboard_read_handler(long arg1, long arg2, long arg3, long arg4, long arg5);
static long sys_keyboard_poll_handler(long arg1, long arg2, long arg3, long arg4, long arg5);
static long sys_keyboard_ioctl_handler(long arg1, long arg2, long arg3, long arg4, long arg5);
static long sys_keyboard_getchar_handler(long arg1, long arg2, long arg3, long arg4, long arg5);

/**
 * Initialize keyboard system calls
 */
int keyboard_syscalls_init(void) {
    int result;
    
    /* Register keyboard system calls */
    result = register_syscall(SYS_KEYBOARD_READ, sys_keyboard_read_handler);
    if (result != 0) {
        return result;
    }
    
    result = register_syscall(SYS_KEYBOARD_POLL, sys_keyboard_poll_handler);
    if (result != 0) {
        return result;
    }
    
    result = register_syscall(SYS_KEYBOARD_IOCTL, sys_keyboard_ioctl_handler);
    if (result != 0) {
        return result;
    }
    
    result = register_syscall(SYS_KEYBOARD_GETCHAR, sys_keyboard_getchar_handler);
    if (result != 0) {
        return result;
    }
    
    return 0;
}

/**
 * System call handler for keyboard_read
 * arg1: buffer pointer
 * arg2: buffer size
 */
static long sys_keyboard_read_handler(long arg1, long arg2, long arg3, long arg4, long arg5) {
    (void)arg3; (void)arg4; (void)arg5;  /* Unused parameters */
    
    void* buffer = (void*)arg1;
    size_t count = (size_t)arg2;
    
    /* Validate parameters */
    if (!buffer || count == 0) {
        return KEYBOARD_ERROR_INVALID_PARAM;
    }
    
    /* Validate user-space buffer access */
    if (!is_user_address(buffer, count)) {
        return KEYBOARD_ERROR_INVALID_PARAM;
    }
    
    /* Call keyboard read function */
    return sys_keyboard_read(buffer, count);
}

/**
 * System call handler for keyboard_poll
 */
static long sys_keyboard_poll_handler(long arg1, long arg2, long arg3, long arg4, long arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;  /* Unused parameters */
    
    return sys_keyboard_poll();
}

/**
 * System call handler for keyboard_ioctl
 * arg1: command
 * arg2: argument pointer
 */
static long sys_keyboard_ioctl_handler(long arg1, long arg2, long arg3, long arg4, long arg5) {
    (void)arg3; (void)arg4; (void)arg5;  /* Unused parameters */
    
    int cmd = (int)arg1;
    void* arg = (void*)arg2;
    
    /* Validate argument pointer if provided */
    if (arg && !is_user_address(arg, sizeof(uint8_t))) {
        return KEYBOARD_ERROR_INVALID_PARAM;
    }
    
    /* Call keyboard ioctl function */
    return sys_keyboard_ioctl(cmd, arg);
}

/**
 * System call handler for keyboard_getchar
 */
static long sys_keyboard_getchar_handler(long arg1, long arg2, long arg3, long arg4, long arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;  /* Unused parameters */
    
    /* Get character from keyboard (blocking) */
    char c = keyboard_getchar();
    return (long)c;
}

/**
 * User-space wrapper functions
 * These would typically be in a user-space library
 */

#ifdef USER_SPACE_WRAPPERS

/**
 * User-space wrapper for keyboard_read
 */
int keyboard_read(void* buffer, size_t count) {
    return (int)syscall(SYS_KEYBOARD_READ, (long)buffer, (long)count, 0, 0, 0);
}

/**
 * User-space wrapper for keyboard_poll
 */
int keyboard_poll(void) {
    return (int)syscall(SYS_KEYBOARD_POLL, 0, 0, 0, 0, 0);
}

/**
 * User-space wrapper for keyboard_ioctl
 */
int keyboard_ioctl(int cmd, void* arg) {
    return (int)syscall(SYS_KEYBOARD_IOCTL, (long)cmd, (long)arg, 0, 0, 0);
}

/**
 * User-space wrapper for keyboard_getchar
 */
char keyboard_getchar_user(void) {
    return (char)syscall(SYS_KEYBOARD_GETCHAR, 0, 0, 0, 0, 0);
}

/**
 * User-space convenience functions
 */

/**
 * Non-blocking character read for user-space
 */
int keyboard_getchar_nonblock_user(void) {
    key_event_t event;
    int result = keyboard_read(&event, sizeof(event));
    
    if (result == sizeof(event) && event.type == KEY_EVENT_PRESS && event.ascii != 0) {
        return event.ascii;
    }
    
    return -1;  /* No character available */
}

/**
 * Wait for specific key press
 */
int keyboard_wait_for_key(uint8_t keycode) {
    key_event_t event;
    
    while (true) {
        if (keyboard_read(&event, sizeof(event)) == sizeof(event)) {
            if (event.type == KEY_EVENT_PRESS && event.keycode == keycode) {
                return 0;
            }
        }
    }
}

/**
 * Check if specific modifier is pressed
 */
bool keyboard_is_modifier_pressed(uint8_t modifier) {
    uint8_t modifiers;
    if (keyboard_ioctl(KEYBOARD_IOCTL_GET_MODIFIERS, &modifiers) == 0) {
        return (modifiers & modifier) != 0;
    }
    return false;
}

/**
 * Get keyboard state from user-space
 */
int keyboard_get_state_user(keyboard_state_t* state) {
    if (!state) {
        return -1;
    }
    
    return keyboard_ioctl(KEYBOARD_IOCTL_GET_STATE, state);
}

/**
 * Clear keyboard buffer from user-space
 */
int keyboard_clear_buffer_user(void) {
    return keyboard_ioctl(KEYBOARD_IOCTL_CLEAR_BUFFER, NULL);
}

/**
 * Set keyboard LEDs from user-space
 */
int keyboard_set_leds_user(uint8_t led_state) {
    return keyboard_ioctl(KEYBOARD_IOCTL_SET_LEDS, &led_state);
}

#endif /* USER_SPACE_WRAPPERS */
