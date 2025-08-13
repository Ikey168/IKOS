/* IKOS Inter-Process Communication (IPC) Implementation
 * Provides message-passing based communication between processes
 */

#include "ipc.h"
#include "scheduler.h"
#include "memory.h"
#include <string.h>

/* Global IPC state */
static ipc_queue_t* queue_list = NULL;
static ipc_channel_t* channel_list = NULL;
static ipc_stats_t ipc_statistics;
static uint32_t next_queue_id = 1;
static uint32_t next_channel_id = 1;
static uint32_t next_message_id = 1;
static bool ipc_initialized = false;

/* Process message queues (one per process) */
static ipc_queue_t* process_queues[MAX_TASKS];

/* IPC system lock (would use proper synchronization in full implementation) */
static bool ipc_lock = false;

/* Forward declarations */
static int validate_permissions(uint32_t queue_id, uint32_t pid, uint32_t required_perm);
static void update_statistics(void);
static int enqueue_message(ipc_queue_t* queue, ipc_message_t* message);
static ipc_message_t* dequeue_message(ipc_queue_t* queue, uint32_t flags);
static void wakeup_blocked_processes(ipc_queue_t* queue, bool senders, bool receivers);

/**
 * Initialize the IPC system
 */
int ipc_init(void) {
    if (ipc_initialized) {
        return IPC_SUCCESS;
    }
    
    // Initialize global state
    memset(&ipc_statistics, 0, sizeof(ipc_statistics));
    memset(process_queues, 0, sizeof(process_queues));
    
    queue_list = NULL;
    channel_list = NULL;
    next_queue_id = 1;
    next_channel_id = 1;
    next_message_id = 1;
    ipc_lock = false;
    
    ipc_initialized = true;
    
    return IPC_SUCCESS;
}

/**
 * Create a message queue
 */
uint32_t ipc_create_queue(uint32_t max_messages, uint32_t permissions) {
    if (!ipc_initialized) {
        return IPC_INVALID_CHANNEL;
    }
    
    // Get current process ID
    task_t* current_task = task_get_current();
    if (!current_task) {
        return IPC_INVALID_CHANNEL;
    }
    
    // Allocate queue structure
    ipc_queue_t* queue = (ipc_queue_t*)kmalloc(sizeof(ipc_queue_t));
    if (!queue) {
        return IPC_INVALID_CHANNEL;
    }
    
    // Initialize queue
    memset(queue, 0, sizeof(ipc_queue_t));
    queue->queue_id = next_queue_id++;
    queue->owner_pid = current_task->pid;
    queue->max_messages = max_messages ? max_messages : IPC_MAX_QUEUE_SIZE;
    queue->permissions = permissions;
    queue->is_public = (permissions & IPC_PERM_ALL) != 0;
    
    // Add to global queue list
    queue->next = queue_list;
    queue_list = queue;
    
    // Set as process queue if it's the first one for this process
    if (!process_queues[current_task->pid]) {
        process_queues[current_task->pid] = queue;
    }
    
    ipc_statistics.total_queues_created++;
    ipc_statistics.active_queues++;
    
    return queue->queue_id;
}

/**
 * Destroy a message queue
 */
int ipc_destroy_queue(uint32_t queue_id) {
    ipc_queue_t* queue = ipc_get_queue(queue_id);
    if (!queue) {
        return IPC_ERROR_INVALID_QUEUE;
    }
    
    task_t* current_task = task_get_current();
    if (!current_task || current_task->pid != queue->owner_pid) {
        return IPC_ERROR_PERMISSION;
    }
    
    // Free all queued messages
    ipc_message_t* msg = queue->head;
    while (msg) {
        ipc_message_t* next = msg->next;
        ipc_free_message(msg);
        msg = next;
    }
    
    // Remove from queue list
    if (queue_list == queue) {
        queue_list = queue->next;
    } else {
        ipc_queue_t* prev = queue_list;
        while (prev && prev->next != queue) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = queue->next;
        }
    }
    
    // Clear from process queues
    for (int i = 0; i < MAX_TASKS; i++) {
        if (process_queues[i] == queue) {
            process_queues[i] = NULL;
        }
    }
    
    // Wake up any blocked processes
    wakeup_blocked_processes(queue, true, true);
    
    kfree(queue);
    ipc_statistics.active_queues--;
    
    return IPC_SUCCESS;
}

/**
 * Send a message to a queue
 */
int ipc_send_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags) {
    if (!message) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    ipc_queue_t* queue = ipc_get_queue(queue_id);
    if (!queue) {
        return IPC_ERROR_INVALID_QUEUE;
    }
    
    task_t* current_task = task_get_current();
    if (!current_task) {
        return IPC_ERROR_INVALID_PID;
    }
    
    // Check permissions
    if (validate_permissions(queue_id, current_task->pid, IPC_PERM_WRITE) != IPC_SUCCESS) {
        return IPC_ERROR_PERMISSION;
    }
    
    // Validate message
    if (ipc_validate_message(message) != IPC_SUCCESS) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    // Set sender information
    message->sender_pid = current_task->pid;
    message->timestamp = ipc_get_timestamp();
    message->msg_id = ipc_generate_msg_id();
    
    // Check if queue is full
    if (queue->current_count >= queue->max_messages) {
        if (flags & IPC_FLAG_NON_BLOCKING) {
            return IPC_ERROR_QUEUE_FULL;
        }
        
        // Block sender (in full implementation, would use proper blocking)
        queue->blocked_senders++;
        return IPC_ERROR_QUEUE_FULL; // Simplified for now
    }
    
    // Enqueue message
    int result = enqueue_message(queue, message);
    if (result == IPC_SUCCESS) {
        queue->total_sent++;
        ipc_statistics.total_messages_sent++;
        
        // Wake up any blocked receivers
        wakeup_blocked_processes(queue, false, true);
    }
    
    return result;
}

/**
 * Receive a message from a queue
 */
int ipc_receive_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags) {
    if (!message) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    ipc_queue_t* queue = ipc_get_queue(queue_id);
    if (!queue) {
        return IPC_ERROR_INVALID_QUEUE;
    }
    
    task_t* current_task = task_get_current();
    if (!current_task) {
        return IPC_ERROR_INVALID_PID;
    }
    
    // Check permissions
    if (validate_permissions(queue_id, current_task->pid, IPC_PERM_READ) != IPC_SUCCESS) {
        return IPC_ERROR_PERMISSION;
    }
    
    // Check if queue is empty
    if (queue->current_count == 0) {
        if (flags & IPC_FLAG_NON_BLOCKING) {
            return IPC_ERROR_QUEUE_EMPTY;
        }
        
        // Block receiver (in full implementation, would use proper blocking)
        queue->blocked_receivers++;
        return IPC_ERROR_QUEUE_EMPTY; // Simplified for now
    }
    
    // Dequeue message
    ipc_message_t* received_msg = dequeue_message(queue, flags);
    if (received_msg) {
        ipc_copy_message(message, received_msg);
        ipc_free_message(received_msg);
        
        queue->total_received++;
        ipc_statistics.total_messages_received++;
        
        // Wake up any blocked senders
        wakeup_blocked_processes(queue, true, false);
        
        return IPC_SUCCESS;
    }
    
    return IPC_ERROR_QUEUE_EMPTY;
}

/**
 * Peek at the next message without removing it
 */
int ipc_peek_message(uint32_t queue_id, ipc_message_t* message) {
    if (!message) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    ipc_queue_t* queue = ipc_get_queue(queue_id);
    if (!queue) {
        return IPC_ERROR_INVALID_QUEUE;
    }
    
    task_t* current_task = task_get_current();
    if (!current_task) {
        return IPC_ERROR_INVALID_PID;
    }
    
    // Check permissions
    if (validate_permissions(queue_id, current_task->pid, IPC_PERM_READ) != IPC_SUCCESS) {
        return IPC_ERROR_PERMISSION;
    }
    
    if (queue->head) {
        ipc_copy_message(message, queue->head);
        return IPC_SUCCESS;
    }
    
    return IPC_ERROR_QUEUE_EMPTY;
}

/**
 * Create a named channel
 */
uint32_t ipc_create_channel(const char* name, bool is_broadcast, bool is_persistent) {
    if (!name || strlen(name) == 0) {
        return IPC_INVALID_CHANNEL;
    }
    
    // Check if channel already exists
    if (ipc_find_channel(name)) {
        return IPC_INVALID_CHANNEL;
    }
    
    task_t* current_task = task_get_current();
    if (!current_task) {
        return IPC_INVALID_CHANNEL;
    }
    
    // Allocate channel structure
    ipc_channel_t* channel = (ipc_channel_t*)kmalloc(sizeof(ipc_channel_t));
    if (!channel) {
        return IPC_INVALID_CHANNEL;
    }
    
    // Initialize channel
    memset(channel, 0, sizeof(ipc_channel_t));
    channel->channel_id = next_channel_id++;
    strncpy(channel->name, name, sizeof(channel->name) - 1);
    channel->creator_pid = current_task->pid;
    channel->is_broadcast = is_broadcast;
    channel->is_persistent = is_persistent;
    channel->max_message_size = IPC_MAX_MESSAGE_SIZE;
    
    // Add to channel list
    channel->next = channel_list;
    channel_list = channel;
    
    ipc_statistics.total_channels_created++;
    ipc_statistics.active_channels++;
    
    return channel->channel_id;
}

/**
 * Subscribe to a channel
 */
int ipc_subscribe_channel(uint32_t channel_id, uint32_t pid) {
    ipc_channel_t* channel = ipc_get_channel(channel_id);
    if (!channel) {
        return IPC_ERROR_CHANNEL_NOT_FOUND;
    }
    
    // Check if already subscribed
    for (uint32_t i = 0; i < channel->subscriber_count; i++) {
        if (channel->subscribers[i] == pid) {
            return IPC_SUCCESS; // Already subscribed
        }
    }
    
    // Add subscriber
    if (channel->subscriber_count < 32) {
        channel->subscribers[channel->subscriber_count++] = pid;
        return IPC_SUCCESS;
    }
    
    return IPC_ERROR_QUEUE_FULL; // Subscriber list full
}

/**
 * Send message to channel
 */
int ipc_send_to_channel(uint32_t channel_id, ipc_message_t* message, uint32_t flags) {
    ipc_channel_t* channel = ipc_get_channel(channel_id);
    if (!channel) {
        return IPC_ERROR_CHANNEL_NOT_FOUND;
    }
    
    if (!message) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    message->channel_id = channel_id;
    
    if (channel->is_broadcast) {
        // Send to all subscribers
        for (uint32_t i = 0; i < channel->subscriber_count; i++) {
            ipc_queue_t* queue = process_queues[channel->subscribers[i]];
            if (queue) {
                ipc_message_t* msg_copy = ipc_alloc_message(message->data_size);
                if (msg_copy) {
                    ipc_copy_message(msg_copy, message);
                    msg_copy->receiver_pid = channel->subscribers[i];
                    enqueue_message(queue, msg_copy);
                }
            }
        }
    } else {
        // Send to first available subscriber
        for (uint32_t i = 0; i < channel->subscriber_count; i++) {
            ipc_queue_t* queue = process_queues[channel->subscribers[i]];
            if (queue && queue->current_count < queue->max_messages) {
                message->receiver_pid = channel->subscribers[i];
                return enqueue_message(queue, message);
            }
        }
        return IPC_ERROR_QUEUE_FULL;
    }
    
    return IPC_SUCCESS;
}

/**
 * Synchronous request-reply communication
 */
int ipc_send_request(uint32_t target_pid, ipc_message_t* request, 
                     ipc_message_t* reply, uint32_t timeout_ms) {
    if (!request || !reply) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    // Set request type and generate unique ID
    request->type = IPC_MSG_REQUEST;
    request->msg_id = ipc_generate_msg_id();
    request->receiver_pid = target_pid;
    
    // Send request
    ipc_queue_t* target_queue = process_queues[target_pid];
    if (!target_queue) {
        return IPC_ERROR_INVALID_PID;
    }
    
    int result = enqueue_message(target_queue, request);
    if (result != IPC_SUCCESS) {
        return result;
    }
    
    // Wait for reply (simplified - would use proper blocking/timeout)
    task_t* current_task = task_get_current();
    ipc_queue_t* my_queue = process_queues[current_task->pid];
    if (!my_queue) {
        return IPC_ERROR_INVALID_QUEUE;
    }
    
    // Poll for reply message (simplified implementation)
    uint32_t timeout_counter = timeout_ms;
    while (timeout_counter > 0) {
        ipc_message_t* msg = my_queue->head;
        while (msg) {
            if (msg->type == IPC_MSG_REPLY && msg->reply_to == request->msg_id) {
                ipc_copy_message(reply, msg);
                // Remove message from queue
                if (msg->prev) msg->prev->next = msg->next;
                if (msg->next) msg->next->prev = msg->prev;
                if (my_queue->head == msg) my_queue->head = msg->next;
                if (my_queue->tail == msg) my_queue->tail = msg->prev;
                my_queue->current_count--;
                ipc_free_message(msg);
                return IPC_SUCCESS;
            }
            msg = msg->next;
        }
        
        // Yield CPU and decrement timeout
        sys_yield();
        timeout_counter--;
    }
    
    return IPC_ERROR_TIMEOUT;
}

/**
 * Send reply to a request
 */
int ipc_send_reply(uint32_t target_pid, ipc_message_t* reply) {
    if (!reply) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    reply->type = IPC_MSG_REPLY;
    reply->receiver_pid = target_pid;
    
    ipc_queue_t* target_queue = process_queues[target_pid];
    if (!target_queue) {
        return IPC_ERROR_INVALID_PID;
    }
    
    return enqueue_message(target_queue, reply);
}

/**
 * Asynchronous message sending
 */
int ipc_send_async(uint32_t target_pid, ipc_message_t* message) {
    if (!message) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    message->type = IPC_MSG_NOTIFICATION;
    message->receiver_pid = target_pid;
    message->flags |= IPC_FLAG_NON_BLOCKING;
    
    ipc_queue_t* target_queue = process_queues[target_pid];
    if (!target_queue) {
        return IPC_ERROR_INVALID_PID;
    }
    
    return enqueue_message(target_queue, message);
}

/**
 * Broadcast message to multiple processes
 */
int ipc_broadcast(ipc_message_t* message, uint32_t* target_pids, uint32_t count) {
    if (!message || !target_pids) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    message->flags |= IPC_FLAG_BROADCAST;
    int success_count = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        ipc_queue_t* queue = process_queues[target_pids[i]];
        if (queue) {
            ipc_message_t* msg_copy = ipc_alloc_message(message->data_size);
            if (msg_copy) {
                ipc_copy_message(msg_copy, message);
                msg_copy->receiver_pid = target_pids[i];
                if (enqueue_message(queue, msg_copy) == IPC_SUCCESS) {
                    success_count++;
                }
            }
        }
    }
    
    return success_count > 0 ? IPC_SUCCESS : IPC_ERROR_INVALID_PID;
}

/**
 * Allocate a new message
 */
ipc_message_t* ipc_alloc_message(uint32_t data_size) {
    if (data_size > IPC_MAX_MESSAGE_SIZE) {
        return NULL;
    }
    
    ipc_message_t* message = (ipc_message_t*)kmalloc(sizeof(ipc_message_t));
    if (message) {
        memset(message, 0, sizeof(ipc_message_t));
        message->data_size = data_size;
        message->timestamp = ipc_get_timestamp();
    }
    
    return message;
}

/**
 * Free a message
 */
void ipc_free_message(ipc_message_t* message) {
    if (message) {
        kfree(message);
    }
}

/**
 * Copy a message
 */
int ipc_copy_message(ipc_message_t* dest, const ipc_message_t* src) {
    if (!dest || !src) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    memcpy(dest, src, sizeof(ipc_message_t));
    return IPC_SUCCESS;
}

/**
 * Get queue by ID
 */
ipc_queue_t* ipc_get_queue(uint32_t queue_id) {
    ipc_queue_t* queue = queue_list;
    while (queue) {
        if (queue->queue_id == queue_id) {
            return queue;
        }
        queue = queue->next;
    }
    return NULL;
}

/**
 * Find channel by name
 */
ipc_channel_t* ipc_find_channel(const char* name) {
    if (!name) {
        return NULL;
    }
    
    ipc_channel_t* channel = channel_list;
    while (channel) {
        if (strcmp(channel->name, name) == 0) {
            return channel;
        }
        channel = channel->next;
    }
    return NULL;
}

/**
 * Get channel by ID
 */
ipc_channel_t* ipc_get_channel(uint32_t channel_id) {
    ipc_channel_t* channel = channel_list;
    while (channel) {
        if (channel->channel_id == channel_id) {
            return channel;
        }
        channel = channel->next;
    }
    return NULL;
}

/**
 * Get IPC statistics
 */
ipc_stats_t* ipc_get_stats(void) {
    update_statistics();
    return &ipc_statistics;
}

/**
 * Generate unique message ID
 */
uint32_t ipc_generate_msg_id(void) {
    return next_message_id++;
}

/**
 * Get current timestamp
 */
uint64_t ipc_get_timestamp(void) {
    // In a real implementation, this would get actual system time
    static uint64_t counter = 0;
    return ++counter;
}

/**
 * Validate message structure
 */
int ipc_validate_message(const ipc_message_t* message) {
    if (!message) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    if (message->data_size > IPC_MAX_MESSAGE_SIZE) {
        return IPC_ERROR_INVALID_SIZE;
    }
    
    return IPC_SUCCESS;
}

/* Helper Functions */

/**
 * Validate permissions for queue access
 */
static int validate_permissions(uint32_t queue_id, uint32_t pid, uint32_t required_perm) {
    ipc_queue_t* queue = ipc_get_queue(queue_id);
    if (!queue) {
        return IPC_ERROR_INVALID_QUEUE;
    }
    
    // Owner has all permissions
    if (queue->owner_pid == pid) {
        return IPC_SUCCESS;
    }
    
    // Check public access
    if (queue->is_public && (queue->permissions & required_perm)) {
        return IPC_SUCCESS;
    }
    
    return IPC_ERROR_PERMISSION;
}

/**
 * Add message to queue
 */
static int enqueue_message(ipc_queue_t* queue, ipc_message_t* message) {
    if (!queue || !message) {
        return IPC_ERROR_INVALID_MSG;
    }
    
    if (queue->current_count >= queue->max_messages) {
        return IPC_ERROR_QUEUE_FULL;
    }
    
    message->next = NULL;
    message->prev = queue->tail;
    
    if (queue->tail) {
        queue->tail->next = message;
    } else {
        queue->head = message;
    }
    
    queue->tail = message;
    queue->current_count++;
    
    return IPC_SUCCESS;
}

/**
 * Remove message from queue
 */
static ipc_message_t* dequeue_message(ipc_queue_t* queue, uint32_t flags) {
    if (!queue || !queue->head) {
        return NULL;
    }
    
    ipc_message_t* message = queue->head;
    queue->head = message->next;
    
    if (queue->head) {
        queue->head->prev = NULL;
    } else {
        queue->tail = NULL;
    }
    
    queue->current_count--;
    message->next = message->prev = NULL;
    
    return message;
}

/**
 * Wake up blocked processes (placeholder)
 */
static void wakeup_blocked_processes(ipc_queue_t* queue, bool senders, bool receivers) {
    // In a full implementation, this would wake up blocked processes
    // For now, just reset counters
    if (senders) {
        queue->blocked_senders = 0;
    }
    if (receivers) {
        queue->blocked_receivers = 0;
    }
}

/**
 * Update system statistics
 */
static void update_statistics(void) {
    // Update memory usage
    uint64_t memory_used = 0;
    
    // Count queue memory
    ipc_queue_t* queue = queue_list;
    while (queue) {
        memory_used += sizeof(ipc_queue_t);
        
        // Count messages in queue
        ipc_message_t* msg = queue->head;
        while (msg) {
            memory_used += sizeof(ipc_message_t);
            msg = msg->next;
        }
        
        queue = queue->next;
    }
    
    // Count channel memory
    ipc_channel_t* channel = channel_list;
    while (channel) {
        memory_used += sizeof(ipc_channel_t);
        channel = channel->next;
    }
    
    ipc_statistics.memory_used = memory_used;
}
