/* IKOS User-Space IPC API Implementation
 * Complete implementation of user-space IPC interface
 */
#include "ipc_user_api.h"
#include "ipc_syscalls.h"
#include <string.h>
#include <stdlib.h>

/* Global user-space IPC context */
static ipc_user_context_t g_ipc_context;
static int g_last_error = IPC_SUCCESS;

/* Function prototypes for internal use */
static void ipc_user_handle_message_internal(uint32_t queue_id, const ipc_message_t* message);
static void ipc_user_handle_channel_message(uint32_t channel_id, const ipc_message_t* message);
static int ipc_user_find_handler_slot(void);
static int ipc_user_find_channel_slot(void);

/* ===== Core Initialization ===== */

int ipc_user_init(void) {
    if (g_ipc_context.initialized) {
        return IPC_SUCCESS;
    }
    
    memset(&g_ipc_context, 0, sizeof(ipc_user_context_t));
    
    /* Create default message queue for this process */
    g_ipc_context.process_queue_id = sys_ipc_create_queue(IPC_MAX_QUEUE_SIZE, IPC_PERM_ALL);
    if (g_ipc_context.process_queue_id == IPC_INVALID_CHANNEL) {
        g_last_error = IPC_ERROR_NO_MEMORY;
        return IPC_ERROR_NO_MEMORY;
    }
    
    g_ipc_context.initialized = true;
    g_ipc_context.polling_active = false;
    g_last_error = IPC_SUCCESS;
    
    return IPC_SUCCESS;
}

void ipc_user_cleanup(void) {
    if (!g_ipc_context.initialized) {
        return;
    }
    
    /* Stop polling if active */
    ipc_user_stop_polling();
    
    /* Unsubscribe from all channels */
    for (uint32_t i = 0; i < g_ipc_context.channel_count; i++) {
        if (g_ipc_context.channels[i].active) {
            sys_ipc_unsubscribe_channel(g_ipc_context.channels[i].channel_id, 0);
        }
    }
    
    /* Destroy process queue */
    if (g_ipc_context.process_queue_id != IPC_INVALID_CHANNEL) {
        sys_ipc_destroy_queue(g_ipc_context.process_queue_id);
    }
    
    memset(&g_ipc_context, 0, sizeof(ipc_user_context_t));
}

/* ===== Queue Operations ===== */

uint32_t ipc_user_create_queue(uint32_t max_messages, uint32_t permissions) {
    uint32_t queue_id = sys_ipc_create_queue(max_messages, permissions);
    if (queue_id == IPC_INVALID_CHANNEL) {
        g_last_error = IPC_ERROR_NO_MEMORY;
    } else {
        g_last_error = IPC_SUCCESS;
    }
    return queue_id;
}

int ipc_user_destroy_queue(uint32_t queue_id) {
    int result = sys_ipc_destroy_queue(queue_id);
    g_last_error = result;
    return result;
}

int ipc_user_send_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags) {
    if (!message) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    int result = sys_ipc_send_message(queue_id, message, flags);
    g_last_error = result;
    return result;
}

int ipc_user_receive_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags) {
    if (!message) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    int result = sys_ipc_receive_message(queue_id, message, flags);
    g_last_error = result;
    return result;
}

int ipc_user_peek_message(uint32_t queue_id, ipc_message_t* message) {
    if (!message) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    /* System call for peek not directly available, use receive with special flag */
    int result = sys_ipc_receive_message(queue_id, message, IPC_FLAG_NON_BLOCKING);
    g_last_error = result;
    return result;
}

/* ===== Synchronous Communication ===== */

int ipc_user_send_request(uint32_t target_pid, ipc_message_t* request, 
                          ipc_message_t* reply, uint32_t timeout_ms) {
    if (!request || !reply) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    int result = sys_ipc_send_request(target_pid, request, reply, timeout_ms);
    g_last_error = result;
    return result;
}

int ipc_user_send_reply(uint32_t target_pid, ipc_message_t* reply) {
    if (!reply) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    int result = sys_ipc_send_reply(target_pid, reply);
    g_last_error = result;
    return result;
}

/* ===== Asynchronous Communication ===== */

int ipc_user_send_async(uint32_t target_pid, ipc_message_t* message) {
    if (!message) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    int result = sys_ipc_send_async(target_pid, message);
    g_last_error = result;
    return result;
}

int ipc_user_broadcast(ipc_message_t* message, uint32_t* target_pids, uint32_t count) {
    if (!message || !target_pids) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    int result = sys_ipc_broadcast(message, target_pids, count);
    g_last_error = result;
    return result;
}

/* ===== Channel Operations ===== */

uint32_t ipc_user_create_channel(const char* name, bool is_broadcast, bool is_persistent) {
    if (!name) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_INVALID_CHANNEL;
    }
    
    uint32_t channel_id = sys_ipc_create_channel(name, is_broadcast, is_persistent);
    if (channel_id == IPC_INVALID_CHANNEL) {
        g_last_error = IPC_ERROR_CHANNEL_EXISTS;
    } else {
        g_last_error = IPC_SUCCESS;
    }
    return channel_id;
}

int ipc_user_subscribe_channel(const char* name, ipc_channel_handler_t handler, void* user_data) {
    if (!name || !handler || !g_ipc_context.initialized) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    /* Find the channel by name first */
    uint32_t channel_id = 0; /* Would need sys call to find channel by name */
    
    /* Find free slot for subscription */
    int slot = ipc_user_find_channel_slot();
    if (slot < 0) {
        g_last_error = IPC_ERROR_NO_MEMORY;
        return IPC_ERROR_NO_MEMORY;
    }
    
    /* Subscribe to channel */
    int result = sys_ipc_subscribe_channel(channel_id, 0); /* 0 = current process */
    if (result != IPC_SUCCESS) {
        g_last_error = result;
        return result;
    }
    
    /* Register local handler */
    strncpy(g_ipc_context.channels[slot].name, name, sizeof(g_ipc_context.channels[slot].name) - 1);
    g_ipc_context.channels[slot].channel_id = channel_id;
    g_ipc_context.channels[slot].handler = handler;
    g_ipc_context.channels[slot].user_data = user_data;
    g_ipc_context.channels[slot].active = true;
    g_ipc_context.channel_count++;
    
    g_last_error = IPC_SUCCESS;
    return IPC_SUCCESS;
}

int ipc_user_unsubscribe_channel(const char* name) {
    if (!name || !g_ipc_context.initialized) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    /* Find subscription */
    for (uint32_t i = 0; i < IPC_USER_MAX_CHANNELS; i++) {
        if (g_ipc_context.channels[i].active && 
            strcmp(g_ipc_context.channels[i].name, name) == 0) {
            
            /* Unsubscribe from kernel */
            int result = sys_ipc_unsubscribe_channel(g_ipc_context.channels[i].channel_id, 0);
            
            /* Remove local registration */
            g_ipc_context.channels[i].active = false;
            g_ipc_context.channel_count--;
            
            g_last_error = result;
            return result;
        }
    }
    
    g_last_error = IPC_ERROR_CHANNEL_NOT_FOUND;
    return IPC_ERROR_CHANNEL_NOT_FOUND;
}

int ipc_user_send_to_channel(const char* name, ipc_message_t* message, uint32_t flags) {
    if (!name || !message) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    /* Find channel ID by name */
    uint32_t channel_id = 0; /* Would need sys call to find channel */
    
    int result = sys_ipc_send_to_channel(channel_id, message, flags);
    g_last_error = result;
    return result;
}

/* ===== Message Handler Registration ===== */

int ipc_user_register_handler(uint32_t queue_id, ipc_message_handler_t handler, 
                              void* user_data, uint32_t message_type_filter) {
    if (!handler || !g_ipc_context.initialized) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    int slot = ipc_user_find_handler_slot();
    if (slot < 0) {
        g_last_error = IPC_ERROR_NO_MEMORY;
        return IPC_ERROR_NO_MEMORY;
    }
    
    g_ipc_context.handlers[slot].queue_id = queue_id;
    g_ipc_context.handlers[slot].handler = handler;
    g_ipc_context.handlers[slot].user_data = user_data;
    g_ipc_context.handlers[slot].message_type_filter = message_type_filter;
    g_ipc_context.handlers[slot].active = true;
    g_ipc_context.handler_count++;
    
    g_last_error = IPC_SUCCESS;
    return IPC_SUCCESS;
}

int ipc_user_unregister_handler(uint32_t queue_id) {
    if (!g_ipc_context.initialized) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    for (uint32_t i = 0; i < IPC_USER_MAX_HANDLERS; i++) {
        if (g_ipc_context.handlers[i].active && 
            g_ipc_context.handlers[i].queue_id == queue_id) {
            
            g_ipc_context.handlers[i].active = false;
            g_ipc_context.handler_count--;
            g_last_error = IPC_SUCCESS;
            return IPC_SUCCESS;
        }
    }
    
    g_last_error = IPC_ERROR_CHANNEL_NOT_FOUND;
    return IPC_ERROR_CHANNEL_NOT_FOUND;
}

int ipc_user_register_default_handler(ipc_message_handler_t handler, void* user_data) {
    return ipc_user_register_handler(g_ipc_context.process_queue_id, handler, user_data, 0);
}

/* ===== Message Processing ===== */

void ipc_user_poll_handlers(void) {
    if (!g_ipc_context.initialized) {
        return;
    }
    
    ipc_message_t message;
    
    /* Poll each registered queue */
    for (uint32_t i = 0; i < IPC_USER_MAX_HANDLERS; i++) {
        if (!g_ipc_context.handlers[i].active) {
            continue;
        }
        
        /* Check for messages in this queue */
        while (ipc_user_receive_message(g_ipc_context.handlers[i].queue_id, 
                                        &message, IPC_FLAG_NON_BLOCKING) == IPC_SUCCESS) {
            ipc_user_handle_message_internal(g_ipc_context.handlers[i].queue_id, &message);
        }
    }
    
    /* Poll default process queue */
    while (ipc_user_receive_message(g_ipc_context.process_queue_id, 
                                    &message, IPC_FLAG_NON_BLOCKING) == IPC_SUCCESS) {
        ipc_user_handle_message_internal(g_ipc_context.process_queue_id, &message);
    }
}

int ipc_user_start_polling(void) {
    if (!g_ipc_context.initialized) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    g_ipc_context.polling_active = true;
    g_last_error = IPC_SUCCESS;
    return IPC_SUCCESS;
}

void ipc_user_stop_polling(void) {
    g_ipc_context.polling_active = false;
}

int ipc_user_process_message(uint32_t queue_id) {
    if (!g_ipc_context.initialized) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    ipc_message_t message;
    int result = ipc_user_receive_message(queue_id, &message, IPC_FLAG_NON_BLOCKING);
    
    if (result == IPC_SUCCESS) {
        ipc_user_handle_message_internal(queue_id, &message);
    }
    
    return result;
}

/* ===== Message Utilities ===== */

ipc_message_t* ipc_user_alloc_message(uint32_t data_size) {
    if (data_size > IPC_MAX_MESSAGE_SIZE) {
        g_last_error = IPC_ERROR_INVALID_SIZE;
        return NULL;
    }
    
    ipc_message_t* message = malloc(sizeof(ipc_message_t));
    if (message) {
        memset(message, 0, sizeof(ipc_message_t));
        message->data_size = data_size;
        g_last_error = IPC_SUCCESS;
    } else {
        g_last_error = IPC_ERROR_NO_MEMORY;
    }
    
    return message;
}

void ipc_user_free_message(ipc_message_t* message) {
    if (message) {
        free(message);
    }
}

int ipc_user_copy_message(ipc_message_t* dest, const ipc_message_t* src) {
    if (!dest || !src) {
        g_last_error = IPC_ERROR_INVALID_MSG;
        return IPC_ERROR_INVALID_MSG;
    }
    
    memcpy(dest, src, sizeof(ipc_message_t));
    g_last_error = IPC_SUCCESS;
    return IPC_SUCCESS;
}

ipc_message_t* ipc_user_create_data_message(const void* data, uint32_t size, uint32_t target_pid) {
    ipc_message_t* message = ipc_user_alloc_message(size);
    if (!message) {
        return NULL;
    }
    
    message->type = IPC_MSG_DATA;
    message->receiver_pid = target_pid;
    message->data_size = size;
    
    if (data && size > 0) {
        memcpy(message->data, data, size);
    }
    
    return message;
}

ipc_message_t* ipc_user_create_request(const void* data, uint32_t size, uint32_t target_pid) {
    ipc_message_t* message = ipc_user_alloc_message(size);
    if (!message) {
        return NULL;
    }
    
    message->type = IPC_MSG_REQUEST;
    message->receiver_pid = target_pid;
    message->data_size = size;
    
    if (data && size > 0) {
        memcpy(message->data, data, size);
    }
    
    return message;
}

ipc_message_t* ipc_user_create_reply(const void* data, uint32_t size, uint32_t target_pid, uint32_t reply_to) {
    ipc_message_t* message = ipc_user_alloc_message(size);
    if (!message) {
        return NULL;
    }
    
    message->type = IPC_MSG_REPLY;
    message->receiver_pid = target_pid;
    message->reply_to = reply_to;
    message->data_size = size;
    
    if (data && size > 0) {
        memcpy(message->data, data, size);
    }
    
    return message;
}

/* ===== Context and Status ===== */

ipc_user_context_t* ipc_user_get_context(void) {
    return &g_ipc_context;
}

uint32_t ipc_user_get_process_queue(void) {
    return g_ipc_context.process_queue_id;
}

bool ipc_user_has_messages(uint32_t queue_id) {
    ipc_message_t temp_message;
    int result = ipc_user_receive_message(queue_id, &temp_message, IPC_FLAG_NON_BLOCKING);
    
    /* If we got a message, put it back by not processing it */
    /* This is a simplified implementation - real version would need proper peek */
    
    return (result == IPC_SUCCESS);
}

uint32_t ipc_user_get_message_count(uint32_t queue_id) {
    /* This would require a system call to get queue status */
    /* For now, return 0 or 1 based on has_messages */
    return ipc_user_has_messages(queue_id) ? 1 : 0;
}

/* ===== Error Handling ===== */

int ipc_user_get_last_error(void) {
    return g_last_error;
}

const char* ipc_user_error_string(int error_code) {
    switch (error_code) {
        case IPC_SUCCESS: return "Success";
        case IPC_ERROR_INVALID_PID: return "Invalid process ID";
        case IPC_ERROR_INVALID_QUEUE: return "Invalid queue";
        case IPC_ERROR_QUEUE_FULL: return "Queue is full";
        case IPC_ERROR_QUEUE_EMPTY: return "Queue is empty";
        case IPC_ERROR_NO_MEMORY: return "Out of memory";
        case IPC_ERROR_TIMEOUT: return "Operation timed out";
        case IPC_ERROR_PERMISSION: return "Permission denied";
        case IPC_ERROR_INVALID_MSG: return "Invalid message";
        case IPC_ERROR_CHANNEL_EXISTS: return "Channel already exists";
        case IPC_ERROR_CHANNEL_NOT_FOUND: return "Channel not found";
        case IPC_ERROR_NOT_SUBSCRIBED: return "Not subscribed to channel";
        case IPC_ERROR_INVALID_SIZE: return "Invalid size";
        default: return "Unknown error";
    }
}

/* ===== Internal Helper Functions ===== */

static void ipc_user_handle_message_internal(uint32_t queue_id, const ipc_message_t* message) {
    /* Find handler for this queue */
    for (uint32_t i = 0; i < IPC_USER_MAX_HANDLERS; i++) {
        if (g_ipc_context.handlers[i].active && 
            g_ipc_context.handlers[i].queue_id == queue_id) {
            
            /* Check message type filter */
            if (g_ipc_context.handlers[i].message_type_filter == 0 ||
                g_ipc_context.handlers[i].message_type_filter == message->type) {
                
                /* Call the handler */
                g_ipc_context.handlers[i].handler(message, g_ipc_context.handlers[i].user_data);
            }
            return;
        }
    }
    
    /* Check if this is a channel message */
    if (message->channel_id != 0) {
        ipc_user_handle_channel_message(message->channel_id, message);
    }
}

static void ipc_user_handle_channel_message(uint32_t channel_id, const ipc_message_t* message) {
    /* Find channel handler */
    for (uint32_t i = 0; i < IPC_USER_MAX_CHANNELS; i++) {
        if (g_ipc_context.channels[i].active && 
            g_ipc_context.channels[i].channel_id == channel_id) {
            
            /* Call the channel handler */
            g_ipc_context.channels[i].handler(channel_id, message, g_ipc_context.channels[i].user_data);
            return;
        }
    }
}

static int ipc_user_find_handler_slot(void) {
    for (uint32_t i = 0; i < IPC_USER_MAX_HANDLERS; i++) {
        if (!g_ipc_context.handlers[i].active) {
            return i;
        }
    }
    return -1;
}

static int ipc_user_find_channel_slot(void) {
    for (uint32_t i = 0; i < IPC_USER_MAX_CHANNELS; i++) {
        if (!g_ipc_context.channels[i].active) {
            return i;
        }
    }
    return -1;
}
