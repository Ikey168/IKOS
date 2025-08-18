/*
 * IKOS Operating System - GUI Test Application
 * Issue #37: Graphical User Interface Implementation
 *
 * Test application demonstrating GUI system capabilities.
 */

#include "gui.h"
#include "gui_internal.h"
#include "memory.h"
#include "string.h"

/* Test application state */
static gui_window_t* g_main_window = NULL;
static gui_widget_t* g_button1 = NULL;
static gui_widget_t* g_button2 = NULL;
static gui_widget_t* g_textbox = NULL;
static gui_widget_t* g_checkbox = NULL;
static gui_widget_t* g_listbox = NULL;
static gui_widget_t* g_progressbar = NULL;
static gui_widget_t* g_label = NULL;

/* Event handlers */
void test_button_handler(gui_event_t* event, void* user_data);
void test_window_handler(gui_event_t* event, void* user_data);
void test_textbox_handler(gui_event_t* event, void* user_data);

/* ================================
 * GUI Test Application
 * ================================ */

int gui_test_init(void) {
    /* Initialize GUI system */
    if (gui_init() != 0) {
        return -1;
    }
    
    /* Create main test window */
    gui_rect_t window_bounds = gui_rect_make(100, 100, 600, 400);
    g_main_window = gui_create_window("GUI Test Application", window_bounds, GUI_WINDOW_NORMAL);
    if (!g_main_window) {
        return -1;
    }
    
    /* Set window event handler */
    gui_set_window_event_handler(g_main_window, test_window_handler, NULL);
    
    /* Create test widgets */
    
    /* Test button 1 */
    gui_rect_t btn1_bounds = gui_rect_make(20, GUI_TITLE_BAR_HEIGHT + 20, 120, 30);
    g_button1 = gui_create_button(btn1_bounds, "Click Me!", g_main_window->root_widget);
    gui_set_event_handler(g_button1, test_button_handler, (void*)"button1");
    
    /* Test button 2 */
    gui_rect_t btn2_bounds = gui_rect_make(160, GUI_TITLE_BAR_HEIGHT + 20, 120, 30);
    g_button2 = gui_create_button(btn2_bounds, "Show Message", g_main_window->root_widget);
    gui_set_event_handler(g_button2, test_button_handler, (void*)"button2");
    
    /* Test label */
    gui_rect_t label_bounds = gui_rect_make(20, GUI_TITLE_BAR_HEIGHT + 60, 260, 20);
    g_label = gui_create_label(label_bounds, "Welcome to IKOS GUI System!", g_main_window->root_widget);
    
    /* Test textbox */
    gui_rect_t textbox_bounds = gui_rect_make(20, GUI_TITLE_BAR_HEIGHT + 90, 260, 25);
    g_textbox = gui_create_textbox(textbox_bounds, "Type here...", g_main_window->root_widget);
    gui_set_event_handler(g_textbox, test_textbox_handler, NULL);
    
    /* Test checkbox */
    gui_rect_t checkbox_bounds = gui_rect_make(20, GUI_TITLE_BAR_HEIGHT + 130, 150, 20);
    g_checkbox = gui_create_checkbox(checkbox_bounds, "Enable feature", false, g_main_window->root_widget);
    
    /* Test listbox */
    gui_rect_t listbox_bounds = gui_rect_make(300, GUI_TITLE_BAR_HEIGHT + 20, 180, 120);
    g_listbox = gui_create_listbox(listbox_bounds, g_main_window->root_widget);
    gui_listbox_add_item(g_listbox, "Item 1");
    gui_listbox_add_item(g_listbox, "Item 2");
    gui_listbox_add_item(g_listbox, "Item 3");
    gui_listbox_add_item(g_listbox, "Item 4");
    gui_listbox_add_item(g_listbox, "Item 5");
    
    /* Test progress bar */
    gui_rect_t progress_bounds = gui_rect_make(20, GUI_TITLE_BAR_HEIGHT + 160, 260, 20);
    g_progressbar = gui_create_progressbar(progress_bounds, 0, 100, g_main_window->root_widget);
    gui_progressbar_set_value(g_progressbar, 50);
    
    /* Show the window */
    gui_show_window(g_main_window, true);
    
    return 0;
}

void gui_test_shutdown(void) {
    if (g_main_window) {
        gui_destroy_window(g_main_window);
        g_main_window = NULL;
    }
    
    gui_shutdown();
}

int gui_test_run(void) {
    /* Run GUI test with simple event loop */
    gui_event_t event;
    int frames = 0;
    bool running = true;
    
    while (running) {
        /* Process events */
        while (gui_get_event(&event)) {
            /* Check for quit conditions */
            if (event.type == GUI_EVENT_WINDOW_CLOSE && event.target == g_main_window) {
                running = false;
                break;
            }
            
            /* Handle other events through widget handlers */
        }
        
        /* Update progress bar animation */
        if (g_progressbar && (frames % 60) == 0) { /* Update every 60 frames */
            int32_t current = gui_progressbar_get_value(g_progressbar);
            current += 5;
            if (current > 100) current = 0;
            gui_progressbar_set_value(g_progressbar, current);
        }
        
        /* Update and render */
        gui_update();
        gui_render();
        
        frames++;
        
        /* Simple frame limiting - in real OS this would be handled by scheduler */
        for (volatile int i = 0; i < 100000; i++) {
            /* Busy wait to slow down rendering */
        }
    }
    
    return 0;
}

/* ================================
 * Event Handlers
 * ================================ */

void test_button_handler(gui_event_t* event, void* user_data) {
    if (event->type != GUI_EVENT_MOUSE_CLICK) return;
    
    const char* button_id = (const char*)user_data;
    
    if (strcmp(button_id, "button1") == 0) {
        /* Button 1 clicked - update label */
        static int click_count = 0;
        click_count++;
        
        char buffer[64];
        sprintf(buffer, "Button clicked %d times!", click_count);
        gui_set_widget_text(g_label, buffer);
        
        /* Toggle checkbox */
        bool checked = gui_checkbox_is_checked(g_checkbox);
        gui_checkbox_set_checked(g_checkbox, !checked);
        
    } else if (strcmp(button_id, "button2") == 0) {
        /* Button 2 clicked - show message box */
        gui_show_message_box("Test Message", "This is a test message box from the GUI system!");
        
        /* Select next item in listbox */
        int32_t selected = gui_listbox_get_selected(g_listbox);
        selected = (selected + 1) % 5; /* Cycle through items */
        gui_listbox_set_selected(g_listbox, selected);
    }
}

void test_window_handler(gui_event_t* event, void* user_data) {
    (void)user_data; /* Unused */
    
    switch (event->type) {
        case GUI_EVENT_WINDOW_CLOSE:
            /* Window close requested - could show confirmation dialog */
            break;
            
        case GUI_EVENT_WINDOW_FOCUS:
            /* Window gained focus */
            gui_set_widget_text(g_label, "Window focused!");
            break;
            
        case GUI_EVENT_KEY_DOWN:
            /* Handle keyboard shortcuts */
            if (event->data.keyboard.keycode == 27) { /* ESC key */
                gui_post_event(event); /* Re-post as window close */
                gui_event_t close_event = *event;
                close_event.type = GUI_EVENT_WINDOW_CLOSE;
                gui_post_event(&close_event);
            }
            break;
            
        default:
            break;
    }
}

void test_textbox_handler(gui_event_t* event, void* user_data) {
    (void)user_data; /* Unused */
    
    if (event->type == GUI_EVENT_CHAR_INPUT) {
        /* Handle character input to textbox */
        const char* current_text = gui_get_widget_text(g_textbox);
        if (current_text) {
            size_t len = strlen(current_text);
            if (len < 50) { /* Limit text length */
                char* new_text = (char*)malloc(len + 2);
                if (new_text) {
                    strcpy(new_text, current_text);
                    new_text[len] = event->data.keyboard.character;
                    new_text[len + 1] = '\0';
                    gui_set_widget_text(g_textbox, new_text);
                    free(new_text);
                }
            }
        }
    } else if (event->type == GUI_EVENT_KEY_DOWN) {
        /* Handle special keys */
        if (event->data.keyboard.keycode == 8) { /* Backspace */
            const char* current_text = gui_get_widget_text(g_textbox);
            if (current_text && strlen(current_text) > 0) {
                size_t len = strlen(current_text);
                char* new_text = (char*)malloc(len);
                if (new_text) {
                    strncpy(new_text, current_text, len - 1);
                    new_text[len - 1] = '\0';
                    gui_set_widget_text(g_textbox, new_text);
                    free(new_text);
                }
            }
        }
    }
}

/* ================================
 * Advanced GUI Test Features
 * ================================ */

void gui_test_create_multiple_windows(void) {
    /* Create additional test windows to test window management */
    
    for (int i = 0; i < 3; i++) {
        char title[32];
        sprintf(title, "Test Window %d", i + 1);
        
        gui_rect_t bounds = gui_rect_make(150 + i * 50, 150 + i * 50, 300, 200);
        gui_window_t* window = gui_create_window(title, bounds, GUI_WINDOW_NORMAL);
        
        if (window) {
            /* Add a simple button to each window */
            gui_rect_t btn_bounds = gui_rect_make(20, GUI_TITLE_BAR_HEIGHT + 20, 100, 30);
            gui_widget_t* button = gui_create_button(btn_bounds, "Close", window->root_widget);
            
            /* Add a label showing window number */
            gui_rect_t label_bounds = gui_rect_make(20, GUI_TITLE_BAR_HEIGHT + 60, 200, 20);
            char label_text[32];
            sprintf(label_text, "This is window #%d", i + 1);
            gui_widget_t* label = gui_create_label(label_bounds, label_text, window->root_widget);
            
            gui_show_window(window, true);
        }
    }
}

void gui_test_window_operations(void) {
    /* Test various window operations */
    
    if (g_main_window) {
        /* Test window state changes */
        gui_set_window_state(g_main_window, GUI_WINDOW_STATE_MAXIMIZED);
        
        /* Wait a bit */
        for (volatile int i = 0; i < 1000000; i++);
        
        gui_set_window_state(g_main_window, GUI_WINDOW_STATE_NORMAL);
        
        /* Test window movement */
        gui_move_window(g_main_window, gui_point_make(200, 200));
        
        /* Test window resizing */
        gui_resize_window(g_main_window, gui_size_make(500, 300));
    }
}

void gui_test_widget_operations(void) {
    /* Test dynamic widget creation and destruction */
    
    if (g_main_window) {
        /* Create temporary widgets */
        gui_rect_t temp_bounds = gui_rect_make(350, GUI_TITLE_BAR_HEIGHT + 200, 120, 30);
        gui_widget_t* temp_button = gui_create_button(temp_bounds, "Temporary", g_main_window->root_widget);
        
        /* Show it briefly */
        gui_render();
        for (volatile int i = 0; i < 2000000; i++);
        
        /* Remove it */
        gui_destroy_widget(temp_button);
    }
}

/* ================================
 * Performance Testing
 * ================================ */

void gui_test_performance(void) {
    /* Test GUI performance with many widgets */
    
    gui_rect_t perf_window_bounds = gui_rect_make(50, 50, 700, 500);
    gui_window_t* perf_window = gui_create_window("Performance Test", perf_window_bounds, GUI_WINDOW_NORMAL);
    
    if (perf_window) {
        /* Create many small widgets */
        for (int y = 0; y < 10; y++) {
            for (int x = 0; x < 15; x++) {
                gui_rect_t widget_bounds = gui_rect_make(
                    10 + x * 45, 
                    GUI_TITLE_BAR_HEIGHT + 10 + y * 35, 
                    40, 
                    30
                );
                
                char button_text[8];
                sprintf(button_text, "%d,%d", x, y);
                
                gui_widget_t* widget = gui_create_button(widget_bounds, button_text, perf_window->root_widget);
            }
        }
        
        gui_show_window(perf_window, true);
        
        /* Render multiple times to test performance */
        uint64_t start_frames = 0, start_events = 0;
        uint32_t start_windows = 0, start_widgets = 0;
        gui_get_statistics(&start_frames, &start_events, &start_windows, &start_widgets);
        
        for (int i = 0; i < 100; i++) {
            gui_update();
            gui_render();
        }
        
        uint64_t end_frames = 0, end_events = 0;
        uint32_t end_windows = 0, end_widgets = 0;
        gui_get_statistics(&end_frames, &end_events, &end_windows, &end_widgets);
        
        /* Update performance window title with results */
        char perf_title[64];
        sprintf(perf_title, "Performance: %llu frames, %u widgets", 
                end_frames - start_frames, end_widgets);
        gui_set_window_title(perf_window, perf_title);
        
        /* Keep window open for a while */
        for (volatile int i = 0; i < 3000000; i++);
        
        gui_destroy_window(perf_window);
    }
}

/* ================================
 * Main Test Entry Point
 * ================================ */

int gui_run_tests(void) {
    /* Initialize GUI test */
    if (gui_test_init() != 0) {
        return -1;
    }
    
    /* Run basic test */
    gui_test_run();
    
    /* Cleanup */
    gui_test_shutdown();
    
    return 0;
}

/* Simple test function that can be called from kernel */
void gui_simple_test(void) {
    if (gui_init() == 0) {
        /* Create a simple window */
        gui_rect_t bounds = gui_rect_make(200, 150, 400, 300);
        gui_window_t* window = gui_create_window("IKOS GUI Test", bounds, GUI_WINDOW_NORMAL);
        
        if (window) {
            /* Add a label */
            gui_rect_t label_bounds = gui_rect_make(20, GUI_TITLE_BAR_HEIGHT + 20, 360, 20);
            gui_widget_t* label = gui_create_label(label_bounds, "GUI System Initialized Successfully!", window->root_widget);
            
            /* Add a button */
            gui_rect_t btn_bounds = gui_rect_make(150, GUI_TITLE_BAR_HEIGHT + 60, 100, 30);
            gui_widget_t* button = gui_create_button(btn_bounds, "OK", window->root_widget);
            
            gui_show_window(window, true);
            
            /* Render a few frames */
            for (int i = 0; i < 10; i++) {
                gui_update();
                gui_render();
                
                /* Simple delay */
                for (volatile int j = 0; j < 500000; j++);
            }
            
            gui_destroy_window(window);
        }
        
        gui_shutdown();
    }
}
