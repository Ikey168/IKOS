/*
 * IKOS Terminal GUI Integration - Core Implementation
 * Issue #43 - Terminal Emulator GUI Integration
 * 
 * Integrates the CLI terminal into the GUI environment with support for
 * multiple terminal instances, tabs/windows, and seamless CLI-GUI interaction.
 */

#include "terminal_gui.h"
#include "memory.h"
#include "stdio.h"
#include "string.h"
#include "gui_widgets.h"
#include "gui_render.h"
#include <stddef.h>
#include <stdlib.h>

/* ================================
 * Global Terminal GUI Manager
 * ================================ */

static terminal_gui_manager_t g_terminal_gui_manager = {0};

/* ================================
 * Static Function Declarations
 * ================================ */

static void terminal_gui_init_default_config(terminal_gui_config_t* config);
static int terminal_gui_create_window(terminal_gui_instance_t* instance);
static int terminal_gui_create_widgets(terminal_gui_instance_t* instance);
static void terminal_gui_calculate_layout(terminal_gui_instance_t* instance);
static int terminal_gui_update_terminal_size(terminal_gui_instance_t* instance);
static void terminal_gui_render_background(terminal_gui_instance_t* instance);
static void terminal_gui_render_text_content(terminal_gui_instance_t* instance);
static int terminal_gui_process_key_input(terminal_gui_instance_t* instance, uint32_t key, bool shift, bool ctrl, bool alt);
static int terminal_gui_handle_mouse_click(terminal_gui_instance_t* instance, gui_point_t position, gui_mouse_button_t button);
static void terminal_gui_blink_cursor(terminal_gui_instance_t* instance);
static int terminal_gui_allocate_clipboard(uint32_t size);
static void terminal_gui_free_clipboard(void);

/* GUI event callbacks */
static void terminal_gui_window_paint_callback(gui_window_t* window, gui_event_t* event);
static void terminal_gui_window_key_callback(gui_window_t* window, gui_event_t* event);
static void terminal_gui_window_mouse_callback(gui_window_t* window, gui_event_t* event);
static void terminal_gui_window_resize_callback(gui_window_t* window, gui_event_t* event);
static void terminal_gui_window_close_callback(gui_window_t* window, gui_event_t* event);

/* ================================
 * Core Terminal GUI Functions
 * ================================ */

int terminal_gui_init(void) {
    if (g_terminal_gui_manager.initialized) {
        return TERMINAL_GUI_SUCCESS;
    }
    
    // Clear manager structure
    memset(&g_terminal_gui_manager, 0, sizeof(terminal_gui_manager_t));
    
    // Initialize default configuration
    terminal_gui_init_default_config(&g_terminal_gui_manager.default_config);
    
    // Set global settings
    g_terminal_gui_manager.global_mode = TERMINAL_GUI_MODE_WINDOW;
    g_terminal_gui_manager.enable_multi_instance = true;
    g_terminal_gui_manager.next_instance_id = 1;
    
    // Mark as initialized
    g_terminal_gui_manager.initialized = true;
    
    // Register with GUI system
    if (terminal_gui_register_with_gui_system() != TERMINAL_GUI_SUCCESS) {
        return TERMINAL_GUI_ERROR_GUI_ERROR;
    }
    
    return TERMINAL_GUI_SUCCESS;
}

void terminal_gui_cleanup(void) {
    if (!g_terminal_gui_manager.initialized) {
        return;
    }
    
    // Destroy all instances
    for (uint32_t i = 0; i < TERMINAL_GUI_MAX_INSTANCES; i++) {
        if (g_terminal_gui_manager.instances[i].active) {
            terminal_gui_destroy_instance(&g_terminal_gui_manager.instances[i]);
        }
    }
    
    // Free clipboard
    terminal_gui_free_clipboard();
    
    // Clear manager
    memset(&g_terminal_gui_manager, 0, sizeof(terminal_gui_manager_t));
}

terminal_gui_instance_t* terminal_gui_create_instance(const terminal_gui_config_t* config) {
    if (!g_terminal_gui_manager.initialized) {
        return NULL;
    }
    
    // Find free instance slot
    terminal_gui_instance_t* instance = NULL;
    for (uint32_t i = 0; i < TERMINAL_GUI_MAX_INSTANCES; i++) {
        if (!g_terminal_gui_manager.instances[i].active) {
            instance = &g_terminal_gui_manager.instances[i];
            break;
        }
    }
    
    if (!instance) {
        return NULL; // No free slots
    }
    
    // Clear instance
    memset(instance, 0, sizeof(terminal_gui_instance_t));
    
    // Set configuration
    if (config) {
        instance->config = *config;
    } else {
        instance->config = g_terminal_gui_manager.default_config;
    }
    
    // Initialize instance
    instance->id = g_terminal_gui_manager.next_instance_id++;
    instance->active = true;
    instance->state = TERMINAL_GUI_STATE_INACTIVE;
    snprintf(instance->title, sizeof(instance->title), "Terminal %u", instance->id);
    
    // Calculate initial layout
    instance->visible_cols = (TERMINAL_GUI_DEFAULT_WIDTH - 2 * TERMINAL_GUI_PADDING) / instance->config.char_width;
    instance->visible_rows = (TERMINAL_GUI_DEFAULT_HEIGHT - TERMINAL_GUI_TAB_HEIGHT - 2 * TERMINAL_GUI_PADDING) / instance->config.char_height;
    
    // Initialize terminal
    if (terminal_init(&instance->terminal, instance->visible_cols, instance->visible_rows) != TERMINAL_SUCCESS) {
        instance->active = false;
        return NULL;
    }
    
    // Create GUI window and widgets
    if (terminal_gui_create_window(instance) != TERMINAL_GUI_SUCCESS) {
        terminal_destroy(&instance->terminal);
        instance->active = false;
        return NULL;
    }
    
    // Initialize cursor
    instance->cursor_visible = true;
    instance->blink_timer = 0;
    
    // Update manager
    g_terminal_gui_manager.instance_count++;
    
    return instance;
}

int terminal_gui_destroy_instance(terminal_gui_instance_t* instance) {
    if (!instance || !instance->active) {
        return TERMINAL_GUI_ERROR_INSTANCE_NOT_FOUND;
    }
    
    // Close GUI window
    if (instance->window) {
        gui_destroy_window(instance->window);
        instance->window = NULL;
    }
    
    // Destroy terminal
    terminal_destroy(&instance->terminal);
    
    // Clear instance
    memset(instance, 0, sizeof(terminal_gui_instance_t));
    
    // Update manager
    g_terminal_gui_manager.instance_count--;
    if (g_terminal_gui_manager.focused_instance == instance) {
        g_terminal_gui_manager.focused_instance = NULL;
    }
    
    return TERMINAL_GUI_SUCCESS;
}

terminal_gui_instance_t* terminal_gui_get_instance(uint32_t id) {
    for (uint32_t i = 0; i < TERMINAL_GUI_MAX_INSTANCES; i++) {
        if (g_terminal_gui_manager.instances[i].active && 
            g_terminal_gui_manager.instances[i].id == id) {
            return &g_terminal_gui_manager.instances[i];
        }
    }
    return NULL;
}

terminal_gui_instance_t* terminal_gui_get_focused_instance(void) {
    return g_terminal_gui_manager.focused_instance;
}

/* ================================
 * Window and Tab Management
 * ================================ */

int terminal_gui_show_window(terminal_gui_instance_t* instance) {
    if (!instance || !instance->active || !instance->window) {
        return TERMINAL_GUI_ERROR_INVALID_PARAM;
    }
    
    if (gui_show_window(instance->window) != GUI_SUCCESS) {
        return TERMINAL_GUI_ERROR_GUI_ERROR;
    }
    
    instance->state = TERMINAL_GUI_STATE_ACTIVE;
    instance->needs_redraw = true;
    
    return TERMINAL_GUI_SUCCESS;
}

int terminal_gui_hide_window(terminal_gui_instance_t* instance) {
    if (!instance || !instance->active || !instance->window) {
        return TERMINAL_GUI_ERROR_INVALID_PARAM;
    }
    
    if (gui_hide_window(instance->window) != GUI_SUCCESS) {
        return TERMINAL_GUI_ERROR_GUI_ERROR;
    }
    
    instance->state = TERMINAL_GUI_STATE_INACTIVE;
    
    return TERMINAL_GUI_SUCCESS;
}

int terminal_gui_set_window_title(terminal_gui_instance_t* instance, const char* title) {
    if (!instance || !instance->active || !title) {
        return TERMINAL_GUI_ERROR_INVALID_PARAM;
    }
    
    strncpy(instance->title, title, sizeof(instance->title) - 1);
    instance->title[sizeof(instance->title) - 1] = '\0';
    
    if (instance->window) {
        gui_set_window_title(instance->window, instance->title);
    }
    
    return TERMINAL_GUI_SUCCESS;
}

/* ================================
 * Terminal Operations
 * ================================ */

int terminal_gui_write_text(terminal_gui_instance_t* instance, const char* text, uint32_t length) {
    if (!instance || !instance->active || !text) {
        return TERMINAL_GUI_ERROR_INVALID_PARAM;
    }
    
    // Write to terminal
    for (uint32_t i = 0; i < length; i++) {
        if (terminal_write_char(&instance->terminal, text[i]) != TERMINAL_SUCCESS) {
            return TERMINAL_GUI_ERROR_TERMINAL_ERROR;
        }
    }
    
    // Mark for redraw
    instance->needs_redraw = true;
    
    return TERMINAL_GUI_SUCCESS;
}

int terminal_gui_write_char(terminal_gui_instance_t* instance, char c) {
    return terminal_gui_write_text(instance, &c, 1);
}

int terminal_gui_clear_screen(terminal_gui_instance_t* instance) {
    if (!instance || !instance->active) {
        return TERMINAL_GUI_ERROR_INVALID_PARAM;
    }
    
    if (terminal_clear_screen(&instance->terminal) != TERMINAL_SUCCESS) {
        return TERMINAL_GUI_ERROR_TERMINAL_ERROR;
    }
    
    instance->needs_redraw = true;
    
    return TERMINAL_GUI_SUCCESS;
}

/* ================================
 * Rendering Functions
 * ================================ */

int terminal_gui_render(terminal_gui_instance_t* instance) {
    if (!instance || !instance->active || !instance->window) {
        return TERMINAL_GUI_ERROR_INVALID_PARAM;
    }
    
    if (!instance->needs_redraw) {
        return TERMINAL_GUI_SUCCESS;
    }
    
    // Render background
    terminal_gui_render_background(instance);
    
    // Render text content
    terminal_gui_render_text_content(instance);
    
    // Render cursor
    if (instance->cursor_visible) {
        terminal_gui_render_cursor(instance);
    }
    
    // Render selection if active
    if (instance->selection.active) {
        terminal_gui_render_selection(instance);
    }
    
    // Render scrollbar if visible
    if (instance->scrollbar.visible) {
        terminal_gui_render_scrollbar(instance);
    }
    
    // Render tabs if enabled
    if (instance->has_tabs) {
        terminal_gui_render_tabs(instance);
    }
    
    instance->needs_redraw = false;
    
    return TERMINAL_GUI_SUCCESS;
}

int terminal_gui_render_character(terminal_gui_instance_t* instance, uint32_t x, uint32_t y, char c, gui_color_t fg, gui_color_t bg) {
    if (!instance || !instance->window) {
        return TERMINAL_GUI_ERROR_INVALID_PARAM;
    }
    
    // Calculate pixel position
    gui_point_t pixel_pos = terminal_gui_char_to_pixel(instance, (gui_point_t){x, y});
    
    // Render background
    gui_rect_t char_rect = {
        pixel_pos.x, pixel_pos.y,
        instance->config.char_width, instance->config.char_height
    };
    gui_fill_rect(instance->window->framebuffer, &char_rect, bg);
    
    // Render character
    gui_draw_char(instance->window->framebuffer, pixel_pos.x, pixel_pos.y, c, fg, instance->config.font_name);
    
    return TERMINAL_GUI_SUCCESS;
}

/* ================================
 * Event Handling
 * ================================ */

int terminal_gui_handle_key_event(terminal_gui_instance_t* instance, gui_event_t* event) {
    if (!instance || !instance->active || !event) {
        return TERMINAL_GUI_ERROR_INVALID_PARAM;
    }
    
    if (event->type == GUI_EVENT_KEY_DOWN) {
        uint32_t key = event->data.key.keycode;
        bool shift = (event->data.key.modifiers & GUI_MODIFIER_SHIFT) != 0;
        bool ctrl = (event->data.key.modifiers & GUI_MODIFIER_CTRL) != 0;
        bool alt = (event->data.key.modifiers & GUI_MODIFIER_ALT) != 0;
        
        return terminal_gui_process_key_input(instance, key, shift, ctrl, alt);
    }
    
    return TERMINAL_GUI_SUCCESS;
}

int terminal_gui_handle_mouse_event(terminal_gui_instance_t* instance, gui_event_t* event) {
    if (!instance || !instance->active || !event) {
        return TERMINAL_GUI_ERROR_INVALID_PARAM;
    }
    
    gui_point_t position = event->data.mouse.position;
    
    switch (event->type) {
        case GUI_EVENT_MOUSE_DOWN:
            return terminal_gui_handle_mouse_click(instance, position, event->data.mouse.button);
            
        case GUI_EVENT_MOUSE_MOVE:
            if (instance->selection.active) {
                terminal_gui_update_selection(instance, position);
            }
            break;
            
        case GUI_EVENT_MOUSE_UP:
            if (instance->selection.active) {
                terminal_gui_end_selection(instance);
            }
            break;
            
        default:
            break;
    }
    
    return TERMINAL_GUI_SUCCESS;
}

/* ================================
 * Utility Functions
 * ================================ */

gui_point_t terminal_gui_pixel_to_char(terminal_gui_instance_t* instance, gui_point_t pixel) {
    gui_point_t char_pos;
    char_pos.x = (pixel.x - instance->terminal_rect.x) / instance->config.char_width;
    char_pos.y = (pixel.y - instance->terminal_rect.y) / instance->config.char_height;
    return char_pos;
}

gui_point_t terminal_gui_char_to_pixel(terminal_gui_instance_t* instance, gui_point_t character) {
    gui_point_t pixel_pos;
    pixel_pos.x = instance->terminal_rect.x + character.x * instance->config.char_width;
    pixel_pos.y = instance->terminal_rect.y + character.y * instance->config.char_height;
    return pixel_pos;
}

int terminal_gui_get_default_config(terminal_gui_config_t* config) {
    if (!config) {
        return TERMINAL_GUI_ERROR_INVALID_PARAM;
    }
    
    terminal_gui_init_default_config(config);
    return TERMINAL_GUI_SUCCESS;
}

/* ================================
 * Command Line Interface
 * ================================ */

int terminal_gui_run_command(terminal_gui_instance_t* instance, const char* command) {
    if (!instance || !instance->active || !command) {
        return TERMINAL_GUI_ERROR_INVALID_PARAM;
    }
    
    // Write command to terminal
    terminal_gui_write_text(instance, command, strlen(command));
    terminal_gui_write_char(instance, '\n');
    
    // TODO: Integrate with actual command execution system
    // For now, just echo the command
    terminal_gui_write_text(instance, "Command executed: ", 18);
    terminal_gui_write_text(instance, command, strlen(command));
    terminal_gui_write_char(instance, '\n');
    
    return TERMINAL_GUI_SUCCESS;
}

int terminal_gui_execute_shell(terminal_gui_instance_t* instance) {
    if (!instance || !instance->active) {
        return TERMINAL_GUI_ERROR_INVALID_PARAM;
    }
    
    // Initialize shell prompt
    terminal_gui_write_text(instance, "IKOS Shell > ", 13);
    
    return TERMINAL_GUI_SUCCESS;
}

/* ================================
 * Static Helper Functions
 * ================================ */

static void terminal_gui_init_default_config(terminal_gui_config_t* config) {
    config->mode = TERMINAL_GUI_MODE_WINDOW;
    config->bg_color = TERMINAL_GUI_BG_COLOR;
    config->fg_color = TERMINAL_GUI_FG_COLOR;
    config->cursor_color = TERMINAL_GUI_CURSOR_COLOR;
    config->selection_color = TERMINAL_GUI_SELECTION_COLOR;
    config->char_width = TERMINAL_GUI_CHAR_WIDTH;
    config->char_height = TERMINAL_GUI_CHAR_HEIGHT;
    config->show_scrollbar = true;
    config->enable_tabs = false;
    config->enable_mouse = true;
    config->enable_clipboard = true;
    strncpy(config->font_name, "default", sizeof(config->font_name));
    config->font_size = 12;
    config->on_char_input = NULL;
    config->on_resize = NULL;
    config->on_close = NULL;
    config->on_focus = NULL;
}

static int terminal_gui_create_window(terminal_gui_instance_t* instance) {
    gui_window_config_t window_config = {0};
    window_config.title = instance->title;
    window_config.x = 100 + (instance->id - 1) * 50;
    window_config.y = 100 + (instance->id - 1) * 50;
    window_config.width = TERMINAL_GUI_DEFAULT_WIDTH;
    window_config.height = TERMINAL_GUI_DEFAULT_HEIGHT;
    window_config.resizable = true;
    window_config.user_data = instance;
    
    instance->window = gui_create_window(&window_config);
    if (!instance->window) {
        return TERMINAL_GUI_ERROR_GUI_ERROR;
    }
    
    // Set event callbacks
    gui_set_window_paint_callback(instance->window, terminal_gui_window_paint_callback);
    gui_set_window_key_callback(instance->window, terminal_gui_window_key_callback);
    gui_set_window_mouse_callback(instance->window, terminal_gui_window_mouse_callback);
    gui_set_window_resize_callback(instance->window, terminal_gui_window_resize_callback);
    gui_set_window_close_callback(instance->window, terminal_gui_window_close_callback);
    
    return terminal_gui_create_widgets(instance);
}

static int terminal_gui_create_widgets(terminal_gui_instance_t* instance) {
    // Calculate layout
    terminal_gui_calculate_layout(instance);
    
    // Create canvas widget for terminal content
    instance->canvas = gui_create_widget(GUI_WIDGET_CANVAS);
    if (!instance->canvas) {
        return TERMINAL_GUI_ERROR_GUI_ERROR;
    }
    
    gui_set_widget_bounds(instance->canvas, &instance->terminal_rect);
    gui_add_widget_to_window(instance->window, instance->canvas);
    
    // Create scrollbar if needed
    if (instance->config.show_scrollbar) {
        instance->scrollbar_widget = gui_create_widget(GUI_WIDGET_SCROLLBAR);
        if (instance->scrollbar_widget) {
            gui_set_widget_bounds(instance->scrollbar_widget, &instance->scrollbar.rect);
            gui_add_widget_to_window(instance->window, instance->scrollbar_widget);
            instance->scrollbar.visible = true;
        }
    }
    
    return TERMINAL_GUI_SUCCESS;
}

static void terminal_gui_calculate_layout(terminal_gui_instance_t* instance) {
    gui_rect_t window_rect;
    gui_get_window_bounds(instance->window, &window_rect);
    
    // Terminal content area
    instance->terminal_rect.x = TERMINAL_GUI_PADDING;
    instance->terminal_rect.y = TERMINAL_GUI_PADDING;
    instance->terminal_rect.width = window_rect.width - 2 * TERMINAL_GUI_PADDING;
    instance->terminal_rect.height = window_rect.height - 2 * TERMINAL_GUI_PADDING;
    
    // Adjust for tabs if enabled
    if (instance->has_tabs) {
        instance->tab_bar_rect.x = 0;
        instance->tab_bar_rect.y = 0;
        instance->tab_bar_rect.width = window_rect.width;
        instance->tab_bar_rect.height = TERMINAL_GUI_TAB_HEIGHT;
        
        instance->terminal_rect.y += TERMINAL_GUI_TAB_HEIGHT;
        instance->terminal_rect.height -= TERMINAL_GUI_TAB_HEIGHT;
    }
    
    // Adjust for scrollbar if visible
    if (instance->config.show_scrollbar) {
        instance->scrollbar.rect.x = window_rect.width - TERMINAL_GUI_SCROLLBAR_WIDTH;
        instance->scrollbar.rect.y = instance->terminal_rect.y;
        instance->scrollbar.rect.width = TERMINAL_GUI_SCROLLBAR_WIDTH;
        instance->scrollbar.rect.height = instance->terminal_rect.height;
        
        instance->terminal_rect.width -= TERMINAL_GUI_SCROLLBAR_WIDTH;
    }
    
    // Update terminal size
    terminal_gui_update_terminal_size(instance);
}

static int terminal_gui_update_terminal_size(terminal_gui_instance_t* instance) {
    uint32_t new_cols = instance->terminal_rect.width / instance->config.char_width;
    uint32_t new_rows = instance->terminal_rect.height / instance->config.char_height;
    
    if (new_cols != instance->visible_cols || new_rows != instance->visible_rows) {
        instance->visible_cols = new_cols;
        instance->visible_rows = new_rows;
        
        // Resize terminal
        if (terminal_resize(&instance->terminal, new_cols, new_rows) != TERMINAL_SUCCESS) {
            return TERMINAL_GUI_ERROR_TERMINAL_ERROR;
        }
        
        instance->needs_redraw = true;
    }
    
    return TERMINAL_GUI_SUCCESS;
}

/* ================================
 * GUI Event Callbacks
 * ================================ */

static void terminal_gui_window_paint_callback(gui_window_t* window, gui_event_t* event) {
    terminal_gui_instance_t* instance = (terminal_gui_instance_t*)gui_get_window_user_data(window);
    if (instance) {
        terminal_gui_render(instance);
    }
}

static void terminal_gui_window_key_callback(gui_window_t* window, gui_event_t* event) {
    terminal_gui_instance_t* instance = (terminal_gui_instance_t*)gui_get_window_user_data(window);
    if (instance) {
        terminal_gui_handle_key_event(instance, event);
    }
}

static void terminal_gui_window_mouse_callback(gui_window_t* window, gui_event_t* event) {
    terminal_gui_instance_t* instance = (terminal_gui_instance_t*)gui_get_window_user_data(window);
    if (instance) {
        terminal_gui_handle_mouse_event(instance, event);
    }
}

static void terminal_gui_window_resize_callback(gui_window_t* window, gui_event_t* event) {
    terminal_gui_instance_t* instance = (terminal_gui_instance_t*)gui_get_window_user_data(window);
    if (instance) {
        terminal_gui_calculate_layout(instance);
        instance->needs_redraw = true;
    }
}

static void terminal_gui_window_close_callback(gui_window_t* window, gui_event_t* event) {
    terminal_gui_instance_t* instance = (terminal_gui_instance_t*)gui_get_window_user_data(window);
    if (instance) {
        if (instance->config.on_close) {
            instance->config.on_close(instance);
        } else {
            terminal_gui_destroy_instance(instance);
        }
    }
}

/* ================================
 * Error Handling
 * ================================ */

const char* terminal_gui_get_error_string(terminal_gui_error_t error) {
    switch (error) {
        case TERMINAL_GUI_SUCCESS: return "Success";
        case TERMINAL_GUI_ERROR_INVALID_PARAM: return "Invalid parameter";
        case TERMINAL_GUI_ERROR_NO_MEMORY: return "Out of memory";
        case TERMINAL_GUI_ERROR_NOT_INITIALIZED: return "Terminal GUI not initialized";
        case TERMINAL_GUI_ERROR_INSTANCE_NOT_FOUND: return "Instance not found";
        case TERMINAL_GUI_ERROR_GUI_ERROR: return "GUI system error";
        case TERMINAL_GUI_ERROR_TERMINAL_ERROR: return "Terminal system error";
        case TERMINAL_GUI_ERROR_MAX_INSTANCES: return "Maximum instances reached";
        case TERMINAL_GUI_ERROR_INVALID_TAB: return "Invalid tab index";
        default: return "Unknown error";
    }
}
