/*
 * IKOS Operating System - GUI System Core Implementation
 * Issue #37: Graphical User Interface Implementation
 *
 * Core GUI system providing window management, widgets, and event handling.
 * Integrates with the framebuffer driver for hardware graphics acceleration.
 */

#include "gui.h"
#include "gui_internal.h"
#include "framebuffer.h"
#include "memory.h"
#include "string.h"
#include "keyboard.h"
#include <stddef.h>

/* ================================
 * Global State
 * ================================ */

gui_desktop_t g_desktop;
static bool g_gui_initialized = false;
static uint32_t g_next_window_id = 1;
uint32_t g_next_widget_id = 1;

/* Widget and window storage */
static gui_window_t g_windows[GUI_MAX_WINDOWS];
gui_widget_t g_widgets[GUI_MAX_WIDGETS];
static bool g_window_slots[GUI_MAX_WINDOWS] = {false};
bool g_widget_slots[GUI_MAX_WIDGETS] = {false};

/* Graphics context */
gui_graphics_context_t g_graphics_ctx;

/* Graphics context */
static gui_graphics_context_t g_graphics_ctx;

/* ================================
 * Internal Helper Functions
 * ================================ */

gui_window_t* allocate_window(void) {
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (!g_window_slots[i]) {
            g_window_slots[i] = true;
            memset(&g_windows[i], 0, sizeof(gui_window_t));
            g_windows[i].id = g_next_window_id++;
            return &g_windows[i];
        }
    }
    return NULL;
}

void free_window(gui_window_t* window) {
    if (!window) return;
    
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (&g_windows[i] == window) {
            g_window_slots[i] = false;
            break;
        }
    }
}

gui_widget_t* allocate_widget(void) {
    for (int i = 0; i < GUI_MAX_WIDGETS; i++) {
        if (!g_widget_slots[i]) {
            g_widget_slots[i] = true;
            memset(&g_widgets[i], 0, sizeof(gui_widget_t));
            g_widgets[i].id = g_next_widget_id++;
            return &g_widgets[i];
        }
    }
    return NULL;
}

void free_widget(gui_widget_t* widget) {
    if (!widget) return;
    
    for (int i = 0; i < GUI_MAX_WIDGETS; i++) {
        if (&g_widgets[i] == widget) {
            g_widget_slots[i] = false;
            break;
        }
    }
}

void add_window_to_list(gui_window_t* window) {
    window->next = g_desktop.window_list;
    g_desktop.window_list = window;
    g_desktop.window_count++;
}

void remove_window_from_list(gui_window_t* window) {
    if (!window) return;
    
    if (g_desktop.window_list == window) {
        g_desktop.window_list = window->next;
    } else {
        gui_window_t* current = g_desktop.window_list;
        while (current && current->next != window) {
            current = current->next;
        }
        if (current) {
            current->next = window->next;
        }
    }
    g_desktop.window_count--;
}

void add_widget_to_parent(gui_widget_t* widget, gui_widget_t* parent) {
    if (!widget || !parent) return;
    
    widget->parent = parent;
    widget->next_sibling = parent->first_child;
    parent->first_child = widget;
}

void remove_widget_from_parent(gui_widget_t* widget) {
    if (!widget || !widget->parent) return;
    
    gui_widget_t* parent = widget->parent;
    if (parent->first_child == widget) {
        parent->first_child = widget->next_sibling;
    } else {
        gui_widget_t* current = parent->first_child;
        while (current && current->next_sibling != widget) {
            current = current->next_sibling;
        }
        if (current) {
            current->next_sibling = widget->next_sibling;
        }
    }
    widget->parent = NULL;
    widget->next_sibling = NULL;
}

void invalidate_window_rect(gui_window_t* window, gui_rect_t rect) {
    if (!window) return;
    
    if (!window->needs_redraw) {
        window->dirty_rect = rect;
        window->needs_redraw = true;
    } else {
        window->dirty_rect = gui_rect_union(window->dirty_rect, rect);
    }
}

/* ================================
 * Core GUI System
 * ================================ */

int gui_init(void) {
    if (g_gui_initialized) {
        return 0; /* Already initialized */
    }
    
    /* Initialize framebuffer */
    if (fb_init() != 0) {
        return -1;
    }
    
    /* Clear global state */
    memset(&g_desktop, 0, sizeof(gui_desktop_t));
    memset(g_windows, 0, sizeof(g_windows));
    memset(g_widgets, 0, sizeof(g_widgets));
    memset(g_window_slots, 0, sizeof(g_window_slots));
    memset(g_widget_slots, 0, sizeof(g_widget_slots));
    
    /* Initialize desktop */
    g_desktop.background_color = GUI_COLOR_LIGHT_GRAY;
    g_desktop.screen_bounds = gui_rect_make(0, 0, fb_get_width(), fb_get_height());
    g_desktop.show_taskbar = true;
    g_desktop.show_desktop_icons = true;
    g_desktop.cursor_visible = true;
    g_desktop.cursor_position = gui_point_make(fb_get_width() / 2, fb_get_height() / 2);
    
    /* Initialize graphics context */
    memset(&g_graphics_ctx, 0, sizeof(gui_graphics_context_t));
    g_graphics_ctx.foreground_color = GUI_COLOR_BLACK;
    g_graphics_ctx.background_color = GUI_COLOR_WHITE;
    g_graphics_ctx.font_size = GUI_DEFAULT_FONT_SIZE;
    
    g_gui_initialized = true;
    
    /* Clear screen with desktop background */
    fb_clear_screen();
    fb_fill_rect(0, 0, fb_get_width(), fb_get_height(), 
                 gui_color_make_rgb(192, 192, 192)); /* Light gray */
    
    return 0;
}

void gui_shutdown(void) {
    if (!g_gui_initialized) return;
    
    /* Destroy all windows */
    gui_window_t* window = g_desktop.window_list;
    while (window) {
        gui_window_t* next = window->next;
        gui_destroy_window(window);
        window = next;
    }
    
    /* Cleanup desktop state */
    memset(&g_desktop, 0, sizeof(gui_desktop_t));
    g_gui_initialized = false;
}

int gui_main_loop(void) {
    if (!g_gui_initialized) {
        return -1;
    }
    
    gui_event_t event;
    
    while (1) {
        /* Process events */
        while (gui_get_event(&event)) {
            /* Handle system events first */
            switch (event.type) {
                case GUI_EVENT_MOUSE_MOVE:
                    g_desktop.cursor_position = event.data.mouse.position;
                    break;
                    
                case GUI_EVENT_MOUSE_DOWN:
                case GUI_EVENT_MOUSE_UP:
                case GUI_EVENT_MOUSE_CLICK: {
                    /* Find window at mouse position */
                    gui_window_t* target = gui_find_window_at_point(event.data.mouse.position);
                    if (target && target != g_desktop.active_window) {
                        gui_set_active_window(target);
                    }
                    event.target = target;
                    break;
                }
                
                case GUI_EVENT_KEY_DOWN:
                case GUI_EVENT_KEY_UP:
                case GUI_EVENT_CHAR_INPUT:
                    event.target = g_desktop.active_window;
                    break;
                    
                default:
                    break;
            }
            
            /* Dispatch event to target */
            if (event.target) {
                gui_window_t* window = (gui_window_t*)event.target;
                if (window->event_handler) {
                    window->event_handler(&event, window->user_data);
                }
            }
            
            g_desktop.events_processed++;
        }
        
        /* Update and render */
        gui_update();
        gui_render();
        
        /* Yield CPU - in a real OS we'd wait for interrupts */
        /* For now, just continue */
    }
    
    return 0;
}

void gui_update(void) {
    /* Update window states, animations, etc. */
    gui_window_t* window = g_desktop.window_list;
    while (window) {
        /* Check if window needs updating */
        if (window->needs_redraw) {
            /* Update window contents */
        }
        window = window->next;
    }
}

void gui_render(void) {
    bool screen_dirty = false;
    
    /* Check if any window needs redrawing */
    gui_window_t* window = g_desktop.window_list;
    while (window) {
        if (window->needs_redraw && window->visible) {
            screen_dirty = true;
            break;
        }
        window = window->next;
    }
    
    if (!screen_dirty) {
        return; /* Nothing to render */
    }
    
    /* Clear background */
    fb_fill_rect(0, 0, fb_get_width(), fb_get_height(),
                 gui_color_make_rgb(192, 192, 192));
    
    /* Render windows from back to front (reverse z-order) */
    /* For simplicity, we'll render in creation order for now */
    window = g_desktop.window_list;
    while (window) {
        if (window->visible) {
            gui_render_window(window);
            window->needs_redraw = false;
        }
        window = window->next;
    }
    
    /* Render cursor */
    if (g_desktop.cursor_visible) {
        gui_render_cursor();
    }
    
    /* Swap buffers if double buffering is available */
    fb_swap_buffers();
    
    g_desktop.frames_rendered++;
}

/* ================================
 * Desktop Management
 * ================================ */

gui_desktop_t* gui_get_desktop(void) {
    return &g_desktop;
}

void gui_set_wallpaper(const char* path) {
    if (g_desktop.wallpaper_path) {
        /* Free existing wallpaper path */
        g_desktop.wallpaper_path = NULL;
    }
    
    if (path) {
        /* Allocate and copy new path */
        size_t len = strlen(path);
        g_desktop.wallpaper_path = (char*)malloc(len + 1);
        if (g_desktop.wallpaper_path) {
            strcpy(g_desktop.wallpaper_path, path);
        }
    }
    
    gui_invalidate_screen();
}

void gui_show_taskbar(bool show) {
    g_desktop.show_taskbar = show;
    gui_invalidate_screen();
}

void gui_invalidate_screen(void) {
    /* Mark all visible windows as needing redraw */
    gui_window_t* window = g_desktop.window_list;
    while (window) {
        if (window->visible) {
            invalidate_window_rect(window, window->bounds);
        }
        window = window->next;
    }
}

/* ================================
 * Window Management
 * ================================ */

gui_window_t* gui_create_window(const char* title, gui_rect_t bounds, gui_window_type_t type) {
    if (!g_gui_initialized) return NULL;
    
    gui_window_t* window = allocate_window();
    if (!window) return NULL;
    
    /* Initialize window */
    window->type = type;
    window->state = GUI_WINDOW_STATE_NORMAL;
    window->bounds = bounds;
    window->restored_bounds = bounds;
    window->visible = false;
    window->resizable = true;
    window->movable = true;
    window->closable = true;
    window->minimizable = true;
    window->maximizable = true;
    window->z_order = 0;
    window->needs_redraw = true;
    window->dirty_rect = bounds;
    
    /* Copy title */
    if (title) {
        size_t len = strlen(title);
        window->title = (char*)malloc(len + 1);
        if (window->title) {
            strcpy(window->title, title);
        }
    }
    
    /* Create root widget */
    window->root_widget = allocate_widget();
    if (window->root_widget) {
        window->root_widget->type = GUI_WIDGET_PANEL;
        window->root_widget->bounds = gui_rect_make(0, 0, bounds.width, bounds.height);
        window->root_widget->visible = true;
        window->root_widget->enabled = true;
        window->root_widget->background_color = GUI_COLOR_LIGHT_GRAY;
        window->root_widget->foreground_color = GUI_COLOR_BLACK;
    }
    
    /* Allocate back buffer for window */
    window->back_buffer = (fb_color_t*)malloc(bounds.width * bounds.height * sizeof(fb_color_t));
    
    /* Add to window list */
    add_window_to_list(window);
    
    return window;
}

void gui_destroy_window(gui_window_t* window) {
    if (!window) return;
    
    /* Remove from active window if it was active */
    if (g_desktop.active_window == window) {
        g_desktop.active_window = NULL;
    }
    
    /* Destroy all child widgets */
    if (window->root_widget) {
        gui_destroy_widget(window->root_widget);
    }
    
    /* Free resources */
    if (window->title) {
        free(window->title);
    }
    if (window->back_buffer) {
        free(window->back_buffer);
    }
    
    /* Remove from window list */
    remove_window_from_list(window);
    
    /* Free window slot */
    free_window(window);
    
    /* Redraw screen */
    gui_invalidate_screen();
}

void gui_show_window(gui_window_t* window, bool show) {
    if (!window || window->visible == show) return;
    
    window->visible = show;
    if (show) {
        window->needs_redraw = true;
        invalidate_window_rect(window, window->bounds);
        gui_set_active_window(window);
    } else {
        gui_invalidate_screen(); /* Need to redraw what was behind it */
    }
}

void gui_move_window(gui_window_t* window, gui_point_t position) {
    if (!window || !window->movable) return;
    
    gui_rect_t old_bounds = window->bounds;
    window->bounds.x = position.x;
    window->bounds.y = position.y;
    
    /* Invalidate old and new positions */
    gui_invalidate_screen();
    invalidate_window_rect(window, window->bounds);
}

void gui_resize_window(gui_window_t* window, gui_size_t size) {
    if (!window || !window->resizable) return;
    
    /* Update window bounds */
    window->bounds.width = size.width;
    window->bounds.height = size.height;
    
    /* Update root widget bounds */
    if (window->root_widget) {
        window->root_widget->bounds.width = size.width;
        window->root_widget->bounds.height = size.height;
    }
    
    /* Reallocate back buffer */
    if (window->back_buffer) {
        free(window->back_buffer);
        window->back_buffer = (fb_color_t*)malloc(size.width * size.height * sizeof(fb_color_t));
    }
    
    /* Mark for redraw */
    invalidate_window_rect(window, window->bounds);
    gui_invalidate_screen();
}

void gui_set_window_title(gui_window_t* window, const char* title) {
    if (!window) return;
    
    /* Free existing title */
    if (window->title) {
        free(window->title);
        window->title = NULL;
    }
    
    /* Copy new title */
    if (title) {
        size_t len = strlen(title);
        window->title = (char*)malloc(len + 1);
        if (window->title) {
            strcpy(window->title, title);
        }
    }
    
    /* Redraw title bar */
    gui_rect_t title_bar = {window->bounds.x, window->bounds.y, 
                           window->bounds.width, GUI_TITLE_BAR_HEIGHT};
    invalidate_window_rect(window, title_bar);
}

void gui_set_window_state(gui_window_t* window, gui_window_state_t state) {
    if (!window || window->state == state) return;
    
    gui_window_state_t old_state = window->state;
    window->state = state;
    
    switch (state) {
        case GUI_WINDOW_STATE_NORMAL:
            if (old_state == GUI_WINDOW_STATE_MAXIMIZED || old_state == GUI_WINDOW_STATE_FULLSCREEN) {
                window->bounds = window->restored_bounds;
            }
            break;
            
        case GUI_WINDOW_STATE_MAXIMIZED:
            if (old_state == GUI_WINDOW_STATE_NORMAL) {
                window->restored_bounds = window->bounds;
            }
            window->bounds = gui_rect_make(0, 0, g_desktop.screen_bounds.width, 
                                         g_desktop.screen_bounds.height - GUI_TASKBAR_HEIGHT);
            break;
            
        case GUI_WINDOW_STATE_FULLSCREEN:
            if (old_state == GUI_WINDOW_STATE_NORMAL) {
                window->restored_bounds = window->bounds;
            }
            window->bounds = g_desktop.screen_bounds;
            break;
            
        case GUI_WINDOW_STATE_MINIMIZED:
            /* Hide window */
            gui_show_window(window, false);
            return;
    }
    
    /* Update root widget bounds */
    if (window->root_widget) {
        window->root_widget->bounds.width = window->bounds.width;
        window->root_widget->bounds.height = window->bounds.height;
    }
    
    /* Reallocate back buffer */
    if (window->back_buffer) {
        free(window->back_buffer);
        window->back_buffer = (fb_color_t*)malloc(window->bounds.width * window->bounds.height * sizeof(fb_color_t));
    }
    
    invalidate_window_rect(window, window->bounds);
    gui_invalidate_screen();
}

void gui_bring_window_to_front(gui_window_t* window) {
    if (!window) return;
    
    /* Remove from current position */
    remove_window_from_list(window);
    
    /* Add to front of list */
    add_window_to_list(window);
    
    /* Set as active */
    gui_set_active_window(window);
}

void gui_set_active_window(gui_window_t* window) {
    if (g_desktop.active_window == window) return;
    
    /* Deactivate current window */
    if (g_desktop.active_window) {
        gui_window_t* old_window = g_desktop.active_window;
        /* Redraw title bar to show inactive state */
        gui_rect_t title_bar = {old_window->bounds.x, old_window->bounds.y,
                               old_window->bounds.width, GUI_TITLE_BAR_HEIGHT};
        invalidate_window_rect(old_window, title_bar);
    }
    
    /* Activate new window */
    g_desktop.active_window = window;
    
    if (window) {
        /* Bring to front */
        gui_bring_window_to_front(window);
        
        /* Redraw title bar to show active state */
        gui_rect_t title_bar = {window->bounds.x, window->bounds.y,
                               window->bounds.width, GUI_TITLE_BAR_HEIGHT};
        invalidate_window_rect(window, title_bar);
    }
}

gui_window_t* gui_get_active_window(void) {
    return g_desktop.active_window;
}

gui_window_t* gui_find_window_at_point(gui_point_t point) {
    /* Search from front to back */
    gui_window_t* window = g_desktop.window_list;
    while (window) {
        if (window->visible && gui_rect_contains_point(window->bounds, point)) {
            return window;
        }
        window = window->next;
    }
    return NULL;
}

/* Continue in next file... */
