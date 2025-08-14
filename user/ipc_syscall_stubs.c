/* IKOS User-Space IPC System Call Stubs
 * Placeholder implementations for system calls when building user-space code
 */
#include "ipc_syscalls.h"
#include <stdio.h>

/* Stub implementations - these would be replaced with actual system calls */

uint32_t sys_ipc_create_queue(uint32_t max_messages, uint32_t permissions) {
    printf("[STUB] sys_ipc_create_queue(max=%u, perm=%u)\n", max_messages, permissions);
    return 1; /* Return fake queue ID */
}

int sys_ipc_destroy_queue(uint32_t queue_id) {
    printf("[STUB] sys_ipc_destroy_queue(queue=%u)\n", queue_id);
    return 0; /* Success */
}

int sys_ipc_send_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags) {
    printf("[STUB] sys_ipc_send_message(queue=%u, flags=%u)\n", queue_id, flags);
    return 0; /* Success */
}

int sys_ipc_receive_message(uint32_t queue_id, ipc_message_t* message, uint32_t flags) {
    printf("[STUB] sys_ipc_receive_message(queue=%u, flags=%u)\n", queue_id, flags);
    return -4; /* Queue empty for testing */
}

uint32_t sys_ipc_create_channel(const char* name, bool is_broadcast, bool is_persistent) {
    printf("[STUB] sys_ipc_create_channel(name=%s, broadcast=%d, persistent=%d)\n", 
           name, is_broadcast, is_persistent);
    return 1; /* Return fake channel ID */
}

int sys_ipc_subscribe_channel(uint32_t channel_id, uint32_t pid) {
    printf("[STUB] sys_ipc_subscribe_channel(channel=%u, pid=%u)\n", channel_id, pid);
    return 0; /* Success */
}

int sys_ipc_unsubscribe_channel(uint32_t channel_id, uint32_t pid) {
    printf("[STUB] sys_ipc_unsubscribe_channel(channel=%u, pid=%u)\n", channel_id, pid);
    return 0; /* Success */
}

int sys_ipc_send_to_channel(uint32_t channel_id, ipc_message_t* message, uint32_t flags) {
    printf("[STUB] sys_ipc_send_to_channel(channel=%u, flags=%u)\n", channel_id, flags);
    return 0; /* Success */
}

int sys_ipc_send_request(uint32_t target_pid, ipc_message_t* request, 
                         ipc_message_t* reply, uint32_t timeout_ms) {
    printf("[STUB] sys_ipc_send_request(pid=%u, timeout=%u)\n", target_pid, timeout_ms);
    return -6; /* Timeout for testing */
}

int sys_ipc_send_reply(uint32_t target_pid, ipc_message_t* reply) {
    printf("[STUB] sys_ipc_send_reply(pid=%u)\n", target_pid);
    return 0; /* Success */
}

int sys_ipc_send_async(uint32_t target_pid, ipc_message_t* message) {
    printf("[STUB] sys_ipc_send_async(pid=%u)\n", target_pid);
    return 0; /* Success */
}

int sys_ipc_broadcast(ipc_message_t* message, uint32_t* target_pids, uint32_t count) {
    printf("[STUB] sys_ipc_broadcast(count=%u)\n", count);
    return 0; /* Success */
}
