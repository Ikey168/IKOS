/* IKOS Network Stack - UDP Protocol Implementation
 * Issue #44 - TCP/IP Protocol Implementation
 * 
 * Implements UDP (User Datagram Protocol) for connectionless communication
 * RFC 768 - User Datagram Protocol
 */

#ifndef UDP_H
#define UDP_H

#include <stdint.h>
#include <stdbool.h>
#include "network.h"
#include "ip.h"
#include "socket.h"

/* ================================
 * UDP Constants
 * ================================ */

#define UDP_HEADER_SIZE        8       /* UDP header size in bytes */
#define UDP_MAX_PAYLOAD        65507   /* Max UDP payload (65535 - IP header - UDP header) */
#define UDP_MIN_PORT          1        /* Minimum port number */
#define UDP_MAX_PORT          65535    /* Maximum port number */
#define UDP_EPHEMERAL_MIN     49152    /* Start of ephemeral port range */
#define UDP_EPHEMERAL_MAX     65535    /* End of ephemeral port range */

/* Well-known UDP ports */
#define UDP_PORT_DNS          53       /* Domain Name System */
#define UDP_PORT_DHCP_SERVER  67       /* DHCP Server */
#define UDP_PORT_DHCP_CLIENT  68       /* DHCP Client */
#define UDP_PORT_TFTP         69       /* Trivial File Transfer Protocol */
#define UDP_PORT_NTP          123      /* Network Time Protocol */
#define UDP_PORT_SNMP         161      /* Simple Network Management Protocol */

/* ================================
 * UDP Header Structure
 * ================================ */

/* UDP Header (RFC 768) */
typedef struct __attribute__((packed)) {
    uint16_t src_port;         /* Source port */
    uint16_t dest_port;        /* Destination port */
    uint16_t length;           /* Length of UDP header and data */
    uint16_t checksum;         /* Checksum (optional, 0 = disabled) */
} udp_header_t;

/* UDP Packet Structure */
typedef struct {
    udp_header_t header;       /* UDP header */
    uint8_t data[];            /* Payload data */
} udp_packet_t;

/* ================================
 * UDP Socket Structure
 * ================================ */

/* UDP Socket Control Block */
typedef struct udp_socket {
    /* Socket identification */
    uint16_t local_port;       /* Local port number */
    uint16_t remote_port;      /* Remote port number (for connected UDP) */
    ip_addr_t local_addr;      /* Local IP address */
    ip_addr_t remote_addr;     /* Remote IP address (for connected UDP) */
    
    /* Socket state */
    bool bound;                /* Socket is bound to local address */
    bool connected;            /* Socket is connected to remote address */
    
    /* Buffer management */
    netbuf_t* recv_queue;      /* Received datagram queue */
    uint32_t recv_queue_size;  /* Current receive queue size */
    uint32_t recv_queue_max;   /* Maximum receive queue size */
    
    /* Socket options */
    bool broadcast_enabled;    /* Allow broadcast transmission */
    bool checksum_enabled;     /* Enable UDP checksum */
    int recv_timeout;          /* Receive timeout in ms (0 = no timeout) */
    
    /* Statistics */
    uint64_t packets_sent;     /* Packets transmitted */
    uint64_t packets_received; /* Packets received */
    uint64_t bytes_sent;       /* Bytes transmitted */
    uint64_t bytes_received;   /* Bytes received */
    uint64_t errors;           /* Error count */
    
    /* List linkage */
    struct udp_socket* next;   /* Next socket in hash chain */
} udp_socket_t;

/* ================================
 * UDP Statistics
 * ================================ */

/* UDP Protocol Statistics */
typedef struct {
    /* Packet counters */
    uint64_t packets_sent;     /* Total packets sent */
    uint64_t packets_received; /* Total packets received */
    uint64_t bytes_sent;       /* Total bytes sent */
    uint64_t bytes_received;   /* Total bytes received */
    
    /* Error counters */
    uint64_t bad_checksum;     /* Packets with bad checksum */
    uint64_t invalid_length;   /* Packets with invalid length */
    uint64_t no_socket;        /* Packets with no matching socket */
    uint64_t buffer_full;      /* Receive buffer overflow */
    uint64_t send_errors;      /* Send errors */
    
    /* Port statistics */
    uint32_t ports_in_use;     /* Number of ports currently in use */
    uint32_t ephemeral_ports;  /* Number of ephemeral ports allocated */
} udp_stats_t;

/* ================================
 * UDP Function Declarations
 * ================================ */

/* Protocol Initialization */
int udp_init(void);
void udp_shutdown(void);

/* Packet Processing */
int udp_receive_packet(netdev_t* dev, netbuf_t* buf);
int udp_send_packet(udp_socket_t* sock, const void* data, size_t len, 
                   ip_addr_t dest_addr, uint16_t dest_port);

/* Socket Operations */
udp_socket_t* udp_socket_create(void);
int udp_socket_bind(udp_socket_t* sock, ip_addr_t addr, uint16_t port);
int udp_socket_connect(udp_socket_t* sock, ip_addr_t addr, uint16_t port);
int udp_socket_send(udp_socket_t* sock, const void* data, size_t len);
int udp_socket_sendto(udp_socket_t* sock, const void* data, size_t len,
                     ip_addr_t dest_addr, uint16_t dest_port);
int udp_socket_recv(udp_socket_t* sock, void* buffer, size_t len);
int udp_socket_recvfrom(udp_socket_t* sock, void* buffer, size_t len,
                       ip_addr_t* src_addr, uint16_t* src_port);
int udp_socket_close(udp_socket_t* sock);

/* Socket Management */
udp_socket_t* udp_find_socket(uint16_t port);
int udp_register_socket(udp_socket_t* sock);
int udp_unregister_socket(udp_socket_t* sock);

/* Port Management */
uint16_t udp_allocate_port(void);
int udp_bind_port(uint16_t port, udp_socket_t* sock);
int udp_release_port(uint16_t port);
bool udp_port_in_use(uint16_t port);

/* Header Operations */
udp_header_t* udp_get_header(netbuf_t* buf);
int udp_build_header(netbuf_t* buf, uint16_t src_port, uint16_t dest_port, uint16_t len);
uint16_t udp_calculate_checksum(const udp_header_t* header, ip_addr_t src_addr, 
                               ip_addr_t dest_addr, const void* data, uint16_t len);
bool udp_verify_checksum(const udp_header_t* header, ip_addr_t src_addr, 
                        ip_addr_t dest_addr, uint16_t len);

/* Socket Options */
int udp_set_socket_option(udp_socket_t* sock, int option, const void* value, size_t len);
int udp_get_socket_option(udp_socket_t* sock, int option, void* value, size_t* len);

/* Utility Functions */
void udp_print_header(const udp_header_t* header);
void udp_dump_packet(const udp_packet_t* packet, size_t len);
void udp_print_stats(void);
void udp_reset_stats(void);
udp_stats_t* udp_get_stats(void);

/* ================================
 * UDP Error Codes
 * ================================ */

#define UDP_SUCCESS            0       /* Operation successful */
#define UDP_ERROR_INVALID_ARG  -1      /* Invalid argument */
#define UDP_ERROR_NO_MEMORY    -2      /* Out of memory */
#define UDP_ERROR_PORT_IN_USE  -3      /* Port already in use */
#define UDP_ERROR_NO_SOCKET    -4      /* No matching socket found */
#define UDP_ERROR_BUFFER_FULL  -5      /* Receive buffer full */
#define UDP_ERROR_SEND_FAILED  -6      /* Send operation failed */
#define UDP_ERROR_NOT_BOUND    -7      /* Socket not bound */
#define UDP_ERROR_NOT_CONNECTED -8     /* Socket not connected */
#define UDP_ERROR_TIMEOUT      -9      /* Operation timed out */
#define UDP_ERROR_CHECKSUM     -10     /* Checksum validation failed */

/* ================================
 * UDP Inline Helper Functions
 * ================================ */

/* Get UDP payload length from header */
static inline uint16_t udp_get_payload_length(const udp_header_t* header) {
    return ntohs(header->length) - UDP_HEADER_SIZE;
}

/* Check if port is in ephemeral range */
static inline bool udp_is_ephemeral_port(uint16_t port) {
    return (port >= UDP_EPHEMERAL_MIN && port <= UDP_EPHEMERAL_MAX);
}

/* Check if port is well-known */
static inline bool udp_is_well_known_port(uint16_t port) {
    return (port > 0 && port < 1024);
}

/* Convert network byte order port to host byte order */
static inline uint16_t udp_ntohs(uint16_t net_port) {
    return ntohs(net_port);
}

/* Convert host byte order port to network byte order */
static inline uint16_t udp_htons(uint16_t host_port) {
    return htons(host_port);
}

#endif /* UDP_H */
