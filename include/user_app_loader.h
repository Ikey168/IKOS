/* IKOS Simple User-Space Application Loader - Issue #17
 * Provides file system integration for loading user-space executables
 */

#ifndef USER_APP_LOADER_H
#define USER_APP_LOADER_H

#include <stdint.h>
#include <stdbool.h>
#include "process.h"
#include "vfs.h"

/* Application loading result codes */
typedef enum {
    APP_LOAD_SUCCESS = 0,
    APP_LOAD_FILE_NOT_FOUND = -1,
    APP_LOAD_INVALID_ELF = -2,
    APP_LOAD_NO_MEMORY = -3,
    APP_LOAD_PROCESS_CREATION_FAILED = -4,
    APP_LOAD_CONTEXT_SETUP_FAILED = -5
} app_load_result_t;

/* Application information structure */
typedef struct {
    char name[64];
    char path[256];
    uint32_t size;
    bool is_executable;
    uint32_t permissions;
} app_info_t;

/* ========================== Core Loading Functions ========================== */

/**
 * Load and execute a user-space application from the file system
 * @param path Full path to the executable file
 * @param args Command line arguments (NULL-terminated array)
 * @param env Environment variables (NULL-terminated array)
 * @return Process ID on success, negative error code on failure
 */
int32_t load_user_application(const char* path, const char* const* args, const char* const* env);

/**
 * Load an application from memory (embedded binary)
 * @param name Application name
 * @param binary_data Pointer to ELF binary data
 * @param size Size of binary data
 * @param args Command line arguments
 * @return Process ID on success, negative error code on failure
 */
int32_t load_embedded_application(const char* name, const void* binary_data, size_t size, const char* const* args);

/**
 * Execute a user-space process that has been loaded
 * @param proc Process to execute
 * @return 0 on success, negative error code on failure
 */
int execute_user_process(process_t* proc);

/* ========================== File System Integration ========================== */

/**
 * Initialize the application loader with file system integration
 * @return 0 on success, negative error code on failure
 */
int app_loader_init(void);

/**
 * Check if a file is an executable ELF binary
 * @param path Path to the file
 * @return true if executable, false otherwise
 */
bool is_executable_file(const char* path);

/**
 * Get information about an application file
 * @param path Path to the application
 * @param info Pointer to structure to fill with information
 * @return 0 on success, negative error code on failure
 */
int get_application_info(const char* path, app_info_t* info);

/**
 * List available applications in a directory
 * @param directory_path Directory to scan for applications
 * @param app_list Array to store application information
 * @param max_apps Maximum number of applications to return
 * @return Number of applications found, negative error code on failure
 */
int list_applications(const char* directory_path, app_info_t* app_list, uint32_t max_apps);

/* ========================== Built-in Applications ========================== */

/**
 * Load and run the hello world test application
 * @return Process ID on success, negative error code on failure
 */
int32_t run_hello_world(void);

/**
 * Load and run a simple shell application
 * @return Process ID on success, negative error code on failure
 */
int32_t run_simple_shell(void);

/**
 * Load and run a system information application
 * @return Process ID on success, negative error code on failure
 */
int32_t run_system_info(void);

/**
 * Load and run an IPC test application
 * @return Process ID on success, negative error code on failure
 */
int32_t run_ipc_test(void);

/* ========================== Process Management Integration ========================== */

/**
 * Start the init process (first user-space process)
 * @return Process ID on success, negative error code on failure
 */
int32_t start_init_process(void);

/**
 * Create a new user process by forking the current process
 * @return Process ID of child on success (in parent), 0 in child, negative on error
 */
int32_t fork_user_process(void);

/**
 * Replace the current process with a new executable
 * @param path Path to the new executable
 * @param args Command line arguments
 * @param env Environment variables
 * @return Does not return on success, negative error code on failure
 */
int exec_user_process(const char* path, const char* const* args, const char* const* env);

/**
 * Wait for a child process to terminate
 * @param pid Process ID to wait for (0 = any child)
 * @param status Pointer to store exit status
 * @return Process ID of terminated child, negative error code on failure
 */
int32_t wait_for_process(int32_t pid, int* status);

/* ========================== Context Switching Helpers ========================== */

/**
 * Switch from kernel mode to user mode
 * @param proc Process to switch to
 * @return Does not return on success, negative error code on failure
 */
int switch_to_user_mode(process_t* proc);

/**
 * Handle return from user mode to kernel mode
 * @param interrupt_frame Saved interrupt frame from user mode
 * @return 0 to continue process, negative to terminate
 */
int handle_user_mode_return(interrupt_frame_t* interrupt_frame);

/**
 * Set up initial user mode context for a new process
 * @param proc Process to set up
 * @param entry_point Entry point address in user space
 * @param stack_top Top of user stack
 * @param args Command line arguments
 * @return 0 on success, negative error code on failure
 */
int setup_user_context(process_t* proc, uint64_t entry_point, uint64_t stack_top, const char* const* args);

/* ========================== Application Utilities ========================== */

/**
 * Validate ELF executable for user-space execution
 * @param elf_data Pointer to ELF data
 * @param size Size of ELF data
 * @return true if valid user-space ELF, false otherwise
 */
bool validate_user_elf(const void* elf_data, size_t size);

/**
 * Parse command line arguments string into array
 * @param command_line Command line string
 * @param args Array to store parsed arguments
 * @param max_args Maximum number of arguments
 * @return Number of arguments parsed
 */
int parse_command_line(const char* command_line, const char** args, uint32_t max_args);

/**
 * Set up environment variables for a user process
 * @param proc Process to set up environment for
 * @param env Environment variables array
 * @return 0 on success, negative error code on failure
 */
int setup_process_environment(process_t* proc, const char* const* env);

/* ========================== Security and Validation ========================== */

/**
 * Validate user-space address range
 * @param addr Starting address
 * @param size Size of range
 * @return true if valid user space range, false otherwise
 */
bool validate_user_address_range(uint64_t addr, size_t size);

/**
 * Check if a user process has permission to access a file
 * @param proc Process requesting access
 * @param path File path
 * @param access_mode Requested access mode (read/write/execute)
 * @return true if access allowed, false otherwise
 */
bool check_file_access_permission(process_t* proc, const char* path, uint32_t access_mode);

/**
 * Apply security restrictions to a user process
 * @param proc Process to apply restrictions to
 * @return 0 on success, negative error code on failure
 */
int apply_security_restrictions(process_t* proc);

/* ========================== Debugging and Monitoring ========================== */

/**
 * Print information about all running user processes
 */
void print_process_list(void);

/**
 * Print detailed information about a specific process
 * @param pid Process ID to print information for
 */
void print_process_info(uint32_t pid);

/**
 * Get process statistics
 * @param stats Pointer to structure to fill with statistics
 * @return 0 on success, negative error code on failure
 */
int get_process_statistics(process_stats_t* stats);

/* ========================== Error Handling ========================== */

/**
 * Convert application loader error code to string
 * @param error_code Error code to convert
 * @return String description of error
 */
const char* app_loader_error_string(app_load_result_t error_code);

/**
 * Crash information structure
 */
typedef struct {
    uint32_t crash_type;                /* Type of crash */
    uint64_t crash_address;             /* Address where crash occurred */
    uint32_t error_code;                /* Error code */
    char description[128];              /* Crash description */
} crash_info_t;

/**
 * Handle user-space application crash
 * @param proc Process that crashed
 * @param crash_info Information about the crash
 */
void handle_application_crash(process_t* proc, crash_info_t* crash_info);

/* ========================== Constants and Limits ========================== */

#define MAX_COMMAND_LINE_ARGS   32      /* Maximum command line arguments */
#define MAX_ENVIRONMENT_VARS    64      /* Maximum environment variables */
#define MAX_APPLICATIONS       128      /* Maximum applications that can be tracked */
#define DEFAULT_USER_STACK_SIZE (2 * 1024 * 1024)  /* 2MB default stack */
#define USER_HEAP_INITIAL_SIZE  (1 * 1024 * 1024)  /* 1MB initial heap */

/* Application directories */
#define USER_APPS_DIR           "/usr/bin"
#define SYSTEM_APPS_DIR         "/bin"
#define LOCAL_APPS_DIR          "/usr/local/bin"

/* Built-in application names */
#define INIT_PROCESS_NAME       "init"
#define SHELL_PROCESS_NAME      "shell"
#define HELLO_WORLD_NAME        "hello"
#define SYSTEM_INFO_NAME        "sysinfo"
#define IPC_TEST_NAME           "ipctest"

#endif /* USER_APP_LOADER_H */
