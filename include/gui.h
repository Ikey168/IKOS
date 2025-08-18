/*
 * IKOS Operating System - GUI System Header
 * Issue #37: Graphical User Interface Implementation
 *
 * Comprehensive GUI system providing window management, widgets,
 * and event handling for graphical applications.
 */

#ifndef GUI_H
#define GUI_H

#include "framebuffer.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================
 * GUI Constants
 * ================================ */

#define GUI_MAX_WINDOWS         32
#define GUI_MAX_WIDGETS         256
#define GUI_MAX_EVENT_QUEUE     64
#define GUI_DEFAULT_FONT_SIZE   12
#define GUI_TITLE_BAR_HEIGHT    24
#define GUI_BORDER_WIDTH        2
#define GUI_TASKBAR_HEIGHT      32

/* Standard colors */
#define GUI_COLOR_WHITE         0xFFFFFFFF
#define GUI_COLOR_BLACK         0xFF000000
#define GUI_COLOR_GRAY          0xFF808080
#define GUI_COLOR_LIGHT_GRAY    0xFFC0C0C0
#define GUI_COLOR_DARK_GRAY     0xFF404040
#define GUI_COLOR_BLUE          0xFF0000FF
#define GUI_COLOR_RED           0xFFFF0000
#define GUI_COLOR_GREEN         0xFF00FF00
#define GUI_COLOR_YELLOW        0xFFFFFF00
#define GUI_COLOR_CYAN          0xFF00FFFF
#define GUI_COLOR_MAGENTA       0xFFFF00FF

/* ================================
 * Basic Data Types
 * ================================ */

typedef uint32_t gui_color_t;

typedef struct {
    int32_t x, y;
} gui_point_t;

typedef struct {
    int32_t x, y;
    uint32_t width, height;
} gui_rect_t;

typedef struct {
    uint32_t width, height;
} gui_size_t;

/* ================================
 * Event System
 * ================================ */

typedef enum {
    GUI_EVENT_NONE = 0,
    GUI_EVENT_MOUSE_MOVE,
    GUI_EVENT_MOUSE_DOWN,
    GUI_EVENT_MOUSE_UP,
    GUI_EVENT_MOUSE_CLICK,
    GUI_EVENT_MOUSE_DOUBLE_CLICK,
    GUI_EVENT_KEY_DOWN,
    GUI_EVENT_KEY_UP,
    GUI_EVENT_CHAR_INPUT,
    GUI_EVENT_WINDOW_CLOSE,
    GUI_EVENT_WINDOW_RESIZE,
    GUI_EVENT_WINDOW_MOVE,
    GUI_EVENT_WINDOW_FOCUS,
    GUI_EVENT_PAINT,
    GUI_EVENT_TIMER,
    GUI_EVENT_CUSTOM
} gui_event_type_t;

typedef enum {
    GUI_MOUSE_LEFT = 0x01,
    GUI_MOUSE_RIGHT = 0x02,
    GUI_MOUSE_MIDDLE = 0x04
} gui_mouse_button_t;

typedef struct {
    gui_event_type_t type;
    uint32_t timestamp;
    void* target;           /* Window or widget that receives the event */
    
    union {
        struct {
            gui_point_t position;
            gui_mouse_button_t buttons;
            int32_t wheel_delta;
        } mouse;
        
        struct {
            uint32_t keycode;
            uint32_t modifiers;
            char character;
        } keyboard;
        
        struct {
            gui_rect_t area;
        } paint;
        
        struct {
            uint32_t timer_id;
        } timer;
        
        struct {
            void* data;
            uint32_t size;
        } custom;
    } data;
} gui_event_t;

typedef void (*gui_event_handler_t)(gui_event_t* event, void* user_data);

/* ================================
 * Widget System
 * ================================ */

typedef enum {
    GUI_WIDGET_WINDOW = 0,
    GUI_WIDGET_BUTTON,
    GUI_WIDGET_LABEL,
    GUI_WIDGET_TEXTBOX,
    GUI_WIDGET_CHECKBOX,
    GUI_WIDGET_RADIOBUTTON,
    GUI_WIDGET_LISTBOX,
    GUI_WIDGET_PANEL,
    GUI_WIDGET_MENU,
    GUI_WIDGET_MENUITEM,
    GUI_WIDGET_SCROLLBAR,
    GUI_WIDGET_PROGRESSBAR,
    GUI_WIDGET_CUSTOM
} gui_widget_type_t;

typedef struct gui_widget {
    uint32_t id;
    gui_widget_type_t type;
    gui_rect_t bounds;
    bool visible;
    bool enabled;
    bool focused;
    gui_color_t background_color;
    gui_color_t foreground_color;
    char* text;
    
    struct gui_widget* parent;
    struct gui_widget* first_child;
    struct gui_widget* next_sibling;
    
    gui_event_handler_t event_handler;
    void* user_data;
    
    /* Widget-specific data */
    union {
        struct {
            bool pressed;
        } button;
        
        struct {
            char* content;
            uint32_t cursor_pos;
            uint32_t selection_start;
            uint32_t selection_end;
        } textbox;
        
        struct {
            bool checked;
        } checkbox;
        
        struct {
            char** items;
            uint32_t item_count;
            int32_t selected_index;
        } listbox;
        
        struct {
            int32_t min_value;
            int32_t max_value;
            int32_t current_value;
        } progressbar;
    } widget_data;
} gui_widget_t;

/* ================================
 * Window System
 * ================================ */

typedef enum {
    GUI_WINDOW_NORMAL = 0,
    GUI_WINDOW_DIALOG,
    GUI_WINDOW_POPUP,
    GUI_WINDOW_TOOLTIP
} gui_window_type_t;

typedef enum {
    GUI_WINDOW_STATE_NORMAL = 0,
    GUI_WINDOW_STATE_MINIMIZED,
    GUI_WINDOW_STATE_MAXIMIZED,
    GUI_WINDOW_STATE_FULLSCREEN
} gui_window_state_t;

typedef struct gui_window {
    uint32_t id;
    gui_window_type_t type;
    gui_window_state_t state;
    gui_rect_t bounds;
    gui_rect_t restored_bounds;
    char* title;
    bool visible;
    bool resizable;
    bool movable;
    bool closable;
    bool minimizable;
    bool maximizable;
    int32_t z_order;
    
    gui_widget_t* root_widget;
    gui_event_handler_t event_handler;
    void* user_data;
    
    fb_color_t* back_buffer;
    bool needs_redraw;
    gui_rect_t dirty_rect;
    
    struct gui_window* next;
} gui_window_t;

/* ================================
 * Graphics Context
 * ================================ */

typedef struct {
    gui_window_t* target_window;
    gui_rect_t clip_rect;
    gui_color_t foreground_color;
    gui_color_t background_color;
    uint32_t font_size;
    bool font_bold;
    bool font_italic;
} gui_graphics_context_t;

/* ================================
 * Desktop Environment
 * ================================ */

typedef struct {
    gui_color_t background_color;
    char* wallpaper_path;
    bool show_taskbar;
    bool show_desktop_icons;
    gui_rect_t screen_bounds;
    
    gui_window_t* active_window;
    gui_window_t* window_list;
    gui_widget_t* focused_widget;
    
    gui_point_t cursor_position;
    bool cursor_visible;
    
    /* Event queue */
    gui_event_t event_queue[GUI_MAX_EVENT_QUEUE];
    uint32_t event_queue_head;
    uint32_t event_queue_tail;
    
    /* Statistics */
    uint64_t frames_rendered;
    uint64_t events_processed;
    uint32_t window_count;
    uint32_t widget_count;
} gui_desktop_t;

/* ================================
 * Function Declarations
 * ================================ */

/* Core GUI System */
int gui_init(void);
void gui_shutdown(void);
int gui_main_loop(void);
void gui_update(void);
void gui_render(void);

/* Desktop Management */
gui_desktop_t* gui_get_desktop(void);
void gui_set_wallpaper(const char* path);
void gui_show_taskbar(bool show);
void gui_invalidate_screen(void);

/* Window Management */
gui_window_t* gui_create_window(const char* title, gui_rect_t bounds, gui_window_type_t type);
void gui_destroy_window(gui_window_t* window);
void gui_show_window(gui_window_t* window, bool show);
void gui_move_window(gui_window_t* window, gui_point_t position);
void gui_resize_window(gui_window_t* window, gui_size_t size);
void gui_set_window_title(gui_window_t* window, const char* title);
void gui_set_window_state(gui_window_t* window, gui_window_state_t state);
void gui_bring_window_to_front(gui_window_t* window);
void gui_set_active_window(gui_window_t* window);
gui_window_t* gui_get_active_window(void);
gui_window_t* gui_find_window_at_point(gui_point_t point);

/* Widget Management */
gui_widget_t* gui_create_widget(gui_widget_type_t type, gui_rect_t bounds, gui_widget_t* parent);
void gui_destroy_widget(gui_widget_t* widget);
void gui_show_widget(gui_widget_t* widget, bool show);
void gui_move_widget(gui_widget_t* widget, gui_point_t position);
void gui_resize_widget(gui_widget_t* widget, gui_size_t size);
void gui_set_widget_text(gui_widget_t* widget, const char* text);
const char* gui_get_widget_text(gui_widget_t* widget);
void gui_set_widget_colors(gui_widget_t* widget, gui_color_t fg, gui_color_t bg);
void gui_set_widget_enabled(gui_widget_t* widget, bool enabled);
void gui_set_widget_focus(gui_widget_t* widget);
gui_widget_t* gui_get_focused_widget(void);
gui_widget_t* gui_find_widget_at_point(gui_window_t* window, gui_point_t point);

/* Specific Widget Creation */
gui_widget_t* gui_create_button(gui_rect_t bounds, const char* text, gui_widget_t* parent);
gui_widget_t* gui_create_label(gui_rect_t bounds, const char* text, gui_widget_t* parent);
gui_widget_t* gui_create_textbox(gui_rect_t bounds, const char* text, gui_widget_t* parent);
gui_widget_t* gui_create_checkbox(gui_rect_t bounds, const char* text, bool checked, gui_widget_t* parent);
gui_widget_t* gui_create_listbox(gui_rect_t bounds, gui_widget_t* parent);
gui_widget_t* gui_create_progressbar(gui_rect_t bounds, int32_t min, int32_t max, gui_widget_t* parent);

/* Widget Operations */
void gui_button_set_pressed(gui_widget_t* button, bool pressed);
bool gui_button_is_pressed(gui_widget_t* button);
void gui_checkbox_set_checked(gui_widget_t* checkbox, bool checked);
bool gui_checkbox_is_checked(gui_widget_t* checkbox);
void gui_textbox_set_cursor_pos(gui_widget_t* textbox, uint32_t pos);
uint32_t gui_textbox_get_cursor_pos(gui_widget_t* textbox);
void gui_listbox_add_item(gui_widget_t* listbox, const char* item);
void gui_listbox_remove_item(gui_widget_t* listbox, uint32_t index);
void gui_listbox_set_selected(gui_widget_t* listbox, int32_t index);
int32_t gui_listbox_get_selected(gui_widget_t* listbox);
void gui_progressbar_set_value(gui_widget_t* progressbar, int32_t value);
int32_t gui_progressbar_get_value(gui_widget_t* progressbar);

/* Event System */
void gui_post_event(gui_event_t* event);
bool gui_get_event(gui_event_t* event);
void gui_set_event_handler(gui_widget_t* widget, gui_event_handler_t handler, void* user_data);
void gui_set_window_event_handler(gui_window_t* window, gui_event_handler_t handler, void* user_data);

/* Graphics Operations */
gui_graphics_context_t* gui_get_graphics_context(gui_window_t* window);
void gui_clear_rect(gui_graphics_context_t* ctx, gui_rect_t rect, gui_color_t color);
void gui_draw_pixel(gui_graphics_context_t* ctx, gui_point_t point, gui_color_t color);
void gui_draw_line(gui_graphics_context_t* ctx, gui_point_t start, gui_point_t end, gui_color_t color);
void gui_draw_rect(gui_graphics_context_t* ctx, gui_rect_t rect, gui_color_t color);
void gui_fill_rect(gui_graphics_context_t* ctx, gui_rect_t rect, gui_color_t color);
void gui_draw_circle(gui_graphics_context_t* ctx, gui_point_t center, uint32_t radius, gui_color_t color);
void gui_fill_circle(gui_graphics_context_t* ctx, gui_point_t center, uint32_t radius, gui_color_t color);
void gui_draw_text(gui_graphics_context_t* ctx, gui_point_t position, const char* text);
void gui_draw_char(gui_graphics_context_t* ctx, gui_point_t position, char c);

/* Utility Functions */
gui_rect_t gui_rect_make(int32_t x, int32_t y, uint32_t width, uint32_t height);
gui_point_t gui_point_make(int32_t x, int32_t y);
gui_size_t gui_size_make(uint32_t width, uint32_t height);
bool gui_rect_contains_point(gui_rect_t rect, gui_point_t point);
bool gui_rect_intersects(gui_rect_t rect1, gui_rect_t rect2);
gui_rect_t gui_rect_intersection(gui_rect_t rect1, gui_rect_t rect2);
gui_rect_t gui_rect_union(gui_rect_t rect1, gui_rect_t rect2);
gui_color_t gui_color_make_rgb(uint8_t r, uint8_t g, uint8_t b);
gui_color_t gui_color_make_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/* Font and Text Utilities */
uint32_t gui_text_width(const char* text, uint32_t font_size);
uint32_t gui_text_height(uint32_t font_size);
void gui_text_bounds(const char* text, uint32_t font_size, gui_size_t* bounds);

/* Input Handling */
void gui_set_cursor_position(gui_point_t position);
gui_point_t gui_get_cursor_position(void);
void gui_show_cursor(bool show);
bool gui_is_cursor_visible(void);

/* Resource Management */
int gui_load_font(const char* path, uint32_t size);
int gui_load_image(const char* path);
void gui_free_font(int font_id);
void gui_free_image(int image_id);

/* Statistics and Debug */
void gui_get_statistics(uint64_t* frames, uint64_t* events, uint32_t* windows, uint32_t* widgets);
void gui_debug_print_window_tree(void);
void gui_debug_print_widget_tree(gui_widget_t* widget, int depth);

#ifdef __cplusplus
}
#endif

#endif /* GUI_H */
