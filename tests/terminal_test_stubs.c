/* Terminal Emulator Test Stubs
 * Issue #34 - VT100/ANSI Terminal Emulator Implementation
 * 
 * Provides stub implementations for kernel functions needed by the terminal
 * emulator when running in user-space test mode.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "../include/terminal.h"

/* ========================== Memory Management Stubs ========================== */

// Use standard library malloc/free directly
// void* malloc(size_t size) - already provided by stdlib
// void free(void* ptr) - already provided by stdlib

/* ========================== String Function Stubs ========================== */

char* strdup(const char* s) {
    if (!s) return NULL;
    
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

// Simple strchr implementation
char* strchr(const char* s, int c) {
    if (!s) return NULL;
    
    while (*s) {
        if (*s == c) return (char*)s;
        s++;
    }
    
    return (c == '\0') ? (char*)s : NULL;
}

char* strtok(char* str, const char* delim) {
    static char* strtok_buffer = NULL;
    if (str != NULL) {
        strtok_buffer = str;
    }
    
    if (strtok_buffer == NULL || *strtok_buffer == '\0') {
        return NULL;
    }
    
    // Skip leading delimiters
    while (*strtok_buffer && strchr(delim, *strtok_buffer)) {
        strtok_buffer++;
    }
    
    if (*strtok_buffer == '\0') {
        return NULL;
    }
    
    char* token_start = strtok_buffer;
    
    // Find end of token
    while (*strtok_buffer && !strchr(delim, *strtok_buffer)) {
        strtok_buffer++;
    }
    
    if (*strtok_buffer) {
        *strtok_buffer = '\0';
        strtok_buffer++;
    }
    
    return token_start;
}

int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    
    if (str == NULL) return 0;
    
    // Skip whitespace
    while (*str == ' ' || *str == '\t') str++;
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Convert digits
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result * sign;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}

/* ========================== Debug and Utility Stubs ========================== */

void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

/* Missing functions that might be referenced */
void terminal_move_cursor_to_next_line(terminal_t* term) {
    // This is implemented as a static function in terminal.c
    // For testing, we'll provide a simple implementation
    if (!term) return;
    
    term->cursor.x = 0;
    term->cursor.y++;
    
    if (term->cursor.y >= term->active_buffer->height) {
        term->cursor.y = term->active_buffer->height - 1;
        // Would normally call scroll function here
    }
}

void terminal_init_tab_stops(terminal_t* term) {
    // This is implemented as a static function in terminal.c
    // For testing, we'll provide a simple implementation
    if (!term) return;
    
    // Clear all tab stops
    memset(term->tab_stops, false, sizeof(term->tab_stops));
    
    // Set default tab stops every 8 columns
    for (uint16_t i = 8; i < 132; i += 8) {
        term->tab_stops[i] = true;
    }
}

int terminal_put_char_at(terminal_t* term, uint16_t x, uint16_t y, char c) {
    // This is implemented as a static function in terminal.c
    // For testing, we'll provide a simple implementation
    if (!term || !term->active_buffer) return -1;
    
    terminal_buffer_t* buffer = term->active_buffer;
    
    if (x >= buffer->width || y >= buffer->height) {
        return -1;
    }
    
    terminal_cell_t* cell = &buffer->cells[y * buffer->width + x];
    cell->character = (uint16_t)c;
    cell->fg_color = term->current_fg_color;
    cell->bg_color = term->current_bg_color;
    cell->attributes = term->current_attributes;
    
    return 0;
}

/* Additional missing function declarations that are static in the original */
extern void terminal_handle_newline(terminal_t* term);
extern void terminal_handle_carriage_return(terminal_t* term);
extern void terminal_handle_backspace(terminal_t* term);
extern void terminal_handle_tab(terminal_t* term);

/* Provide implementations for the static functions needed by tests */
void terminal_handle_newline(terminal_t* term) {
    if (!term) return;
    terminal_move_cursor_to_next_line(term);
}

void terminal_handle_carriage_return(terminal_t* term) {
    if (!term) return;
    term->cursor.x = 0;
}

void terminal_handle_backspace(terminal_t* term) {
    if (!term) return;
    if (term->cursor.x > 0) {
        term->cursor.x--;
    }
}

void terminal_handle_tab(terminal_t* term) {
    if (!term) return;
    uint16_t next_tab = terminal_next_tab_stop(term, term->cursor.x);
    if (next_tab < term->active_buffer->width) {
        term->cursor.x = next_tab;
    } else {
        term->cursor.x = term->active_buffer->width - 1;
    }
}
