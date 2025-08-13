/* IKOS Process Manager Header
 * Comprehensive process management for handling multiple user processes
 */

#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "process.h"

/* Process Manager Configuration */
#define PM_MAX_PROCESSES            256     /* Maximum concurrent processes */
#define PM_MAX_PROCESS_NAME         64      /* Maximum process name length */
#define PM_MAX_PROCESS_ARGS         32      /* Maximum process arguments */
#define PM_PROCESS_HASH_SIZE        64      /* Hash table size for process lookup */
#define PM_IPC_BUFFER_SIZE          4096    /* IPC message buffer size */
#define PM_MAX_IPC_CHANNELS         128     /* Maximum IPC channels */

/* Process Manager States */
typedef enum {
    PM_STATE_UNINITIALIZED = 0,
    PM_STATE_INITIALIZING,
    PM_STATE_RUNNING,
    PM_STATE_SHUTTING_DOWN,
    PM_STATE_ERROR
} pm_state_t;

/* Process Table Entry Status */
typedef enum {
    PM_ENTRY_FREE = 0,          /* Entry is available */
    PM_ENTRY_ALLOCATED,         /* Entry is allocated but process not started */
    PM_ENTRY_ACTIVE,            /* Process is active */
    PM_ENTRY_ZOMBIE,            /* Process terminated but not cleaned up */
    PM_ENTRY_TERMINATING       /* Process is being terminated */
} pm_entry_status_t;

/* Process Creation Parameters */
typedef struct {
    char name[PM_MAX_PROCESS_NAME];         /* Process name */
    char* argv[PM_MAX_PROCESS_ARGS];        /* Process arguments */
    int argc;                               /* Argument count */
    char* envp[PM_MAX_PROCESS_ARGS];        /* Environment variables */
    int envc;                               /* Environment count */
    process_priority_t priority;            /* Process priority */
    uint64_t memory_limit;                  /* Memory usage limit */
    uint64_t time_limit;                    /* CPU time limit */
    uint32_t flags;                         /* Creation flags */
} pm_create_params_t;

/* Process creation flags */
#define PM_FLAG_INHERIT_ENV         0x0001  /* Inherit parent environment */
#define PM_FLAG_DETACHED           0x0002   /* Create detached process */
#define PM_FLAG_PRIVILEGED         0x0004   /* Elevated privileges */
#define PM_FLAG_REAL_TIME          0x0008   /* Real-time scheduling */

/* IPC Message Types */
typedef enum {
    PM_IPC_REQUEST = 1,         /* Process request to kernel */
    PM_IPC_RESPONSE,            /* Kernel response to process */
    PM_IPC_NOTIFICATION,        /* Kernel notification to process */
    PM_IPC_BROADCAST,           /* Broadcast message */
    PM_IPC_SIGNAL               /* Signal delivery */
} pm_ipc_type_t;

/* IPC Message Structure */
typedef struct {
    pm_ipc_type_t type;         /* Message type */
    uint32_t src_pid;           /* Source process ID */
    uint32_t dst_pid;           /* Destination process ID */
    uint32_t channel_id;        /* IPC channel ID */
    uint32_t message_id;        /* Unique message ID */
    uint32_t data_size;         /* Size of data payload */
    uint64_t timestamp;         /* Message timestamp */
    uint32_t flags;             /* Message flags */
    char data[PM_IPC_BUFFER_SIZE]; /* Message data */
} pm_ipc_message_t;

/* IPC Channel Structure */
typedef struct {
    uint32_t channel_id;        /* Channel identifier */
    uint32_t owner_pid;         /* Channel owner process */
    uint32_t permissions;       /* Access permissions */
    bool is_active;             /* Channel active status */
    pm_ipc_message_t* queue;    /* Message queue */
    uint32_t queue_size;        /* Queue size */
    uint32_t queue_head;        /* Queue head index */
    uint32_t queue_tail;        /* Queue tail index */
} pm_ipc_channel_t;

/* Process Table Entry */
typedef struct {
    pm_entry_status_t status;   /* Entry status */
    process_t* process;         /* Pointer to process structure */
    uint64_t creation_time;     /* Process creation timestamp */
    uint64_t last_activity;     /* Last activity timestamp */
    uint32_t hash_next;         /* Next entry in hash chain */
    pm_ipc_channel_t* ipc_channels[PM_MAX_IPC_CHANNELS]; /* IPC channels */
    uint32_t active_channels;   /* Number of active IPC channels */
} pm_process_entry_t;

/* Process Manager Statistics */
typedef struct {
    uint32_t total_created;     /* Total processes created */
    uint32_t total_terminated;  /* Total processes terminated */
    uint32_t current_active;    /* Currently active processes */
    uint32_t current_zombie;    /* Current zombie processes */
    uint32_t peak_active;       /* Peak active processes */
    uint64_t context_switches;  /* Total context switches */
    uint64_t ipc_messages;      /* Total IPC messages */
    uint64_t total_cpu_time;    /* Total CPU time consumed */
    uint64_t total_memory_used; /* Total memory allocated */
} pm_statistics_t;

/* Process Manager Structure */
typedef struct {
    pm_state_t state;           /* Process manager state */
    pm_process_entry_t table[PM_MAX_PROCESSES]; /* Process table */
    uint32_t hash_table[PM_PROCESS_HASH_SIZE];  /* Hash table for PID lookup */
    pm_ipc_channel_t ipc_channels[PM_MAX_IPC_CHANNELS]; /* IPC channels */
    pm_statistics_t stats;      /* Process manager statistics */
    uint32_t next_pid;          /* Next available PID */
    uint32_t next_channel_id;   /* Next available channel ID */
    volatile int lock;          /* Process manager lock */
} process_manager_t;

/* ================================
 * Process Manager Core Functions
 * ================================ */

/* Initialization and shutdown */
int pm_init(void);
int pm_shutdown(void);
pm_state_t pm_get_state(void);

/* Process creation and destruction */
int pm_create_process(const pm_create_params_t* params, uint32_t* pid_out);
int pm_create_process_from_elf(const char* name, void* elf_data, size_t elf_size, uint32_t* pid_out);
int pm_terminate_process(uint32_t pid, int exit_code);
int pm_kill_process(uint32_t pid, int signal);

/* Process lookup and management */
process_t* pm_get_process(uint32_t pid);
int pm_get_process_list(uint32_t* pids, uint32_t max_count, uint32_t* count_out);
int pm_wait_for_process(uint32_t pid, int* exit_code);

/* Process state management */
int pm_suspend_process(uint32_t pid);
int pm_resume_process(uint32_t pid);
int pm_set_process_priority(uint32_t pid, process_priority_t priority);
int pm_get_process_info(uint32_t pid, process_t* info_out);

/* ================================
 * Process Table Management
 * ================================ */

/* Process table operations */
int pm_table_add_process(process_t* process);
int pm_table_remove_process(uint32_t pid);
process_t* pm_table_lookup_process(uint32_t pid);
int pm_table_get_all_processes(process_t** processes, uint32_t* count);

/* Process table utilities */
uint32_t pm_table_allocate_pid(void);
void pm_table_free_pid(uint32_t pid);
bool pm_table_is_pid_valid(uint32_t pid);
uint32_t pm_table_hash_pid(uint32_t pid);

/* ================================
 * Inter-Process Communication
 * ================================ */

/* IPC channel management */
int pm_ipc_create_channel(uint32_t owner_pid, uint32_t* channel_id);
int pm_ipc_destroy_channel(uint32_t channel_id);
int pm_ipc_set_channel_permissions(uint32_t channel_id, uint32_t permissions);

/* IPC messaging */
int pm_ipc_send_message(const pm_ipc_message_t* message);
int pm_ipc_receive_message(uint32_t pid, uint32_t channel_id, pm_ipc_message_t* message_out);
int pm_ipc_broadcast_message(const pm_ipc_message_t* message);

/* IPC utilities */
int pm_ipc_create_message(pm_ipc_type_t type, uint32_t src_pid, uint32_t dst_pid, 
                         const void* data, size_t data_size, pm_ipc_message_t* msg_out);
bool pm_ipc_channel_has_messages(uint32_t channel_id);
int pm_ipc_get_channel_info(uint32_t channel_id, pm_ipc_channel_t* info_out);

/* ================================
 * System Call Interface
 * ================================ */

/* Process management system calls */
int sys_pm_create_process(const char* name, char* const argv[], char* const envp[]);
int sys_pm_exit_process(int exit_code);
int sys_pm_wait_process(uint32_t pid);
int sys_pm_get_process_info(uint32_t pid, process_t* info);

/* IPC system calls */
int sys_pm_ipc_create_channel(uint32_t* channel_id);
int sys_pm_ipc_send(uint32_t channel_id, const void* data, size_t size);
int sys_pm_ipc_receive(uint32_t channel_id, void* buffer, size_t size);
int sys_pm_ipc_broadcast(const void* data, size_t size);

/* ================================
 * Monitoring and Statistics
 * ================================ */

/* Statistics */
void pm_get_statistics(pm_statistics_t* stats);
void pm_reset_statistics(void);
void pm_update_statistics(void);

/* Monitoring */
int pm_monitor_process(uint32_t pid, bool enable);
int pm_get_process_usage(uint32_t pid, uint64_t* cpu_time, uint64_t* memory_usage);
int pm_set_resource_limits(uint32_t pid, uint64_t memory_limit, uint64_t time_limit);

/* Debugging and diagnostics */
void pm_dump_process_table(void);
void pm_dump_ipc_channels(void);
void pm_dump_statistics(void);
int pm_validate_process_table(void);

/* ================================
 * Error Codes
 * ================================ */

#define PM_SUCCESS                  0       /* Operation successful */
#define PM_ERROR_INVALID_PARAM      -1      /* Invalid parameter */
#define PM_ERROR_NO_MEMORY          -2      /* Out of memory */
#define PM_ERROR_NOT_FOUND          -3      /* Process not found */
#define PM_ERROR_PERMISSION_DENIED  -4      /* Permission denied */
#define PM_ERROR_RESOURCE_LIMIT     -5      /* Resource limit exceeded */
#define PM_ERROR_INVALID_STATE      -6      /* Invalid state */
#define PM_ERROR_TIMEOUT            -7      /* Operation timeout */
#define PM_ERROR_IPC_FAILURE        -8      /* IPC operation failed */
#define PM_ERROR_TABLE_FULL         -9      /* Process table full */
#define PM_ERROR_ALREADY_EXISTS     -10     /* Process already exists */

#endif /* PROCESS_MANAGER_H */
