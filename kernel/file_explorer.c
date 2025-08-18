/* IKOS File Explorer Implementation - Issue #41
 * Graphical file manager with VFS integration and GUI interface
 */

#include "file_explorer.h"
#include "gui.h"
#include "vfs.h"
//#include "app_loader.h"  // TODO: Fix header conflicts
#include "memory.h"
#include "string.h"
#include "kernel_log.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* Simple string function stubs for missing functions */
static const char* strrchr_impl(const char* str, int c) {
    const char* last = NULL;
    while (*str) {
        if (*str == c) last = str;
        str++;
    }
    return last;
}

static char* strcat_impl(char* dest, const char* src) {
    char* ptr = dest + strlen(dest);
    while (*src) {
        *ptr++ = *src++;
    }
    *ptr = '\0';
    return dest;
}

static int snprintf_impl(char* buffer, size_t size, const char* format, ...) {
    /* Simple implementation - just copy a simple string for now */
    if (buffer && size > 0) {
        strncpy(buffer, "File Item", size - 1);
        buffer[size - 1] = '\0';
        return strlen(buffer);
    }
    return 0;
}

/* Simple logging macros if not available */
#define KLOG_INFO(cat, fmt, ...) do { } while(0)
#define KLOG_WARN(cat, fmt, ...) do { } while(0) 
#define KLOG_ERROR(cat, fmt, ...) do { } while(0)
#define KLOG_DEBUG(cat, fmt, ...) do { } while(0)

/* ================================
 * Global State
 * ================================ */

static bool g_file_explorer_initialized = false;
static file_explorer_config_t g_config;
static file_explorer_stats_t g_stats;

/* Window registry */
static file_explorer_window_t* g_windows[16];
static bool g_window_slots[16] = {false};
static uint32_t g_window_count = 0;

/* File type associations */
static const char* g_file_type_icons[] = {
    [FILE_TYPE_UNKNOWN] = "?",
    [FILE_TYPE_DIRECTORY] = "D",
    [FILE_TYPE_TEXT] = "T",
    [FILE_TYPE_EXECUTABLE] = "E",
    [FILE_TYPE_IMAGE] = "I",
    [FILE_TYPE_ARCHIVE] = "A",
    [FILE_TYPE_SYSTEM] = "S"
};

static const char* g_file_type_descriptions[] = {
    [FILE_TYPE_UNKNOWN] = "Unknown File",
    [FILE_TYPE_DIRECTORY] = "Directory",
    [FILE_TYPE_TEXT] = "Text File",
    [FILE_TYPE_EXECUTABLE] = "Executable File",
    [FILE_TYPE_IMAGE] = "Image File",
    [FILE_TYPE_ARCHIVE] = "Archive File",
    [FILE_TYPE_SYSTEM] = "System File"
};

/* ================================
 * Internal Helper Functions
 * ================================ */

static file_explorer_window_t* allocate_window(void);
static void free_window(file_explorer_window_t* window);
static void file_explorer_create_toolbar(file_explorer_window_t* window);
static void file_explorer_create_content_area(file_explorer_window_t* window);
static void file_explorer_create_status_bar(file_explorer_window_t* window);
static void file_explorer_create_context_menu(file_explorer_window_t* window);
static void file_explorer_populate_file_list(file_explorer_window_t* window);
static void file_explorer_update_navigation_buttons(file_explorer_window_t* window);

/* ================================
 * Core File Explorer Functions
 * ================================ */

int file_explorer_init(file_explorer_config_t* config) {
    if (g_file_explorer_initialized) {
        KLOG_WARN(LOG_CAT_KERNEL, "File Explorer already initialized");
        return FILE_EXPLORER_SUCCESS;
    }
    
    KLOG_INFO(LOG_CAT_KERNEL, "Initializing File Explorer");
    
    /* Set default configuration if none provided */
    if (config) {
        g_config = *config;
    } else {
        /* Default configuration */
        strncpy(g_config.default_path, "/", sizeof(g_config.default_path) - 1);
        g_config.default_view_mode = FILE_VIEW_LIST;
        g_config.show_hidden_files = false;
        g_config.show_details_panel = true;
        g_config.show_sidebar = true;
        g_config.window_width = FILE_EXPLORER_DEFAULT_WIDTH;
        g_config.window_height = FILE_EXPLORER_DEFAULT_HEIGHT;
        g_config.enable_file_preview = false;
        g_config.enable_thumbnails = false;
    }
    
    /* Initialize statistics */
    memset(&g_stats, 0, sizeof(g_stats));
    
    /* Clear window registry */
    memset(g_windows, 0, sizeof(g_windows));
    memset(g_window_slots, 0, sizeof(g_window_slots));
    g_window_count = 0;
    
    /* Verify dependencies */
    if (!gui_init()) {
        KLOG_ERROR(LOG_CAT_KERNEL, "GUI system not available");
        return FILE_EXPLORER_ERROR_GUI_ERROR;
    }
    
    g_file_explorer_initialized = true;
    KLOG_INFO(LOG_CAT_KERNEL, "File Explorer initialized successfully");
    
    return FILE_EXPLORER_SUCCESS;
}

void file_explorer_shutdown(void) {
    if (!g_file_explorer_initialized) {
        return;
    }
    
    KLOG_INFO(LOG_CAT_KERNEL, "Shutting down File Explorer");
    
    /* Close all windows */
    for (uint32_t i = 0; i < 16; i++) {
        if (g_window_slots[i] && g_windows[i]) {
            file_explorer_destroy_window(g_windows[i]);
        }
    }
    
    g_file_explorer_initialized = false;
    KLOG_INFO(LOG_CAT_KERNEL, "File Explorer shutdown complete");
}

file_explorer_config_t* file_explorer_get_config(void) {
    if (!g_file_explorer_initialized) {
        return NULL;
    }
    return &g_config;
}

int file_explorer_get_stats(file_explorer_stats_t* stats) {
    if (!g_file_explorer_initialized || !stats) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    g_stats.windows_open = g_window_count;
    *stats = g_stats;
    return FILE_EXPLORER_SUCCESS;
}

/* ================================
 * Window Management
 * ================================ */

file_explorer_window_t* file_explorer_create_window(const char* initial_path) {
    if (!g_file_explorer_initialized) {
        KLOG_ERROR(LOG_CAT_KERNEL, "File Explorer not initialized");
        return NULL;
    }
    
    /* Allocate window */
    file_explorer_window_t* window = allocate_window();
    if (!window) {
        KLOG_ERROR(LOG_CAT_KERNEL, "Failed to allocate file explorer window");
        return NULL;
    }
    
    /* Initialize window state */
    strncpy(window->current_path, initial_path ? initial_path : g_config.default_path, 
            sizeof(window->current_path) - 1);
    window->view_mode = g_config.default_view_mode;
    window->show_hidden_files = g_config.show_hidden_files;
    window->show_details_panel = g_config.show_details_panel;
    window->show_sidebar = g_config.show_sidebar;
    window->selected_count = 0;
    window->pending_operation = FILE_OP_NONE;
    window->clipboard_is_cut = false;
    window->history_position = 0;
    window->history_count = 0;
    window->sort_column = 0;
    window->sort_ascending = true;
    
    /* Create main window */
    gui_rect_t window_bounds = gui_rect_make(100, 100, g_config.window_width, g_config.window_height);
    window->main_window = gui_create_window("File Explorer", window_bounds, GUI_WINDOW_NORMAL);
    if (!window->main_window) {
        KLOG_ERROR(LOG_CAT_KERNEL, "Failed to create GUI window");
        free_window(window);
        return NULL;
    }
    
    /* Create UI components */
    if (file_explorer_create_ui(window) != FILE_EXPLORER_SUCCESS) {
        KLOG_ERROR(LOG_CAT_KERNEL, "Failed to create UI components");
        gui_destroy_window(window->main_window);
        free_window(window);
        return NULL;
    }
    
    /* Load initial directory */
    if (file_explorer_load_directory(window, window->current_path) != FILE_EXPLORER_SUCCESS) {
        KLOG_WARN(LOG_CAT_KERNEL, "Failed to load initial directory: %s", window->current_path);
        /* Try to load root directory as fallback */
        if (file_explorer_load_directory(window, "/") != FILE_EXPLORER_SUCCESS) {
            KLOG_ERROR(LOG_CAT_KERNEL, "Failed to load root directory");
            gui_destroy_window(window->main_window);
            free_window(window);
            return NULL;
        }
        strncpy(window->current_path, "/", sizeof(window->current_path) - 1);
    }
    
    /* Set up event handlers */
    gui_set_window_event_handler(window->main_window, file_explorer_window_close_wrapper, window);
    
    /* Update statistics */
    g_stats.windows_open++;
    g_window_count++;
    
    KLOG_INFO(LOG_CAT_KERNEL, "Created file explorer window for path: %s", window->current_path);
    
    return window;
}

void file_explorer_destroy_window(file_explorer_window_t* window) {
    if (!window) return;
    
    KLOG_INFO(LOG_CAT_KERNEL, "Destroying file explorer window");
    
    /* Destroy GUI window */
    if (window->main_window) {
        gui_destroy_window(window->main_window);
    }
    
    /* Free navigation history */
    for (int i = 0; i < window->history_count; i++) {
        if (window->navigation_history[i]) {
            kfree(window->navigation_history[i]);
        }
    }
    
    /* Free window slot */
    free_window(window);
    
    /* Update statistics */
    if (g_window_count > 0) {
        g_window_count--;
    }
}

int file_explorer_show_window(file_explorer_window_t* window, bool show) {
    if (!window || !window->main_window) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    gui_show_window(window->main_window, show);
    if (show) {
        gui_bring_window_to_front(window->main_window);
    }
    
    return FILE_EXPLORER_SUCCESS;
}

/* ================================
 * Navigation Functions
 * ================================ */

int file_explorer_navigate_to(file_explorer_window_t* window, const char* path) {
    if (!window || !path) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    KLOG_DEBUG(LOG_CAT_KERNEL, "Navigating to: %s", path);
    
    /* Verify path exists */
    vfs_stat_t stat;
    if (vfs_stat(path, &stat) != 0) {
        KLOG_ERROR(LOG_CAT_KERNEL, "Path not found: %s", path);
        return FILE_EXPLORER_ERROR_PATH_NOT_FOUND;
    }
    
    if (stat.st_mode != VFS_FILE_TYPE_DIRECTORY) {
        KLOG_ERROR(LOG_CAT_KERNEL, "Path is not a directory: %s", path);
        return FILE_EXPLORER_ERROR_NOT_DIRECTORY;
    }
    
    /* Add current path to navigation history */
    if (window->history_position < 31) {
        /* Free old history if moving forward */
        for (int i = window->history_position + 1; i < window->history_count; i++) {
            if (window->navigation_history[i]) {
                kfree(window->navigation_history[i]);
                window->navigation_history[i] = NULL;
            }
        }
        
        /* Add current path to history */
        size_t path_len = strlen(window->current_path);
        window->navigation_history[window->history_position] = (char*)kmalloc(path_len + 1);
        if (window->navigation_history[window->history_position]) {
            strcpy(window->navigation_history[window->history_position], window->current_path);
        }
        
        window->history_position++;
        window->history_count = window->history_position;
    }
    
    /* Update current path */
    strncpy(window->current_path, path, sizeof(window->current_path) - 1);
    
    /* Load new directory */
    int result = file_explorer_load_directory(window, path);
    if (result != FILE_EXPLORER_SUCCESS) {
        return result;
    }
    
    /* Update UI */
    file_explorer_update_ui(window);
    
    return FILE_EXPLORER_SUCCESS;
}

int file_explorer_navigate_back(file_explorer_window_t* window) {
    if (!window) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    if (window->history_position <= 0) {
        return FILE_EXPLORER_SUCCESS; /* Nothing to go back to */
    }
    
    window->history_position--;
    const char* previous_path = window->navigation_history[window->history_position];
    
    if (!previous_path) {
        return FILE_EXPLORER_ERROR_PATH_NOT_FOUND;
    }
    
    /* Navigate without adding to history */
    strncpy(window->current_path, previous_path, sizeof(window->current_path) - 1);
    int result = file_explorer_load_directory(window, window->current_path);
    if (result == FILE_EXPLORER_SUCCESS) {
        file_explorer_update_ui(window);
    }
    
    return result;
}

int file_explorer_navigate_forward(file_explorer_window_t* window) {
    if (!window) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    if (window->history_position >= window->history_count - 1) {
        return FILE_EXPLORER_SUCCESS; /* Nothing to go forward to */
    }
    
    window->history_position++;
    const char* next_path = window->navigation_history[window->history_position];
    
    if (!next_path) {
        return FILE_EXPLORER_ERROR_PATH_NOT_FOUND;
    }
    
    /* Navigate without adding to history */
    strncpy(window->current_path, next_path, sizeof(window->current_path) - 1);
    int result = file_explorer_load_directory(window, window->current_path);
    if (result == FILE_EXPLORER_SUCCESS) {
        file_explorer_update_ui(window);
    }
    
    return result;
}

int file_explorer_navigate_up(file_explorer_window_t* window) {
    if (!window) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    char* parent_path = file_explorer_get_parent_path(window->current_path);
    if (!parent_path) {
        return FILE_EXPLORER_SUCCESS; /* Already at root */
    }
    
    int result = file_explorer_navigate_to(window, parent_path);
    kfree(parent_path);
    
    return result;
}

int file_explorer_navigate_home(file_explorer_window_t* window) {
    if (!window) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    /* Navigate to default path */
    return file_explorer_navigate_to(window, g_config.default_path);
}

int file_explorer_refresh(file_explorer_window_t* window) {
    if (!window) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    /* Reload current directory */
    int result = file_explorer_load_directory(window, window->current_path);
    if (result == FILE_EXPLORER_SUCCESS) {
        file_explorer_update_ui(window);
    }
    
    return result;
}

/* ================================
 * File Listing and Display
 * ================================ */

int file_explorer_load_directory(file_explorer_window_t* window, const char* path) {
    if (!window || !path) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    KLOG_DEBUG(LOG_CAT_KERNEL, "Loading directory: %s", path);
    
    /* Clear current file list */
    window->file_count = 0;
    window->selected_count = 0;
    
    /* Use VFS integration to list directory */
    uint32_t file_count = 0;
    int result = file_explorer_vfs_list_directory(path, window->files, 
                                                FILE_EXPLORER_MAX_FILES, &file_count);
    if (result != FILE_EXPLORER_SUCCESS) {
        KLOG_ERROR(LOG_CAT_KERNEL, "Failed to list directory: %s", path);
        return result;
    }
    
    window->file_count = file_count;
    
    /* Sort files */
    file_explorer_sort_files(window, window->sort_column, window->sort_ascending);
    
    /* Update statistics */
    g_stats.total_files_viewed += file_count;
    
    KLOG_DEBUG(LOG_CAT_KERNEL, "Loaded %u files from directory: %s", file_count, path);
    
    return FILE_EXPLORER_SUCCESS;
}

int file_explorer_update_file_list(file_explorer_window_t* window) {
    if (!window || !window->file_list) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    /* Clear current list */
    /* Note: This would clear the listbox widget items in a real implementation */
    
    /* Populate file list widget */
    file_explorer_populate_file_list(window);
    
    return FILE_EXPLORER_SUCCESS;
}

void file_explorer_set_view_mode(file_explorer_window_t* window, file_view_mode_t mode) {
    if (!window) return;
    
    window->view_mode = mode;
    file_explorer_update_file_list(window);
}

void file_explorer_sort_files(file_explorer_window_t* window, uint32_t column, bool ascending) {
    if (!window || window->file_count == 0) return;
    
    /* Simple bubble sort for demonstration */
    for (uint32_t i = 0; i < window->file_count - 1; i++) {
        for (uint32_t j = 0; j < window->file_count - i - 1; j++) {
            bool should_swap = false;
            
            switch (column) {
                case 0: /* Name */
                    should_swap = (strcmp(window->files[j].name, window->files[j + 1].name) > 0) == ascending;
                    break;
                case 1: /* Size */
                    should_swap = (window->files[j].size > window->files[j + 1].size) == ascending;
                    break;
                case 2: /* Modified time */
                    should_swap = (window->files[j].modified_time > window->files[j + 1].modified_time) == ascending;
                    break;
                default:
                    should_swap = false;
                    break;
            }
            
            if (should_swap) {
                file_entry_t temp = window->files[j];
                window->files[j] = window->files[j + 1];
                window->files[j + 1] = temp;
            }
        }
    }
    
    window->sort_column = column;
    window->sort_ascending = ascending;
}

/* ================================
 * File Operations
 * ================================ */

int file_explorer_open_file(file_explorer_window_t* window, const char* file_path) {
    if (!window || !file_path) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    KLOG_INFO(LOG_CAT_KERNEL, "Opening file: %s", file_path);
    
    /* Get file information */
    vfs_stat_t stat;
    if (vfs_stat(file_path, &stat) != 0) {
        return FILE_EXPLORER_ERROR_PATH_NOT_FOUND;
    }
    
    if (stat.st_mode == VFS_FILE_TYPE_DIRECTORY) {
        /* Navigate to directory */
        return file_explorer_navigate_to(window, file_path);
    } else {
        /* Try to launch with application loader */
        // TODO: Restore when app_loader headers are fixed
        // int32_t instance_id = app_execute_file(file_path, NULL, NULL);
        // if (instance_id > 0) {
        //     KLOG_INFO(LOG_CAT_KERNEL, "Launched application for file: %s (Instance ID: %d)", 
        //               file_path, instance_id);
        //     return FILE_EXPLORER_SUCCESS;
        // } else {
        //     KLOG_ERROR(LOG_CAT_KERNEL, "Failed to open file: %s", file_path);
        //     return FILE_EXPLORER_ERROR_OPERATION_FAILED;
        // }
        KLOG_INFO(LOG_CAT_KERNEL, "Would open file: %s", file_path);
        return FILE_EXPLORER_SUCCESS;
    }
}

int file_explorer_create_directory(file_explorer_window_t* window, const char* dir_name) {
    if (!window || !dir_name) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    /* Create full path */
    char* full_path = file_explorer_combine_paths(window->current_path, dir_name);
    if (!full_path) {
        return FILE_EXPLORER_ERROR_NO_MEMORY;
    }
    
    /* Create directory using VFS */
    int result = vfs_mkdir(full_path, 0755);
    kfree(full_path);
    
    if (result == 0) {
        /* Refresh directory listing */
        file_explorer_refresh(window);
        g_stats.create_operations++;
        return FILE_EXPLORER_SUCCESS;
    } else {
        return FILE_EXPLORER_ERROR_OPERATION_FAILED;
    }
}

int file_explorer_create_file(file_explorer_window_t* window, const char* file_name) {
    if (!window || !file_name) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    /* Create full path */
    char* full_path = file_explorer_combine_paths(window->current_path, file_name);
    if (!full_path) {
        return FILE_EXPLORER_ERROR_NO_MEMORY;
    }
    
    /* Create file using VFS */
    int fd = vfs_open(full_path, VFS_O_CREAT | VFS_O_WRONLY, 0644);
    kfree(full_path);
    
    if (fd >= 0) {
        vfs_close(fd);
        /* Refresh directory listing */
        file_explorer_refresh(window);
        g_stats.create_operations++;
        return FILE_EXPLORER_SUCCESS;
    } else {
        return FILE_EXPLORER_ERROR_OPERATION_FAILED;
    }
}

/* ================================
 * UI Creation Functions
 * ================================ */

int file_explorer_create_ui(file_explorer_window_t* window) {
    if (!window || !window->main_window) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    /* Create toolbar */
    file_explorer_create_toolbar(window);
    
    /* Create content area */
    file_explorer_create_content_area(window);
    
    /* Create status bar */
    file_explorer_create_status_bar(window);
    
    /* Create context menu */
    file_explorer_create_context_menu(window);
    
    return FILE_EXPLORER_SUCCESS;
}

static void file_explorer_create_toolbar(file_explorer_window_t* window) {
    /* Create toolbar panel */
    gui_rect_t toolbar_bounds = gui_rect_make(0, 0, window->main_window->bounds.width, 40);
    window->toolbar_panel = gui_create_widget(GUI_WIDGET_PANEL, toolbar_bounds, 
                                            window->main_window->root_widget);
    
    if (!window->toolbar_panel) return;
    
    /* Create navigation buttons */
    int button_x = 5;
    const int button_width = 30;
    const int button_height = 30;
    const int button_spacing = 5;
    
    window->back_button = gui_create_button(
        gui_rect_make(button_x, 5, button_width, button_height), "◄", window->toolbar_panel);
    button_x += button_width + button_spacing;
    
    window->forward_button = gui_create_button(
        gui_rect_make(button_x, 5, button_width, button_height), "►", window->toolbar_panel);
    button_x += button_width + button_spacing;
    
    window->up_button = gui_create_button(
        gui_rect_make(button_x, 5, button_width, button_height), "▲", window->toolbar_panel);
    button_x += button_width + button_spacing;
    
    window->home_button = gui_create_button(
        gui_rect_make(button_x, 5, button_width, button_height), "🏠", window->toolbar_panel);
    button_x += button_width + button_spacing;
    
    window->refresh_button = gui_create_button(
        gui_rect_make(button_x, 5, button_width, button_height), "↻", window->toolbar_panel);
    button_x += button_width + button_spacing + 10;
    
    /* Create address bar */
    int address_width = window->main_window->bounds.width - button_x - 100;
    window->address_bar = gui_create_textbox(
        gui_rect_make(button_x, 7, address_width, 26), window->current_path, window->toolbar_panel);
    
    /* Create view mode button */
    button_x += address_width + 10;
    window->view_mode_button = gui_create_button(
        gui_rect_make(button_x, 5, 40, button_height), "☰", window->toolbar_panel);
    
    /* Set up event handlers */
    if (window->back_button) {
        gui_set_event_handler(window->back_button, file_explorer_back_clicked_wrapper, window);
    }
    if (window->forward_button) {
        gui_set_event_handler(window->forward_button, file_explorer_forward_clicked_wrapper, window);
    }
    if (window->up_button) {
        gui_set_event_handler(window->up_button, file_explorer_up_clicked_wrapper, window);
    }
    if (window->home_button) {
        gui_set_event_handler(window->home_button, file_explorer_home_clicked_wrapper, window);
    }
    if (window->refresh_button) {
        gui_set_event_handler(window->refresh_button, file_explorer_refresh_clicked_wrapper, window);
    }
}

static void file_explorer_create_content_area(file_explorer_window_t* window) {
    int content_y = 40; /* Below toolbar */
    int content_height = window->main_window->bounds.height - 40 - 25; /* Above status bar */
    
    /* Create main content panel */
    gui_rect_t content_bounds = gui_rect_make(0, content_y, window->main_window->bounds.width, content_height);
    window->content_panel = gui_create_widget(GUI_WIDGET_PANEL, content_bounds, 
                                            window->main_window->root_widget);
    
    if (!window->content_panel) return;
    
    /* Create file list */
    int list_x = window->show_sidebar ? 200 : 0;
    int list_width = window->main_window->bounds.width - list_x;
    if (window->show_details_panel) {
        list_width -= 200;
    }
    
    gui_rect_t list_bounds = gui_rect_make(list_x, 0, list_width, content_height);
    window->file_list = gui_create_listbox(list_bounds, window->content_panel);
    
    if (window->file_list) {
        gui_set_event_handler(window->file_list, file_explorer_file_selected_wrapper, window);
    }
    
    /* Create sidebar if enabled */
    if (window->show_sidebar) {
        gui_rect_t sidebar_bounds = gui_rect_make(0, 0, 200, content_height);
        window->sidebar_panel = gui_create_widget(GUI_WIDGET_PANEL, sidebar_bounds, window->content_panel);
        
        /* Add sidebar content (shortcuts, favorites, etc.) */
        if (window->sidebar_panel) {
            gui_widget_t* shortcuts_label = gui_create_label(
                gui_rect_make(5, 5, 190, 20), "Quick Access", window->sidebar_panel);
            (void)shortcuts_label; /* Suppress unused variable warning */
        }
    }
    
    /* Create details panel if enabled */
    if (window->show_details_panel) {
        int details_x = window->main_window->bounds.width - 200;
        gui_rect_t details_bounds = gui_rect_make(details_x, 0, 200, content_height);
        window->details_panel = gui_create_widget(GUI_WIDGET_PANEL, details_bounds, window->content_panel);
        
        /* Add details content */
        if (window->details_panel) {
            gui_widget_t* details_label = gui_create_label(
                gui_rect_make(5, 5, 190, 20), "Properties", window->details_panel);
            (void)details_label; /* Suppress unused variable warning */
        }
    }
}

static void file_explorer_create_status_bar(file_explorer_window_t* window) {
    int status_y = window->main_window->bounds.height - 25;
    
    /* Create status bar panel */
    gui_rect_t status_bounds = gui_rect_make(0, status_y, window->main_window->bounds.width, 25);
    window->status_bar = gui_create_widget(GUI_WIDGET_PANEL, status_bounds, 
                                         window->main_window->root_widget);
    
    if (!window->status_bar) return;
    
    /* Create status label */
    window->status_label = gui_create_label(
        gui_rect_make(5, 3, 300, 19), "Ready", window->status_bar);
    
    /* Create selection info label */
    int selection_x = window->main_window->bounds.width - 200;
    window->selection_info = gui_create_label(
        gui_rect_make(selection_x, 3, 195, 19), "", window->status_bar);
}

static void file_explorer_create_context_menu(file_explorer_window_t* window) {
    /* Context menu would be created on demand in a real implementation */
    /* For now, we'll just initialize the pointers to NULL */
    window->context_menu = NULL;
    window->context_open = NULL;
    window->context_copy = NULL;
    window->context_cut = NULL;
    window->context_paste = NULL;
    window->context_delete = NULL;
    window->context_rename = NULL;
    window->context_properties = NULL;
}

/* ================================
 * Helper Functions
 * ================================ */

static file_explorer_window_t* allocate_window(void) {
    for (uint32_t i = 0; i < 16; i++) {
        if (!g_window_slots[i]) {
            g_window_slots[i] = true;
            g_windows[i] = (file_explorer_window_t*)kmalloc(sizeof(file_explorer_window_t));
            if (g_windows[i]) {
                memset(g_windows[i], 0, sizeof(file_explorer_window_t));
                return g_windows[i];
            }
            g_window_slots[i] = false;
            return NULL;
        }
    }
    return NULL;
}

static void free_window(file_explorer_window_t* window) {
    if (!window) return;
    
    for (uint32_t i = 0; i < 16; i++) {
        if (g_windows[i] == window) {
            kfree(window);
            g_windows[i] = NULL;
            g_window_slots[i] = false;
            break;
        }
    }
}

static void file_explorer_populate_file_list(file_explorer_window_t* window) {
    if (!window || !window->file_list) return;
    
    /* Clear current list items */
    /* This would clear the listbox in a real implementation */
    
    /* Add files to list */
    for (uint32_t i = 0; i < window->file_count; i++) {
        file_entry_t* entry = &window->files[i];
        
        /* Skip hidden files if not showing them */
        if (!window->show_hidden_files && entry->name[0] == '.') {
            continue;
        }
        
        /* Format list item based on view mode */
        char item_text[512];
        switch (window->view_mode) {
            case FILE_VIEW_LIST:
                snprintf_impl(item_text, sizeof(item_text), "%s %s", 
                        g_file_type_icons[entry->type_category], entry->name);
                break;
            case FILE_VIEW_DETAILS: {
                char size_str[32];
                file_explorer_format_file_size(entry->size, size_str, sizeof(size_str));
                snprintf_impl(item_text, sizeof(item_text), "%s %-30s %10s", 
                        g_file_type_icons[entry->type_category], entry->name, size_str);
                break;
            }
            case FILE_VIEW_ICONS:
            default:
                snprintf_impl(item_text, sizeof(item_text), "%s\n%s", 
                        g_file_type_icons[entry->type_category], entry->name);
                break;
        }
        
        /* Add item to listbox */
        gui_listbox_add_item(window->file_list, item_text);
    }
}

void file_explorer_update_ui(file_explorer_window_t* window) {
    if (!window) return;
    
    /* Update address bar */
    if (window->address_bar) {
        gui_set_widget_text(window->address_bar, window->current_path);
    }
    
    /* Update file list */
    file_explorer_update_file_list(window);
    
    /* Update navigation buttons */
    file_explorer_update_navigation_buttons(window);
    
    /* Update status bar */
    file_explorer_update_status_bar(window);
    
    /* Update window title */
    char title[512];
    snprintf_impl(title, sizeof(title), "File Explorer - %s", window->current_path);
    gui_set_window_title(window->main_window, title);
}

static void file_explorer_update_navigation_buttons(file_explorer_window_t* window) {
    if (!window) return;
    
    /* Update back button state */
    if (window->back_button) {
        gui_set_widget_enabled(window->back_button, window->history_position > 0);
    }
    
    /* Update forward button state */
    if (window->forward_button) {
        gui_set_widget_enabled(window->forward_button, 
                              window->history_position < window->history_count - 1);
    }
    
    /* Update up button state */
    if (window->up_button) {
        bool can_go_up = strcmp(window->current_path, "/") != 0;
        gui_set_widget_enabled(window->up_button, can_go_up);
    }
}

void file_explorer_update_status_bar(file_explorer_window_t* window) {
    if (!window || !window->status_label) return;
    
    /* Update status message */
    char status_text[256];
    snprintf_impl(status_text, sizeof(status_text), "%u items", window->file_count);
    gui_set_widget_text(window->status_label, status_text);
    
    /* Update selection info */
    if (window->selection_info && window->selected_count > 0) {
        char selection_text[256];
        snprintf_impl(selection_text, sizeof(selection_text), "%u selected", window->selected_count);
        gui_set_widget_text(window->selection_info, selection_text);
    } else if (window->selection_info) {
        gui_set_widget_text(window->selection_info, "");
    }
}

/* ================================
 * VFS Integration
 * ================================ */

int file_explorer_vfs_list_directory(const char* path, file_entry_t* entries, 
                                   uint32_t max_entries, uint32_t* count) {
    if (!path || !entries || !count) {
        return FILE_EXPLORER_ERROR_INVALID_PARAM;
    }
    
    *count = 0;
    
    /* Open directory */
    int dir_fd = vfs_opendir(path);
    if (dir_fd < 0) {
        return FILE_EXPLORER_ERROR_VFS_ERROR;
    }
    
    /* Read directory entries */
    vfs_dirent_t dirent;
    uint32_t entry_count = 0;
    
    while (entry_count < max_entries && vfs_readdir(dir_fd, &dirent) == 0) {
        file_entry_t* entry = &entries[entry_count];
        
        /* Copy name */
        strncpy(entry->name, dirent.d_name, sizeof(entry->name) - 1);
        entry->name[sizeof(entry->name) - 1] = '\0';
        
        /* Create full path */
        snprintf_impl(entry->full_path, sizeof(entry->full_path), "%s/%s", path, entry->name);
        
        /* Get file information */
        if (vfs_stat(entry->full_path, &entry->stat) == 0) {
            entry->is_directory = (entry->stat.st_mode == VFS_FILE_TYPE_DIRECTORY);
            entry->size = entry->stat.st_size;
            entry->permissions = entry->stat.st_perm;
            entry->modified_time = entry->stat.st_mtime;
            
            /* Detect file type category */
            entry->type_category = file_explorer_detect_file_type(entry->name, &entry->stat);
        } else {
            /* Set defaults if stat fails */
            entry->is_directory = false;
            entry->size = 0;
            entry->permissions = 0;
            entry->modified_time = 0;
            entry->type_category = FILE_TYPE_UNKNOWN;
        }
        
        entry->selected = false;
        entry_count++;
    }
    
    vfs_closedir(dir_fd);
    *count = entry_count;
    
    return FILE_EXPLORER_SUCCESS;
}

/* ================================
 * File Type Detection
 * ================================ */

file_type_category_t file_explorer_detect_file_type(const char* filename, const vfs_stat_t* stat) {
    if (!filename || !stat) {
        return FILE_TYPE_UNKNOWN;
    }
    
    /* Check if it's a directory */
    if (stat->st_mode == VFS_FILE_TYPE_DIRECTORY) {
        return FILE_TYPE_DIRECTORY;
    }
    
    /* Simple file extension detection */
    const char* ext = strrchr_impl(filename, '.');
    if (ext) {
        ext++; /* Skip the dot */
        
        /* Text files */
        if (strcmp(ext, "txt") == 0 || strcmp(ext, "md") == 0 || 
            strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0) {
            return FILE_TYPE_TEXT;
        }
        
        /* Executable files */
        if (strcmp(ext, "exe") == 0 || strcmp(ext, "bin") == 0 || 
            strcmp(ext, "out") == 0) {
            return FILE_TYPE_EXECUTABLE;
        }
        
        /* Image files */
        if (strcmp(ext, "bmp") == 0 || strcmp(ext, "png") == 0 || 
            strcmp(ext, "jpg") == 0 || strcmp(ext, "gif") == 0) {
            return FILE_TYPE_IMAGE;
        }
        
        /* Archive files */
        if (strcmp(ext, "zip") == 0 || strcmp(ext, "tar") == 0 || 
            strcmp(ext, "gz") == 0) {
            return FILE_TYPE_ARCHIVE;
        }
        
        /* System files */
        if (strcmp(ext, "sys") == 0 || strcmp(ext, "cfg") == 0 || 
            strcmp(ext, "conf") == 0) {
            return FILE_TYPE_SYSTEM;
        }
    }
    
    /* Check for executable permissions */
    if (stat->st_perm & 0111) { /* Any execute bit set */
        return FILE_TYPE_EXECUTABLE;
    }
    
    return FILE_TYPE_UNKNOWN;
}

const char* file_explorer_get_file_type_icon(file_type_category_t type) {
    if (type < sizeof(g_file_type_icons) / sizeof(g_file_type_icons[0])) {
        return g_file_type_icons[type];
    }
    return g_file_type_icons[FILE_TYPE_UNKNOWN];
}

const char* file_explorer_get_file_type_description(file_type_category_t type) {
    if (type < sizeof(g_file_type_descriptions) / sizeof(g_file_type_descriptions[0])) {
        return g_file_type_descriptions[type];
    }
    return g_file_type_descriptions[FILE_TYPE_UNKNOWN];
}

/* ================================
 * Utility Functions
 * ================================ */

char* file_explorer_get_parent_path(const char* path) {
    if (!path || strcmp(path, "/") == 0) {
        return NULL;
    }
    
    /* Find last slash */
    const char* last_slash = strrchr_impl(path, '/');
    if (!last_slash || last_slash == path) {
        /* Root directory */
        char* root = (char*)kmalloc(2);
        if (root) {
            strcpy(root, "/");
        }
        return root;
    }
    
    /* Copy parent path */
    size_t parent_len = last_slash - path;
    char* parent = (char*)kmalloc(parent_len + 1);
    if (parent) {
        strncpy(parent, path, parent_len);
        parent[parent_len] = '\0';
    }
    
    return parent;
}

char* file_explorer_combine_paths(const char* base, const char* relative) {
    if (!base || !relative) {
        return NULL;
    }
    
    size_t base_len = strlen(base);
    size_t relative_len = strlen(relative);
    bool need_slash = (base_len > 0 && base[base_len - 1] != '/');
    
    size_t total_len = base_len + relative_len + (need_slash ? 1 : 0) + 1;
    char* result = (char*)kmalloc(total_len);
    
    if (result) {
        strcpy(result, base);
        if (need_slash) {
            strcat_impl(result, "/");
        }
        strcat_impl(result, relative);
    }
    
    return result;
}

void file_explorer_format_file_size(uint64_t size, char* buffer, size_t buffer_size) {
    if (!buffer) return;
    
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double display_size = (double)size;
    
    while (display_size >= 1024.0 && unit_index < 4) {
        display_size /= 1024.0;
        unit_index++;
    }
    
    if (unit_index == 0) {
        snprintf_impl(buffer, buffer_size, "%lu %s", (unsigned long)size, units[unit_index]);
    } else {
        snprintf_impl(buffer, buffer_size, "%.1f %s", display_size, units[unit_index]);
    }
}

/* ================================
 * Application Registration
 * ================================ */

int file_explorer_register_application(void) {
    /* TODO: Register with application loader when headers are fixed */
    return FILE_EXPLORER_SUCCESS;
}

int file_explorer_launch_instance(const char* initial_path) {
    /* Initialize if needed */
    if (!g_file_explorer_initialized) {
        if (file_explorer_init(NULL) != FILE_EXPLORER_SUCCESS) {
            return -1;
        }
    }
    
    /* Create and show window */
    file_explorer_window_t* window = file_explorer_create_window(initial_path);
    if (!window) {
        return -1;
    }
    
    file_explorer_show_window(window, true);
    
    /* Return window ID as instance identifier */
    return (int)window->main_window->id;
}

/* ================================
 * GUI Event Handler Wrappers
 * ================================ */

void file_explorer_back_clicked_wrapper(gui_event_t* event, void* user_data) {
    (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        file_explorer_navigate_back(window);
    }
}

void file_explorer_forward_clicked_wrapper(gui_event_t* event, void* user_data) {
    (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        file_explorer_navigate_forward(window);
    }
}

void file_explorer_up_clicked_wrapper(gui_event_t* event, void* user_data) {
    (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        file_explorer_navigate_up(window);
    }
}

void file_explorer_home_clicked_wrapper(gui_event_t* event, void* user_data) {
    (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        file_explorer_navigate_home(window);
    }
}

void file_explorer_refresh_clicked_wrapper(gui_event_t* event, void* user_data) {
    (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        file_explorer_refresh(window);
    }
}

void file_explorer_file_selected_wrapper(gui_event_t* event, void* user_data) {
    (void)event; (void)user_data;
    /* Handle file selection */
}

void file_explorer_window_close_wrapper(gui_event_t* event, void* user_data) {
    (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        file_explorer_destroy_window(window);
    }
}

/* ================================
 * Placeholder Event Handlers
 * ================================ */

void file_explorer_on_back_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        file_explorer_navigate_back(window);
    }
}

void file_explorer_on_forward_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        file_explorer_navigate_forward(window);
    }
}

void file_explorer_on_up_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        file_explorer_navigate_up(window);
    }
}

void file_explorer_on_home_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        file_explorer_navigate_home(window);
    }
}

void file_explorer_on_refresh_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        file_explorer_refresh(window);
    }
}

void file_explorer_on_address_changed(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window && widget) {
        const char* new_path = gui_get_widget_text(widget);
        if (new_path) {
            file_explorer_navigate_to(window, new_path);
        }
    }
}

void file_explorer_on_view_mode_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        /* Cycle through view modes */
        file_view_mode_t next_mode = (file_view_mode_t)((window->view_mode + 1) % 3);
        file_explorer_set_view_mode(window, next_mode);
    }
}

void file_explorer_on_file_selected(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event; (void)user_data;
    /* Handle file selection */
}

void file_explorer_on_file_double_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window && window->file_list) {
        /* Get selected file and open it */
        int32_t selected = gui_listbox_get_selected(window->file_list);
        if (selected >= 0 && selected < (int32_t)window->file_count) {
            file_explorer_open_file(window, window->files[selected].full_path);
        }
    }
}

void file_explorer_on_file_right_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event; (void)user_data;
    /* Show context menu */
}

void file_explorer_on_window_close(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        file_explorer_destroy_window(window);
    }
}

void file_explorer_on_window_resize(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event;
    file_explorer_window_t* window = (file_explorer_window_t*)user_data;
    if (window) {
        /* Update UI layout for new window size */
        file_explorer_update_ui(window);
    }
}

/* Placeholder context menu handlers */
void file_explorer_on_context_open(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event; (void)user_data;
}

void file_explorer_on_context_copy(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event; (void)user_data;
}

void file_explorer_on_context_cut(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event; (void)user_data;
}

void file_explorer_on_context_paste(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event; (void)user_data;
}

void file_explorer_on_context_delete(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event; (void)user_data;
}

void file_explorer_on_context_rename(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event; (void)user_data;
}

void file_explorer_on_context_properties(gui_widget_t* widget, gui_event_t* event, void* user_data) {
    (void)widget; (void)event; (void)user_data;
}
