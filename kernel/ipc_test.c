/* IKOS IPC System Test Program
 * Demonstrates message-passing based inter-process communication
 */

#include "ipc.h"
#include "scheduler.h"

/* Test task functions */
void producer_task(void);
void consumer_task(void);
void server_task(void);
void client_task(void);
void broadcast_sender_task(void);
void broadcast_receiver_task(void);

/* Utility functions */
void print_string(const char* str);
void print_number(uint32_t num);
void print_ipc_stats(void);

/* Global test data */
static uint32_t test_channel_id;
static uint32_t server_queue_id;
static uint32_t broadcast_channel_id;

/**
 * Main IPC test function
 */
int main(void) {
    print_string("IKOS Inter-Process Communication Test\n");
    print_string("====================================\n\n");
    
    // Initialize IPC system
    print_string("Initializing IPC system...\n");
    if (ipc_init() != IPC_SUCCESS) {
        print_string("ERROR: Failed to initialize IPC system\n");
        return -1;
    }
    print_string("IPC system initialized successfully\n\n");
    
    // Create test channels
    print_string("Creating test channels...\n");
    
    test_channel_id = ipc_create_channel("test-channel", false, false);
    if (test_channel_id == IPC_INVALID_CHANNEL) {
        print_string("ERROR: Failed to create test channel\n");
        return -1;
    }
    print_string("Created test channel (ID: ");
    print_number(test_channel_id);
    print_string(")\n");
    
    broadcast_channel_id = ipc_create_channel("broadcast-channel", true, false);
    if (broadcast_channel_id == IPC_INVALID_CHANNEL) {
        print_string("ERROR: Failed to create broadcast channel\n");
        return -1;
    }
    print_string("Created broadcast channel (ID: ");
    print_number(broadcast_channel_id);
    print_string(")\n\n");
    
    // Create message queues for different tasks
    print_string("Creating message queues...\n");
    
    server_queue_id = ipc_create_queue(32, IPC_PERM_ALL);
    if (server_queue_id == IPC_INVALID_CHANNEL) {
        print_string("ERROR: Failed to create server queue\n");
        return -1;
    }
    print_string("Created server queue (ID: ");
    print_number(server_queue_id);
    print_string(")\n\n");
    
    // Create test tasks
    print_string("Creating IPC test tasks...\n");
    
    task_t* producer = task_create("Producer", producer_task, PRIORITY_NORMAL, 4096);
    if (!producer) {
        print_string("ERROR: Failed to create Producer task\n");
        return -1;
    }
    print_string("Created Producer task (PID: ");
    print_number(producer->pid);
    print_string(")\n");
    
    task_t* consumer = task_create("Consumer", consumer_task, PRIORITY_NORMAL, 4096);
    if (!consumer) {
        print_string("ERROR: Failed to create Consumer task\n");
        return -1;
    }
    print_string("Created Consumer task (PID: ");
    print_number(consumer->pid);
    print_string(")\n");
    
    task_t* server = task_create("Server", server_task, PRIORITY_HIGH, 4096);
    if (!server) {
        print_string("ERROR: Failed to create Server task\n");
        return -1;
    }
    print_string("Created Server task (PID: ");
    print_number(server->pid);
    print_string(")\n");
    
    task_t* client = task_create("Client", client_task, PRIORITY_NORMAL, 4096);
    if (!client) {
        print_string("ERROR: Failed to create Client task\n");
        return -1;
    }
    print_string("Created Client task (PID: ");
    print_number(client->pid);
    print_string(")\n");
    
    task_t* bc_sender = task_create("BCSender", broadcast_sender_task, PRIORITY_NORMAL, 4096);
    if (!bc_sender) {
        print_string("ERROR: Failed to create Broadcast Sender task\n");
        return -1;
    }
    print_string("Created Broadcast Sender task (PID: ");
    print_number(bc_sender->pid);
    print_string(")\n");
    
    task_t* bc_receiver = task_create("BCReceiver", broadcast_receiver_task, PRIORITY_NORMAL, 4096);
    if (!bc_receiver) {
        print_string("ERROR: Failed to create Broadcast Receiver task\n");
        return -1;
    }
    print_string("Created Broadcast Receiver task (PID: ");
    print_number(bc_receiver->pid);
    print_string(")\n\n");
    
    // Subscribe tasks to channels
    print_string("Subscribing tasks to channels...\n");
    ipc_subscribe_channel(test_channel_id, producer->pid);
    ipc_subscribe_channel(test_channel_id, consumer->pid);
    ipc_subscribe_channel(broadcast_channel_id, bc_sender->pid);
    ipc_subscribe_channel(broadcast_channel_id, bc_receiver->pid);
    print_string("Channel subscriptions complete\n\n");
    
    print_string("Starting IPC demonstration...\n");
    print_string("Tasks will communicate via messages\n\n");
    
    // Main monitoring loop
    uint32_t stats_counter = 0;
    while (1) {
        // Yield to other tasks
        sys_yield();
        
        // Display IPC statistics periodically
        if (++stats_counter % 2000 == 0) {
            print_ipc_stats();
        }
    }
    
    return 0;
}

/**
 * Producer task - sends messages to channel
 */
void producer_task(void) {
    uint32_t message_count = 0;
    
    while (1) {
        message_count++;
        
        // Create message
        ipc_message_t* message = ipc_alloc_message(64);
        if (!message) {
            print_string("Producer: Failed to allocate message\n");
            sys_yield();
            continue;
        }
        
        // Fill message data
        message->type = IPC_MSG_DATA;
        message->priority = IPC_PRIORITY_NORMAL;
        sprintf((char*)message->data, "Hello from Producer #%u", message_count);
        message->data_size = strlen((char*)message->data) + 1;
        
        print_string("Producer: Sending message #");
        print_number(message_count);
        print_string("\n");
        
        // Send to channel
        int result = ipc_send_to_channel(test_channel_id, message, IPC_FLAG_NON_BLOCKING);
        if (result != IPC_SUCCESS) {
            print_string("Producer: Failed to send message (error: ");
            print_number(result);
            print_string(")\n");
        }
        
        ipc_free_message(message);
        
        // Send every 5 yields
        for (int i = 0; i < 5; i++) {
            sys_yield();
        }
    }
}

/**
 * Consumer task - receives messages from channel
 */
void consumer_task(void) {
    while (1) {
        ipc_message_t message;
        
        // Try to receive message from our queue
        task_t* current = task_get_current();
        uint32_t my_queue = ipc_create_queue(16, IPC_PERM_ALL);
        
        if (my_queue != IPC_INVALID_CHANNEL) {
            int result = ipc_receive_message(my_queue, &message, IPC_FLAG_NON_BLOCKING);
            if (result == IPC_SUCCESS) {
                print_string("Consumer: Received message: '");
                print_string((char*)message.data);
                print_string("' from PID ");
                print_number(message.sender_pid);
                print_string("\n");
            }
        }
        
        // Yield CPU
        sys_yield();
    }
}

/**
 * Server task - handles client requests
 */
void server_task(void) {
    print_string("Server: Starting request-reply server\n");
    
    while (1) {
        ipc_message_t request;
        
        // Check for incoming requests
        int result = ipc_receive_message(server_queue_id, &request, IPC_FLAG_NON_BLOCKING);
        if (result == IPC_SUCCESS) {
            print_string("Server: Received request from PID ");
            print_number(request.sender_pid);
            print_string(": '");
            print_string((char*)request.data);
            print_string("'\n");
            
            // Process request and send reply
            ipc_message_t* reply = ipc_alloc_message(128);
            if (reply) {
                reply->type = IPC_MSG_REPLY;
                reply->reply_to = request.msg_id;
                reply->priority = IPC_PRIORITY_HIGH;
                
                sprintf((char*)reply->data, "Server response to: %s", (char*)request.data);
                reply->data_size = strlen((char*)reply->data) + 1;
                
                int send_result = ipc_send_reply(request.sender_pid, reply);
                if (send_result == IPC_SUCCESS) {
                    print_string("Server: Sent reply to PID ");
                    print_number(request.sender_pid);
                    print_string("\n");
                } else {
                    print_string("Server: Failed to send reply\n");
                }
                
                ipc_free_message(reply);
            }
        }
        
        sys_yield();
    }
}

/**
 * Client task - sends requests to server
 */
void client_task(void) {
    uint32_t request_count = 0;
    
    // Wait a bit for server to start
    for (int i = 0; i < 10; i++) {
        sys_yield();
    }
    
    while (1) {
        request_count++;
        
        // Create request
        ipc_message_t* request = ipc_alloc_message(64);
        ipc_message_t reply;
        
        if (request) {
            sprintf((char*)request->data, "Client request #%u", request_count);
            request->data_size = strlen((char*)request->data) + 1;
            
            print_string("Client: Sending request #");
            print_number(request_count);
            print_string("\n");
            
            // Send synchronous request
            task_t* server_task_ptr = task_get_by_pid(3); // Assuming server has PID 3
            if (server_task_ptr) {
                int result = ipc_send_request(server_task_ptr->pid, request, &reply, 1000);
                if (result == IPC_SUCCESS) {
                    print_string("Client: Received reply: '");
                    print_string((char*)reply.data);
                    print_string("'\n");
                } else {
                    print_string("Client: Request failed or timed out\n");
                }
            }
            
            ipc_free_message(request);
        }
        
        // Send request every 15 yields
        for (int i = 0; i < 15; i++) {
            sys_yield();
        }
    }
}

/**
 * Broadcast sender task
 */
void broadcast_sender_task(void) {
    uint32_t broadcast_count = 0;
    
    while (1) {
        broadcast_count++;
        
        // Create broadcast message
        ipc_message_t* message = ipc_alloc_message(96);
        if (message) {
            message->type = IPC_MSG_NOTIFICATION;
            message->priority = IPC_PRIORITY_NORMAL;
            message->flags = IPC_FLAG_BROADCAST;
            
            sprintf((char*)message->data, "Broadcast notification #%u from sender", broadcast_count);
            message->data_size = strlen((char*)message->data) + 1;
            
            print_string("Broadcast Sender: Sending notification #");
            print_number(broadcast_count);
            print_string("\n");
            
            // Send to broadcast channel
            int result = ipc_send_to_channel(broadcast_channel_id, message, IPC_FLAG_NON_BLOCKING);
            if (result != IPC_SUCCESS) {
                print_string("Broadcast Sender: Failed to send broadcast\n");
            }
            
            ipc_free_message(message);
        }
        
        // Send broadcast every 20 yields
        for (int i = 0; i < 20; i++) {
            sys_yield();
        }
    }
}

/**
 * Broadcast receiver task
 */
void broadcast_receiver_task(void) {
    while (1) {
        ipc_message_t message;
        
        // Try to receive broadcast messages
        task_t* current = task_get_current();
        uint32_t my_queue = ipc_create_queue(8, IPC_PERM_ALL);
        
        if (my_queue != IPC_INVALID_CHANNEL) {
            int result = ipc_receive_message(my_queue, &message, IPC_FLAG_NON_BLOCKING);
            if (result == IPC_SUCCESS && (message.flags & IPC_FLAG_BROADCAST)) {
                print_string("Broadcast Receiver: Got broadcast: '");
                print_string((char*)message.data);
                print_string("'\n");
            }
        }
        
        sys_yield();
    }
}

/**
 * Print IPC system statistics
 */
void print_ipc_stats(void) {
    ipc_stats_t* stats = ipc_get_stats();
    
    print_string("=== IPC System Statistics ===\n");
    print_string("Messages sent: ");
    print_number((uint32_t)stats->total_messages_sent);
    print_string("\nMessages received: ");
    print_number((uint32_t)stats->total_messages_received);
    print_string("\nMessages dropped: ");
    print_number((uint32_t)stats->total_messages_dropped);
    print_string("\nActive queues: ");
    print_number(stats->active_queues);
    print_string("\nActive channels: ");
    print_number(stats->active_channels);
    print_string("\nMemory used: ");
    print_number((uint32_t)stats->memory_used);
    print_string(" bytes\n");
    
    print_string("Current task: ");
    task_t* current = task_get_current();
    if (current) {
        print_string(current->name);
        print_string(" (PID: ");
        print_number(current->pid);
        print_string(")");
    }
    print_string("\n\n");
}

/**
 * Print string to console (placeholder implementation)
 */
void print_string(const char* str) {
    // Simple VGA text mode output
    static volatile char* video_memory = (char*)0xB8000;
    static int cursor_pos = 0;
    
    while (*str) {
        if (*str == '\n') {
            cursor_pos = (cursor_pos / 160 + 1) * 160;
        } else {
            video_memory[cursor_pos] = *str;
            video_memory[cursor_pos + 1] = 0x07; // White on black
            cursor_pos += 2;
        }
        
        // Wrap around if at end of screen
        if (cursor_pos >= 160 * 25 * 2) {
            cursor_pos = 0;
        }
        
        str++;
    }
}

/**
 * Print number to console
 */
void print_number(uint32_t num) {
    char buffer[32];
    char* ptr = buffer + sizeof(buffer) - 1;
    *ptr = '\0';
    
    if (num == 0) {
        *(--ptr) = '0';
    } else {
        while (num > 0) {
            *(--ptr) = '0' + (num % 10);
            num /= 10;
        }
    }
    
    print_string(ptr);
}

/**
 * Simple sprintf implementation for message formatting
 */
int sprintf(char* buffer, const char* format, ...) {
    // Simplified sprintf - just handles %s and %u
    va_list args;
    va_start(args, format);
    
    char* dest = buffer;
    const char* src = format;
    
    while (*src) {
        if (*src == '%') {
            src++;
            if (*src == 's') {
                char* str = va_arg(args, char*);
                while (*str) {
                    *dest++ = *str++;
                }
            } else if (*src == 'u') {
                uint32_t num = va_arg(args, uint32_t);
                char temp[16];
                char* ptr = temp + sizeof(temp) - 1;
                *ptr = '\0';
                
                if (num == 0) {
                    *(--ptr) = '0';
                } else {
                    while (num > 0) {
                        *(--ptr) = '0' + (num % 10);
                        num /= 10;
                    }
                }
                
                while (*ptr) {
                    *dest++ = *ptr++;
                }
            }
            src++;
        } else {
            *dest++ = *src++;
        }
    }
    
    *dest = '\0';
    va_end(args);
    
    return dest - buffer;
}
