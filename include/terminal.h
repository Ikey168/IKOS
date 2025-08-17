/* IKOS Terminal Emulator - Header Definitions
 * Issue #34 - VT100/ANSI Terminal Emulator Implementation
 * 
 * Provides comprehensive terminal emulation with VT100/ANSI escape sequence support,
 * cursor management, text formatting, and screen buffer management.
 */

#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stdbool.h>

/* ========================== Terminal Constants ========================== */

/* Screen dimensions */
#define TERMINAL_MAX_WIDTH          132     /* Maximum terminal width */
#define TERMINAL_MAX_HEIGHT         50      /* Maximum terminal height */
#define TERMINAL_DEFAULT_WIDTH      80      /* Default terminal width */
#define TERMINAL_DEFAULT_HEIGHT     25      /* Default terminal height */

/* Buffer sizes */
#define TERMINAL_INPUT_BUFFER_SIZE  1024    /* Input buffer size */
#define TERMINAL_ESCAPE_BUFFER_SIZE 64      /* Escape sequence buffer size */
#define TERMINAL_SCROLLBACK_LINES   1000    /* Scrollback buffer lines */

/* Color definitions */
#define TERMINAL_COLOR_BLACK        0
#define TERMINAL_COLOR_RED          1
#define TERMINAL_COLOR_GREEN        2
#define TERMINAL_COLOR_YELLOW       3
#define TERMINAL_COLOR_BLUE         4
#define TERMINAL_COLOR_MAGENTA      5
#define TERMINAL_COLOR_CYAN         6
#define TERMINAL_COLOR_WHITE        7
#define TERMINAL_COLOR_BRIGHT_BLACK 8
#define TERMINAL_COLOR_BRIGHT_RED   9
#define TERMINAL_COLOR_BRIGHT_GREEN 10
#define TERMINAL_COLOR_BRIGHT_YELLOW 11
#define TERMINAL_COLOR_BRIGHT_BLUE  12
#define TERMINAL_COLOR_BRIGHT_MAGENTA 13
#define TERMINAL_COLOR_BRIGHT_CYAN  14
#define TERMINAL_COLOR_BRIGHT_WHITE 15

/* Character attributes */
#define TERMINAL_ATTR_NORMAL        0x00
#define TERMINAL_ATTR_BOLD          0x01
#define TERMINAL_ATTR_DIM           0x02
#define TERMINAL_ATTR_ITALIC        0x04
#define TERMINAL_ATTR_UNDERLINE     0x08
#define TERMINAL_ATTR_BLINK         0x10
#define TERMINAL_ATTR_REVERSE       0x20
#define TERMINAL_ATTR_STRIKETHROUGH 0x40
#define TERMINAL_ATTR_HIDDEN        0x80

/* Special keys */
#define TERMINAL_KEY_BACKSPACE      0x08
#define TERMINAL_KEY_TAB            0x09
#define TERMINAL_KEY_ENTER          0x0A
#define TERMINAL_KEY_ESCAPE         0x1B
#define TERMINAL_KEY_DELETE         0x7F
#define TERMINAL_KEY_UP             0x100
#define TERMINAL_KEY_DOWN           0x101
#define TERMINAL_KEY_LEFT           0x102
#define TERMINAL_KEY_RIGHT          0x103
#define TERMINAL_KEY_HOME           0x104
#define TERMINAL_KEY_END            0x105
#define TERMINAL_KEY_PAGE_UP        0x106
#define TERMINAL_KEY_PAGE_DOWN      0x107
#define TERMINAL_KEY_INSERT         0x108
#define TERMINAL_KEY_F1             0x110
#define TERMINAL_KEY_F2             0x111
#define TERMINAL_KEY_F3             0x112
#define TERMINAL_KEY_F4             0x113
#define TERMINAL_KEY_F5             0x114
#define TERMINAL_KEY_F6             0x115
#define TERMINAL_KEY_F7             0x116
#define TERMINAL_KEY_F8             0x117
#define TERMINAL_KEY_F9             0x118
#define TERMINAL_KEY_F10            0x119
#define TERMINAL_KEY_F11            0x11A
#define TERMINAL_KEY_F12            0x11B

/* Error codes */
#define TERMINAL_SUCCESS            0
#define TERMINAL_ERROR_INVALID      -1
#define TERMINAL_ERROR_MEMORY       -2
#define TERMINAL_ERROR_NOT_INIT     -3
#define TERMINAL_ERROR_BUFFER_FULL  -4
#define TERMINAL_ERROR_OUT_OF_BOUNDS -5

/* ========================== Data Structures ========================== */

/* Character cell with attributes */
typedef struct {
    uint16_t character;         /* Unicode character (or ASCII) */
    uint8_t fg_color;          /* Foreground color */
    uint8_t bg_color;          /* Background color */
    uint8_t attributes;        /* Text attributes (bold, underline, etc.) */
} terminal_cell_t;

/* Cursor position */
typedef struct {
    uint16_t x;                /* Column position */
    uint16_t y;                /* Row position */
} terminal_cursor_t;

/* Terminal dimensions */
typedef struct {
    uint16_t width;            /* Terminal width in columns */
    uint16_t height;           /* Terminal height in rows */
} terminal_size_t;

/* Terminal state */
typedef enum {
    TERMINAL_STATE_NORMAL,     /* Normal character processing */
    TERMINAL_STATE_ESCAPE,     /* Processing escape sequence */
    TERMINAL_STATE_CSI,        /* Processing Control Sequence Introducer */
    TERMINAL_STATE_OSC,        /* Processing Operating System Command */
    TERMINAL_STATE_DCS,        /* Processing Device Control String */
    TERMINAL_STATE_CHARSET     /* Processing character set selection */
} terminal_state_t;

/* Escape sequence parser state */
typedef struct {
    terminal_state_t state;    /* Current parser state */
    char buffer[TERMINAL_ESCAPE_BUFFER_SIZE]; /* Escape sequence buffer */
    uint16_t buffer_pos;       /* Buffer position */
    int32_t params[16];        /* Parsed parameters */
    uint8_t param_count;       /* Number of parameters */
    char intermediate;         /* Intermediate character */
    char final;                /* Final character */
} terminal_parser_t;

/* Screen buffer */
typedef struct {
    terminal_cell_t* cells;    /* Character cells */
    uint16_t width;            /* Buffer width */
    uint16_t height;           /* Buffer height */
    uint16_t scroll_top;       /* Scroll region top */
    uint16_t scroll_bottom;    /* Scroll region bottom */
} terminal_buffer_t;

/* Terminal configuration */
typedef struct {
    terminal_size_t size;      /* Terminal dimensions */
    uint8_t default_fg_color;  /* Default foreground color */
    uint8_t default_bg_color;  /* Default background color */
    bool cursor_visible;       /* Cursor visibility */
    bool cursor_blink;         /* Cursor blinking */
    bool autowrap;             /* Automatic line wrapping */
    bool insert_mode;          /* Insert mode enabled */
    bool origin_mode;          /* Origin mode (relative to scroll region) */
    bool application_cursor;   /* Application cursor key mode */
    bool application_keypad;   /* Application keypad mode */
    uint16_t tab_width;        /* Tab stop width */
} terminal_config_t;

/* Terminal statistics */
typedef struct {
    uint64_t characters_processed; /* Total characters processed */
    uint64_t escape_sequences;     /* Escape sequences processed */
    uint64_t screen_updates;       /* Screen updates performed */
    uint64_t scroll_operations;    /* Scroll operations */
    uint64_t input_characters;     /* Input characters received */
    uint32_t memory_usage;         /* Memory usage in bytes */
} terminal_stats_t;

/* Main terminal structure */
typedef struct {
    /* Configuration */
    terminal_config_t config;  /* Terminal configuration */
    
    /* Display state */
    terminal_buffer_t main_buffer;     /* Main screen buffer */
    terminal_buffer_t alt_buffer;      /* Alternate screen buffer */
    terminal_buffer_t* active_buffer;  /* Currently active buffer */
    terminal_cursor_t cursor;          /* Cursor position */
    terminal_cursor_t saved_cursor;    /* Saved cursor position */
    
    /* Character attributes */
    uint8_t current_fg_color;  /* Current foreground color */
    uint8_t current_bg_color;  /* Current background color */
    uint8_t current_attributes; /* Current text attributes */
    
    /* Parser state */
    terminal_parser_t parser;   /* Escape sequence parser */
    
    /* Input handling */
    char input_buffer[TERMINAL_INPUT_BUFFER_SIZE]; /* Input buffer */
    uint16_t input_head;        /* Input buffer head */
    uint16_t input_tail;        /* Input buffer tail */
    
    /* Scrollback buffer */
    terminal_cell_t* scrollback_buffer; /* Scrollback buffer */
    uint16_t scrollback_size;   /* Scrollback buffer size */
    uint16_t scrollback_head;   /* Scrollback buffer head */
    uint16_t scrollback_count;  /* Number of lines in scrollback */
    
    /* Tab stops */
    bool tab_stops[TERMINAL_MAX_WIDTH]; /* Tab stop positions */
    
    /* Statistics */
    terminal_stats_t stats;     /* Terminal statistics */
    
    /* State flags */
    bool initialized;           /* Terminal initialized */
    bool dirty;                 /* Screen needs redraw */
    bool in_alt_screen;         /* Using alternate screen */
} terminal_t;

/* ========================== Function Declarations ========================== */

/* Core terminal functions */
int terminal_init(terminal_t* term, uint16_t width, uint16_t height);
void terminal_destroy(terminal_t* term);
int terminal_resize(terminal_t* term, uint16_t width, uint16_t height);
void terminal_reset(terminal_t* term);

/* Character processing */
int terminal_write_char(terminal_t* term, char c);
int terminal_write_string(terminal_t* term, const char* str);
int terminal_write_buffer(terminal_t* term, const char* buffer, size_t length);

/* Cursor management */
int terminal_set_cursor(terminal_t* term, uint16_t x, uint16_t y);
int terminal_get_cursor(terminal_t* term, uint16_t* x, uint16_t* y);
int terminal_move_cursor(terminal_t* term, int16_t dx, int16_t dy);
void terminal_save_cursor(terminal_t* term);
void terminal_restore_cursor(terminal_t* term);

/* Screen manipulation */
int terminal_clear_screen(terminal_t* term);
int terminal_clear_line(terminal_t* term);
int terminal_scroll_up(terminal_t* term, uint16_t lines);
int terminal_scroll_down(terminal_t* term, uint16_t lines);
int terminal_insert_lines(terminal_t* term, uint16_t count);
int terminal_delete_lines(terminal_t* term, uint16_t count);

/* Character and line operations */
int terminal_insert_chars(terminal_t* term, uint16_t count);
int terminal_delete_chars(terminal_t* term, uint16_t count);
int terminal_erase_chars(terminal_t* term, uint16_t count);

/* Attribute management */
int terminal_set_fg_color(terminal_t* term, uint8_t color);
int terminal_set_bg_color(terminal_t* term, uint8_t color);
int terminal_set_attributes(terminal_t* term, uint8_t attributes);
int terminal_reset_attributes(terminal_t* term);

/* Input handling */
int terminal_read_char(terminal_t* term);
int terminal_read_line(terminal_t* term, char* buffer, size_t size);
int terminal_handle_key(terminal_t* term, uint16_t key);

/* Configuration */
int terminal_set_config(terminal_t* term, const terminal_config_t* config);
int terminal_get_config(terminal_t* term, terminal_config_t* config);
int terminal_set_size(terminal_t* term, uint16_t width, uint16_t height);
int terminal_get_size(terminal_t* term, uint16_t* width, uint16_t* height);

/* Scrollback buffer */
int terminal_get_scrollback_line(terminal_t* term, int16_t line_offset, 
                                terminal_cell_t* buffer, uint16_t buffer_size);
int terminal_clear_scrollback(terminal_t* term);

/* Tab stops */
void terminal_set_tab_stop(terminal_t* term, uint16_t column);
void terminal_clear_tab_stop(terminal_t* term, uint16_t column);
void terminal_clear_all_tab_stops(terminal_t* term);
uint16_t terminal_next_tab_stop(terminal_t* term, uint16_t column);

/* Screen buffer management */
int terminal_switch_to_alt_screen(terminal_t* term);
int terminal_switch_to_main_screen(terminal_t* term);

/* Escape sequence processing */
int terminal_process_escape_sequence(terminal_t* term, const char* sequence);

/* Rendering interface */
int terminal_render_screen(terminal_t* term);
int terminal_render_cursor(terminal_t* term);
int terminal_get_screen_buffer(terminal_t* term, terminal_cell_t** buffer, 
                              uint16_t* width, uint16_t* height);

/* Utility functions */
const char* terminal_get_version(void);
int terminal_get_stats(terminal_t* term, terminal_stats_t* stats);
bool terminal_is_dirty(terminal_t* term);
void terminal_mark_dirty(terminal_t* term);
void terminal_mark_clean(terminal_t* term);

/* Color conversion utilities */
uint8_t terminal_rgb_to_color(uint8_t r, uint8_t g, uint8_t b);
void terminal_color_to_rgb(uint8_t color, uint8_t* r, uint8_t* g, uint8_t* b);
uint16_t terminal_color_to_vga(uint8_t fg_color, uint8_t bg_color);

/* VT100/ANSI compatibility functions */
int terminal_device_attributes(terminal_t* term, char* response, size_t size);
int terminal_cursor_position_report(terminal_t* term, char* response, size_t size);
int terminal_set_scroll_region(terminal_t* term, uint16_t top, uint16_t bottom);

/* Debug and testing functions */
void terminal_dump_state(terminal_t* term);
void terminal_dump_buffer(terminal_t* term);
int terminal_self_test(void);

#endif /* TERMINAL_H */
