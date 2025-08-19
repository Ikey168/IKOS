/*
 * IKOS Socket System Calls - Header Definitions
 * Issue #46 - Sockets API for User Applications
 * 
 * Provides Berkeley-style socket system call interface for user-space
 * applications to perform network communication.
 */

#ifndef SOCKET_SYSCALLS_H
#define SOCKET_SYSCALLS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "net/socket.h"
#include "syscalls.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================
 * Socket System Call Prototypes
 * ================================ */

/* Basic Socket Operations */
long sys_socket(int domain, int type, int protocol);
long sys_bind(int sockfd, const void* addr, uint32_t addrlen);
long sys_listen(int sockfd, int backlog);
long sys_accept(int sockfd, void* addr, uint32_t* addrlen);
long sys_connect(int sockfd, const void* addr, uint32_t addrlen);

/* Data Transfer */
long sys_send(int sockfd, const void* buf, size_t len, int flags);
long sys_recv(int sockfd, void* buf, size_t len, int flags);
long sys_sendto(int sockfd, const void* buf, size_t len, int flags,
                const void* dest_addr, uint32_t addrlen);
long sys_recvfrom(int sockfd, void* buf, size_t len, int flags,
                  void* src_addr, uint32_t* addrlen);

/* Socket Control */
long sys_shutdown(int sockfd, int how);
long sys_setsockopt(int sockfd, int level, int optname, 
                    const void* optval, uint32_t optlen);
long sys_getsockopt(int sockfd, int level, int optname, 
                    void* optval, uint32_t* optlen);

/* Socket Information */
long sys_getsockname(int sockfd, void* addr, uint32_t* addrlen);
long sys_getpeername(int sockfd, void* addr, uint32_t* addrlen);

/* ================================
 * Socket Management Functions
 * ================================ */

/* System initialization */
int socket_syscall_init(void);
void socket_syscall_cleanup(void);

/* Socket table management */
int socket_table_init(void);
void socket_table_cleanup(void);
int socket_fd_alloc(socket_t* sock);
void socket_fd_free(int fd);
socket_t* socket_fd_to_socket(int fd);
int socket_to_fd(socket_t* sock);

/* Validation functions */
bool is_valid_socket_fd(int fd);
bool is_valid_socket_addr(const void* addr, uint32_t addrlen);
bool is_valid_socket_domain(int domain);
bool is_valid_socket_type(int type);
bool is_valid_socket_protocol(int protocol);

/* Memory management for socket operations */
int copy_sockaddr_from_user(sockaddr_in_t* dest, const void* src, uint32_t addrlen);
int copy_sockaddr_to_user(void* dest, const sockaddr_in_t* src, uint32_t* addrlen);
int validate_user_buffer(const void* buf, size_t len, bool write_access);

/* ================================
 * Socket Descriptor Table
 * ================================ */

#define SOCKET_FD_MAX           1024    /* Maximum socket file descriptors */
#define SOCKET_FD_OFFSET        1000    /* Offset to distinguish from regular FDs */

typedef struct socket_fd_entry {
    socket_t* socket;           /* Pointer to socket structure */
    bool allocated;             /* Entry is allocated */
    uint32_t ref_count;         /* Reference count */
    uint32_t flags;             /* File descriptor flags */
} socket_fd_entry_t;

typedef struct socket_fd_table {
    socket_fd_entry_t entries[SOCKET_FD_MAX];
    uint32_t next_fd;           /* Next available file descriptor */
    uint32_t allocated_count;   /* Number of allocated descriptors */
    bool initialized;           /* Table is initialized */
} socket_fd_table_t;

/* Global socket descriptor table */
extern socket_fd_table_t g_socket_fd_table;

/* ================================
 * Socket Error Handling
 * ================================ */

/* Set last socket error for current process */
void socket_set_errno(int error);

/* Get last socket error for current process */
int socket_get_errno(void);

/* Convert network error to socket error */
int network_error_to_socket_error(int net_error);

/* Socket error strings */
const char* socket_error_string(int error);

/* ================================
 * Socket Statistics
 * ================================ */

typedef struct socket_stats {
    uint64_t sockets_created;       /* Total sockets created */
    uint64_t sockets_destroyed;     /* Total sockets destroyed */
    uint64_t sockets_active;        /* Currently active sockets */
    uint64_t tcp_connections;       /* Active TCP connections */
    uint64_t udp_sockets;           /* Active UDP sockets */
    uint64_t bytes_sent;            /* Total bytes sent */
    uint64_t bytes_received;        /* Total bytes received */
    uint64_t packets_sent;          /* Total packets sent */
    uint64_t packets_received;      /* Total packets received */
    uint64_t errors;                /* Total socket errors */
} socket_stats_t;

/* Get socket statistics */
int socket_get_stats(socket_stats_t* stats);

/* Print socket statistics */
void socket_print_stats(void);

/* ================================
 * Socket Configuration
 * ================================ */

/* Socket buffer sizes */
#define SOCKET_DEFAULT_RCVBUF   8192    /* Default receive buffer size */
#define SOCKET_DEFAULT_SNDBUF   8192    /* Default send buffer size */
#define SOCKET_MAX_RCVBUF       65536   /* Maximum receive buffer size */
#define SOCKET_MAX_SNDBUF       65536   /* Maximum send buffer size */

/* Socket timeouts */
#define SOCKET_DEFAULT_TIMEOUT  30000   /* Default timeout in milliseconds */
#define SOCKET_MAX_TIMEOUT      300000  /* Maximum timeout in milliseconds */

/* Connection limits */
#define SOCKET_MAX_BACKLOG      128     /* Maximum listen backlog */
#define SOCKET_DEFAULT_BACKLOG  5       /* Default listen backlog */

/* ================================
 * Non-blocking Socket Support
 * ================================ */

/* Socket flags */
#define SOCKET_FLAG_NONBLOCK    0x01    /* Non-blocking mode */
#define SOCKET_FLAG_KEEPALIVE   0x02    /* Keep connection alive */
#define SOCKET_FLAG_REUSEADDR   0x04    /* Reuse address */
#define SOCKET_FLAG_BROADCAST   0x08    /* Allow broadcast */

/* Non-blocking operation support */
int socket_set_nonblocking(socket_t* sock, bool nonblock);
bool socket_is_nonblocking(socket_t* sock);
int socket_would_block(socket_t* sock, int operation);

/* Operation types for would_block check */
#define SOCKET_OP_READ          1
#define SOCKET_OP_WRITE         2
#define SOCKET_OP_ACCEPT        3
#define SOCKET_OP_CONNECT       4

/* ================================
 * Socket Event Notification
 * ================================ */

/* Socket events */
#define SOCKET_EVENT_READ       0x01    /* Data available for reading */
#define SOCKET_EVENT_WRITE      0x02    /* Space available for writing */
#define SOCKET_EVENT_ERROR      0x04    /* Error condition */
#define SOCKET_EVENT_HANGUP     0x08    /* Connection closed by peer */

/* Event notification callback */
typedef void (*socket_event_callback_t)(socket_t* sock, uint32_t events, void* user_data);

/* Register event callback */
int socket_register_event_callback(socket_t* sock, socket_event_callback_t callback, void* user_data);

/* Unregister event callback */
int socket_unregister_event_callback(socket_t* sock);

/* Trigger socket events */
void socket_trigger_event(socket_t* sock, uint32_t events);

#ifdef __cplusplus
}
#endif

#endif /* SOCKET_SYSCALLS_H */
