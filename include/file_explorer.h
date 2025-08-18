/* IKOS File Explorer - Issue #41
 * Graphical file manager for browsing and managing files
 * Integrates with VFS, GUI system, and application loader
 */

#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include "gui.h"
#include "vfs.h"
//#include "app_loader.h"  // TODO: Fix header conflicts
#include <stdint.h>
#include <stdbool.h>

/* ================================
 * Configuration and Constants
 * ================================ */

#define FILE_EXPLORER_MAX_PATH       VFS_MAX_PATH_LENGTH
#define FILE_EXPLORER_MAX_FILENAME   VFS_MAX_FILENAME_LENGTH
#define FILE_EXPLORER_MAX_FILES      256
#define FILE_EXPLORER_DEFAULT_WIDTH  640
#define FILE_EXPLORER_DEFAULT_HEIGHT 480

/* File Explorer View Modes */
typedef enum {
    FILE_VIEW_LIST = 0,      /* List view with details */
    FILE_VIEW_ICONS,         /* Icon view */
    FILE_VIEW_DETAILS        /* Detailed list view */
} file_view_mode_t;

/* File type icons/categories */
typedef enum {
    FILE_TYPE_UNKNOWN = 0,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_TEXT,
    FILE_TYPE_EXECUTABLE,
    FILE_TYPE_IMAGE,
    FILE_TYPE_ARCHIVE,
    FILE_TYPE_SYSTEM
} file_type_category_t;

/* File operation types */
typedef enum {
    FILE_OP_NONE = 0,
    FILE_OP_COPY,
    FILE_OP_MOVE,
    FILE_OP_DELETE,
    FILE_OP_CREATE_DIR,
    FILE_OP_CREATE_FILE,
    FILE_OP_RENAME,
    FILE_OP_PROPERTIES
} file_operation_t;

/* ================================
 * Data Structures
 * ================================ */

/* File entry information */
typedef struct file_entry {
    char name[FILE_EXPLORER_MAX_FILENAME];
    char full_path[FILE_EXPLORER_MAX_PATH];
    vfs_stat_t stat;
    file_type_category_t type_category;
    bool selected;
    bool is_directory;
    uint64_t size;
    uint32_t permissions;
    uint64_t modified_time;
} file_entry_t;

/* File Explorer Window State */
typedef struct file_explorer_window {
    gui_window_t* main_window;
    
    /* Toolbar widgets */
    gui_widget_t* toolbar_panel;
    gui_widget_t* back_button;
    gui_widget_t* forward_button;
    gui_widget_t* up_button;
    gui_widget_t* home_button;
    gui_widget_t* refresh_button;
    gui_widget_t* address_bar;
    gui_widget_t* view_mode_button;
    
    /* Main content area */
    gui_widget_t* content_panel;
    gui_widget_t* sidebar_panel;
    gui_widget_t* file_list;
    gui_widget_t* details_panel;
    
    /* Context menu */
    gui_widget_t* context_menu;
    gui_widget_t* context_open;
    gui_widget_t* context_copy;
    gui_widget_t* context_cut;
    gui_widget_t* context_paste;
    gui_widget_t* context_delete;
    gui_widget_t* context_rename;
    gui_widget_t* context_properties;
    
    /* Status bar */
    gui_widget_t* status_bar;
    gui_widget_t* status_label;
    gui_widget_t* selection_info;
    
    /* Navigation state */
    char current_path[FILE_EXPLORER_MAX_PATH];
    char* navigation_history[32];
    int history_position;
    int history_count;
    
    /* File listing */
    file_entry_t files[FILE_EXPLORER_MAX_FILES];
    uint32_t file_count;
    file_view_mode_t view_mode;
    
    /* Selection and operations */
    uint32_t selected_count;
    file_operation_t pending_operation;
    char clipboard_path[FILE_EXPLORER_MAX_PATH];
    bool clipboard_is_cut;
    
    /* Settings */
    bool show_hidden_files;
    bool show_details_panel;
    bool show_sidebar;
    uint32_t sort_column;
    bool sort_ascending;
} file_explorer_window_t;

/* File Explorer Configuration */
typedef struct file_explorer_config {
    char default_path[FILE_EXPLORER_MAX_PATH];
    file_view_mode_t default_view_mode;
    bool show_hidden_files;
    bool show_details_panel;
    bool show_sidebar;
    uint32_t window_width;
    uint32_t window_height;
    bool enable_file_preview;
    bool enable_thumbnails;
} file_explorer_config_t;

/* File Explorer Statistics */
typedef struct file_explorer_stats {
    uint32_t windows_open;
    uint32_t total_files_viewed;
    uint32_t total_operations;
    uint32_t copy_operations;
    uint32_t move_operations;
    uint32_t delete_operations;
    uint32_t create_operations;
} file_explorer_stats_t;

/* ================================
 * Core File Explorer Functions
 * ================================ */

/* Initialization and shutdown */
int file_explorer_init(file_explorer_config_t* config);
void file_explorer_shutdown(void);
file_explorer_config_t* file_explorer_get_config(void);
int file_explorer_get_stats(file_explorer_stats_t* stats);

/* Window management */
file_explorer_window_t* file_explorer_create_window(const char* initial_path);
void file_explorer_destroy_window(file_explorer_window_t* window);
int file_explorer_show_window(file_explorer_window_t* window, bool show);

/* Navigation */
int file_explorer_navigate_to(file_explorer_window_t* window, const char* path);
int file_explorer_navigate_back(file_explorer_window_t* window);
int file_explorer_navigate_forward(file_explorer_window_t* window);
int file_explorer_navigate_up(file_explorer_window_t* window);
int file_explorer_navigate_home(file_explorer_window_t* window);
int file_explorer_refresh(file_explorer_window_t* window);

/* File listing and display */
int file_explorer_load_directory(file_explorer_window_t* window, const char* path);
int file_explorer_update_file_list(file_explorer_window_t* window);
void file_explorer_set_view_mode(file_explorer_window_t* window, file_view_mode_t mode);
void file_explorer_sort_files(file_explorer_window_t* window, uint32_t column, bool ascending);
void file_explorer_filter_files(file_explorer_window_t* window, const char* filter);

/* File selection */
void file_explorer_select_file(file_explorer_window_t* window, uint32_t index, bool multi_select);
void file_explorer_select_all(file_explorer_window_t* window);
void file_explorer_clear_selection(file_explorer_window_t* window);
uint32_t file_explorer_get_selected_files(file_explorer_window_t* window, file_entry_t** selected);

/* File operations */
int file_explorer_open_file(file_explorer_window_t* window, const char* file_path);
int file_explorer_copy_files(file_explorer_window_t* window, file_entry_t* files, uint32_t count, const char* dest_path);
int file_explorer_move_files(file_explorer_window_t* window, file_entry_t* files, uint32_t count, const char* dest_path);
int file_explorer_delete_files(file_explorer_window_t* window, file_entry_t* files, uint32_t count);
int file_explorer_create_directory(file_explorer_window_t* window, const char* dir_name);
int file_explorer_create_file(file_explorer_window_t* window, const char* file_name);
int file_explorer_rename_file(file_explorer_window_t* window, const char* old_name, const char* new_name);

/* Clipboard operations */
void file_explorer_copy_to_clipboard(file_explorer_window_t* window);
void file_explorer_cut_to_clipboard(file_explorer_window_t* window);
void file_explorer_paste_from_clipboard(file_explorer_window_t* window);

/* File type detection */
file_type_category_t file_explorer_detect_file_type(const char* filename, const vfs_stat_t* stat);
const char* file_explorer_get_file_type_icon(file_type_category_t type);
const char* file_explorer_get_file_type_description(file_type_category_t type);

/* ================================
 * UI Event Handlers
 * ================================ */

/* Toolbar event handlers */
void file_explorer_on_back_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_forward_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_up_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_home_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_refresh_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_address_changed(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_view_mode_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data);

/* File list event handlers */
void file_explorer_on_file_selected(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_file_double_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_file_right_clicked(gui_widget_t* widget, gui_event_t* event, void* user_data);

/* Context menu event handlers */
void file_explorer_on_context_open(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_context_copy(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_context_cut(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_context_paste(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_context_delete(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_context_rename(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_context_properties(gui_widget_t* widget, gui_event_t* event, void* user_data);

/* Window event handlers */
void file_explorer_on_window_close(gui_widget_t* widget, gui_event_t* event, void* user_data);
void file_explorer_on_window_resize(gui_widget_t* widget, gui_event_t* event, void* user_data);

/* ================================
 * Utility Functions
 * ================================ */

/* Path utilities */
char* file_explorer_get_parent_path(const char* path);
char* file_explorer_get_filename(const char* path);
char* file_explorer_combine_paths(const char* base, const char* relative);
bool file_explorer_is_path_absolute(const char* path);

/* File size formatting */
void file_explorer_format_file_size(uint64_t size, char* buffer, size_t buffer_size);
void file_explorer_format_date_time(uint64_t timestamp, char* buffer, size_t buffer_size);
void file_explorer_format_permissions(uint32_t permissions, char* buffer, size_t buffer_size);

/* Dialog utilities */
bool file_explorer_show_confirm_dialog(const char* title, const char* message);
bool file_explorer_show_input_dialog(const char* title, const char* prompt, char* result, size_t result_size);
void file_explorer_show_error_dialog(const char* title, const char* message);
void file_explorer_show_properties_dialog(file_explorer_window_t* window, const char* file_path);

/* ================================
 * Integration Functions
 * ================================ */

/* Application loader integration */
int file_explorer_register_application(void);
int file_explorer_launch_instance(const char* initial_path);

/* VFS integration */
int file_explorer_vfs_list_directory(const char* path, file_entry_t* entries, uint32_t max_entries, uint32_t* count);
int file_explorer_vfs_get_file_info(const char* path, file_entry_t* entry);

/* GUI integration */
int file_explorer_create_ui(file_explorer_window_t* window);
void file_explorer_update_ui(file_explorer_window_t* window);
void file_explorer_update_status_bar(file_explorer_window_t* window);

/* ================================
 * Error Codes
 * ================================ */

#define FILE_EXPLORER_SUCCESS                0
#define FILE_EXPLORER_ERROR_INVALID_PARAM   -1
#define FILE_EXPLORER_ERROR_NO_MEMORY       -2
#define FILE_EXPLORER_ERROR_VFS_ERROR       -3
#define FILE_EXPLORER_ERROR_GUI_ERROR       -4
#define FILE_EXPLORER_ERROR_PATH_NOT_FOUND  -5
#define FILE_EXPLORER_ERROR_ACCESS_DENIED   -6
#define FILE_EXPLORER_ERROR_OPERATION_FAILED -7
#define FILE_EXPLORER_ERROR_NOT_DIRECTORY   -8
#define FILE_EXPLORER_ERROR_FILE_EXISTS     -9
#define FILE_EXPLORER_ERROR_DISK_FULL       -10

/* ================================
 * Debug and Testing
 * ================================ */

#ifdef FILE_EXPLORER_DEBUG
void file_explorer_debug_print_state(file_explorer_window_t* window);
void file_explorer_debug_print_file_list(file_explorer_window_t* window);
#endif

/* Test functions */
void file_explorer_run_tests(void);
void file_explorer_test_basic_operations(void);
void file_explorer_test_navigation(void);
void file_explorer_test_file_operations(void);

/* GUI Event Handler Wrappers */
void file_explorer_back_clicked_wrapper(gui_event_t* event, void* user_data);
void file_explorer_forward_clicked_wrapper(gui_event_t* event, void* user_data);
void file_explorer_up_clicked_wrapper(gui_event_t* event, void* user_data);
void file_explorer_home_clicked_wrapper(gui_event_t* event, void* user_data);
void file_explorer_refresh_clicked_wrapper(gui_event_t* event, void* user_data);
void file_explorer_file_selected_wrapper(gui_event_t* event, void* user_data);
void file_explorer_window_close_wrapper(gui_event_t* event, void* user_data);

#endif /* FILE_EXPLORER_H */
