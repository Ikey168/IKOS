/* IKOS Unified Application Loader - Issue #40
 * Provides unified execution of GUI and CLI applications with
 * integrated process launching, application management, and system API
 */

#ifndef APP_LOADER_H
#define APP_LOADER_H

#include <stdint.h>
#include <stdbool.h>
#include "process.h"
#include "user_app_loader.h"
#include "gui.h"

/* Application types */
typedef enum {
    APP_TYPE_CLI = 0,        /* Command-line application */
    APP_TYPE_GUI = 1,        /* Graphical application */
    APP_TYPE_HYBRID = 2,     /* Application that can run in both modes */
    APP_TYPE_UNKNOWN = 3     /* Unknown/invalid application type */
} app_type_t;

/* Application launch mode */
typedef enum {
    APP_LAUNCH_BACKGROUND = 0,  /* Launch in background */
    APP_LAUNCH_FOREGROUND = 1,  /* Launch in foreground */
    APP_LAUNCH_DETACHED = 2     /* Launch detached from parent */
} app_launch_mode_t;

/* Application launch flags */
#define APP_FLAG_NONE           0x0000
#define APP_FLAG_GUI_ENABLE     0x0001  /* Enable GUI subsystem for this app */
#define APP_FLAG_CLI_ENABLE     0x0002  /* Enable CLI subsystem for this app */
#define APP_FLAG_AUTO_DETECT    0x0004  /* Auto-detect application type */
#define APP_FLAG_DEBUG_MODE     0x0008  /* Launch with debugging enabled */
#define APP_FLAG_RESTRICTED     0x0010  /* Run with restricted permissions */
#define APP_FLAG_SYSTEM_LEVEL   0x0020  /* System-level application */

/* Application descriptor */
typedef struct {
    char name[64];                  /* Application name */
    char path[256];                 /* Full path to executable */
    char icon_path[256];            /* Path to application icon */
    char description[512];          /* Application description */
    app_type_t type;                /* Application type */
    uint32_t flags;                 /* Launch flags */
    uint32_t memory_limit;          /* Memory limit in bytes */
    uint32_t cpu_priority;          /* CPU priority (0-100) */
    char working_directory[256];    /* Default working directory */
    char config_file[256];          /* Configuration file path */
    bool installed;                 /* Whether app is installed */
    uint64_t install_time;          /* Installation timestamp */
    uint64_t last_run_time;         /* Last execution timestamp */
    uint32_t run_count;             /* Number of times executed */
} app_descriptor_t;

/* Application instance - running application */
typedef struct {
    uint32_t instance_id;           /* Unique instance ID */
    app_descriptor_t* descriptor;   /* Application descriptor */
    process_t* process;             /* Associated process */
    app_type_t runtime_type;        /* Actual runtime type */
    app_launch_mode_t launch_mode;  /* How it was launched */
    uint32_t flags;                 /* Runtime flags */
    gui_window_t* main_window;      /* Main GUI window (if GUI app) */
    uint64_t start_time;            /* When the application started */
    uint64_t cpu_time_used;         /* CPU time consumed */
    uint32_t memory_used;           /* Current memory usage */
    bool is_responding;             /* Whether app is responding */
    char** argv;                    /* Command line arguments */
    char** envp;                    /* Environment variables */
} app_instance_t;

/* Application loader configuration */
typedef struct {
    char system_apps_dir[256];      /* System applications directory */
    char user_apps_dir[256];        /* User applications directory */
    char temp_dir[256];             /* Temporary directory */
    uint32_t max_concurrent_apps;   /* Maximum concurrent applications */
    uint32_t default_memory_limit;  /* Default memory limit */
    uint32_t app_timeout;           /* Application response timeout */
    bool gui_enabled;               /* Whether GUI subsystem is available */
    bool cli_enabled;               /* Whether CLI subsystem is available */
    bool auto_cleanup;              /* Auto cleanup terminated apps */
} app_loader_config_t;

/* Application loader statistics */
typedef struct {
    uint32_t apps_loaded;           /* Total applications loaded */
    uint32_t apps_running;          /* Currently running applications */
    uint32_t apps_terminated;      /* Applications that have terminated */
    uint32_t apps_crashed;          /* Applications that crashed */
    uint32_t gui_apps_active;      /* Active GUI applications */
    uint32_t cli_apps_active;      /* Active CLI applications */
    uint64_t total_memory_used;     /* Total memory used by applications */
    uint64_t total_cpu_time;        /* Total CPU time consumed */
    uint32_t launch_failures;      /* Number of launch failures */
    uint32_t registry_size;         /* Number of registered applications */
} app_loader_stats_t;

/* ================================
 * Core Application Loader Functions
 * ================================ */

/**
 * Initialize the unified application loader
 * @param config Configuration parameters
 * @return 0 on success, negative error code on failure
 */
int app_loader_init(app_loader_config_t* config);

/**
 * Shutdown the application loader and cleanup resources
 */
void app_loader_shutdown(void);

/**
 * Get current application loader configuration
 * @return Pointer to current configuration
 */
app_loader_config_t* app_loader_get_config(void);

/**
 * Get application loader statistics
 * @param stats Pointer to statistics structure to fill
 * @return 0 on success, negative error code on failure
 */
int app_loader_get_stats(app_loader_stats_t* stats);

/* ================================
 * Application Registration and Discovery
 * ================================ */

/**
 * Register an application in the system
 * @param descriptor Application descriptor
 * @return 0 on success, negative error code on failure
 */
int app_register(app_descriptor_t* descriptor);

/**
 * Unregister an application from the system
 * @param name Application name
 * @return 0 on success, negative error code on failure
 */
int app_unregister(const char* name);

/**
 * Scan directory for applications and register them
 * @param directory_path Directory to scan
 * @return Number of applications found and registered
 */
int app_scan_directory(const char* directory_path);

/**
 * Find an application by name
 * @param name Application name
 * @return Pointer to application descriptor, NULL if not found
 */
app_descriptor_t* app_find_by_name(const char* name);

/**
 * Find an application by path
 * @param path Application executable path
 * @return Pointer to application descriptor, NULL if not found
 */
app_descriptor_t* app_find_by_path(const char* path);

/**
 * List all registered applications
 * @param descriptors Array to store application descriptors
 * @param max_count Maximum number of descriptors to return
 * @return Number of applications returned
 */
int app_list_all(app_descriptor_t* descriptors, uint32_t max_count);

/**
 * List applications by type
 * @param type Application type to filter by
 * @param descriptors Array to store application descriptors
 * @param max_count Maximum number of descriptors to return
 * @return Number of applications returned
 */
int app_list_by_type(app_type_t type, app_descriptor_t* descriptors, uint32_t max_count);

/* ================================
 * Application Execution
 * ================================ */

/**
 * Launch an application by name
 * @param name Application name
 * @param argv Command line arguments (NULL-terminated)
 * @param envp Environment variables (NULL-terminated)
 * @param mode Launch mode
 * @param flags Launch flags
 * @return Instance ID on success, negative error code on failure
 */
int32_t app_launch_by_name(const char* name, char* const argv[], char* const envp[], 
                          app_launch_mode_t mode, uint32_t flags);

/**
 * Launch an application by path
 * @param path Path to executable
 * @param argv Command line arguments (NULL-terminated)
 * @param envp Environment variables (NULL-terminated)
 * @param mode Launch mode
 * @param flags Launch flags
 * @return Instance ID on success, negative error code on failure
 */
int32_t app_launch_by_path(const char* path, char* const argv[], char* const envp[], 
                          app_launch_mode_t mode, uint32_t flags);

/**
 * Launch a GUI application with window management
 * @param name Application name
 * @param argv Command line arguments
 * @param envp Environment variables
 * @param parent_window Parent window (or NULL)
 * @return Instance ID on success, negative error code on failure
 */
int32_t app_launch_gui(const char* name, char* const argv[], char* const envp[], 
                      gui_window_t* parent_window);

/**
 * Launch a CLI application in terminal
 * @param name Application name
 * @param argv Command line arguments
 * @param envp Environment variables
 * @param terminal_id Terminal to run in (or 0 for new)
 * @return Instance ID on success, negative error code on failure
 */
int32_t app_launch_cli(const char* name, char* const argv[], char* const envp[], 
                      uint32_t terminal_id);

/**
 * Execute application from file system with auto-detection
 * @param path Full path to executable
 * @param argv Command line arguments
 * @param envp Environment variables
 * @return Instance ID on success, negative error code on failure
 */
int32_t app_execute_file(const char* path, char* const argv[], char* const envp[]);

/* ================================
 * Application Instance Management
 * ================================ */

/**
 * Get application instance by ID
 * @param instance_id Instance ID
 * @return Pointer to application instance, NULL if not found
 */
app_instance_t* app_get_instance(uint32_t instance_id);

/**
 * Get all running application instances
 * @param instances Array to store instance pointers
 * @param max_count Maximum number of instances to return
 * @return Number of instances returned
 */
int app_get_all_instances(app_instance_t** instances, uint32_t max_count);

/**
 * Get running instances of a specific application
 * @param name Application name
 * @param instances Array to store instance pointers
 * @param max_count Maximum number of instances to return
 * @return Number of instances returned
 */
int app_get_instances_by_name(const char* name, app_instance_t** instances, uint32_t max_count);

/**
 * Terminate an application instance
 * @param instance_id Instance ID
 * @param force Whether to force termination
 * @return 0 on success, negative error code on failure
 */
int app_terminate_instance(uint32_t instance_id, bool force);

/**
 * Send signal to application instance
 * @param instance_id Instance ID
 * @param signal Signal to send
 * @return 0 on success, negative error code on failure
 */
int app_signal_instance(uint32_t instance_id, int signal);

/**
 * Check if application instance is responding
 * @param instance_id Instance ID
 * @return true if responding, false otherwise
 */
bool app_is_responding(uint32_t instance_id);

/**
 * Get application instance resource usage
 * @param instance_id Instance ID
 * @param memory_used Pointer to store memory usage
 * @param cpu_time Pointer to store CPU time
 * @return 0 on success, negative error code on failure
 */
int app_get_resource_usage(uint32_t instance_id, uint32_t* memory_used, uint64_t* cpu_time);

/* ================================
 * Application Type Detection
 * ================================ */

/**
 * Detect application type from ELF binary
 * @param elf_data Pointer to ELF data
 * @param size Size of ELF data
 * @return Detected application type
 */
app_type_t app_detect_type_from_elf(const void* elf_data, size_t size);

/**
 * Detect application type from file path
 * @param path Path to executable file
 * @return Detected application type
 */
app_type_t app_detect_type_from_path(const char* path);

/**
 * Detect application type from process information
 * @param proc Process structure
 * @return Detected application type
 */
app_type_t app_detect_type_from_process(process_t* proc);

/* ================================
 * System Integration
 * ================================ */

/**
 * Create application API context for running application
 * @param instance_id Instance ID
 * @return 0 on success, negative error code on failure
 */
int app_create_api_context(uint32_t instance_id);

/**
 * Setup environment for GUI application
 * @param instance Instance pointer
 * @return 0 on success, negative error code on failure
 */
int app_setup_gui_environment(app_instance_t* instance);

/**
 * Setup environment for CLI application
 * @param instance Instance pointer
 * @param terminal_id Terminal ID to use
 * @return 0 on success, negative error code on failure
 */
int app_setup_cli_environment(app_instance_t* instance, uint32_t terminal_id);

/**
 * Cleanup application environment on termination
 * @param instance Instance pointer
 */
void app_cleanup_environment(app_instance_t* instance);

/* ================================
 * Event Handling
 * ================================ */

/**
 * Handle application termination event
 * @param instance_id Instance ID
 * @param exit_code Exit code
 */
void app_handle_termination(uint32_t instance_id, int exit_code);

/**
 * Handle application crash event
 * @param instance_id Instance ID
 * @param crash_info Crash information
 */
void app_handle_crash(uint32_t instance_id, void* crash_info);

/**
 * Handle application output event
 * @param instance_id Instance ID
 * @param output_data Output data
 * @param size Size of output data
 */
void app_handle_output(uint32_t instance_id, const void* output_data, size_t size);

/* ================================
 * Constants and Limits
 * ================================ */

#define APP_LOADER_MAX_APPS             128     /* Maximum registered applications */
#define APP_LOADER_MAX_INSTANCES        64      /* Maximum concurrent instances */
#define APP_LOADER_MAX_NAME_LENGTH      64      /* Maximum application name length */
#define APP_LOADER_MAX_PATH_LENGTH      256     /* Maximum path length */
#define APP_LOADER_MAX_ARGS             32      /* Maximum command line arguments */
#define APP_LOADER_DEFAULT_TIMEOUT      30000   /* Default timeout in milliseconds */

/* Error codes */
#define APP_ERROR_SUCCESS               0       /* Success */
#define APP_ERROR_NOT_FOUND            -1       /* Application not found */
#define APP_ERROR_INVALID_TYPE         -2       /* Invalid application type */
#define APP_ERROR_LAUNCH_FAILED        -3       /* Launch failed */
#define APP_ERROR_NO_MEMORY            -4       /* Out of memory */
#define APP_ERROR_INVALID_PARAM        -5       /* Invalid parameter */
#define APP_ERROR_PERMISSION_DENIED    -6       /* Permission denied */
#define APP_ERROR_RESOURCE_BUSY        -7       /* Resource busy */
#define APP_ERROR_TIMEOUT              -8       /* Operation timeout */
#define APP_ERROR_NOT_RESPONDING       -9       /* Application not responding */
#define APP_ERROR_ALREADY_EXISTS       -10      /* Application already exists */

/* Built-in application names */
#define APP_NAME_SHELL                  "shell"
#define APP_NAME_FILE_MANAGER           "filemanager"
#define APP_NAME_TERMINAL               "terminal"
#define APP_NAME_SYSTEM_INFO            "sysinfo"
#define APP_NAME_PROCESS_MANAGER        "procman"
#define APP_NAME_CALCULATOR             "calc"
#define APP_NAME_TEXT_EDITOR            "editor"

#endif /* APP_LOADER_H */
