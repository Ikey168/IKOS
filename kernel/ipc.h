/* IKOS Inter-Process Communication (IPC) Header
 * Defines structures and functions for IPC in the IKOS kernel
 */

#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include <stdbool.h>

/* Constants */
#define IPC_MAX_QUEUE_SIZE      256
#define IPC_MAX_MESSAGE_SIZE    512
#define IPC_PERM_READ          0x01
#define IPC_PERM_WRITE         0x02
#define IPC_PERM_ALL           0x03

/* IPC Flags */
#define IPC_FLAG_NON_BLOCKING   0x01
#define IPC_FLAG_BROADCAST      0x02

/* Message Types */
#define IPC_MSG_NOTIFICATION    0
#define IPC_MSG_REQUEST         1
#define IPC_MSG_REPLY           2
#define IPC_MSG_KEYBOARD_EVENT  3
#define IPC_MSG_KEYBOARD_REGISTER   4
#define IPC_MSG_KEYBOARD_UNREGISTER 5

/* IPC Statistics Structure */
typedef struct {
    uint32_t total_queues_created;
    uint32_t active_queues;
    uint32_t total_messages_sent;
    uint32_t total_messages_received;
    uint64_t memory_used;
} ipc_stats_t;

/* IPC Message Structure */
typedef struct ipc_message {
    uint32_t msg_id;
    uint32_t type;
    uint32_t receiver_pid;
    uint32_t sender_pid;
    uint32_t data_size;
    uint32_t flags;
    uint64_t timestamp;
    struct ipc_message* next;
    struct ipc_message* prev;
    char data[IPC_MAX_MESSAGE_SIZE];
} ipc_message_t;

/* IPC Queue Structure */
typedef struct ipc_queue {
    uint32_t queue_id;
    uint32_t owner_pid;
    uint32_t max_messages;
    uint32_t current_count;
    uint32_t permissions;
    bool is_public;
    struct ipc_queue* next;
    ipc_message_t* head;
    ipc_message_t* tail;
    uint32_t blocked_senders;
    uint32_t blocked_receivers;
    uint32_t total_sent;
    uint32_t total_received;
} ipc_queue_t;

/* IPC Channel Structure */
typedef struct ipc_channel {
    uint32_t channel_id;
    uint32_t creator_pid;
    bool is_broadcast;
    bool is_persistent;
    uint32_t max_message_size;
    uint32_t subscriber_count;
    uint32_t subscribers[32];
    struct ipc_channel* next;
    char name[32];
} ipc_channel_t;

/* Function Prototypes */

/* IPC System Initialization */
int ipc_init(void);

/* Message Queue Management */
uint32_t ipc_create_queue(uint32_t max_messages, uint32_t permissions);
int ipc_destroy_queue(uint32_t queue_id);

/* Message Sending and Receiving */
int ipc_send_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags);
int ipc_receive_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags);
int ipc_peek_message(uint32_t queue_id, ipc_message_t* message);

/* Channel Management */
uint32_t ipc_create_channel(const char* name, bool is_broadcast, bool is_persistent);
int ipc_subscribe_channel(uint32_t channel_id, uint32_t pid);
int ipc_send_to_channel(uint32_t channel_id, ipc_message_t* message, uint32_t flags);

/* Request-Reply Communication */
int ipc_send_request(uint32_t target_pid, ipc_message_t* request, 
                     ipc_message_t* reply, uint32_t timeout_ms);
int ipc_send_reply(uint32_t target_pid, ipc_message_t* reply);

/* Asynchronous Messaging */
int ipc_send_async(uint32_t target_pid, ipc_message_t* message);
int ipc_broadcast(ipc_message_t* message, uint32_t* target_pids, uint32_t count);

/* Message Allocation and Freeing */
ipc_message_t* ipc_alloc_message(uint32_t data_size);
void ipc_free_message(ipc_message_t* message);

/* Message Copying */
int ipc_copy_message(ipc_message_t* dest, const ipc_message_t* src);

/* IPC Statistics */
ipc_stats_t* ipc_get_stats(void);

/* Keyboard Driver IPC Functions */
int ipc_register_keyboard_driver(uint32_t driver_pid);
int ipc_unregister_keyboard_driver(uint32_t driver_pid);
int ipc_send_keyboard_event(ipc_message_t* kbd_event);

#endif /* IPC_H */