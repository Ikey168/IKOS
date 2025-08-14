/* IKOS Keyboard User-Space API Implementation
 * High-level keyboard input functions for user applications
 */

#include "keyboard_user_api.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* External functions */
extern int printf(const char* format, ...);
extern int putchar(int c);
extern int puts(const char* s);

/* External string functions - declare but don't implement */
extern size_t strlen(const char* str);
extern int strcmp(const char* s1, const char* s2);
extern int strncmp(const char* s1, const char* s2, size_t n);

/* Simple sprintf - avoid snprintf conflicts */
static int simple_sprintf(char* buffer, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    int written = 0;
    const char* p = format;
    
    while (*p) {
        if (*p == '%') {
            p++;
            if (*p == 's') {
                /* String */
                const char* s = va_arg(args, const char*);
                while (*s) {
                    buffer[written++] = *s++;
                }
            } else if (*p == 'd') {
                /* Integer - simple implementation */
                int num = va_arg(args, int);
                if (num == 0) {
                    buffer[written++] = '0';
                } else {
                    char temp[16];
                    int i = 0;
                    bool neg = num < 0;
                    if (neg) {
                        num = -num;
                        buffer[written++] = '-';
                    }
                    while (num > 0) {
                        temp[i++] = '0' + (num % 10);
                        num /= 10;
                    }
                    while (i > 0) {
                        buffer[written++] = temp[--i];
                    }
                }
            } else if (*p == 'c') {
                /* Character */
                int c = va_arg(args, int);
                buffer[written++] = (char)c;
            } else if (*p == '%') {
                /* Literal % */
                buffer[written++] = '%';
            }
            p++;
        } else {
            buffer[written++] = *p++;
        }
    }
    
    buffer[written] = '\0';
    va_end(args);
    return written;
}

/* Key name lookup table */
static const char* key_names[] = {
    [KEY_ESCAPE] = "Escape",
    [KEY_1] = "1", [KEY_2] = "2", [KEY_3] = "3", [KEY_4] = "4", [KEY_5] = "5",
    [KEY_6] = "6", [KEY_7] = "7", [KEY_8] = "8", [KEY_9] = "9", [KEY_0] = "0",
    [KEY_MINUS] = "Minus", [KEY_EQUALS] = "Equals", [KEY_BACKSPACE] = "Backspace",
    [KEY_TAB] = "Tab", [KEY_Q] = "Q", [KEY_W] = "W", [KEY_E] = "E", [KEY_R] = "R",
    [KEY_T] = "T", [KEY_Y] = "Y", [KEY_U] = "U", [KEY_I] = "I", [KEY_O] = "O",
    [KEY_P] = "P", [KEY_LBRACKET] = "Left Bracket", [KEY_RBRACKET] = "Right Bracket",
    [KEY_ENTER] = "Enter", [KEY_LCTRL] = "Left Ctrl", [KEY_A] = "A", [KEY_S] = "S",
    [KEY_D] = "D", [KEY_F] = "F", [KEY_G] = "G", [KEY_H] = "H", [KEY_J] = "J",
    [KEY_K] = "K", [KEY_L] = "L", [KEY_SEMICOLON] = "Semicolon",
    [KEY_APOSTROPHE] = "Apostrophe", [KEY_GRAVE] = "Grave", [KEY_LSHIFT] = "Left Shift",
    [KEY_BACKSLASH] = "Backslash", [KEY_Z] = "Z", [KEY_X] = "X", [KEY_C] = "C",
    [KEY_V] = "V", [KEY_B] = "B", [KEY_N] = "N", [KEY_M] = "M", [KEY_COMMA] = "Comma",
    [KEY_PERIOD] = "Period", [KEY_SLASH] = "Slash", [KEY_RSHIFT] = "Right Shift",
    [KEY_MULTIPLY] = "Multiply", [KEY_LALT] = "Left Alt", [KEY_SPACE] = "Space",
    [KEY_CAPSLOCK] = "Caps Lock", [KEY_F1] = "F1", [KEY_F2] = "F2", [KEY_F3] = "F3",
    [KEY_F4] = "F4", [KEY_F5] = "F5", [KEY_F6] = "F6", [KEY_F7] = "F7",
    [KEY_F8] = "F8", [KEY_F9] = "F9", [KEY_F10] = "F10"
};

/* Error message lookup table */
static const char* error_messages[] = {
    [0] = "Success",
    [-KEYBOARD_ERROR_INIT] = "Initialization error",
    [-KEYBOARD_ERROR_TIMEOUT] = "Timeout error",
    [-KEYBOARD_ERROR_BUFFER_FULL] = "Buffer full",
    [-KEYBOARD_ERROR_BUFFER_EMPTY] = "Buffer empty",
    [-KEYBOARD_ERROR_INVALID_PARAM] = "Invalid parameter",
    [-KEYBOARD_ERROR_NOT_READY] = "Device not ready",
    [-KEYBOARD_ERROR_HARDWARE] = "Hardware error",
    [-KEYBOARD_ERROR_LISTENER_FULL] = "Listener table full",
    [-KEYBOARD_ERROR_LISTENER_INVALID] = "Invalid listener"
};

/* ================================
 * High-Level Input Functions
 * ================================ */

/**
 * Read a line of text from keyboard
 */
int keyboard_read_line(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return -1;
    }
    
    size_t pos = 0;
    char c;
    
    while (pos < size - 1) {
        c = keyboard_get_char();
        
        if (c == '\n' || c == '\r') {
            break;  /* End of line */
        } else if (c == '\b' || c == 127) {
            /* Backspace */
            if (pos > 0) {
                pos--;
                putchar('\b');
                putchar(' ');
                putchar('\b');
            }
        } else if (c >= 32 && c <= 126) {
            /* Printable character */
            buffer[pos++] = c;
            putchar(c);
        }
        /* Ignore other characters */
    }
    
    buffer[pos] = '\0';
    putchar('\n');
    
    return (int)pos;
}

/**
 * Read a string with echo
 */
int keyboard_read_string(const char* prompt, char* buffer, size_t size) {
    if (prompt) {
        printf("%s", prompt);
    }
    
    return keyboard_read_line(buffer, size);
}

/**
 * Read a password (no echo)
 */
int keyboard_read_password(const char* prompt, char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return -1;
    }
    
    if (prompt) {
        printf("%s", prompt);
    }
    
    size_t pos = 0;
    char c;
    
    while (pos < size - 1) {
        c = keyboard_get_char();
        
        if (c == '\n' || c == '\r') {
            break;  /* End of line */
        } else if (c == '\b' || c == 127) {
            /* Backspace */
            if (pos > 0) {
                pos--;
                putchar('\b');
                putchar(' ');
                putchar('\b');
            }
        } else if (c >= 32 && c <= 126) {
            /* Printable character - don't echo */
            buffer[pos++] = c;
            putchar('*');  /* Echo asterisk instead */
        }
        /* Ignore other characters */
    }
    
    buffer[pos] = '\0';
    putchar('\n');
    
    return (int)pos;
}

/**
 * Read an integer from keyboard
 */
int keyboard_read_integer(const char* prompt, int* value) {
    if (!value) {
        return -1;
    }
    
    char buffer[16];
    int result = keyboard_read_string(prompt, buffer, sizeof(buffer));
    
    if (result > 0) {
        /* Simple integer parsing */
        int num = 0;
        int sign = 1;
        int i = 0;
        
        if (buffer[0] == '-') {
            sign = -1;
            i = 1;
        } else if (buffer[0] == '+') {
            i = 1;
        }
        
        while (i < result && buffer[i] >= '0' && buffer[i] <= '9') {
            num = num * 10 + (buffer[i] - '0');
            i++;
        }
        
        if (i == result || (i == result && buffer[i] == '\0')) {
            *value = num * sign;
            return 0;
        }
    }
    
    return -1;  /* Invalid input */
}

/**
 * Present a menu and get user selection
 */
int keyboard_menu_select(const char* title, const char** options, int count) {
    if (!options || count <= 0) {
        return -1;
    }
    
    while (1) {
        /* Display menu */
        if (title) {
            printf("\n%s\n", title);
            for (int i = 0; i < (int)strlen(title); i++) {
                putchar('=');
            }
            putchar('\n');
        }
        
        for (int i = 0; i < count; i++) {
            printf("%d. %s\n", i + 1, options[i]);
        }
        printf("\nSelect option (1-%d): ", count);
        
        /* Get selection */
        int selection;
        if (keyboard_read_integer("", &selection) == 0) {
            if (selection >= 1 && selection <= count) {
                return selection - 1;  /* Return 0-based index */
            }
        }
        
        printf("Invalid selection. Please try again.\n");
    }
}

/**
 * Wait for user confirmation (Y/N)
 */
int keyboard_confirm(const char* prompt) {
    if (prompt) {
        printf("%s (y/n): ", prompt);
    } else {
        printf("Confirm (y/n): ");
    }
    
    while (1) {
        char c = keyboard_get_char();
        
        if (c == 'y' || c == 'Y') {
            putchar('y');
            putchar('\n');
            return 1;
        } else if (c == 'n' || c == 'N') {
            putchar('n');
            putchar('\n');
            return 0;
        }
        /* Ignore other characters */
    }
}

/* ================================
 * Key Combination Handling
 * ================================ */

/**
 * Check for specific key combination
 */
bool keyboard_check_combination(uint8_t keycode, uint8_t modifiers) {
    key_event_t event;
    
    if (keyboard_get_next_event_nonblock(&event) == 0) {
        if (event.type == KEY_EVENT_PRESS && 
            event.keycode == keycode && 
            (event.modifiers & modifiers) == modifiers) {
            return true;
        }
    }
    
    return false;
}

/**
 * Wait for specific key combination
 */
int keyboard_wait_combination(uint8_t keycode, uint8_t modifiers) {
    key_event_t event;
    
    while (keyboard_get_next_event(&event) == 0) {
        if (event.type == KEY_EVENT_PRESS && 
            event.keycode == keycode && 
            (event.modifiers & modifiers) == modifiers) {
            return 0;
        }
    }
    
    return -1;
}

/**
 * Register hotkey callback (placeholder - would need system support)
 */
int keyboard_register_hotkey(uint8_t keycode, uint8_t modifiers, void (*callback)(void)) {
    (void)keycode; (void)modifiers; (void)callback;
    /* This would require kernel support for hotkey registration */
    return -1;  /* Not implemented */
}

/**
 * Unregister hotkey callback
 */
int keyboard_unregister_hotkey(int hotkey_id) {
    (void)hotkey_id;
    /* This would require kernel support for hotkey registration */
    return -1;  /* Not implemented */
}

/* ================================
 * Utility Functions
 * ================================ */

/**
 * Convert key event to string representation
 */
int keyboard_event_to_string(const key_event_t* event, char* buffer, size_t size) {
    if (!event || !buffer || size == 0) {
        return -1;
    }
    
    int written = 0;
    
    /* Add modifiers */
    if (event->modifiers & MOD_CTRL) {
        written += simple_sprintf(buffer + written, "Ctrl+");
    }
    if (event->modifiers & MOD_ALT) {
        written += simple_sprintf(buffer + written, "Alt+");
    }
    if (event->modifiers & MOD_SHIFT) {
        written += simple_sprintf(buffer + written, "Shift+");
    }
    
    /* Add key name */
    const char* key_name = keyboard_get_key_name(event->keycode);
    if (key_name) {
        written += simple_sprintf(buffer + written, "%s", key_name);
    } else {
        written += simple_sprintf(buffer + written, "Key%d", (int)event->keycode);
    }
    
    /* Add event type */
    if (event->type == KEY_EVENT_RELEASE) {
        written += simple_sprintf(buffer + written, " (released)");
    }
    
    return written;
}

/**
 * Get key name string
 */
const char* keyboard_get_key_name(uint8_t keycode) {
    if (keycode < sizeof(key_names) / sizeof(key_names[0])) {
        return key_names[keycode];
    }
    return NULL;
}

/**
 * Get modifier string
 */
int keyboard_get_modifier_string(uint8_t modifiers, char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return -1;
    }
    
    int written = 0;
    
    if (modifiers & MOD_CTRL) {
        written += simple_sprintf(buffer + written, "Ctrl ");
    }
    if (modifiers & MOD_ALT) {
        written += simple_sprintf(buffer + written, "Alt ");
    }
    if (modifiers & MOD_SHIFT) {
        written += simple_sprintf(buffer + written, "Shift ");
    }
    if (modifiers & MOD_CAPS) {
        written += simple_sprintf(buffer + written, "Caps ");
    }
    
    /* Remove trailing space */
    if (written > 0 && buffer[written - 1] == ' ') {
        buffer[written - 1] = '\0';
        written--;
    }
    
    if (written == 0) {
        simple_sprintf(buffer, "None");
        written = 4;
    }
    
    return written;
}

/**
 * Parse key combination string
 */
int keyboard_parse_combination(const char* str, uint8_t* keycode, uint8_t* modifiers) {
    if (!str || !keycode || !modifiers) {
        return -1;
    }
    
    *modifiers = 0;
    *keycode = 0;
    
    /* Simple parser for combinations like "Ctrl+Alt+A" */
    const char* pos = str;
    
    while (*pos) {
        if (strncmp(pos, "Ctrl+", 5) == 0) {
            *modifiers |= MOD_CTRL;
            pos += 5;
        } else if (strncmp(pos, "Alt+", 4) == 0) {
            *modifiers |= MOD_ALT;
            pos += 4;
        } else if (strncmp(pos, "Shift+", 6) == 0) {
            *modifiers |= MOD_SHIFT;
            pos += 6;
        } else {
            /* Try to match key name */
            for (uint8_t i = 0; i < sizeof(key_names) / sizeof(key_names[0]); i++) {
                if (key_names[i] && strcmp(pos, key_names[i]) == 0) {
                    *keycode = i;
                    return 0;
                }
            }
            
            /* Try single character */
            if (strlen(pos) == 1) {
                char c = *pos;
                if (c >= 'A' && c <= 'Z') {
                    *keycode = KEY_A + (c - 'A');
                    return 0;
                } else if (c >= 'a' && c <= 'z') {
                    *keycode = KEY_A + (c - 'a');
                    return 0;
                } else if (c >= '0' && c <= '9') {
                    *keycode = KEY_0 + (c - '0');
                    return 0;
                }
            }
            
            return -1;  /* Unknown key */
        }
    }
    
    return -1;  /* No key found */
}

/* ================================
 * Error Handling
 * ================================ */

/**
 * Get error string for keyboard error code
 */
const char* keyboard_get_error_string(int error_code) {
    int index = -error_code;
    
    if (error_code == 0) {
        return error_messages[0];
    }
    
    if (index > 0 && index < (int)(sizeof(error_messages) / sizeof(error_messages[0]))) {
        return error_messages[index];
    }
    
    return "Unknown error";
}

/**
 * Print keyboard error message
 */
void keyboard_print_error(int error_code, const char* prefix) {
    const char* error_msg = keyboard_get_error_string(error_code);
    
    if (prefix) {
        printf("%s: %s (code %d)\n", prefix, error_msg, error_code);
    } else {
        printf("Keyboard error: %s (code %d)\n", error_msg, error_code);
    }
}
