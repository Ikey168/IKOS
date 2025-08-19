/*
 * IKOS Socket User-Space API - Header Definitions
 * Issue #46 - Sockets API for User Applications
 * 
 * Provides Berkeley-style socket API for user-space applications
 * with complete network communication support.
 */

#ifndef SOCKET_USER_API_H
#define SOCKET_USER_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "syscalls.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================
 * Socket Address Structures
 * ================================ */

/* Address families */
#define AF_UNSPEC           0   /* Unspecified */
#define AF_INET             2   /* IPv4 Internet protocols */
#define PF_INET             AF_INET

/* Socket types */
#define SOCK_STREAM         1   /* Sequenced, reliable, connection-based */
#define SOCK_DGRAM          2   /* Connectionless, unreliable datagrams */
#define SOCK_RAW            3   /* Raw protocol interface */

/* Protocol numbers */
#define IPPROTO_IP          0   /* Dummy protocol for TCP */
#define IPPROTO_ICMP        1   /* Internet Control Message Protocol */
#define IPPROTO_TCP         6   /* Transmission Control Protocol */
#define IPPROTO_UDP         17  /* User Datagram Protocol */

/* Socket options */
#define SOL_SOCKET          1   /* Socket-level options */
#define SO_DEBUG            1   /* Turn on debugging info recording */
#define SO_REUSEADDR        2   /* Allow local address reuse */
#define SO_TYPE             3   /* Get socket type */
#define SO_ERROR            4   /* Get error status and clear */
#define SO_DONTROUTE        5   /* Don't use gateway routing */
#define SO_BROADCAST        6   /* Allow transmission of broadcast msgs */
#define SO_SNDBUF           7   /* Send buffer size */
#define SO_RCVBUF           8   /* Receive buffer size */
#define SO_KEEPALIVE        9   /* Keep connections alive */
#define SO_OOBINLINE        10  /* Leave received OOB data in line */
#define SO_LINGER           13  /* Linger on close if data present */
#define SO_REUSEPORT        15  /* Allow local address & port reuse */

/* Shutdown flags */
#define SHUT_RD             0   /* Shut down read side */
#define SHUT_WR             1   /* Shut down write side */
#define SHUT_RDWR           2   /* Shut down both sides */

/* Message flags */
#define MSG_PEEK            0x02    /* Peek at incoming data */
#define MSG_WAITALL         0x40    /* Wait for complete data */
#define MSG_DONTWAIT        0x80    /* Non-blocking operation */

/* IPv4 address structure */
typedef struct in_addr {
    uint32_t s_addr;        /* Internet address */
} in_addr_t;

/* IPv4 socket address structure */
typedef struct sockaddr_in {
    uint16_t sin_family;    /* Address family (AF_INET) */
    uint16_t sin_port;      /* Port number */
    in_addr_t sin_addr;     /* Internet address */
    uint8_t sin_zero[8];    /* Padding to make structure same size as sockaddr */
} sockaddr_in_t;

/* Generic socket address structure */
typedef struct sockaddr {
    uint16_t sa_family;     /* Address family */
    char sa_data[14];       /* Address data */
} sockaddr_t;

/* Socket length type */
typedef uint32_t socklen_t;

/* ================================
 * Socket API Functions
 * ================================ */

/* Basic Socket Operations */
int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);

/* Data Transfer */
ssize_t send(int sockfd, const void* buf, size_t len, int flags);
ssize_t recv(int sockfd, void* buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr* dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,
                 struct sockaddr* src_addr, socklen_t* addrlen);

/* Socket Control */
int close_socket(int sockfd);  /* Note: close() conflicts with file close */
int shutdown(int sockfd, int how);
int setsockopt(int sockfd, int level, int optname, 
               const void* optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname, 
               void* optval, socklen_t* optlen);

/* Socket Information */
int getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
int getpeername(int sockfd, struct sockaddr* addr, socklen_t* addrlen);

/* ================================
 * Address Utility Functions
 * ================================ */

/* IPv4 address conversion */
uint32_t inet_addr(const char* cp);
char* inet_ntoa(struct in_addr in);
int inet_aton(const char* cp, struct in_addr* inp);
const char* inet_ntop(int af, const void* src, char* dst, socklen_t size);
int inet_pton(int af, const char* src, void* dst);

/* Host byte order conversion */
uint32_t htonl(uint32_t hostlong);
uint16_t htons(uint16_t hostshort);
uint32_t ntohl(uint32_t netlong);
uint16_t ntohs(uint16_t netshort);

/* Socket address utilities */
int sockaddr_in_init(struct sockaddr_in* addr, uint32_t ip, uint16_t port);
int sockaddr_in_from_string(struct sockaddr_in* addr, const char* ip_str, uint16_t port);
char* sockaddr_in_to_string(const struct sockaddr_in* addr, char* buf, size_t len);
bool sockaddr_in_equal(const struct sockaddr_in* addr1, const struct sockaddr_in* addr2);

/* ================================
 * Socket Error Handling
 * ================================ */

/* Socket error codes */
#define SOCK_SUCCESS        0   /* Success */
#define SOCK_ERROR         -1   /* General error */
#define SOCK_EBADF         -9   /* Bad file descriptor */
#define SOCK_EAGAIN        -11  /* Try again */
#define SOCK_ENOTSOCK      -88  /* Socket operation on non-socket */
#define SOCK_EADDRINUSE    -98  /* Address already in use */
#define SOCK_EADDRNOTAVAIL -99  /* Cannot assign requested address */
#define SOCK_ENETDOWN      -100 /* Network is down */
#define SOCK_ENETUNREACH   -101 /* Network is unreachable */
#define SOCK_ECONNABORTED  -103 /* Software caused connection abort */
#define SOCK_ECONNRESET    -104 /* Connection reset by peer */
#define SOCK_ENOBUFS       -105 /* No buffer space available */
#define SOCK_EISCONN       -106 /* Transport endpoint is already connected */
#define SOCK_ENOTCONN      -107 /* Transport endpoint is not connected */
#define SOCK_ETIMEDOUT     -110 /* Connection timed out */
#define SOCK_ECONNREFUSED  -111 /* Connection refused */
#define SOCK_EINPROGRESS   -115 /* Operation now in progress */

/* Get last socket error */
int socket_errno(void);

/* Get socket error string */
const char* socket_strerror(int error);

/* ================================
 * Socket Library Initialization
 * ================================ */

/* Initialize socket library */
int socket_lib_init(void);

/* Cleanup socket library */
void socket_lib_cleanup(void);

/* Check if socket library is initialized */
bool socket_lib_is_initialized(void);

/* ================================
 * High-level Socket Utilities
 * ================================ */

/* TCP Client */
int tcp_client_connect(const char* host, uint16_t port);
int tcp_client_send_string(int sockfd, const char* str);
int tcp_client_recv_string(int sockfd, char* buf, size_t len);

/* TCP Server */
int tcp_server_create(uint16_t port, int backlog);
int tcp_server_accept_client(int server_fd, char* client_ip, size_t ip_len, uint16_t* client_port);

/* UDP Client */
int udp_client_create(void);
int udp_client_send_to(int sockfd, const char* host, uint16_t port, const void* data, size_t len);
int udp_client_recv_from(int sockfd, void* data, size_t len, char* from_ip, size_t ip_len, uint16_t* from_port);

/* UDP Server */
int udp_server_create(uint16_t port);
int udp_server_recv_from(int sockfd, void* data, size_t len, char* from_ip, size_t ip_len, uint16_t* from_port);
int udp_server_send_to(int sockfd, const char* host, uint16_t port, const void* data, size_t len);

/* ================================
 * Socket Configuration
 * ================================ */

/* Socket buffer management */
int socket_set_send_buffer_size(int sockfd, int size);
int socket_set_recv_buffer_size(int sockfd, int size);
int socket_get_send_buffer_size(int sockfd);
int socket_get_recv_buffer_size(int sockfd);

/* Socket timeout management */
int socket_set_send_timeout(int sockfd, int timeout_ms);
int socket_set_recv_timeout(int sockfd, int timeout_ms);
int socket_get_send_timeout(int sockfd);
int socket_get_recv_timeout(int sockfd);

/* Socket flags */
int socket_set_nonblocking(int sockfd, bool nonblock);
bool socket_is_nonblocking(int sockfd);
int socket_set_reuseaddr(int sockfd, bool reuse);
int socket_set_keepalive(int sockfd, bool keepalive);
int socket_set_broadcast(int sockfd, bool broadcast);

/* ================================
 * Socket Statistics
 * ================================ */

typedef struct socket_user_stats {
    uint64_t sockets_created;
    uint64_t sockets_closed;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t send_calls;
    uint64_t recv_calls;
    uint64_t errors;
} socket_user_stats_t;

/* Get socket statistics */
int socket_get_user_stats(socket_user_stats_t* stats);

/* Print socket statistics */
void socket_print_user_stats(void);

/* Reset socket statistics */
void socket_reset_user_stats(void);

/* ================================
 * Socket Multiplexing (Basic)
 * ================================ */

/* File descriptor set for basic multiplexing */
#define FD_SETSIZE          64

typedef struct fd_set {
    uint32_t fds_bits[FD_SETSIZE / 32];
} fd_set;

/* FD set manipulation macros */
#define FD_ZERO(set)        memset((set), 0, sizeof(fd_set))
#define FD_SET(fd, set)     ((set)->fds_bits[(fd)/32] |= (1 << ((fd) % 32)))
#define FD_CLR(fd, set)     ((set)->fds_bits[(fd)/32] &= ~(1 << ((fd) % 32)))
#define FD_ISSET(fd, set)   (((set)->fds_bits[(fd)/32] & (1 << ((fd) % 32))) != 0)

/* Timeout structure */
struct timeval {
    long tv_sec;        /* seconds */
    long tv_usec;       /* microseconds */
};

/* Basic select() functionality */
int socket_select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout);

/* ================================
 * Constants and Limits
 * ================================ */

/* Well-known ports */
#define PORT_ECHO           7
#define PORT_DISCARD        9
#define PORT_DAYTIME        13
#define PORT_FTP_DATA       20
#define PORT_FTP            21
#define PORT_SSH            22
#define PORT_TELNET         23
#define PORT_SMTP           25
#define PORT_DNS            53
#define PORT_HTTP           80
#define PORT_POP3           110
#define PORT_IMAP           143
#define PORT_HTTPS          443

/* Address constants */
#define INADDR_ANY          0x00000000  /* Any address */
#define INADDR_LOOPBACK     0x7F000001  /* Loopback address (127.0.0.1) */
#define INADDR_BROADCAST    0xFFFFFFFF  /* Broadcast address */
#define INADDR_NONE         0xFFFFFFFF  /* Invalid address */

/* Limits */
#define SOCKET_MAX_HOSTNAME 256         /* Maximum hostname length */
#define SOCKET_MAX_BACKLOG  128         /* Maximum listen backlog */
#define SOCKET_MAX_BUFSIZE  65536       /* Maximum buffer size */

#ifdef __cplusplus
}
#endif

#endif /* SOCKET_USER_API_H */
