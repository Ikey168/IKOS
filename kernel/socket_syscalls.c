/*
 * IKOS Socket System Calls - Implementation
 * Issue #46 - Sockets API for User Applications
 * 
 * Provides Berkeley-style socket system call implementation for kernel-space
 * with integration to the existing network stack.
 */

#include "../include/socket_syscalls.h"
#include "../include/net/network.h"
#include "../include/net/tcp.h"
#include "../include/net/udp.h"
#include "../include/process.h"
#include "../include/memory.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ================================
 * Global Socket State
 * ================================ */

socket_fd_table_t g_socket_fd_table;
static socket_stats_t g_socket_stats;
static bool socket_subsystem_initialized = false;

/* ================================
 * Socket Subsystem Initialization
 * ================================ */

int socket_syscall_init(void) {
    if (socket_subsystem_initialized) {
        return SOCKET_SUCCESS;
    }
    
    /* Initialize socket descriptor table */
    if (socket_table_init() != SOCKET_SUCCESS) {
        printf("Failed to initialize socket descriptor table\n");
        return SOCKET_ERROR;
    }
    
    /* Initialize socket statistics */
    memset(&g_socket_stats, 0, sizeof(socket_stats_t));
    
    /* Register socket system calls */
    register_syscall_handler(SYS_SOCKET, (syscall_handler_t)sys_socket);
    register_syscall_handler(SYS_BIND, (syscall_handler_t)sys_bind);
    register_syscall_handler(SYS_LISTEN, (syscall_handler_t)sys_listen);
    register_syscall_handler(SYS_ACCEPT, (syscall_handler_t)sys_accept);
    register_syscall_handler(SYS_CONNECT, (syscall_handler_t)sys_connect);
    register_syscall_handler(SYS_SEND, (syscall_handler_t)sys_send);
    register_syscall_handler(SYS_RECV, (syscall_handler_t)sys_recv);
    register_syscall_handler(SYS_SENDTO, (syscall_handler_t)sys_sendto);
    register_syscall_handler(SYS_RECVFROM, (syscall_handler_t)sys_recvfrom);
    register_syscall_handler(SYS_SHUTDOWN, (syscall_handler_t)sys_shutdown);
    register_syscall_handler(SYS_SETSOCKOPT, (syscall_handler_t)sys_setsockopt);
    register_syscall_handler(SYS_GETSOCKOPT, (syscall_handler_t)sys_getsockopt);
    register_syscall_handler(SYS_GETSOCKNAME, (syscall_handler_t)sys_getsockname);
    register_syscall_handler(SYS_GETPEERNAME, (syscall_handler_t)sys_getpeername);
    
    socket_subsystem_initialized = true;
    printf("Socket subsystem initialized successfully\n");
    return SOCKET_SUCCESS;
}

void socket_syscall_cleanup(void) {
    if (!socket_subsystem_initialized) {
        return;
    }
    
    /* Unregister system calls */
    unregister_syscall_handler(SYS_SOCKET);
    unregister_syscall_handler(SYS_BIND);
    unregister_syscall_handler(SYS_LISTEN);
    unregister_syscall_handler(SYS_ACCEPT);
    unregister_syscall_handler(SYS_CONNECT);
    unregister_syscall_handler(SYS_SEND);
    unregister_syscall_handler(SYS_RECV);
    unregister_syscall_handler(SYS_SENDTO);
    unregister_syscall_handler(SYS_RECVFROM);
    unregister_syscall_handler(SYS_SHUTDOWN);
    unregister_syscall_handler(SYS_SETSOCKOPT);
    unregister_syscall_handler(SYS_GETSOCKOPT);
    unregister_syscall_handler(SYS_GETSOCKNAME);
    unregister_syscall_handler(SYS_GETPEERNAME);
    
    /* Cleanup socket descriptor table */
    socket_table_cleanup();
    
    socket_subsystem_initialized = false;
    printf("Socket subsystem cleaned up\n");
}

/* ================================
 * Socket Descriptor Table Management
 * ================================ */

int socket_table_init(void) {
    memset(&g_socket_fd_table, 0, sizeof(socket_fd_table_t));
    g_socket_fd_table.next_fd = SOCKET_FD_OFFSET;
    g_socket_fd_table.initialized = true;
    return SOCKET_SUCCESS;
}

void socket_table_cleanup(void) {
    /* Close all open sockets */
    for (uint32_t i = 0; i < SOCKET_FD_MAX; i++) {
        if (g_socket_fd_table.entries[i].allocated) {
            socket_fd_free(i + SOCKET_FD_OFFSET);
        }
    }
    
    memset(&g_socket_fd_table, 0, sizeof(socket_fd_table_t));
}

int socket_fd_alloc(socket_t* sock) {
    if (!sock || !g_socket_fd_table.initialized) {
        return SOCKET_EBADF;
    }
    
    /* Find available slot */
    for (uint32_t i = 0; i < SOCKET_FD_MAX; i++) {
        uint32_t idx = (g_socket_fd_table.next_fd - SOCKET_FD_OFFSET + i) % SOCKET_FD_MAX;
        
        if (!g_socket_fd_table.entries[idx].allocated) {
            /* Allocate this slot */
            g_socket_fd_table.entries[idx].socket = sock;
            g_socket_fd_table.entries[idx].allocated = true;
            g_socket_fd_table.entries[idx].ref_count = 1;
            g_socket_fd_table.entries[idx].flags = 0;
            
            g_socket_fd_table.allocated_count++;
            g_socket_fd_table.next_fd = idx + 1 + SOCKET_FD_OFFSET;
            
            return idx + SOCKET_FD_OFFSET;
        }
    }
    
    return SOCKET_ENOBUFS; /* No available slots */
}

void socket_fd_free(int fd) {
    if (fd < SOCKET_FD_OFFSET || fd >= SOCKET_FD_OFFSET + SOCKET_FD_MAX) {
        return;
    }
    
    uint32_t idx = fd - SOCKET_FD_OFFSET;
    if (g_socket_fd_table.entries[idx].allocated) {
        /* Close the socket if this is the last reference */
        g_socket_fd_table.entries[idx].ref_count--;
        if (g_socket_fd_table.entries[idx].ref_count == 0) {
            socket_t* sock = g_socket_fd_table.entries[idx].socket;
            if (sock && sock->ops && sock->ops->close) {
                sock->ops->close(sock);
            }
            
            /* Free the socket structure */
            socket_free(sock);
        }
        
        /* Free the descriptor */
        memset(&g_socket_fd_table.entries[idx], 0, sizeof(socket_fd_entry_t));
        g_socket_fd_table.allocated_count--;
    }
}

socket_t* socket_fd_to_socket(int fd) {
    if (fd < SOCKET_FD_OFFSET || fd >= SOCKET_FD_OFFSET + SOCKET_FD_MAX) {
        return NULL;
    }
    
    uint32_t idx = fd - SOCKET_FD_OFFSET;
    if (g_socket_fd_table.entries[idx].allocated) {
        return g_socket_fd_table.entries[idx].socket;
    }
    
    return NULL;
}

int socket_to_fd(socket_t* sock) {
    if (!sock) {
        return SOCKET_EBADF;
    }
    
    /* Search for socket in table */
    for (uint32_t i = 0; i < SOCKET_FD_MAX; i++) {
        if (g_socket_fd_table.entries[i].allocated && 
            g_socket_fd_table.entries[i].socket == sock) {
            return i + SOCKET_FD_OFFSET;
        }
    }
    
    return SOCKET_EBADF;
}

/* ================================
 * Socket System Call Implementations
 * ================================ */

long sys_socket(int domain, int type, int protocol) {
    /* Validate parameters */
    if (!is_valid_socket_domain(domain)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    if (!is_valid_socket_type(type)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Create socket structure */
    socket_t* sock = socket_alloc(domain, type, protocol);
    if (!sock) {
        socket_set_errno(SOCKET_ENOBUFS);
        return SOCKET_ENOBUFS;
    }
    
    /* Allocate file descriptor */
    int fd = socket_fd_alloc(sock);
    if (fd < 0) {
        socket_free(sock);
        socket_set_errno(fd);
        return fd;
    }
    
    /* Update statistics */
    g_socket_stats.sockets_created++;
    g_socket_stats.sockets_active++;
    
    if (type == SOCK_STREAM) {
        g_socket_stats.tcp_connections++;
    } else if (type == SOCK_DGRAM) {
        g_socket_stats.udp_sockets++;
    }
    
    printf("Created socket: domain=%d, type=%d, protocol=%d, fd=%d\n", 
           domain, type, protocol, fd);
    
    return fd;
}

long sys_bind(int sockfd, const void* addr, uint32_t addrlen) {
    /* Validate socket file descriptor */
    socket_t* sock = socket_fd_to_socket(sockfd);
    if (!sock) {
        socket_set_errno(SOCKET_EBADF);
        return SOCKET_EBADF;
    }
    
    /* Validate address */
    if (!is_valid_socket_addr(addr, addrlen)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Copy address from user space */
    sockaddr_in_t bind_addr;
    int result = copy_sockaddr_from_user(&bind_addr, addr, addrlen);
    if (result != SOCKET_SUCCESS) {
        socket_set_errno(result);
        return result;
    }
    
    /* Perform bind operation */
    if (sock->ops && sock->ops->bind) {
        result = sock->ops->bind(sock, &bind_addr);
        if (result != SOCKET_SUCCESS) {
            socket_set_errno(network_error_to_socket_error(result));
            return network_error_to_socket_error(result);
        }
    } else {
        socket_set_errno(SOCKET_ENOTSOCK);
        return SOCKET_ENOTSOCK;
    }
    
    printf("Socket %d bound to address\n", sockfd);
    return SOCKET_SUCCESS;
}

long sys_listen(int sockfd, int backlog) {
    /* Validate socket file descriptor */
    socket_t* sock = socket_fd_to_socket(sockfd);
    if (!sock) {
        socket_set_errno(SOCKET_EBADF);
        return SOCKET_EBADF;
    }
    
    /* Validate backlog */
    if (backlog < 0) {
        backlog = SOCKET_DEFAULT_BACKLOG;
    } else if (backlog > SOCKET_MAX_BACKLOG) {
        backlog = SOCKET_MAX_BACKLOG;
    }
    
    /* Perform listen operation */
    if (sock->ops && sock->ops->listen) {
        int result = sock->ops->listen(sock, backlog);
        if (result != SOCKET_SUCCESS) {
            socket_set_errno(network_error_to_socket_error(result));
            return network_error_to_socket_error(result);
        }
    } else {
        socket_set_errno(SOCKET_ENOTSOCK);
        return SOCKET_ENOTSOCK;
    }
    
    printf("Socket %d listening with backlog %d\n", sockfd, backlog);
    return SOCKET_SUCCESS;
}

long sys_accept(int sockfd, void* addr, uint32_t* addrlen) {
    /* Validate socket file descriptor */
    socket_t* sock = socket_fd_to_socket(sockfd);
    if (!sock) {
        socket_set_errno(SOCKET_EBADF);
        return SOCKET_EBADF;
    }
    
    /* Validate address parameters */
    if (addr && !validate_user_buffer(addr, sizeof(sockaddr_in_t), true)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    if (addrlen && !validate_user_buffer(addrlen, sizeof(uint32_t), true)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Create new socket for accepted connection */
    socket_t* new_sock = socket_alloc(sock->domain, sock->type, sock->protocol);
    if (!new_sock) {
        socket_set_errno(SOCKET_ENOBUFS);
        return SOCKET_ENOBUFS;
    }
    
    /* Perform accept operation */
    if (sock->ops && sock->ops->accept) {
        int result = sock->ops->accept(sock, new_sock);
        if (result != SOCKET_SUCCESS) {
            socket_free(new_sock);
            if (result == SOCKET_EAGAIN && socket_is_nonblocking(sock)) {
                socket_set_errno(SOCKET_EAGAIN);
                return SOCKET_EAGAIN;
            }
            socket_set_errno(network_error_to_socket_error(result));
            return network_error_to_socket_error(result);
        }
    } else {
        socket_free(new_sock);
        socket_set_errno(SOCKET_ENOTSOCK);
        return SOCKET_ENOTSOCK;
    }
    
    /* Allocate file descriptor for new socket */
    int new_fd = socket_fd_alloc(new_sock);
    if (new_fd < 0) {
        socket_free(new_sock);
        socket_set_errno(new_fd);
        return new_fd;
    }
    
    /* Copy address information to user space */
    if (addr && addrlen) {
        copy_sockaddr_to_user(addr, &new_sock->remote_addr, addrlen);
    }
    
    /* Update statistics */
    g_socket_stats.sockets_created++;
    g_socket_stats.sockets_active++;
    
    printf("Socket %d accepted connection, new socket fd=%d\n", sockfd, new_fd);
    return new_fd;
}

long sys_connect(int sockfd, const void* addr, uint32_t addrlen) {
    /* Validate socket file descriptor */
    socket_t* sock = socket_fd_to_socket(sockfd);
    if (!sock) {
        socket_set_errno(SOCKET_EBADF);
        return SOCKET_EBADF;
    }
    
    /* Validate address */
    if (!is_valid_socket_addr(addr, addrlen)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Copy address from user space */
    sockaddr_in_t connect_addr;
    int result = copy_sockaddr_from_user(&connect_addr, addr, addrlen);
    if (result != SOCKET_SUCCESS) {
        socket_set_errno(result);
        return result;
    }
    
    /* Perform connect operation */
    if (sock->ops && sock->ops->connect) {
        result = sock->ops->connect(sock, &connect_addr);
        if (result != SOCKET_SUCCESS) {
            if (result == SOCKET_EINPROGRESS && socket_is_nonblocking(sock)) {
                socket_set_errno(SOCKET_EINPROGRESS);
                return SOCKET_EINPROGRESS;
            }
            socket_set_errno(network_error_to_socket_error(result));
            return network_error_to_socket_error(result);
        }
    } else {
        socket_set_errno(SOCKET_ENOTSOCK);
        return SOCKET_ENOTSOCK;
    }
    
    printf("Socket %d connected to remote address\n", sockfd);
    return SOCKET_SUCCESS;
}

long sys_send(int sockfd, const void* buf, size_t len, int flags) {
    /* Validate socket file descriptor */
    socket_t* sock = socket_fd_to_socket(sockfd);
    if (!sock) {
        socket_set_errno(SOCKET_EBADF);
        return SOCKET_EBADF;
    }
    
    /* Validate buffer */
    if (!validate_user_buffer(buf, len, false)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Perform send operation */
    int result = 0;
    if (sock->ops && sock->ops->send) {
        result = sock->ops->send(sock, buf, len, flags);
        if (result < 0) {
            if (result == SOCKET_EAGAIN && socket_is_nonblocking(sock)) {
                socket_set_errno(SOCKET_EAGAIN);
                return SOCKET_EAGAIN;
            }
            socket_set_errno(network_error_to_socket_error(result));
            return network_error_to_socket_error(result);
        }
    } else {
        socket_set_errno(SOCKET_ENOTSOCK);
        return SOCKET_ENOTSOCK;
    }
    
    /* Update statistics */
    g_socket_stats.bytes_sent += result;
    g_socket_stats.packets_sent++;
    
    return result;
}

long sys_recv(int sockfd, void* buf, size_t len, int flags) {
    /* Validate socket file descriptor */
    socket_t* sock = socket_fd_to_socket(sockfd);
    if (!sock) {
        socket_set_errno(SOCKET_EBADF);
        return SOCKET_EBADF;
    }
    
    /* Validate buffer */
    if (!validate_user_buffer(buf, len, true)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Perform receive operation */
    int result = 0;
    if (sock->ops && sock->ops->recv) {
        result = sock->ops->recv(sock, buf, len, flags);
        if (result < 0) {
            if (result == SOCKET_EAGAIN && socket_is_nonblocking(sock)) {
                socket_set_errno(SOCKET_EAGAIN);
                return SOCKET_EAGAIN;
            }
            socket_set_errno(network_error_to_socket_error(result));
            return network_error_to_socket_error(result);
        }
    } else {
        socket_set_errno(SOCKET_ENOTSOCK);
        return SOCKET_ENOTSOCK;
    }
    
    /* Update statistics */
    g_socket_stats.bytes_received += result;
    g_socket_stats.packets_received++;
    
    return result;
}

long sys_sendto(int sockfd, const void* buf, size_t len, int flags,
                const void* dest_addr, uint32_t addrlen) {
    /* Validate socket file descriptor */
    socket_t* sock = socket_fd_to_socket(sockfd);
    if (!sock) {
        socket_set_errno(SOCKET_EBADF);
        return SOCKET_EBADF;
    }
    
    /* Validate buffer */
    if (!validate_user_buffer(buf, len, false)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Validate destination address if provided */
    sockaddr_in_t dest;
    if (dest_addr && addrlen > 0) {
        if (!is_valid_socket_addr(dest_addr, addrlen)) {
            socket_set_errno(SOCKET_ERROR);
            return SOCKET_ERROR;
        }
        
        int result = copy_sockaddr_from_user(&dest, dest_addr, addrlen);
        if (result != SOCKET_SUCCESS) {
            socket_set_errno(result);
            return result;
        }
    }
    
    /* For UDP sockets, use sendto-specific function if available */
    int result = 0;
    if (sock->type == SOCK_DGRAM && dest_addr) {
        /* UDP sendto operation */
        if (sock->domain == AF_INET) {
            result = udp_send_packet((udp_socket_t*)sock->protocol_data, buf, len, 
                                   dest.sin_addr.s_addr, dest.sin_port);
        } else {
            socket_set_errno(SOCKET_ERROR);
            return SOCKET_ERROR;
        }
    } else {
        /* Use regular send operation */
        if (sock->ops && sock->ops->send) {
            result = sock->ops->send(sock, buf, len, flags);
        } else {
            socket_set_errno(SOCKET_ENOTSOCK);
            return SOCKET_ENOTSOCK;
        }
    }
    
    if (result < 0) {
        if (result == SOCKET_EAGAIN && socket_is_nonblocking(sock)) {
            socket_set_errno(SOCKET_EAGAIN);
            return SOCKET_EAGAIN;
        }
        socket_set_errno(network_error_to_socket_error(result));
        return network_error_to_socket_error(result);
    }
    
    /* Update statistics */
    g_socket_stats.bytes_sent += result;
    g_socket_stats.packets_sent++;
    
    return result;
}

long sys_recvfrom(int sockfd, void* buf, size_t len, int flags,
                  void* src_addr, uint32_t* addrlen) {
    /* Validate socket file descriptor */
    socket_t* sock = socket_fd_to_socket(sockfd);
    if (!sock) {
        socket_set_errno(SOCKET_EBADF);
        return SOCKET_EBADF;
    }
    
    /* Validate buffer */
    if (!validate_user_buffer(buf, len, true)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Validate source address parameters */
    if (src_addr && !validate_user_buffer(src_addr, sizeof(sockaddr_in_t), true)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    if (addrlen && !validate_user_buffer(addrlen, sizeof(uint32_t), true)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Perform receive operation */
    int result = 0;
    sockaddr_in_t src;
    
    if (sock->ops && sock->ops->recv) {
        result = sock->ops->recv(sock, buf, len, flags);
        if (result < 0) {
            if (result == SOCKET_EAGAIN && socket_is_nonblocking(sock)) {
                socket_set_errno(SOCKET_EAGAIN);
                return SOCKET_EAGAIN;
            }
            socket_set_errno(network_error_to_socket_error(result));
            return network_error_to_socket_error(result);
        }
        
        /* Get source address from socket */
        src = sock->remote_addr;
    } else {
        socket_set_errno(SOCKET_ENOTSOCK);
        return SOCKET_ENOTSOCK;
    }
    
    /* Copy source address to user space */
    if (src_addr && addrlen) {
        copy_sockaddr_to_user(src_addr, &src, addrlen);
    }
    
    /* Update statistics */
    g_socket_stats.bytes_received += result;
    g_socket_stats.packets_received++;
    
    return result;
}

long sys_shutdown(int sockfd, int how) {
    /* Validate socket file descriptor */
    socket_t* sock = socket_fd_to_socket(sockfd);
    if (!sock) {
        socket_set_errno(SOCKET_EBADF);
        return SOCKET_EBADF;
    }
    
    /* Validate shutdown type */
    if (how < SHUT_RD || how > SHUT_RDWR) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Perform shutdown operation */
    if (sock->ops && sock->ops->shutdown) {
        int result = sock->ops->shutdown(sock, how);
        if (result != SOCKET_SUCCESS) {
            socket_set_errno(network_error_to_socket_error(result));
            return network_error_to_socket_error(result);
        }
    } else {
        socket_set_errno(SOCKET_ENOTSOCK);
        return SOCKET_ENOTSOCK;
    }
    
    printf("Socket %d shutdown: how=%d\n", sockfd, how);
    return SOCKET_SUCCESS;
}

long sys_setsockopt(int sockfd, int level, int optname, 
                    const void* optval, uint32_t optlen) {
    /* Validate socket file descriptor */
    socket_t* sock = socket_fd_to_socket(sockfd);
    if (!sock) {
        socket_set_errno(SOCKET_EBADF);
        return SOCKET_EBADF;
    }
    
    /* Validate option value */
    if (optval && !validate_user_buffer(optval, optlen, false)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Perform setsockopt operation */
    if (sock->ops && sock->ops->setsockopt) {
        int result = sock->ops->setsockopt(sock, level, optname, optval, optlen);
        if (result != SOCKET_SUCCESS) {
            socket_set_errno(network_error_to_socket_error(result));
            return network_error_to_socket_error(result);
        }
    } else {
        socket_set_errno(SOCKET_ENOTSOCK);
        return SOCKET_ENOTSOCK;
    }
    
    return SOCKET_SUCCESS;
}

long sys_getsockopt(int sockfd, int level, int optname, 
                    void* optval, uint32_t* optlen) {
    /* Validate socket file descriptor */
    socket_t* sock = socket_fd_to_socket(sockfd);
    if (!sock) {
        socket_set_errno(SOCKET_EBADF);
        return SOCKET_EBADF;
    }
    
    /* Validate option value buffer */
    if (optval && optlen && !validate_user_buffer(optval, *optlen, true)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    if (optlen && !validate_user_buffer(optlen, sizeof(uint32_t), true)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Perform getsockopt operation */
    if (sock->ops && sock->ops->getsockopt) {
        int result = sock->ops->getsockopt(sock, level, optname, optval, optlen);
        if (result != SOCKET_SUCCESS) {
            socket_set_errno(network_error_to_socket_error(result));
            return network_error_to_socket_error(result);
        }
    } else {
        socket_set_errno(SOCKET_ENOTSOCK);
        return SOCKET_ENOTSOCK;
    }
    
    return SOCKET_SUCCESS;
}

long sys_getsockname(int sockfd, void* addr, uint32_t* addrlen) {
    /* Validate socket file descriptor */
    socket_t* sock = socket_fd_to_socket(sockfd);
    if (!sock) {
        socket_set_errno(SOCKET_EBADF);
        return SOCKET_EBADF;
    }
    
    /* Validate address parameters */
    if (!addr || !addrlen) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    if (!validate_user_buffer(addr, sizeof(sockaddr_in_t), true) ||
        !validate_user_buffer(addrlen, sizeof(uint32_t), true)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Copy local address to user space */
    copy_sockaddr_to_user(addr, &sock->local_addr, addrlen);
    
    return SOCKET_SUCCESS;
}

long sys_getpeername(int sockfd, void* addr, uint32_t* addrlen) {
    /* Validate socket file descriptor */
    socket_t* sock = socket_fd_to_socket(sockfd);
    if (!sock) {
        socket_set_errno(SOCKET_EBADF);
        return SOCKET_EBADF;
    }
    
    /* Check if socket is connected */
    if (!socket_is_connected(sock)) {
        socket_set_errno(SOCKET_ENOTCONN);
        return SOCKET_ENOTCONN;
    }
    
    /* Validate address parameters */
    if (!addr || !addrlen) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    if (!validate_user_buffer(addr, sizeof(sockaddr_in_t), true) ||
        !validate_user_buffer(addrlen, sizeof(uint32_t), true)) {
        socket_set_errno(SOCKET_ERROR);
        return SOCKET_ERROR;
    }
    
    /* Copy remote address to user space */
    copy_sockaddr_to_user(addr, &sock->remote_addr, addrlen);
    
    return SOCKET_SUCCESS;
}

/* ================================
 * Utility Functions
 * ================================ */

bool is_valid_socket_fd(int fd) {
    return (fd >= SOCKET_FD_OFFSET && 
            fd < SOCKET_FD_OFFSET + SOCKET_FD_MAX &&
            g_socket_fd_table.entries[fd - SOCKET_FD_OFFSET].allocated);
}

bool is_valid_socket_addr(const void* addr, uint32_t addrlen) {
    if (!addr || addrlen < sizeof(sockaddr_in_t)) {
        return false;
    }
    
    return validate_user_buffer(addr, addrlen, false);
}

bool is_valid_socket_domain(int domain) {
    return (domain == AF_INET || domain == AF_UNSPEC);
}

bool is_valid_socket_type(int type) {
    return (type == SOCK_STREAM || type == SOCK_DGRAM || type == SOCK_RAW);
}

bool is_valid_socket_protocol(int protocol) {
    return (protocol == 0 || protocol == IPPROTO_TCP || 
            protocol == IPPROTO_UDP || protocol == IPPROTO_ICMP);
}

int copy_sockaddr_from_user(sockaddr_in_t* dest, const void* src, uint32_t addrlen) {
    if (!dest || !src || addrlen < sizeof(sockaddr_in_t)) {
        return SOCKET_ERROR;
    }
    
    if (copy_from_user(dest, src, sizeof(sockaddr_in_t)) != 0) {
        return SOCKET_ERROR;
    }
    
    return SOCKET_SUCCESS;
}

int copy_sockaddr_to_user(void* dest, const sockaddr_in_t* src, uint32_t* addrlen) {
    if (!dest || !src || !addrlen) {
        return SOCKET_ERROR;
    }
    
    uint32_t copy_len = sizeof(sockaddr_in_t);
    if (*addrlen < copy_len) {
        copy_len = *addrlen;
    }
    
    if (copy_to_user(dest, src, copy_len) != 0) {
        return SOCKET_ERROR;
    }
    
    if (copy_to_user(addrlen, &copy_len, sizeof(uint32_t)) != 0) {
        return SOCKET_ERROR;
    }
    
    return SOCKET_SUCCESS;
}

int validate_user_buffer(const void* buf, size_t len, bool write_access) {
    if (!buf || len == 0) {
        return false;
    }
    
    /* Use existing validation function */
    return (validate_user_pointer(buf, len) == 0);
}

void socket_set_errno(int error) {
    /* TODO: Set per-process socket error */
    /* For now, just log the error */
    if (error != SOCKET_SUCCESS) {
        printf("Socket error: %d (%s)\n", error, socket_error_string(error));
    }
}

int socket_get_errno(void) {
    /* TODO: Get per-process socket error */
    return 0;
}

int network_error_to_socket_error(int net_error) {
    switch (net_error) {
        case NET_SUCCESS: return SOCKET_SUCCESS;
        case NET_ERROR_NOMEM: return SOCKET_ENOBUFS;
        case NET_ERROR_INVALID_PARAM: return SOCKET_ERROR;
        case NET_ERROR_NOT_FOUND: return SOCKET_EADDRNOTAVAIL;
        case NET_ERROR_TIMEOUT: return SOCKET_ETIMEDOUT;
        case NET_ERROR_QUEUE_FULL: return SOCKET_ENOBUFS;
        default: return SOCKET_ERROR;
    }
}

const char* socket_error_string(int error) {
    switch (error) {
        case SOCKET_SUCCESS: return "Success";
        case SOCKET_ERROR: return "General error";
        case SOCKET_EBADF: return "Bad file descriptor";
        case SOCKET_ENOTSOCK: return "Socket operation on non-socket";
        case SOCKET_EADDRINUSE: return "Address already in use";
        case SOCKET_EADDRNOTAVAIL: return "Cannot assign requested address";
        case SOCKET_ENETDOWN: return "Network is down";
        case SOCKET_ENETUNREACH: return "Network is unreachable";
        case SOCKET_ECONNABORTED: return "Software caused connection abort";
        case SOCKET_ECONNRESET: return "Connection reset by peer";
        case SOCKET_ENOBUFS: return "No buffer space available";
        case SOCKET_EISCONN: return "Transport endpoint is already connected";
        case SOCKET_ENOTCONN: return "Transport endpoint is not connected";
        case SOCKET_ETIMEDOUT: return "Connection timed out";
        case SOCKET_ECONNREFUSED: return "Connection refused";
        case SOCKET_EAGAIN: return "Try again";
        case SOCKET_EINPROGRESS: return "Operation now in progress";
        default: return "Unknown error";
    }
}

/* ================================
 * Socket Statistics
 * ================================ */

int socket_get_stats(socket_stats_t* stats) {
    if (!stats) {
        return SOCKET_ERROR;
    }
    
    *stats = g_socket_stats;
    return SOCKET_SUCCESS;
}

void socket_print_stats(void) {
    printf("\nSocket Statistics:\n");
    printf("==================\n");
    printf("Sockets created:     %lu\n", g_socket_stats.sockets_created);
    printf("Sockets destroyed:   %lu\n", g_socket_stats.sockets_destroyed);
    printf("Sockets active:      %lu\n", g_socket_stats.sockets_active);
    printf("TCP connections:     %lu\n", g_socket_stats.tcp_connections);
    printf("UDP sockets:         %lu\n", g_socket_stats.udp_sockets);
    printf("Bytes sent:          %lu\n", g_socket_stats.bytes_sent);
    printf("Bytes received:      %lu\n", g_socket_stats.bytes_received);
    printf("Packets sent:        %lu\n", g_socket_stats.packets_sent);
    printf("Packets received:    %lu\n", g_socket_stats.packets_received);
    printf("Errors:              %lu\n", g_socket_stats.errors);
}

/* ================================
 * Non-blocking Socket Support
 * ================================ */

int socket_set_nonblocking(socket_t* sock, bool nonblock) {
    if (!sock) {
        return SOCKET_EBADF;
    }
    
    if (nonblock) {
        sock->flags |= SOCKET_FLAG_NONBLOCK;
    } else {
        sock->flags &= ~SOCKET_FLAG_NONBLOCK;
    }
    
    return SOCKET_SUCCESS;
}

bool socket_is_nonblocking(socket_t* sock) {
    if (!sock) {
        return false;
    }
    
    return (sock->flags & SOCKET_FLAG_NONBLOCK) != 0;
}

int socket_would_block(socket_t* sock, int operation) {
    if (!sock) {
        return false;
    }
    
    /* Check if socket is in non-blocking mode */
    if (!socket_is_nonblocking(sock)) {
        return false;
    }
    
    /* Check operation-specific conditions */
    switch (operation) {
        case SOCKET_OP_READ:
            /* Check if receive buffer has data */
            return (socket_buffer_available(sock->recv_buffer) == 0);
            
        case SOCKET_OP_WRITE:
            /* Check if send buffer has space */
            return (socket_buffer_space(sock->send_buffer) == 0);
            
        case SOCKET_OP_ACCEPT:
            /* Check if there are pending connections */
            return (sock->state != SOCKET_STATE_LISTEN || sock->backlog_count == 0);
            
        case SOCKET_OP_CONNECT:
            /* Check if connection is in progress */
            return (sock->state == SOCKET_STATE_SYN_SENT);
            
        default:
            return false;
    }
}

/* ================================
 * Socket Event Notification
 * ================================ */

int socket_register_event_callback(socket_t* sock, socket_event_callback_t callback, void* user_data) {
    if (!sock || !callback) {
        return SOCKET_ERROR;
    }
    
    sock->event_callback = callback;
    sock->event_user_data = user_data;
    
    return SOCKET_SUCCESS;
}

int socket_unregister_event_callback(socket_t* sock) {
    if (!sock) {
        return SOCKET_ERROR;
    }
    
    sock->event_callback = NULL;
    sock->event_user_data = NULL;
    
    return SOCKET_SUCCESS;
}

void socket_trigger_event(socket_t* sock, uint32_t events) {
    if (!sock || !sock->event_callback) {
        return;
    }
    
    sock->event_callback(sock, events, sock->event_user_data);
}
