/* IKOS IPC System Calls Implementation
 * Provides system call interface for IPC operations
 */

#include "ipc.h"
#include "ipc_syscalls.h"
#include "scheduler.h"

/* System call numbers for IPC operations */
#define SYS_IPC_CREATE_QUEUE        50
#define SYS_IPC_DESTROY_QUEUE       51
#define SYS_IPC_SEND_MESSAGE        52
#define SYS_IPC_RECEIVE_MESSAGE     53
#define SYS_IPC_CREATE_CHANNEL      54
#define SYS_IPC_SUBSCRIBE_CHANNEL   55
#define SYS_IPC_SEND_TO_CHANNEL     56
#define SYS_IPC_SEND_REQUEST        57
#define SYS_IPC_SEND_REPLY          58
#define SYS_IPC_SEND_ASYNC          59
#define SYS_IPC_BROADCAST           60

/* System call parameter structure */
typedef struct syscall_params {
    uint64_t param1;
    uint64_t param2;
    uint64_t param3;
    uint64_t param4;
    uint64_t param5;
} syscall_params_t;

/**
 * Main IPC system call handler
 */
uint64_t ipc_syscall_handler(uint32_t syscall_num, syscall_params_t* params) {
    if (!params) {
        return (uint64_t)IPC_ERROR_INVALID_MSG;
    }
    
    switch (syscall_num) {
        case SYS_IPC_CREATE_QUEUE:
            return (uint64_t)sys_ipc_create_queue(
                (uint32_t)params->param1,  // max_messages
                (uint32_t)params->param2   // permissions
            );
            
        case SYS_IPC_DESTROY_QUEUE:
            return (uint64_t)sys_ipc_destroy_queue(
                (uint32_t)params->param1   // queue_id
            );
            
        case SYS_IPC_SEND_MESSAGE:
            return (uint64_t)sys_ipc_send_message(
                (uint32_t)params->param1,         // queue_id
                (ipc_message_t*)params->param2,   // message
                (uint32_t)params->param3          // flags
            );
            
        case SYS_IPC_RECEIVE_MESSAGE:
            return (uint64_t)sys_ipc_receive_message(
                (uint32_t)params->param1,         // queue_id
                (ipc_message_t*)params->param2,   // message
                (uint32_t)params->param3          // flags
            );
            
        case SYS_IPC_CREATE_CHANNEL:
            return (uint64_t)sys_ipc_create_channel(
                (const char*)params->param1,      // name
                (bool)params->param2,             // is_broadcast
                (bool)params->param3              // is_persistent
            );
            
        case SYS_IPC_SUBSCRIBE_CHANNEL:
            return (uint64_t)sys_ipc_subscribe_channel(
                (uint32_t)params->param1,         // channel_id
                (uint32_t)params->param2          // pid
            );
            
        case SYS_IPC_SEND_TO_CHANNEL:
            return (uint64_t)sys_ipc_send_to_channel(
                (uint32_t)params->param1,         // channel_id
                (ipc_message_t*)params->param2,   // message
                (uint32_t)params->param3          // flags
            );
            
        case SYS_IPC_SEND_REQUEST:
            return (uint64_t)sys_ipc_send_request(
                (uint32_t)params->param1,         // target_pid
                (ipc_message_t*)params->param2,   // request
                (ipc_message_t*)params->param3,   // reply
                (uint32_t)params->param4          // timeout_ms
            );
            
        case SYS_IPC_SEND_REPLY:
            return (uint64_t)sys_ipc_send_reply(
                (uint32_t)params->param1,         // target_pid
                (ipc_message_t*)params->param2    // reply
            );
            
        case SYS_IPC_SEND_ASYNC:
            return (uint64_t)sys_ipc_send_async(
                (uint32_t)params->param1,         // target_pid
                (ipc_message_t*)params->param2    // message
            );
            
        case SYS_IPC_BROADCAST:
            return (uint64_t)sys_ipc_broadcast(
                (ipc_message_t*)params->param1,   // message
                (uint32_t*)params->param2,        // target_pids
                (uint32_t)params->param3          // count
            );
            
        default:
            return (uint64_t)IPC_ERROR_INVALID_MSG;
    }
}

/**
 * System call: Create message queue
 */
uint32_t sys_ipc_create_queue(uint32_t max_messages, uint32_t permissions) {
    // Validate parameters
    if (max_messages > IPC_MAX_QUEUE_SIZE) {
        return IPC_INVALID_CHANNEL;
    }
    
    return ipc_create_queue(max_messages, permissions);
}

/**
 * System call: Destroy message queue
 */
int sys_ipc_destroy_queue(uint32_t queue_id) {
    return ipc_destroy_queue(queue_id);
}

/**
 * System call: Send message to queue
 */
int sys_ipc_send_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags) {
    // Validate user-space message pointer
    if (!is_valid_user_pointer(message, sizeof(ipc_message_t))) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    // Copy message from user space
    ipc_message_t kernel_message;
    if (copy_from_user(&kernel_message, message, sizeof(ipc_message_t)) != 0) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    return ipc_send_message(queue_id, &kernel_message, flags);
}

/**
 * System call: Receive message from queue
 */
int sys_ipc_receive_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags) {
    // Validate user-space message pointer
    if (!is_valid_user_pointer(message, sizeof(ipc_message_t))) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    ipc_message_t kernel_message;
    int result = ipc_receive_message(queue_id, &kernel_message, flags);
    
    if (result == IPC_SUCCESS) {
        // Copy message to user space
        if (copy_to_user(message, &kernel_message, sizeof(ipc_message_t)) != 0) {
            return IPC_ERROR_INVALID_MSG;
        }
    }
    
    return result;
}

/**
 * System call: Create named channel
 */
uint32_t sys_ipc_create_channel(const char* name, bool is_broadcast, bool is_persistent) {
    // Validate user-space string pointer
    if (!is_valid_user_string(name, 64)) {
        return IPC_INVALID_CHANNEL;
    }
    
    // Copy name from user space
    char kernel_name[64];
    if (copy_string_from_user(kernel_name, name, sizeof(kernel_name)) != 0) {
        return IPC_INVALID_CHANNEL;
    }
    
    return ipc_create_channel(kernel_name, is_broadcast, is_persistent);
}

/**
 * System call: Subscribe to channel
 */
int sys_ipc_subscribe_channel(uint32_t channel_id, uint32_t pid) {
    // If pid is 0, use current process
    if (pid == 0) {
        task_t* current = task_get_current();
        if (!current) {
            return IPC_ERROR_INVALID_PID;
        }
        pid = current->pid;
    }
    
    return ipc_subscribe_channel(channel_id, pid);
}

/**
 * System call: Send message to channel
 */
int sys_ipc_send_to_channel(uint32_t channel_id, ipc_message_t* message, uint32_t flags) {
    // Validate user-space message pointer
    if (!is_valid_user_pointer(message, sizeof(ipc_message_t))) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    // Copy message from user space
    ipc_message_t kernel_message;
    if (copy_from_user(&kernel_message, message, sizeof(ipc_message_t)) != 0) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    return ipc_send_to_channel(channel_id, &kernel_message, flags);
}

/**
 * System call: Send synchronous request
 */
int sys_ipc_send_request(uint32_t target_pid, ipc_message_t* request, 
                         ipc_message_t* reply, uint32_t timeout_ms) {
    // Validate user-space pointers
    if (!is_valid_user_pointer(request, sizeof(ipc_message_t)) ||
        !is_valid_user_pointer(reply, sizeof(ipc_message_t))) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    // Copy request from user space
    ipc_message_t kernel_request, kernel_reply;
    if (copy_from_user(&kernel_request, request, sizeof(ipc_message_t)) != 0) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    int result = ipc_send_request(target_pid, &kernel_request, &kernel_reply, timeout_ms);
    
    if (result == IPC_SUCCESS) {
        // Copy reply to user space
        if (copy_to_user(reply, &kernel_reply, sizeof(ipc_message_t)) != 0) {
            return IPC_ERROR_INVALID_MSG;
        }
    }
    
    return result;
}

/**
 * System call: Send reply to request
 */
int sys_ipc_send_reply(uint32_t target_pid, ipc_message_t* reply) {
    // Validate user-space message pointer
    if (!is_valid_user_pointer(reply, sizeof(ipc_message_t))) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    // Copy reply from user space
    ipc_message_t kernel_reply;
    if (copy_from_user(&kernel_reply, reply, sizeof(ipc_message_t)) != 0) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    return ipc_send_reply(target_pid, &kernel_reply);
}

/**
 * System call: Send asynchronous message
 */
int sys_ipc_send_async(uint32_t target_pid, ipc_message_t* message) {
    // Validate user-space message pointer
    if (!is_valid_user_pointer(message, sizeof(ipc_message_t))) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    // Copy message from user space
    ipc_message_t kernel_message;
    if (copy_from_user(&kernel_message, message, sizeof(ipc_message_t)) != 0) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    return ipc_send_async(target_pid, &kernel_message);
}

/**
 * System call: Broadcast message
 */
int sys_ipc_broadcast(ipc_message_t* message, uint32_t* target_pids, uint32_t count) {
    // Validate parameters
    if (count > 64) { // Reasonable limit
        return IPC_ERROR_INVALID_SIZE;
    }
    
    if (!is_valid_user_pointer(message, sizeof(ipc_message_t)) ||
        !is_valid_user_pointer(target_pids, count * sizeof(uint32_t))) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    // Copy message and target list from user space
    ipc_message_t kernel_message;
    uint32_t kernel_pids[64];
    
    if (copy_from_user(&kernel_message, message, sizeof(ipc_message_t)) != 0 ||
        copy_from_user(kernel_pids, target_pids, count * sizeof(uint32_t)) != 0) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    return ipc_broadcast(&kernel_message, kernel_pids, count);
}

/* User space validation functions (placeholders) */

/**
 * Check if pointer is valid in user space
 */
bool is_valid_user_pointer(const void* ptr, size_t size) {
    // In a real implementation, this would check if the pointer
    // is in valid user space and accessible
    if (!ptr || size == 0) {
        return false;
    }
    
    // Simple range check (assuming user space is above 0x10000000)
    uintptr_t addr = (uintptr_t)ptr;
    return (addr >= 0x10000000) && (addr + size >= addr); // Check for overflow
}

/**
 * Check if string is valid in user space
 */
bool is_valid_user_string(const char* str, size_t max_len) {
    if (!str) {
        return false;
    }
    
    // Check if string is in valid user space and null-terminated
    for (size_t i = 0; i < max_len; i++) {
        if (!is_valid_user_pointer(&str[i], 1)) {
            return false;
        }
        if (str[i] == '\0') {
            return true;
        }
    }
    
    return false; // String too long or not null-terminated
}

/**
 * Copy data from user space to kernel space
 */
int copy_from_user(void* dest, const void* src, size_t size) {
    // In a real implementation, this would handle page faults
    // and validate memory access
    if (!dest || !src || size == 0) {
        return -1;
    }
    
    if (!is_valid_user_pointer(src, size)) {
        return -1;
    }
    
    memcpy(dest, src, size);
    return 0;
}

/**
 * Copy data from kernel space to user space
 */
int copy_to_user(void* dest, const void* src, size_t size) {
    // In a real implementation, this would handle page faults
    // and validate memory access
    if (!dest || !src || size == 0) {
        return -1;
    }
    
    if (!is_valid_user_pointer(dest, size)) {
        return -1;
    }
    
    memcpy(dest, src, size);
    return 0;
}

/**
 * Copy string from user space to kernel space
 */
int copy_string_from_user(char* dest, const char* src, size_t max_len) {
    if (!dest || !src || max_len == 0) {
        return -1;
    }
    
    if (!is_valid_user_string(src, max_len)) {
        return -1;
    }
    
    size_t len = 0;
    while (len < max_len - 1 && src[len] != '\0') {
        dest[len] = src[len];
        len++;
    }
    dest[len] = '\0';
    
    return 0;
}
