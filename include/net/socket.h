/* IKOS Network Stack - Socket API
 * Issue #35 - Network Stack Implementation
 * 
 * Provides BSD-style socket interface for network communication
 * supporting TCP, UDP, and raw sockets.
 */

#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "network.h"

/* ================================
 * Socket Constants
 * ================================ */

/* Address Families */
#define AF_UNSPEC          0
#define AF_INET            2        /* IPv4 */
#define PF_INET            AF_INET

/* Socket Types */
#define SOCK_STREAM        1        /* TCP - reliable, connection-based */
#define SOCK_DGRAM         2        /* UDP - unreliable, connectionless */
#define SOCK_RAW           3        /* Raw sockets */

/* Protocol Numbers */
#define IPPROTO_IP         0        /* IP protocol */
#define IPPROTO_ICMP       1        /* ICMP protocol */
#define IPPROTO_TCP        6        /* TCP protocol */
#define IPPROTO_UDP        17       /* UDP protocol */

/* Socket Options */
#define SOL_SOCKET         1        /* Socket level */
#define SO_DEBUG           1        /* Debug info */
#define SO_REUSEADDR       2        /* Reuse address */
#define SO_TYPE            3        /* Socket type */
#define SO_ERROR           4        /* Socket error */
#define SO_DONTROUTE       5        /* Don't route */
#define SO_BROADCAST       6        /* Permit broadcast */
#define SO_SNDBUF          7        /* Send buffer size */
#define SO_RCVBUF          8        /* Receive buffer size */
#define SO_KEEPALIVE       9        /* Keep connections alive */
#define SO_OOBINLINE       10       /* Out-of-band inline */
#define SO_LINGER          13       /* Linger on close */
#define SO_REUSEPORT       15       /* Reuse port */

/* Socket Flags for send/recv */
#define MSG_PEEK           0x02     /* Peek at incoming data */
#define MSG_WAITALL        0x40     /* Wait for complete data */
#define MSG_DONTWAIT       0x80     /* Non-blocking operation */
#define MSG_TRUNC          0x20     /* Data truncated */

/* Socket States */
typedef enum {
    SOCKET_STATE_CLOSED = 0,       /* Socket is closed */
    SOCKET_STATE_LISTEN,           /* Socket is listening */
    SOCKET_STATE_SYN_SENT,         /* SYN sent, waiting for SYN-ACK */
    SOCKET_STATE_SYN_RECEIVED,     /* SYN received, waiting for ACK */
    SOCKET_STATE_ESTABLISHED,      /* Connection established */
    SOCKET_STATE_FIN_WAIT_1,       /* FIN sent, waiting for ACK */
    SOCKET_STATE_FIN_WAIT_2,       /* FIN ACKed, waiting for FIN */
    SOCKET_STATE_TIME_WAIT,        /* Waiting for 2MSL */
    SOCKET_STATE_CLOSE_WAIT,       /* Remote FIN received */
    SOCKET_STATE_LAST_ACK,         /* Last ACK sent */
    SOCKET_STATE_CLOSING           /* Both sides closing */
} socket_state_t;

/* Socket Errors */
#define SOCKET_SUCCESS         0
#define SOCKET_ERROR_INVALID   -1   /* Invalid argument */
#define SOCKET_ERROR_NOMEM     -2   /* No memory */
#define SOCKET_ERROR_NOBUFS    -3   /* No buffer space */
#define SOCKET_ERROR_AGAIN     -4   /* Try again */
#define SOCKET_ERROR_INTR      -5   /* Interrupted */
#define SOCKET_ERROR_FAULT     -6   /* Bad address */
#define SOCKET_ERROR_CONNRESET -7   /* Connection reset */
#define SOCKET_ERROR_TIMEOUT   -8   /* Operation timed out */
#define SOCKET_ERROR_REFUSED   -9   /* Connection refused */
#define SOCKET_ERROR_HOSTUNREACH -10 /* Host unreachable */
#define SOCKET_ERROR_NETUNREACH -11  /* Network unreachable */

#define INVALID_SOCKET         -1

/* ================================
 * Socket Data Structures
 * ================================ */

/* Socket Address */
typedef struct {
    uint16_t sa_family;        /* Address family */
    char sa_data[14];          /* Address data */
} sockaddr_t;

/* Internet Socket Address */
typedef struct {
    uint16_t sin_family;       /* AF_INET */
    uint16_t sin_port;         /* Port number (network byte order) */
    ip_addr_t sin_addr;        /* IPv4 address */
    uint8_t sin_zero[8];       /* Padding */
} sockaddr_in_t;

/* Socket Linger Structure */
typedef struct {
    int l_onoff;               /* Linger on/off */
    int l_linger;              /* Linger time (seconds) */
} linger_t;

/* Socket Buffer */
typedef struct socket_buffer {
    uint8_t* data;             /* Buffer data */
    uint32_t size;             /* Buffer size */
    uint32_t head;             /* Data start */
    uint32_t tail;             /* Data end */
    uint32_t count;            /* Data count */
    struct socket_buffer* next; /* Next buffer */
} socket_buffer_t;

/* Socket Control Block */
typedef struct socket {
    int fd;                    /* File descriptor */
    int family;                /* Address family (AF_INET) */
    int type;                  /* Socket type (SOCK_STREAM, SOCK_DGRAM) */
    int protocol;              /* Protocol (IPPROTO_TCP, IPPROTO_UDP) */
    
    /* State */
    socket_state_t state;      /* Socket state */
    uint32_t flags;            /* Socket flags */
    int error;                 /* Last error */
    
    /* Addresses */
    sockaddr_in_t local_addr;  /* Local address and port */
    sockaddr_in_t remote_addr; /* Remote address and port */
    
    /* Buffers */
    socket_buffer_t* send_buf; /* Send buffer */
    socket_buffer_t* recv_buf; /* Receive buffer */
    uint32_t send_buf_size;    /* Send buffer size */
    uint32_t recv_buf_size;    /* Receive buffer size */
    
    /* Connection management */
    struct socket* parent;     /* Parent socket (for accepted connections) */
    struct socket* accept_queue; /* Queue of pending connections */
    uint32_t backlog;          /* Listen backlog */
    
    /* Timeouts */
    uint32_t send_timeout;     /* Send timeout (ms) */
    uint32_t recv_timeout;     /* Receive timeout (ms) */
    
    /* Protocol-specific data */
    void* protocol_data;       /* TCP/UDP specific data */
    
    /* List management */
    struct socket* next;       /* Next socket in list */
    struct socket* hash_next;  /* Next in hash table */
} socket_t;

/* Socket Hash Table */
#define SOCKET_HASH_SIZE       256
typedef struct {
    socket_t* buckets[SOCKET_HASH_SIZE];
    uint32_t count;
    uint32_t next_fd;
} socket_table_t;

/* ================================
 * Socket API Functions
 * ================================ */

/* Basic Socket Operations */
int socket(int domain, int type, int protocol);
int bind(int sockfd, const sockaddr_t* addr, uint32_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, sockaddr_t* addr, uint32_t* addrlen);
int connect(int sockfd, const sockaddr_t* addr, uint32_t addrlen);

/* Data Transfer */
int send(int sockfd, const void* buf, size_t len, int flags);
int recv(int sockfd, void* buf, size_t len, int flags);
int sendto(int sockfd, const void* buf, size_t len, int flags,
           const sockaddr_t* dest_addr, uint32_t addrlen);
int recvfrom(int sockfd, void* buf, size_t len, int flags,
             sockaddr_t* src_addr, uint32_t* addrlen);

/* Socket Control */
int close(int sockfd);
int shutdown(int sockfd, int how);
int setsockopt(int sockfd, int level, int optname, const void* optval, uint32_t optlen);
int getsockopt(int sockfd, int level, int optname, void* optval, uint32_t* optlen);

/* Socket Information */
int getsockname(int sockfd, sockaddr_t* addr, uint32_t* addrlen);
int getpeername(int sockfd, sockaddr_t* addr, uint32_t* addrlen);

/* ================================
 * Socket Management Functions
 * ================================ */

/* Socket Table Management */
int socket_table_init(void);
void socket_table_cleanup(void);
socket_t* socket_alloc(int domain, int type, int protocol);
void socket_free(socket_t* sock);
socket_t* socket_find_by_fd(int fd);
socket_t* socket_find_by_addr(const sockaddr_in_t* local, const sockaddr_in_t* remote);

/* Socket State Management */
int socket_set_state(socket_t* sock, socket_state_t state);
socket_state_t socket_get_state(socket_t* sock);
bool socket_is_connected(socket_t* sock);
bool socket_is_listening(socket_t* sock);

/* Socket Buffer Management */
int socket_buffer_alloc(socket_buffer_t** buf, uint32_t size);
void socket_buffer_free(socket_buffer_t* buf);
int socket_buffer_put(socket_buffer_t* buf, const void* data, uint32_t len);
int socket_buffer_get(socket_buffer_t* buf, void* data, uint32_t len);
uint32_t socket_buffer_available(socket_buffer_t* buf);
uint32_t socket_buffer_space(socket_buffer_t* buf);

/* ================================
 * Socket Address Utilities
 * ================================ */

/* Address Conversion */
int sockaddr_in_from_ip_port(sockaddr_in_t* addr, ip_addr_t ip, uint16_t port);
int sockaddr_in_to_ip_port(const sockaddr_in_t* addr, ip_addr_t* ip, uint16_t* port);
int sockaddr_from_string(sockaddr_in_t* addr, const char* ip_str, uint16_t port);
char* sockaddr_to_string(const sockaddr_in_t* addr, char* buf, size_t len);

/* Address Comparison */
bool sockaddr_equal(const sockaddr_in_t* addr1, const sockaddr_in_t* addr2);
bool sockaddr_addr_equal(const sockaddr_in_t* addr1, const sockaddr_in_t* addr2);
bool sockaddr_port_equal(const sockaddr_in_t* addr1, const sockaddr_in_t* addr2);

/* ================================
 * Socket Protocol Interface
 * ================================ */

/* Protocol Operations */
typedef struct socket_proto_ops {
    int (*bind)(socket_t* sock, const sockaddr_in_t* addr);
    int (*connect)(socket_t* sock, const sockaddr_in_t* addr);
    int (*listen)(socket_t* sock, int backlog);
    int (*accept)(socket_t* sock, socket_t* new_sock);
    int (*send)(socket_t* sock, const void* data, size_t len, int flags);
    int (*recv)(socket_t* sock, void* data, size_t len, int flags);
    int (*close)(socket_t* sock);
    int (*shutdown)(socket_t* sock, int how);
    int (*setsockopt)(socket_t* sock, int level, int optname, 
                      const void* optval, uint32_t optlen);
    int (*getsockopt)(socket_t* sock, int level, int optname, 
                      void* optval, uint32_t* optlen);
} socket_proto_ops_t;

/* Protocol Registration */
int socket_register_protocol(int domain, int type, int protocol, 
                            const socket_proto_ops_t* ops);
socket_proto_ops_t* socket_get_protocol_ops(int domain, int type, int protocol);

/* ================================
 * Socket Statistics
 * ================================ */

typedef struct {
    uint64_t sockets_created;     /* Total sockets created */
    uint64_t sockets_destroyed;   /* Total sockets destroyed */
    uint32_t sockets_active;      /* Currently active sockets */
    uint32_t sockets_tcp;         /* Active TCP sockets */
    uint32_t sockets_udp;         /* Active UDP sockets */
    uint32_t sockets_raw;         /* Active raw sockets */
    uint64_t bytes_sent;          /* Total bytes sent */
    uint64_t bytes_received;      /* Total bytes received */
    uint64_t packets_sent;        /* Total packets sent */
    uint64_t packets_received;    /* Total packets received */
    uint64_t errors;              /* Socket errors */
} socket_stats_t;

socket_stats_t* socket_get_stats(void);
void socket_reset_stats(void);
void socket_print_stats(void);

/* ================================
 * Socket Debugging
 * ================================ */

/* Socket Debugging */
void socket_dump(const socket_t* sock);
void socket_dump_all(void);
void socket_print_info(const socket_t* sock);
void socket_table_dump(void);

/* Buffer Debugging */
void socket_buffer_dump(const socket_buffer_t* buf);

/* ================================
 * Socket Initialization
 * ================================ */

/* Socket subsystem initialization */
int socket_init(void);
void socket_shutdown(void);

/* ================================
 * Internal Socket Functions
 * ================================ */

/* Socket Hash Functions */
uint32_t socket_hash_addr(const sockaddr_in_t* local, const sockaddr_in_t* remote);
int socket_hash_insert(socket_t* sock);
void socket_hash_remove(socket_t* sock);

/* Socket Notification */
int socket_notify_data_available(socket_t* sock);
int socket_notify_space_available(socket_t* sock);
int socket_notify_connection(socket_t* sock);
int socket_notify_error(socket_t* sock, int error);

#endif /* SOCKET_H */
