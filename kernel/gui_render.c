/*
 * IKOS Operating System - GUI Event System and Rendering
 * Issue #37: Graphical User Interface Implementation
 *
 * Event handling, graphics operations, and widget rendering.
 */

#include "gui.h"
#include "gui_internal.h"
#include "framebuffer.h"
#include "memory.h"
#include "string.h"
#include "keyboard.h"

/* External references */
extern gui_desktop_t g_desktop;
extern gui_graphics_context_t g_graphics_ctx;

/* ================================
 * Event System
 * ================================ */

void gui_post_event(gui_event_t* event) {
    if (!event) return;
    
    uint32_t next_tail = (g_desktop.event_queue_tail + 1) % GUI_MAX_EVENT_QUEUE;
    if (next_tail == g_desktop.event_queue_head) {
        /* Queue is full, drop event */
        return;
    }
    
    g_desktop.event_queue[g_desktop.event_queue_tail] = *event;
    g_desktop.event_queue_tail = next_tail;
}

bool gui_get_event(gui_event_t* event) {
    if (!event) return false;
    
    if (g_desktop.event_queue_head == g_desktop.event_queue_tail) {
        /* Queue is empty */
        return false;
    }
    
    *event = g_desktop.event_queue[g_desktop.event_queue_head];
    g_desktop.event_queue_head = (g_desktop.event_queue_head + 1) % GUI_MAX_EVENT_QUEUE;
    
    return true;
}

void gui_set_event_handler(gui_widget_t* widget, gui_event_handler_t handler, void* user_data) {
    if (!widget) return;
    
    widget->event_handler = handler;
    widget->user_data = user_data;
}

void gui_set_window_event_handler(gui_window_t* window, gui_event_handler_t handler, void* user_data) {
    if (!window) return;
    
    window->event_handler = handler;
    window->user_data = user_data;
}

/* ================================
 * Graphics Operations
 * ================================ */

gui_graphics_context_t* gui_get_graphics_context(gui_window_t* window) {
    if (!window) return NULL;
    
    g_graphics_ctx.target_window = window;
    g_graphics_ctx.clip_rect = gui_rect_make(0, 0, window->bounds.width, window->bounds.height);
    
    return &g_graphics_ctx;
}

void gui_clear_rect(gui_graphics_context_t* ctx, gui_rect_t rect, gui_color_t color) {
    if (!ctx || !ctx->target_window) return;
    
    gui_fill_rect(ctx, rect, color);
}

void gui_draw_pixel(gui_graphics_context_t* ctx, gui_point_t point, gui_color_t color) {
    if (!ctx || !ctx->target_window) return;
    
    gui_window_t* window = ctx->target_window;
    
    /* Check bounds */
    if (point.x < 0 || point.y < 0 || 
        point.x >= (int32_t)window->bounds.width || 
        point.y >= (int32_t)window->bounds.height) {
        return;
    }
    
    /* Check clipping */
    if (!gui_rect_contains_point(ctx->clip_rect, point)) {
        return;
    }
    
    /* Draw to window back buffer if available */
    if (window->back_buffer) {
        uint32_t offset = point.y * window->bounds.width + point.x;
        window->back_buffer[offset] = color;
    }
    
    /* Also draw directly to framebuffer */
    fb_draw_pixel(window->bounds.x + point.x, window->bounds.y + point.y, color);
}

void gui_draw_line(gui_graphics_context_t* ctx, gui_point_t start, gui_point_t end, gui_color_t color) {
    if (!ctx) return;
    
    /* Bresenham's line algorithm */
    int32_t dx = abs(end.x - start.x);
    int32_t dy = abs(end.y - start.y);
    int32_t sx = (start.x < end.x) ? 1 : -1;
    int32_t sy = (start.y < end.y) ? 1 : -1;
    int32_t err = dx - dy;
    
    gui_point_t current = start;
    
    while (true) {
        gui_draw_pixel(ctx, current, color);
        
        if (current.x == end.x && current.y == end.y) break;
        
        int32_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            current.x += sx;
        }
        if (e2 < dx) {
            err += dx;
            current.y += sy;
        }
    }
}

void gui_draw_rect(gui_graphics_context_t* ctx, gui_rect_t rect, gui_color_t color) {
    if (!ctx) return;
    
    /* Draw four lines for rectangle outline */
    gui_point_t top_left = {rect.x, rect.y};
    gui_point_t top_right = {rect.x + (int32_t)rect.width - 1, rect.y};
    gui_point_t bottom_left = {rect.x, rect.y + (int32_t)rect.height - 1};
    gui_point_t bottom_right = {rect.x + (int32_t)rect.width - 1, rect.y + (int32_t)rect.height - 1};
    
    gui_draw_line(ctx, top_left, top_right, color);      /* Top */
    gui_draw_line(ctx, top_right, bottom_right, color);  /* Right */
    gui_draw_line(ctx, bottom_right, bottom_left, color); /* Bottom */
    gui_draw_line(ctx, bottom_left, top_left, color);    /* Left */
}

void gui_fill_rect(gui_graphics_context_t* ctx, gui_rect_t rect, gui_color_t color) {
    if (!ctx) return;
    
    for (int32_t y = rect.y; y < rect.y + (int32_t)rect.height; y++) {
        for (int32_t x = rect.x; x < rect.x + (int32_t)rect.width; x++) {
            gui_draw_pixel(ctx, gui_point_make(x, y), color);
        }
    }
}

void gui_draw_circle(gui_graphics_context_t* ctx, gui_point_t center, uint32_t radius, gui_color_t color) {
    if (!ctx) return;
    
    /* Midpoint circle algorithm */
    int32_t x = radius;
    int32_t y = 0;
    int32_t err = 0;
    
    while (x >= y) {
        gui_draw_pixel(ctx, gui_point_make(center.x + x, center.y + y), color);
        gui_draw_pixel(ctx, gui_point_make(center.x + y, center.y + x), color);
        gui_draw_pixel(ctx, gui_point_make(center.x - y, center.y + x), color);
        gui_draw_pixel(ctx, gui_point_make(center.x - x, center.y + y), color);
        gui_draw_pixel(ctx, gui_point_make(center.x - x, center.y - y), color);
        gui_draw_pixel(ctx, gui_point_make(center.x - y, center.y - x), color);
        gui_draw_pixel(ctx, gui_point_make(center.x + y, center.y - x), color);
        gui_draw_pixel(ctx, gui_point_make(center.x + x, center.y - y), color);
        
        if (err <= 0) {
            y += 1;
            err += 2*y + 1;
        }
        
        if (err > 0) {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}

void gui_fill_circle(gui_graphics_context_t* ctx, gui_point_t center, uint32_t radius, gui_color_t color) {
    if (!ctx) return;
    
    for (int32_t y = -((int32_t)radius); y <= (int32_t)radius; y++) {
        for (int32_t x = -((int32_t)radius); x <= (int32_t)radius; x++) {
            if (x*x + y*y <= (int32_t)(radius*radius)) {
                gui_draw_pixel(ctx, gui_point_make(center.x + x, center.y + y), color);
            }
        }
    }
}

void gui_draw_text(gui_graphics_context_t* ctx, gui_point_t position, const char* text) {
    if (!ctx || !text) return;
    
    gui_point_t current_pos = position;
    
    while (*text) {
        gui_draw_char(ctx, current_pos, *text);
        current_pos.x += 8; /* Assume 8-pixel wide characters */
        text++;
    }
}

void gui_draw_char(gui_graphics_context_t* ctx, gui_point_t position, char c) {
    if (!ctx) return;
    
    /* Use framebuffer's character rendering if available */
    if (ctx->target_window) {
        fb_draw_char(ctx->target_window->bounds.x + position.x,
                     ctx->target_window->bounds.y + position.y,
                     c, ctx->foreground_color);
    }
}

/* ================================
 * Window and Widget Rendering
 * ================================ */

void gui_render_window(gui_window_t* window) {
    if (!window || !window->visible) return;
    
    gui_graphics_context_t* ctx = gui_get_graphics_context(window);
    if (!ctx) return;
    
    /* Clear window background */
    gui_rect_t client_area = {0, 0, window->bounds.width, window->bounds.height};
    gui_clear_rect(ctx, client_area, GUI_COLOR_LIGHT_GRAY);
    
    /* Draw title bar */
    if (window->type == GUI_WINDOW_NORMAL) {
        gui_render_title_bar(window, ctx);
    }
    
    /* Draw window border */
    gui_render_window_border(window, ctx);
    
    /* Render root widget and all children */
    if (window->root_widget) {
        gui_render_widget(window->root_widget, ctx);
    }
}

void gui_render_title_bar(gui_window_t* window, gui_graphics_context_t* ctx) {
    if (!window || !ctx) return;
    
    /* Title bar background */
    gui_color_t title_bg = (window == g_desktop.active_window) ? 
                          GUI_COLOR_BLUE : GUI_COLOR_GRAY;
    
    gui_rect_t title_bar = {0, 0, window->bounds.width, GUI_TITLE_BAR_HEIGHT};
    gui_fill_rect(ctx, title_bar, title_bg);
    
    /* Title text */
    if (window->title) {
        gui_color_t old_fg = ctx->foreground_color;
        ctx->foreground_color = GUI_COLOR_WHITE;
        gui_draw_text(ctx, gui_point_make(5, 5), window->title);
        ctx->foreground_color = old_fg;
    }
    
    /* Window control buttons (close, minimize, maximize) */
    if (window->closable) {
        gui_rect_t close_btn = {(int32_t)window->bounds.width - 20, 2, 16, 16};
        gui_fill_rect(ctx, close_btn, GUI_COLOR_RED);
        gui_draw_text(ctx, gui_point_make(close_btn.x + 4, close_btn.y + 2), "X");
    }
}

void gui_render_window_border(gui_window_t* window, gui_graphics_context_t* ctx) {
    if (!window || !ctx) return;
    
    gui_color_t border_color = (window == g_desktop.active_window) ?
                              GUI_COLOR_DARK_GRAY : GUI_COLOR_GRAY;
    
    /* Draw border around entire window */
    gui_rect_t border = {0, 0, window->bounds.width, window->bounds.height};
    gui_draw_rect(ctx, border, border_color);
    
    /* Draw separator between title bar and client area */
    if (window->type == GUI_WINDOW_NORMAL) {
        gui_point_t start = {0, GUI_TITLE_BAR_HEIGHT};
        gui_point_t end = {(int32_t)window->bounds.width, GUI_TITLE_BAR_HEIGHT};
        gui_draw_line(ctx, start, end, border_color);
    }
}

void gui_render_widget(gui_widget_t* widget, gui_graphics_context_t* ctx) {
    if (!widget || !widget->visible || !ctx) return;
    
    /* Set clipping to widget bounds */
    gui_rect_t old_clip = ctx->clip_rect;
    ctx->clip_rect = gui_rect_intersection(ctx->clip_rect, widget->bounds);
    
    /* Render widget based on type */
    switch (widget->type) {
        case GUI_WIDGET_BUTTON:
            gui_render_button(widget, ctx);
            break;
        case GUI_WIDGET_LABEL:
            gui_render_label(widget, ctx);
            break;
        case GUI_WIDGET_TEXTBOX:
            gui_render_textbox(widget, ctx);
            break;
        case GUI_WIDGET_CHECKBOX:
            gui_render_checkbox(widget, ctx);
            break;
        case GUI_WIDGET_LISTBOX:
            gui_render_listbox(widget, ctx);
            break;
        case GUI_WIDGET_PROGRESSBAR:
            gui_render_progressbar(widget, ctx);
            break;
        case GUI_WIDGET_PANEL:
            gui_render_panel(widget, ctx);
            break;
        default:
            /* Generic widget rendering */
            gui_fill_rect(ctx, widget->bounds, widget->background_color);
            if (widget->text) {
                gui_color_t old_fg = ctx->foreground_color;
                ctx->foreground_color = widget->foreground_color;
                gui_draw_text(ctx, gui_point_make(widget->bounds.x + 2, widget->bounds.y + 2), 
                             widget->text);
                ctx->foreground_color = old_fg;
            }
            break;
    }
    
    /* Render child widgets */
    gui_widget_t* child = widget->first_child;
    while (child) {
        gui_render_widget(child, ctx);
        child = child->next_sibling;
    }
    
    /* Restore clipping */
    ctx->clip_rect = old_clip;
}

void gui_render_button(gui_widget_t* button, gui_graphics_context_t* ctx) {
    if (!button || !ctx) return;
    
    gui_color_t bg_color = button->background_color;
    if (button->widget_data.button.pressed) {
        bg_color = GUI_COLOR_DARK_GRAY;
    } else if (!button->enabled) {
        bg_color = GUI_COLOR_GRAY;
    }
    
    /* Fill background */
    gui_fill_rect(ctx, button->bounds, bg_color);
    
    /* Draw border */
    gui_color_t border_color = button->focused ? GUI_COLOR_BLUE : GUI_COLOR_BLACK;
    gui_draw_rect(ctx, button->bounds, border_color);
    
    /* Draw text */
    if (button->text) {
        gui_color_t text_color = button->enabled ? button->foreground_color : GUI_COLOR_DARK_GRAY;
        gui_color_t old_fg = ctx->foreground_color;
        ctx->foreground_color = text_color;
        
        /* Center text in button */
        uint32_t text_width = gui_text_width(button->text, ctx->font_size);
        uint32_t text_height = gui_text_height(ctx->font_size);
        gui_point_t text_pos = {
            button->bounds.x + ((int32_t)button->bounds.width - (int32_t)text_width) / 2,
            button->bounds.y + ((int32_t)button->bounds.height - (int32_t)text_height) / 2
        };
        
        gui_draw_text(ctx, text_pos, button->text);
        ctx->foreground_color = old_fg;
    }
}

void gui_render_label(gui_widget_t* label, gui_graphics_context_t* ctx) {
    if (!label || !ctx) return;
    
    /* Labels typically don't have background unless specified */
    if (label->background_color != GUI_COLOR_WHITE) { /* Assuming white means transparent */
        gui_fill_rect(ctx, label->bounds, label->background_color);
    }
    
    /* Draw text */
    if (label->text) {
        gui_color_t old_fg = ctx->foreground_color;
        ctx->foreground_color = label->foreground_color;
        gui_draw_text(ctx, gui_point_make(label->bounds.x + 2, label->bounds.y + 2), label->text);
        ctx->foreground_color = old_fg;
    }
}

void gui_render_textbox(gui_widget_t* textbox, gui_graphics_context_t* ctx) {
    if (!textbox || !ctx) return;
    
    /* Fill background */
    gui_color_t bg_color = textbox->enabled ? textbox->background_color : GUI_COLOR_LIGHT_GRAY;
    gui_fill_rect(ctx, textbox->bounds, bg_color);
    
    /* Draw border */
    gui_color_t border_color = textbox->focused ? GUI_COLOR_BLUE : GUI_COLOR_BLACK;
    gui_draw_rect(ctx, textbox->bounds, border_color);
    
    /* Draw text content */
    if (textbox->widget_data.textbox.content) {
        gui_color_t old_fg = ctx->foreground_color;
        ctx->foreground_color = textbox->foreground_color;
        gui_draw_text(ctx, gui_point_make(textbox->bounds.x + 3, textbox->bounds.y + 3),
                     textbox->widget_data.textbox.content);
        ctx->foreground_color = old_fg;
    }
    
    /* Draw cursor if focused */
    if (textbox->focused) {
        uint32_t cursor_x = textbox->bounds.x + 3;
        if (textbox->widget_data.textbox.content) {
            /* Calculate cursor position based on text */
            cursor_x += textbox->widget_data.textbox.cursor_pos * 8; /* Assume 8-pixel wide chars */
        }
        
        gui_point_t cursor_start = {(int32_t)cursor_x, textbox->bounds.y + 2};
        gui_point_t cursor_end = {(int32_t)cursor_x, textbox->bounds.y + (int32_t)textbox->bounds.height - 3};
        gui_draw_line(ctx, cursor_start, cursor_end, GUI_COLOR_BLACK);
    }
}

void gui_render_checkbox(gui_widget_t* checkbox, gui_graphics_context_t* ctx) {
    if (!checkbox || !ctx) return;
    
    /* Draw checkbox square */
    gui_rect_t box = {checkbox->bounds.x + 2, checkbox->bounds.y + 2, 12, 12};
    gui_fill_rect(ctx, box, GUI_COLOR_WHITE);
    gui_draw_rect(ctx, box, GUI_COLOR_BLACK);
    
    /* Draw checkmark if checked */
    if (checkbox->widget_data.checkbox.checked) {
        gui_point_t check1 = {box.x + 2, box.y + 6};
        gui_point_t check2 = {box.x + 5, box.y + 9};
        gui_point_t check3 = {box.x + 10, box.y + 3};
        
        gui_draw_line(ctx, check1, check2, GUI_COLOR_BLACK);
        gui_draw_line(ctx, check2, check3, GUI_COLOR_BLACK);
    }
    
    /* Draw label text */
    if (checkbox->text) {
        gui_color_t old_fg = ctx->foreground_color;
        ctx->foreground_color = checkbox->foreground_color;
        gui_draw_text(ctx, gui_point_make(checkbox->bounds.x + 18, checkbox->bounds.y + 2), 
                     checkbox->text);
        ctx->foreground_color = old_fg;
    }
}

void gui_render_listbox(gui_widget_t* listbox, gui_graphics_context_t* ctx) {
    if (!listbox || !ctx) return;
    
    /* Fill background */
    gui_fill_rect(ctx, listbox->bounds, listbox->background_color);
    
    /* Draw border */
    gui_draw_rect(ctx, listbox->bounds, GUI_COLOR_BLACK);
    
    /* Draw items */
    if (listbox->widget_data.listbox.items) {
        gui_color_t old_fg = ctx->foreground_color;
        ctx->foreground_color = listbox->foreground_color;
        
        uint32_t item_height = 16; /* Fixed item height */
        uint32_t visible_items = (listbox->bounds.height - 4) / item_height;
        
        for (uint32_t i = 0; i < listbox->widget_data.listbox.item_count && i < visible_items; i++) {
            gui_point_t item_pos = {
                listbox->bounds.x + 3,
                listbox->bounds.y + 3 + (int32_t)(i * item_height)
            };
            
            /* Highlight selected item */
            if (listbox->widget_data.listbox.selected_index == (int32_t)i) {
                gui_rect_t highlight = {
                    listbox->bounds.x + 1,
                    item_pos.y - 1,
                    listbox->bounds.width - 2,
                    item_height
                };
                gui_fill_rect(ctx, highlight, GUI_COLOR_BLUE);
                ctx->foreground_color = GUI_COLOR_WHITE;
            }
            
            gui_draw_text(ctx, item_pos, listbox->widget_data.listbox.items[i]);
            
            /* Restore color */
            if (listbox->widget_data.listbox.selected_index == (int32_t)i) {
                ctx->foreground_color = listbox->foreground_color;
            }
        }
        
        ctx->foreground_color = old_fg;
    }
}

void gui_render_progressbar(gui_widget_t* progressbar, gui_graphics_context_t* ctx) {
    if (!progressbar || !ctx) return;
    
    /* Fill background */
    gui_fill_rect(ctx, progressbar->bounds, progressbar->background_color);
    
    /* Draw border */
    gui_draw_rect(ctx, progressbar->bounds, GUI_COLOR_BLACK);
    
    /* Calculate progress */
    int32_t range = progressbar->widget_data.progressbar.max_value - 
                   progressbar->widget_data.progressbar.min_value;
    if (range > 0) {
        int32_t progress = progressbar->widget_data.progressbar.current_value - 
                          progressbar->widget_data.progressbar.min_value;
        uint32_t fill_width = ((uint64_t)progress * (progressbar->bounds.width - 4)) / range;
        
        gui_rect_t fill_rect = {
            progressbar->bounds.x + 2,
            progressbar->bounds.y + 2,
            fill_width,
            progressbar->bounds.height - 4
        };
        
        gui_fill_rect(ctx, fill_rect, GUI_COLOR_BLUE);
    }
}

void gui_render_panel(gui_widget_t* panel, gui_graphics_context_t* ctx) {
    if (!panel || !ctx) return;
    
    /* Fill background */
    gui_fill_rect(ctx, panel->bounds, panel->background_color);
    
    /* Optionally draw border */
    /* gui_draw_rect(ctx, panel->bounds, GUI_COLOR_GRAY); */
}

void gui_render_cursor(void) {
    if (!g_desktop.cursor_visible) return;
    
    gui_point_t pos = g_desktop.cursor_position;
    
    /* Simple arrow cursor */
    fb_draw_pixel(pos.x, pos.y, GUI_COLOR_BLACK);
    fb_draw_pixel(pos.x + 1, pos.y + 1, GUI_COLOR_BLACK);
    fb_draw_pixel(pos.x + 2, pos.y + 2, GUI_COLOR_BLACK);
    fb_draw_pixel(pos.x, pos.y + 1, GUI_COLOR_BLACK);
    fb_draw_pixel(pos.x, pos.y + 2, GUI_COLOR_BLACK);
    fb_draw_pixel(pos.x + 1, pos.y + 2, GUI_COLOR_BLACK);
}

/* Continue with utility functions and input handling in the next file... */
