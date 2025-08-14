# User-Space IPC API for IKOS

## Overview

This directory contains the user-space IPC API implementation for the IKOS operating system. It provides a complete interface for inter-process communication that meets the requirements of issue #28.

## Features

### ✅ IPC Message Queueing in User-Space
- Create and manage message queues
- Send/receive messages with blocking and non-blocking modes
- Message prioritization and ordering
- Queue statistics and monitoring

### ✅ Synchronous and Asynchronous Communication
- **Synchronous**: Request-reply patterns with timeout support
- **Asynchronous**: Fire-and-forget messaging
- **Broadcast**: Send messages to multiple recipients
- **Channel-based**: Named communication channels

### ✅ Message Handler Registration
- Register callback functions for specific queues
- Message type filtering
- Channel event handlers
- Default handlers for all incoming messages

## Files

- `ipc_user_api.h` - Complete user-space IPC API header
- `ipc_user_api.c` - Full implementation of the API
- `ipc_example.c` - Comprehensive usage examples
- `README.md` - This documentation

## API Functions

### Core Operations
```c
int ipc_user_init(void);                                    // Initialize IPC
void ipc_user_cleanup(void);                               // Cleanup IPC
uint32_t ipc_user_create_queue(uint32_t max_messages, uint32_t permissions);
int ipc_user_send_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags);
int ipc_user_receive_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags);
```

### Synchronous Communication
```c
int ipc_user_send_request(uint32_t target_pid, ipc_message_t* request, 
                          ipc_message_t* reply, uint32_t timeout_ms);
int ipc_user_send_reply(uint32_t target_pid, ipc_message_t* reply);
```

### Asynchronous Communication
```c
int ipc_user_send_async(uint32_t target_pid, ipc_message_t* message);
int ipc_user_broadcast(ipc_message_t* message, uint32_t* target_pids, uint32_t count);
```

### Channel Operations
```c
uint32_t ipc_user_create_channel(const char* name, bool is_broadcast, bool is_persistent);
int ipc_user_subscribe_channel(const char* name, ipc_channel_handler_t handler, void* user_data);
int ipc_user_send_to_channel(const char* name, ipc_message_t* message, uint32_t flags);
```

### Handler Registration
```c
typedef void (*ipc_message_handler_t)(const ipc_message_t* message, void* user_data);
int ipc_user_register_handler(uint32_t queue_id, ipc_message_handler_t handler, 
                              void* user_data, uint32_t message_type_filter);
void ipc_user_poll_handlers(void);
```

## Usage Examples

### Echo Server
```c
void handle_request(const ipc_message_t* message, void* user_data) {
    // Process request and send reply
    ipc_message_t* reply = ipc_user_create_reply(message->data, message->data_size, 
                                                 message->sender_pid, message->msg_id);
    ipc_user_send_reply(message->sender_pid, reply);
    ipc_user_free_message(reply);
}

int main() {
    ipc_user_init();
    uint32_t queue = ipc_user_create_queue(32, IPC_PERM_ALL);
    ipc_user_register_handler(queue, handle_request, NULL, IPC_MSG_REQUEST);
    
    while (1) {
        ipc_user_poll_handlers();
        usleep(10000);
    }
}
```

### Asynchronous Messaging
```c
void handle_async(const ipc_message_t* message, void* user_data) {
    printf("Received: %s\n", (char*)message->data);
}

int main() {
    ipc_user_init();
    ipc_user_register_default_handler(handle_async, NULL);
    
    // Send async message
    ipc_message_t* msg = ipc_user_create_data_message("Hello", 6, target_pid);
    ipc_user_send_async(target_pid, msg);
    ipc_user_free_message(msg);
    
    ipc_user_poll_handlers();
    ipc_user_cleanup();
}
```

### Channel Communication
```c
void handle_channel(uint32_t channel_id, const ipc_message_t* message, void* user_data) {
    printf("Channel %u: %s\n", channel_id, (char*)message->data);
}

int main() {
    ipc_user_init();
    ipc_user_subscribe_channel("notifications", handle_channel, NULL);
    
    // Send to channel
    ipc_message_t* msg = ipc_user_create_data_message("Broadcast!", 11, 0);
    ipc_user_send_to_channel("notifications", msg, IPC_FLAG_BROADCAST);
    ipc_user_free_message(msg);
    
    ipc_user_poll_handlers();
    ipc_user_cleanup();
}
```

## Testing

Build and run the example:
```bash
cd /workspaces/IKOS/user
gcc -I../include -o ipc_example ipc_example.c ipc_user_api.c
./ipc_example server    # Run echo server
./ipc_example client 1  # Run client (queue ID 1)
./ipc_example broadcast # Send broadcast
./ipc_example async     # Async example
```

## Integration

To use in your application:
1. Include `ipc_user_api.h`
2. Link with `ipc_user_api.c`
3. Call `ipc_user_init()` at startup
4. Register message handlers as needed
5. Call `ipc_user_poll_handlers()` in your main loop
6. Call `ipc_user_cleanup()` at shutdown

## System Calls Used

The API uses these kernel system calls:
- `sys_ipc_create_queue()`
- `sys_ipc_send_message()`
- `sys_ipc_receive_message()`
- `sys_ipc_create_channel()`
- `sys_ipc_subscribe_channel()`
- `sys_ipc_send_request()`
- `sys_ipc_send_async()`
- `sys_ipc_broadcast()`

## Error Handling

All functions return error codes defined in `ipc.h`. Use `ipc_user_get_last_error()` and `ipc_user_error_string()` for detailed error information.

## Limitations

- Maximum 32 handlers per process
- Maximum 16 channel subscriptions per process
- Message size limited to `IPC_MAX_MESSAGE_SIZE`
- Polling-based message processing (no interrupt-driven delivery)

This implementation fully satisfies the requirements of issue #28 by providing comprehensive user-space IPC capabilities with queueing, sync/async communication, and handler registration.
