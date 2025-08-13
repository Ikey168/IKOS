/* IKOS IPC System Calls Header
 * Defines system call interface for IPC operations
 */

#ifndef IPC_SYSCALLS_H
#define IPC_SYSCALLS_H

#include "ipc.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* System call function prototypes */
uint32_t sys_ipc_create_queue(uint32_t max_messages, uint32_t permissions);
int sys_ipc_destroy_queue(uint32_t queue_id);
int sys_ipc_send_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags);
int sys_ipc_receive_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags);

uint32_t sys_ipc_create_channel(const char* name, bool is_broadcast, bool is_persistent);
int sys_ipc_subscribe_channel(uint32_t channel_id, uint32_t pid);
int sys_ipc_send_to_channel(uint32_t channel_id, ipc_message_t* message, uint32_t flags);

int sys_ipc_send_request(uint32_t target_pid, ipc_message_t* request, 
                         ipc_message_t* reply, uint32_t timeout_ms);
int sys_ipc_send_reply(uint32_t target_pid, ipc_message_t* reply);
int sys_ipc_send_async(uint32_t target_pid, ipc_message_t* message);
int sys_ipc_broadcast(ipc_message_t* message, uint32_t* target_pids, uint32_t count);

/* User space validation functions */
bool is_valid_user_pointer(const void* ptr, size_t size);
bool is_valid_user_string(const char* str, size_t max_len);
int copy_from_user(void* dest, const void* src, size_t size);
int copy_to_user(void* dest, const void* src, size_t size);
int copy_string_from_user(char* dest, const char* src, size_t max_len);

/* System call handler */
typedef struct syscall_params syscall_params_t;
uint64_t ipc_syscall_handler(uint32_t syscall_num, syscall_params_t* params);

#endif /* IPC_SYSCALLS_H */
