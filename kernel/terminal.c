/* IKOS Terminal Emulator - Core Implementation
 * Issue #34 - VT100/ANSI Terminal Emulator Implementation
 * 
 * Provides comprehensive terminal emulation with VT100/ANSI escape sequence support,
 * cursor management, text formatting, and screen buffer management.
 */

#include "terminal.h"
#include "memory.h"
#include "stdio.h"
#include "string.h"
#include <stddef.h>
#include <stdlib.h>

/* ========================== Static Functions ========================== */

static void terminal_init_default_config(terminal_config_t* config);
static int terminal_allocate_buffers(terminal_t* term);
static void terminal_free_buffers(terminal_t* term);
static void terminal_init_tab_stops(terminal_t* term);
static void terminal_clear_buffer(terminal_buffer_t* buffer);
static int terminal_scroll_buffer_up(terminal_t* term, uint16_t lines);
static int terminal_scroll_buffer_down(terminal_t* term, uint16_t lines);
static void terminal_move_cursor_to_next_line(terminal_t* term);
static void terminal_handle_newline(terminal_t* term);
static void terminal_handle_carriage_return(terminal_t* term);
static void terminal_handle_backspace(terminal_t* term);
static void terminal_handle_tab(terminal_t* term);
static int terminal_put_char_at(terminal_t* term, uint16_t x, uint16_t y, char c);
static void terminal_add_to_scrollback(terminal_t* term, terminal_cell_t* line);

/* External escape sequence processor function */
extern int terminal_process_escape_sequence(terminal_t* term, const char* sequence);

/* ========================== Core Terminal Functions ========================== */

int terminal_init(terminal_t* term, uint16_t width, uint16_t height) {
    if (!term || width == 0 || height == 0 || 
        width > TERMINAL_MAX_WIDTH || height > TERMINAL_MAX_HEIGHT) {
        return TERMINAL_ERROR_INVALID;
    }
    
    // Clear the terminal structure
    memset(term, 0, sizeof(terminal_t));
    
    // Initialize default configuration
    terminal_init_default_config(&term->config);
    term->config.size.width = width;
    term->config.size.height = height;
    
    // Allocate buffers
    if (terminal_allocate_buffers(term) != TERMINAL_SUCCESS) {
        return TERMINAL_ERROR_MEMORY;
    }
    
    // Initialize cursor position
    term->cursor.x = 0;
    term->cursor.y = 0;
    term->saved_cursor.x = 0;
    term->saved_cursor.y = 0;
    
    // Set default colors and attributes
    term->current_fg_color = term->config.default_fg_color;
    term->current_bg_color = term->config.default_bg_color;
    term->current_attributes = TERMINAL_ATTR_NORMAL;
    
    // Initialize parser state
    term->parser.state = TERMINAL_STATE_NORMAL;
    term->parser.buffer_pos = 0;
    term->parser.param_count = 0;
    
    // Set active buffer to main buffer
    term->active_buffer = &term->main_buffer;
    term->in_alt_screen = false;
    
    // Initialize tab stops
    terminal_init_tab_stops(term);
    
    // Clear buffers
    terminal_clear_buffer(&term->main_buffer);
    terminal_clear_buffer(&term->alt_buffer);
    
    // Initialize scrollback
    term->scrollback_head = 0;
    term->scrollback_count = 0;
    
    // Mark as initialized and dirty
    term->initialized = true;
    term->dirty = true;
    
    return TERMINAL_SUCCESS;
}

void terminal_destroy(terminal_t* term) {
    if (!term || !term->initialized) {
        return;
    }
    
    terminal_free_buffers(term);
    memset(term, 0, sizeof(terminal_t));
}

int terminal_resize(terminal_t* term, uint16_t width, uint16_t height) {
    if (!term || !term->initialized || width == 0 || height == 0 ||
        width > TERMINAL_MAX_WIDTH || height > TERMINAL_MAX_HEIGHT) {
        return TERMINAL_ERROR_INVALID;
    }
    
    // Store old dimensions
    uint16_t old_width = term->config.size.width;
    uint16_t old_height = term->config.size.height;
    
    // If dimensions are the same, nothing to do
    if (width == old_width && height == old_height) {
        return TERMINAL_SUCCESS;
    }
    
    // Free old buffers
    terminal_free_buffers(term);
    
    // Update dimensions
    term->config.size.width = width;
    term->config.size.height = height;
    
    // Allocate new buffers
    if (terminal_allocate_buffers(term) != TERMINAL_SUCCESS) {
        return TERMINAL_ERROR_MEMORY;
    }
    
    // Adjust cursor position if necessary
    if (term->cursor.x >= width) {
        term->cursor.x = width - 1;
    }
    if (term->cursor.y >= height) {
        term->cursor.y = height - 1;
    }
    
    // Clear new buffers
    terminal_clear_buffer(&term->main_buffer);
    terminal_clear_buffer(&term->alt_buffer);
    
    // Reinitialize tab stops
    terminal_init_tab_stops(term);
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

void terminal_reset(terminal_t* term) {
    if (!term || !term->initialized) {
        return;
    }
    
    // Reset cursor position
    term->cursor.x = 0;
    term->cursor.y = 0;
    term->saved_cursor.x = 0;
    term->saved_cursor.y = 0;
    
    // Reset colors and attributes
    term->current_fg_color = term->config.default_fg_color;
    term->current_bg_color = term->config.default_bg_color;
    term->current_attributes = TERMINAL_ATTR_NORMAL;
    
    // Reset parser state
    term->parser.state = TERMINAL_STATE_NORMAL;
    term->parser.buffer_pos = 0;
    term->parser.param_count = 0;
    
    // Switch to main screen if in alternate
    if (term->in_alt_screen) {
        terminal_switch_to_main_screen(term);
    }
    
    // Clear buffers
    terminal_clear_buffer(&term->main_buffer);
    terminal_clear_buffer(&term->alt_buffer);
    
    // Reset configuration to defaults
    terminal_init_default_config(&term->config);
    
    // Reinitialize tab stops
    terminal_init_tab_stops(term);
    
    // Clear scrollback
    term->scrollback_head = 0;
    term->scrollback_count = 0;
    
    term->dirty = true;
}

/* ========================== Character Processing ========================== */

int terminal_write_char(terminal_t* term, char c) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    term->stats.characters_processed++;
    
    // Handle escape sequences
    if (term->parser.state != TERMINAL_STATE_NORMAL) {
        return terminal_process_escape_sequence(term, &c);
    }
    
    // Handle special characters
    switch ((unsigned char)c) {
        case '\n':
            terminal_handle_newline(term);
            break;
            
        case '\r':
            terminal_handle_carriage_return(term);
            break;
            
        case '\b':
            terminal_handle_backspace(term);
            break;
            
        case '\t':
            terminal_handle_tab(term);
            break;
            
        case 27:  // ESC character
            term->parser.state = TERMINAL_STATE_ESCAPE;
            term->parser.buffer_pos = 0;
            term->parser.param_count = 0;
            break;
            
        case '\a':  // Bell character
            // TODO: Implement bell/beep functionality
            break;
            
        default:
            // Handle printable characters
            if (c >= 32 && c <= 126) {
                terminal_put_char_at(term, term->cursor.x, term->cursor.y, c);
                
                // Move cursor forward
                term->cursor.x++;
                if (term->cursor.x >= term->active_buffer->width) {
                    if (term->config.autowrap) {
                        terminal_move_cursor_to_next_line(term);
                    } else {
                        term->cursor.x = term->active_buffer->width - 1;
                    }
                }
            }
            break;
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

int terminal_write_string(terminal_t* term, const char* str) {
    if (!term || !term->initialized || !str) {
        return TERMINAL_ERROR_INVALID;
    }
    
    int result = TERMINAL_SUCCESS;
    while (*str && result == TERMINAL_SUCCESS) {
        result = terminal_write_char(term, *str++);
    }
    
    return result;
}

int terminal_write_buffer(terminal_t* term, const char* buffer, size_t length) {
    if (!term || !term->initialized || !buffer) {
        return TERMINAL_ERROR_INVALID;
    }
    
    int result = TERMINAL_SUCCESS;
    for (size_t i = 0; i < length && result == TERMINAL_SUCCESS; i++) {
        result = terminal_write_char(term, buffer[i]);
    }
    
    return result;
}

/* ========================== Cursor Management ========================== */

int terminal_set_cursor(terminal_t* term, uint16_t x, uint16_t y) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    if (x >= term->active_buffer->width || y >= term->active_buffer->height) {
        return TERMINAL_ERROR_OUT_OF_BOUNDS;
    }
    
    term->cursor.x = x;
    term->cursor.y = y;
    term->dirty = true;
    
    return TERMINAL_SUCCESS;
}

int terminal_get_cursor(terminal_t* term, uint16_t* x, uint16_t* y) {
    if (!term || !term->initialized || !x || !y) {
        return TERMINAL_ERROR_INVALID;
    }
    
    *x = term->cursor.x;
    *y = term->cursor.y;
    
    return TERMINAL_SUCCESS;
}

int terminal_move_cursor(terminal_t* term, int16_t dx, int16_t dy) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    int16_t new_x = (int16_t)term->cursor.x + dx;
    int16_t new_y = (int16_t)term->cursor.y + dy;
    
    // Clamp to valid range
    if (new_x < 0) new_x = 0;
    if (new_x >= (int16_t)term->active_buffer->width) {
        new_x = term->active_buffer->width - 1;
    }
    if (new_y < 0) new_y = 0;
    if (new_y >= (int16_t)term->active_buffer->height) {
        new_y = term->active_buffer->height - 1;
    }
    
    term->cursor.x = (uint16_t)new_x;
    term->cursor.y = (uint16_t)new_y;
    term->dirty = true;
    
    return TERMINAL_SUCCESS;
}

void terminal_save_cursor(terminal_t* term) {
    if (!term || !term->initialized) {
        return;
    }
    
    term->saved_cursor.x = term->cursor.x;
    term->saved_cursor.y = term->cursor.y;
}

void terminal_restore_cursor(terminal_t* term) {
    if (!term || !term->initialized) {
        return;
    }
    
    term->cursor.x = term->saved_cursor.x;
    term->cursor.y = term->saved_cursor.y;
    
    // Ensure cursor is within bounds
    if (term->cursor.x >= term->active_buffer->width) {
        term->cursor.x = term->active_buffer->width - 1;
    }
    if (term->cursor.y >= term->active_buffer->height) {
        term->cursor.y = term->active_buffer->height - 1;
    }
    
    term->dirty = true;
}

/* ========================== Screen Manipulation ========================== */

int terminal_clear_screen(terminal_t* term) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    terminal_clear_buffer(term->active_buffer);
    term->cursor.x = 0;
    term->cursor.y = 0;
    term->dirty = true;
    
    return TERMINAL_SUCCESS;
}

int terminal_clear_line(terminal_t* term) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    terminal_buffer_t* buffer = term->active_buffer;
    uint16_t y = term->cursor.y;
    
    if (y >= buffer->height) {
        return TERMINAL_ERROR_OUT_OF_BOUNDS;
    }
    
    terminal_cell_t* line = &buffer->cells[y * buffer->width];
    for (uint16_t x = 0; x < buffer->width; x++) {
        line[x].character = ' ';
        line[x].fg_color = term->current_fg_color;
        line[x].bg_color = term->current_bg_color;
        line[x].attributes = TERMINAL_ATTR_NORMAL;
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

int terminal_scroll_up(terminal_t* term, uint16_t lines) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    return terminal_scroll_buffer_up(term, lines);
}

int terminal_scroll_down(terminal_t* term, uint16_t lines) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    return terminal_scroll_buffer_down(term, lines);
}

/* ========================== Static Helper Functions ========================== */

static void terminal_init_default_config(terminal_config_t* config) {
    config->size.width = TERMINAL_DEFAULT_WIDTH;
    config->size.height = TERMINAL_DEFAULT_HEIGHT;
    config->default_fg_color = TERMINAL_COLOR_WHITE;
    config->default_bg_color = TERMINAL_COLOR_BLACK;
    config->cursor_visible = true;
    config->cursor_blink = true;
    config->autowrap = true;
    config->insert_mode = false;
    config->origin_mode = false;
    config->application_cursor = false;
    config->application_keypad = false;
    config->tab_width = 8;
}

static int terminal_allocate_buffers(terminal_t* term) {
    uint16_t width = term->config.size.width;
    uint16_t height = term->config.size.height;
    size_t buffer_size = width * height * sizeof(terminal_cell_t);
    size_t scrollback_size = TERMINAL_SCROLLBACK_LINES * width * sizeof(terminal_cell_t);
    
    // Allocate main buffer
    term->main_buffer.cells = (terminal_cell_t*)malloc(buffer_size);
    if (!term->main_buffer.cells) {
        return TERMINAL_ERROR_MEMORY;
    }
    term->main_buffer.width = width;
    term->main_buffer.height = height;
    term->main_buffer.scroll_top = 0;
    term->main_buffer.scroll_bottom = height - 1;
    
    // Allocate alternate buffer
    term->alt_buffer.cells = (terminal_cell_t*)malloc(buffer_size);
    if (!term->alt_buffer.cells) {
        free(term->main_buffer.cells);
        return TERMINAL_ERROR_MEMORY;
    }
    term->alt_buffer.width = width;
    term->alt_buffer.height = height;
    term->alt_buffer.scroll_top = 0;
    term->alt_buffer.scroll_bottom = height - 1;
    
    // Allocate scrollback buffer
    term->scrollback_buffer = (terminal_cell_t*)malloc(scrollback_size);
    if (!term->scrollback_buffer) {
        free(term->main_buffer.cells);
        free(term->alt_buffer.cells);
        return TERMINAL_ERROR_MEMORY;
    }
    term->scrollback_size = TERMINAL_SCROLLBACK_LINES;
    
    // Update memory usage statistics
    term->stats.memory_usage = buffer_size * 2 + scrollback_size;
    
    return TERMINAL_SUCCESS;
}

static void terminal_free_buffers(terminal_t* term) {
    if (term->main_buffer.cells) {
        free(term->main_buffer.cells);
        term->main_buffer.cells = NULL;
    }
    
    if (term->alt_buffer.cells) {
        free(term->alt_buffer.cells);
        term->alt_buffer.cells = NULL;
    }
    
    if (term->scrollback_buffer) {
        free(term->scrollback_buffer);
        term->scrollback_buffer = NULL;
    }
    
    term->stats.memory_usage = 0;
}

static void terminal_init_tab_stops(terminal_t* term) {
    // Clear all tab stops
    memset(term->tab_stops, false, sizeof(term->tab_stops));
    
    // Set default tab stops every 8 columns
    for (uint16_t i = term->config.tab_width; i < TERMINAL_MAX_WIDTH; i += term->config.tab_width) {
        term->tab_stops[i] = true;
    }
}

static void terminal_clear_buffer(terminal_buffer_t* buffer) {
    if (!buffer || !buffer->cells) {
        return;
    }
    
    for (uint16_t y = 0; y < buffer->height; y++) {
        for (uint16_t x = 0; x < buffer->width; x++) {
            terminal_cell_t* cell = &buffer->cells[y * buffer->width + x];
            cell->character = ' ';
            cell->fg_color = TERMINAL_COLOR_WHITE;
            cell->bg_color = TERMINAL_COLOR_BLACK;
            cell->attributes = TERMINAL_ATTR_NORMAL;
        }
    }
}

static int terminal_scroll_buffer_up(terminal_t* term, uint16_t lines) {
    terminal_buffer_t* buffer = term->active_buffer;
    
    if (lines == 0 || lines >= buffer->height) {
        terminal_clear_buffer(buffer);
        term->stats.scroll_operations++;
        term->dirty = true;
        return TERMINAL_SUCCESS;
    }
    
    // Save top lines to scrollback if using main buffer
    if (buffer == &term->main_buffer) {
        for (uint16_t i = 0; i < lines; i++) {
            terminal_cell_t* line = &buffer->cells[i * buffer->width];
            terminal_add_to_scrollback(term, line);
        }
    }
    
    // Move lines up
    size_t move_size = (buffer->height - lines) * buffer->width * sizeof(terminal_cell_t);
    memmove(buffer->cells, &buffer->cells[lines * buffer->width], move_size);
    
    // Clear bottom lines
    for (uint16_t y = buffer->height - lines; y < buffer->height; y++) {
        for (uint16_t x = 0; x < buffer->width; x++) {
            terminal_cell_t* cell = &buffer->cells[y * buffer->width + x];
            cell->character = ' ';
            cell->fg_color = term->current_fg_color;
            cell->bg_color = term->current_bg_color;
            cell->attributes = TERMINAL_ATTR_NORMAL;
        }
    }
    
    term->stats.scroll_operations++;
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

static int terminal_scroll_buffer_down(terminal_t* term, uint16_t lines) {
    terminal_buffer_t* buffer = term->active_buffer;
    
    if (lines == 0 || lines >= buffer->height) {
        terminal_clear_buffer(buffer);
        term->stats.scroll_operations++;
        term->dirty = true;
        return TERMINAL_SUCCESS;
    }
    
    // Move lines down
    size_t move_size = (buffer->height - lines) * buffer->width * sizeof(terminal_cell_t);
    memmove(&buffer->cells[lines * buffer->width], buffer->cells, move_size);
    
    // Clear top lines
    for (uint16_t y = 0; y < lines; y++) {
        for (uint16_t x = 0; x < buffer->width; x++) {
            terminal_cell_t* cell = &buffer->cells[y * buffer->width + x];
            cell->character = ' ';
            cell->fg_color = term->current_fg_color;
            cell->bg_color = term->current_bg_color;
            cell->attributes = TERMINAL_ATTR_NORMAL;
        }
    }
    
    term->stats.scroll_operations++;
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

static void terminal_move_cursor_to_next_line(terminal_t* term) {
    term->cursor.x = 0;
    term->cursor.y++;
    
    if (term->cursor.y >= term->active_buffer->height) {
        term->cursor.y = term->active_buffer->height - 1;
        terminal_scroll_buffer_up(term, 1);
    }
}

static void terminal_handle_newline(terminal_t* term) {
    terminal_move_cursor_to_next_line(term);
}

static void terminal_handle_carriage_return(terminal_t* term) {
    term->cursor.x = 0;
}

static void terminal_handle_backspace(terminal_t* term) {
    if (term->cursor.x > 0) {
        term->cursor.x--;
    }
}

static void terminal_handle_tab(terminal_t* term) {
    uint16_t next_tab = terminal_next_tab_stop(term, term->cursor.x);
    if (next_tab < term->active_buffer->width) {
        term->cursor.x = next_tab;
    } else {
        term->cursor.x = term->active_buffer->width - 1;
    }
}

static int terminal_put_char_at(terminal_t* term, uint16_t x, uint16_t y, char c) {
    terminal_buffer_t* buffer = term->active_buffer;
    
    if (x >= buffer->width || y >= buffer->height) {
        return TERMINAL_ERROR_OUT_OF_BOUNDS;
    }
    
    terminal_cell_t* cell = &buffer->cells[y * buffer->width + x];
    cell->character = (uint16_t)c;
    cell->fg_color = term->current_fg_color;
    cell->bg_color = term->current_bg_color;
    cell->attributes = term->current_attributes;
    
    return TERMINAL_SUCCESS;
}

static void terminal_add_to_scrollback(terminal_t* term, terminal_cell_t* line) {
    if (!term->scrollback_buffer || !line) {
        return;
    }
    
    uint16_t width = term->config.size.width;
    terminal_cell_t* dest = &term->scrollback_buffer[term->scrollback_head * width];
    
    memcpy(dest, line, width * sizeof(terminal_cell_t));
    
    term->scrollback_head = (term->scrollback_head + 1) % term->scrollback_size;
    if (term->scrollback_count < term->scrollback_size) {
        term->scrollback_count++;
    }
}

/* ========================== Tab Stop Management ========================== */

void terminal_set_tab_stop(terminal_t* term, uint16_t column) {
    if (term && term->initialized && column < TERMINAL_MAX_WIDTH) {
        term->tab_stops[column] = true;
    }
}

void terminal_clear_tab_stop(terminal_t* term, uint16_t column) {
    if (term && term->initialized && column < TERMINAL_MAX_WIDTH) {
        term->tab_stops[column] = false;
    }
}

void terminal_clear_all_tab_stops(terminal_t* term) {
    if (term && term->initialized) {
        memset(term->tab_stops, false, sizeof(term->tab_stops));
    }
}

uint16_t terminal_next_tab_stop(terminal_t* term, uint16_t column) {
    if (!term || !term->initialized) {
        return column;
    }
    
    for (uint16_t i = column + 1; i < TERMINAL_MAX_WIDTH; i++) {
        if (term->tab_stops[i]) {
            return i;
        }
    }
    
    return TERMINAL_MAX_WIDTH;
}

/* ========================== Utility Functions ========================== */

const char* terminal_get_version(void) {
    return "IKOS Terminal Emulator v1.0.0 - VT100/ANSI Compatible";
}

bool terminal_is_dirty(terminal_t* term) {
    return term && term->initialized && term->dirty;
}

void terminal_mark_dirty(terminal_t* term) {
    if (term && term->initialized) {
        term->dirty = true;
    }
}

void terminal_mark_clean(terminal_t* term) {
    if (term && term->initialized) {
        term->dirty = false;
    }
}
