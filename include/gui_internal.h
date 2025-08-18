/*
 * IKOS Operating System - GUI Internal Header
 * Issue #37: Graphical User Interface Implementation
 *
 * Internal functions and definitions shared between GUI modules.
 */

#ifndef GUI_INTERNAL_H
#define GUI_INTERNAL_H

#include "gui.h"

/* ================================
 * Internal Function Declarations
 * ================================ */

/* Memory management */
gui_widget_t* allocate_widget(void);
void free_widget(gui_widget_t* widget);
gui_window_t* allocate_window(void);
void free_window(gui_window_t* window);

/* Widget hierarchy management */
void add_widget_to_parent(gui_widget_t* widget, gui_widget_t* parent);
void remove_widget_from_parent(gui_widget_t* widget);

/* Window management */
void add_window_to_list(gui_window_t* window);
void remove_window_from_list(gui_window_t* window);
void invalidate_window_rect(gui_window_t* window, gui_rect_t rect);

/* Widget traversal */
gui_widget_t* gui_find_widget_recursive(gui_widget_t* widget, gui_point_t point);

/* Rendering functions */
void gui_render_window(gui_window_t* window);
void gui_render_title_bar(gui_window_t* window, gui_graphics_context_t* ctx);
void gui_render_window_border(gui_window_t* window, gui_graphics_context_t* ctx);
void gui_render_widget(gui_widget_t* widget, gui_graphics_context_t* ctx);
void gui_render_button(gui_widget_t* button, gui_graphics_context_t* ctx);
void gui_render_label(gui_widget_t* label, gui_graphics_context_t* ctx);
void gui_render_textbox(gui_widget_t* textbox, gui_graphics_context_t* ctx);
void gui_render_checkbox(gui_widget_t* checkbox, gui_graphics_context_t* ctx);
void gui_render_listbox(gui_widget_t* listbox, gui_graphics_context_t* ctx);
void gui_render_progressbar(gui_widget_t* progressbar, gui_graphics_context_t* ctx);
void gui_render_panel(gui_widget_t* panel, gui_graphics_context_t* ctx);
void gui_render_cursor(void);

/* Global state access */
extern gui_desktop_t g_desktop;
extern gui_widget_t g_widgets[GUI_MAX_WIDGETS];
extern bool g_widget_slots[GUI_MAX_WIDGETS];
extern uint32_t g_next_widget_id;
extern gui_graphics_context_t g_graphics_ctx;

#endif /* GUI_INTERNAL_H */
