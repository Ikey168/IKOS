/* IKOS Terminal Emulator - Test Program
 * Issue #34 - VT100/ANSI Terminal Emulator Implementation
 * 
 * Comprehensive test suite for the terminal emulator functionality,
 * including escape sequences, cursor control, and screen manipulation.
 */

#include "terminal.h"
#include "stdio.h"
#include "string.h"
#include <assert.h>

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test macros */
#define TEST_START(name) \
    do { \
        tests_run++; \
        printf("Testing %s... ", name); \
    } while(0)

#define TEST_PASS() \
    do { \
        tests_passed++; \
        printf("PASS\n"); \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        tests_failed++; \
        printf("FAIL: %s\n", msg); \
    } while(0)

#define TEST_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            TEST_FAIL(msg); \
            return; \
        } \
    } while(0)

/* Test helper functions */
static void dump_test_results(void);
static void test_terminal_init_destroy(void);
static void test_terminal_resize(void);
static void test_cursor_operations(void);
static void test_character_writing(void);
static void test_screen_manipulation(void);
static void test_escape_sequences(void);
static void test_color_and_attributes(void);
static void test_scrolling(void);
static void test_input_handling(void);
static void test_tab_stops(void);
static void test_alternate_screen(void);
static void test_scrollback_buffer(void);
static void test_line_operations(void);
static void test_character_operations(void);
static void verify_screen_content(terminal_t* term, int x, int y, char expected);
static void verify_cursor_position(terminal_t* term, int expected_x, int expected_y);

/* ========================== Main Test Function ========================== */

int main(void) {
    printf("IKOS Terminal Emulator Test Suite\n");
    printf("=================================\n\n");
    
    // Run all tests
    test_terminal_init_destroy();
    test_terminal_resize();
    test_cursor_operations();
    test_character_writing();
    test_screen_manipulation();
    test_escape_sequences();
    test_color_and_attributes();
    test_scrolling();
    test_input_handling();
    test_tab_stops();
    test_alternate_screen();
    test_scrollback_buffer();
    test_line_operations();
    test_character_operations();
    
    // Dump results
    dump_test_results();
    
    return (tests_failed > 0) ? 1 : 0;
}

/* ========================== Test Implementations ========================== */

static void test_terminal_init_destroy(void) {
    TEST_START("terminal initialization and destruction");
    
    terminal_t term;
    
    // Test successful initialization
    int result = terminal_init(&term, 80, 25);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "initialization failed");
    TEST_ASSERT(term.initialized == true, "initialized flag not set");
    TEST_ASSERT(term.config.size.width == 80, "width not set correctly");
    TEST_ASSERT(term.config.size.height == 25, "height not set correctly");
    TEST_ASSERT(term.cursor.x == 0, "cursor x not initialized to 0");
    TEST_ASSERT(term.cursor.y == 0, "cursor y not initialized to 0");
    
    // Test invalid parameters
    terminal_t term2;
    result = terminal_init(&term2, 0, 25);
    TEST_ASSERT(result == TERMINAL_ERROR_INVALID, "should fail with width 0");
    
    result = terminal_init(&term2, 80, 0);
    TEST_ASSERT(result == TERMINAL_ERROR_INVALID, "should fail with height 0");
    
    result = terminal_init(&term2, TERMINAL_MAX_WIDTH + 1, 25);
    TEST_ASSERT(result == TERMINAL_ERROR_INVALID, "should fail with width too large");
    
    // Test destruction
    terminal_destroy(&term);
    TEST_ASSERT(term.initialized == false, "initialized flag not cleared");
    
    TEST_PASS();
}

static void test_terminal_resize(void) {
    TEST_START("terminal resizing");
    
    terminal_t term;
    terminal_init(&term, 80, 25);
    
    // Test successful resize
    int result = terminal_resize(&term, 100, 30);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "resize failed");
    TEST_ASSERT(term.config.size.width == 100, "width not updated");
    TEST_ASSERT(term.config.size.height == 30, "height not updated");
    
    // Test cursor adjustment on resize
    terminal_set_cursor(&term, 50, 20);
    result = terminal_resize(&term, 40, 15);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "resize failed");
    TEST_ASSERT(term.cursor.x == 39, "cursor x not adjusted");
    TEST_ASSERT(term.cursor.y == 14, "cursor y not adjusted");
    
    // Test invalid resize
    result = terminal_resize(&term, 0, 25);
    TEST_ASSERT(result == TERMINAL_ERROR_INVALID, "should fail with invalid dimensions");
    
    terminal_destroy(&term);
    TEST_PASS();
}

static void test_cursor_operations(void) {
    TEST_START("cursor operations");
    
    terminal_t term;
    terminal_init(&term, 80, 25);
    
    // Test cursor positioning
    int result = terminal_set_cursor(&term, 10, 5);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "set cursor failed");
    verify_cursor_position(&term, 10, 5);
    
    // Test cursor movement
    result = terminal_move_cursor(&term, 5, 3);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "move cursor failed");
    verify_cursor_position(&term, 15, 8);
    
    // Test cursor movement with bounds checking
    result = terminal_move_cursor(&term, 100, 100);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "move cursor failed");
    verify_cursor_position(&term, 79, 24);
    
    result = terminal_move_cursor(&term, -100, -100);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "move cursor failed");
    verify_cursor_position(&term, 0, 0);
    
    // Test cursor save/restore
    terminal_set_cursor(&term, 20, 10);
    terminal_save_cursor(&term);
    terminal_set_cursor(&term, 50, 15);
    terminal_restore_cursor(&term);
    verify_cursor_position(&term, 20, 10);
    
    terminal_destroy(&term);
    TEST_PASS();
}

static void test_character_writing(void) {
    TEST_START("character writing");
    
    terminal_t term;
    terminal_init(&term, 80, 25);
    
    // Test single character writing
    int result = terminal_write_char(&term, 'A');
    TEST_ASSERT(result == TERMINAL_SUCCESS, "write char failed");
    verify_screen_content(&term, 0, 0, 'A');
    verify_cursor_position(&term, 1, 0);
    
    // Test string writing
    result = terminal_write_string(&term, "Hello");
    TEST_ASSERT(result == TERMINAL_SUCCESS, "write string failed");
    verify_screen_content(&term, 1, 0, 'H');
    verify_screen_content(&term, 2, 0, 'e');
    verify_screen_content(&term, 5, 0, 'o');
    verify_cursor_position(&term, 6, 0);
    
    // Test newline handling
    result = terminal_write_char(&term, '\n');
    TEST_ASSERT(result == TERMINAL_SUCCESS, "write newline failed");
    verify_cursor_position(&term, 0, 1);
    
    // Test carriage return
    terminal_write_string(&term, "Test");
    result = terminal_write_char(&term, '\r');
    TEST_ASSERT(result == TERMINAL_SUCCESS, "write carriage return failed");
    verify_cursor_position(&term, 0, 1);
    
    // Test backspace
    terminal_set_cursor(&term, 5, 5);
    result = terminal_write_char(&term, '\b');
    TEST_ASSERT(result == TERMINAL_SUCCESS, "write backspace failed");
    verify_cursor_position(&term, 4, 5);
    
    terminal_destroy(&term);
    TEST_PASS();
}

static void test_screen_manipulation(void) {
    TEST_START("screen manipulation");
    
    terminal_t term;
    terminal_init(&term, 80, 25);
    
    // Fill screen with test data
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            terminal_set_cursor(&term, x, y);
            terminal_write_char(&term, 'A' + (x % 26));
        }
    }
    
    // Test screen clearing
    int result = terminal_clear_screen(&term);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "clear screen failed");
    verify_cursor_position(&term, 0, 0);
    verify_screen_content(&term, 0, 0, ' ');
    verify_screen_content(&term, 79, 24, ' ');
    
    // Test line clearing
    terminal_write_string(&term, "Test line");
    terminal_set_cursor(&term, 0, 0);
    result = terminal_clear_line(&term);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "clear line failed");
    verify_screen_content(&term, 0, 0, ' ');
    verify_screen_content(&term, 8, 0, ' ');
    
    terminal_destroy(&term);
    TEST_PASS();
}

static void test_escape_sequences(void) {
    TEST_START("escape sequence processing");
    
    terminal_t term;
    terminal_init(&term, 80, 25);
    
    // Test cursor movement escape sequences
    int result = terminal_write_string(&term, "\e[10;20H");
    TEST_ASSERT(result == TERMINAL_SUCCESS, "cursor position escape failed");
    verify_cursor_position(&term, 19, 9);  // 1-based to 0-based conversion
    
    result = terminal_write_string(&term, "\e[5A");
    TEST_ASSERT(result == TERMINAL_SUCCESS, "cursor up escape failed");
    verify_cursor_position(&term, 19, 4);
    
    result = terminal_write_string(&term, "\e[3B");
    TEST_ASSERT(result == TERMINAL_SUCCESS, "cursor down escape failed");
    verify_cursor_position(&term, 19, 7);
    
    result = terminal_write_string(&term, "\e[5C");
    TEST_ASSERT(result == TERMINAL_SUCCESS, "cursor forward escape failed");
    verify_cursor_position(&term, 24, 7);
    
    result = terminal_write_string(&term, "\e[2D");
    TEST_ASSERT(result == TERMINAL_SUCCESS, "cursor backward escape failed");
    verify_cursor_position(&term, 22, 7);
    
    // Test screen erase escape sequences
    terminal_write_string(&term, "Test content");
    result = terminal_write_string(&term, "\e[2J");
    TEST_ASSERT(result == TERMINAL_SUCCESS, "erase display escape failed");
    verify_screen_content(&term, 0, 0, ' ');
    
    terminal_destroy(&term);
    TEST_PASS();
}

static void test_color_and_attributes(void) {
    TEST_START("color and text attributes");
    
    terminal_t term;
    terminal_init(&term, 80, 25);
    
    // Test color setting
    int result = terminal_set_fg_color(&term, TERMINAL_COLOR_RED);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "set foreground color failed");
    TEST_ASSERT(term.current_fg_color == TERMINAL_COLOR_RED, "foreground color not set");
    
    result = terminal_set_bg_color(&term, TERMINAL_COLOR_BLUE);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "set background color failed");
    TEST_ASSERT(term.current_bg_color == TERMINAL_COLOR_BLUE, "background color not set");
    
    // Test attribute setting
    result = terminal_set_attributes(&term, TERMINAL_ATTR_BOLD | TERMINAL_ATTR_UNDERLINE);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "set attributes failed");
    TEST_ASSERT(term.current_attributes == (TERMINAL_ATTR_BOLD | TERMINAL_ATTR_UNDERLINE), 
               "attributes not set");
    
    // Test attribute reset
    result = terminal_reset_attributes(&term);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "reset attributes failed");
    TEST_ASSERT(term.current_attributes == TERMINAL_ATTR_NORMAL, "attributes not reset");
    
    // Test SGR escape sequences
    result = terminal_write_string(&term, "\e[31;1m");  // Red, bold
    TEST_ASSERT(result == TERMINAL_SUCCESS, "SGR escape failed");
    TEST_ASSERT(term.current_fg_color == TERMINAL_COLOR_RED, "SGR color not set");
    TEST_ASSERT(term.current_attributes & TERMINAL_ATTR_BOLD, "SGR bold not set");
    
    result = terminal_write_string(&term, "\e[0m");  // Reset
    TEST_ASSERT(result == TERMINAL_SUCCESS, "SGR reset failed");
    TEST_ASSERT(term.current_attributes == TERMINAL_ATTR_NORMAL, "SGR reset failed");
    
    terminal_destroy(&term);
    TEST_PASS();
}

static void test_scrolling(void) {
    TEST_START("scrolling operations");
    
    terminal_t term;
    terminal_init(&term, 80, 25);
    
    // Fill screen with test pattern
    for (int y = 0; y < 25; y++) {
        terminal_set_cursor(&term, 0, y);
        char line[10];
        snprintf(line, sizeof(line), "Line %02d", y);
        terminal_write_string(&term, line);
    }
    
    // Test scroll up
    int result = terminal_scroll_up(&term, 3);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "scroll up failed");
    
    // Test scroll down
    result = terminal_scroll_down(&term, 2);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "scroll down failed");
    
    terminal_destroy(&term);
    TEST_PASS();
}

static void test_input_handling(void) {
    TEST_START("input handling");
    
    terminal_t term;
    terminal_init(&term, 80, 25);
    
    // Test key handling
    int result = terminal_handle_key(&term, 'A');
    TEST_ASSERT(result == TERMINAL_SUCCESS, "handle regular key failed");
    
    result = terminal_handle_key(&term, TERMINAL_KEY_UP);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "handle arrow key failed");
    
    result = terminal_handle_key(&term, TERMINAL_KEY_F1);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "handle function key failed");
    
    // Test character reading
    int c = terminal_read_char(&term);
    TEST_ASSERT(c == 'A', "read char failed");
    
    terminal_destroy(&term);
    TEST_PASS();
}

static void test_tab_stops(void) {
    TEST_START("tab stop management");
    
    terminal_t term;
    terminal_init(&term, 80, 25);
    
    // Test default tab stops
    terminal_set_cursor(&term, 0, 0);
    terminal_write_char(&term, '\t');
    verify_cursor_position(&term, 8, 0);
    
    // Test custom tab stop
    terminal_set_tab_stop(&term, 20);
    terminal_set_cursor(&term, 15, 0);
    terminal_write_char(&term, '\t');
    verify_cursor_position(&term, 20, 0);
    
    // Test tab stop clearing
    terminal_clear_tab_stop(&term, 20);
    terminal_set_cursor(&term, 15, 0);
    terminal_write_char(&term, '\t');
    verify_cursor_position(&term, 24, 0);  // Next default tab stop
    
    terminal_destroy(&term);
    TEST_PASS();
}

static void test_alternate_screen(void) {
    TEST_START("alternate screen buffer");
    
    terminal_t term;
    terminal_init(&term, 80, 25);
    
    // Write to main screen
    terminal_write_string(&term, "Main screen content");
    
    // Switch to alternate screen
    int result = terminal_switch_to_alt_screen(&term);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "switch to alt screen failed");
    TEST_ASSERT(term.in_alt_screen == true, "alt screen flag not set");
    
    // Write to alternate screen
    terminal_write_string(&term, "Alt screen content");
    
    // Switch back to main screen
    result = terminal_switch_to_main_screen(&term);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "switch to main screen failed");
    TEST_ASSERT(term.in_alt_screen == false, "alt screen flag not cleared");
    
    terminal_destroy(&term);
    TEST_PASS();
}

static void test_scrollback_buffer(void) {
    TEST_START("scrollback buffer");
    
    terminal_t term;
    terminal_init(&term, 80, 25);
    
    // Fill screen and cause scrolling to populate scrollback
    for (int i = 0; i < 50; i++) {
        char line[20];
        snprintf(line, sizeof(line), "Scrollback line %d\n", i);
        terminal_write_string(&term, line);
    }
    
    // Test scrollback retrieval
    terminal_cell_t line_buffer[80];
    int result = terminal_get_scrollback_line(&term, -1, line_buffer, 80);
    TEST_ASSERT(result > 0, "get scrollback line failed");
    
    // Test scrollback clearing
    result = terminal_clear_scrollback(&term);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "clear scrollback failed");
    TEST_ASSERT(term.scrollback_count == 0, "scrollback not cleared");
    
    terminal_destroy(&term);
    TEST_PASS();
}

static void test_line_operations(void) {
    TEST_START("line operations");
    
    terminal_t term;
    terminal_init(&term, 80, 25);
    
    // Test line insertion
    terminal_set_cursor(&term, 0, 10);
    int result = terminal_insert_lines(&term, 3);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "insert lines failed");
    
    // Test line deletion
    result = terminal_delete_lines(&term, 2);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "delete lines failed");
    
    terminal_destroy(&term);
    TEST_PASS();
}

static void test_character_operations(void) {
    TEST_START("character operations");
    
    terminal_t term;
    terminal_init(&term, 80, 25);
    
    // Set up test line
    terminal_write_string(&term, "Hello World");
    terminal_set_cursor(&term, 5, 0);
    
    // Test character insertion
    int result = terminal_insert_chars(&term, 3);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "insert chars failed");
    
    // Test character deletion
    result = terminal_delete_chars(&term, 2);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "delete chars failed");
    
    // Test character erasure
    result = terminal_erase_chars(&term, 1);
    TEST_ASSERT(result == TERMINAL_SUCCESS, "erase chars failed");
    
    terminal_destroy(&term);
    TEST_PASS();
}

/* ========================== Test Helper Functions ========================== */

static void verify_screen_content(terminal_t* term, int x, int y, char expected) {
    if (x >= 0 && x < (int)term->active_buffer->width && 
        y >= 0 && y < (int)term->active_buffer->height) {
        terminal_cell_t* cell = &term->active_buffer->cells[y * term->active_buffer->width + x];
        if (cell->character != (uint16_t)expected) {
            char msg[100];
            snprintf(msg, sizeof(msg), "expected '%c' at (%d,%d), got '%c'", 
                    expected, x, y, (char)cell->character);
            TEST_FAIL(msg);
        }
    }
}

static void verify_cursor_position(terminal_t* term, int expected_x, int expected_y) {
    if (term->cursor.x != (uint16_t)expected_x || term->cursor.y != (uint16_t)expected_y) {
        char msg[100];
        snprintf(msg, sizeof(msg), "expected cursor at (%d,%d), got (%d,%d)", 
                expected_x, expected_y, term->cursor.x, term->cursor.y);
        TEST_FAIL(msg);
    }
}

static void dump_test_results(void) {
    printf("\n=================================\n");
    printf("Test Results Summary:\n");
    printf("  Total tests run: %d\n", tests_run);
    printf("  Tests passed: %d\n", tests_passed);
    printf("  Tests failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("  Result: ALL TESTS PASSED\n");
    } else {
        printf("  Result: %d TESTS FAILED\n", tests_failed);
    }
    printf("=================================\n");
}
