/*
 * IKOS Terminal GUI Integration - Additional Test Suite
 * Issue #43 - Terminal Emulator GUI Integration
 * 
 * Additional tests for terminal GUI integration functionality.
 */

#include "../include/terminal_gui.h"
#include "stdio.h"
#include "string.h"
#include <stdint.h>
#include <stdbool.h>

/* ================================
 * Test Utility Functions
 * ================================ */

static void print_test_result(const char* test_name, bool passed) {
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

static void print_test_header(const char* test_suite_name) {
    printf("\n=== %s ===\n", test_suite_name);
}

/* ================================
 * Integration Test Functions
 * ================================ */

void test_terminal_gui_integration_basic(void) {
    print_test_header("Terminal GUI Basic Integration Test");
    
    bool passed = true;
    
    // Test system initialization
    if (terminal_gui_init() != TERMINAL_GUI_SUCCESS) {
        passed = false;
        printf("Failed to initialize terminal GUI system\n");
    }
    
    // Create terminal instance
    terminal_gui_instance_t* terminal = terminal_gui_create_instance(NULL);
    if (!terminal) {
        passed = false;
        printf("Failed to create terminal instance\n");
    }
    
    if (terminal && passed) {
        // Test basic operations
        if (terminal_gui_write_text(terminal, "Test output\n", 12) != TERMINAL_GUI_SUCCESS) {
            passed = false;
        }
        
        if (terminal_gui_show_window(terminal) != TERMINAL_GUI_SUCCESS) {
            passed = false;
        }
        
        // Clean up
        terminal_gui_destroy_instance(terminal);
    }
    
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Basic Integration", passed);
}

void test_terminal_gui_multiple_windows(void) {
    print_test_header("Terminal GUI Multiple Windows Test");
    
    bool passed = true;
    
    terminal_gui_init();
    
    // Create multiple terminal instances
    terminal_gui_instance_t* term1 = terminal_gui_create_instance(NULL);
    terminal_gui_instance_t* term2 = terminal_gui_create_instance(NULL);
    terminal_gui_instance_t* term3 = terminal_gui_create_instance(NULL);
    
    if (!term1 || !term2 || !term3) {
        passed = false;
        printf("Failed to create multiple terminal instances\n");
    }
    
    if (passed) {
        // Set different titles
        terminal_gui_set_window_title(term1, "Terminal 1");
        terminal_gui_set_window_title(term2, "Terminal 2");
        terminal_gui_set_window_title(term3, "Terminal 3");
        
        // Show all windows
        terminal_gui_show_window(term1);
        terminal_gui_show_window(term2);
        terminal_gui_show_window(term3);
        
        // Write different content to each
        terminal_gui_write_text(term1, "This is Terminal 1\n", 19);
        terminal_gui_write_text(term2, "This is Terminal 2\n", 19);
        terminal_gui_write_text(term3, "This is Terminal 3\n", 19);
    }
    
    // Clean up
    if (term1) terminal_gui_destroy_instance(term1);
    if (term2) terminal_gui_destroy_instance(term2);
    if (term3) terminal_gui_destroy_instance(term3);
    
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Multiple Windows", passed);
}

void test_terminal_gui_configuration(void) {
    print_test_header("Terminal GUI Configuration Test");
    
    bool passed = true;
    
    terminal_gui_init();
    
    // Test custom configuration
    terminal_gui_config_t config;
    if (terminal_gui_get_default_config(&config) != TERMINAL_GUI_SUCCESS) {
        passed = false;
        printf("Failed to get default configuration\n");
    }
    
    if (passed) {
        // Modify configuration
        config.bg_color = GUI_COLOR_BLUE;
        config.fg_color = GUI_COLOR_YELLOW;
        config.enable_tabs = true;
        config.show_scrollbar = true;
        
        // Create terminal with custom config
        terminal_gui_instance_t* terminal = terminal_gui_create_instance(&config);
        if (!terminal) {
            passed = false;
            printf("Failed to create terminal with custom config\n");
        }
        
        if (terminal) {
            // Verify configuration was applied
            if (terminal->config.bg_color != GUI_COLOR_BLUE ||
                terminal->config.fg_color != GUI_COLOR_YELLOW ||
                !terminal->config.enable_tabs ||
                !terminal->config.show_scrollbar) {
                passed = false;
                printf("Custom configuration not applied correctly\n");
            }
            
            terminal_gui_destroy_instance(terminal);
        }
    }
    
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Configuration", passed);
}

void test_terminal_gui_tab_functionality(void) {
    print_test_header("Terminal GUI Tab Functionality Test");
    
    bool passed = true;
    
    terminal_gui_init();
    
    // Create terminal with tab support
    terminal_gui_config_t config;
    terminal_gui_get_default_config(&config);
    config.enable_tabs = true;
    
    terminal_gui_instance_t* terminal = terminal_gui_create_instance(&config);
    if (!terminal) {
        passed = false;
        printf("Failed to create terminal with tab support\n");
    }
    
    if (terminal && passed) {
        // Add tabs
        if (terminal_gui_add_tab(terminal, "Tab A") != TERMINAL_GUI_SUCCESS ||
            terminal_gui_add_tab(terminal, "Tab B") != TERMINAL_GUI_SUCCESS ||
            terminal_gui_add_tab(terminal, "Tab C") != TERMINAL_GUI_SUCCESS) {
            passed = false;
            printf("Failed to add tabs\n");
        }
        
        // Test tab switching
        if (passed) {
            if (terminal_gui_switch_tab(terminal, 1) != TERMINAL_GUI_SUCCESS ||
                terminal_gui_switch_tab(terminal, 2) != TERMINAL_GUI_SUCCESS ||
                terminal_gui_switch_tab(terminal, 0) != TERMINAL_GUI_SUCCESS) {
                passed = false;
                printf("Failed to switch tabs\n");
            }
        }
        
        // Test tab removal
        if (passed) {
            if (terminal_gui_remove_tab(terminal, 1) != TERMINAL_GUI_SUCCESS) {
                passed = false;
                printf("Failed to remove tab\n");
            }
        }
        
        terminal_gui_destroy_instance(terminal);
    }
    
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Tab Functionality", passed);
}

void test_terminal_gui_scrolling_functionality(void) {
    print_test_header("Terminal GUI Scrolling Test");
    
    bool passed = true;
    
    terminal_gui_init();
    
    terminal_gui_instance_t* terminal = terminal_gui_create_instance(NULL);
    if (!terminal) {
        passed = false;
        printf("Failed to create terminal\n");
    }
    
    if (terminal && passed) {
        // Fill terminal with enough content to enable scrolling
        for (int i = 0; i < 50; i++) {
            char line[64];
            snprintf(line, sizeof(line), "Line %d - Terminal scrolling test content\n", i + 1);
            if (terminal_gui_write_text(terminal, line, strlen(line)) != TERMINAL_GUI_SUCCESS) {
                passed = false;
                printf("Failed to write test content\n");
                break;
            }
        }
        
        if (passed) {
            // Test scrolling operations
            if (terminal_gui_scroll_up(terminal, 10) != TERMINAL_GUI_SUCCESS ||
                terminal_gui_scroll_down(terminal, 5) != TERMINAL_GUI_SUCCESS ||
                terminal_gui_scroll_to_top(terminal) != TERMINAL_GUI_SUCCESS ||
                terminal_gui_scroll_to_bottom(terminal) != TERMINAL_GUI_SUCCESS) {
                passed = false;
                printf("Failed scrolling operations\n");
            }
        }
        
        terminal_gui_destroy_instance(terminal);
    }
    
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Scrolling", passed);
}

void test_terminal_gui_command_execution(void) {
    print_test_header("Terminal GUI Command Execution Test");
    
    bool passed = true;
    
    terminal_gui_init();
    
    terminal_gui_instance_t* terminal = terminal_gui_create_instance(NULL);
    if (!terminal) {
        passed = false;
        printf("Failed to create terminal\n");
    }
    
    if (terminal && passed) {
        // Test command execution
        const char* test_commands[] = {
            "help",
            "ls",
            "echo test",
            "pwd"
        };
        
        for (size_t i = 0; i < sizeof(test_commands) / sizeof(test_commands[0]); i++) {
            if (terminal_gui_run_command(terminal, test_commands[i]) != TERMINAL_GUI_SUCCESS) {
                passed = false;
                printf("Failed to execute command: %s\n", test_commands[i]);
                break;
            }
        }
        
        // Test shell execution
        if (passed) {
            if (terminal_gui_execute_shell(terminal) != TERMINAL_GUI_SUCCESS) {
                passed = false;
                printf("Failed to execute shell\n");
            }
        }
        
        terminal_gui_destroy_instance(terminal);
    }
    
    terminal_gui_cleanup();
    print_test_result("Terminal GUI Command Execution", passed);
}

/* ================================
 * Main Test Suite Runner
 * ================================ */

void test_terminal_gui_comprehensive_suite(void) {
    printf("\n======= Terminal GUI Comprehensive Test Suite =======\n");
    
    test_terminal_gui_integration_basic();
    test_terminal_gui_multiple_windows();
    test_terminal_gui_configuration();
    test_terminal_gui_tab_functionality();
    test_terminal_gui_scrolling_functionality();
    test_terminal_gui_command_execution();
    
    printf("\nTerminal GUI comprehensive test suite completed.\n");
}

/* ================================
 * Demo Functions
 * ================================ */

void terminal_gui_demo_basic(void) {
    printf("Starting Terminal GUI Basic Demo...\n");
    
    // Initialize system
    if (terminal_gui_init() != TERMINAL_GUI_SUCCESS) {
        printf("Failed to initialize Terminal GUI system\n");
        return;
    }
    
    // Create demo terminal
    terminal_gui_instance_t* demo_terminal = terminal_gui_create_instance(NULL);
    if (!demo_terminal) {
        printf("Failed to create demo terminal\n");
        terminal_gui_cleanup();
        return;
    }
    
    // Set demo title
    terminal_gui_set_window_title(demo_terminal, "IKOS Terminal Demo");
    
    // Show window
    terminal_gui_show_window(demo_terminal);
    
    // Write demo content
    terminal_gui_write_text(demo_terminal, "Welcome to IKOS Terminal GUI!\n", 31);
    terminal_gui_write_text(demo_terminal, "=================================\n", 34);
    terminal_gui_write_text(demo_terminal, "This is a demonstration of the GUI-integrated terminal.\n", 57);
    terminal_gui_write_text(demo_terminal, "Features:\n", 10);
    terminal_gui_write_text(demo_terminal, "- Multiple terminal instances\n", 30);
    terminal_gui_write_text(demo_terminal, "- Tab support\n", 14);
    terminal_gui_write_text(demo_terminal, "- Scrolling and selection\n", 26);
    terminal_gui_write_text(demo_terminal, "- Command execution\n", 20);
    terminal_gui_write_text(demo_terminal, "- Full GUI integration\n", 23);
    terminal_gui_write_text(demo_terminal, "\nType commands or press ESC to exit.\n", 37);
    
    // Start shell
    terminal_gui_execute_shell(demo_terminal);
    
    printf("Terminal GUI demo window created and displayed\n");
    
    // Note: In a real implementation, we would wait for user interaction
    // For this demo, we just clean up after a short time
    
    // Clean up
    terminal_gui_destroy_instance(demo_terminal);
    terminal_gui_cleanup();
    
    printf("Terminal GUI Basic Demo completed\n");
}

void terminal_gui_demo_multiple_terminals(void) {
    printf("Starting Terminal GUI Multiple Terminals Demo...\n");
    
    terminal_gui_init();
    
    // Create multiple terminals with different configurations
    terminal_gui_config_t config1, config2, config3;
    
    // Terminal 1: Default configuration
    terminal_gui_get_default_config(&config1);
    
    // Terminal 2: Blue theme with tabs
    terminal_gui_get_default_config(&config2);
    config2.bg_color = GUI_COLOR_BLUE;
    config2.fg_color = GUI_COLOR_WHITE;
    config2.enable_tabs = true;
    
    // Terminal 3: Green theme with custom font
    terminal_gui_get_default_config(&config3);
    config3.bg_color = GUI_COLOR_BLACK;
    config3.fg_color = GUI_COLOR_GREEN;
    config3.font_size = 14;
    
    // Create terminals
    terminal_gui_instance_t* term1 = terminal_gui_create_instance(&config1);
    terminal_gui_instance_t* term2 = terminal_gui_create_instance(&config2);
    terminal_gui_instance_t* term3 = terminal_gui_create_instance(&config3);
    
    if (term1 && term2 && term3) {
        // Configure terminals
        terminal_gui_set_window_title(term1, "System Terminal");
        terminal_gui_set_window_title(term2, "Development Terminal");
        terminal_gui_set_window_title(term3, "Monitoring Terminal");
        
        // Show all terminals
        terminal_gui_show_window(term1);
        terminal_gui_show_window(term2);
        terminal_gui_show_window(term3);
        
        // Add content to each terminal
        terminal_gui_write_text(term1, "System Terminal Ready\n> ", 24);
        terminal_gui_write_text(term2, "Development Environment\n> ", 27);
        terminal_gui_write_text(term3, "System Monitor\n> ", 18);
        
        printf("Created 3 terminal instances with different themes\n");
    } else {
        printf("Failed to create multiple terminals\n");
    }
    
    // Clean up
    if (term1) terminal_gui_destroy_instance(term1);
    if (term2) terminal_gui_destroy_instance(term2);
    if (term3) terminal_gui_destroy_instance(term3);
    
    terminal_gui_cleanup();
    
    printf("Multiple Terminals Demo completed\n");
}
