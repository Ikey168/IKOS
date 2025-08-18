/*
 * IKOS Operating System - GUI Widget System and Rendering
 * Issue #37: Graphical User Interface Implementation
 *
 * Widget management, event handling, and rendering implementation.
 */

#include "gui.h"
#include "gui_internal.h"
#include "framebuffer.h"
#include "memory.h"
#include "string.h"

/* External references to global state */
extern gui_desktop_t g_desktop;
extern gui_widget_t g_widgets[GUI_MAX_WIDGETS];
extern bool g_widget_slots[GUI_MAX_WIDGETS];
extern uint32_t g_next_widget_id;

/* ================================
 * Widget Management
 * ================================ */

gui_widget_t* gui_create_widget(gui_widget_type_t type, gui_rect_t bounds, gui_widget_t* parent) {
    gui_widget_t* widget = allocate_widget();
    if (!widget) return NULL;
    
    /* Initialize widget */
    widget->type = type;
    widget->bounds = bounds;
    widget->visible = true;
    widget->enabled = true;
    widget->focused = false;
    widget->background_color = GUI_COLOR_LIGHT_GRAY;
    widget->foreground_color = GUI_COLOR_BLACK;
    widget->text = NULL;
    widget->parent = NULL;
    widget->first_child = NULL;
    widget->next_sibling = NULL;
    widget->event_handler = NULL;
    widget->user_data = NULL;
    
    /* Initialize widget-specific data */
    memset(&widget->widget_data, 0, sizeof(widget->widget_data));
    
    /* Add to parent if specified */
    if (parent) {
        add_widget_to_parent(widget, parent);
    }
    
    g_desktop.widget_count++;
    return widget;
}

void gui_destroy_widget(gui_widget_t* widget) {
    if (!widget) return;
    
    /* Remove focus if this widget has it */
    if (g_desktop.focused_widget == widget) {
        g_desktop.focused_widget = NULL;
    }
    
    /* Recursively destroy all child widgets */
    gui_widget_t* child = widget->first_child;
    while (child) {
        gui_widget_t* next = child->next_sibling;
        gui_destroy_widget(child);
        child = next;
    }
    
    /* Remove from parent */
    remove_widget_from_parent(widget);
    
    /* Free widget-specific resources */
    if (widget->text) {
        free(widget->text);
    }
    
    switch (widget->type) {
        case GUI_WIDGET_TEXTBOX:
            if (widget->widget_data.textbox.content) {
                free(widget->widget_data.textbox.content);
            }
            break;
        case GUI_WIDGET_LISTBOX:
            if (widget->widget_data.listbox.items) {
                for (uint32_t i = 0; i < widget->widget_data.listbox.item_count; i++) {
                    if (widget->widget_data.listbox.items[i]) {
                        free(widget->widget_data.listbox.items[i]);
                    }
                }
                free(widget->widget_data.listbox.items);
            }
            break;
        default:
            break;
    }
    
    /* Free widget slot */
    free_widget(widget);
    g_desktop.widget_count--;
}

void gui_show_widget(gui_widget_t* widget, bool show) {
    if (!widget || widget->visible == show) return;
    
    widget->visible = show;
    
    /* Invalidate widget area */
    if (widget->parent) {
        /* Find the window containing this widget */
        gui_widget_t* root = widget;
        while (root->parent) {
            root = root->parent;
        }
        
        /* Find window that owns this root widget */
        gui_window_t* window = g_desktop.window_list;
        while (window) {
            if (window->root_widget == root) {
                invalidate_window_rect(window, widget->bounds);
                break;
            }
            window = window->next;
        }
    }
}

void gui_move_widget(gui_widget_t* widget, gui_point_t position) {
    if (!widget) return;
    
    gui_rect_t old_bounds = widget->bounds;
    widget->bounds.x = position.x;
    widget->bounds.y = position.y;
    
    /* Invalidate old and new positions */
    if (widget->parent) {
        gui_widget_t* root = widget;
        while (root->parent) {
            root = root->parent;
        }
        
        gui_window_t* window = g_desktop.window_list;
        while (window) {
            if (window->root_widget == root) {
                invalidate_window_rect(window, old_bounds);
                invalidate_window_rect(window, widget->bounds);
                break;
            }
            window = window->next;
        }
    }
}

void gui_resize_widget(gui_widget_t* widget, gui_size_t size) {
    if (!widget) return;
    
    widget->bounds.width = size.width;
    widget->bounds.height = size.height;
    
    /* Invalidate widget area */
    if (widget->parent) {
        gui_widget_t* root = widget;
        while (root->parent) {
            root = root->parent;
        }
        
        gui_window_t* window = g_desktop.window_list;
        while (window) {
            if (window->root_widget == root) {
                invalidate_window_rect(window, widget->bounds);
                break;
            }
            window = window->next;
        }
    }
}

void gui_set_widget_text(gui_widget_t* widget, const char* text) {
    if (!widget) return;
    
    /* Free existing text */
    if (widget->text) {
        free(widget->text);
        widget->text = NULL;
    }
    
    /* Copy new text */
    if (text) {
        size_t len = strlen(text);
        widget->text = (char*)malloc(len + 1);
        if (widget->text) {
            strcpy(widget->text, text);
        }
    }
    
    /* Invalidate widget for redraw */
    gui_show_widget(widget, widget->visible); /* Trigger invalidation */
}

const char* gui_get_widget_text(gui_widget_t* widget) {
    return widget ? widget->text : NULL;
}

void gui_set_widget_colors(gui_widget_t* widget, gui_color_t fg, gui_color_t bg) {
    if (!widget) return;
    
    widget->foreground_color = fg;
    widget->background_color = bg;
    
    /* Invalidate widget for redraw */
    gui_show_widget(widget, widget->visible);
}

void gui_set_widget_enabled(gui_widget_t* widget, bool enabled) {
    if (!widget || widget->enabled == enabled) return;
    
    widget->enabled = enabled;
    
    /* Remove focus if disabling */
    if (!enabled && g_desktop.focused_widget == widget) {
        g_desktop.focused_widget = NULL;
    }
    
    /* Invalidate widget for redraw */
    gui_show_widget(widget, widget->visible);
}

void gui_set_widget_focus(gui_widget_t* widget) {
    if (!widget || !widget->enabled || g_desktop.focused_widget == widget) return;
    
    /* Remove focus from current widget */
    if (g_desktop.focused_widget) {
        g_desktop.focused_widget->focused = false;
        gui_show_widget(g_desktop.focused_widget, g_desktop.focused_widget->visible);
    }
    
    /* Set focus to new widget */
    g_desktop.focused_widget = widget;
    widget->focused = true;
    gui_show_widget(widget, widget->visible);
}

gui_widget_t* gui_get_focused_widget(void) {
    return g_desktop.focused_widget;
}

gui_widget_t* gui_find_widget_at_point(gui_window_t* window, gui_point_t point) {
    if (!window || !window->root_widget) return NULL;
    
    /* Convert point to window-relative coordinates */
    gui_point_t window_point = {point.x - window->bounds.x, point.y - window->bounds.y};
    
    return gui_find_widget_recursive(window->root_widget, window_point);
}

gui_widget_t* gui_find_widget_recursive(gui_widget_t* widget, gui_point_t point) {
    if (!widget || !widget->visible || !gui_rect_contains_point(widget->bounds, point)) {
        return NULL;
    }
    
    /* Check children first (they're in front) */
    gui_widget_t* child = widget->first_child;
    while (child) {
        gui_point_t child_point = {point.x - widget->bounds.x, point.y - widget->bounds.y};
        gui_widget_t* found = gui_find_widget_recursive(child, child_point);
        if (found) return found;
        child = child->next_sibling;
    }
    
    /* Return this widget if no children found */
    return widget;
}

/* ================================
 * Specific Widget Creation
 * ================================ */

gui_widget_t* gui_create_button(gui_rect_t bounds, const char* text, gui_widget_t* parent) {
    gui_widget_t* button = gui_create_widget(GUI_WIDGET_BUTTON, bounds, parent);
    if (!button) return NULL;
    
    gui_set_widget_text(button, text);
    button->background_color = GUI_COLOR_LIGHT_GRAY;
    button->widget_data.button.pressed = false;
    
    return button;
}

gui_widget_t* gui_create_label(gui_rect_t bounds, const char* text, gui_widget_t* parent) {
    gui_widget_t* label = gui_create_widget(GUI_WIDGET_LABEL, bounds, parent);
    if (!label) return NULL;
    
    gui_set_widget_text(label, text);
    label->background_color = GUI_COLOR_WHITE; /* Transparent would be better */
    
    return label;
}

gui_widget_t* gui_create_textbox(gui_rect_t bounds, const char* text, gui_widget_t* parent) {
    gui_widget_t* textbox = gui_create_widget(GUI_WIDGET_TEXTBOX, bounds, parent);
    if (!textbox) return NULL;
    
    textbox->background_color = GUI_COLOR_WHITE;
    textbox->widget_data.textbox.cursor_pos = 0;
    textbox->widget_data.textbox.selection_start = 0;
    textbox->widget_data.textbox.selection_end = 0;
    
    if (text) {
        size_t len = strlen(text);
        textbox->widget_data.textbox.content = (char*)malloc(len + 1);
        if (textbox->widget_data.textbox.content) {
            strcpy(textbox->widget_data.textbox.content, text);
            textbox->widget_data.textbox.cursor_pos = len;
        }
    }
    
    return textbox;
}

gui_widget_t* gui_create_checkbox(gui_rect_t bounds, const char* text, bool checked, gui_widget_t* parent) {
    gui_widget_t* checkbox = gui_create_widget(GUI_WIDGET_CHECKBOX, bounds, parent);
    if (!checkbox) return NULL;
    
    gui_set_widget_text(checkbox, text);
    checkbox->widget_data.checkbox.checked = checked;
    
    return checkbox;
}

gui_widget_t* gui_create_listbox(gui_rect_t bounds, gui_widget_t* parent) {
    gui_widget_t* listbox = gui_create_widget(GUI_WIDGET_LISTBOX, bounds, parent);
    if (!listbox) return NULL;
    
    listbox->background_color = GUI_COLOR_WHITE;
    listbox->widget_data.listbox.items = NULL;
    listbox->widget_data.listbox.item_count = 0;
    listbox->widget_data.listbox.selected_index = -1;
    
    return listbox;
}

gui_widget_t* gui_create_progressbar(gui_rect_t bounds, int32_t min, int32_t max, gui_widget_t* parent) {
    gui_widget_t* progressbar = gui_create_widget(GUI_WIDGET_PROGRESSBAR, bounds, parent);
    if (!progressbar) return NULL;
    
    progressbar->background_color = GUI_COLOR_LIGHT_GRAY;
    progressbar->widget_data.progressbar.min_value = min;
    progressbar->widget_data.progressbar.max_value = max;
    progressbar->widget_data.progressbar.current_value = min;
    
    return progressbar;
}

/* ================================
 * Widget Operations
 * ================================ */

void gui_button_set_pressed(gui_widget_t* button, bool pressed) {
    if (!button || button->type != GUI_WIDGET_BUTTON) return;
    
    button->widget_data.button.pressed = pressed;
    gui_show_widget(button, button->visible);
}

bool gui_button_is_pressed(gui_widget_t* button) {
    if (!button || button->type != GUI_WIDGET_BUTTON) return false;
    return button->widget_data.button.pressed;
}

void gui_checkbox_set_checked(gui_widget_t* checkbox, bool checked) {
    if (!checkbox || checkbox->type != GUI_WIDGET_CHECKBOX) return;
    
    checkbox->widget_data.checkbox.checked = checked;
    gui_show_widget(checkbox, checkbox->visible);
}

bool gui_checkbox_is_checked(gui_widget_t* checkbox) {
    if (!checkbox || checkbox->type != GUI_WIDGET_CHECKBOX) return false;
    return checkbox->widget_data.checkbox.checked;
}

void gui_textbox_set_cursor_pos(gui_widget_t* textbox, uint32_t pos) {
    if (!textbox || textbox->type != GUI_WIDGET_TEXTBOX) return;
    
    if (textbox->widget_data.textbox.content) {
        uint32_t len = strlen(textbox->widget_data.textbox.content);
        if (pos > len) pos = len;
    } else {
        pos = 0;
    }
    
    textbox->widget_data.textbox.cursor_pos = pos;
    gui_show_widget(textbox, textbox->visible);
}

uint32_t gui_textbox_get_cursor_pos(gui_widget_t* textbox) {
    if (!textbox || textbox->type != GUI_WIDGET_TEXTBOX) return 0;
    return textbox->widget_data.textbox.cursor_pos;
}

void gui_listbox_add_item(gui_widget_t* listbox, const char* item) {
    if (!listbox || listbox->type != GUI_WIDGET_LISTBOX || !item) return;
    
    uint32_t new_count = listbox->widget_data.listbox.item_count + 1;
    char** new_items = (char**)realloc(listbox->widget_data.listbox.items, 
                                       new_count * sizeof(char*));
    if (!new_items) return;
    
    listbox->widget_data.listbox.items = new_items;
    
    size_t len = strlen(item);
    listbox->widget_data.listbox.items[listbox->widget_data.listbox.item_count] = 
        (char*)malloc(len + 1);
    
    if (listbox->widget_data.listbox.items[listbox->widget_data.listbox.item_count]) {
        strcpy(listbox->widget_data.listbox.items[listbox->widget_data.listbox.item_count], item);
        listbox->widget_data.listbox.item_count = new_count;
        gui_show_widget(listbox, listbox->visible);
    }
}

void gui_listbox_remove_item(gui_widget_t* listbox, uint32_t index) {
    if (!listbox || listbox->type != GUI_WIDGET_LISTBOX) return;
    if (index >= listbox->widget_data.listbox.item_count) return;
    
    /* Free the item */
    free(listbox->widget_data.listbox.items[index]);
    
    /* Shift remaining items */
    for (uint32_t i = index; i < listbox->widget_data.listbox.item_count - 1; i++) {
        listbox->widget_data.listbox.items[i] = listbox->widget_data.listbox.items[i + 1];
    }
    
    listbox->widget_data.listbox.item_count--;
    
    /* Update selection if necessary */
    if (listbox->widget_data.listbox.selected_index == (int32_t)index) {
        listbox->widget_data.listbox.selected_index = -1;
    } else if (listbox->widget_data.listbox.selected_index > (int32_t)index) {
        listbox->widget_data.listbox.selected_index--;
    }
    
    gui_show_widget(listbox, listbox->visible);
}

void gui_listbox_set_selected(gui_widget_t* listbox, int32_t index) {
    if (!listbox || listbox->type != GUI_WIDGET_LISTBOX) return;
    if (index >= 0 && index >= (int32_t)listbox->widget_data.listbox.item_count) return;
    
    listbox->widget_data.listbox.selected_index = index;
    gui_show_widget(listbox, listbox->visible);
}

int32_t gui_listbox_get_selected(gui_widget_t* listbox) {
    if (!listbox || listbox->type != GUI_WIDGET_LISTBOX) return -1;
    return listbox->widget_data.listbox.selected_index;
}

void gui_progressbar_set_value(gui_widget_t* progressbar, int32_t value) {
    if (!progressbar || progressbar->type != GUI_WIDGET_PROGRESSBAR) return;
    
    /* Clamp value to range */
    if (value < progressbar->widget_data.progressbar.min_value) {
        value = progressbar->widget_data.progressbar.min_value;
    }
    if (value > progressbar->widget_data.progressbar.max_value) {
        value = progressbar->widget_data.progressbar.max_value;
    }
    
    progressbar->widget_data.progressbar.current_value = value;
    gui_show_widget(progressbar, progressbar->visible);
}

int32_t gui_progressbar_get_value(gui_widget_t* progressbar) {
    if (!progressbar || progressbar->type != GUI_WIDGET_PROGRESSBAR) return 0;
    return progressbar->widget_data.progressbar.current_value;
}

/* Continue in the next file with event system and rendering... */
