/* IKOS Network Stack - Ethernet Layer Implementation
 * Issue #35 - Network Stack Implementation
 * 
 * Implements Ethernet frame processing, address management,
 * and protocol demultiplexing for Layer 2 networking.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "../include/net/ethernet.h"
#include "../include/net/network.h"
#include "../include/net/ip.h"

/* ================================
 * Global Ethernet State
 * ================================ */

static bool ethernet_initialized = false;

/* Protocol handlers */
#define ETH_MAX_PROTOCOLS 16
static struct {
    uint16_t type;
    eth_protocol_handler_t handler;
} eth_protocols[ETH_MAX_PROTOCOLS];
static uint32_t eth_protocol_count = 0;

/* Standard Ethernet addresses */
const eth_addr_t eth_addr_broadcast = ETH_ADDR_BROADCAST;
const eth_addr_t eth_addr_zero = ETH_ADDR_ZERO;

/* Ethernet statistics */
static eth_stats_t eth_global_stats;

/* ================================
 * Ethernet Frame Processing
 * ================================ */

int eth_receive_frame(netdev_t* dev, netbuf_t* buf) {
    if (!dev || !buf || buf->len < ETH_HEADER_SIZE) {
        return NET_ERROR_INVALID;
    }
    
    /* Get Ethernet header */
    eth_header_t* header = (eth_header_t*)buf->data;
    
    /* Validate frame */
    if (!eth_frame_valid((eth_frame_t*)buf->data, buf->len)) {
        eth_global_stats.errors_length++;
        return NET_ERROR_INVALID;
    }
    
    /* Update statistics */
    eth_global_stats.frames_received++;
    eth_global_stats.bytes_received += buf->len;
    
    /* Check destination address */
    if (eth_addr_is_broadcast(&header->dest)) {
        eth_global_stats.broadcast_frames++;
    } else if (eth_addr_is_multicast(&header->dest)) {
        eth_global_stats.multicast_frames++;
    } else {
        eth_global_stats.unicast_frames++;
        
        /* Check if frame is for us */
        if (eth_addr_compare(&header->dest, &dev->hw_addr) != 0) {
            /* Not for us, drop it (unless promiscuous) */
            if (!(dev->flags & NETDEV_FLAG_PROMISC)) {
                eth_global_stats.dropped_frames++;
                return NET_ERROR_INVALID;
            }
        }
    }
    
    /* Get EtherType */
    uint16_t eth_type = ntohs(header->type);
    
    /* Remove Ethernet header */
    if (netbuf_pull(buf, ETH_HEADER_SIZE) != NET_SUCCESS) {
        return NET_ERROR_INVALID;
    }
    
    /* Find protocol handler */
    eth_protocol_handler_t handler = eth_get_protocol_handler(eth_type);
    if (handler) {
        return handler(dev, buf);
    }
    
    printf("No handler for EtherType 0x%04x\n", eth_type);
    return NET_ERROR_INVALID;
}

int eth_send_frame(netdev_t* dev, const eth_addr_t* dest, uint16_t type, netbuf_t* buf) {
    if (!dev || !dest || !buf) {
        return NET_ERROR_INVALID;
    }
    
    /* Check if we have space for Ethernet header */
    if (buf->head < ETH_HEADER_SIZE) {
        return NET_ERROR_INVALID;
    }
    
    /* Add Ethernet header */
    if (netbuf_push(buf, ETH_HEADER_SIZE) != NET_SUCCESS) {
        return NET_ERROR_INVALID;
    }
    
    eth_header_t* header = (eth_header_t*)buf->data;
    eth_addr_copy(&header->dest, dest);
    eth_addr_copy(&header->src, &dev->hw_addr);
    header->type = htons(type);
    
    /* Pad frame if necessary */
    if (buf->len < ETH_FRAME_MIN_SIZE) {
        uint32_t pad_len = ETH_FRAME_MIN_SIZE - buf->len;
        if (netbuf_put(buf, pad_len) != NET_SUCCESS) {
            return NET_ERROR_INVALID;
        }
        /* Zero padding */
        memset(buf->data + buf->len - pad_len, 0, pad_len);
    }
    
    /* Update statistics */
    eth_global_stats.frames_sent++;
    eth_global_stats.bytes_sent += buf->len;
    
    if (eth_addr_is_broadcast(dest)) {
        eth_global_stats.broadcast_frames++;
    } else if (eth_addr_is_multicast(dest)) {
        eth_global_stats.multicast_frames++;
    } else {
        eth_global_stats.unicast_frames++;
    }
    
    /* Transmit frame */
    return netdev_transmit(dev, buf);
}

int eth_send_packet(netdev_t* dev, const eth_addr_t* dest, uint16_t type, 
                    const void* data, uint32_t len) {
    if (!dev || !dest || !data || len == 0) {
        return NET_ERROR_INVALID;
    }
    
    /* Allocate network buffer */
    netbuf_t* buf = netbuf_alloc(len + ETH_HEADER_SIZE);
    if (!buf) {
        return NET_ERROR_NOMEM;
    }
    
    /* Reserve space for Ethernet header */
    netbuf_reserve(buf, ETH_HEADER_SIZE);
    
    /* Copy data */
    memcpy(buf->data, data, len);
    netbuf_put(buf, len);
    
    /* Send frame */
    int result = eth_send_frame(dev, dest, type, buf);
    
    /* Free buffer */
    netbuf_free(buf);
    
    return result;
}

/* ================================
 * Frame Validation
 * ================================ */

bool eth_frame_valid(const eth_frame_t* frame, uint32_t len) {
    if (!frame || len < ETH_FRAME_MIN_SIZE || len > ETH_FRAME_MAX_SIZE) {
        return false;
    }
    
    /* Check for valid destination address */
    if (!eth_addr_valid(&frame->header.dest)) {
        return false;
    }
    
    /* Check for valid source address */
    if (!eth_addr_valid(&frame->header.src)) {
        return false;
    }
    
    /* Check EtherType */
    uint16_t eth_type = ntohs(frame->header.type);
    if (eth_type < 0x0600 && eth_type != len - ETH_HEADER_SIZE) {
        /* Length field doesn't match actual length */
        return false;
    }
    
    return true;
}

bool eth_addr_valid(const eth_addr_t* addr) {
    if (!addr) {
        return false;
    }
    
    /* Check for all zeros (invalid) */
    static const eth_addr_t zero_addr = ETH_ADDR_ZERO;
    if (eth_addr_compare(addr, &zero_addr) == 0) {
        return false;
    }
    
    return true;
}

bool eth_addr_is_broadcast(const eth_addr_t* addr) {
    if (!addr) {
        return false;
    }
    
    return eth_addr_compare(addr, &eth_addr_broadcast) == 0;
}

bool eth_addr_is_multicast(const eth_addr_t* addr) {
    if (!addr) {
        return false;
    }
    
    /* Multicast bit is LSB of first octet */
    return (addr->addr[0] & 0x01) != 0;
}

bool eth_addr_is_unicast(const eth_addr_t* addr) {
    return !eth_addr_is_broadcast(addr) && !eth_addr_is_multicast(addr);
}

/* ================================
 * Address Operations
 * ================================ */

int eth_addr_compare(const eth_addr_t* addr1, const eth_addr_t* addr2) {
    if (!addr1 || !addr2) {
        return -1;
    }
    
    return memcmp(addr1->addr, addr2->addr, ETH_ADDR_LEN);
}

void eth_addr_copy(eth_addr_t* dest, const eth_addr_t* src) {
    if (dest && src) {
        memcpy(dest->addr, src->addr, ETH_ADDR_LEN);
    }
}

void eth_addr_set_broadcast(eth_addr_t* addr) {
    if (addr) {
        memset(addr->addr, 0xFF, ETH_ADDR_LEN);
    }
}

void eth_addr_set_zero(eth_addr_t* addr) {
    if (addr) {
        memset(addr->addr, 0x00, ETH_ADDR_LEN);
    }
}

/* ================================
 * Frame Utilities
 * ================================ */

uint32_t eth_frame_size(uint32_t payload_len) {
    uint32_t frame_size = ETH_HEADER_SIZE + payload_len;
    
    /* Minimum frame size */
    if (frame_size < ETH_FRAME_MIN_SIZE) {
        frame_size = ETH_FRAME_MIN_SIZE;
    }
    
    return frame_size;
}

uint32_t eth_payload_size(uint32_t frame_len) {
    if (frame_len < ETH_HEADER_SIZE) {
        return 0;
    }
    
    return frame_len - ETH_HEADER_SIZE;
}

eth_header_t* eth_get_header(netbuf_t* buf) {
    if (!buf || buf->len < ETH_HEADER_SIZE) {
        return NULL;
    }
    
    return (eth_header_t*)buf->data;
}

void* eth_get_payload(netbuf_t* buf) {
    if (!buf || buf->len < ETH_HEADER_SIZE) {
        return NULL;
    }
    
    return buf->data + ETH_HEADER_SIZE;
}

uint16_t eth_get_type(const eth_header_t* header) {
    if (!header) {
        return 0;
    }
    
    return ntohs(header->type);
}

/* ================================
 * Address String Conversion
 * ================================ */

int eth_addr_from_string(const char* str, eth_addr_t* addr) {
    if (!str || !addr) {
        return NET_ERROR_INVALID;
    }
    
    uint32_t a, b, c, d, e, f;
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &a, &b, &c, &d, &e, &f) != 6) {
        return NET_ERROR_INVALID;
    }
    
    if (a > 255 || b > 255 || c > 255 || d > 255 || e > 255 || f > 255) {
        return NET_ERROR_INVALID;
    }
    
    addr->addr[0] = a;
    addr->addr[1] = b;
    addr->addr[2] = c;
    addr->addr[3] = d;
    addr->addr[4] = e;
    addr->addr[5] = f;
    
    return NET_SUCCESS;
}

char* eth_addr_to_string(const eth_addr_t* addr, char* buf, size_t len) {
    if (!addr || !buf || len < 18) {
        return NULL;
    }
    
    snprintf(buf, len, "%02x:%02x:%02x:%02x:%02x:%02x",
             addr->addr[0], addr->addr[1], addr->addr[2],
             addr->addr[3], addr->addr[4], addr->addr[5]);
    
    return buf;
}

void eth_addr_print(const eth_addr_t* addr) {
    char buf[18];
    if (eth_addr_to_string(addr, buf, sizeof(buf))) {
        printf("%s", buf);
    } else {
        printf("invalid");
    }
}

/* ================================
 * Address Generation
 * ================================ */

void eth_addr_random(eth_addr_t* addr) {
    if (!addr) {
        return;
    }
    
    /* Generate random MAC address */
    /* Set locally administered bit */
    addr->addr[0] = 0x02;
    addr->addr[1] = 0x00;
    addr->addr[2] = 0x00;
    addr->addr[3] = 0x12;
    addr->addr[4] = 0x34;
    addr->addr[5] = 0x56;
    
    /* TODO: Use proper random number generator */
    static uint8_t counter = 0;
    addr->addr[5] = counter++;
}

void eth_addr_from_serial(eth_addr_t* addr, uint32_t serial) {
    if (!addr) {
        return;
    }
    
    /* Generate MAC from serial number */
    addr->addr[0] = 0x02; /* Locally administered */
    addr->addr[1] = 0x00;
    addr->addr[2] = (serial >> 24) & 0xFF;
    addr->addr[3] = (serial >> 16) & 0xFF;
    addr->addr[4] = (serial >> 8) & 0xFF;
    addr->addr[5] = serial & 0xFF;
}

/* ================================
 * Protocol Registration
 * ================================ */

int eth_register_protocol(uint16_t type, eth_protocol_handler_t handler) {
    if (!handler || eth_protocol_count >= ETH_MAX_PROTOCOLS) {
        return NET_ERROR_INVALID;
    }
    
    /* Check if protocol already registered */
    for (uint32_t i = 0; i < eth_protocol_count; i++) {
        if (eth_protocols[i].type == type) {
            eth_protocols[i].handler = handler;
            return NET_SUCCESS;
        }
    }
    
    /* Add new protocol */
    eth_protocols[eth_protocol_count].type = type;
    eth_protocols[eth_protocol_count].handler = handler;
    eth_protocol_count++;
    
    printf("Registered Ethernet protocol 0x%04x\n", type);
    return NET_SUCCESS;
}

int eth_unregister_protocol(uint16_t type) {
    for (uint32_t i = 0; i < eth_protocol_count; i++) {
        if (eth_protocols[i].type == type) {
            /* Shift remaining protocols */
            for (uint32_t j = i; j < eth_protocol_count - 1; j++) {
                eth_protocols[j] = eth_protocols[j + 1];
            }
            eth_protocol_count--;
            printf("Unregistered Ethernet protocol 0x%04x\n", type);
            return NET_SUCCESS;
        }
    }
    
    return NET_ERROR_INVALID;
}

eth_protocol_handler_t eth_get_protocol_handler(uint16_t type) {
    for (uint32_t i = 0; i < eth_protocol_count; i++) {
        if (eth_protocols[i].type == type) {
            return eth_protocols[i].handler;
        }
    }
    
    return NULL;
}

/* ================================
 * Statistics and Monitoring
 * ================================ */

eth_stats_t* eth_get_stats(netdev_t* dev) {
    if (dev) {
        /* Return device-specific stats (not implemented) */
        return NULL;
    }
    
    /* Return global stats */
    return &eth_global_stats;
}

void eth_reset_stats(netdev_t* dev) {
    if (!dev) {
        /* Reset global stats */
        memset(&eth_global_stats, 0, sizeof(eth_global_stats));
    }
    /* Device-specific reset not implemented */
}

void eth_print_stats(netdev_t* dev) {
    eth_stats_t* stats = &eth_global_stats;
    
    printf("Ethernet Statistics%s:\n", dev ? "" : " (Global)");
    printf("  Frames received: %llu\n", stats->frames_received);
    printf("  Frames sent: %llu\n", stats->frames_sent);
    printf("  Bytes received: %llu\n", stats->bytes_received);
    printf("  Bytes sent: %llu\n", stats->bytes_sent);
    printf("  Broadcast frames: %llu\n", stats->broadcast_frames);
    printf("  Multicast frames: %llu\n", stats->multicast_frames);
    printf("  Unicast frames: %llu\n", stats->unicast_frames);
    printf("  Dropped frames: %llu\n", stats->dropped_frames);
    printf("  CRC errors: %llu\n", stats->errors_crc);
    printf("  Length errors: %llu\n", stats->errors_length);
    printf("  Alignment errors: %llu\n", stats->errors_alignment);
}

/* ================================
 * Debugging
 * ================================ */

void eth_dump_frame(const eth_frame_t* frame, uint32_t len) {
    if (!frame) {
        return;
    }
    
    printf("Ethernet Frame (%u bytes):\n", len);
    printf("  Destination: ");
    eth_addr_print(&frame->header.dest);
    printf("\n  Source:      ");
    eth_addr_print(&frame->header.src);
    printf("\n  Type:        0x%04x\n", ntohs(frame->header.type));
    
    /* Dump payload (first 64 bytes) */
    uint32_t payload_len = len - ETH_HEADER_SIZE;
    uint32_t dump_len = payload_len > 64 ? 64 : payload_len;
    
    printf("  Payload (%u bytes):\n", payload_len);
    const uint8_t* payload = (const uint8_t*)frame->payload;
    for (uint32_t i = 0; i < dump_len; i++) {
        if (i % 16 == 0) {
            printf("    %04x: ", i);
        }
        printf("%02x ", payload[i]);
        if (i % 16 == 15) {
            printf("\n");
        }
    }
    if (dump_len % 16 != 0) {
        printf("\n");
    }
    
    if (payload_len > dump_len) {
        printf("    ... (%u more bytes)\n", payload_len - dump_len);
    }
}

void eth_dump_header(const eth_header_t* header) {
    if (!header) {
        return;
    }
    
    printf("Ethernet Header:\n");
    printf("  Destination: ");
    eth_addr_print(&header->dest);
    printf("\n  Source:      ");
    eth_addr_print(&header->src);
    printf("\n  Type:        0x%04x\n", ntohs(header->type));
}

/* ================================
 * Ethernet Initialization
 * ================================ */

int ethernet_init(void) {
    if (ethernet_initialized) {
        return NET_SUCCESS;
    }
    
    printf("Initializing Ethernet layer...\n");
    
    /* Initialize statistics */
    memset(&eth_global_stats, 0, sizeof(eth_global_stats));
    
    /* Initialize protocol table */
    memset(eth_protocols, 0, sizeof(eth_protocols));
    eth_protocol_count = 0;
    
    /* Register IP protocol */
    eth_register_protocol(ETH_TYPE_IP, ip_receive_packet);
    
    ethernet_initialized = true;
    printf("Ethernet layer initialized\n");
    
    return NET_SUCCESS;
}

void ethernet_shutdown(void) {
    if (!ethernet_initialized) {
        return;
    }
    
    printf("Shutting down Ethernet layer...\n");
    
    /* Clear protocol table */
    memset(eth_protocols, 0, sizeof(eth_protocols));
    eth_protocol_count = 0;
    
    ethernet_initialized = false;
    printf("Ethernet layer shutdown complete\n");
}

int eth_device_init(netdev_t* dev) {
    if (!dev) {
        return NET_ERROR_INVALID;
    }
    
    /* Set default MTU for Ethernet */
    if (dev->mtu == 0) {
        dev->mtu = ETH_PAYLOAD_MAX;
    }
    
    /* Generate MAC address if not set */
    static const eth_addr_t zero_addr = ETH_ADDR_ZERO;
    if (eth_addr_compare(&dev->hw_addr, &zero_addr) == 0) {
        /* Generate a random MAC address */
        eth_addr_random(&dev->hw_addr);
        printf("Generated MAC address for %s: ", dev->name);
        eth_addr_print(&dev->hw_addr);
        printf("\n");
    }
    
    return NET_SUCCESS;
}

void eth_device_cleanup(netdev_t* dev) {
    /* Nothing to clean up for Ethernet devices */
    (void)dev;
}
