/* IKOS Network Stack - UDP Protocol Implementation
 * Issue #44 - TCP/IP Protocol Implementation
 * 
 * Implements UDP (User Datagram Protocol) for connectionless communication
 * RFC 768 - User Datagram Protocol
 */

#include "udp.h"
#include "ip.h"
#include "network.h"
#include <string.h>
#include <stdlib.h>

/* ================================
 * Global Variables
 * ================================ */

/* UDP socket hash table */
#define UDP_SOCKET_HASH_SIZE 256
static udp_socket_t* udp_socket_table[UDP_SOCKET_HASH_SIZE];

/* UDP port allocation bitmap */
static uint8_t udp_port_bitmap[8192]; /* 65536 ports / 8 bits per byte */

/* UDP statistics */
static udp_stats_t udp_stats;

/* Next ephemeral port to allocate */
static uint16_t next_ephemeral_port = UDP_EPHEMERAL_MIN;

/* ================================
 * Internal Function Declarations
 * ================================ */

static uint32_t udp_hash_socket(uint16_t port);
static void udp_set_port_used(uint16_t port);
static void udp_set_port_free(uint16_t port);
static bool udp_is_port_used(uint16_t port);

/* ================================
 * Protocol Initialization
 * ================================ */

/**
 * Initialize UDP protocol
 */
int udp_init(void) {
    /* Clear socket table */
    memset(udp_socket_table, 0, sizeof(udp_socket_table));
    
    /* Clear port bitmap */
    memset(udp_port_bitmap, 0, sizeof(udp_port_bitmap));
    
    /* Clear statistics */
    memset(&udp_stats, 0, sizeof(udp_stats));
    
    /* Register UDP protocol with IP layer */
    ip_register_protocol(IPPROTO_UDP, udp_receive_packet);
    
    return UDP_SUCCESS;
}

/**
 * Shutdown UDP protocol
 */
void udp_shutdown(void) {
    /* Unregister UDP protocol */
    ip_unregister_protocol(IPPROTO_UDP);
    
    /* Close all sockets */
    for (int i = 0; i < UDP_SOCKET_HASH_SIZE; i++) {
        udp_socket_t* sock = udp_socket_table[i];
        while (sock) {
            udp_socket_t* next = sock->next;
            udp_socket_close(sock);
            sock = next;
        }
    }
    
    /* Clear all data structures */
    memset(udp_socket_table, 0, sizeof(udp_socket_table));
    memset(udp_port_bitmap, 0, sizeof(udp_port_bitmap));
    memset(&udp_stats, 0, sizeof(udp_stats));
}

/* ================================
 * Packet Processing
 * ================================ */

/**
 * Receive UDP packet from IP layer
 */
int udp_receive_packet(netdev_t* dev, netbuf_t* buf) {
    udp_header_t* header;
    udp_socket_t* sock;
    ip_header_t* ip_header;
    uint16_t length, payload_len;
    int result = UDP_SUCCESS;
    
    /* Get IP header for checksum calculation */
    ip_header = ip_get_header(buf);
    if (!ip_header) {
        udp_stats.invalid_length++;
        return UDP_ERROR_INVALID_ARG;
    }
    
    /* Get UDP header */
    header = udp_get_header(buf);
    if (!header) {
        udp_stats.invalid_length++;
        return UDP_ERROR_INVALID_ARG;
    }
    
    /* Validate packet length */
    length = ntohs(header->length);
    if (length < UDP_HEADER_SIZE || length > buf->len) {
        udp_stats.invalid_length++;
        return UDP_ERROR_INVALID_ARG;
    }
    
    /* Verify checksum if enabled */
    if (header->checksum != 0) {
        if (!udp_verify_checksum(header, ip_header->src_addr, ip_header->dest_addr, length)) {
            udp_stats.bad_checksum++;
            return UDP_ERROR_CHECKSUM;
        }
    }
    
    /* Find matching socket */
    sock = udp_find_socket(ntohs(header->dest_port));
    if (!sock) {
        udp_stats.no_socket++;
        return UDP_ERROR_NO_SOCKET;
    }
    
    /* Check receive buffer space */
    if (sock->recv_queue_size >= sock->recv_queue_max) {
        udp_stats.buffer_full++;
        return UDP_ERROR_BUFFER_FULL;
    }
    
    /* Remove UDP header from buffer */
    netbuf_pull(buf, UDP_HEADER_SIZE);
    payload_len = length - UDP_HEADER_SIZE;
    
    /* Add to socket receive queue */
    netbuf_t* recv_buf = netbuf_clone(buf);
    if (!recv_buf) {
        return UDP_ERROR_NO_MEMORY;
    }
    
    /* Set buffer metadata */
    recv_buf->protocol = IPPROTO_UDP;
    recv_buf->src_addr = ip_header->src_addr;
    recv_buf->dest_addr = ip_header->dest_addr;
    recv_buf->src_port = ntohs(header->src_port);
    recv_buf->dest_port = ntohs(header->dest_port);
    
    /* Add to receive queue */
    if (!sock->recv_queue) {
        sock->recv_queue = recv_buf;
    } else {
        netbuf_t* last = sock->recv_queue;
        while (last->next) {
            last = last->next;
        }
        last->next = recv_buf;
    }
    
    sock->recv_queue_size++;
    sock->packets_received++;
    sock->bytes_received += payload_len;
    
    /* Update global statistics */
    udp_stats.packets_received++;
    udp_stats.bytes_received += payload_len;
    
    return result;
}

/**
 * Send UDP packet
 */
int udp_send_packet(udp_socket_t* sock, const void* data, size_t len, 
                   ip_addr_t dest_addr, uint16_t dest_port) {
    netbuf_t* buf;
    udp_header_t* header;
    ip_header_t* ip_header;
    uint16_t total_len;
    uint16_t checksum = 0;
    int result;
    
    if (!sock || !data || len == 0) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    if (len > UDP_MAX_PAYLOAD) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    /* Allocate buffer */
    total_len = UDP_HEADER_SIZE + len;
    buf = netbuf_alloc(total_len);
    if (!buf) {
        return UDP_ERROR_NO_MEMORY;
    }
    
    /* Reserve space for IP header */
    netbuf_reserve(buf, IP_HEADER_MIN_SIZE);
    
    /* Build UDP header */
    header = (udp_header_t*)netbuf_push(buf, UDP_HEADER_SIZE);
    header->src_port = htons(sock->local_port);
    header->dest_port = htons(dest_port);
    header->length = htons(total_len);
    header->checksum = 0; /* Calculate later */
    
    /* Copy payload data */
    memcpy(netbuf_push(buf, len), data, len);
    
    /* Calculate checksum if enabled */
    if (sock->checksum_enabled) {
        checksum = udp_calculate_checksum(header, sock->local_addr, dest_addr, 
                                         netbuf_data(buf) + UDP_HEADER_SIZE, len);
        header->checksum = htons(checksum);
    }
    
    /* Send via IP layer */
    result = ip_send_packet_from(sock->local_addr, dest_addr, IPPROTO_UDP, buf);
    
    if (result == NET_SUCCESS) {
        sock->packets_sent++;
        sock->bytes_sent += len;
        udp_stats.packets_sent++;
        udp_stats.bytes_sent += len;
    } else {
        sock->errors++;
        udp_stats.send_errors++;
    }
    
    netbuf_free(buf);
    return (result == NET_SUCCESS) ? UDP_SUCCESS : UDP_ERROR_SEND_FAILED;
}

/* ================================
 * Socket Operations
 * ================================ */

/**
 * Create UDP socket
 */
udp_socket_t* udp_socket_create(void) {
    udp_socket_t* sock = malloc(sizeof(udp_socket_t));
    if (!sock) {
        return NULL;
    }
    
    /* Initialize socket */
    memset(sock, 0, sizeof(udp_socket_t));
    sock->recv_queue_max = 32; /* Default receive queue size */
    sock->checksum_enabled = true;
    
    return sock;
}

/**
 * Bind UDP socket to local address and port
 */
int udp_socket_bind(udp_socket_t* sock, ip_addr_t addr, uint16_t port) {
    if (!sock) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    if (sock->bound) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    /* Allocate port if not specified */
    if (port == 0) {
        port = udp_allocate_port();
        if (port == 0) {
            return UDP_ERROR_NO_MEMORY;
        }
    } else {
        /* Check if port is available */
        if (udp_port_in_use(port)) {
            return UDP_ERROR_PORT_IN_USE;
        }
        udp_bind_port(port, sock);
    }
    
    sock->local_addr = addr;
    sock->local_port = port;
    sock->bound = true;
    
    /* Register socket */
    return udp_register_socket(sock);
}

/**
 * Connect UDP socket to remote address (optional for UDP)
 */
int udp_socket_connect(udp_socket_t* sock, ip_addr_t addr, uint16_t port) {
    if (!sock || port == 0) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    sock->remote_addr = addr;
    sock->remote_port = port;
    sock->connected = true;
    
    return UDP_SUCCESS;
}

/**
 * Send data on connected UDP socket
 */
int udp_socket_send(udp_socket_t* sock, const void* data, size_t len) {
    if (!sock || !data) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    if (!sock->connected) {
        return UDP_ERROR_NOT_CONNECTED;
    }
    
    return udp_send_packet(sock, data, len, sock->remote_addr, sock->remote_port);
}

/**
 * Send data to specific destination
 */
int udp_socket_sendto(udp_socket_t* sock, const void* data, size_t len,
                     ip_addr_t dest_addr, uint16_t dest_port) {
    if (!sock || !data) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    if (!sock->bound) {
        return UDP_ERROR_NOT_BOUND;
    }
    
    return udp_send_packet(sock, data, len, dest_addr, dest_port);
}

/**
 * Receive data from connected UDP socket
 */
int udp_socket_recv(udp_socket_t* sock, void* buffer, size_t len) {
    netbuf_t* buf;
    size_t copy_len;
    
    if (!sock || !buffer) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    if (!sock->recv_queue) {
        return UDP_ERROR_WOULD_BLOCK; /* No data available */
    }
    
    /* Get first buffer from queue */
    buf = sock->recv_queue;
    sock->recv_queue = buf->next;
    sock->recv_queue_size--;
    
    /* Copy data to user buffer */
    copy_len = (buf->len < len) ? buf->len : len;
    memcpy(buffer, netbuf_data(buf), copy_len);
    
    netbuf_free(buf);
    return copy_len;
}

/**
 * Receive data with source address information
 */
int udp_socket_recvfrom(udp_socket_t* sock, void* buffer, size_t len,
                       ip_addr_t* src_addr, uint16_t* src_port) {
    netbuf_t* buf;
    size_t copy_len;
    
    if (!sock || !buffer) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    if (!sock->recv_queue) {
        return UDP_ERROR_WOULD_BLOCK; /* No data available */
    }
    
    /* Get first buffer from queue */
    buf = sock->recv_queue;
    sock->recv_queue = buf->next;
    sock->recv_queue_size--;
    
    /* Copy data to user buffer */
    copy_len = (buf->len < len) ? buf->len : len;
    memcpy(buffer, netbuf_data(buf), copy_len);
    
    /* Return source address information */
    if (src_addr) {
        *src_addr = buf->src_addr;
    }
    if (src_port) {
        *src_port = buf->src_port;
    }
    
    netbuf_free(buf);
    return copy_len;
}

/**
 * Close UDP socket
 */
int udp_socket_close(udp_socket_t* sock) {
    if (!sock) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    /* Unregister socket */
    udp_unregister_socket(sock);
    
    /* Release port */
    if (sock->bound) {
        udp_release_port(sock->local_port);
    }
    
    /* Free receive queue */
    netbuf_t* buf = sock->recv_queue;
    while (buf) {
        netbuf_t* next = buf->next;
        netbuf_free(buf);
        buf = next;
    }
    
    /* Free socket */
    free(sock);
    return UDP_SUCCESS;
}

/* ================================
 * Socket Management
 * ================================ */

/**
 * Find UDP socket by port
 */
udp_socket_t* udp_find_socket(uint16_t port) {
    uint32_t hash = udp_hash_socket(port);
    udp_socket_t* sock = udp_socket_table[hash];
    
    while (sock) {
        if (sock->local_port == port) {
            return sock;
        }
        sock = sock->next;
    }
    
    return NULL;
}

/**
 * Register UDP socket in hash table
 */
int udp_register_socket(udp_socket_t* sock) {
    if (!sock || !sock->bound) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    uint32_t hash = udp_hash_socket(sock->local_port);
    sock->next = udp_socket_table[hash];
    udp_socket_table[hash] = sock;
    
    return UDP_SUCCESS;
}

/**
 * Unregister UDP socket from hash table
 */
int udp_unregister_socket(udp_socket_t* sock) {
    if (!sock || !sock->bound) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    uint32_t hash = udp_hash_socket(sock->local_port);
    udp_socket_t* current = udp_socket_table[hash];
    udp_socket_t* prev = NULL;
    
    while (current) {
        if (current == sock) {
            if (prev) {
                prev->next = current->next;
            } else {
                udp_socket_table[hash] = current->next;
            }
            current->next = NULL;
            return UDP_SUCCESS;
        }
        prev = current;
        current = current->next;
    }
    
    return UDP_ERROR_NO_SOCKET;
}

/* ================================
 * Port Management
 * ================================ */

/**
 * Allocate ephemeral port
 */
uint16_t udp_allocate_port(void) {
    uint16_t start_port = next_ephemeral_port;
    
    do {
        if (!udp_is_port_used(next_ephemeral_port)) {
            uint16_t port = next_ephemeral_port;
            udp_set_port_used(port);
            
            /* Update next ephemeral port */
            next_ephemeral_port++;
            if (next_ephemeral_port > UDP_EPHEMERAL_MAX) {
                next_ephemeral_port = UDP_EPHEMERAL_MIN;
            }
            
            udp_stats.ephemeral_ports++;
            return port;
        }
        
        next_ephemeral_port++;
        if (next_ephemeral_port > UDP_EPHEMERAL_MAX) {
            next_ephemeral_port = UDP_EPHEMERAL_MIN;
        }
    } while (next_ephemeral_port != start_port);
    
    return 0; /* No ports available */
}

/**
 * Bind port to socket
 */
int udp_bind_port(uint16_t port, udp_socket_t* sock) {
    if (port == 0 || port > UDP_MAX_PORT || !sock) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    if (udp_is_port_used(port)) {
        return UDP_ERROR_PORT_IN_USE;
    }
    
    udp_set_port_used(port);
    udp_stats.ports_in_use++;
    
    return UDP_SUCCESS;
}

/**
 * Release port
 */
int udp_release_port(uint16_t port) {
    if (port == 0 || port > UDP_MAX_PORT) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    if (!udp_is_port_used(port)) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    udp_set_port_free(port);
    udp_stats.ports_in_use--;
    
    if (udp_is_ephemeral_port(port)) {
        udp_stats.ephemeral_ports--;
    }
    
    return UDP_SUCCESS;
}

/**
 * Check if port is in use
 */
bool udp_port_in_use(uint16_t port) {
    return udp_is_port_used(port);
}

/* ================================
 * Header Operations
 * ================================ */

/**
 * Get UDP header from buffer
 */
udp_header_t* udp_get_header(netbuf_t* buf) {
    if (!buf || buf->len < UDP_HEADER_SIZE) {
        return NULL;
    }
    
    return (udp_header_t*)netbuf_data(buf);
}

/**
 * Build UDP header
 */
int udp_build_header(netbuf_t* buf, uint16_t src_port, uint16_t dest_port, uint16_t len) {
    udp_header_t* header;
    
    if (!buf) {
        return UDP_ERROR_INVALID_ARG;
    }
    
    header = (udp_header_t*)netbuf_push(buf, UDP_HEADER_SIZE);
    if (!header) {
        return UDP_ERROR_NO_MEMORY;
    }
    
    header->src_port = htons(src_port);
    header->dest_port = htons(dest_port);
    header->length = htons(len);
    header->checksum = 0; /* To be calculated later */
    
    return UDP_SUCCESS;
}

/**
 * Calculate UDP checksum
 */
uint16_t udp_calculate_checksum(const udp_header_t* header, ip_addr_t src_addr, 
                               ip_addr_t dest_addr, const void* data, uint16_t len) {
    uint32_t sum = 0;
    uint16_t* ptr;
    
    /* Pseudo header */
    sum += (src_addr.addr >> 16) & 0xFFFF;
    sum += src_addr.addr & 0xFFFF;
    sum += (dest_addr.addr >> 16) & 0xFFFF;
    sum += dest_addr.addr & 0xFFFF;
    sum += htons(IPPROTO_UDP);
    sum += header->length;
    
    /* UDP header */
    ptr = (uint16_t*)header;
    sum += ptr[0]; /* src_port */
    sum += ptr[1]; /* dest_port */
    sum += ptr[2]; /* length */
    /* Skip checksum field (ptr[3]) */
    
    /* Data */
    ptr = (uint16_t*)data;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    
    /* Handle odd byte */
    if (len == 1) {
        sum += (*((uint8_t*)ptr)) << 8;
    }
    
    /* Fold 32-bit sum to 16 bits */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

/**
 * Verify UDP checksum
 */
bool udp_verify_checksum(const udp_header_t* header, ip_addr_t src_addr, 
                        ip_addr_t dest_addr, uint16_t len) {
    uint16_t calc_checksum;
    uint16_t orig_checksum = header->checksum;
    
    /* Calculate checksum with original checksum field set to 0 */
    ((udp_header_t*)header)->checksum = 0;
    calc_checksum = udp_calculate_checksum(header, src_addr, dest_addr, 
                                          (uint8_t*)header + UDP_HEADER_SIZE, 
                                          len - UDP_HEADER_SIZE);
    ((udp_header_t*)header)->checksum = orig_checksum;
    
    return (calc_checksum == ntohs(orig_checksum));
}

/* ================================
 * Utility Functions
 * ================================ */

/**
 * Print UDP header
 */
void udp_print_header(const udp_header_t* header) {
    if (!header) return;
    
    printf("UDP Header:\n");
    printf("  Source Port: %u\n", ntohs(header->src_port));
    printf("  Dest Port: %u\n", ntohs(header->dest_port));
    printf("  Length: %u\n", ntohs(header->length));
    printf("  Checksum: 0x%04x\n", ntohs(header->checksum));
}

/**
 * Print UDP statistics
 */
void udp_print_stats(void) {
    printf("UDP Statistics:\n");
    printf("  Packets sent: %llu\n", udp_stats.packets_sent);
    printf("  Packets received: %llu\n", udp_stats.packets_received);
    printf("  Bytes sent: %llu\n", udp_stats.bytes_sent);
    printf("  Bytes received: %llu\n", udp_stats.bytes_received);
    printf("  Bad checksum: %llu\n", udp_stats.bad_checksum);
    printf("  Invalid length: %llu\n", udp_stats.invalid_length);
    printf("  No socket: %llu\n", udp_stats.no_socket);
    printf("  Buffer full: %llu\n", udp_stats.buffer_full);
    printf("  Send errors: %llu\n", udp_stats.send_errors);
    printf("  Ports in use: %u\n", udp_stats.ports_in_use);
    printf("  Ephemeral ports: %u\n", udp_stats.ephemeral_ports);
}

/**
 * Reset UDP statistics
 */
void udp_reset_stats(void) {
    memset(&udp_stats, 0, sizeof(udp_stats));
}

/**
 * Get UDP statistics
 */
udp_stats_t* udp_get_stats(void) {
    return &udp_stats;
}

/* ================================
 * Internal Helper Functions
 * ================================ */

/**
 * Hash function for socket table
 */
static uint32_t udp_hash_socket(uint16_t port) {
    return port % UDP_SOCKET_HASH_SIZE;
}

/**
 * Mark port as used in bitmap
 */
static void udp_set_port_used(uint16_t port) {
    uint32_t byte_index = port / 8;
    uint8_t bit_index = port % 8;
    udp_port_bitmap[byte_index] |= (1 << bit_index);
}

/**
 * Mark port as free in bitmap
 */
static void udp_set_port_free(uint16_t port) {
    uint32_t byte_index = port / 8;
    uint8_t bit_index = port % 8;
    udp_port_bitmap[byte_index] &= ~(1 << bit_index);
}

/**
 * Check if port is used in bitmap
 */
static bool udp_is_port_used(uint16_t port) {
    uint32_t byte_index = port / 8;
    uint8_t bit_index = port % 8;
    return (udp_port_bitmap[byte_index] & (1 << bit_index)) != 0;
}
