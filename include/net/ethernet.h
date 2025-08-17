/* IKOS Network Stack - Ethernet Protocol
 * Issue #35 - Network Stack Implementation
 * 
 * Defines Ethernet frame structure and processing functions
 * for Layer 2 network communication.
 */

#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>
#include <stdbool.h>
#include "network.h"

/* ================================
 * Ethernet Constants
 * ================================ */

#define ETH_FRAME_MIN_SIZE     60      /* Minimum frame size */
#define ETH_FRAME_MAX_SIZE     1518    /* Maximum frame size */
#define ETH_HEADER_SIZE        14      /* Ethernet header size */
#define ETH_FCS_SIZE           4       /* Frame Check Sequence size */
#define ETH_PAYLOAD_MIN        46      /* Minimum payload size */
#define ETH_PAYLOAD_MAX        1500    /* Maximum payload size (MTU) */

/* Ethernet Types (EtherType field) */
#define ETH_TYPE_IP            0x0800  /* IPv4 */
#define ETH_TYPE_ARP           0x0806  /* ARP */
#define ETH_TYPE_RARP          0x8035  /* RARP */
#define ETH_TYPE_IPV6          0x86DD  /* IPv6 */
#define ETH_TYPE_VLAN          0x8100  /* VLAN */

/* ================================
 * Ethernet Frame Structure
 * ================================ */

/* Ethernet Header */
typedef struct __attribute__((packed)) {
    eth_addr_t dest;           /* Destination MAC address */
    eth_addr_t src;            /* Source MAC address */
    uint16_t type;             /* EtherType / Length */
} eth_header_t;

/* Complete Ethernet Frame */
typedef struct __attribute__((packed)) {
    eth_header_t header;       /* Ethernet header */
    uint8_t payload[];         /* Payload data */
} eth_frame_t;

/* VLAN Header (optional) */
typedef struct __attribute__((packed)) {
    uint16_t tci;              /* Tag Control Information */
    uint16_t type;             /* EtherType */
} vlan_header_t;

/* ================================
 * Ethernet Processing Functions
 * ================================ */

/* Frame Processing */
int eth_receive_frame(netdev_t* dev, netbuf_t* buf);
int eth_send_frame(netdev_t* dev, const eth_addr_t* dest, 
                   uint16_t type, netbuf_t* buf);
int eth_send_packet(netdev_t* dev, const eth_addr_t* dest, 
                    uint16_t type, const void* data, uint32_t len);

/* Frame Validation */
bool eth_frame_valid(const eth_frame_t* frame, uint32_t len);
bool eth_addr_valid(const eth_addr_t* addr);
bool eth_addr_is_broadcast(const eth_addr_t* addr);
bool eth_addr_is_multicast(const eth_addr_t* addr);
bool eth_addr_is_unicast(const eth_addr_t* addr);

/* Address Operations */
int eth_addr_compare(const eth_addr_t* addr1, const eth_addr_t* addr2);
void eth_addr_copy(eth_addr_t* dest, const eth_addr_t* src);
void eth_addr_set_broadcast(eth_addr_t* addr);
void eth_addr_set_zero(eth_addr_t* addr);

/* Frame Utilities */
uint32_t eth_frame_size(uint32_t payload_len);
uint32_t eth_payload_size(uint32_t frame_len);
eth_header_t* eth_get_header(netbuf_t* buf);
void* eth_get_payload(netbuf_t* buf);
uint16_t eth_get_type(const eth_header_t* header);

/* Statistics and Monitoring */
typedef struct {
    uint64_t frames_received;   /* Total frames received */
    uint64_t frames_sent;       /* Total frames sent */
    uint64_t bytes_received;    /* Total bytes received */
    uint64_t bytes_sent;        /* Total bytes sent */
    uint64_t errors_crc;        /* CRC errors */
    uint64_t errors_length;     /* Length errors */
    uint64_t errors_alignment;  /* Alignment errors */
    uint64_t dropped_frames;    /* Dropped frames */
    uint64_t broadcast_frames;  /* Broadcast frames */
    uint64_t multicast_frames;  /* Multicast frames */
    uint64_t unicast_frames;    /* Unicast frames */
} eth_stats_t;

/* Get Ethernet statistics */
eth_stats_t* eth_get_stats(netdev_t* dev);
void eth_reset_stats(netdev_t* dev);
void eth_print_stats(netdev_t* dev);

/* ================================
 * Ethernet Address Utilities
 * ================================ */

/* Standard Ethernet Addresses */
extern const eth_addr_t eth_addr_broadcast;
extern const eth_addr_t eth_addr_zero;

/* Address String Conversion */
int eth_addr_from_string(const char* str, eth_addr_t* addr);
char* eth_addr_to_string(const eth_addr_t* addr, char* buf, size_t len);
void eth_addr_print(const eth_addr_t* addr);

/* Address Generation */
void eth_addr_random(eth_addr_t* addr);
void eth_addr_from_serial(eth_addr_t* addr, uint32_t serial);

/* ================================
 * Ethernet Configuration
 * ================================ */

/* Ethernet Interface Configuration */
typedef struct {
    eth_addr_t mac_addr;       /* MAC address */
    uint32_t mtu;              /* Maximum Transmission Unit */
    bool promiscuous;          /* Promiscuous mode */
    bool broadcast;            /* Accept broadcast frames */
    bool multicast;            /* Accept multicast frames */
    uint32_t rx_buffer_size;   /* Receive buffer size */
    uint32_t tx_buffer_size;   /* Transmit buffer size */
} eth_config_t;

/* Configuration Functions */
int eth_configure_interface(netdev_t* dev, const eth_config_t* config);
int eth_get_configuration(netdev_t* dev, eth_config_t* config);
int eth_set_mac_address(netdev_t* dev, const eth_addr_t* addr);
int eth_set_mtu(netdev_t* dev, uint32_t mtu);
int eth_set_promiscuous(netdev_t* dev, bool enable);

/* ================================
 * Ethernet Debugging
 * ================================ */

/* Frame Debugging */
void eth_dump_frame(const eth_frame_t* frame, uint32_t len);
void eth_dump_header(const eth_header_t* header);
void eth_print_frame_info(const eth_frame_t* frame, uint32_t len);

/* Address Debugging */
void eth_dump_addr(const eth_addr_t* addr, const char* label);

/* ================================
 * Ethernet Protocol Registration
 * ================================ */

/* Protocol Handler Function Type */
typedef int (*eth_protocol_handler_t)(netdev_t* dev, netbuf_t* buf);

/* Protocol Registration */
int eth_register_protocol(uint16_t type, eth_protocol_handler_t handler);
int eth_unregister_protocol(uint16_t type);
eth_protocol_handler_t eth_get_protocol_handler(uint16_t type);

/* ================================
 * Ethernet Initialization
 * ================================ */

/* Ethernet subsystem initialization */
int ethernet_init(void);
void ethernet_shutdown(void);

/* Per-device Ethernet initialization */
int eth_device_init(netdev_t* dev);
void eth_device_cleanup(netdev_t* dev);

#endif /* ETHERNET_H */
