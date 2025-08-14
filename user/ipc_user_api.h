/* IKOS User-Space IPC API Header
 * Provides a complete user-space interface for inter-process communication
 */
#ifndef IPC_USER_API_H
#define IPC_USER_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ipc.h"

/* Maximum number of handlers per process */
#define IPC_USER_MAX_HANDLERS 32
#define IPC_USER_MAX_CHANNELS 16

/* Message handler callback function */
typedef void (*ipc_message_handler_t)(const ipc_message_t* message, void* user_data);

/* Channel event handler */
typedef void (*ipc_channel_handler_t)(uint32_t channel_id, const ipc_message_t* message, void* user_data);

/* Handler registration structure */
typedef struct {
    uint32_t queue_id;
    ipc_message_handler_t handler;
    void* user_data;
    bool active;
    uint32_t message_type_filter;  /* 0 = all types */
} ipc_handler_entry_t;

/* Channel subscription structure */
typedef struct {
    uint32_t channel_id;
    char name[64];
    ipc_channel_handler_t handler;
    void* user_data;
    bool active;
} ipc_channel_subscription_t;

/* User-space IPC context */
typedef struct {
    uint32_t process_queue_id;
    ipc_handler_entry_t handlers[IPC_USER_MAX_HANDLERS];
    ipc_channel_subscription_t channels[IPC_USER_MAX_CHANNELS];
    uint32_t handler_count;
    uint32_t channel_count;
    bool initialized;
    bool polling_active;
} ipc_user_context_t;

/* ===== Core Queue Operations ===== */

/* Initialize user-space IPC system */
int ipc_user_init(void);

/* Cleanup user-space IPC system */
void ipc_user_cleanup(void);

/* Create a message queue for the current process */
uint32_t ipc_user_create_queue(uint32_t max_messages, uint32_t permissions);

/* Destroy a message queue */
int ipc_user_destroy_queue(uint32_t queue_id);

/* Send a message to a queue (blocking/non-blocking) */
int ipc_user_send_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags);

/* Receive a message from a queue (blocking/non-blocking) */
int ipc_user_receive_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags);

/* Peek at next message without removing it */
int ipc_user_peek_message(uint32_t queue_id, ipc_message_t* message);

/* ===== Synchronous Communication ===== */

/* Send request and wait for reply */
int ipc_user_send_request(uint32_t target_pid, ipc_message_t* request, 
                          ipc_message_t* reply, uint32_t timeout_ms);

/* Send reply to a request */
int ipc_user_send_reply(uint32_t target_pid, ipc_message_t* reply);

/* ===== Asynchronous Communication ===== */

/* Send asynchronous message */
int ipc_user_send_async(uint32_t target_pid, ipc_message_t* message);

/* Broadcast message to multiple processes */
int ipc_user_broadcast(ipc_message_t* message, uint32_t* target_pids, uint32_t count);

/* ===== Channel Operations ===== */

/* Create a named channel */
uint32_t ipc_user_create_channel(const char* name, bool is_broadcast, bool is_persistent);

/* Subscribe to a channel */
int ipc_user_subscribe_channel(const char* name, ipc_channel_handler_t handler, void* user_data);

/* Unsubscribe from a channel */
int ipc_user_unsubscribe_channel(const char* name);

/* Send message to a channel */
int ipc_user_send_to_channel(const char* name, ipc_message_t* message, uint32_t flags);

/* ===== Message Handler Registration ===== */

/* Register a message handler for specific queue */
int ipc_user_register_handler(uint32_t queue_id, ipc_message_handler_t handler, 
                              void* user_data, uint32_t message_type_filter);

/* Unregister a message handler */
int ipc_user_unregister_handler(uint32_t queue_id);

/* Register handler for all messages to current process */
int ipc_user_register_default_handler(ipc_message_handler_t handler, void* user_data);

/* ===== Message Processing ===== */

/* Poll for messages and call handlers (non-blocking) */
void ipc_user_poll_handlers(void);

/* Start background message polling thread */
int ipc_user_start_polling(void);

/* Stop background message polling */
void ipc_user_stop_polling(void);

/* Process single message from queue */
int ipc_user_process_message(uint32_t queue_id);

/* ===== Message Utilities ===== */

/* Allocate a new message */
ipc_message_t* ipc_user_alloc_message(uint32_t data_size);

/* Free a message */
void ipc_user_free_message(ipc_message_t* message);

/* Copy message data */
int ipc_user_copy_message(ipc_message_t* dest, const ipc_message_t* src);

/* Create simple data message */
ipc_message_t* ipc_user_create_data_message(const void* data, uint32_t size, uint32_t target_pid);

/* Create request message */
ipc_message_t* ipc_user_create_request(const void* data, uint32_t size, uint32_t target_pid);

/* Create reply message */
ipc_message_t* ipc_user_create_reply(const void* data, uint32_t size, uint32_t target_pid, uint32_t reply_to);

/* ===== Context and Status ===== */

/* Get user IPC context */
ipc_user_context_t* ipc_user_get_context(void);

/* Get current process queue ID */
uint32_t ipc_user_get_process_queue(void);

/* Check if queue has pending messages */
bool ipc_user_has_messages(uint32_t queue_id);

/* Get number of pending messages */
uint32_t ipc_user_get_message_count(uint32_t queue_id);

/* ===== Error Handling ===== */

/* Get last IPC error code */
int ipc_user_get_last_error(void);

/* Get error description string */
const char* ipc_user_error_string(int error_code);

/* ===== Convenience Macros ===== */

/* Send message with default flags */
#define ipc_user_send(queue_id, message) \
    ipc_user_send_message(queue_id, message, IPC_FLAG_BLOCKING)

/* Send message non-blocking */
#define ipc_user_send_nb(queue_id, message) \
    ipc_user_send_message(queue_id, message, IPC_FLAG_NON_BLOCKING)

/* Receive message with default flags */
#define ipc_user_receive(queue_id, message) \
    ipc_user_receive_message(queue_id, message, IPC_FLAG_BLOCKING)

/* Receive message non-blocking */
#define ipc_user_receive_nb(queue_id, message) \
    ipc_user_receive_message(queue_id, message, IPC_FLAG_NON_BLOCKING)

#endif /* IPC_USER_API_H */
