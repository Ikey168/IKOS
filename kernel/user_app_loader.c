/* IKOS Simple User-Space Application Loader Implementation - Issue #17
 * Provides file system integration for loading user-space executables
 */

#include "../include/user_app_loader.h"
#include "../include/process.h"
#include "../include/vfs.h"
#include "../include/elf.h"
#include "../include/kernel_log.h"
#include "../include/syscalls.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* External data for built-in applications */
extern const uint8_t hello_world_binary[];
extern const size_t hello_world_binary_size;

/* Global application loader state */
static bool app_loader_initialized = false;
static app_info_t known_applications[MAX_APPLICATIONS];
static uint32_t num_known_applications = 0;

/* Statistics */
static struct {
    uint32_t applications_loaded;
    uint32_t applications_running;
    uint32_t applications_crashed;
    uint32_t load_failures;
} app_loader_stats = {0};

/* ========================== Core Loading Functions ========================== */

int32_t load_user_application(const char* path, const char* const* args, const char* const* env) {
    if (!app_loader_initialized) {
        KLOG_ERROR(LOG_CAT_PROCESSESS, "Application loader not initialized");
        return APP_LOAD_PROCESS_CREATION_FAILED;
    }
    
    if (!path) {
        KLOG_ERROR(LOG_CAT_PROCESSESS, "Invalid path provided");
        return APP_LOAD_FILE_NOT_FOUND;
    }
    
    KLOG_INFO(LOG_CAT_PROCESSESS, "Loading user application: %s", path);
    
    /* Check if file exists and is executable */
    if (!is_executable_file(path)) {
        KLOG_ERROR(LOG_CAT_PROCESS, "File not found or not executable: %s", path);
        app_loader_stats.load_failures++;
        return APP_LOAD_FILE_NOT_FOUND;
    }
    
    /* Open and read the file */
    file_handle_t file = vfs_open(path, VFS_O_RDONLY, 0);
    if (file == VFS_INVALID_HANDLE) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Failed to open file: %s", path);
        app_loader_stats.load_failures++;
        return APP_LOAD_FILE_NOT_FOUND;
    }
    
    /* Get file size */
    vfs_stat_t file_stats;
    if (vfs_stat(path, &file_stats) != VFS_SUCCESS) {
        vfs_close(file);
        KLOG_ERROR(LOG_CAT_PROCESS, "Failed to get file stats: %s", path);
        app_loader_stats.load_failures++;
        return APP_LOAD_FILE_NOT_FOUND;
    }
    
    /* Allocate buffer for file contents */
    void* file_buffer = kalloc(file_stats.size);
    if (!file_buffer) {
        vfs_close(file);
        KLOG_ERROR(LOG_CAT_PROCESS, "Failed to allocate memory for file: %s", path);
        app_loader_stats.load_failures++;
        return APP_LOAD_NO_MEMORY;
    }
    
    /* Read file contents */
    ssize_t bytes_read = vfs_read(file, file_buffer, file_stats.size);
    vfs_close(file);
    
    if (bytes_read != (ssize_t)file_stats.size) {
        kfree(file_buffer);
        KLOG_ERROR(LOG_CAT_PROCESS, "Failed to read complete file: %s", path);
        app_loader_stats.load_failures++;
        return APP_LOAD_FILE_NOT_FOUND;
    }
    
    /* Load the application from memory */
    int32_t result = load_embedded_application(path, file_buffer, file_stats.size, args);
    
    /* Free the buffer */
    kfree(file_buffer);
    
    return result;
}

int32_t load_embedded_application(const char* name, const void* binary_data, size_t size, const char* const* args) {
    if (!app_loader_initialized) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Application loader not initialized");
        return APP_LOAD_PROCESS_CREATION_FAILED;
    }
    
    if (!name || !binary_data || size == 0) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Invalid parameters for embedded application load");
        return APP_LOAD_INVALID_ELF;
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "Loading embedded application: %s (%zu bytes)", name, size);
    
    /* Validate ELF binary */
    if (!validate_user_elf(binary_data, size)) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Invalid ELF binary: %s", name);
        app_loader_stats.load_failures++;
        return APP_LOAD_INVALID_ELF;
    }
    
    /* Create process from ELF */
    process_t* proc = process_create_from_elf(name, (void*)binary_data, size);
    if (!proc) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Failed to create process: %s", name);
        app_loader_stats.load_failures++;
        return APP_LOAD_PROCESS_CREATION_FAILED;
    }
    
    /* Set up command line arguments */
    if (args) {
        /* TODO: Implement argument passing to user space */
        KLOG_DEBUG(LOG_CAT_PROCESS, "Command line arguments not yet implemented");
    }
    
    /* Execute the process */
    int result = execute_user_process(proc);
    if (result != 0) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Failed to execute process: %s", name);
        process_terminate(proc);
        app_loader_stats.load_failures++;
        return APP_LOAD_CONTEXT_SETUP_FAILED;
    }
    
    app_loader_stats.applications_loaded++;
    app_loader_stats.applications_running++;
    
    KLOG_INFO(LOG_CAT_PROCESS, "Successfully loaded and started application: %s (PID %u)", name, proc->pid);
    return (int32_t)proc->pid;
}

int execute_user_process(process_t* proc) {
    if (!proc) {
        return APP_LOAD_PROCESS_CREATION_FAILED;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Executing user process: %s (PID %u)", proc->name, proc->pid);
    
    /* Set up user mode context */
    int result = setup_user_context(proc, proc->entry_point, proc->stack_start + proc->stack_size, NULL);
    if (result != 0) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Failed to set up user context for process %u", proc->pid);
        return result;
    }
    
    /* Add process to ready queue */
    process_add_to_ready_queue(proc);
    
    /* If this is the first process, switch to it immediately */
    if (current_process == NULL) {
        KLOG_INFO(LOG_CAT_PROCESS, "Starting first user process: %s", proc->name);
        result = switch_to_user_mode(proc);
        if (result != 0) {
            KLOG_ERROR(LOG_CAT_PROCESS, "Failed to switch to user mode for process %u", proc->pid);
            return result;
        }
    }
    
    return 0;
}

/* ========================== File System Integration ========================== */

int app_loader_init(void) {
    if (app_loader_initialized) {
        return 0;
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "Initializing application loader");
    
    /* Clear known applications list */
    memset(known_applications, 0, sizeof(known_applications));
    num_known_applications = 0;
    
    /* Clear statistics */
    memset(&app_loader_stats, 0, sizeof(app_loader_stats));
    
    /* Scan for built-in applications */
    KLOG_DEBUG(LOG_CAT_PROCESS, "Registering built-in applications");
    
    /* Register hello world */
    if (num_known_applications < MAX_APPLICATIONS) {
        strncpy(known_applications[num_known_applications].name, HELLO_WORLD_NAME, 
                sizeof(known_applications[num_known_applications].name) - 1);
        strncpy(known_applications[num_known_applications].path, "embedded://hello", 
                sizeof(known_applications[num_known_applications].path) - 1);
        known_applications[num_known_applications].is_executable = true;
        known_applications[num_known_applications].size = 0; /* Will be set when loaded */
        num_known_applications++;
    }
    
    app_loader_initialized = true;
    KLOG_INFO(LOG_CAT_PROCESS, "Application loader initialized with %u known applications", num_known_applications);
    
    return 0;
}

bool is_executable_file(const char* path) {
    if (!path) {
        return false;
    }
    
    /* Check for embedded applications */
    if (strncmp(path, "embedded://", 11) == 0) {
        return true;
    }
    
    /* Check file system */
    vfs_stat_t stats;
    if (vfs_stat(path, &stats) != VFS_SUCCESS) {
        return false;
    }
    
    /* Check if file is executable */
    if (!(stats.permissions & VFS_PERM_EXEC)) {
        return false;
    }
    
    /* Basic ELF header check */
    file_handle_t file = vfs_open(path, VFS_O_RDONLY, 0);
    if (file == VFS_INVALID_HANDLE) {
        return false;
    }
    
    uint8_t elf_header[16];
    ssize_t bytes_read = vfs_read(file, elf_header, sizeof(elf_header));
    vfs_close(file);
    
    if (bytes_read < 16) {
        return false;
    }
    
    /* Check ELF magic number */
    if (elf_header[0] != 0x7F || elf_header[1] != 'E' || 
        elf_header[2] != 'L' || elf_header[3] != 'F') {
        return false;
    }
    
    return true;
}

int get_application_info(const char* path, app_info_t* info) {
    if (!path || !info) {
        return APP_LOAD_FILE_NOT_FOUND;
    }
    
    memset(info, 0, sizeof(app_info_t));
    
    /* Check for embedded applications */
    if (strncmp(path, "embedded://", 11) == 0) {
        const char* app_name = path + 11;
        strncpy(info->name, app_name, sizeof(info->name) - 1);
        strncpy(info->path, path, sizeof(info->path) - 1);
        info->is_executable = true;
        info->size = 0; /* Unknown for embedded */
        info->permissions = VFS_PERM_READ | VFS_PERM_EXEC;
        return 0;
    }
    
    /* Get file system information */
    vfs_stat_t stats;
    if (vfs_stat(path, &stats) != VFS_SUCCESS) {
        return APP_LOAD_FILE_NOT_FOUND;
    }
    
    /* Extract name from path */
    const char* name_start = strrchr(path, '/');
    if (name_start) {
        name_start++; /* Skip the '/' */
    } else {
        name_start = path;
    }
    
    strncpy(info->name, name_start, sizeof(info->name) - 1);
    strncpy(info->path, path, sizeof(info->path) - 1);
    info->size = stats.size;
    info->permissions = stats.permissions;
    info->is_executable = (stats.permissions & VFS_PERM_EXEC) != 0;
    
    return 0;
}

int list_applications(const char* directory_path, app_info_t* app_list, uint32_t max_apps) {
    if (!directory_path || !app_list || max_apps == 0) {
        return APP_LOAD_FILE_NOT_FOUND;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Listing applications in directory: %s", directory_path);
    
    uint32_t app_count = 0;
    
    /* Add embedded applications first */
    for (uint32_t i = 0; i < num_known_applications && app_count < max_apps; i++) {
        if (strncmp(known_applications[i].path, "embedded://", 11) == 0) {
            memcpy(&app_list[app_count], &known_applications[i], sizeof(app_info_t));
            app_count++;
        }
    }
    
    /* TODO: Scan file system directory */
    /* This would require directory reading functionality in VFS */
    KLOG_DEBUG(LOG_CAT_PROCESS, "File system directory scanning not yet implemented");
    
    KLOG_INFO(LOG_CAT_PROCESS, "Found %u applications", app_count);
    return app_count;
}

/* ========================== Built-in Applications ========================== */

int32_t run_hello_world(void) {
    KLOG_INFO(LOG_CAT_PROCESS, "Starting hello world application");
    
    /* Use embedded hello world binary if available */
    if (hello_world_binary && hello_world_binary_size > 0) {
        return load_embedded_application(HELLO_WORLD_NAME, hello_world_binary, hello_world_binary_size, NULL);
    }
    
    /* Fallback to file system */
    const char* path = "/usr/bin/hello";
    return load_user_application(path, NULL, NULL);
}

int32_t run_simple_shell(void) {
    KLOG_INFO(LOG_CAT_PROCESS, "Starting simple shell application");
    
    /* TODO: Implement simple shell application */
    KLOG_ERROR(LOG_CAT_PROCESS, "Simple shell not yet implemented");
    return APP_LOAD_FILE_NOT_FOUND;
}

int32_t run_system_info(void) {
    KLOG_INFO(LOG_CAT_PROCESS, "Starting system info application");
    
    /* TODO: Implement system info application */
    KLOG_ERROR(LOG_CAT_PROCESS, "System info application not yet implemented");
    return APP_LOAD_FILE_NOT_FOUND;
}

int32_t run_ipc_test(void) {
    KLOG_INFO(LOG_CAT_PROCESS, "Starting IPC test application");
    
    /* TODO: Implement IPC test application */
    KLOG_ERROR(LOG_CAT_PROCESS, "IPC test application not yet implemented");
    return APP_LOAD_FILE_NOT_FOUND;
}

/* ========================== Process Management Integration ========================== */

int32_t start_init_process(void) {
    KLOG_INFO(LOG_CAT_PROCESS, "Starting init process");
    
    /* Try to load init from file system first */
    int32_t pid = load_user_application("/sbin/init", NULL, NULL);
    if (pid > 0) {
        KLOG_INFO(LOG_CAT_PROCESS, "Init process started from file system (PID %d)", pid);
        return pid;
    }
    
    /* Fallback to hello world as init */
    KLOG_INFO(LOG_CAT_PROCESS, "Init not found, using hello world as init");
    pid = run_hello_world();
    if (pid > 0) {
        KLOG_INFO(LOG_CAT_PROCESS, "Hello world started as init process (PID %d)", pid);
        return pid;
    }
    
    KLOG_ERROR(LOG_CAT_PROCESS, "Failed to start any init process");
    return APP_LOAD_PROCESS_CREATION_FAILED;
}

int32_t fork_user_process(void) {
    /* TODO: Implement process forking */
    KLOG_ERROR(LOG_CAT_PROCESS, "Process forking not yet implemented");
    return APP_LOAD_PROCESS_CREATION_FAILED;
}

int exec_user_process(const char* path, const char* const* args, const char* const* env) {
    /* TODO: Implement process exec */
    KLOG_ERROR(LOG_CAT_PROCESS, "Process exec not yet implemented");
    return APP_LOAD_PROCESS_CREATION_FAILED;
}

int32_t wait_for_process(int32_t pid, int* status) {
    /* TODO: Implement process waiting */
    KLOG_ERROR(LOG_CAT_PROCESS, "Process waiting not yet implemented");
    return APP_LOAD_PROCESS_CREATION_FAILED;
}

/* ========================== Context Switching Helpers ========================== */

int switch_to_user_mode(process_t* proc) {
    if (!proc) {
        return APP_LOAD_PROCESS_CREATION_FAILED;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Switching to user mode for process %u", proc->pid);
    
    /* Set current process */
    current_process = proc;
    proc->state = PROCESS_STATE_RUNNING;
    
    /* Switch address space */
    if (proc->address_space && proc->address_space->page_directory) {
        vmm_switch_address_space(proc->address_space);
    }
    
    /* Call assembly routine to switch to user mode */
    switch_to_user_mode_asm(&proc->context);
    
    /* This should not return in normal operation */
    KLOG_ERROR(LOG_CAT_PROCESS, "Unexpected return from user mode switch");
    return APP_LOAD_CONTEXT_SETUP_FAILED;
}

int handle_user_mode_return(interrupt_frame_t* interrupt_frame) {
    if (!current_process) {
        KLOG_ERROR(LOG_CAT_PROCESS, "User mode return with no current process");
        return -1;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Handling return from user mode for process %u", current_process->pid);
    
    /* Save context from interrupt frame */
    /* TODO: Implement context saving */
    
    /* Process is no longer running */
    current_process->state = PROCESS_STATE_READY;
    
    return 0;
}

int setup_user_context(process_t* proc, uint64_t entry_point, uint64_t stack_top, const char* const* args) {
    if (!proc) {
        return APP_LOAD_PROCESS_CREATION_FAILED;
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "Setting up user context for process %u", proc->pid);
    
    /* Clear context */
    memset(&proc->context, 0, sizeof(process_context_t));
    
    /* Set up initial register state */
    proc->context.rip = entry_point;
    proc->context.rsp = stack_top;
    proc->context.rbp = stack_top;
    
    /* Set up segments for user mode */
    proc->context.cs = 0x1B;  /* User code segment (GDT entry 3, DPL 3) */
    proc->context.ds = 0x23;  /* User data segment (GDT entry 4, DPL 3) */
    proc->context.es = 0x23;
    proc->context.fs = 0x23;
    proc->context.gs = 0x23;
    proc->context.ss = 0x23;  /* User stack segment */
    
    /* Set up flags for user mode */
    proc->context.rflags = 0x202; /* Enable interrupts, set reserved bit */
    
    /* Set page directory */
    if (proc->address_space && proc->address_space->page_directory) {
        proc->context.cr3 = (uint64_t)proc->address_space->page_directory;
    }
    
    /* TODO: Set up command line arguments on stack */
    if (args) {
        KLOG_DEBUG(LOG_CAT_PROCESS, "Command line argument setup not yet implemented");
    }
    
    KLOG_DEBUG(LOG_CAT_PROCESS, "User context setup complete: entry=0x%lx, stack=0x%lx", entry_point, stack_top);
    
    return 0;
}

/* ========================== Application Utilities ========================== */

bool validate_user_elf(const void* elf_data, size_t size) {
    if (!elf_data || size < sizeof(elf64_header_t)) {
        return false;
    }
    
    const elf64_header_t* header = (const elf64_header_t*)elf_data;
    
    /* Check ELF magic number */
    if (header->e_ident[EI_MAG0] != ELFMAG0 ||
        header->e_ident[EI_MAG1] != ELFMAG1 ||
        header->e_ident[EI_MAG2] != ELFMAG2 ||
        header->e_ident[EI_MAG3] != ELFMAG3) {
        return false;
    }
    
    /* Check for 64-bit ELF */
    if (header->e_ident[EI_CLASS] != ELFCLASS64) {
        return false;
    }
    
    /* Check for little endian */
    if (header->e_ident[EI_DATA] != ELFDATA2LSB) {
        return false;
    }
    
    /* Check for executable type */
    if (header->e_type != ET_EXEC) {
        return false;
    }
    
    /* Check for x86_64 architecture */
    if (header->e_machine != EM_X86_64) {
        return false;
    }
    
    /* Validate entry point is in user space */
    if (!validate_user_address_range(header->e_entry, 1)) {
        return false;
    }
    
    return true;
}

int parse_command_line(const char* command_line, const char** args, uint32_t max_args) {
    /* TODO: Implement command line parsing */
    KLOG_DEBUG(LOG_CAT_PROCESS, "Command line parsing not yet implemented");
    return 0;
}

int setup_process_environment(process_t* proc, const char* const* env) {
    /* TODO: Implement environment variable setup */
    KLOG_DEBUG(LOG_CAT_PROCESS, "Environment variable setup not yet implemented");
    return 0;
}

/* ========================== Security and Validation ========================== */

bool validate_user_address_range(uint64_t addr, size_t size) {
    uint64_t end_addr = addr + size;
    
    /* Check for overflow */
    if (end_addr < addr) {
        return false;
    }
    
    /* Check if range is within user space */
    if (addr < USER_SPACE_START || end_addr > USER_SPACE_END) {
        return false;
    }
    
    return true;
}

bool check_file_access_permission(process_t* proc, const char* path, uint32_t access_mode) {
    /* TODO: Implement file access permission checking */
    KLOG_DEBUG(LOG_CAT_PROCESS, "File access permission checking not yet implemented");
    return true; /* Allow all access for now */
}

int apply_security_restrictions(process_t* proc) {
    /* TODO: Implement security restrictions */
    KLOG_DEBUG(LOG_CAT_PROCESS, "Security restrictions not yet implemented");
    return 0;
}

/* ========================== Debugging and Monitoring ========================== */

void print_process_list(void) {
    KLOG_INFO(LOG_CAT_PROCESS, "=== Process List ===");
    
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid != 0) {
            const char* state_str = "UNKNOWN";
            switch (processes[i].state) {
                case PROCESS_STATE_READY:      state_str = "READY"; break;
                case PROCESS_STATE_RUNNING:    state_str = "RUNNING"; break;
                case PROCESS_STATE_BLOCKED:    state_str = "BLOCKED"; break;
                case PROCESS_STATE_ZOMBIE:     state_str = "ZOMBIE"; break;
                case PROCESS_STATE_TERMINATED: state_str = "TERMINATED"; break;
            }
            
            KLOG_INFO(LOG_CAT_PROCESS, "PID %3u: %-16s %s", 
                     processes[i].pid, processes[i].name, state_str);
        }
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "=== End Process List ===");
}

void print_process_info(uint32_t pid) {
    process_t* proc = process_find_by_pid(pid);
    if (!proc) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Process not found: PID %u", pid);
        return;
    }
    
    KLOG_INFO(LOG_CAT_PROCESS, "=== Process Info: PID %u ===", pid);
    KLOG_INFO(LOG_CAT_PROCESS, "Name: %s", proc->name);
    KLOG_INFO(LOG_CAT_PROCESS, "Parent PID: %u", proc->ppid);
    KLOG_INFO(LOG_CAT_PROCESS, "State: %d", proc->state);
    KLOG_INFO(LOG_CAT_PROCESS, "Priority: %d", proc->priority);
    KLOG_INFO(LOG_CAT_PROCESS, "Entry Point: 0x%lx", proc->entry_point);
    KLOG_INFO(LOG_CAT_PROCESS, "Stack Start: 0x%lx", proc->stack_start);
    KLOG_INFO(LOG_CAT_PROCESS, "Stack Size: %lu", proc->stack_size);
    KLOG_INFO(LOG_CAT_PROCESS, "=== End Process Info ===");
}

int get_process_statistics(process_stats_t* stats) {
    if (!stats) {
        return APP_LOAD_PROCESS_CREATION_FAILED;
    }
    
    /* TODO: Implement statistics gathering */
    stats->processes_created = app_loader_stats.applications_loaded;
    stats->processes_running = app_loader_stats.applications_running;
    stats->processes_terminated = 0; /* TODO: Track this */
    stats->context_switches = 0; /* TODO: Track this */
    
    return 0;
}

/* ========================== Error Handling ========================== */

const char* app_loader_error_string(app_load_result_t error_code) {
    switch (error_code) {
        case APP_LOAD_SUCCESS:                  return "Success";
        case APP_LOAD_FILE_NOT_FOUND:          return "File not found";
        case APP_LOAD_INVALID_ELF:             return "Invalid ELF file";
        case APP_LOAD_NO_MEMORY:               return "Out of memory";
        case APP_LOAD_PROCESS_CREATION_FAILED: return "Process creation failed";
        case APP_LOAD_CONTEXT_SETUP_FAILED:    return "Context setup failed";
        default:                               return "Unknown error";
    }
}

void handle_application_crash(process_t* proc, crash_info_t* crash_info) {
    /* TODO: Implement crash handling */
    KLOG_ERROR(LOG_CAT_PROCESS, "Application crash handling not yet implemented");
    
    if (proc) {
        KLOG_ERROR(LOG_CAT_PROCESS, "Process %u (%s) crashed", proc->pid, proc->name);
        app_loader_stats.applications_crashed++;
        app_loader_stats.applications_running--;
    }
}
