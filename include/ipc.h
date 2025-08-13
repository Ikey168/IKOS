/* IKOS Inter-Process Communication (IPC) System
 * Provides message-passing based communication between processes
 */

#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include <stdbool.h>

/* Maximum message size in bytes */
#define IPC_MAX_MESSAGE_SIZE    4096
#define IPC_MAX_QUEUE_SIZE      64
#define IPC_MAX_CHANNELS        256
#define IPC_INVALID_CHANNEL     0xFFFFFFFF

/* IPC Message Types */
typedef enum {
    IPC_MSG_DATA        = 0x01,    /* Regular data message */
    IPC_MSG_REQUEST     = 0x02,    /* Request message (expects reply) */
    IPC_MSG_REPLY       = 0x03,    /* Reply to request */
    IPC_MSG_NOTIFICATION = 0x04,   /* Asynchronous notification */
    IPC_MSG_SIGNAL      = 0x05,    /* Process signal */
    IPC_MSG_CONTROL     = 0x06     /* Control/management message */
} ipc_msg_type_t;

/* IPC Message Priorities */
typedef enum {
    IPC_PRIORITY_LOW    = 0,
    IPC_PRIORITY_NORMAL = 1,
    IPC_PRIORITY_HIGH   = 2,
    IPC_PRIORITY_URGENT = 3
} ipc_priority_t;

/* IPC Message Flags */
#define IPC_FLAG_BLOCKING       0x01    /* Blocking send/receive */
#define IPC_FLAG_NON_BLOCKING   0x02    /* Non-blocking operation */
#define IPC_FLAG_BROADCAST      0x04    /* Broadcast to all subscribers */
#define IPC_FLAG_MULTICAST      0x08    /* Send to multiple recipients */
#define IPC_FLAG_RELIABLE       0x10    /* Guaranteed delivery */
#define IPC_FLAG_ORDERED        0x20    /* Maintain message order */

/* IPC Message Structure */
typedef struct ipc_message {
    uint32_t msg_id;                    /* Unique message identifier */
    uint32_t sender_pid;                /* Sender process ID */
    uint32_t receiver_pid;              /* Receiver process ID (0 = broadcast) */
    uint32_t channel_id;                /* Channel identifier */
    
    ipc_msg_type_t type;                /* Message type */
    ipc_priority_t priority;            /* Message priority */
    uint32_t flags;                     /* Message flags */
    
    uint32_t data_size;                 /* Size of data payload */
    uint64_t timestamp;                 /* Message timestamp */
    uint32_t sequence_number;           /* Sequence number for ordering */
    uint32_t reply_to;                  /* Message ID this is replying to */
    
    uint8_t data[IPC_MAX_MESSAGE_SIZE]; /* Message payload */
    
    /* Internal queue management */
    struct ipc_message* next;           /* Next message in queue */
    struct ipc_message* prev;           /* Previous message in queue */
} ipc_message_t;

/* Message Queue Structure */
typedef struct ipc_queue {
    uint32_t queue_id;                  /* Queue identifier */
    uint32_t owner_pid;                 /* Process that owns this queue */
    uint32_t max_messages;              /* Maximum messages in queue */
    uint32_t current_count;             /* Current message count */
    
    ipc_message_t* head;                /* First message in queue */
    ipc_message_t* tail;                /* Last message in queue */
    
    /* Synchronization */
    uint32_t blocked_senders;           /* Count of blocked sender processes */
    uint32_t blocked_receivers;         /* Count of blocked receiver processes */
    
    /* Access control */
    uint32_t permissions;               /* Queue access permissions */
    bool is_public;                     /* Public queue accessible by all */
    
    /* Statistics */
    uint64_t total_sent;                /* Total messages sent */
    uint64_t total_received;            /* Total messages received */
    uint64_t total_dropped;             /* Total messages dropped */
    
    struct ipc_queue* next;             /* Next queue in list */
} ipc_queue_t;

/* IPC Channel Structure for named communication */
typedef struct ipc_channel {
    uint32_t channel_id;                /* Channel identifier */
    char name[64];                      /* Channel name */
    uint32_t creator_pid;               /* Process that created channel */
    
    /* Subscriber management */
    uint32_t subscribers[32];           /* List of subscribed PIDs */
    uint32_t subscriber_count;          /* Number of subscribers */
    
    /* Channel properties */
    bool is_broadcast;                  /* Broadcast channel */
    bool is_persistent;                 /* Messages persist if no receivers */
    uint32_t max_message_size;          /* Maximum message size for channel */
    
    /* Message buffer for persistent channels */
    ipc_message_t* persistent_messages; /* Buffered messages */
    uint32_t buffered_count;            /* Number of buffered messages */
    
    struct ipc_channel* next;           /* Next channel in list */
} ipc_channel_t;

/* IPC Statistics */
typedef struct ipc_stats {
    uint64_t total_messages_sent;       /* Total messages sent system-wide */
    uint64_t total_messages_received;   /* Total messages received */
    uint64_t total_messages_dropped;    /* Total messages dropped */
    uint64_t total_queues_created;      /* Total queues created */
    uint64_t total_channels_created;    /* Total channels created */
    uint32_t active_queues;             /* Currently active queues */
    uint32_t active_channels;           /* Currently active channels */
    uint64_t memory_used;               /* Memory used by IPC system */
} ipc_stats_t;

/* IPC System Calls */

/* Initialize IPC system */
int ipc_init(void);

/* Message Queue Operations */
uint32_t ipc_create_queue(uint32_t max_messages, uint32_t permissions);
int ipc_destroy_queue(uint32_t queue_id);
int ipc_send_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags);
int ipc_receive_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags);
int ipc_peek_message(uint32_t queue_id, ipc_message_t* message);

/* Channel Operations */
uint32_t ipc_create_channel(const char* name, bool is_broadcast, bool is_persistent);
int ipc_destroy_channel(uint32_t channel_id);
int ipc_subscribe_channel(uint32_t channel_id, uint32_t pid);
int ipc_unsubscribe_channel(uint32_t channel_id, uint32_t pid);
int ipc_send_to_channel(uint32_t channel_id, ipc_message_t* message, uint32_t flags);

/* Synchronous Communication */
int ipc_send_request(uint32_t target_pid, ipc_message_t* request, 
                     ipc_message_t* reply, uint32_t timeout_ms);
int ipc_send_reply(uint32_t target_pid, ipc_message_t* reply);

/* Asynchronous Communication */
int ipc_send_async(uint32_t target_pid, ipc_message_t* message);
int ipc_receive_async(ipc_message_t* message, uint32_t timeout_ms);

/* Broadcast/Multicast */
int ipc_broadcast(ipc_message_t* message, uint32_t* target_pids, uint32_t count);
int ipc_multicast_channel(uint32_t channel_id, ipc_message_t* message);

/* Message Management */
ipc_message_t* ipc_alloc_message(uint32_t data_size);
void ipc_free_message(ipc_message_t* message);
int ipc_copy_message(ipc_message_t* dest, const ipc_message_t* src);

/* Queue Management */
ipc_queue_t* ipc_get_queue(uint32_t queue_id);
ipc_queue_t* ipc_get_process_queue(uint32_t pid);
int ipc_flush_queue(uint32_t queue_id);
uint32_t ipc_get_queue_count(uint32_t queue_id);

/* Channel Management */
ipc_channel_t* ipc_find_channel(const char* name);
ipc_channel_t* ipc_get_channel(uint32_t channel_id);
int ipc_list_channels(char* buffer, uint32_t buffer_size);

/* Statistics and Monitoring */
ipc_stats_t* ipc_get_stats(void);
int ipc_get_queue_stats(uint32_t queue_id, ipc_queue_t* stats);
int ipc_get_process_ipc_info(uint32_t pid, char* buffer, uint32_t buffer_size);

/* Utility Functions */
uint32_t ipc_generate_msg_id(void);
uint64_t ipc_get_timestamp(void);
bool ipc_is_valid_pid(uint32_t pid);
int ipc_validate_message(const ipc_message_t* message);

/* Error Codes */
#define IPC_SUCCESS             0
#define IPC_ERROR_INVALID_PID   -1
#define IPC_ERROR_INVALID_QUEUE -2
#define IPC_ERROR_QUEUE_FULL    -3
#define IPC_ERROR_QUEUE_EMPTY   -4
#define IPC_ERROR_NO_MEMORY     -5
#define IPC_ERROR_TIMEOUT       -6
#define IPC_ERROR_PERMISSION    -7
#define IPC_ERROR_INVALID_MSG   -8
#define IPC_ERROR_CHANNEL_EXISTS -9
#define IPC_ERROR_CHANNEL_NOT_FOUND -10
#define IPC_ERROR_NOT_SUBSCRIBED -11
#define IPC_ERROR_INVALID_SIZE  -12

/* Permission Flags */
#define IPC_PERM_READ           0x01
#define IPC_PERM_WRITE          0x02
#define IPC_PERM_CREATE         0x04
#define IPC_PERM_DELETE         0x08
#define IPC_PERM_ALL            0xFF

#endif /* IPC_H */
