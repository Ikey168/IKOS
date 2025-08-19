/*
 * IKOS Socket User-Space API - Implementation
 * Issue #46 - Sockets API for User Applications
 * 
 * User-space implementation of Berkeley-style socket API providing
 * network communication capabilities for applications.
 */

#include "../include/socket_user_api.h"
#include "../include/syscalls.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ================================
 * Global State
 * ================================ */

static bool socket_lib_initialized = false;
static socket_user_stats_t user_stats;
static int last_socket_error = 0;

/* ================================
 * Library Initialization
 * ================================ */

int socket_lib_init(void) {
    if (socket_lib_initialized) {
        return SOCK_SUCCESS;
    }
    
    /* Initialize statistics */
    memset(&user_stats, 0, sizeof(socket_user_stats_t));
    last_socket_error = 0;
    
    socket_lib_initialized = true;
    printf("Socket library initialized\n");
    return SOCK_SUCCESS;
}

void socket_lib_cleanup(void) {
    if (!socket_lib_initialized) {
        return;
    }
    
    socket_lib_initialized = false;
    printf("Socket library cleaned up\n");
}

bool socket_lib_is_initialized(void) {
    return socket_lib_initialized;
}

/* ================================
 * Basic Socket Operations
 * ================================ */

int socket(int domain, int type, int protocol) {
    if (!socket_lib_initialized) {
        socket_lib_init();
    }
    
    long result = syscall3(SYS_SOCKET, domain, type, protocol);
    
    if (result >= 0) {
        user_stats.sockets_created++;
        printf("Created socket: domain=%d, type=%d, protocol=%d, fd=%ld\n", 
               domain, type, protocol, result);
    } else {
        user_stats.errors++;
        last_socket_error = (int)result;
        printf("Failed to create socket: %ld\n", result);
    }
    
    return (int)result;
}

int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    long result = syscall3(SYS_BIND, sockfd, (long)addr, addrlen);
    
    if (result != 0) {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (int)result;
}

int listen(int sockfd, int backlog) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    long result = syscall2(SYS_LISTEN, sockfd, backlog);
    
    if (result != 0) {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (int)result;
}

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    long result = syscall3(SYS_ACCEPT, sockfd, (long)addr, (long)addrlen);
    
    if (result >= 0) {
        user_stats.sockets_created++;
        printf("Accepted connection: server_fd=%d, client_fd=%ld\n", sockfd, result);
    } else {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (int)result;
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    long result = syscall3(SYS_CONNECT, sockfd, (long)addr, addrlen);
    
    if (result != 0) {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (int)result;
}

/* ================================
 * Data Transfer Operations
 * ================================ */

ssize_t send(int sockfd, const void* buf, size_t len, int flags) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    long result = syscall4(SYS_SEND, sockfd, (long)buf, len, flags);
    
    if (result >= 0) {
        user_stats.bytes_sent += result;
        user_stats.send_calls++;
    } else {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (ssize_t)result;
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    long result = syscall4(SYS_RECV, sockfd, (long)buf, len, flags);
    
    if (result >= 0) {
        user_stats.bytes_received += result;
        user_stats.recv_calls++;
    } else {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (ssize_t)result;
}

ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr* dest_addr, socklen_t addrlen) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    long result = syscall6(SYS_SENDTO, sockfd, (long)buf, len, flags, (long)dest_addr, addrlen);
    
    if (result >= 0) {
        user_stats.bytes_sent += result;
        user_stats.send_calls++;
    } else {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (ssize_t)result;
}

ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,
                 struct sockaddr* src_addr, socklen_t* addrlen) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    long result = syscall6(SYS_RECVFROM, sockfd, (long)buf, len, flags, (long)src_addr, (long)addrlen);
    
    if (result >= 0) {
        user_stats.bytes_received += result;
        user_stats.recv_calls++;
    } else {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (ssize_t)result;
}

/* ================================
 * Socket Control Operations
 * ================================ */

int close_socket(int sockfd) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    /* Use standard close() syscall for now */
    long result = syscall1(SYS_CLOSE, sockfd);
    
    if (result == 0) {
        user_stats.sockets_closed++;
    } else {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (int)result;
}

int shutdown(int sockfd, int how) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    long result = syscall2(SYS_SHUTDOWN, sockfd, how);
    
    if (result != 0) {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (int)result;
}

int setsockopt(int sockfd, int level, int optname, 
               const void* optval, socklen_t optlen) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    long result = syscall5(SYS_SETSOCKOPT, sockfd, level, optname, (long)optval, optlen);
    
    if (result != 0) {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (int)result;
}

int getsockopt(int sockfd, int level, int optname, 
               void* optval, socklen_t* optlen) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    long result = syscall5(SYS_GETSOCKOPT, sockfd, level, optname, (long)optval, (long)optlen);
    
    if (result != 0) {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (int)result;
}

int getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    long result = syscall3(SYS_GETSOCKNAME, sockfd, (long)addr, (long)addrlen);
    
    if (result != 0) {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (int)result;
}

int getpeername(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    if (!socket_lib_initialized) {
        return SOCK_ERROR;
    }
    
    long result = syscall3(SYS_GETPEERNAME, sockfd, (long)addr, (long)addrlen);
    
    if (result != 0) {
        user_stats.errors++;
        last_socket_error = (int)result;
    }
    
    return (int)result;
}

/* ================================
 * Address Utility Functions
 * ================================ */

uint32_t inet_addr(const char* cp) {
    if (!cp) {
        return INADDR_NONE;
    }
    
    uint32_t addr = 0;
    uint32_t parts[4];
    char* endptr;
    
    /* Parse dotted decimal notation */
    for (int i = 0; i < 4; i++) {
        parts[i] = strtoul(cp, &endptr, 10);
        if (parts[i] > 255) {
            return INADDR_NONE;
        }
        
        if (i < 3) {
            if (*endptr != '.') {
                return INADDR_NONE;
            }
            cp = endptr + 1;
        } else {
            if (*endptr != '\0') {
                return INADDR_NONE;
            }
        }
    }
    
    /* Combine parts into 32-bit address */
    addr = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    return htonl(addr);
}

char* inet_ntoa(struct in_addr in) {
    static char buffer[16];
    uint32_t addr = ntohl(in.s_addr);
    
    snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u",
             (addr >> 24) & 0xFF,
             (addr >> 16) & 0xFF,
             (addr >> 8) & 0xFF,
             addr & 0xFF);
    
    return buffer;
}

int inet_aton(const char* cp, struct in_addr* inp) {
    if (!cp || !inp) {
        return 0;
    }
    
    uint32_t addr = inet_addr(cp);
    if (addr == INADDR_NONE) {
        return 0;
    }
    
    inp->s_addr = addr;
    return 1;
}

const char* inet_ntop(int af, const void* src, char* dst, socklen_t size) {
    if (af != AF_INET || !src || !dst || size < 16) {
        return NULL;
    }
    
    struct in_addr* addr = (struct in_addr*)src;
    const char* result = inet_ntoa(*addr);
    
    if (strlen(result) >= size) {
        return NULL;
    }
    
    strcpy(dst, result);
    return dst;
}

int inet_pton(int af, const char* src, void* dst) {
    if (af != AF_INET || !src || !dst) {
        return -1;
    }
    
    struct in_addr addr;
    if (!inet_aton(src, &addr)) {
        return 0;
    }
    
    *(struct in_addr*)dst = addr;
    return 1;
}

/* ================================
 * Byte Order Conversion
 * ================================ */

uint32_t htonl(uint32_t hostlong) {
    /* Convert host to network byte order (big-endian) */
    return ((hostlong & 0xFF000000) >> 24) |
           ((hostlong & 0x00FF0000) >> 8)  |
           ((hostlong & 0x0000FF00) << 8)  |
           ((hostlong & 0x000000FF) << 24);
}

uint16_t htons(uint16_t hostshort) {
    /* Convert host to network byte order (big-endian) */
    return ((hostshort & 0xFF00) >> 8) | ((hostshort & 0x00FF) << 8);
}

uint32_t ntohl(uint32_t netlong) {
    /* Convert network to host byte order */
    return htonl(netlong);  /* Same operation for conversion */
}

uint16_t ntohs(uint16_t netshort) {
    /* Convert network to host byte order */
    return htons(netshort);  /* Same operation for conversion */
}

/* ================================
 * Socket Address Utilities
 * ================================ */

int sockaddr_in_init(struct sockaddr_in* addr, uint32_t ip, uint16_t port) {
    if (!addr) {
        return SOCK_ERROR;
    }
    
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = htonl(ip);
    addr->sin_port = htons(port);
    
    return SOCK_SUCCESS;
}

int sockaddr_in_from_string(struct sockaddr_in* addr, const char* ip_str, uint16_t port) {
    if (!addr || !ip_str) {
        return SOCK_ERROR;
    }
    
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    
    if (!inet_aton(ip_str, &addr->sin_addr)) {
        return SOCK_ERROR;
    }
    
    return SOCK_SUCCESS;
}

char* sockaddr_in_to_string(const struct sockaddr_in* addr, char* buf, size_t len) {
    if (!addr || !buf || len < 22) {
        return NULL;
    }
    
    snprintf(buf, len, "%s:%u", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    return buf;
}

bool sockaddr_in_equal(const struct sockaddr_in* addr1, const struct sockaddr_in* addr2) {
    if (!addr1 || !addr2) {
        return false;
    }
    
    return (addr1->sin_family == addr2->sin_family &&
            addr1->sin_addr.s_addr == addr2->sin_addr.s_addr &&
            addr1->sin_port == addr2->sin_port);
}

/* ================================
 * Error Handling
 * ================================ */

int socket_errno(void) {
    return last_socket_error;
}

const char* socket_strerror(int error) {
    switch (error) {
        case SOCK_SUCCESS: return "Success";
        case SOCK_ERROR: return "General error";
        case SOCK_EBADF: return "Bad file descriptor";
        case SOCK_EAGAIN: return "Try again";
        case SOCK_ENOTSOCK: return "Socket operation on non-socket";
        case SOCK_EADDRINUSE: return "Address already in use";
        case SOCK_EADDRNOTAVAIL: return "Cannot assign requested address";
        case SOCK_ENETDOWN: return "Network is down";
        case SOCK_ENETUNREACH: return "Network is unreachable";
        case SOCK_ECONNABORTED: return "Software caused connection abort";
        case SOCK_ECONNRESET: return "Connection reset by peer";
        case SOCK_ENOBUFS: return "No buffer space available";
        case SOCK_EISCONN: return "Transport endpoint is already connected";
        case SOCK_ENOTCONN: return "Transport endpoint is not connected";
        case SOCK_ETIMEDOUT: return "Connection timed out";
        case SOCK_ECONNREFUSED: return "Connection refused";
        case SOCK_EINPROGRESS: return "Operation now in progress";
        default: return "Unknown error";
    }
}

/* ================================
 * High-level Socket Utilities
 * ================================ */

int tcp_client_connect(const char* host, uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        return sockfd;
    }
    
    struct sockaddr_in addr;
    if (sockaddr_in_from_string(&addr, host, port) != SOCK_SUCCESS) {
        close_socket(sockfd);
        return SOCK_ERROR;
    }
    
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) != SOCK_SUCCESS) {
        close_socket(sockfd);
        return SOCK_ERROR;
    }
    
    return sockfd;
}

int tcp_client_send_string(int sockfd, const char* str) {
    if (!str) {
        return SOCK_ERROR;
    }
    
    return (int)send(sockfd, str, strlen(str), 0);
}

int tcp_client_recv_string(int sockfd, char* buf, size_t len) {
    if (!buf || len == 0) {
        return SOCK_ERROR;
    }
    
    int result = (int)recv(sockfd, buf, len - 1, 0);
    if (result > 0) {
        buf[result] = '\0';
    }
    
    return result;
}

int tcp_server_create(uint16_t port, int backlog) {
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        return sockfd;
    }
    
    /* Enable address reuse */
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_in addr;
    sockaddr_in_init(&addr, INADDR_ANY, port);
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) != SOCK_SUCCESS) {
        close_socket(sockfd);
        return SOCK_ERROR;
    }
    
    if (listen(sockfd, backlog) != SOCK_SUCCESS) {
        close_socket(sockfd);
        return SOCK_ERROR;
    }
    
    return sockfd;
}

int tcp_server_accept_client(int server_fd, char* client_ip, size_t ip_len, uint16_t* client_port) {
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
    if (client_fd >= 0) {
        if (client_ip && ip_len > 0) {
            strncpy(client_ip, inet_ntoa(client_addr.sin_addr), ip_len - 1);
            client_ip[ip_len - 1] = '\0';
        }
        
        if (client_port) {
            *client_port = ntohs(client_addr.sin_port);
        }
    }
    
    return client_fd;
}

int udp_client_create(void) {
    return socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

int udp_client_send_to(int sockfd, const char* host, uint16_t port, const void* data, size_t len) {
    struct sockaddr_in addr;
    if (sockaddr_in_from_string(&addr, host, port) != SOCK_SUCCESS) {
        return SOCK_ERROR;
    }
    
    return (int)sendto(sockfd, data, len, 0, (struct sockaddr*)&addr, sizeof(addr));
}

int udp_client_recv_from(int sockfd, void* data, size_t len, char* from_ip, size_t ip_len, uint16_t* from_port) {
    struct sockaddr_in from_addr;
    socklen_t addrlen = sizeof(from_addr);
    
    int result = (int)recvfrom(sockfd, data, len, 0, (struct sockaddr*)&from_addr, &addrlen);
    
    if (result >= 0) {
        if (from_ip && ip_len > 0) {
            strncpy(from_ip, inet_ntoa(from_addr.sin_addr), ip_len - 1);
            from_ip[ip_len - 1] = '\0';
        }
        
        if (from_port) {
            *from_port = ntohs(from_addr.sin_port);
        }
    }
    
    return result;
}

int udp_server_create(uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        return sockfd;
    }
    
    struct sockaddr_in addr;
    sockaddr_in_init(&addr, INADDR_ANY, port);
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) != SOCK_SUCCESS) {
        close_socket(sockfd);
        return SOCK_ERROR;
    }
    
    return sockfd;
}

int udp_server_recv_from(int sockfd, void* data, size_t len, char* from_ip, size_t ip_len, uint16_t* from_port) {
    return udp_client_recv_from(sockfd, data, len, from_ip, ip_len, from_port);
}

int udp_server_send_to(int sockfd, const char* host, uint16_t port, const void* data, size_t len) {
    return udp_client_send_to(sockfd, host, port, data, len);
}

/* ================================
 * Socket Configuration
 * ================================ */

int socket_set_send_buffer_size(int sockfd, int size) {
    return setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
}

int socket_set_recv_buffer_size(int sockfd, int size) {
    return setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
}

int socket_get_send_buffer_size(int sockfd) {
    int size;
    socklen_t len = sizeof(size);
    
    if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &size, &len) == SOCK_SUCCESS) {
        return size;
    }
    
    return SOCK_ERROR;
}

int socket_get_recv_buffer_size(int sockfd) {
    int size;
    socklen_t len = sizeof(size);
    
    if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, &len) == SOCK_SUCCESS) {
        return size;
    }
    
    return SOCK_ERROR;
}

int socket_set_nonblocking(int sockfd, bool nonblock) {
    /* This would typically use fcntl, but we'll use a socket option */
    int flag = nonblock ? 1 : 0;
    return setsockopt(sockfd, SOL_SOCKET, SO_DEBUG, &flag, sizeof(flag)); /* Placeholder */
}

bool socket_is_nonblocking(int sockfd) {
    int flag;
    socklen_t len = sizeof(flag);
    
    if (getsockopt(sockfd, SOL_SOCKET, SO_DEBUG, &flag, &len) == SOCK_SUCCESS) {
        return (flag != 0);
    }
    
    return false;
}

int socket_set_reuseaddr(int sockfd, bool reuse) {
    int flag = reuse ? 1 : 0;
    return setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
}

int socket_set_keepalive(int sockfd, bool keepalive) {
    int flag = keepalive ? 1 : 0;
    return setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
}

int socket_set_broadcast(int sockfd, bool broadcast) {
    int flag = broadcast ? 1 : 0;
    return setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &flag, sizeof(flag));
}

/* ================================
 * Socket Statistics
 * ================================ */

int socket_get_user_stats(socket_user_stats_t* stats) {
    if (!stats) {
        return SOCK_ERROR;
    }
    
    *stats = user_stats;
    return SOCK_SUCCESS;
}

void socket_print_user_stats(void) {
    printf("\nUser-Space Socket Statistics:\n");
    printf("============================\n");
    printf("Sockets created:     %lu\n", user_stats.sockets_created);
    printf("Sockets closed:      %lu\n", user_stats.sockets_closed);
    printf("Bytes sent:          %lu\n", user_stats.bytes_sent);
    printf("Bytes received:      %lu\n", user_stats.bytes_received);
    printf("Send calls:          %lu\n", user_stats.send_calls);
    printf("Receive calls:       %lu\n", user_stats.recv_calls);
    printf("Errors:              %lu\n", user_stats.errors);
}

void socket_reset_user_stats(void) {
    memset(&user_stats, 0, sizeof(socket_user_stats_t));
}

/* ================================
 * Basic Select Implementation
 * ================================ */

int socket_select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout) {
    /* Basic implementation - just check if any sockets would not block */
    /* This is a simplified version for demonstration */
    
    int ready_count = 0;
    
    /* Check read file descriptors */
    if (readfds) {
        for (int fd = 0; fd < nfds; fd++) {
            if (FD_ISSET(fd, readfds)) {
                /* For now, assume all sockets are ready for reading */
                ready_count++;
            }
        }
    }
    
    /* Check write file descriptors */
    if (writefds) {
        for (int fd = 0; fd < nfds; fd++) {
            if (FD_ISSET(fd, writefds)) {
                /* For now, assume all sockets are ready for writing */
                ready_count++;
            }
        }
    }
    
    /* Exception file descriptors are not implemented in this basic version */
    if (exceptfds) {
        FD_ZERO(exceptfds);
    }
    
    return ready_count;
}
