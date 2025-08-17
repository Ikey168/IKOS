/* IKOS Network Stack - IPv4 Protocol
 * Issue #35 - Network Stack Implementation
 * 
 * Defines IPv4 header structure, routing, and packet processing
 * for Layer 3 network communication.
 */

#ifndef IP_H
#define IP_H

#include <stdint.h>
#include <stdbool.h>
#include "network.h"

/* ================================
 * IPv4 Constants
 * ================================ */

#define IP_VERSION             4       /* IPv4 version */
#define IP_HEADER_MIN_SIZE     20      /* Minimum header size */
#define IP_HEADER_MAX_SIZE     60      /* Maximum header size */
#define IP_MAX_PACKET_SIZE     65535   /* Maximum packet size */
#define IP_DEFAULT_TTL         64      /* Default Time To Live */
#define IP_MAX_ROUTES          256     /* Maximum routing table entries */

/* IPv4 Protocol Numbers */
#define IP_PROTO_ICMP          1       /* ICMP */
#define IP_PROTO_TCP           6       /* TCP */
#define IP_PROTO_UDP           17      /* UDP */

/* IPv4 Flags */
#define IP_FLAG_RESERVED       0x8000  /* Reserved flag */
#define IP_FLAG_DONT_FRAGMENT  0x4000  /* Don't fragment flag */
#define IP_FLAG_MORE_FRAGMENTS 0x2000  /* More fragments flag */
#define IP_FRAGMENT_OFFSET_MASK 0x1FFF /* Fragment offset mask */

/* IPv4 Type of Service (ToS) */
#define IP_TOS_LOWDELAY        0x10    /* Low delay */
#define IP_TOS_THROUGHPUT      0x08    /* High throughput */
#define IP_TOS_RELIABILITY     0x04    /* High reliability */
#define IP_TOS_LOWCOST         0x02    /* Low cost */

/* ================================
 * IPv4 Header Structure
 * ================================ */

/* IPv4 Header */
typedef struct __attribute__((packed)) {
    uint8_t version_ihl;       /* Version (4 bits) + IHL (4 bits) */
    uint8_t tos;               /* Type of Service */
    uint16_t total_length;     /* Total length */
    uint16_t identification;   /* Identification */
    uint16_t flags_fragment;   /* Flags (3 bits) + Fragment offset (13 bits) */
    uint8_t ttl;               /* Time to Live */
    uint8_t protocol;          /* Protocol */
    uint16_t checksum;         /* Header checksum */
    ip_addr_t src_addr;        /* Source address */
    ip_addr_t dest_addr;       /* Destination address */
    uint8_t options[];         /* Options (variable length) */
} ip_header_t;

/* IPv4 Packet Structure */
typedef struct {
    ip_header_t header;        /* IPv4 header */
    uint8_t payload[];         /* Payload data */
} ip_packet_t;

/* ================================
 * IPv4 Routing
 * ================================ */

/* Route Types */
typedef enum {
    IP_ROUTE_DIRECT = 1,       /* Direct route (same subnet) */
    IP_ROUTE_INDIRECT = 2,     /* Indirect route (via gateway) */
    IP_ROUTE_DEFAULT = 3       /* Default route */
} ip_route_type_t;

/* Routing Table Entry */
typedef struct ip_route {
    ip_addr_t destination;     /* Destination network */
    ip_addr_t netmask;         /* Network mask */
    ip_addr_t gateway;         /* Gateway address */
    netdev_t* interface;       /* Output interface */
    ip_route_type_t type;      /* Route type */
    uint32_t metric;           /* Route metric */
    uint32_t flags;            /* Route flags */
    struct ip_route* next;     /* Next route in table */
} ip_route_t;

/* Routing Table */
typedef struct {
    ip_route_t* routes;        /* Route list */
    uint32_t count;            /* Number of routes */
    ip_route_t* default_route; /* Default route */
} ip_routing_table_t;

/* ================================
 * IPv4 Fragmentation
 * ================================ */

/* Fragment Information */
typedef struct {
    uint16_t id;               /* Identification */
    ip_addr_t src_addr;        /* Source address */
    ip_addr_t dest_addr;       /* Destination address */
    uint8_t protocol;          /* Protocol */
    uint32_t total_length;     /* Expected total length */
    uint32_t received_length;  /* Received length so far */
    netbuf_t* fragments;       /* Fragment list */
    uint32_t timer;            /* Reassembly timer */
    struct ip_fragment* next;  /* Next fragment entry */
} ip_fragment_t;

/* Fragmentation Table */
typedef struct {
    ip_fragment_t* fragments;  /* Fragment list */
    uint32_t count;            /* Number of fragment entries */
    uint32_t timeout;          /* Fragment timeout (seconds) */
} ip_fragment_table_t;

/* ================================
 * IPv4 Statistics
 * ================================ */

typedef struct {
    uint64_t packets_received;     /* Total packets received */
    uint64_t packets_sent;         /* Total packets sent */
    uint64_t bytes_received;       /* Total bytes received */
    uint64_t bytes_sent;           /* Total bytes sent */
    uint64_t packets_forwarded;    /* Packets forwarded */
    uint64_t packets_dropped;      /* Packets dropped */
    uint64_t header_errors;        /* Header errors */
    uint64_t checksum_errors;      /* Checksum errors */
    uint64_t ttl_exceeded;         /* TTL exceeded */
    uint64_t fragments_created;    /* Fragments created */
    uint64_t fragments_received;   /* Fragments received */
    uint64_t fragments_reassembled; /* Fragments reassembled */
    uint64_t fragments_failed;     /* Fragment reassembly failures */
    uint64_t no_routes;            /* No route to destination */
} ip_stats_t;

/* ================================
 * IPv4 Processing Functions
 * ================================ */

/* Packet Processing */
int ip_receive_packet(netdev_t* dev, netbuf_t* buf);
int ip_send_packet(ip_addr_t dest, uint8_t protocol, netbuf_t* buf);
int ip_send_packet_from(ip_addr_t src, ip_addr_t dest, uint8_t protocol, netbuf_t* buf);
int ip_forward_packet(netbuf_t* buf);

/* Header Operations */
ip_header_t* ip_get_header(netbuf_t* buf);
int ip_build_header(netbuf_t* buf, ip_addr_t src, ip_addr_t dest, 
                    uint8_t protocol, uint16_t len);
bool ip_header_valid(const ip_header_t* header, uint32_t len);
uint16_t ip_calculate_checksum(const ip_header_t* header);
bool ip_verify_checksum(const ip_header_t* header);

/* Address Operations */
bool ip_addr_is_broadcast(ip_addr_t addr, ip_addr_t netmask);
bool ip_addr_is_multicast(ip_addr_t addr);
bool ip_addr_is_loopback(ip_addr_t addr);
bool ip_addr_is_local(ip_addr_t addr);
bool ip_addr_in_subnet(ip_addr_t addr, ip_addr_t network, ip_addr_t netmask);

/* ================================
 * IPv4 Routing Functions
 * ================================ */

/* Routing Table Management */
int ip_routing_init(void);
void ip_routing_cleanup(void);
int ip_route_add(ip_addr_t dest, ip_addr_t netmask, ip_addr_t gateway, 
                 netdev_t* interface, uint32_t metric);
int ip_route_delete(ip_addr_t dest, ip_addr_t netmask);
ip_route_t* ip_route_lookup(ip_addr_t dest);
int ip_route_set_default(ip_addr_t gateway, netdev_t* interface);

/* Route Resolution */
netdev_t* ip_route_output(ip_addr_t dest, ip_addr_t* next_hop);
int ip_get_local_address(netdev_t* dev, ip_addr_t* addr);

/* ================================
 * IPv4 Fragmentation Functions
 * ================================ */

/* Fragmentation */
int ip_fragment_packet(netbuf_t* buf, uint32_t mtu);
bool ip_needs_fragmentation(netbuf_t* buf, uint32_t mtu);

/* Reassembly */
int ip_reassemble_init(void);
void ip_reassemble_cleanup(void);
netbuf_t* ip_reassemble_packet(netbuf_t* buf);
void ip_reassemble_timeout(void);

/* ================================
 * IPv4 Protocol Registration
 * ================================ */

/* Protocol Handler Function Type */
typedef int (*ip_protocol_handler_t)(netdev_t* dev, netbuf_t* buf);

/* Protocol Registration */
int ip_register_protocol(uint8_t protocol, ip_protocol_handler_t handler);
int ip_unregister_protocol(uint8_t protocol);
ip_protocol_handler_t ip_get_protocol_handler(uint8_t protocol);

/* ================================
 * IPv4 Configuration
 * ================================ */

/* IPv4 Interface Configuration */
typedef struct {
    ip_addr_t ip_addr;         /* IP address */
    ip_addr_t netmask;         /* Network mask */
    ip_addr_t gateway;         /* Default gateway */
    ip_addr_t dns_server;      /* DNS server */
    bool forwarding_enabled;   /* IP forwarding */
    uint8_t default_ttl;       /* Default TTL */
} ip_config_t;

/* Configuration Functions */
int ip_configure_interface(netdev_t* dev, const ip_config_t* config);
int ip_get_interface_config(netdev_t* dev, ip_config_t* config);
int ip_set_address(netdev_t* dev, ip_addr_t addr, ip_addr_t netmask);
int ip_set_gateway(netdev_t* dev, ip_addr_t gateway);

/* ================================
 * IPv4 Utilities
 * ================================ */

/* Address String Conversion */
uint32_t ip_addr_from_string(const char* str);
char* ip_addr_to_string(ip_addr_t addr, char* buf, size_t len);
void ip_addr_print(ip_addr_t addr);

/* Header Field Access */
static inline uint8_t ip_get_version(const ip_header_t* header) {
    return (header->version_ihl >> 4) & 0xF;
}

static inline uint8_t ip_get_header_length(const ip_header_t* header) {
    return (header->version_ihl & 0xF) * 4;
}

static inline uint16_t ip_get_flags(const ip_header_t* header) {
    return ntohs(header->flags_fragment) & 0xE000;
}

static inline uint16_t ip_get_fragment_offset(const ip_header_t* header) {
    return (ntohs(header->flags_fragment) & IP_FRAGMENT_OFFSET_MASK) * 8;
}

/* ================================
 * IPv4 Debugging
 * ================================ */

/* Packet Debugging */
void ip_dump_packet(const ip_packet_t* packet, uint32_t len);
void ip_dump_header(const ip_header_t* header);
void ip_print_packet_info(const ip_packet_t* packet, uint32_t len);

/* Routing Table Debugging */
void ip_print_routing_table(void);
void ip_dump_route(const ip_route_t* route);

/* Statistics */
ip_stats_t* ip_get_stats(void);
void ip_reset_stats(void);
void ip_print_stats(void);

/* ================================
 * IPv4 Initialization
 * ================================ */

/* IPv4 subsystem initialization */
int ip_init(void);
void ip_shutdown(void);

#endif /* IP_H */
