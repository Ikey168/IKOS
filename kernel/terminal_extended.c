/* IKOS Terminal Emulator - Extended Functions Implementation
 * Issue #34 - VT100/ANSI Terminal Emulator Implementation
 * 
 * Additional terminal functions for configuration, input handling,
 * scrollback management, and rendering interface.
 */

#include "terminal.h"
#include "memory.h"
#include "stdio.h"
#include "string.h"
#include <stdlib.h>

/* ========================== Configuration Functions ========================== */

int terminal_set_config(terminal_t* term, const terminal_config_t* config) {
    if (!term || !term->initialized || !config) {
        return TERMINAL_ERROR_INVALID;
    }
    
    // Validate configuration
    if (config->size.width == 0 || config->size.height == 0 ||
        config->size.width > TERMINAL_MAX_WIDTH || 
        config->size.height > TERMINAL_MAX_HEIGHT) {
        return TERMINAL_ERROR_INVALID;
    }
    
    // Check if resize is needed
    bool need_resize = (config->size.width != term->config.size.width ||
                       config->size.height != term->config.size.height);
    
    // Copy configuration
    term->config = *config;
    
    // Resize if needed
    if (need_resize) {
        int result = terminal_resize(term, config->size.width, config->size.height);
        if (result != TERMINAL_SUCCESS) {
            return result;
        }
    }
    
    // Update tab stops if tab width changed
    if (config->tab_width != term->config.tab_width) {
        terminal_init_tab_stops(term);
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

int terminal_get_config(terminal_t* term, terminal_config_t* config) {
    if (!term || !term->initialized || !config) {
        return TERMINAL_ERROR_INVALID;
    }
    
    *config = term->config;
    return TERMINAL_SUCCESS;
}

int terminal_set_size(terminal_t* term, uint16_t width, uint16_t height) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    return terminal_resize(term, width, height);
}

int terminal_get_size(terminal_t* term, uint16_t* width, uint16_t* height) {
    if (!term || !term->initialized || !width || !height) {
        return TERMINAL_ERROR_INVALID;
    }
    
    *width = term->config.size.width;
    *height = term->config.size.height;
    return TERMINAL_SUCCESS;
}

/* ========================== Input Handling ========================== */

int terminal_read_char(terminal_t* term) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    if (term->input_head == term->input_tail) {
        return -1;  // No input available
    }
    
    char c = term->input_buffer[term->input_tail];
    term->input_tail = (term->input_tail + 1) % TERMINAL_INPUT_BUFFER_SIZE;
    term->stats.input_characters++;
    
    return (unsigned char)c;
}

int terminal_read_line(terminal_t* term, char* buffer, size_t size) {
    if (!term || !term->initialized || !buffer || size == 0) {
        return TERMINAL_ERROR_INVALID;
    }
    
    size_t pos = 0;
    int c;
    
    while (pos < size - 1) {
        c = terminal_read_char(term);
        if (c == -1) {
            break;  // No more input
        }
        
        if (c == '\n' || c == '\r') {
            break;  // End of line
        }
        
        buffer[pos++] = (char)c;
    }
    
    buffer[pos] = '\0';
    return pos;
}

int terminal_handle_key(terminal_t* term, uint16_t key) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    char sequence[8];
    int seq_len = 0;
    
    switch (key) {
        case TERMINAL_KEY_UP:
            if (term->config.application_cursor) {
                strcpy(sequence, "\eOA");
                seq_len = 3;
            } else {
                strcpy(sequence, "\e[A");
                seq_len = 3;
            }
            break;
            
        case TERMINAL_KEY_DOWN:
            if (term->config.application_cursor) {
                strcpy(sequence, "\eOB");
                seq_len = 3;
            } else {
                strcpy(sequence, "\e[B");
                seq_len = 3;
            }
            break;
            
        case TERMINAL_KEY_RIGHT:
            if (term->config.application_cursor) {
                strcpy(sequence, "\eOC");
                seq_len = 3;
            } else {
                strcpy(sequence, "\e[C");
                seq_len = 3;
            }
            break;
            
        case TERMINAL_KEY_LEFT:
            if (term->config.application_cursor) {
                strcpy(sequence, "\eOD");
                seq_len = 3;
            } else {
                strcpy(sequence, "\e[D");
                seq_len = 3;
            }
            break;
            
        case TERMINAL_KEY_HOME:
            strcpy(sequence, "\e[H");
            seq_len = 3;
            break;
            
        case TERMINAL_KEY_END:
            strcpy(sequence, "\e[F");
            seq_len = 3;
            break;
            
        case TERMINAL_KEY_PAGE_UP:
            strcpy(sequence, "\e[5~");
            seq_len = 4;
            break;
            
        case TERMINAL_KEY_PAGE_DOWN:
            strcpy(sequence, "\e[6~");
            seq_len = 4;
            break;
            
        case TERMINAL_KEY_INSERT:
            strcpy(sequence, "\e[2~");
            seq_len = 4;
            break;
            
        case TERMINAL_KEY_DELETE:
            strcpy(sequence, "\e[3~");
            seq_len = 4;
            break;
            
        case TERMINAL_KEY_F1:
            strcpy(sequence, "\eOP");
            seq_len = 3;
            break;
            
        case TERMINAL_KEY_F2:
            strcpy(sequence, "\eOQ");
            seq_len = 3;
            break;
            
        case TERMINAL_KEY_F3:
            strcpy(sequence, "\eOR");
            seq_len = 3;
            break;
            
        case TERMINAL_KEY_F4:
            strcpy(sequence, "\eOS");
            seq_len = 3;
            break;
            
        case TERMINAL_KEY_F5:
            strcpy(sequence, "\e[15~");
            seq_len = 5;
            break;
            
        case TERMINAL_KEY_F6:
            strcpy(sequence, "\e[17~");
            seq_len = 5;
            break;
            
        case TERMINAL_KEY_F7:
            strcpy(sequence, "\e[18~");
            seq_len = 5;
            break;
            
        case TERMINAL_KEY_F8:
            strcpy(sequence, "\e[19~");
            seq_len = 5;
            break;
            
        case TERMINAL_KEY_F9:
            strcpy(sequence, "\e[20~");
            seq_len = 5;
            break;
            
        case TERMINAL_KEY_F10:
            strcpy(sequence, "\e[21~");
            seq_len = 5;
            break;
            
        case TERMINAL_KEY_F11:
            strcpy(sequence, "\e[23~");
            seq_len = 5;
            break;
            
        case TERMINAL_KEY_F12:
            strcpy(sequence, "\e[24~");
            seq_len = 5;
            break;
            
        default:
            // Regular ASCII character
            if (key <= 255) {
                sequence[0] = (char)key;
                seq_len = 1;
            } else {
                return TERMINAL_SUCCESS;  // Unknown key, ignore
            }
            break;
    }
    
    // Add sequence to input buffer
    for (int i = 0; i < seq_len; i++) {
        uint16_t next_head = (term->input_head + 1) % TERMINAL_INPUT_BUFFER_SIZE;
        if (next_head != term->input_tail) {
            term->input_buffer[term->input_head] = sequence[i];
            term->input_head = next_head;
        } else {
            return TERMINAL_ERROR_BUFFER_FULL;
        }
    }
    
    return TERMINAL_SUCCESS;
}

/* ========================== Scrollback Buffer Management ========================== */

int terminal_get_scrollback_line(terminal_t* term, int16_t line_offset, 
                                terminal_cell_t* buffer, uint16_t buffer_size) {
    if (!term || !term->initialized || !buffer || buffer_size == 0) {
        return TERMINAL_ERROR_INVALID;
    }
    
    if (!term->scrollback_buffer || term->scrollback_count == 0) {
        return TERMINAL_ERROR_INVALID;
    }
    
    if (line_offset >= 0 || -line_offset > (int16_t)term->scrollback_count) {
        return TERMINAL_ERROR_OUT_OF_BOUNDS;
    }
    
    // Calculate the actual line position in the circular buffer
    uint16_t line_index = (term->scrollback_head + term->scrollback_count + line_offset) % term->scrollback_size;
    uint16_t width = term->config.size.width;
    
    // Copy the line to the output buffer
    uint16_t copy_size = (buffer_size < width) ? buffer_size : width;
    terminal_cell_t* src = &term->scrollback_buffer[line_index * width];
    memcpy(buffer, src, copy_size * sizeof(terminal_cell_t));
    
    return copy_size;
}

int terminal_clear_scrollback(terminal_t* term) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    term->scrollback_head = 0;
    term->scrollback_count = 0;
    
    return TERMINAL_SUCCESS;
}

/* ========================== Screen Buffer Management ========================== */

int terminal_switch_to_alt_screen(terminal_t* term) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    if (!term->in_alt_screen) {
        term->active_buffer = &term->alt_buffer;
        term->in_alt_screen = true;
        terminal_clear_buffer(&term->alt_buffer);
        term->cursor.x = 0;
        term->cursor.y = 0;
        term->dirty = true;
    }
    
    return TERMINAL_SUCCESS;
}

int terminal_switch_to_main_screen(terminal_t* term) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    if (term->in_alt_screen) {
        term->active_buffer = &term->main_buffer;
        term->in_alt_screen = false;
        term->dirty = true;
    }
    
    return TERMINAL_SUCCESS;
}

/* ========================== Character and Line Operations ========================== */

int terminal_insert_lines(terminal_t* term, uint16_t count) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    if (count == 0) {
        return TERMINAL_SUCCESS;
    }
    
    terminal_buffer_t* buffer = term->active_buffer;
    uint16_t y = term->cursor.y;
    
    if (y >= buffer->height) {
        return TERMINAL_ERROR_OUT_OF_BOUNDS;
    }
    
    // Limit count to available space
    if (count > buffer->height - y) {
        count = buffer->height - y;
    }
    
    // Move existing lines down
    if (y + count < buffer->height) {
        size_t move_size = (buffer->height - y - count) * buffer->width * sizeof(terminal_cell_t);
        memmove(&buffer->cells[(y + count) * buffer->width],
                &buffer->cells[y * buffer->width], move_size);
    }
    
    // Clear the inserted lines
    for (uint16_t line = y; line < y + count; line++) {
        for (uint16_t x = 0; x < buffer->width; x++) {
            terminal_cell_t* cell = &buffer->cells[line * buffer->width + x];
            cell->character = ' ';
            cell->fg_color = term->current_fg_color;
            cell->bg_color = term->current_bg_color;
            cell->attributes = TERMINAL_ATTR_NORMAL;
        }
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

int terminal_delete_lines(terminal_t* term, uint16_t count) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    if (count == 0) {
        return TERMINAL_SUCCESS;
    }
    
    terminal_buffer_t* buffer = term->active_buffer;
    uint16_t y = term->cursor.y;
    
    if (y >= buffer->height) {
        return TERMINAL_ERROR_OUT_OF_BOUNDS;
    }
    
    // Limit count to available lines
    if (count > buffer->height - y) {
        count = buffer->height - y;
    }
    
    // Move lines up
    if (y + count < buffer->height) {
        size_t move_size = (buffer->height - y - count) * buffer->width * sizeof(terminal_cell_t);
        memmove(&buffer->cells[y * buffer->width],
                &buffer->cells[(y + count) * buffer->width], move_size);
    }
    
    // Clear the bottom lines
    for (uint16_t line = buffer->height - count; line < buffer->height; line++) {
        for (uint16_t x = 0; x < buffer->width; x++) {
            terminal_cell_t* cell = &buffer->cells[line * buffer->width + x];
            cell->character = ' ';
            cell->fg_color = term->current_fg_color;
            cell->bg_color = term->current_bg_color;
            cell->attributes = TERMINAL_ATTR_NORMAL;
        }
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

int terminal_insert_chars(terminal_t* term, uint16_t count) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    if (count == 0) {
        return TERMINAL_SUCCESS;
    }
    
    terminal_buffer_t* buffer = term->active_buffer;
    uint16_t x = term->cursor.x;
    uint16_t y = term->cursor.y;
    
    if (x >= buffer->width || y >= buffer->height) {
        return TERMINAL_ERROR_OUT_OF_BOUNDS;
    }
    
    // Limit count to available space
    if (count > buffer->width - x) {
        count = buffer->width - x;
    }
    
    terminal_cell_t* line = &buffer->cells[y * buffer->width];
    
    // Move existing characters to the right
    if (x + count < buffer->width) {
        size_t move_size = (buffer->width - x - count) * sizeof(terminal_cell_t);
        memmove(&line[x + count], &line[x], move_size);
    }
    
    // Clear the inserted characters
    for (uint16_t i = x; i < x + count; i++) {
        line[i].character = ' ';
        line[i].fg_color = term->current_fg_color;
        line[i].bg_color = term->current_bg_color;
        line[i].attributes = TERMINAL_ATTR_NORMAL;
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

int terminal_delete_chars(terminal_t* term, uint16_t count) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    if (count == 0) {
        return TERMINAL_SUCCESS;
    }
    
    terminal_buffer_t* buffer = term->active_buffer;
    uint16_t x = term->cursor.x;
    uint16_t y = term->cursor.y;
    
    if (x >= buffer->width || y >= buffer->height) {
        return TERMINAL_ERROR_OUT_OF_BOUNDS;
    }
    
    // Limit count to available characters
    if (count > buffer->width - x) {
        count = buffer->width - x;
    }
    
    terminal_cell_t* line = &buffer->cells[y * buffer->width];
    
    // Move characters to the left
    if (x + count < buffer->width) {
        size_t move_size = (buffer->width - x - count) * sizeof(terminal_cell_t);
        memmove(&line[x], &line[x + count], move_size);
    }
    
    // Clear the rightmost characters
    for (uint16_t i = buffer->width - count; i < buffer->width; i++) {
        line[i].character = ' ';
        line[i].fg_color = term->current_fg_color;
        line[i].bg_color = term->current_bg_color;
        line[i].attributes = TERMINAL_ATTR_NORMAL;
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

int terminal_erase_chars(terminal_t* term, uint16_t count) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    if (count == 0) {
        return TERMINAL_SUCCESS;
    }
    
    terminal_buffer_t* buffer = term->active_buffer;
    uint16_t x = term->cursor.x;
    uint16_t y = term->cursor.y;
    
    if (x >= buffer->width || y >= buffer->height) {
        return TERMINAL_ERROR_OUT_OF_BOUNDS;
    }
    
    // Limit count to available characters
    if (count > buffer->width - x) {
        count = buffer->width - x;
    }
    
    terminal_cell_t* line = &buffer->cells[y * buffer->width];
    
    // Erase characters
    for (uint16_t i = x; i < x + count; i++) {
        line[i].character = ' ';
        line[i].fg_color = term->current_fg_color;
        line[i].bg_color = term->current_bg_color;
        line[i].attributes = TERMINAL_ATTR_NORMAL;
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

/* ========================== Attribute Management ========================== */

int terminal_set_fg_color(terminal_t* term, uint8_t color) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    term->current_fg_color = color;
    return TERMINAL_SUCCESS;
}

int terminal_set_bg_color(terminal_t* term, uint8_t color) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    term->current_bg_color = color;
    return TERMINAL_SUCCESS;
}

int terminal_set_attributes(terminal_t* term, uint8_t attributes) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    term->current_attributes = attributes;
    return TERMINAL_SUCCESS;
}

int terminal_reset_attributes(terminal_t* term) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    term->current_attributes = TERMINAL_ATTR_NORMAL;
    term->current_fg_color = term->config.default_fg_color;
    term->current_bg_color = term->config.default_bg_color;
    
    return TERMINAL_SUCCESS;
}

/* ========================== Rendering Interface ========================== */

int terminal_render_screen(terminal_t* term) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    // This function should interface with the VGA/framebuffer driver
    // For now, we just mark the terminal as clean
    term->stats.screen_updates++;
    term->dirty = false;
    
    return TERMINAL_SUCCESS;
}

int terminal_render_cursor(terminal_t* term) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    // This function should render the cursor at the current position
    // Implementation depends on the display driver
    
    return TERMINAL_SUCCESS;
}

int terminal_get_screen_buffer(terminal_t* term, terminal_cell_t** buffer, 
                              uint16_t* width, uint16_t* height) {
    if (!term || !term->initialized || !buffer || !width || !height) {
        return TERMINAL_ERROR_INVALID;
    }
    
    *buffer = term->active_buffer->cells;
    *width = term->active_buffer->width;
    *height = term->active_buffer->height;
    
    return TERMINAL_SUCCESS;
}

/* ========================== Statistics and Utility Functions ========================== */

int terminal_get_stats(terminal_t* term, terminal_stats_t* stats) {
    if (!term || !term->initialized || !stats) {
        return TERMINAL_ERROR_INVALID;
    }
    
    *stats = term->stats;
    return TERMINAL_SUCCESS;
}

/* ========================== Color Conversion Utilities ========================== */

uint8_t terminal_rgb_to_color(uint8_t r, uint8_t g, uint8_t b) {
    // Simple RGB to 16-color conversion
    uint8_t color = 0;
    
    if (r > 128) color |= 1;
    if (g > 128) color |= 2;
    if (b > 128) color |= 4;
    
    // Add bright bit if any component is very bright
    if (r > 192 || g > 192 || b > 192) {
        color |= 8;
    }
    
    return color;
}

void terminal_color_to_rgb(uint8_t color, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (!r || !g || !b) {
        return;
    }
    
    bool bright = (color & 8) != 0;
    uint8_t base = bright ? 192 : 128;
    uint8_t dim = bright ? 64 : 0;
    
    *r = (color & 1) ? base : dim;
    *g = (color & 2) ? base : dim;
    *b = (color & 4) ? base : dim;
}

uint16_t terminal_color_to_vga(uint8_t fg_color, uint8_t bg_color) {
    return ((bg_color & 0x0F) << 4) | (fg_color & 0x0F);
}

/* ========================== VT100/ANSI Compatibility Functions ========================== */

int terminal_device_attributes(terminal_t* term, char* response, size_t size) {
    if (!term || !term->initialized || !response || size == 0) {
        return TERMINAL_ERROR_INVALID;
    }
    
    // VT100 device attributes response
    const char* da_response = "\e[?1;0c";
    size_t len = strlen(da_response);
    
    if (size <= len) {
        return TERMINAL_ERROR_BUFFER_FULL;
    }
    
    strcpy(response, da_response);
    return len;
}

int terminal_cursor_position_report(terminal_t* term, char* response, size_t size) {
    if (!term || !term->initialized || !response || size == 0) {
        return TERMINAL_ERROR_INVALID;
    }
    
    int len = snprintf(response, size, "\e[%d;%dR", 
                      term->cursor.y + 1, term->cursor.x + 1);
    
    if (len >= (int)size) {
        return TERMINAL_ERROR_BUFFER_FULL;
    }
    
    return len;
}

int terminal_set_scroll_region(terminal_t* term, uint16_t top, uint16_t bottom) {
    if (!term || !term->initialized) {
        return TERMINAL_ERROR_NOT_INIT;
    }
    
    if (top >= term->active_buffer->height || bottom >= term->active_buffer->height || top > bottom) {
        return TERMINAL_ERROR_INVALID;
    }
    
    term->active_buffer->scroll_top = top;
    term->active_buffer->scroll_bottom = bottom;
    
    // Move cursor to top-left of scroll region if in origin mode
    if (term->config.origin_mode) {
        term->cursor.x = 0;
        term->cursor.y = top;
    }
    
    return TERMINAL_SUCCESS;
}

/* ========================== Debug and Testing Functions ========================== */

void terminal_dump_state(terminal_t* term) {
    if (!term || !term->initialized) {
        printf("Terminal not initialized\n");
        return;
    }
    
    printf("Terminal State Dump:\n");
    printf("  Size: %dx%d\n", term->config.size.width, term->config.size.height);
    printf("  Cursor: (%d, %d)\n", term->cursor.x, term->cursor.y);
    printf("  Colors: FG=%d, BG=%d\n", term->current_fg_color, term->current_bg_color);
    printf("  Attributes: 0x%02X\n", term->current_attributes);
    printf("  Parser State: %d\n", term->parser.state);
    printf("  Alt Screen: %s\n", term->in_alt_screen ? "Yes" : "No");
    printf("  Dirty: %s\n", term->dirty ? "Yes" : "No");
    printf("  Scrollback: %d/%d lines\n", term->scrollback_count, term->scrollback_size);
    printf("  Memory Usage: %d bytes\n", term->stats.memory_usage);
}

void terminal_dump_buffer(terminal_t* term) {
    if (!term || !term->initialized || !term->active_buffer->cells) {
        printf("Terminal buffer not available\n");
        return;
    }
    
    printf("Terminal Buffer Dump (%dx%d):\n", 
           term->active_buffer->width, term->active_buffer->height);
    
    for (uint16_t y = 0; y < term->active_buffer->height; y++) {
        printf("Line %2d: ", y);
        for (uint16_t x = 0; x < term->active_buffer->width; x++) {
            terminal_cell_t* cell = &term->active_buffer->cells[y * term->active_buffer->width + x];
            char c = (cell->character >= 32 && cell->character <= 126) ? 
                     (char)cell->character : '.';
            printf("%c", c);
        }
        printf("\n");
    }
}

int terminal_self_test(void) {
    printf("Running Terminal Emulator Self Test...\n");
    
    // Test terminal creation and initialization
    terminal_t term;
    if (terminal_init(&term, 80, 25) != TERMINAL_SUCCESS) {
        printf("FAIL: Terminal initialization\n");
        return -1;
    }
    
    // Test basic character writing
    if (terminal_write_string(&term, "Hello, World!") != TERMINAL_SUCCESS) {
        printf("FAIL: String writing\n");
        terminal_destroy(&term);
        return -1;
    }
    
    // Test cursor movement
    if (terminal_set_cursor(&term, 10, 5) != TERMINAL_SUCCESS) {
        printf("FAIL: Cursor positioning\n");
        terminal_destroy(&term);
        return -1;
    }
    
    uint16_t x, y;
    if (terminal_get_cursor(&term, &x, &y) != TERMINAL_SUCCESS || x != 10 || y != 5) {
        printf("FAIL: Cursor position retrieval\n");
        terminal_destroy(&term);
        return -1;
    }
    
    // Test screen clearing
    if (terminal_clear_screen(&term) != TERMINAL_SUCCESS) {
        printf("FAIL: Screen clearing\n");
        terminal_destroy(&term);
        return -1;
    }
    
    // Test escape sequence processing
    if (terminal_write_string(&term, "\e[31mRed text\e[0m") != TERMINAL_SUCCESS) {
        printf("FAIL: Escape sequence processing\n");
        terminal_destroy(&term);
        return -1;
    }
    
    // Clean up
    terminal_destroy(&term);
    
    printf("PASS: All terminal tests completed successfully\n");
    return 0;
}
