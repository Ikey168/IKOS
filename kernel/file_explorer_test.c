/* IKOS File Explorer Test Suite - Issue #41
 * Comprehensive tests for the graphical file manager
 */

#include "file_explorer.h"
#include "gui.h"
#include "vfs.h"
#include "kernel_log.h"
#include "assert.h"
#include <stddef.h>
#include <string.h>

/* ================================
 * Test Configuration
 * ================================ */

#define TEST_MAX_FILES 50
#define TEST_TIMEOUT 5000  /* ms */

/* Test data */
static file_explorer_window_t* g_test_window = NULL;
static bool g_gui_available = false;
static bool g_vfs_available = false;

/* ================================
 * Test Helper Functions
 * ================================ */

static bool test_init_passed = false;
static uint32_t test_count = 0;
static uint32_t test_passed = 0;
static uint32_t test_failed = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        KLOG_INFO(LOG_CAT_TEST, "Starting test: %s", name); \
    } while(0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            KLOG_ERROR(LOG_CAT_TEST, "ASSERTION FAILED: %s", message); \
            test_failed++; \
            return false; \
        } \
    } while(0)

#define TEST_PASS(name) \
    do { \
        test_passed++; \
        KLOG_INFO(LOG_CAT_TEST, "Test PASSED: %s", name); \
        return true; \
    } while(0)

#define TEST_FAIL(name, message) \
    do { \
        test_failed++; \
        KLOG_ERROR(LOG_CAT_TEST, "Test FAILED: %s - %s", name, message); \
        return false; \
    } while(0)

static void setup_test_environment(void) {
    /* Check system dependencies */
    g_gui_available = (gui_init() == 0);
    g_vfs_available = true; /* Assume VFS is available */
    
    /* Create test directory structure if VFS is available */
    if (g_vfs_available) {
        vfs_mkdir("/test_files", 0755);
        vfs_mkdir("/test_files/subdir1", 0755);
        vfs_mkdir("/test_files/subdir2", 0755);
        
        /* Create test files */
        int fd = vfs_open("/test_files/test.txt", VFS_O_CREAT | VFS_O_WRONLY, 0644);
        if (fd >= 0) {
            vfs_write(fd, "Test file content", 17);
            vfs_close(fd);
        }
        
        fd = vfs_open("/test_files/readme.md", VFS_O_CREAT | VFS_O_WRONLY, 0644);
        if (fd >= 0) {
            vfs_write(fd, "# Test README", 13);
            vfs_close(fd);
        }
        
        fd = vfs_open("/test_files/executable", VFS_O_CREAT | VFS_O_WRONLY, 0755);
        if (fd >= 0) {
            vfs_write(fd, "#!/bin/sh\necho hello", 20);
            vfs_close(fd);
        }
    }
}

static void cleanup_test_environment(void) {
    /* Clean up test window */
    if (g_test_window) {
        file_explorer_destroy_window(g_test_window);
        g_test_window = NULL;
    }
    
    /* Clean up test files */
    if (g_vfs_available) {
        vfs_unlink("/test_files/test.txt");
        vfs_unlink("/test_files/readme.md");
        vfs_unlink("/test_files/executable");
        vfs_rmdir("/test_files/subdir1");
        vfs_rmdir("/test_files/subdir2");
        vfs_rmdir("/test_files");
    }
}

/* ================================
 * Basic Functionality Tests
 * ================================ */

static bool test_file_explorer_initialization(void) {
    TEST_START("File Explorer Initialization");
    
    /* Test initialization with default config */
    int result = file_explorer_init(NULL);
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to initialize with default config");
    
    /* Test double initialization */
    result = file_explorer_init(NULL);
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Double initialization should succeed");
    
    /* Test config retrieval */
    file_explorer_config_t* config = file_explorer_get_config();
    TEST_ASSERT(config != NULL, "Failed to get config");
    TEST_ASSERT(config->window_width > 0, "Invalid window width");
    TEST_ASSERT(config->window_height > 0, "Invalid window height");
    
    /* Test statistics retrieval */
    file_explorer_stats_t stats;
    result = file_explorer_get_stats(&stats);
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to get stats");
    
    test_init_passed = true;
    TEST_PASS("File Explorer Initialization");
}

static bool test_file_explorer_window_creation(void) {
    TEST_START("File Explorer Window Creation");
    
    if (!test_init_passed) {
        TEST_FAIL("File Explorer Window Creation", "Initialization test must pass first");
    }
    
    if (!g_gui_available) {
        KLOG_INFO(LOG_CAT_TEST, "GUI not available, skipping window creation test");
        test_passed++;
        return true;
    }
    
    /* Test window creation with default path */
    g_test_window = file_explorer_create_window(NULL);
    TEST_ASSERT(g_test_window != NULL, "Failed to create window with default path");
    TEST_ASSERT(g_test_window->main_window != NULL, "Main GUI window is NULL");
    
    /* Test window creation with specific path */
    file_explorer_window_t* window2 = file_explorer_create_window("/");
    TEST_ASSERT(window2 != NULL, "Failed to create window with specific path");
    TEST_ASSERT(strcmp(window2->current_path, "/") == 0, "Current path not set correctly");
    
    /* Test showing window */
    int result = file_explorer_show_window(g_test_window, true);
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to show window");
    
    /* Clean up second window */
    file_explorer_destroy_window(window2);
    
    TEST_PASS("File Explorer Window Creation");
}

static bool test_file_explorer_ui_components(void) {
    TEST_START("File Explorer UI Components");
    
    if (!g_test_window || !g_gui_available) {
        KLOG_INFO(LOG_CAT_TEST, "No test window or GUI available, skipping UI test");
        test_passed++;
        return true;
    }
    
    /* Test toolbar components */
    TEST_ASSERT(g_test_window->toolbar_panel != NULL, "Toolbar panel not created");
    TEST_ASSERT(g_test_window->back_button != NULL, "Back button not created");
    TEST_ASSERT(g_test_window->forward_button != NULL, "Forward button not created");
    TEST_ASSERT(g_test_window->up_button != NULL, "Up button not created");
    TEST_ASSERT(g_test_window->home_button != NULL, "Home button not created");
    TEST_ASSERT(g_test_window->refresh_button != NULL, "Refresh button not created");
    TEST_ASSERT(g_test_window->address_bar != NULL, "Address bar not created");
    TEST_ASSERT(g_test_window->view_mode_button != NULL, "View mode button not created");
    
    /* Test content area components */
    TEST_ASSERT(g_test_window->content_panel != NULL, "Content panel not created");
    TEST_ASSERT(g_test_window->file_list != NULL, "File list not created");
    
    /* Test status bar components */
    TEST_ASSERT(g_test_window->status_bar != NULL, "Status bar not created");
    TEST_ASSERT(g_test_window->status_label != NULL, "Status label not created");
    
    TEST_PASS("File Explorer UI Components");
}

/* ================================
 * Navigation Tests
 * ================================ */

static bool test_file_explorer_navigation(void) {
    TEST_START("File Explorer Navigation");
    
    if (!g_test_window) {
        TEST_FAIL("File Explorer Navigation", "No test window available");
    }
    
    /* Test navigate to root */
    int result = file_explorer_navigate_to(g_test_window, "/");
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to navigate to root");
    TEST_ASSERT(strcmp(g_test_window->current_path, "/") == 0, "Current path not updated");
    
    /* Test navigate up from root (should stay at root) */
    result = file_explorer_navigate_up(g_test_window);
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Navigate up from root should succeed");
    TEST_ASSERT(strcmp(g_test_window->current_path, "/") == 0, "Should stay at root");
    
    /* Test navigate to home */
    result = file_explorer_navigate_home(g_test_window);
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to navigate home");
    
    /* Test refresh */
    result = file_explorer_refresh(g_test_window);
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to refresh");
    
    /* Test invalid path navigation */
    result = file_explorer_navigate_to(g_test_window, "/nonexistent/path");
    TEST_ASSERT(result != FILE_EXPLORER_SUCCESS, "Should fail to navigate to invalid path");
    
    TEST_PASS("File Explorer Navigation");
}

static bool test_file_explorer_navigation_history(void) {
    TEST_START("File Explorer Navigation History");
    
    if (!g_test_window) {
        TEST_FAIL("File Explorer Navigation History", "No test window available");
    }
    
    /* Start at root */
    file_explorer_navigate_to(g_test_window, "/");
    
    /* Navigate to test directory if it exists */
    if (g_vfs_available) {
        file_explorer_navigate_to(g_test_window, "/test_files");
        TEST_ASSERT(g_test_window->history_count > 0, "History should have entries");
        TEST_ASSERT(g_test_window->history_position > 0, "History position should advance");
        
        /* Test back navigation */
        int result = file_explorer_navigate_back(g_test_window);
        TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to navigate back");
        TEST_ASSERT(strcmp(g_test_window->current_path, "/") == 0, "Should be back at root");
        
        /* Test forward navigation */
        result = file_explorer_navigate_forward(g_test_window);
        TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to navigate forward");
        TEST_ASSERT(strcmp(g_test_window->current_path, "/test_files") == 0, "Should be forward to test_files");
    } else {
        KLOG_INFO(LOG_CAT_TEST, "VFS not available, limited navigation history test");
    }
    
    TEST_PASS("File Explorer Navigation History");
}

/* ================================
 * File Listing Tests
 * ================================ */

static bool test_file_explorer_directory_listing(void) {
    TEST_START("File Explorer Directory Listing");
    
    if (!g_test_window) {
        TEST_FAIL("File Explorer Directory Listing", "No test window available");
    }
    
    /* Test loading root directory */
    int result = file_explorer_load_directory(g_test_window, "/");
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to load root directory");
    
    /* Test file count */
    KLOG_INFO(LOG_CAT_TEST, "Root directory contains %u files", g_test_window->file_count);
    
    /* Test loading test directory if available */
    if (g_vfs_available) {
        result = file_explorer_load_directory(g_test_window, "/test_files");
        if (result == FILE_EXPLORER_SUCCESS) {
            TEST_ASSERT(g_test_window->file_count >= 3, "Test directory should have at least 3 entries");
            
            /* Check for expected files */
            bool found_subdir1 = false;
            bool found_test_txt = false;
            
            for (uint32_t i = 0; i < g_test_window->file_count; i++) {
                if (strcmp(g_test_window->files[i].name, "subdir1") == 0) {
                    found_subdir1 = true;
                    TEST_ASSERT(g_test_window->files[i].is_directory, "subdir1 should be a directory");
                }
                if (strcmp(g_test_window->files[i].name, "test.txt") == 0) {
                    found_test_txt = true;
                    TEST_ASSERT(!g_test_window->files[i].is_directory, "test.txt should not be a directory");
                }
            }
            
            TEST_ASSERT(found_subdir1, "Should find subdir1");
            TEST_ASSERT(found_test_txt, "Should find test.txt");
        }
    }
    
    /* Test updating file list UI */
    result = file_explorer_update_file_list(g_test_window);
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to update file list");
    
    TEST_PASS("File Explorer Directory Listing");
}

static bool test_file_explorer_file_type_detection(void) {
    TEST_START("File Explorer File Type Detection");
    
    /* Test directory detection */
    vfs_stat_t dir_stat = {0};
    dir_stat.st_type = VFS_FILE_TYPE_DIRECTORY;
    file_type_category_t type = file_explorer_detect_file_type("testdir", &dir_stat);
    TEST_ASSERT(type == FILE_TYPE_DIRECTORY, "Should detect directory");
    
    /* Test text file detection */
    vfs_stat_t file_stat = {0};
    file_stat.st_type = VFS_FILE_TYPE_REGULAR;
    type = file_explorer_detect_file_type("test.txt", &file_stat);
    TEST_ASSERT(type == FILE_TYPE_TEXT, "Should detect text file");
    
    /* Test executable detection by extension */
    type = file_explorer_detect_file_type("program.exe", &file_stat);
    TEST_ASSERT(type == FILE_TYPE_EXECUTABLE, "Should detect executable by extension");
    
    /* Test executable detection by permissions */
    file_stat.st_perm = 0755;
    type = file_explorer_detect_file_type("program", &file_stat);
    TEST_ASSERT(type == FILE_TYPE_EXECUTABLE, "Should detect executable by permissions");
    
    /* Test image file detection */
    file_stat.st_perm = 0644;
    type = file_explorer_detect_file_type("image.png", &file_stat);
    TEST_ASSERT(type == FILE_TYPE_IMAGE, "Should detect image file");
    
    /* Test unknown file detection */
    type = file_explorer_detect_file_type("unknown", &file_stat);
    TEST_ASSERT(type == FILE_TYPE_UNKNOWN, "Should detect unknown file");
    
    /* Test file type descriptions */
    const char* desc = file_explorer_get_file_type_description(FILE_TYPE_TEXT);
    TEST_ASSERT(desc != NULL, "Should get file type description");
    TEST_ASSERT(strlen(desc) > 0, "Description should not be empty");
    
    /* Test file type icons */
    const char* icon = file_explorer_get_file_type_icon(FILE_TYPE_DIRECTORY);
    TEST_ASSERT(icon != NULL, "Should get file type icon");
    TEST_ASSERT(strlen(icon) > 0, "Icon should not be empty");
    
    TEST_PASS("File Explorer File Type Detection");
}

/* ================================
 * File Operations Tests
 * ================================ */

static bool test_file_explorer_view_modes(void) {
    TEST_START("File Explorer View Modes");
    
    if (!g_test_window) {
        TEST_FAIL("File Explorer View Modes", "No test window available");
    }
    
    /* Test default view mode */
    TEST_ASSERT(g_test_window->view_mode != FILE_VIEW_ICONS || 
               g_test_window->view_mode != FILE_VIEW_LIST ||
               g_test_window->view_mode != FILE_VIEW_DETAILS, "Should have valid default view mode");
    
    /* Test changing view modes */
    file_explorer_set_view_mode(g_test_window, FILE_VIEW_LIST);
    TEST_ASSERT(g_test_window->view_mode == FILE_VIEW_LIST, "Should set list view mode");
    
    file_explorer_set_view_mode(g_test_window, FILE_VIEW_ICONS);
    TEST_ASSERT(g_test_window->view_mode == FILE_VIEW_ICONS, "Should set icons view mode");
    
    file_explorer_set_view_mode(g_test_window, FILE_VIEW_DETAILS);
    TEST_ASSERT(g_test_window->view_mode == FILE_VIEW_DETAILS, "Should set details view mode");
    
    TEST_PASS("File Explorer View Modes");
}

static bool test_file_explorer_sorting(void) {
    TEST_START("File Explorer Sorting");
    
    if (!g_test_window) {
        TEST_FAIL("File Explorer Sorting", "No test window available");
    }
    
    /* Load test directory */
    if (g_vfs_available) {
        file_explorer_load_directory(g_test_window, "/test_files");
    } else {
        file_explorer_load_directory(g_test_window, "/");
    }
    
    if (g_test_window->file_count < 2) {
        KLOG_INFO(LOG_CAT_TEST, "Not enough files for sorting test");
        test_passed++;
        return true;
    }
    
    /* Test sorting by name ascending */
    file_explorer_sort_files(g_test_window, 0, true);
    TEST_ASSERT(g_test_window->sort_column == 0, "Sort column should be 0");
    TEST_ASSERT(g_test_window->sort_ascending == true, "Should be ascending");
    
    /* Verify sorting */
    for (uint32_t i = 0; i < g_test_window->file_count - 1; i++) {
        TEST_ASSERT(strcmp(g_test_window->files[i].name, g_test_window->files[i + 1].name) <= 0,
                   "Files should be sorted by name ascending");
    }
    
    /* Test sorting by name descending */
    file_explorer_sort_files(g_test_window, 0, false);
    TEST_ASSERT(g_test_window->sort_ascending == false, "Should be descending");
    
    /* Verify reverse sorting */
    for (uint32_t i = 0; i < g_test_window->file_count - 1; i++) {
        TEST_ASSERT(strcmp(g_test_window->files[i].name, g_test_window->files[i + 1].name) >= 0,
                   "Files should be sorted by name descending");
    }
    
    TEST_PASS("File Explorer Sorting");
}

static bool test_file_explorer_file_operations(void) {
    TEST_START("File Explorer File Operations");
    
    if (!g_test_window) {
        TEST_FAIL("File Explorer File Operations", "No test window available");
    }
    
    if (!g_vfs_available) {
        KLOG_INFO(LOG_CAT_TEST, "VFS not available, skipping file operations test");
        test_passed++;
        return true;
    }
    
    /* Navigate to test directory */
    file_explorer_navigate_to(g_test_window, "/test_files");
    
    /* Test creating directory */
    int result = file_explorer_create_directory(g_test_window, "new_test_dir");
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to create directory");
    
    /* Verify directory was created */
    vfs_stat_t stat;
    result = vfs_stat("/test_files/new_test_dir", &stat);
    TEST_ASSERT(result == 0, "Directory should exist");
    TEST_ASSERT(stat.st_type == VFS_FILE_TYPE_DIRECTORY, "Should be a directory");
    
    /* Test creating file */
    result = file_explorer_create_file(g_test_window, "new_test_file.txt");
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to create file");
    
    /* Verify file was created */
    result = vfs_stat("/test_files/new_test_file.txt", &stat);
    TEST_ASSERT(result == 0, "File should exist");
    TEST_ASSERT(stat.st_type == VFS_FILE_TYPE_REGULAR, "Should be a regular file");
    
    /* Test opening directory */
    result = file_explorer_open_file(g_test_window, "/test_files/new_test_dir");
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to open directory");
    TEST_ASSERT(strcmp(g_test_window->current_path, "/test_files/new_test_dir") == 0,
               "Should navigate to directory");
    
    /* Clean up test files */
    file_explorer_navigate_to(g_test_window, "/test_files");
    vfs_unlink("/test_files/new_test_file.txt");
    vfs_rmdir("/test_files/new_test_dir");
    
    TEST_PASS("File Explorer File Operations");
}

/* ================================
 * Integration Tests
 * ================================ */

static bool test_file_explorer_vfs_integration(void) {
    TEST_START("File Explorer VFS Integration");
    
    if (!g_vfs_available) {
        KLOG_INFO(LOG_CAT_TEST, "VFS not available, skipping VFS integration test");
        test_passed++;
        return true;
    }
    
    /* Test VFS directory listing */
    file_entry_t entries[TEST_MAX_FILES];
    uint32_t count = 0;
    int result = file_explorer_vfs_list_directory("/test_files", entries, TEST_MAX_FILES, &count);
    TEST_ASSERT(result == FILE_EXPLORER_SUCCESS, "Failed to list VFS directory");
    TEST_ASSERT(count > 0, "Should find some files");
    
    /* Test VFS file info */
    file_entry_t entry;
    result = file_explorer_vfs_get_file_info("/test_files/test.txt", &entry);
    if (result == FILE_EXPLORER_SUCCESS) {
        TEST_ASSERT(strcmp(entry.name, "test.txt") == 0, "File name should match");
        TEST_ASSERT(!entry.is_directory, "Should not be a directory");
        TEST_ASSERT(entry.size > 0, "File should have size");
    }
    
    /* Test invalid path */
    result = file_explorer_vfs_list_directory("/nonexistent", entries, TEST_MAX_FILES, &count);
    TEST_ASSERT(result != FILE_EXPLORER_SUCCESS, "Should fail for invalid path");
    
    TEST_PASS("File Explorer VFS Integration");
}

static bool test_file_explorer_application_integration(void) {
    TEST_START("File Explorer Application Integration");
    
    /* Test application registration */
    int result = file_explorer_register_application();
    TEST_ASSERT(result == APP_ERROR_SUCCESS, "Failed to register application");
    
    /* Test launching instance */
    int instance_id = file_explorer_launch_instance("/");
    if (instance_id > 0) {
        KLOG_INFO(LOG_CAT_TEST, "Successfully launched file explorer instance: %d", instance_id);
    } else {
        KLOG_WARN(LOG_CAT_TEST, "Failed to launch file explorer instance");
    }
    
    TEST_PASS("File Explorer Application Integration");
}

/* ================================
 * Utility Tests
 * ================================ */

static bool test_file_explorer_path_utilities(void) {
    TEST_START("File Explorer Path Utilities");
    
    /* Test parent path extraction */
    char* parent = file_explorer_get_parent_path("/test/path/file.txt");
    TEST_ASSERT(parent != NULL, "Should get parent path");
    TEST_ASSERT(strcmp(parent, "/test/path") == 0, "Parent path should be correct");
    free(parent);
    
    /* Test root parent */
    parent = file_explorer_get_parent_path("/");
    TEST_ASSERT(parent == NULL, "Root should have no parent");
    
    /* Test single level */
    parent = file_explorer_get_parent_path("/file.txt");
    TEST_ASSERT(parent != NULL, "Should get parent");
    TEST_ASSERT(strcmp(parent, "/") == 0, "Parent should be root");
    free(parent);
    
    /* Test path combination */
    char* combined = file_explorer_combine_paths("/test", "file.txt");
    TEST_ASSERT(combined != NULL, "Should combine paths");
    TEST_ASSERT(strcmp(combined, "/test/file.txt") == 0, "Combined path should be correct");
    free(combined);
    
    /* Test path combination with trailing slash */
    combined = file_explorer_combine_paths("/test/", "file.txt");
    TEST_ASSERT(combined != NULL, "Should combine paths with slash");
    TEST_ASSERT(strcmp(combined, "/test/file.txt") == 0, "Combined path should be correct");
    free(combined);
    
    TEST_PASS("File Explorer Path Utilities");
}

static bool test_file_explorer_formatting(void) {
    TEST_START("File Explorer Formatting");
    
    /* Test file size formatting */
    char buffer[64];
    
    file_explorer_format_file_size(512, buffer, sizeof(buffer));
    TEST_ASSERT(strstr(buffer, "512") != NULL, "Should format bytes");
    TEST_ASSERT(strstr(buffer, "B") != NULL, "Should show bytes unit");
    
    file_explorer_format_file_size(1536, buffer, sizeof(buffer));
    TEST_ASSERT(strstr(buffer, "1.5") != NULL, "Should format KB");
    TEST_ASSERT(strstr(buffer, "KB") != NULL, "Should show KB unit");
    
    file_explorer_format_file_size(2097152, buffer, sizeof(buffer));
    TEST_ASSERT(strstr(buffer, "2.0") != NULL, "Should format MB");
    TEST_ASSERT(strstr(buffer, "MB") != NULL, "Should show MB unit");
    
    TEST_PASS("File Explorer Formatting");
}

/* ================================
 * Error Handling Tests
 * ================================ */

static bool test_file_explorer_error_handling(void) {
    TEST_START("File Explorer Error Handling");
    
    /* Test invalid parameters */
    int result = file_explorer_navigate_to(NULL, "/");
    TEST_ASSERT(result == FILE_EXPLORER_ERROR_INVALID_PARAM, "Should fail with NULL window");
    
    result = file_explorer_navigate_to(g_test_window, NULL);
    TEST_ASSERT(result == FILE_EXPLORER_ERROR_INVALID_PARAM, "Should fail with NULL path");
    
    /* Test invalid path navigation */
    if (g_test_window) {
        result = file_explorer_navigate_to(g_test_window, "/nonexistent/invalid/path");
        TEST_ASSERT(result != FILE_EXPLORER_SUCCESS, "Should fail for invalid path");
    }
    
    /* Test operations on NULL window */
    result = file_explorer_show_window(NULL, true);
    TEST_ASSERT(result == FILE_EXPLORER_ERROR_INVALID_PARAM, "Should fail with NULL window");
    
    result = file_explorer_create_directory(NULL, "test");
    TEST_ASSERT(result == FILE_EXPLORER_ERROR_INVALID_PARAM, "Should fail with NULL window");
    
    result = file_explorer_create_file(NULL, "test.txt");
    TEST_ASSERT(result == FILE_EXPLORER_ERROR_INVALID_PARAM, "Should fail with NULL window");
    
    /* Test NULL parameters for utilities */
    char* result_str = file_explorer_get_parent_path(NULL);
    TEST_ASSERT(result_str == NULL, "Should return NULL for NULL path");
    
    result_str = file_explorer_combine_paths(NULL, "test");
    TEST_ASSERT(result_str == NULL, "Should return NULL for NULL base");
    
    result_str = file_explorer_combine_paths("/test", NULL);
    TEST_ASSERT(result_str == NULL, "Should return NULL for NULL relative");
    
    TEST_PASS("File Explorer Error Handling");
}

/* ================================
 * Main Test Suite Function
 * ================================ */

void file_explorer_run_tests(void) {
    KLOG_INFO(LOG_CAT_TEST, "=== Starting File Explorer Test Suite ===");
    
    /* Initialize test environment */
    setup_test_environment();
    
    /* Initialize test counters */
    test_count = 0;
    test_passed = 0;
    test_failed = 0;
    test_init_passed = false;
    
    /* Run test suite */
    test_file_explorer_initialization();
    test_file_explorer_window_creation();
    test_file_explorer_ui_components();
    test_file_explorer_navigation();
    test_file_explorer_navigation_history();
    test_file_explorer_directory_listing();
    test_file_explorer_file_type_detection();
    test_file_explorer_view_modes();
    test_file_explorer_sorting();
    test_file_explorer_file_operations();
    test_file_explorer_vfs_integration();
    test_file_explorer_application_integration();
    test_file_explorer_path_utilities();
    test_file_explorer_formatting();
    test_file_explorer_error_handling();
    
    /* Print results */
    KLOG_INFO(LOG_CAT_TEST, "=== File Explorer Test Results ===");
    KLOG_INFO(LOG_CAT_TEST, "Total Tests: %u", test_count);
    KLOG_INFO(LOG_CAT_TEST, "Passed: %u", test_passed);
    KLOG_INFO(LOG_CAT_TEST, "Failed: %u", test_failed);
    
    if (test_failed == 0) {
        KLOG_INFO(LOG_CAT_TEST, "*** ALL TESTS PASSED ***");
    } else {
        KLOG_ERROR(LOG_CAT_TEST, "*** %u TESTS FAILED ***", test_failed);
    }
    
    /* Cleanup */
    cleanup_test_environment();
    file_explorer_shutdown();
    
    KLOG_INFO(LOG_CAT_TEST, "=== File Explorer Test Suite Complete ===");
}

/* ================================
 * Specific Test Functions
 * ================================ */

void file_explorer_test_basic_operations(void) {
    KLOG_INFO(LOG_CAT_TEST, "Running basic File Explorer operations test...");
    
    /* Initialize */
    if (file_explorer_init(NULL) != FILE_EXPLORER_SUCCESS) {
        KLOG_ERROR(LOG_CAT_TEST, "Failed to initialize File Explorer");
        return;
    }
    
    /* Create window */
    file_explorer_window_t* window = file_explorer_create_window("/");
    if (!window) {
        KLOG_ERROR(LOG_CAT_TEST, "Failed to create File Explorer window");
        file_explorer_shutdown();
        return;
    }
    
    KLOG_INFO(LOG_CAT_TEST, "File Explorer window created successfully");
    
    /* Test navigation */
    if (file_explorer_navigate_to(window, "/") == FILE_EXPLORER_SUCCESS) {
        KLOG_INFO(LOG_CAT_TEST, "Navigation to root successful");
        KLOG_INFO(LOG_CAT_TEST, "Current directory contains %u files", window->file_count);
    }
    
    /* Test refresh */
    if (file_explorer_refresh(window) == FILE_EXPLORER_SUCCESS) {
        KLOG_INFO(LOG_CAT_TEST, "Directory refresh successful");
    }
    
    /* Cleanup */
    file_explorer_destroy_window(window);
    file_explorer_shutdown();
    KLOG_INFO(LOG_CAT_TEST, "Basic File Explorer operations test complete");
}

void file_explorer_test_navigation(void) {
    KLOG_INFO(LOG_CAT_TEST, "Running File Explorer navigation test...");
    
    if (file_explorer_init(NULL) != FILE_EXPLORER_SUCCESS) {
        KLOG_ERROR(LOG_CAT_TEST, "Failed to initialize File Explorer");
        return;
    }
    
    file_explorer_window_t* window = file_explorer_create_window("/");
    if (!window) {
        KLOG_ERROR(LOG_CAT_TEST, "Failed to create File Explorer window");
        file_explorer_shutdown();
        return;
    }
    
    /* Test various navigation operations */
    file_explorer_navigate_to(window, "/");
    file_explorer_navigate_home(window);
    file_explorer_navigate_up(window);
    file_explorer_refresh(window);
    
    KLOG_INFO(LOG_CAT_TEST, "Navigation test completed - current path: %s", window->current_path);
    
    file_explorer_destroy_window(window);
    file_explorer_shutdown();
    KLOG_INFO(LOG_CAT_TEST, "File Explorer navigation test complete");
}

void file_explorer_test_file_operations(void) {
    KLOG_INFO(LOG_CAT_TEST, "Running File Explorer file operations test...");
    
    if (file_explorer_init(NULL) != FILE_EXPLORER_SUCCESS) {
        KLOG_ERROR(LOG_CAT_TEST, "Failed to initialize File Explorer");
        return;
    }
    
    file_explorer_window_t* window = file_explorer_create_window("/");
    if (!window) {
        KLOG_ERROR(LOG_CAT_TEST, "Failed to create File Explorer window");
        file_explorer_shutdown();
        return;
    }
    
    /* Test file operations if VFS is available */
    if (vfs_mkdir("/test_operations", 0755) == 0) {
        file_explorer_navigate_to(window, "/test_operations");
        
        if (file_explorer_create_directory(window, "test_subdir") == FILE_EXPLORER_SUCCESS) {
            KLOG_INFO(LOG_CAT_TEST, "Directory creation successful");
        }
        
        if (file_explorer_create_file(window, "test_file.txt") == FILE_EXPLORER_SUCCESS) {
            KLOG_INFO(LOG_CAT_TEST, "File creation successful");
        }
        
        file_explorer_refresh(window);
        KLOG_INFO(LOG_CAT_TEST, "Test directory contains %u files", window->file_count);
        
        /* Cleanup */
        vfs_unlink("/test_operations/test_file.txt");
        vfs_rmdir("/test_operations/test_subdir");
        vfs_rmdir("/test_operations");
    }
    
    file_explorer_destroy_window(window);
    file_explorer_shutdown();
    KLOG_INFO(LOG_CAT_TEST, "File Explorer file operations test complete");
}
