/* IKOS Process Manager System Calls
 * User-space API for process management operations
 */

#include "process_manager.h"
#include "process.h"
#include "interrupts.h"
#include <string.h>

/* Function declarations */
static void debug_print(const char* format, ...);
static bool validate_user_pointer(const void* ptr, size_t size);
static int copy_from_user(void* dest, const void* src, size_t size);
static int copy_to_user(void* dest, const void* src, size_t size);

/* System call numbers for process manager */
#define SYS_PM_CREATE_PROCESS       200
#define SYS_PM_EXIT_PROCESS         201
#define SYS_PM_WAIT_PROCESS         202
#define SYS_PM_GET_PROCESS_INFO     203
#define SYS_PM_IPC_CREATE_CHANNEL   204
#define SYS_PM_IPC_SEND             205
#define SYS_PM_IPC_RECEIVE          206
#define SYS_PM_IPC_BROADCAST        207
#define SYS_PM_GET_PROCESS_LIST     208
#define SYS_PM_KILL_PROCESS         209

/* User-space process creation structure */
typedef struct {
    char name[64];              /* Process name */
    char* argv[32];             /* Arguments */
    int argc;                   /* Argument count */
    char* envp[32];             /* Environment */
    int envc;                   /* Environment count */
    int priority;               /* Process priority */
    uint32_t flags;             /* Creation flags */
} user_process_create_t;

/* User-space IPC message structure */
typedef struct {
    uint32_t type;              /* Message type */
    uint32_t dst_pid;           /* Destination PID */
    uint32_t channel_id;        /* Channel ID */
    uint32_t data_size;         /* Data size */
    void* data;                 /* Data pointer */
} user_ipc_message_t;

/**
 * System call: Create process
 */
int sys_pm_create_process(const char* name, char* const argv[], char* const envp[]) {
    if (!validate_user_pointer(name, 64) || !name[0]) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    /* Get current process to set as parent */
    process_t* current = process_get_current();
    if (!current) {
        return PM_ERROR_INVALID_STATE;
    }
    
    /* Create process creation parameters */
    pm_create_params_t params = {0};
    
    /* Copy process name */
    if (copy_from_user(params.name, name, sizeof(params.name)) != 0) {
        return PM_ERROR_INVALID_PARAM;
    }
    params.name[sizeof(params.name) - 1] = '\0';
    
    /* Copy arguments if provided */
    params.argc = 0;
    if (argv && validate_user_pointer(argv, sizeof(char*))) {
        for (int i = 0; i < PM_MAX_PROCESS_ARGS && argv[i]; i++) {
            if (!validate_user_pointer(argv[i], 256)) {
                break;
            }
            /* For now, just count arguments - full copying would be implemented here */
            params.argc++;
        }
    }
    
    /* Copy environment if provided */
    params.envc = 0;
    if (envp && validate_user_pointer(envp, sizeof(char*))) {
        for (int i = 0; i < PM_MAX_PROCESS_ARGS && envp[i]; i++) {
            if (!validate_user_pointer(envp[i], 256)) {
                break;
            }
            /* For now, just count environment variables */
            params.envc++;
        }
    }
    
    /* Set default parameters */
    params.priority = PROCESS_PRIORITY_NORMAL;
    params.memory_limit = 0; /* No limit */
    params.time_limit = 0;   /* No limit */
    params.flags = 0;
    
    /* Create the process */
    uint32_t new_pid;
    int result = pm_create_process(&params, &new_pid);
    
    if (result == PM_SUCCESS) {
        debug_print("Process Manager: Created process '%s' with PID %u (parent PID %u)\n", 
                   params.name, new_pid, current->pid);
        return (int)new_pid;
    }
    
    return result;
}

/**
 * System call: Exit current process
 */
int sys_pm_exit_process(int exit_code) {
    process_t* current = process_get_current();
    if (!current) {
        return PM_ERROR_INVALID_STATE;
    }
    
    debug_print("Process Manager: Process PID %u exiting with code %d\n", 
               current->pid, exit_code);
    
    return pm_terminate_process(current->pid, exit_code);
}

/**
 * System call: Wait for process
 */
int sys_pm_wait_process(uint32_t pid) {
    if (pid == 0) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    process_t* current = process_get_current();
    if (!current) {
        return PM_ERROR_INVALID_STATE;
    }
    
    /* Check if process exists and is a child */
    process_t* target = pm_get_process(pid);
    if (!target) {
        return PM_ERROR_NOT_FOUND;
    }
    
    /* For now, return immediately - full wait implementation would block */
    debug_print("Process Manager: Process PID %u waiting for PID %u\n", 
               current->pid, pid);
    
    return PM_SUCCESS;
}

/**
 * System call: Get process information
 */
int sys_pm_get_process_info(uint32_t pid, process_t* info) {
    if (pid == 0 || !validate_user_pointer(info, sizeof(process_t))) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    process_t* process = pm_get_process(pid);
    if (!process) {
        return PM_ERROR_NOT_FOUND;
    }
    
    /* Copy process information to user space */
    if (copy_to_user(info, process, sizeof(process_t)) != 0) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    return PM_SUCCESS;
}

/**
 * System call: Create IPC channel
 */
int sys_pm_ipc_create_channel(uint32_t* channel_id) {
    if (!validate_user_pointer(channel_id, sizeof(uint32_t))) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    process_t* current = process_get_current();
    if (!current) {
        return PM_ERROR_INVALID_STATE;
    }
    
    uint32_t new_channel_id;
    int result = pm_ipc_create_channel(current->pid, &new_channel_id);
    
    if (result == PM_SUCCESS) {
        if (copy_to_user(channel_id, &new_channel_id, sizeof(uint32_t)) != 0) {
            /* Clean up created channel */
            pm_ipc_destroy_channel(new_channel_id);
            return PM_ERROR_INVALID_PARAM;
        }
        
        debug_print("Process Manager: Created IPC channel %u for PID %u\n", 
                   new_channel_id, current->pid);
    }
    
    return result;
}

/**
 * System call: Send IPC message
 */
int sys_pm_ipc_send(uint32_t channel_id, const void* data, size_t size) {
    if (channel_id == 0 || size > PM_IPC_BUFFER_SIZE || 
        !validate_user_pointer(data, size)) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    process_t* current = process_get_current();
    if (!current) {
        return PM_ERROR_INVALID_STATE;
    }
    
    /* Create IPC message */
    pm_ipc_message_t message = {0};
    message.type = PM_IPC_REQUEST;
    message.src_pid = current->pid;
    message.dst_pid = 0; /* Broadcast or channel-based */
    message.channel_id = channel_id;
    message.data_size = (uint32_t)size;
    
    /* Copy data from user space */
    if (copy_from_user(message.data, data, size) != 0) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    debug_print("Process Manager: IPC send from PID %u on channel %u (%zu bytes)\n", 
               current->pid, channel_id, size);
    
    return pm_ipc_send_message(&message);
}

/**
 * System call: Receive IPC message
 */
int sys_pm_ipc_receive(uint32_t channel_id, void* buffer, size_t size) {
    if (channel_id == 0 || size == 0 || !validate_user_pointer(buffer, size)) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    process_t* current = process_get_current();
    if (!current) {
        return PM_ERROR_INVALID_STATE;
    }
    
    pm_ipc_message_t message;
    int result = pm_ipc_receive_message(current->pid, channel_id, &message);
    
    if (result == PM_SUCCESS) {
        /* Copy received data to user buffer */
        size_t copy_size = (message.data_size < size) ? message.data_size : size;
        if (copy_to_user(buffer, message.data, copy_size) != 0) {
            return PM_ERROR_INVALID_PARAM;
        }
        
        debug_print("Process Manager: IPC receive by PID %u on channel %u (%u bytes)\n", 
                   current->pid, channel_id, message.data_size);
        
        return (int)copy_size;
    }
    
    return result;
}

/**
 * System call: Broadcast IPC message
 */
int sys_pm_ipc_broadcast(const void* data, size_t size) {
    if (size > PM_IPC_BUFFER_SIZE || !validate_user_pointer(data, size)) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    process_t* current = process_get_current();
    if (!current) {
        return PM_ERROR_INVALID_STATE;
    }
    
    /* Create broadcast message */
    pm_ipc_message_t message = {0};
    message.type = PM_IPC_BROADCAST;
    message.src_pid = current->pid;
    message.dst_pid = 0; /* Broadcast to all */
    message.channel_id = 0;
    message.data_size = (uint32_t)size;
    
    /* Copy data from user space */
    if (copy_from_user(message.data, data, size) != 0) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    debug_print("Process Manager: Broadcast from PID %u (%zu bytes)\n", 
               current->pid, size);
    
    return pm_ipc_broadcast_message(&message);
}

/**
 * System call: Get process list
 */
int sys_pm_get_process_list(uint32_t* pids, uint32_t max_count, uint32_t* count_out) {
    if (!validate_user_pointer(pids, max_count * sizeof(uint32_t)) ||
        !validate_user_pointer(count_out, sizeof(uint32_t))) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    uint32_t count;
    int result = pm_get_process_list(pids, max_count, &count);
    
    if (result == PM_SUCCESS) {
        if (copy_to_user(count_out, &count, sizeof(uint32_t)) != 0) {
            return PM_ERROR_INVALID_PARAM;
        }
    }
    
    return result;
}

/**
 * System call: Kill process
 */
int sys_pm_kill_process(uint32_t pid, int signal) {
    if (pid == 0) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    process_t* current = process_get_current();
    if (!current) {
        return PM_ERROR_INVALID_STATE;
    }
    
    /* Check permissions - for now, any process can kill any other */
    debug_print("Process Manager: PID %u killing PID %u with signal %d\n", 
               current->pid, pid, signal);
    
    return pm_kill_process(pid, signal);
}

/**
 * Extended system call handler for process manager
 */
long handle_pm_system_call(interrupt_frame_t* frame) {
    if (!frame) {
        return -1;
    }
    
    uint64_t syscall_num = frame->rax;
    
    switch (syscall_num) {
        case SYS_PM_CREATE_PROCESS:
            return sys_pm_create_process((const char*)frame->rdi, 
                                       (char* const*)frame->rsi, 
                                       (char* const*)frame->rdx);
        
        case SYS_PM_EXIT_PROCESS:
            return sys_pm_exit_process((int)frame->rdi);
        
        case SYS_PM_WAIT_PROCESS:
            return sys_pm_wait_process((uint32_t)frame->rdi);
        
        case SYS_PM_GET_PROCESS_INFO:
            return sys_pm_get_process_info((uint32_t)frame->rdi, 
                                         (process_t*)frame->rsi);
        
        case SYS_PM_IPC_CREATE_CHANNEL:
            return sys_pm_ipc_create_channel((uint32_t*)frame->rdi);
        
        case SYS_PM_IPC_SEND:
            return sys_pm_ipc_send((uint32_t)frame->rdi, 
                                 (const void*)frame->rsi, 
                                 (size_t)frame->rdx);
        
        case SYS_PM_IPC_RECEIVE:
            return sys_pm_ipc_receive((uint32_t)frame->rdi, 
                                    (void*)frame->rsi, 
                                    (size_t)frame->rdx);
        
        case SYS_PM_IPC_BROADCAST:
            return sys_pm_ipc_broadcast((const void*)frame->rdi, 
                                      (size_t)frame->rsi);
        
        case SYS_PM_GET_PROCESS_LIST:
            return sys_pm_get_process_list((uint32_t*)frame->rdi, 
                                         (uint32_t)frame->rsi, 
                                         (uint32_t*)frame->rdx);
        
        case SYS_PM_KILL_PROCESS:
            return sys_pm_kill_process((uint32_t)frame->rdi, (int)frame->rsi);
        
        default:
            return -1; /* Unknown system call */
    }
}

/* ================================
 * Helper Functions
 * ================================ */

/**
 * Validate user space pointer
 */
static bool validate_user_pointer(const void* ptr, size_t size) {
    uint64_t addr = (uint64_t)ptr;
    
    /* Check if pointer is in user space range */
    if (addr < USER_SPACE_START || addr >= USER_SPACE_END) {
        return false;
    }
    
    /* Check if the entire range is in user space */
    if (addr + size > USER_SPACE_END) {
        return false;
    }
    
    return true;
}

/**
 * Copy data from user space to kernel space
 */
static int copy_from_user(void* dest, const void* src, size_t size) {
    if (!validate_user_pointer(src, size)) {
        return -1;
    }
    
    /* In a real kernel, this would use safe memory copying */
    memcpy(dest, src, size);
    return 0;
}

/**
 * Copy data from kernel space to user space
 */
static int copy_to_user(void* dest, const void* src, size_t size) {
    if (!validate_user_pointer(dest, size)) {
        return -1;
    }
    
    /* In a real kernel, this would use safe memory copying */
    memcpy(dest, src, size);
    return 0;
}

/**
 * Simple debug print function
 */
static void debug_print(const char* format, ...) {
    /* In a real kernel, this would output to the console/log */
    /* For now, it's a placeholder */
    (void)format;
}
