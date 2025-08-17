/* IKOS Terminal Emulator - VT100/ANSI Escape Sequence Processor
 * Issue #34 - VT100/ANSI Terminal Emulator Implementation
 * 
 * Handles parsing and processing of VT100/ANSI escape sequences for
 * cursor control, text formatting, and terminal control commands.
 */

#include "terminal.h"
#include "string.h"
#include "stdio.h"
#include <ctype.h>
#include <stdlib.h>

/* ========================== Static Function Declarations ========================== */

static int terminal_parse_csi_sequence(terminal_t* term, const char* seq);
static int terminal_execute_csi_command(terminal_t* term, char final_char, 
                                       int* params, int param_count);
static int terminal_parse_osc_sequence(terminal_t* term, const char* seq);
static int terminal_parse_dcs_sequence(terminal_t* term, const char* seq);
static void terminal_reset_parser(terminal_t* term);
static bool is_final_character(char c);
static bool is_intermediate_character(char c);
static bool is_parameter_character(char c);
static int terminal_parse_parameters(const char* param_str, int* params, int max_params);

/* CSI Command handlers */
static int handle_cursor_up(terminal_t* term, int* params, int param_count);
static int handle_cursor_down(terminal_t* term, int* params, int param_count);
static int handle_cursor_forward(terminal_t* term, int* params, int param_count);
static int handle_cursor_backward(terminal_t* term, int* params, int param_count);
static int handle_cursor_next_line(terminal_t* term, int* params, int param_count);
static int handle_cursor_prev_line(terminal_t* term, int* params, int param_count);
static int handle_cursor_horizontal_absolute(terminal_t* term, int* params, int param_count);
static int handle_cursor_position(terminal_t* term, int* params, int param_count);
static int handle_erase_display(terminal_t* term, int* params, int param_count);
static int handle_erase_line(terminal_t* term, int* params, int param_count);
static int handle_scroll_up(terminal_t* term, int* params, int param_count);
static int handle_scroll_down(terminal_t* term, int* params, int param_count);
static int handle_select_graphic_rendition(terminal_t* term, int* params, int param_count);
static int handle_device_status_report(terminal_t* term, int* params, int param_count);
static int handle_set_mode(terminal_t* term, int* params, int param_count);
static int handle_reset_mode(terminal_t* term, int* params, int param_count);
static int handle_save_cursor(terminal_t* term, int* params, int param_count);
static int handle_restore_cursor(terminal_t* term, int* params, int param_count);
static int handle_insert_lines(terminal_t* term, int* params, int param_count);
static int handle_delete_lines(terminal_t* term, int* params, int param_count);
static int handle_insert_characters(terminal_t* term, int* params, int param_count);
static int handle_delete_characters(terminal_t* term, int* params, int param_count);
static int handle_erase_characters(terminal_t* term, int* params, int param_count);
static int handle_set_scroll_region(terminal_t* term, int* params, int param_count);

/* ========================== Main Escape Sequence Processor ========================== */

int terminal_process_escape_sequence(terminal_t* term, const char* sequence) {
    if (!term || !term->initialized || !sequence) {
        return TERMINAL_ERROR_INVALID;
    }
    
    char c = *sequence;
    terminal_parser_t* parser = &term->parser;
    
    switch (parser->state) {
        case TERMINAL_STATE_ESCAPE:
            // Handle ESC sequences
            switch (c) {
                case '[':
                    parser->state = TERMINAL_STATE_CSI;
                    parser->buffer_pos = 0;
                    parser->param_count = 0;
                    break;
                    
                case ']':
                    parser->state = TERMINAL_STATE_OSC;
                    parser->buffer_pos = 0;
                    break;
                    
                case 'P':
                    parser->state = TERMINAL_STATE_DCS;
                    parser->buffer_pos = 0;
                    break;
                    
                case 'D':  // Index (IND)
                    if (term->cursor.y >= term->active_buffer->height - 1) {
                        terminal_scroll_buffer_up(term, 1);
                    } else {
                        term->cursor.y++;
                    }
                    terminal_reset_parser(term);
                    break;
                    
                case 'E':  // Next Line (NEL)
                    terminal_move_cursor_to_next_line(term);
                    terminal_reset_parser(term);
                    break;
                    
                case 'H':  // Tab Set (HTS)
                    terminal_set_tab_stop(term, term->cursor.x);
                    terminal_reset_parser(term);
                    break;
                    
                case 'M':  // Reverse Index (RI)
                    if (term->cursor.y == 0) {
                        terminal_scroll_buffer_down(term, 1);
                    } else {
                        term->cursor.y--;
                    }
                    terminal_reset_parser(term);
                    break;
                    
                case 'Z':  // Identify Terminal (DECID)
                    // Send device attributes response
                    // TODO: Implement output to host
                    terminal_reset_parser(term);
                    break;
                    
                case '7':  // Save Cursor (DECSC)
                    terminal_save_cursor(term);
                    terminal_reset_parser(term);
                    break;
                    
                case '8':  // Restore Cursor (DECRC)
                    terminal_restore_cursor(term);
                    terminal_reset_parser(term);
                    break;
                    
                case '=':  // Application Keypad (DECKPAM)
                    term->config.application_keypad = true;
                    terminal_reset_parser(term);
                    break;
                    
                case '>':  // Normal Keypad (DECKPNM)
                    term->config.application_keypad = false;
                    terminal_reset_parser(term);
                    break;
                    
                case 'c':  // Reset Terminal (RIS)
                    terminal_reset(term);
                    terminal_reset_parser(term);
                    break;
                    
                default:
                    // Unknown escape sequence, ignore
                    terminal_reset_parser(term);
                    break;
            }
            break;
            
        case TERMINAL_STATE_CSI:
            // Handle CSI sequences
            if (parser->buffer_pos < TERMINAL_ESCAPE_BUFFER_SIZE - 1) {
                parser->buffer[parser->buffer_pos++] = c;
                parser->buffer[parser->buffer_pos] = '\0';
            }
            
            if (is_final_character(c)) {
                terminal_parse_csi_sequence(term, parser->buffer);
                terminal_reset_parser(term);
            } else if (parser->buffer_pos >= TERMINAL_ESCAPE_BUFFER_SIZE - 1) {
                // Buffer overflow, reset parser
                terminal_reset_parser(term);
            }
            break;
            
        case TERMINAL_STATE_OSC:
            // Handle OSC sequences
            if (parser->buffer_pos < TERMINAL_ESCAPE_BUFFER_SIZE - 1) {
                parser->buffer[parser->buffer_pos++] = c;
                parser->buffer[parser->buffer_pos] = '\0';
            }
            
            if (c == '\a' || c == 27) {  // BEL or ESC terminates OSC
                terminal_parse_osc_sequence(term, parser->buffer);
                terminal_reset_parser(term);
            } else if (parser->buffer_pos >= TERMINAL_ESCAPE_BUFFER_SIZE - 1) {
                terminal_reset_parser(term);
            }
            break;
            
        case TERMINAL_STATE_DCS:
            // Handle DCS sequences
            if (parser->buffer_pos < TERMINAL_ESCAPE_BUFFER_SIZE - 1) {
                parser->buffer[parser->buffer_pos++] = c;
                parser->buffer[parser->buffer_pos] = '\0';
            }
            
            if (c == 27) {  // ESC terminates DCS
                terminal_parse_dcs_sequence(term, parser->buffer);
                terminal_reset_parser(term);
            } else if (parser->buffer_pos >= TERMINAL_ESCAPE_BUFFER_SIZE - 1) {
                terminal_reset_parser(term);
            }
            break;
            
        default:
            terminal_reset_parser(term);
            break;
    }
    
    term->stats.escape_sequences++;
    return TERMINAL_SUCCESS;
}

/* ========================== CSI Sequence Parser ========================== */

static int terminal_parse_csi_sequence(terminal_t* term, const char* seq) {
    if (!seq || strlen(seq) == 0) {
        return TERMINAL_ERROR_INVALID;
    }
    
    int params[16];
    int param_count = 0;
    char final_char = seq[strlen(seq) - 1];
    
    // Find the parameter string (everything except the final character)
    char param_str[TERMINAL_ESCAPE_BUFFER_SIZE];
    strncpy(param_str, seq, strlen(seq) - 1);
    param_str[strlen(seq) - 1] = '\0';
    
    // Parse parameters
    param_count = terminal_parse_parameters(param_str, params, 16);
    
    return terminal_execute_csi_command(term, final_char, params, param_count);
}

static int terminal_execute_csi_command(terminal_t* term, char final_char, 
                                       int* params, int param_count) {
    switch (final_char) {
        case 'A':  // Cursor Up (CUU)
            return handle_cursor_up(term, params, param_count);
            
        case 'B':  // Cursor Down (CUD)
            return handle_cursor_down(term, params, param_count);
            
        case 'C':  // Cursor Forward (CUF)
            return handle_cursor_forward(term, params, param_count);
            
        case 'D':  // Cursor Backward (CUB)
            return handle_cursor_backward(term, params, param_count);
            
        case 'E':  // Cursor Next Line (CNL)
            return handle_cursor_next_line(term, params, param_count);
            
        case 'F':  // Cursor Previous Line (CPL)
            return handle_cursor_prev_line(term, params, param_count);
            
        case 'G':  // Cursor Horizontal Absolute (CHA)
            return handle_cursor_horizontal_absolute(term, params, param_count);
            
        case 'H':  // Cursor Position (CUP)
        case 'f':  // Horizontal and Vertical Position (HVP)
            return handle_cursor_position(term, params, param_count);
            
        case 'J':  // Erase in Display (ED)
            return handle_erase_display(term, params, param_count);
            
        case 'K':  // Erase in Line (EL)
            return handle_erase_line(term, params, param_count);
            
        case 'S':  // Scroll Up (SU)
            return handle_scroll_up(term, params, param_count);
            
        case 'T':  // Scroll Down (SD)
            return handle_scroll_down(term, params, param_count);
            
        case 'm':  // Select Graphic Rendition (SGR)
            return handle_select_graphic_rendition(term, params, param_count);
            
        case 'n':  // Device Status Report (DSR)
            return handle_device_status_report(term, params, param_count);
            
        case 'h':  // Set Mode (SM)
            return handle_set_mode(term, params, param_count);
            
        case 'l':  // Reset Mode (RM)
            return handle_reset_mode(term, params, param_count);
            
        case 's':  // Save Cursor Position (SCP)
            return handle_save_cursor(term, params, param_count);
            
        case 'u':  // Restore Cursor Position (RCP)
            return handle_restore_cursor(term, params, param_count);
            
        case 'L':  // Insert Lines (IL)
            return handle_insert_lines(term, params, param_count);
            
        case 'M':  // Delete Lines (DL)
            return handle_delete_lines(term, params, param_count);
            
        case '@':  // Insert Characters (ICH)
            return handle_insert_characters(term, params, param_count);
            
        case 'P':  // Delete Characters (DCH)
            return handle_delete_characters(term, params, param_count);
            
        case 'X':  // Erase Characters (ECH)
            return handle_erase_characters(term, params, param_count);
            
        case 'r':  // Set Scroll Region (DECSTBM)
            return handle_set_scroll_region(term, params, param_count);
            
        default:
            // Unknown CSI sequence, ignore
            return TERMINAL_SUCCESS;
    }
}

/* ========================== CSI Command Handlers ========================== */

static int handle_cursor_up(terminal_t* term, int* params, int param_count) {
    int count = (param_count > 0 && params[0] > 0) ? params[0] : 1;
    
    if (term->cursor.y >= count) {
        term->cursor.y -= count;
    } else {
        term->cursor.y = 0;
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

static int handle_cursor_down(terminal_t* term, int* params, int param_count) {
    int count = (param_count > 0 && params[0] > 0) ? params[0] : 1;
    
    if (term->cursor.y + count < term->active_buffer->height) {
        term->cursor.y += count;
    } else {
        term->cursor.y = term->active_buffer->height - 1;
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

static int handle_cursor_forward(terminal_t* term, int* params, int param_count) {
    int count = (param_count > 0 && params[0] > 0) ? params[0] : 1;
    
    if (term->cursor.x + count < term->active_buffer->width) {
        term->cursor.x += count;
    } else {
        term->cursor.x = term->active_buffer->width - 1;
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

static int handle_cursor_backward(terminal_t* term, int* params, int param_count) {
    int count = (param_count > 0 && params[0] > 0) ? params[0] : 1;
    
    if (term->cursor.x >= count) {
        term->cursor.x -= count;
    } else {
        term->cursor.x = 0;
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

static int handle_cursor_position(terminal_t* term, int* params, int param_count) {
    int row = (param_count > 0 && params[0] > 0) ? params[0] - 1 : 0;
    int col = (param_count > 1 && params[1] > 0) ? params[1] - 1 : 0;
    
    // Clamp to valid range
    if (row >= (int)term->active_buffer->height) {
        row = term->active_buffer->height - 1;
    }
    if (col >= (int)term->active_buffer->width) {
        col = term->active_buffer->width - 1;
    }
    
    term->cursor.x = col;
    term->cursor.y = row;
    term->dirty = true;
    
    return TERMINAL_SUCCESS;
}

static int handle_erase_display(terminal_t* term, int* params, int param_count) {
    int mode = (param_count > 0) ? params[0] : 0;
    
    switch (mode) {
        case 0:  // Erase from cursor to end of screen
            // Clear from cursor to end of current line
            for (uint16_t x = term->cursor.x; x < term->active_buffer->width; x++) {
                terminal_put_char_at(term, x, term->cursor.y, ' ');
            }
            
            // Clear all lines below cursor
            for (uint16_t y = term->cursor.y + 1; y < term->active_buffer->height; y++) {
                for (uint16_t x = 0; x < term->active_buffer->width; x++) {
                    terminal_put_char_at(term, x, y, ' ');
                }
            }
            break;
            
        case 1:  // Erase from beginning of screen to cursor
            // Clear all lines above cursor
            for (uint16_t y = 0; y < term->cursor.y; y++) {
                for (uint16_t x = 0; x < term->active_buffer->width; x++) {
                    terminal_put_char_at(term, x, y, ' ');
                }
            }
            
            // Clear from beginning of current line to cursor
            for (uint16_t x = 0; x <= term->cursor.x; x++) {
                terminal_put_char_at(term, x, term->cursor.y, ' ');
            }
            break;
            
        case 2:  // Erase entire screen
        case 3:  // Erase entire screen and scrollback buffer
            terminal_clear_screen(term);
            if (mode == 3) {
                terminal_clear_scrollback(term);
            }
            break;
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

static int handle_erase_line(terminal_t* term, int* params, int param_count) {
    int mode = (param_count > 0) ? params[0] : 0;
    uint16_t y = term->cursor.y;
    
    switch (mode) {
        case 0:  // Erase from cursor to end of line
            for (uint16_t x = term->cursor.x; x < term->active_buffer->width; x++) {
                terminal_put_char_at(term, x, y, ' ');
            }
            break;
            
        case 1:  // Erase from beginning of line to cursor
            for (uint16_t x = 0; x <= term->cursor.x; x++) {
                terminal_put_char_at(term, x, y, ' ');
            }
            break;
            
        case 2:  // Erase entire line
            for (uint16_t x = 0; x < term->active_buffer->width; x++) {
                terminal_put_char_at(term, x, y, ' ');
            }
            break;
    }
    
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

static int handle_select_graphic_rendition(terminal_t* term, int* params, int param_count) {
    // If no parameters, reset to normal
    if (param_count == 0) {
        params = (int[]){0};
        param_count = 1;
    }
    
    for (int i = 0; i < param_count; i++) {
        int param = params[i];
        
        switch (param) {
            case 0:  // Reset all attributes
                term->current_attributes = TERMINAL_ATTR_NORMAL;
                term->current_fg_color = term->config.default_fg_color;
                term->current_bg_color = term->config.default_bg_color;
                break;
                
            case 1:  // Bold
                term->current_attributes |= TERMINAL_ATTR_BOLD;
                break;
                
            case 2:  // Dim
                term->current_attributes |= TERMINAL_ATTR_DIM;
                break;
                
            case 3:  // Italic
                term->current_attributes |= TERMINAL_ATTR_ITALIC;
                break;
                
            case 4:  // Underline
                term->current_attributes |= TERMINAL_ATTR_UNDERLINE;
                break;
                
            case 5:  // Blink
                term->current_attributes |= TERMINAL_ATTR_BLINK;
                break;
                
            case 7:  // Reverse
                term->current_attributes |= TERMINAL_ATTR_REVERSE;
                break;
                
            case 8:  // Hidden
                term->current_attributes |= TERMINAL_ATTR_HIDDEN;
                break;
                
            case 9:  // Strikethrough
                term->current_attributes |= TERMINAL_ATTR_STRIKETHROUGH;
                break;
                
            case 22:  // Normal intensity (not bold or dim)
                term->current_attributes &= ~(TERMINAL_ATTR_BOLD | TERMINAL_ATTR_DIM);
                break;
                
            case 23:  // Not italic
                term->current_attributes &= ~TERMINAL_ATTR_ITALIC;
                break;
                
            case 24:  // Not underlined
                term->current_attributes &= ~TERMINAL_ATTR_UNDERLINE;
                break;
                
            case 25:  // Not blinking
                term->current_attributes &= ~TERMINAL_ATTR_BLINK;
                break;
                
            case 27:  // Not reverse
                term->current_attributes &= ~TERMINAL_ATTR_REVERSE;
                break;
                
            case 28:  // Not hidden
                term->current_attributes &= ~TERMINAL_ATTR_HIDDEN;
                break;
                
            case 29:  // Not strikethrough
                term->current_attributes &= ~TERMINAL_ATTR_STRIKETHROUGH;
                break;
                
            // Foreground colors (30-37, 90-97)
            case 30: case 31: case 32: case 33:
            case 34: case 35: case 36: case 37:
                term->current_fg_color = param - 30;
                break;
                
            case 90: case 91: case 92: case 93:
            case 94: case 95: case 96: case 97:
                term->current_fg_color = param - 90 + 8;
                break;
                
            // Background colors (40-47, 100-107)
            case 40: case 41: case 42: case 43:
            case 44: case 45: case 46: case 47:
                term->current_bg_color = param - 40;
                break;
                
            case 100: case 101: case 102: case 103:
            case 104: case 105: case 106: case 107:
                term->current_bg_color = param - 100 + 8;
                break;
                
            case 39:  // Default foreground color
                term->current_fg_color = term->config.default_fg_color;
                break;
                
            case 49:  // Default background color
                term->current_bg_color = term->config.default_bg_color;
                break;
        }
    }
    
    return TERMINAL_SUCCESS;
}

/* ========================== Helper Functions ========================== */

static void terminal_reset_parser(terminal_t* term) {
    term->parser.state = TERMINAL_STATE_NORMAL;
    term->parser.buffer_pos = 0;
    term->parser.param_count = 0;
    term->parser.intermediate = 0;
    term->parser.final = 0;
}

static bool is_final_character(char c) {
    return (c >= 0x40 && c <= 0x7E);
}

static bool is_intermediate_character(char c) {
    return (c >= 0x20 && c <= 0x2F);
}

static bool is_parameter_character(char c) {
    return (c >= 0x30 && c <= 0x3F);
}

static int terminal_parse_parameters(const char* param_str, int* params, int max_params) {
    if (!param_str || !params || max_params <= 0) {
        return 0;
    }
    
    int param_count = 0;
    char* str_copy = strdup(param_str);
    char* token = strtok(str_copy, ";");
    
    while (token && param_count < max_params) {
        if (strlen(token) > 0) {
            params[param_count] = atoi(token);
        } else {
            params[param_count] = 0;  // Default parameter value
        }
        param_count++;
        token = strtok(NULL, ";");
    }
    
    free(str_copy);
    return param_count;
}

/* ========================== Placeholder Handlers ========================== */

static int handle_cursor_next_line(terminal_t* term, int* params, int param_count) {
    int count = (param_count > 0 && params[0] > 0) ? params[0] : 1;
    term->cursor.x = 0;
    return handle_cursor_down(term, &count, 1);
}

static int handle_cursor_prev_line(terminal_t* term, int* params, int param_count) {
    int count = (param_count > 0 && params[0] > 0) ? params[0] : 1;
    term->cursor.x = 0;
    return handle_cursor_up(term, &count, 1);
}

static int handle_cursor_horizontal_absolute(terminal_t* term, int* params, int param_count) {
    int col = (param_count > 0 && params[0] > 0) ? params[0] - 1 : 0;
    
    if (col >= (int)term->active_buffer->width) {
        col = term->active_buffer->width - 1;
    }
    
    term->cursor.x = col;
    term->dirty = true;
    return TERMINAL_SUCCESS;
}

static int handle_scroll_up(terminal_t* term, int* params, int param_count) {
    int lines = (param_count > 0 && params[0] > 0) ? params[0] : 1;
    return terminal_scroll_up(term, lines);
}

static int handle_scroll_down(terminal_t* term, int* params, int param_count) {
    int lines = (param_count > 0 && params[0] > 0) ? params[0] : 1;
    return terminal_scroll_down(term, lines);
}

static int handle_device_status_report(terminal_t* term, int* params, int param_count) {
    // TODO: Implement device status report response
    return TERMINAL_SUCCESS;
}

static int handle_set_mode(terminal_t* term, int* params, int param_count) {
    // TODO: Implement mode setting
    return TERMINAL_SUCCESS;
}

static int handle_reset_mode(terminal_t* term, int* params, int param_count) {
    // TODO: Implement mode resetting
    return TERMINAL_SUCCESS;
}

static int handle_save_cursor(terminal_t* term, int* params, int param_count) {
    terminal_save_cursor(term);
    return TERMINAL_SUCCESS;
}

static int handle_restore_cursor(terminal_t* term, int* params, int param_count) {
    terminal_restore_cursor(term);
    return TERMINAL_SUCCESS;
}

static int handle_insert_lines(terminal_t* term, int* params, int param_count) {
    int count = (param_count > 0 && params[0] > 0) ? params[0] : 1;
    return terminal_insert_lines(term, count);
}

static int handle_delete_lines(terminal_t* term, int* params, int param_count) {
    int count = (param_count > 0 && params[0] > 0) ? params[0] : 1;
    return terminal_delete_lines(term, count);
}

static int handle_insert_characters(terminal_t* term, int* params, int param_count) {
    int count = (param_count > 0 && params[0] > 0) ? params[0] : 1;
    return terminal_insert_chars(term, count);
}

static int handle_delete_characters(terminal_t* term, int* params, int param_count) {
    int count = (param_count > 0 && params[0] > 0) ? params[0] : 1;
    return terminal_delete_chars(term, count);
}

static int handle_erase_characters(terminal_t* term, int* params, int param_count) {
    int count = (param_count > 0 && params[0] > 0) ? params[0] : 1;
    return terminal_erase_chars(term, count);
}

static int handle_set_scroll_region(terminal_t* term, int* params, int param_count) {
    uint16_t top = (param_count > 0 && params[0] > 0) ? params[0] - 1 : 0;
    uint16_t bottom = (param_count > 1 && params[1] > 0) ? params[1] - 1 : term->active_buffer->height - 1;
    return terminal_set_scroll_region(term, top, bottom);
}

static int terminal_parse_osc_sequence(terminal_t* term, const char* seq) {
    // TODO: Implement OSC sequence parsing
    return TERMINAL_SUCCESS;
}

static int terminal_parse_dcs_sequence(terminal_t* term, const char* seq) {
    // TODO: Implement DCS sequence parsing
    return TERMINAL_SUCCESS;
}
