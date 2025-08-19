/*
 * IKOS Terminal GUI Integration - Header Definitions
 * Issue #43 - Terminal Emulator GUI Integration
 * 
 * Integrates the CLI terminal into the GUI environment with support for
 * multiple terminal instances, tabs/windows, and seamless CLI-GUI interaction.
 */

#ifndef TERMINAL_GUI_H
#define TERMINAL_GUI_H

#include "terminal.h"
#include "gui.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================
 * Terminal GUI Constants
 * ================================ */

#define TERMINAL_GUI_MAX_INSTANCES      16      /* Maximum terminal instances */
#define TERMINAL_GUI_MAX_TABS           8       /* Maximum tabs per window */
#define TERMINAL_GUI_DEFAULT_WIDTH      640     /* Default window width */
#define TERMINAL_GUI_DEFAULT_HEIGHT     480     /* Default window height */
#define TERMINAL_GUI_CHAR_WIDTH         8       /* Character cell width in pixels */
#define TERMINAL_GUI_CHAR_HEIGHT        16      /* Character cell height in pixels */
#define TERMINAL_GUI_SCROLLBAR_WIDTH    16      /* Scrollbar width */
#define TERMINAL_GUI_TAB_HEIGHT         24      /* Tab bar height */
#define TERMINAL_GUI_PADDING            4       /* Interior padding */

/* Terminal GUI colors */
#define TERMINAL_GUI_BG_COLOR           GUI_COLOR_BLACK
#define TERMINAL_GUI_FG_COLOR           GUI_COLOR_WHITE
#define TERMINAL_GUI_SELECTION_COLOR    0xFF3366CC
#define TERMINAL_GUI_CURSOR_COLOR       GUI_COLOR_WHITE
#define TERMINAL_GUI_TAB_ACTIVE_COLOR   GUI_COLOR_LIGHT_GRAY
#define TERMINAL_GUI_TAB_INACTIVE_COLOR GUI_COLOR_GRAY

/* ================================
 * Terminal GUI Data Structures
 * ================================ */

typedef enum {
    TERMINAL_GUI_MODE_WINDOW = 0,       /* Each terminal in separate window */
    TERMINAL_GUI_MODE_TABS              /* Multiple terminals in tabbed interface */
} terminal_gui_mode_t;

typedef enum {
    TERMINAL_GUI_STATE_INACTIVE = 0,
    TERMINAL_GUI_STATE_ACTIVE,
    TERMINAL_GUI_STATE_FOCUSED,
    TERMINAL_GUI_STATE_MINIMIZED
} terminal_gui_state_t;

/* Terminal selection for copy/paste */
typedef struct {
    bool active;
    gui_point_t start;
    gui_point_t end;
    gui_point_t start_char;
    gui_point_t end_char;
} terminal_selection_t;

/* Terminal scrollbar */
typedef struct {
    bool visible;
    gui_rect_t rect;
    uint32_t total_lines;
    uint32_t visible_lines;
    uint32_t scroll_position;
    bool dragging;
    gui_point_t drag_start;
} terminal_scrollbar_t;

/* Terminal tab information */
typedef struct {
    bool active;
    char title[64];
    terminal_gui_state_t state;
    gui_rect_t rect;
    struct terminal_gui_instance* terminal;
} terminal_gui_tab_t;

/* Forward declaration */
typedef struct terminal_gui_instance terminal_gui_instance_t;

/* Terminal GUI event callbacks */
typedef void (*terminal_gui_char_callback_t)(terminal_gui_instance_t* instance, char c);
typedef void (*terminal_gui_resize_callback_t)(terminal_gui_instance_t* instance, uint32_t width, uint32_t height);
typedef void (*terminal_gui_close_callback_t)(terminal_gui_instance_t* instance);
typedef void (*terminal_gui_focus_callback_t)(terminal_gui_instance_t* instance, bool focused);

/* Terminal GUI configuration */
typedef struct {
    terminal_gui_mode_t mode;
    gui_color_t bg_color;
    gui_color_t fg_color;
    gui_color_t cursor_color;
    gui_color_t selection_color;
    uint32_t char_width;
    uint32_t char_height;
    bool show_scrollbar;
    bool enable_tabs;
    bool enable_mouse;
    bool enable_clipboard;
    char font_name[32];
    uint32_t font_size;
    terminal_gui_char_callback_t on_char_input;
    terminal_gui_resize_callback_t on_resize;
    terminal_gui_close_callback_t on_close;
    terminal_gui_focus_callback_t on_focus;
} terminal_gui_config_t;

/* Main terminal GUI instance */
struct terminal_gui_instance {
    /* Identification */
    uint32_t id;
    bool active;
    char title[64];
    
    /* GUI components */
    gui_window_t* window;
    gui_widget_t* canvas;
    gui_widget_t* scrollbar_widget;
    
    /* Terminal integration */
    terminal_t terminal;
    terminal_gui_config_t config;
    terminal_gui_state_t state;
    
    /* Rendering state */
    gui_rect_t terminal_rect;
    uint32_t visible_cols;
    uint32_t visible_rows;
    uint32_t scroll_offset;
    bool needs_redraw;
    
    /* Interaction state */
    terminal_selection_t selection;
    terminal_scrollbar_t scrollbar;
    gui_point_t cursor_screen_pos;
    uint32_t blink_timer;
    bool cursor_visible;
    
    /* Tab support */
    bool has_tabs;
    uint32_t tab_count;
    uint32_t active_tab;
    terminal_gui_tab_t tabs[TERMINAL_GUI_MAX_TABS];
    gui_rect_t tab_bar_rect;
    
    /* Input handling */
    char input_buffer[256];
    uint32_t input_length;
    bool shift_pressed;
    bool ctrl_pressed;
    bool alt_pressed;
};

/* Terminal GUI manager */
typedef struct {
    bool initialized;
    uint32_t instance_count;
    terminal_gui_instance_t instances[TERMINAL_GUI_MAX_INSTANCES];
    terminal_gui_instance_t* focused_instance;
    terminal_gui_config_t default_config;
    
    /* Global settings */
    terminal_gui_mode_t global_mode;
    bool enable_multi_instance;
    uint32_t next_instance_id;
    
    /* Clipboard support */
    char* clipboard_data;
    uint32_t clipboard_size;
} terminal_gui_manager_t;

/* ================================
 * Terminal GUI Core Functions
 * ================================ */

/* Initialization and cleanup */
int terminal_gui_init(void);
void terminal_gui_cleanup(void);

/* Instance management */
terminal_gui_instance_t* terminal_gui_create_instance(const terminal_gui_config_t* config);
int terminal_gui_destroy_instance(terminal_gui_instance_t* instance);
terminal_gui_instance_t* terminal_gui_get_instance(uint32_t id);
terminal_gui_instance_t* terminal_gui_get_focused_instance(void);

/* Window and tab management */
int terminal_gui_show_window(terminal_gui_instance_t* instance);
int terminal_gui_hide_window(terminal_gui_instance_t* instance);
int terminal_gui_set_window_title(terminal_gui_instance_t* instance, const char* title);
int terminal_gui_add_tab(terminal_gui_instance_t* instance, const char* title);
int terminal_gui_remove_tab(terminal_gui_instance_t* instance, uint32_t tab_index);
int terminal_gui_switch_tab(terminal_gui_instance_t* instance, uint32_t tab_index);

/* Terminal operations */
int terminal_gui_write_text(terminal_gui_instance_t* instance, const char* text, uint32_t length);
int terminal_gui_write_char(terminal_gui_instance_t* instance, char c);
int terminal_gui_clear_screen(terminal_gui_instance_t* instance);
int terminal_gui_set_cursor_position(terminal_gui_instance_t* instance, uint32_t x, uint32_t y);

/* Rendering functions */
int terminal_gui_render(terminal_gui_instance_t* instance);
int terminal_gui_render_character(terminal_gui_instance_t* instance, uint32_t x, uint32_t y, char c, gui_color_t fg, gui_color_t bg);
int terminal_gui_render_cursor(terminal_gui_instance_t* instance);
int terminal_gui_render_selection(terminal_gui_instance_t* instance);
int terminal_gui_render_scrollbar(terminal_gui_instance_t* instance);
int terminal_gui_render_tabs(terminal_gui_instance_t* instance);

/* Event handling */
int terminal_gui_handle_key_event(terminal_gui_instance_t* instance, gui_event_t* event);
int terminal_gui_handle_mouse_event(terminal_gui_instance_t* instance, gui_event_t* event);
int terminal_gui_handle_resize_event(terminal_gui_instance_t* instance, gui_event_t* event);
int terminal_gui_handle_focus_event(terminal_gui_instance_t* instance, gui_event_t* event);

/* Scrolling and navigation */
int terminal_gui_scroll_up(terminal_gui_instance_t* instance, uint32_t lines);
int terminal_gui_scroll_down(terminal_gui_instance_t* instance, uint32_t lines);
int terminal_gui_scroll_to_top(terminal_gui_instance_t* instance);
int terminal_gui_scroll_to_bottom(terminal_gui_instance_t* instance);

/* Selection and clipboard */
int terminal_gui_start_selection(terminal_gui_instance_t* instance, gui_point_t start);
int terminal_gui_update_selection(terminal_gui_instance_t* instance, gui_point_t end);
int terminal_gui_end_selection(terminal_gui_instance_t* instance);
int terminal_gui_copy_selection(terminal_gui_instance_t* instance);
int terminal_gui_paste_clipboard(terminal_gui_instance_t* instance);

/* Utility functions */
gui_point_t terminal_gui_pixel_to_char(terminal_gui_instance_t* instance, gui_point_t pixel);
gui_point_t terminal_gui_char_to_pixel(terminal_gui_instance_t* instance, gui_point_t character);
int terminal_gui_get_default_config(terminal_gui_config_t* config);
int terminal_gui_update_size(terminal_gui_instance_t* instance);

/* Command line interface */
int terminal_gui_run_command(terminal_gui_instance_t* instance, const char* command);
int terminal_gui_execute_shell(terminal_gui_instance_t* instance);

/* Integration functions */
int terminal_gui_register_with_gui_system(void);
int terminal_gui_create_menu_entries(void);

/* Error handling */
typedef enum {
    TERMINAL_GUI_SUCCESS = 0,
    TERMINAL_GUI_ERROR_INVALID_PARAM = -1,
    TERMINAL_GUI_ERROR_NO_MEMORY = -2,
    TERMINAL_GUI_ERROR_NOT_INITIALIZED = -3,
    TERMINAL_GUI_ERROR_INSTANCE_NOT_FOUND = -4,
    TERMINAL_GUI_ERROR_GUI_ERROR = -5,
    TERMINAL_GUI_ERROR_TERMINAL_ERROR = -6,
    TERMINAL_GUI_ERROR_MAX_INSTANCES = -7,
    TERMINAL_GUI_ERROR_INVALID_TAB = -8
} terminal_gui_error_t;

const char* terminal_gui_get_error_string(terminal_gui_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* TERMINAL_GUI_H */
