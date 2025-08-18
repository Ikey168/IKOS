/* IKOS Unified Application Loader Implementation - Issue #40
 * Unified execution of GUI and CLI applications with integrated
 * process launching, application management, and system API
 */

#include "app_loader.h"
#include "user_app_loader.h"
#include "process.h"
#include "gui.h"
#include "memory.h"
#include "string.h"
#include "vfs.h"
#include "elf.h"
#include "kernel_log.h"
#include <stddef.h>

/* ================================
 * Global State
 * ================================ */

static bool g_app_loader_initialized = false;
static app_loader_config_t g_config;
static app_loader_stats_t g_stats;

/* Application registry */
static app_descriptor_t g_app_registry[APP_LOADER_MAX_APPS];
static bool g_app_registry_slots[APP_LOADER_MAX_APPS] = {false};
static uint32_t g_app_registry_count = 0;

/* Running application instances */
static app_instance_t g_app_instances[APP_LOADER_MAX_INSTANCES];
static bool g_instance_slots[APP_LOADER_MAX_INSTANCES] = {false};
static uint32_t g_instance_count = 0;
static uint32_t g_next_instance_id = 1;

/* ================================
 * Internal Helper Functions
 * ================================ */

static app_descriptor_t* allocate_app_descriptor(void);
static void free_app_descriptor(app_descriptor_t* descriptor);
static app_instance_t* allocate_app_instance(void);
static void free_app_instance(app_instance_t* instance);
static int detect_app_type_from_elf_headers(const void* elf_data, size_t size);
static int setup_app_environment(app_instance_t* instance);
static void cleanup_app_instance_internal(app_instance_t* instance);

/* ================================
 * Core Application Loader Functions
 * ================================ */

int app_loader_init(app_loader_config_t* config) {
    if (g_app_loader_initialized) {
        KLOG_WARN(LOG_CAT_PROCESS, "Application loader already initialized");
        return APP_ERROR_SUCCESS;
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "Initializing unified application loader");
    
    /* Set default configuration if none provided */
    if (config) {
        g_config = *config;
    } else {
        /* Default configuration */
        strncpy(g_config.system_apps_dir, "/bin", sizeof(g_config.system_apps_dir) - 1);
        strncpy(g_config.user_apps_dir, "/usr/bin", sizeof(g_config.user_apps_dir) - 1);
        strncpy(g_config.temp_dir, "/tmp", sizeof(g_config.temp_dir) - 1);
        g_config.max_concurrent_apps = APP_LOADER_MAX_INSTANCES;
        g_config.default_memory_limit = 16 * 1024 * 1024; /* 16MB */
        g_config.app_timeout = APP_LOADER_DEFAULT_TIMEOUT;
        g_config.gui_enabled = true;
        g_config.cli_enabled = true;
        g_config.auto_cleanup = true;
    }
    
    /* Initialize statistics */
    memset(&g_stats, 0, sizeof(g_stats));
    
    /* Clear registry and instances */
    memset(g_app_registry, 0, sizeof(g_app_registry));
    memset(g_app_registry_slots, 0, sizeof(g_app_registry_slots));
    memset(g_app_instances, 0, sizeof(g_app_instances));
    memset(g_instance_slots, 0, sizeof(g_instance_slots));
    g_app_registry_count = 0;
    g_instance_count = 0;
    g_next_instance_id = 1;
    
    /* Initialize underlying systems */
    if (app_loader_init() != 0) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Failed to initialize user application loader");
        return APP_ERROR_LAUNCH_FAILED;
    }
    
    /* Check GUI availability */
    if (g_config.gui_enabled) {
        if (gui_init() != 0) {
            KLOG_WARN(LOG_CAT_PROCESS, "GUI system not available, disabling GUI support");
            g_config.gui_enabled = false;
        }
    }
    
    /* Register built-in applications */
    KLOG_DEBUG(LOG_CAT_PROCESS, "Registering built-in applications");
    
    /* Register shell application */
    app_descriptor_t shell_desc = {0};
    strncpy(shell_desc.name, APP_NAME_SHELL, sizeof(shell_desc.name) - 1);
    strncpy(shell_desc.path, "embedded://shell", sizeof(shell_desc.path) - 1);
    strncpy(shell_desc.description, "IKOS Command Line Shell", sizeof(shell_desc.description) - 1);
    shell_desc.type = APP_TYPE_CLI;
    shell_desc.flags = APP_FLAG_CLI_ENABLE | APP_FLAG_SYSTEM_LEVEL;
    shell_desc.memory_limit = 2 * 1024 * 1024; /* 2MB */
    shell_desc.cpu_priority = 50;
    shell_desc.installed = true;
    app_register(&shell_desc);
    
    /* Register system info application */
    app_descriptor_t sysinfo_desc = {0};
    strncpy(sysinfo_desc.name, APP_NAME_SYSTEM_INFO, sizeof(sysinfo_desc.name) - 1);
    strncpy(sysinfo_desc.path, "embedded://sysinfo", sizeof(sysinfo_desc.path) - 1);
    strncpy(sysinfo_desc.description, "System Information Utility", sizeof(sysinfo_desc.description) - 1);
    sysinfo_desc.type = APP_TYPE_HYBRID;
    sysinfo_desc.flags = APP_FLAG_GUI_ENABLE | APP_FLAG_CLI_ENABLE | APP_FLAG_AUTO_DETECT;
    sysinfo_desc.memory_limit = 1 * 1024 * 1024; /* 1MB */
    sysinfo_desc.cpu_priority = 30;
    sysinfo_desc.installed = true;
    app_register(&sysinfo_desc);
    
    /* Scan system directories for applications */
    if (g_config.system_apps_dir[0] != '\0') {
        int system_apps = app_scan_directory(g_config.system_apps_dir);
        KLOG_INFO(LOG_CAT_PROCESS, "Found %d system applications", system_apps);
    }
    
    if (g_config.user_apps_dir[0] != '\0') {
        int user_apps = app_scan_directory(g_config.user_apps_dir);
        KLOG_INFO(LOG_CAT_PROCESS, "Found %d user applications", user_apps);
    }
    
    g_app_loader_initialized = true;
    KLOG_INFO(LOG_CAT_PROCESS, "Application loader initialized with %d registered applications", 
              g_app_registry_count);
    
    return APP_ERROR_SUCCESS;
}

void app_loader_shutdown(void) {
    if (!g_app_loader_initialized) {
        return;
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "Shutting down application loader");
    
    /* Terminate all running applications */
    for (uint32_t i = 0; i < APP_LOADER_MAX_INSTANCES; i++) {
        if (g_instance_slots[i] && g_app_instances[i].process) {
            app_terminate_instance(g_app_instances[i].instance_id, true);
        }
    }
    
    /* Clean up resources */
    memset(g_app_registry, 0, sizeof(g_app_registry));
    memset(g_app_instances, 0, sizeof(g_app_instances));
    g_app_registry_count = 0;
    g_instance_count = 0;
    
    g_app_loader_initialized = false;
    KLOG_INFO(LOG_CAT_PROCESS, "Application loader shutdown complete");
}

app_loader_config_t* app_loader_get_config(void) {
    if (!g_app_loader_initialized) {
        return NULL;
    }
    return &g_config;
}

int app_loader_get_stats(app_loader_stats_t* stats) {
    if (!g_app_loader_initialized || !stats) {
        return APP_ERROR_INVALID_PARAM;
    }
    
    /* Update runtime statistics */
    g_stats.registry_size = g_app_registry_count;
    g_stats.apps_running = g_instance_count;
    g_stats.gui_apps_active = 0;
    g_stats.cli_apps_active = 0;
    g_stats.total_memory_used = 0;
    
    /* Count active applications by type */
    for (uint32_t i = 0; i < APP_LOADER_MAX_INSTANCES; i++) {
        if (g_instance_slots[i]) {
            if (g_app_instances[i].runtime_type == APP_TYPE_GUI) {
                g_stats.gui_apps_active++;
            } else if (g_app_instances[i].runtime_type == APP_TYPE_CLI) {
                g_stats.cli_apps_active++;
            }
            g_stats.total_memory_used += g_app_instances[i].memory_used;
        }
    }
    
    *stats = g_stats;
    return APP_ERROR_SUCCESS;
}

/* ================================
 * Application Registration and Discovery
 * ================================ */

int app_register(app_descriptor_t* descriptor) {
    if (!g_app_loader_initialized || !descriptor) {
        return APP_ERROR_INVALID_PARAM;
    }
    
    /* Check if application already exists */
    if (app_find_by_name(descriptor->name) != NULL) {
        KLOG_WARN(LOG_CAT_PROCESS, "Application '%s' already registered", descriptor->name);
        return APP_ERROR_ALREADY_EXISTS;
    }
    
    /* Allocate descriptor slot */
    app_descriptor_t* slot = allocate_app_descriptor();
    if (!slot) {
        KLOG_ERROR(LOG_CAT_PROCESS, "No more application registry slots available");
        return APP_ERROR_NO_MEMORY;
    }
    
    /* Copy descriptor data */
    *slot = *descriptor;
    g_app_registry_count++;
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Registered application: %s (%s)", 
               descriptor->name, descriptor->path);
    
    return APP_ERROR_SUCCESS;
}

int app_unregister(const char* name) {
    if (!g_app_loader_initialized || !name) {
        return APP_ERROR_INVALID_PARAM;
    }
    
    /* Find application */
    for (uint32_t i = 0; i < APP_LOADER_MAX_APPS; i++) {
        if (g_app_registry_slots[i] && 
            strcmp(g_app_registry[i].name, name) == 0) {
            
            /* Check if any instances are running */
            app_instance_t* instances[APP_LOADER_MAX_INSTANCES];
            int running_count = app_get_instances_by_name(name, instances, APP_LOADER_MAX_INSTANCES);
            if (running_count > 0) {
                KLOG_WARN(LOG_CAT_PROCESS, "Cannot unregister '%s': %d instances running", 
                          name, running_count);
                return APP_ERROR_RESOURCE_BUSY;
            }
            
            /* Free the slot */
            free_app_descriptor(&g_app_registry[i]);
            g_app_registry_count--;
            
            KLOG_DEBUG(LOG_CAT_PROCESS, "Unregistered application: %s", name);
            return APP_ERROR_SUCCESS;
        }
    }
    
    return APP_ERROR_NOT_FOUND;
}

int app_scan_directory(const char* directory_path) {
    if (!g_app_loader_initialized || !directory_path) {
        return 0;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Scanning directory for applications: %s", directory_path);
    
    int found_count = 0;
    
    /* TODO: Implement actual directory scanning with VFS */
    /* For now, return 0 as placeholder */
    KLOG_DEBUG(LOG_CAT_PROCESS, "Directory scanning not yet implemented");
    
    return found_count;
}

app_descriptor_t* app_find_by_name(const char* name) {
    if (!g_app_loader_initialized || !name) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < APP_LOADER_MAX_APPS; i++) {
        if (g_app_registry_slots[i] && 
            strcmp(g_app_registry[i].name, name) == 0) {
            return &g_app_registry[i];
        }
    }
    
    return NULL;
}

app_descriptor_t* app_find_by_path(const char* path) {
    if (!g_app_loader_initialized || !path) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < APP_LOADER_MAX_APPS; i++) {
        if (g_app_registry_slots[i] && 
            strcmp(g_app_registry[i].path, path) == 0) {
            return &g_app_registry[i];
        }
    }
    
    return NULL;
}

int app_list_all(app_descriptor_t* descriptors, uint32_t max_count) {
    if (!g_app_loader_initialized || !descriptors) {
        return 0;
    }
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < APP_LOADER_MAX_APPS && count < max_count; i++) {
        if (g_app_registry_slots[i]) {
            descriptors[count] = g_app_registry[i];
            count++;
        }
    }
    
    return count;
}

int app_list_by_type(app_type_t type, app_descriptor_t* descriptors, uint32_t max_count) {
    if (!g_app_loader_initialized || !descriptors) {
        return 0;
    }
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < APP_LOADER_MAX_APPS && count < max_count; i++) {
        if (g_app_registry_slots[i] && 
            (g_app_registry[i].type == type || g_app_registry[i].type == APP_TYPE_HYBRID)) {
            descriptors[count] = g_app_registry[i];
            count++;
        }
    }
    
    return count;
}

/* ================================
 * Application Execution
 * ================================ */

int32_t app_launch_by_name(const char* name, char* const argv[], char* const envp[], 
                          app_launch_mode_t mode, uint32_t flags) {
    if (!g_app_loader_initialized || !name) {
        return APP_ERROR_INVALID_PARAM;
    }
    
    /* Find application descriptor */
    app_descriptor_t* descriptor = app_find_by_name(name);
    if (!descriptor) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Application not found: %s", name);
        g_stats.launch_failures++;
        return APP_ERROR_NOT_FOUND;
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "Launching application: %s", name);
    
    /* Allocate instance */
    app_instance_t* instance = allocate_app_instance();
    if (!instance) {
        KLOG_ERROR(LOG_CAT_PROCESS, "No instance slots available");
        g_stats.launch_failures++;
        return APP_ERROR_NO_MEMORY;
    }
    
    /* Initialize instance */
    instance->instance_id = g_next_instance_id++;
    instance->descriptor = descriptor;
    instance->runtime_type = descriptor->type;
    instance->launch_mode = mode;
    instance->flags = flags | descriptor->flags;
    instance->start_time = 0; /* TODO: Get current time */
    instance->is_responding = true;
    
    /* Copy arguments */
    /* TODO: Implement argument copying */
    instance->argv = NULL;
    instance->envp = NULL;
    
    /* Determine final application type */
    if (flags & APP_FLAG_AUTO_DETECT) {
        /* Auto-detect based on available subsystems and path */
        if (g_config.gui_enabled && (flags & APP_FLAG_GUI_ENABLE)) {
            instance->runtime_type = APP_TYPE_GUI;
        } else if (g_config.cli_enabled && (flags & APP_FLAG_CLI_ENABLE)) {
            instance->runtime_type = APP_TYPE_CLI;
        }
    }
    
    /* Launch application using appropriate method */
    int32_t process_id = APP_ERROR_LAUNCH_FAILED;
    
    if (strncmp(descriptor->path, "embedded://", 11) == 0) {
        /* Launch embedded application */
        const char* embedded_name = descriptor->path + 11;
        
        if (strcmp(embedded_name, "shell") == 0) {
            process_id = run_simple_shell();
        } else if (strcmp(embedded_name, "sysinfo") == 0) {
            process_id = run_system_info();
        } else if (strcmp(embedded_name, "hello") == 0) {
            process_id = run_hello_world();
        }
    } else {
        /* Launch from file system */
        process_id = load_user_application(descriptor->path, (const char* const*)argv, 
                                         (const char* const*)envp);
    }
    
    if (process_id < 0) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Failed to launch application: %s", name);
        free_app_instance(instance);
        g_stats.launch_failures++;
        return process_id;
    }
    
    /* Get process reference */
    instance->process = process_get_by_pid((uint32_t)process_id);
    if (!instance->process) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Cannot find launched process: PID %d", process_id);
        free_app_instance(instance);
        g_stats.launch_failures++;
        return APP_ERROR_LAUNCH_FAILED;
    }
    
    /* Setup environment */
    int result = setup_app_environment(instance);
    if (result != APP_ERROR_SUCCESS) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Failed to setup environment for %s", name);
        app_terminate_instance(instance->instance_id, true);
        return result;
    }
    
    /* Update statistics */
    g_stats.apps_loaded++;
    g_instance_count++;
    
    /* Update descriptor statistics */
    descriptor->last_run_time = instance->start_time;
    descriptor->run_count++;
    
    KLOG_INFO(LOG_CAT_PROCESS, "Successfully launched %s (Instance ID: %u, PID: %d)", 
              name, instance->instance_id, process_id);
    
    return (int32_t)instance->instance_id;
}

int32_t app_launch_by_path(const char* path, char* const argv[], char* const envp[], 
                          app_launch_mode_t mode, uint32_t flags) {
    if (!g_app_loader_initialized || !path) {
        return APP_ERROR_INVALID_PARAM;
    }
    
    /* Check if application is registered */
    app_descriptor_t* descriptor = app_find_by_path(path);
    if (descriptor) {
        /* Use registered application */
        return app_launch_by_name(descriptor->name, argv, envp, mode, flags);
    }
    
    /* Create temporary descriptor for unregistered application */
    app_descriptor_t temp_desc = {0};
    strncpy(temp_desc.name, "unknown", sizeof(temp_desc.name) - 1);
    strncpy(temp_desc.path, path, sizeof(temp_desc.path) - 1);
    strncpy(temp_desc.description, "Unregistered application", sizeof(temp_desc.description) - 1);
    temp_desc.type = app_detect_type_from_path(path);
    temp_desc.flags = flags;
    temp_desc.memory_limit = g_config.default_memory_limit;
    temp_desc.cpu_priority = 50;
    temp_desc.installed = false;
    
    /* Launch directly */
    return app_execute_file(path, argv, envp);
}

int32_t app_launch_gui(const char* name, char* const argv[], char* const envp[], 
                      gui_window_t* parent_window) {
    if (!g_config.gui_enabled) {
        KLOG_ERROR(LOG_CAT_PROCESS, "GUI subsystem not available");
        return APP_ERROR_INVALID_TYPE;
    }
    
    uint32_t flags = APP_FLAG_GUI_ENABLE;
    if (parent_window) {
        /* TODO: Handle parent window relationship */
    }
    
    return app_launch_by_name(name, argv, envp, APP_LAUNCH_FOREGROUND, flags);
}

int32_t app_launch_cli(const char* name, char* const argv[], char* const envp[], 
                      uint32_t terminal_id) {
    if (!g_config.cli_enabled) {
        KLOG_ERROR(LOG_CAT_PROCESS, "CLI subsystem not available");
        return APP_ERROR_INVALID_TYPE;
    }
    
    uint32_t flags = APP_FLAG_CLI_ENABLE;
    /* TODO: Handle terminal_id */
    (void)terminal_id;
    
    return app_launch_by_name(name, argv, envp, APP_LAUNCH_FOREGROUND, flags);
}

int32_t app_execute_file(const char* path, char* const argv[], char* const envp[]) {
    if (!g_app_loader_initialized || !path) {
        return APP_ERROR_INVALID_PARAM;
    }
    
    /* Detect application type */
    app_type_t type = app_detect_type_from_path(path);
    
    /* Launch with appropriate flags */
    uint32_t flags = APP_FLAG_AUTO_DETECT;
    if (type == APP_TYPE_GUI) {
        flags |= APP_FLAG_GUI_ENABLE;
    } else if (type == APP_TYPE_CLI) {
        flags |= APP_FLAG_CLI_ENABLE;
    }
    
    return app_launch_by_path(path, argv, envp, APP_LAUNCH_FOREGROUND, flags);
}

/* ================================
 * Application Instance Management
 * ================================ */

app_instance_t* app_get_instance(uint32_t instance_id) {
    if (!g_app_loader_initialized) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < APP_LOADER_MAX_INSTANCES; i++) {
        if (g_instance_slots[i] && g_app_instances[i].instance_id == instance_id) {
            return &g_app_instances[i];
        }
    }
    
    return NULL;
}

int app_get_all_instances(app_instance_t** instances, uint32_t max_count) {
    if (!g_app_loader_initialized || !instances) {
        return 0;
    }
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < APP_LOADER_MAX_INSTANCES && count < max_count; i++) {
        if (g_instance_slots[i]) {
            instances[count] = &g_app_instances[i];
            count++;
        }
    }
    
    return count;
}

int app_get_instances_by_name(const char* name, app_instance_t** instances, uint32_t max_count) {
    if (!g_app_loader_initialized || !name || !instances) {
        return 0;
    }
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < APP_LOADER_MAX_INSTANCES && count < max_count; i++) {
        if (g_instance_slots[i] && g_app_instances[i].descriptor &&
            strcmp(g_app_instances[i].descriptor->name, name) == 0) {
            instances[count] = &g_app_instances[i];
            count++;
        }
    }
    
    return count;
}

int app_terminate_instance(uint32_t instance_id, bool force) {
    if (!g_app_loader_initialized) {
        return APP_ERROR_INVALID_PARAM;
    }
    
    app_instance_t* instance = app_get_instance(instance_id);
    if (!instance) {
        return APP_ERROR_NOT_FOUND;
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "Terminating application instance %u (%s)", 
              instance_id, instance->descriptor ? instance->descriptor->name : "unknown");
    
    /* Terminate the process */
    if (instance->process) {
        if (force) {
            process_kill(instance->process);
        } else {
            process_terminate(instance->process);
        }
    }
    
    /* Cleanup instance */
    cleanup_app_instance_internal(instance);
    
    return APP_ERROR_SUCCESS;
}

/* ================================
 * Application Type Detection
 * ================================ */

app_type_t app_detect_type_from_elf(const void* elf_data, size_t size) {
    if (!elf_data || size < sizeof(elf64_header_t)) {
        return APP_TYPE_UNKNOWN;
    }
    
    /* Basic ELF validation */
    const elf64_header_t* header = (const elf64_header_t*)elf_data;
    if (header->e_ident[EI_MAG0] != ELFMAG0 ||
        header->e_ident[EI_MAG1] != ELFMAG1 ||
        header->e_ident[EI_MAG2] != ELFMAG2 ||
        header->e_ident[EI_MAG3] != ELFMAG3) {
        return APP_TYPE_UNKNOWN;
    }
    
    /* TODO: Implement more sophisticated type detection */
    /* For now, assume CLI for all ELF binaries */
    return APP_TYPE_CLI;
}

app_type_t app_detect_type_from_path(const char* path) {
    if (!path) {
        return APP_TYPE_UNKNOWN;
    }
    
    /* Check for embedded applications */
    if (strncmp(path, "embedded://", 11) == 0) {
        const char* name = path + 11;
        if (strcmp(name, "shell") == 0) {
            return APP_TYPE_CLI;
        } else if (strcmp(name, "sysinfo") == 0) {
            return APP_TYPE_HYBRID;
        }
        return APP_TYPE_CLI;
    }
    
    /* TODO: Check file extension or read ELF headers */
    /* For now, assume CLI */
    return APP_TYPE_CLI;
}

app_type_t app_detect_type_from_process(process_t* proc) {
    if (!proc) {
        return APP_TYPE_UNKNOWN;
    }
    
    /* TODO: Implement process-based type detection */
    return APP_TYPE_CLI;
}

/* ================================
 * Internal Helper Functions
 * ================================ */

static app_descriptor_t* allocate_app_descriptor(void) {
    for (uint32_t i = 0; i < APP_LOADER_MAX_APPS; i++) {
        if (!g_app_registry_slots[i]) {
            g_app_registry_slots[i] = true;
            return &g_app_registry[i];
        }
    }
    return NULL;
}

static void free_app_descriptor(app_descriptor_t* descriptor) {
    if (!descriptor) return;
    
    /* Find the slot index */
    for (uint32_t i = 0; i < APP_LOADER_MAX_APPS; i++) {
        if (&g_app_registry[i] == descriptor) {
            g_app_registry_slots[i] = false;
            memset(descriptor, 0, sizeof(app_descriptor_t));
            break;
        }
    }
}

static app_instance_t* allocate_app_instance(void) {
    for (uint32_t i = 0; i < APP_LOADER_MAX_INSTANCES; i++) {
        if (!g_instance_slots[i]) {
            g_instance_slots[i] = true;
            memset(&g_app_instances[i], 0, sizeof(app_instance_t));
            return &g_app_instances[i];
        }
    }
    return NULL;
}

static void free_app_instance(app_instance_t* instance) {
    if (!instance) return;
    
    /* Find the slot index */
    for (uint32_t i = 0; i < APP_LOADER_MAX_INSTANCES; i++) {
        if (&g_app_instances[i] == instance) {
            g_instance_slots[i] = false;
            memset(instance, 0, sizeof(app_instance_t));
            if (g_instance_count > 0) {
                g_instance_count--;
            }
            break;
        }
    }
}

static int setup_app_environment(app_instance_t* instance) {
    if (!instance) {
        return APP_ERROR_INVALID_PARAM;
    }
    
    /* Setup based on application type */
    if (instance->runtime_type == APP_TYPE_GUI) {
        return app_setup_gui_environment(instance);
    } else if (instance->runtime_type == APP_TYPE_CLI) {
        return app_setup_cli_environment(instance, 0);
    }
    
    return APP_ERROR_SUCCESS;
}

int app_setup_gui_environment(app_instance_t* instance) {
    if (!instance || !g_config.gui_enabled) {
        return APP_ERROR_INVALID_TYPE;
    }
    
    /* TODO: Setup GUI environment */
    KLOG_DEBUG(LOG_CAT_PROCESS, "Setting up GUI environment for instance %u", 
               instance->instance_id);
    
    return APP_ERROR_SUCCESS;
}

int app_setup_cli_environment(app_instance_t* instance, uint32_t terminal_id) {
    if (!instance || !g_config.cli_enabled) {
        return APP_ERROR_INVALID_TYPE;
    }
    
    /* TODO: Setup CLI environment */
    KLOG_DEBUG(LOG_CAT_PROCESS, "Setting up CLI environment for instance %u (terminal %u)", 
               instance->instance_id, terminal_id);
    
    return APP_ERROR_SUCCESS;
}

static void cleanup_app_instance_internal(app_instance_t* instance) {
    if (!instance) return;
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Cleaning up application instance %u", instance->instance_id);
    
    /* Cleanup environment */
    app_cleanup_environment(instance);
    
    /* Update statistics */
    g_stats.apps_terminated++;
    
    /* Free the instance */
    free_app_instance(instance);
}

void app_cleanup_environment(app_instance_t* instance) {
    if (!instance) return;
    
    /* Cleanup GUI resources */
    if (instance->main_window) {
        gui_destroy_window(instance->main_window);
        instance->main_window = NULL;
    }
    
    /* TODO: Cleanup CLI resources */
    
    /* Free argument arrays */
    if (instance->argv) {
        /* TODO: Free argv array */
    }
    
    if (instance->envp) {
        /* TODO: Free envp array */
    }
}
