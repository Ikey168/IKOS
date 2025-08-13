/* IKOS Process Manager Implementation
 * Comprehensive process management for handling multiple user processes
 */

#include "process_manager.h"
#include "process.h"
#include "elf.h"
#include "vmm.h"
#include "interrupts.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

/* Function declarations */
static void debug_print(const char* format, ...);

/* Global Process Manager instance */
static process_manager_t g_process_manager;
static bool g_pm_initialized = false;

/* Helper macros */
#define PM_LOCK() while (__sync_lock_test_and_set(&g_process_manager.lock, 1)) { /* spin */ }
#define PM_UNLOCK() __sync_lock_release(&g_process_manager.lock)

/* Forward declarations */
static pm_process_entry_t* pm_find_free_entry(void);
static void pm_hash_insert(uint32_t pid, uint32_t entry_index);
static void pm_hash_remove(uint32_t pid);
static uint32_t pm_hash_lookup(uint32_t pid);
static int pm_validate_create_params(const pm_create_params_t* params);
static uint64_t pm_get_timestamp(void);

/* ================================
 * Process Manager Core Functions
 * ================================ */

/**
 * Initialize the Process Manager
 */
int pm_init(void) {
    debug_print("Process Manager: Initializing...\n");
    
    if (g_pm_initialized) {
        debug_print("Process Manager: Already initialized\n");
        return PM_SUCCESS;
    }
    
    /* Clear the process manager structure */
    memset(&g_process_manager, 0, sizeof(process_manager_t));
    
    /* Initialize state */
    g_process_manager.state = PM_STATE_INITIALIZING;
    g_process_manager.next_pid = 1;
    g_process_manager.next_channel_id = 1;
    g_process_manager.lock = 0;
    
    /* Initialize process table entries */
    for (uint32_t i = 0; i < PM_MAX_PROCESSES; i++) {
        g_process_manager.table[i].status = PM_ENTRY_FREE;
        g_process_manager.table[i].process = NULL;
        g_process_manager.table[i].hash_next = UINT32_MAX;
        g_process_manager.table[i].active_channels = 0;
        
        /* Initialize IPC channels for this entry */
        for (uint32_t j = 0; j < PM_MAX_IPC_CHANNELS; j++) {
            g_process_manager.table[i].ipc_channels[j] = NULL;
        }
    }
    
    /* Initialize hash table */
    for (uint32_t i = 0; i < PM_PROCESS_HASH_SIZE; i++) {
        g_process_manager.hash_table[i] = UINT32_MAX;
    }
    
    /* Initialize IPC channels */
    for (uint32_t i = 0; i < PM_MAX_IPC_CHANNELS; i++) {
        g_process_manager.ipc_channels[i].channel_id = 0;
        g_process_manager.ipc_channels[i].is_active = false;
        g_process_manager.ipc_channels[i].queue = NULL;
    }
    
    /* Initialize statistics */
    memset(&g_process_manager.stats, 0, sizeof(pm_statistics_t));
    
    /* Initialize base process system */
    if (process_init() != 0) {
        debug_print("Process Manager: Failed to initialize base process system\n");
        g_process_manager.state = PM_STATE_ERROR;
        return PM_ERROR_INVALID_STATE;
    }
    
    g_process_manager.state = PM_STATE_RUNNING;
    g_pm_initialized = true;
    
    debug_print("Process Manager: Initialization complete\n");
    return PM_SUCCESS;
}

/**
 * Shutdown the Process Manager
 */
int pm_shutdown(void) {
    debug_print("Process Manager: Shutting down...\n");
    
    if (!g_pm_initialized) {
        return PM_SUCCESS;
    }
    
    PM_LOCK();
    g_process_manager.state = PM_STATE_SHUTTING_DOWN;
    
    /* Terminate all active processes */
    for (uint32_t i = 0; i < PM_MAX_PROCESSES; i++) {
        if (g_process_manager.table[i].status == PM_ENTRY_ACTIVE) {
            process_t* proc = g_process_manager.table[i].process;
            if (proc) {
                debug_print("Process Manager: Terminating process PID %u\n", proc->pid);
                process_exit(proc, -1);
                g_process_manager.table[i].status = PM_ENTRY_FREE;
            }
        }
    }
    
    /* Clean up IPC channels */
    for (uint32_t i = 0; i < PM_MAX_IPC_CHANNELS; i++) {
        if (g_process_manager.ipc_channels[i].is_active) {
            pm_ipc_destroy_channel(g_process_manager.ipc_channels[i].channel_id);
        }
    }
    
    g_process_manager.state = PM_STATE_UNINITIALIZED;
    g_pm_initialized = false;
    
    PM_UNLOCK();
    
    debug_print("Process Manager: Shutdown complete\n");
    return PM_SUCCESS;
}

/**
 * Get Process Manager state
 */
pm_state_t pm_get_state(void) {
    return g_process_manager.state;
}

/* ================================
 * Process Creation and Management
 * ================================ */

/**
 * Create a new process with specified parameters
 */
int pm_create_process(const pm_create_params_t* params, uint32_t* pid_out) {
    if (!g_pm_initialized || !params || !pid_out) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    if (g_process_manager.state != PM_STATE_RUNNING) {
        return PM_ERROR_INVALID_STATE;
    }
    
    /* Validate creation parameters */
    if (pm_validate_create_params(params) != PM_SUCCESS) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    PM_LOCK();
    
    /* Find free entry in process table */
    pm_process_entry_t* entry = pm_find_free_entry();
    if (!entry) {
        PM_UNLOCK();
        debug_print("Process Manager: Process table full\n");
        return PM_ERROR_TABLE_FULL;
    }
    
    /* Allocate PID */
    uint32_t pid = pm_table_allocate_pid();
    if (pid == 0) {
        PM_UNLOCK();
        return PM_ERROR_RESOURCE_LIMIT;
    }
    
    /* Create process using base process system */
    process_t* process = process_create(params->name, ""); /* Path will be set later */
    if (!process) {
        pm_table_free_pid(pid);
        PM_UNLOCK();
        debug_print("Process Manager: Failed to create process\n");
        return PM_ERROR_NO_MEMORY;
    }
    
    /* Set process properties */
    process->pid = pid;
    process->priority = params->priority;
    strncpy(process->name, params->name, MAX_PROCESS_NAME - 1);
    process->name[MAX_PROCESS_NAME - 1] = '\0';
    
    /* Update process table entry */
    entry->status = PM_ENTRY_ALLOCATED;
    entry->process = process;
    entry->creation_time = pm_get_timestamp();
    entry->last_activity = entry->creation_time;
    entry->active_channels = 0;
    
    /* Add to hash table */
    uint32_t entry_index = entry - g_process_manager.table;
    pm_hash_insert(pid, entry_index);
    
    /* Update statistics */
    g_process_manager.stats.total_created++;
    g_process_manager.stats.current_active++;
    if (g_process_manager.stats.current_active > g_process_manager.stats.peak_active) {
        g_process_manager.stats.peak_active = g_process_manager.stats.current_active;
    }
    
    entry->status = PM_ENTRY_ACTIVE;
    *pid_out = pid;
    
    PM_UNLOCK();
    
    debug_print("Process Manager: Created process '%s' with PID %u\n", params->name, pid);
    return PM_SUCCESS;
}

/**
 * Create process from ELF data
 */
int pm_create_process_from_elf(const char* name, void* elf_data, size_t elf_size, uint32_t* pid_out) {
    if (!g_pm_initialized || !name || !elf_data || elf_size == 0 || !pid_out) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    PM_LOCK();
    
    /* Find free entry in process table */
    pm_process_entry_t* entry = pm_find_free_entry();
    if (!entry) {
        PM_UNLOCK();
        return PM_ERROR_TABLE_FULL;
    }
    
    /* Allocate PID */
    uint32_t pid = pm_table_allocate_pid();
    if (pid == 0) {
        PM_UNLOCK();
        return PM_ERROR_RESOURCE_LIMIT;
    }
    
    /* Create process from ELF */
    process_t* process = process_create_from_elf(name, elf_data, elf_size);
    if (!process) {
        pm_table_free_pid(pid);
        PM_UNLOCK();
        debug_print("Process Manager: Failed to create process from ELF\n");
        return PM_ERROR_NO_MEMORY;
    }
    
    /* Override PID */
    process->pid = pid;
    
    /* Update process table entry */
    entry->status = PM_ENTRY_ACTIVE;
    entry->process = process;
    entry->creation_time = pm_get_timestamp();
    entry->last_activity = entry->creation_time;
    entry->active_channels = 0;
    
    /* Add to hash table */
    uint32_t entry_index = entry - g_process_manager.table;
    pm_hash_insert(pid, entry_index);
    
    /* Update statistics */
    g_process_manager.stats.total_created++;
    g_process_manager.stats.current_active++;
    if (g_process_manager.stats.current_active > g_process_manager.stats.peak_active) {
        g_process_manager.stats.peak_active = g_process_manager.stats.current_active;
    }
    
    *pid_out = pid;
    
    PM_UNLOCK();
    
    debug_print("Process Manager: Created process '%s' from ELF with PID %u\n", name, pid);
    return PM_SUCCESS;
}

/**
 * Terminate a process
 */
int pm_terminate_process(uint32_t pid, int exit_code) {
    if (!g_pm_initialized || pid == 0) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    PM_LOCK();
    
    /* Find process in table */
    uint32_t entry_index = pm_hash_lookup(pid);
    if (entry_index == UINT32_MAX) {
        PM_UNLOCK();
        return PM_ERROR_NOT_FOUND;
    }
    
    pm_process_entry_t* entry = &g_process_manager.table[entry_index];
    if (entry->status != PM_ENTRY_ACTIVE) {
        PM_UNLOCK();
        return PM_ERROR_INVALID_STATE;
    }
    
    process_t* process = entry->process;
    if (!process) {
        PM_UNLOCK();
        return PM_ERROR_INVALID_STATE;
    }
    
    debug_print("Process Manager: Terminating process PID %u\n", pid);
    
    /* Mark as terminating */
    entry->status = PM_ENTRY_TERMINATING;
    
    /* Close all IPC channels */
    for (uint32_t i = 0; i < PM_MAX_IPC_CHANNELS; i++) {
        if (entry->ipc_channels[i]) {
            pm_ipc_destroy_channel(entry->ipc_channels[i]->channel_id);
            entry->ipc_channels[i] = NULL;
        }
    }
    entry->active_channels = 0;
    
    /* Terminate the process */
    process_exit(process, exit_code);
    
    /* Update entry status */
    entry->status = PM_ENTRY_ZOMBIE;
    
    /* Update statistics */
    g_process_manager.stats.total_terminated++;
    g_process_manager.stats.current_active--;
    g_process_manager.stats.current_zombie++;
    
    PM_UNLOCK();
    
    return PM_SUCCESS;
}

/**
 * Get process by PID
 */
process_t* pm_get_process(uint32_t pid) {
    if (!g_pm_initialized || pid == 0) {
        return NULL;
    }
    
    PM_LOCK();
    
    uint32_t entry_index = pm_hash_lookup(pid);
    if (entry_index == UINT32_MAX) {
        PM_UNLOCK();
        return NULL;
    }
    
    pm_process_entry_t* entry = &g_process_manager.table[entry_index];
    process_t* process = (entry->status == PM_ENTRY_ACTIVE) ? entry->process : NULL;
    
    PM_UNLOCK();
    
    return process;
}

/* ================================
 * Process Table Management
 * ================================ */

/**
 * Allocate a new PID
 */
uint32_t pm_table_allocate_pid(void) {
    uint32_t pid = g_process_manager.next_pid;
    
    /* Find next available PID */
    for (uint32_t attempts = 0; attempts < PM_MAX_PROCESSES; attempts++) {
        if (pm_hash_lookup(pid) == UINT32_MAX) {
            g_process_manager.next_pid = (pid % (PM_MAX_PROCESSES - 1)) + 1;
            return pid;
        }
        pid = (pid % (PM_MAX_PROCESSES - 1)) + 1;
    }
    
    return 0; /* No available PIDs */
}

/**
 * Free a PID
 */
void pm_table_free_pid(uint32_t pid) {
    pm_hash_remove(pid);
}

/**
 * Calculate hash for PID
 */
uint32_t pm_table_hash_pid(uint32_t pid) {
    return pid % PM_PROCESS_HASH_SIZE;
}

/* ================================
 * IPC Implementation
 * ================================ */

/**
 * Create IPC channel
 */
int pm_ipc_create_channel(uint32_t owner_pid, uint32_t* channel_id) {
    if (!g_pm_initialized || !channel_id) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    PM_LOCK();
    
    /* Find free IPC channel */
    pm_ipc_channel_t* channel = NULL;
    uint32_t id = 0;
    
    for (uint32_t i = 0; i < PM_MAX_IPC_CHANNELS; i++) {
        if (!g_process_manager.ipc_channels[i].is_active) {
            channel = &g_process_manager.ipc_channels[i];
            id = g_process_manager.next_channel_id++;
            break;
        }
    }
    
    if (!channel) {
        PM_UNLOCK();
        return PM_ERROR_RESOURCE_LIMIT;
    }
    
    /* Initialize channel */
    channel->channel_id = id;
    channel->owner_pid = owner_pid;
    channel->permissions = 0;
    channel->is_active = true;
    channel->queue = NULL;
    channel->queue_size = 0;
    channel->queue_head = 0;
    channel->queue_tail = 0;
    
    *channel_id = id;
    
    PM_UNLOCK();
    
    debug_print("Process Manager: Created IPC channel %u for PID %u\n", id, owner_pid);
    return PM_SUCCESS;
}

/**
 * Send IPC message
 */
int pm_ipc_send_message(const pm_ipc_message_t* message) {
    if (!g_pm_initialized || !message) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    /* Validate message */
    if (message->data_size > PM_IPC_BUFFER_SIZE) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    debug_print("Process Manager: IPC message from PID %u to PID %u\n", 
                message->src_pid, message->dst_pid);
    
    /* For now, just log the message - full IPC queue implementation would go here */
    g_process_manager.stats.ipc_messages++;
    
    return PM_SUCCESS;
}

/* ================================
 * Helper Functions
 * ================================ */

/**
 * Find free entry in process table
 */
static pm_process_entry_t* pm_find_free_entry(void) {
    for (uint32_t i = 0; i < PM_MAX_PROCESSES; i++) {
        if (g_process_manager.table[i].status == PM_ENTRY_FREE) {
            return &g_process_manager.table[i];
        }
    }
    return NULL;
}

/**
 * Insert PID into hash table
 */
static void pm_hash_insert(uint32_t pid, uint32_t entry_index) {
    uint32_t hash = pm_table_hash_pid(pid);
    g_process_manager.table[entry_index].hash_next = g_process_manager.hash_table[hash];
    g_process_manager.hash_table[hash] = entry_index;
}

/**
 * Remove PID from hash table
 */
static void pm_hash_remove(uint32_t pid) {
    uint32_t hash = pm_table_hash_pid(pid);
    uint32_t* current = &g_process_manager.hash_table[hash];
    
    while (*current != UINT32_MAX) {
        pm_process_entry_t* entry = &g_process_manager.table[*current];
        if (entry->process && entry->process->pid == pid) {
            *current = entry->hash_next;
            entry->hash_next = UINT32_MAX;
            return;
        }
        current = &entry->hash_next;
    }
}

/**
 * Lookup PID in hash table
 */
static uint32_t pm_hash_lookup(uint32_t pid) {
    uint32_t hash = pm_table_hash_pid(pid);
    uint32_t current = g_process_manager.hash_table[hash];
    
    while (current != UINT32_MAX) {
        pm_process_entry_t* entry = &g_process_manager.table[current];
        if (entry->process && entry->process->pid == pid) {
            return current;
        }
        current = entry->hash_next;
    }
    
    return UINT32_MAX;
}

/**
 * Validate process creation parameters
 */
static int pm_validate_create_params(const pm_create_params_t* params) {
    if (!params->name[0]) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    if (params->argc > PM_MAX_PROCESS_ARGS || params->envc > PM_MAX_PROCESS_ARGS) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    return PM_SUCCESS;
}

/**
 * Get current timestamp (simplified version)
 */
static uint64_t pm_get_timestamp(void) {
    /* In a real system, this would get the actual system time */
    static uint64_t counter = 0;
    return ++counter;
}

/**
 * Destroy IPC channel
 */
int pm_ipc_destroy_channel(uint32_t channel_id) {
    if (!g_pm_initialized || channel_id == 0) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    PM_LOCK();
    
    /* Find and deactivate channel */
    for (uint32_t i = 0; i < PM_MAX_IPC_CHANNELS; i++) {
        if (g_process_manager.ipc_channels[i].channel_id == channel_id &&
            g_process_manager.ipc_channels[i].is_active) {
            g_process_manager.ipc_channels[i].is_active = false;
            g_process_manager.ipc_channels[i].channel_id = 0;
            PM_UNLOCK();
            return PM_SUCCESS;
        }
    }
    
    PM_UNLOCK();
    return PM_ERROR_NOT_FOUND;
}

/**
 * Receive IPC message
 */
int pm_ipc_receive_message(uint32_t pid, uint32_t channel_id, pm_ipc_message_t* message_out) {
    if (!g_pm_initialized || pid == 0 || channel_id == 0 || !message_out) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    /* For now, just return that no message is available */
    debug_print("Process Manager: IPC receive by PID %u on channel %u\n", pid, channel_id);
    return PM_ERROR_NOT_FOUND;
}

/**
 * Broadcast IPC message
 */
int pm_ipc_broadcast_message(const pm_ipc_message_t* message) {
    if (!g_pm_initialized || !message) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    debug_print("Process Manager: Broadcasting IPC message from PID %u\n", message->src_pid);
    g_process_manager.stats.ipc_messages++;
    return PM_SUCCESS;
}

/**
 * Get process list
 */
int pm_get_process_list(uint32_t* pids, uint32_t max_count, uint32_t* count_out) {
    if (!g_pm_initialized || !pids || !count_out) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    PM_LOCK();
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < PM_MAX_PROCESSES && count < max_count; i++) {
        if (g_process_manager.table[i].status == PM_ENTRY_ACTIVE && 
            g_process_manager.table[i].process) {
            pids[count++] = g_process_manager.table[i].process->pid;
        }
    }
    
    *count_out = count;
    
    PM_UNLOCK();
    return PM_SUCCESS;
}

/**
 * Kill process
 */
int pm_kill_process(uint32_t pid, int signal) {
    if (!g_pm_initialized || pid == 0) {
        return PM_ERROR_INVALID_PARAM;
    }
    
    debug_print("Process Manager: Killing process PID %u with signal %d\n", pid, signal);
    return pm_terminate_process(pid, signal);
}

/**
 * Check if PID is valid
 */
bool pm_table_is_pid_valid(uint32_t pid) {
    if (!g_pm_initialized || pid == 0) {
        return false;
    }
    
    return pm_hash_lookup(pid) != UINT32_MAX;
}

/**
 * Get Process Manager statistics
 */
void pm_get_statistics(pm_statistics_t* stats) {
    if (!g_pm_initialized || !stats) {
        return;
    }
    
    PM_LOCK();
    *stats = g_process_manager.stats;
    PM_UNLOCK();
}
void pm_dump_process_table(void) {
    if (!g_pm_initialized) {
        debug_print("Process Manager: Not initialized\n");
        return;
    }
    
    debug_print("Process Manager: Process Table Dump\n");
    debug_print("==================================\n");
    
    PM_LOCK();
    
    for (uint32_t i = 0; i < PM_MAX_PROCESSES; i++) {
        pm_process_entry_t* entry = &g_process_manager.table[i];
        if (entry->status != PM_ENTRY_FREE) {
            const char* status_str = "UNKNOWN";
            switch (entry->status) {
                case PM_ENTRY_ALLOCATED: status_str = "ALLOCATED"; break;
                case PM_ENTRY_ACTIVE: status_str = "ACTIVE"; break;
                case PM_ENTRY_ZOMBIE: status_str = "ZOMBIE"; break;
                case PM_ENTRY_TERMINATING: status_str = "TERMINATING"; break;
                default: break;
            }
            
            debug_print("Entry %u: Status=%s, PID=%u, Name='%s'\n", 
                       i, status_str, 
                       entry->process ? entry->process->pid : 0,
                       entry->process ? entry->process->name : "NULL");
        }
    }
    
    debug_print("Statistics: Active=%u, Zombie=%u, Total Created=%u\n",
               g_process_manager.stats.current_active,
               g_process_manager.stats.current_zombie,
               g_process_manager.stats.total_created);
    
    PM_UNLOCK();
}

/**
 * Simple debug print function
 */
static void debug_print(const char* format, ...) {
    /* In a real kernel, this would output to the console/log */
    /* For now, it's a placeholder */
    (void)format;
}
