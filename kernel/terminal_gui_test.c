/*
 * IKOS Terminal GUI Integration - Test Suite
 * Issue #43 - Terminal Emulator GUI Integration
 * 
 * Comprehensive tests for GUI-integrated terminal emulator functionality.
 */

#include "terminal_gui.h"
#include "stdio.h"
#include "string.h"
#include <stdint.h>
#include <stdbool.h>

/* ================================
 * Test Function Declarations
 * ================================ */

static void test_terminal_gui_initialization(void);
static void test_terminal_gui_instance_creation(void);
static void test_terminal_gui_window_management(void);
static void test_terminal_gui_text_operations(void);
static void test_terminal_gui_event_handling(void);
static void test_terminal_gui_multiple_instances(void);
static void test_terminal_gui_tab_support(void);
static void test_terminal_gui_scrolling(void);
static void test_terminal_gui_selection_clipboard(void);
static void test_terminal_gui_command_execution(void);

static void print_test_result(const char* test_name, bool passed);
static void print_test_header(const char* test_suite_name);

/* ================================
 * Main Test Entry Point
 * ================================ */

void terminal_gui_run_tests(void) {
    print_test_header("Terminal GUI Integration Test Suite");
    
    test_terminal_gui_initialization();
    test_terminal_gui_instance_creation();
    test_terminal_gui_window_management();
    test_terminal_gui_text_operations();
    test_terminal_gui_event_handling();
    test_terminal_gui_multiple_instances();
    test_terminal_gui_tab_support();
    test_terminal_gui_scrolling();
    test_terminal_gui_selection_clipboard();
    test_terminal_gui_command_execution();
    
    printf("Terminal GUI Integration tests completed.\n\n");
}

/* ================================
 * Individual Test Functions
 * ================================ */

static void test_terminal_gui_initialization(void) {
    bool passed = true;
    
    // Test initialization
    int result = terminal_gui_init();
    if (result != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Test double initialization
    result = terminal_gui_init();
    if (result != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Test cleanup
    terminal_gui_cleanup();
    
    print_test_result("Terminal GUI Initialization", passed);
}

static void test_terminal_gui_instance_creation(void) {
    bool passed = true;
    
    // Initialize system
    if (terminal_gui_init() != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Test default instance creation
    terminal_gui_instance_t* instance = terminal_gui_create_instance(NULL);
    if (!instance) {
        passed = false;
    }
    
    // Test instance properties
    if (instance && instance->id == 0) {
        passed = false;
    }
    
    // Test instance retrieval
    terminal_gui_instance_t* retrieved = terminal_gui_get_instance(instance->id);
    if (retrieved != instance) {
        passed = false;
    }
    
    // Test instance destruction
    if (instance && terminal_gui_destroy_instance(instance) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Instance Creation", passed);
}

static void test_terminal_gui_window_management(void) {
    bool passed = true;
    
    terminal_gui_init();
    
    // Create instance
    terminal_gui_instance_t* instance = terminal_gui_create_instance(NULL);
    if (!instance) {
        passed = false;
        goto cleanup;
    }
    
    // Test show window
    if (terminal_gui_show_window(instance) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Test window title setting
    const char* test_title = "Test Terminal";
    if (terminal_gui_set_window_title(instance, test_title) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Check title was set
    if (strcmp(instance->title, test_title) != 0) {
        passed = false;
    }
    
    // Test hide window
    if (terminal_gui_hide_window(instance) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
cleanup:
    if (instance) {
        terminal_gui_destroy_instance(instance);
    }
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Window Management", passed);
}

static void test_terminal_gui_text_operations(void) {
    bool passed = true;
    
    terminal_gui_init();
    
    terminal_gui_instance_t* instance = terminal_gui_create_instance(NULL);
    if (!instance) {
        passed = false;
        goto cleanup;
    }
    
    // Test writing text
    const char* test_text = "Hello, Terminal GUI!";
    if (terminal_gui_write_text(instance, test_text, strlen(test_text)) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Test writing individual character
    if (terminal_gui_write_char(instance, '\n') != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Test clear screen
    if (terminal_gui_clear_screen(instance) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Test cursor positioning
    if (terminal_gui_set_cursor_position(instance, 10, 5) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
cleanup:
    if (instance) {
        terminal_gui_destroy_instance(instance);
    }
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Text Operations", passed);
}

static void test_terminal_gui_event_handling(void) {
    bool passed = true;
    
    terminal_gui_init();
    
    terminal_gui_instance_t* instance = terminal_gui_create_instance(NULL);
    if (!instance) {
        passed = false;
        goto cleanup;
    }
    
    // Create test events
    gui_event_t key_event = {0};
    key_event.type = GUI_EVENT_KEY_DOWN;
    key_event.data.key.keycode = 'A';
    key_event.data.key.modifiers = 0;
    
    gui_event_t mouse_event = {0};
    mouse_event.type = GUI_EVENT_MOUSE_DOWN;
    mouse_event.data.mouse.position.x = 100;
    mouse_event.data.mouse.position.y = 100;
    mouse_event.data.mouse.button = GUI_MOUSE_LEFT;
    
    gui_event_t resize_event = {0};
    resize_event.type = GUI_EVENT_WINDOW_RESIZE;
    resize_event.data.resize.width = 800;
    resize_event.data.resize.height = 600;
    
    // Test key event handling
    if (terminal_gui_handle_key_event(instance, &key_event) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Test mouse event handling
    if (terminal_gui_handle_mouse_event(instance, &mouse_event) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Test resize event handling
    if (terminal_gui_handle_resize_event(instance, &resize_event) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
cleanup:
    if (instance) {
        terminal_gui_destroy_instance(instance);
    }
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Event Handling", passed);
}

static void test_terminal_gui_multiple_instances(void) {
    bool passed = true;
    
    terminal_gui_init();
    
    // Create multiple instances
    terminal_gui_instance_t* instance1 = terminal_gui_create_instance(NULL);
    terminal_gui_instance_t* instance2 = terminal_gui_create_instance(NULL);
    terminal_gui_instance_t* instance3 = terminal_gui_create_instance(NULL);
    
    if (!instance1 || !instance2 || !instance3) {
        passed = false;
    }
    
    // Check unique IDs
    if (instance1 && instance2 && instance3) {
        if (instance1->id == instance2->id || 
            instance1->id == instance3->id || 
            instance2->id == instance3->id) {
            passed = false;
        }
    }
    
    // Test focused instance management
    terminal_gui_instance_t* focused = terminal_gui_get_focused_instance();
    // Initially no focused instance is expected
    
    // Cleanup
    if (instance1) terminal_gui_destroy_instance(instance1);
    if (instance2) terminal_gui_destroy_instance(instance2);
    if (instance3) terminal_gui_destroy_instance(instance3);
    
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Multiple Instances", passed);
}

static void test_terminal_gui_tab_support(void) {
    bool passed = true;
    
    terminal_gui_init();
    
    // Create instance with tab support
    terminal_gui_config_t config;
    terminal_gui_get_default_config(&config);
    config.enable_tabs = true;
    
    terminal_gui_instance_t* instance = terminal_gui_create_instance(&config);
    if (!instance) {
        passed = false;
        goto cleanup;
    }
    
    // Test adding tabs
    if (terminal_gui_add_tab(instance, "Tab 1") != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    if (terminal_gui_add_tab(instance, "Tab 2") != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Test switching tabs
    if (terminal_gui_switch_tab(instance, 1) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Test removing tab
    if (terminal_gui_remove_tab(instance, 0) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
cleanup:
    if (instance) {
        terminal_gui_destroy_instance(instance);
    }
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Tab Support", passed);
}

static void test_terminal_gui_scrolling(void) {
    bool passed = true;
    
    terminal_gui_init();
    
    terminal_gui_instance_t* instance = terminal_gui_create_instance(NULL);
    if (!instance) {
        passed = false;
        goto cleanup;
    }
    
    // Fill terminal with text to enable scrolling
    for (int i = 0; i < 30; i++) {
        char line[64];
        snprintf(line, sizeof(line), "Line %d - Testing scrolling functionality\n", i);
        terminal_gui_write_text(instance, line, strlen(line));
    }
    
    // Test scrolling operations
    if (terminal_gui_scroll_up(instance, 5) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    if (terminal_gui_scroll_down(instance, 3) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    if (terminal_gui_scroll_to_top(instance) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    if (terminal_gui_scroll_to_bottom(instance) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
cleanup:
    if (instance) {
        terminal_gui_destroy_instance(instance);
    }
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Scrolling", passed);
}

static void test_terminal_gui_selection_clipboard(void) {
    bool passed = true;
    
    terminal_gui_init();
    
    // Create instance with clipboard support
    terminal_gui_config_t config;
    terminal_gui_get_default_config(&config);
    config.enable_clipboard = true;
    
    terminal_gui_instance_t* instance = terminal_gui_create_instance(&config);
    if (!instance) {
        passed = false;
        goto cleanup;
    }
    
    // Add some text to select
    const char* test_text = "Selectable text for clipboard testing";
    terminal_gui_write_text(instance, test_text, strlen(test_text));
    
    // Test selection operations
    gui_point_t start = {0, 0};
    gui_point_t end = {10, 0};
    
    if (terminal_gui_start_selection(instance, start) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    if (terminal_gui_update_selection(instance, end) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    if (terminal_gui_end_selection(instance) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Test clipboard operations
    if (terminal_gui_copy_selection(instance) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    if (terminal_gui_paste_clipboard(instance) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
cleanup:
    if (instance) {
        terminal_gui_destroy_instance(instance);
    }
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Selection and Clipboard", passed);
}

static void test_terminal_gui_command_execution(void) {
    bool passed = true;
    
    terminal_gui_init();
    
    terminal_gui_instance_t* instance = terminal_gui_create_instance(NULL);
    if (!instance) {
        passed = false;
        goto cleanup;
    }
    
    // Test command execution
    const char* test_command = "ls -la";
    if (terminal_gui_run_command(instance, test_command) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
    // Test shell execution
    if (terminal_gui_execute_shell(instance) != TERMINAL_GUI_SUCCESS) {
        passed = false;
    }
    
cleanup:
    if (instance) {
        terminal_gui_destroy_instance(instance);
    }
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Command Execution", passed);
}

/* ================================
 * Utility Functions
 * ================================ */

static void print_test_result(const char* test_name, bool passed) {
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

static void print_test_header(const char* test_suite_name) {
    printf("\n=== %s ===\n", test_suite_name);
}

/* ================================
 * Basic Integration Test
 * ================================ */

void terminal_gui_test_basic_integration(void) {
    printf("Running basic Terminal GUI integration test...\n");
    
    // Initialize terminal GUI system
    if (terminal_gui_init() != TERMINAL_GUI_SUCCESS) {
        printf("Failed to initialize Terminal GUI system\n");
        return;
    }
    
    // Create a terminal instance
    terminal_gui_instance_t* terminal = terminal_gui_create_instance(NULL);
    if (!terminal) {
        printf("Failed to create terminal instance\n");
        terminal_gui_cleanup();
        return;
    }
    
    // Show the terminal window
    if (terminal_gui_show_window(terminal) != TERMINAL_GUI_SUCCESS) {
        printf("Failed to show terminal window\n");
    } else {
        printf("Terminal window shown successfully\n");
    }
    
    // Write some test text
    const char* welcome_text = "Welcome to IKOS Terminal GUI!\n";
    if (terminal_gui_write_text(terminal, welcome_text, strlen(welcome_text)) == TERMINAL_GUI_SUCCESS) {
        printf("Successfully wrote text to terminal\n");
    }
    
    // Execute shell
    terminal_gui_execute_shell(terminal);
    
    // Clean up
    terminal_gui_destroy_instance(terminal);
    terminal_gui_cleanup();
    
    printf("Basic Terminal GUI integration test completed\n");
}
