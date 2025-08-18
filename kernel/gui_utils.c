/*
 * IKOS Operating System - GUI Utility Functions and Input Handling
 * Issue #37: Graphical User Interface Implementation
 *
 * Utility functions, input handling, and resource management.
 */

#include "gui.h"
#include "gui_internal.h"
#include "framebuffer.h"
#include "memory.h"
#include "string.h"
#include "keyboard.h"

/* External references */
extern gui_desktop_t g_desktop;

/* ================================
 * Utility Functions
 * ================================ */

gui_rect_t gui_rect_make(int32_t x, int32_t y, uint32_t width, uint32_t height) {
    gui_rect_t rect = {x, y, width, height};
    return rect;
}

gui_point_t gui_point_make(int32_t x, int32_t y) {
    gui_point_t point = {x, y};
    return point;
}

gui_size_t gui_size_make(uint32_t width, uint32_t height) {
    gui_size_t size = {width, height};
    return size;
}

bool gui_rect_contains_point(gui_rect_t rect, gui_point_t point) {
    return (point.x >= rect.x && 
            point.y >= rect.y &&
            point.x < rect.x + (int32_t)rect.width &&
            point.y < rect.y + (int32_t)rect.height);
}

bool gui_rect_intersects(gui_rect_t rect1, gui_rect_t rect2) {
    return !(rect1.x + (int32_t)rect1.width <= rect2.x ||
             rect2.x + (int32_t)rect2.width <= rect1.x ||
             rect1.y + (int32_t)rect1.height <= rect2.y ||
             rect2.y + (int32_t)rect2.height <= rect1.y);
}

gui_rect_t gui_rect_intersection(gui_rect_t rect1, gui_rect_t rect2) {
    int32_t x1 = (rect1.x > rect2.x) ? rect1.x : rect2.x;
    int32_t y1 = (rect1.y > rect2.y) ? rect1.y : rect2.y;
    int32_t x2 = ((rect1.x + (int32_t)rect1.width) < (rect2.x + (int32_t)rect2.width)) ? 
                 (rect1.x + (int32_t)rect1.width) : (rect2.x + (int32_t)rect2.width);
    int32_t y2 = ((rect1.y + (int32_t)rect1.height) < (rect2.y + (int32_t)rect2.height)) ?
                 (rect1.y + (int32_t)rect1.height) : (rect2.y + (int32_t)rect2.height);
    
    if (x2 <= x1 || y2 <= y1) {
        return gui_rect_make(0, 0, 0, 0); /* Empty rectangle */
    }
    
    return gui_rect_make(x1, y1, x2 - x1, y2 - y1);
}

gui_rect_t gui_rect_union(gui_rect_t rect1, gui_rect_t rect2) {
    if (rect1.width == 0 || rect1.height == 0) return rect2;
    if (rect2.width == 0 || rect2.height == 0) return rect1;
    
    int32_t x1 = (rect1.x < rect2.x) ? rect1.x : rect2.x;
    int32_t y1 = (rect1.y < rect2.y) ? rect1.y : rect2.y;
    int32_t x2 = ((rect1.x + (int32_t)rect1.width) > (rect2.x + (int32_t)rect2.width)) ?
                 (rect1.x + (int32_t)rect1.width) : (rect2.x + (int32_t)rect2.width);
    int32_t y2 = ((rect1.y + (int32_t)rect1.height) > (rect2.y + (int32_t)rect2.height)) ?
                 (rect1.y + (int32_t)rect1.height) : (rect2.y + (int32_t)rect2.height);
    
    return gui_rect_make(x1, y1, x2 - x1, y2 - y1);
}

gui_color_t gui_color_make_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (0xFF << 24) | (r << 16) | (g << 8) | b;
}

gui_color_t gui_color_make_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

/* ================================
 * Font and Text Utilities
 * ================================ */

uint32_t gui_text_width(const char* text, uint32_t font_size) {
    if (!text) return 0;
    
    /* Simple fixed-width font calculation */
    uint32_t char_width = 8; /* Assume 8 pixels per character */
    if (font_size > 12) char_width = 10;
    if (font_size > 16) char_width = 12;
    
    return strlen(text) * char_width;
}

uint32_t gui_text_height(uint32_t font_size) {
    /* Simple font height calculation */
    if (font_size <= 8) return 8;
    if (font_size <= 12) return 12;
    if (font_size <= 16) return 16;
    return font_size;
}

void gui_text_bounds(const char* text, uint32_t font_size, gui_size_t* bounds) {
    if (!bounds) return;
    
    bounds->width = gui_text_width(text, font_size);
    bounds->height = gui_text_height(font_size);
}

/* ================================
 * Input Handling
 * ================================ */

void gui_set_cursor_position(gui_point_t position) {
    g_desktop.cursor_position = position;
    
    /* Clamp to screen bounds */
    if (g_desktop.cursor_position.x < 0) g_desktop.cursor_position.x = 0;
    if (g_desktop.cursor_position.y < 0) g_desktop.cursor_position.y = 0;
    if (g_desktop.cursor_position.x >= (int32_t)g_desktop.screen_bounds.width) {
        g_desktop.cursor_position.x = (int32_t)g_desktop.screen_bounds.width - 1;
    }
    if (g_desktop.cursor_position.y >= (int32_t)g_desktop.screen_bounds.height) {
        g_desktop.cursor_position.y = (int32_t)g_desktop.screen_bounds.height - 1;
    }
}

gui_point_t gui_get_cursor_position(void) {
    return g_desktop.cursor_position;
}

void gui_show_cursor(bool show) {
    g_desktop.cursor_visible = show;
}

bool gui_is_cursor_visible(void) {
    return g_desktop.cursor_visible;
}

/* ================================
 * Input Event Processing
 * ================================ */

void gui_handle_keyboard_event(uint32_t keycode, bool pressed) {
    gui_event_t event;
    memset(&event, 0, sizeof(event));
    
    event.type = pressed ? GUI_EVENT_KEY_DOWN : GUI_EVENT_KEY_UP;
    event.timestamp = 0; /* TODO: Get system timestamp */
    event.data.keyboard.keycode = keycode;
    event.data.keyboard.modifiers = 0; /* TODO: Track modifier keys */
    
    /* Convert keycode to character if printable */
    if (pressed && keycode >= 32 && keycode <= 126) {
        event.data.keyboard.character = (char)keycode;
    } else {
        event.data.keyboard.character = 0;
    }
    
    gui_post_event(&event);
    
    /* Generate separate character input event for printable characters */
    if (pressed && event.data.keyboard.character) {
        gui_event_t char_event = event;
        char_event.type = GUI_EVENT_CHAR_INPUT;
        gui_post_event(&char_event);
    }
}

void gui_handle_mouse_event(int32_t x, int32_t y, uint32_t buttons, bool button_changed) {
    static uint32_t last_buttons = 0;
    static gui_point_t last_position = {-1, -1};
    
    gui_point_t position = {x, y};
    
    /* Generate mouse move event if position changed */
    if (position.x != last_position.x || position.y != last_position.y) {
        gui_event_t event;
        memset(&event, 0, sizeof(event));
        
        event.type = GUI_EVENT_MOUSE_MOVE;
        event.timestamp = 0;
        event.data.mouse.position = position;
        event.data.mouse.buttons = buttons;
        event.data.mouse.wheel_delta = 0;
        
        gui_post_event(&event);
        
        /* Update cursor position */
        gui_set_cursor_position(position);
        last_position = position;
    }
    
    /* Generate button events if buttons changed */
    if (button_changed || buttons != last_buttons) {
        uint32_t changed = buttons ^ last_buttons;
        
        for (int i = 0; i < 3; i++) { /* Left, right, middle buttons */
            uint32_t button_mask = 1 << i;
            if (changed & button_mask) {
                gui_event_t event;
                memset(&event, 0, sizeof(event));
                
                event.type = (buttons & button_mask) ? GUI_EVENT_MOUSE_DOWN : GUI_EVENT_MOUSE_UP;
                event.timestamp = 0;
                event.data.mouse.position = position;
                event.data.mouse.buttons = button_mask;
                event.data.mouse.wheel_delta = 0;
                
                gui_post_event(&event);
                
                /* Generate click event for mouse down */
                if (event.type == GUI_EVENT_MOUSE_DOWN) {
                    gui_event_t click_event = event;
                    click_event.type = GUI_EVENT_MOUSE_CLICK;
                    gui_post_event(&click_event);
                }
            }
        }
        
        last_buttons = buttons;
    }
}

/* ================================
 * Resource Management
 * ================================ */

int gui_load_font(const char* path, uint32_t size) {
    /* TODO: Implement font loading from file system */
    /* For now, we use built-in bitmap fonts */
    (void)path;
    (void)size;
    return 0; /* Default font ID */
}

int gui_load_image(const char* path) {
    /* TODO: Implement image loading from file system */
    (void)path;
    return -1; /* Not implemented */
}

void gui_free_font(int font_id) {
    /* TODO: Free loaded font resources */
    (void)font_id;
}

void gui_free_image(int image_id) {
    /* TODO: Free loaded image resources */
    (void)image_id;
}

/* ================================
 * Statistics and Debug
 * ================================ */

void gui_get_statistics(uint64_t* frames, uint64_t* events, uint32_t* windows, uint32_t* widgets) {
    if (frames) *frames = g_desktop.frames_rendered;
    if (events) *events = g_desktop.events_processed;
    if (windows) *windows = g_desktop.window_count;
    if (widgets) *widgets = g_desktop.widget_count;
}

void gui_debug_print_window_tree(void) {
    /* TODO: Implement debug output to console or log */
    /* For now, this is a placeholder */
}

void gui_debug_print_widget_tree(gui_widget_t* widget, int depth) {
    /* TODO: Implement debug output to console or log */
    (void)widget;
    (void)depth;
}

/* ================================
 * Integration with Keyboard Driver
 * ================================ */

void gui_keyboard_callback(uint32_t keycode, bool pressed) {
    gui_handle_keyboard_event(keycode, pressed);
}

/* ================================
 * Simple Mouse Driver Integration
 * ================================ */

/* Basic PS/2 mouse support placeholder */
void gui_init_mouse(void) {
    /* TODO: Initialize PS/2 mouse or other mouse hardware */
    /* This would typically involve setting up interrupts and 
       configuring the mouse controller */
}

void gui_mouse_interrupt_handler(void) {
    /* TODO: Handle mouse interrupts and call gui_handle_mouse_event */
    /* This would read mouse data from the PS/2 controller and
       convert it to screen coordinates and button states */
}

/* ================================
 * High-Level Helper Functions
 * ================================ */

gui_window_t* gui_create_message_box(const char* title, const char* message, const char* button_text) {
    /* Calculate window size based on message length */
    uint32_t msg_width = gui_text_width(message, GUI_DEFAULT_FONT_SIZE);
    uint32_t win_width = (msg_width > 200) ? msg_width + 40 : 240;
    uint32_t win_height = 120;
    
    /* Center on screen */
    gui_point_t pos = {
        ((int32_t)g_desktop.screen_bounds.width - (int32_t)win_width) / 2,
        ((int32_t)g_desktop.screen_bounds.height - (int32_t)win_height) / 2
    };
    
    gui_rect_t bounds = {pos.x, pos.y, win_width, win_height};
    gui_window_t* window = gui_create_window(title, bounds, GUI_WINDOW_DIALOG);
    if (!window) return NULL;
    
    /* Create message label */
    gui_rect_t label_bounds = {10, GUI_TITLE_BAR_HEIGHT + 10, win_width - 20, 40};
    gui_widget_t* label = gui_create_label(label_bounds, message, window->root_widget);
    
    /* Create OK button */
    const char* btn_text = button_text ? button_text : "OK";
    gui_rect_t btn_bounds = {(int32_t)win_width - 80, (int32_t)win_height - 40, 70, 25};
    gui_widget_t* button = gui_create_button(btn_bounds, btn_text, window->root_widget);
    
    return window;
}

bool gui_show_message_box(const char* title, const char* message) {
    gui_window_t* msgbox = gui_create_message_box(title, message, "OK");
    if (!msgbox) return false;
    
    gui_show_window(msgbox, true);
    
    /* Simple modal loop - wait for button click or window close */
    gui_event_t event;
    bool done = false;
    
    while (!done) {
        if (gui_get_event(&event)) {
            if (event.target == msgbox) {
                if (event.type == GUI_EVENT_WINDOW_CLOSE ||
                    (event.type == GUI_EVENT_MOUSE_CLICK && 
                     event.data.mouse.position.x >= msgbox->bounds.x + (int32_t)msgbox->bounds.width - 80)) {
                    done = true;
                }
            }
        }
        
        /* Update and render */
        gui_update();
        gui_render();
    }
    
    gui_destroy_window(msgbox);
    return true;
}

/* ================================
 * Window Manager Helpers
 * ================================ */

void gui_cascade_windows(void) {
    int32_t offset = 0;
    gui_window_t* window = g_desktop.window_list;
    
    while (window) {
        if (window->visible && window->type == GUI_WINDOW_NORMAL) {
            gui_move_window(window, gui_point_make(50 + offset, 50 + offset));
            offset += 30;
            
            /* Wrap around if we get too far */
            if (offset > 200) offset = 0;
        }
        window = window->next;
    }
}

void gui_tile_windows_horizontal(void) {
    /* Count visible normal windows */
    uint32_t window_count = 0;
    gui_window_t* window = g_desktop.window_list;
    while (window) {
        if (window->visible && window->type == GUI_WINDOW_NORMAL) {
            window_count++;
        }
        window = window->next;
    }
    
    if (window_count == 0) return;
    
    /* Calculate tile size */
    uint32_t tile_width = g_desktop.screen_bounds.width / window_count;
    uint32_t tile_height = g_desktop.screen_bounds.height - GUI_TASKBAR_HEIGHT;
    
    /* Position windows */
    uint32_t x = 0;
    window = g_desktop.window_list;
    while (window) {
        if (window->visible && window->type == GUI_WINDOW_NORMAL) {
            gui_move_window(window, gui_point_make(x, 0));
            gui_resize_window(window, gui_size_make(tile_width, tile_height));
            x += tile_width;
        }
        window = window->next;
    }
}

void gui_minimize_all_windows(void) {
    gui_window_t* window = g_desktop.window_list;
    while (window) {
        if (window->visible && window->type == GUI_WINDOW_NORMAL) {
            gui_set_window_state(window, GUI_WINDOW_STATE_MINIMIZED);
        }
        window = window->next;
    }
}

/* ================================
 * Accessibility Helpers
 * ================================ */

void gui_set_high_contrast_mode(bool enabled) {
    /* TODO: Implement high contrast color scheme */
    (void)enabled;
}

void gui_set_font_scale(float scale) {
    /* TODO: Implement font scaling for accessibility */
    (void)scale;
}

/* ================================
 * Theme Support
 * ================================ */

typedef struct {
    gui_color_t window_background;
    gui_color_t window_border;
    gui_color_t button_background;
    gui_color_t button_text;
    gui_color_t text_background;
    gui_color_t text_foreground;
    gui_color_t accent_color;
} gui_theme_t;

static gui_theme_t g_current_theme = {
    .window_background = GUI_COLOR_LIGHT_GRAY,
    .window_border = GUI_COLOR_DARK_GRAY,
    .button_background = GUI_COLOR_LIGHT_GRAY,
    .button_text = GUI_COLOR_BLACK,
    .text_background = GUI_COLOR_WHITE,
    .text_foreground = GUI_COLOR_BLACK,
    .accent_color = GUI_COLOR_BLUE
};

void gui_set_theme(gui_theme_t* theme) {
    if (theme) {
        g_current_theme = *theme;
        gui_invalidate_screen(); /* Redraw everything with new theme */
    }
}

gui_theme_t* gui_get_current_theme(void) {
    return &g_current_theme;
}

/* End of GUI utility implementation */
