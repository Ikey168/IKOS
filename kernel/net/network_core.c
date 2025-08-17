/* IKOS Network Stack - Core Network Implementation
 * Issue #35 - Network Stack Implementation
 * 
 * Core networking functionality including network buffer management,
 * device registration, and packet processing infrastructure.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "../include/net/network.h"
#include "../include/net/ethernet.h"
#include "../include/net/ip.h"
#include "../include/memory.h"

/* ================================
 * Global Network State
 * ================================ */

static bool network_initialized = false;
static netbuf_pool_t netbuf_pool;
static netdev_t* netdev_list = NULL;
static uint32_t netdev_count = 0;

/* Network statistics */
static struct {
    uint64_t packets_processed;
    uint64_t packets_dropped;
    uint64_t bytes_processed;
    uint64_t errors;
} network_stats;

/* ================================
 * Network Buffer Pool Management
 * ================================ */

int netbuf_pool_init(void) {
    memset(&netbuf_pool, 0, sizeof(netbuf_pool));
    
    /* Allocate buffer pool */
    for (uint32_t i = 0; i < NET_BUFFER_POOL_SIZE; i++) {
        netbuf_t* buf = (netbuf_t*)malloc(sizeof(netbuf_t) + NET_MAX_PACKET_SIZE);
        if (!buf) {
            printf("Failed to allocate network buffer %u\n", i);
            return NET_ERROR_NOMEM;
        }
        
        /* Initialize buffer */
        memset(buf, 0, sizeof(netbuf_t));
        buf->size = NET_MAX_PACKET_SIZE;
        buf->data = buf->buffer;
        buf->next = netbuf_pool.free_list;
        netbuf_pool.free_list = buf;
        netbuf_pool.free_count++;
        netbuf_pool.total_count++;
    }
    
    netbuf_pool.initialized = true;
    printf("Network buffer pool initialized: %u buffers\n", NET_BUFFER_POOL_SIZE);
    return NET_SUCCESS;
}

netbuf_t* netbuf_alloc(uint32_t size) {
    if (!netbuf_pool.initialized || size > NET_MAX_PACKET_SIZE) {
        netbuf_pool.alloc_failures++;
        return NULL;
    }
    
    if (!netbuf_pool.free_list) {
        netbuf_pool.alloc_failures++;
        return NULL;
    }
    
    /* Get buffer from free list */
    netbuf_t* buf = netbuf_pool.free_list;
    netbuf_pool.free_list = buf->next;
    netbuf_pool.free_count--;
    netbuf_pool.alloc_count++;
    
    /* Initialize buffer for use */
    buf->len = 0;
    buf->head = 0;
    buf->tail = 0;
    buf->protocol = 0;
    buf->dev = NULL;
    buf->next = NULL;
    buf->private_data = NULL;
    
    return buf;
}

void netbuf_free(netbuf_t* buf) {
    if (!buf || !netbuf_pool.initialized) {
        return;
    }
    
    /* Return buffer to free list */
    buf->next = netbuf_pool.free_list;
    netbuf_pool.free_list = buf;
    netbuf_pool.free_count++;
    netbuf_pool.alloc_count--;
}

int netbuf_put(netbuf_t* buf, uint32_t len) {
    if (!buf || len > (buf->size - buf->tail)) {
        return NET_ERROR_INVALID;
    }
    
    buf->tail += len;
    buf->len += len;
    return NET_SUCCESS;
}

int netbuf_pull(netbuf_t* buf, uint32_t len) {
    if (!buf || len > buf->len) {
        return NET_ERROR_INVALID;
    }
    
    buf->head += len;
    buf->len -= len;
    buf->data = buf->buffer + buf->head;
    return NET_SUCCESS;
}

int netbuf_push(netbuf_t* buf, uint32_t len) {
    if (!buf || len > buf->head) {
        return NET_ERROR_INVALID;
    }
    
    buf->head -= len;
    buf->len += len;
    buf->data = buf->buffer + buf->head;
    return NET_SUCCESS;
}

int netbuf_reserve(netbuf_t* buf, uint32_t len) {
    if (!buf || len > buf->size) {
        return NET_ERROR_INVALID;
    }
    
    buf->head = len;
    buf->tail = len;
    buf->data = buf->buffer + buf->head;
    return NET_SUCCESS;
}

/* ================================
 * Network Device Management
 * ================================ */

int netdev_register(netdev_t* dev) {
    if (!dev || !dev->name[0] || !dev->ops) {
        return NET_ERROR_INVALID;
    }
    
    /* Check if device already exists */
    netdev_t* existing = netdev_get_by_name(dev->name);
    if (existing) {
        printf("Network device %s already exists\n", dev->name);
        return NET_ERROR_INVALID;
    }
    
    /* Add to device list */
    dev->next = netdev_list;
    netdev_list = dev;
    netdev_count++;
    
    /* Initialize device statistics */
    memset(&dev->stats, 0, sizeof(netdev_stats_t));
    
    printf("Network device %s registered (type=%d, MTU=%u)\n", 
           dev->name, dev->type, dev->mtu);
    
    return NET_SUCCESS;
}

int netdev_unregister(netdev_t* dev) {
    if (!dev) {
        return NET_ERROR_INVALID;
    }
    
    /* Remove from device list */
    if (netdev_list == dev) {
        netdev_list = dev->next;
    } else {
        netdev_t* curr = netdev_list;
        while (curr && curr->next != dev) {
            curr = curr->next;
        }
        if (curr) {
            curr->next = dev->next;
        }
    }
    
    netdev_count--;
    dev->next = NULL;
    
    printf("Network device %s unregistered\n", dev->name);
    return NET_SUCCESS;
}

netdev_t* netdev_get_by_name(const char* name) {
    if (!name) {
        return NULL;
    }
    
    netdev_t* dev = netdev_list;
    while (dev) {
        if (strcmp(dev->name, name) == 0) {
            return dev;
        }
        dev = dev->next;
    }
    
    return NULL;
}

netdev_t* netdev_get_by_index(uint32_t index) {
    if (index >= netdev_count) {
        return NULL;
    }
    
    netdev_t* dev = netdev_list;
    for (uint32_t i = 0; i < index && dev; i++) {
        dev = dev->next;
    }
    
    return dev;
}

int netdev_up(netdev_t* dev) {
    if (!dev || !dev->ops || !dev->ops->open) {
        return NET_ERROR_INVALID;
    }
    
    if (dev->flags & NETDEV_FLAG_UP) {
        return NET_SUCCESS; /* Already up */
    }
    
    int result = dev->ops->open(dev);
    if (result == NET_SUCCESS) {
        dev->flags |= NETDEV_FLAG_UP;
        printf("Network device %s is up\n", dev->name);
    } else {
        printf("Failed to bring up network device %s: %d\n", dev->name, result);
    }
    
    return result;
}

int netdev_down(netdev_t* dev) {
    if (!dev || !dev->ops || !dev->ops->close) {
        return NET_ERROR_INVALID;
    }
    
    if (!(dev->flags & NETDEV_FLAG_UP)) {
        return NET_SUCCESS; /* Already down */
    }
    
    int result = dev->ops->close(dev);
    if (result == NET_SUCCESS) {
        dev->flags &= ~NETDEV_FLAG_UP;
        printf("Network device %s is down\n", dev->name);
    }
    
    return result;
}

int netdev_transmit(netdev_t* dev, netbuf_t* buf) {
    if (!dev || !buf || !dev->ops || !dev->ops->start_xmit) {
        return NET_ERROR_INVALID;
    }
    
    if (!(dev->flags & NETDEV_FLAG_UP)) {
        return NET_ERROR_NODEV;
    }
    
    /* Update statistics */
    dev->stats.tx_packets++;
    dev->stats.tx_bytes += buf->len;
    
    /* Transmit packet */
    int result = dev->ops->start_xmit(dev, buf);
    if (result != NET_SUCCESS) {
        dev->stats.tx_errors++;
        dev->stats.tx_dropped++;
    }
    
    return result;
}

/* ================================
 * Packet Processing
 * ================================ */

int netdev_receive_packet(netdev_t* dev, netbuf_t* buf) {
    if (!dev || !buf) {
        network_stats.packets_dropped++;
        return NET_ERROR_INVALID;
    }
    
    /* Update device statistics */
    dev->stats.rx_packets++;
    dev->stats.rx_bytes += buf->len;
    
    /* Update network statistics */
    network_stats.packets_processed++;
    network_stats.bytes_processed += buf->len;
    
    /* Set device association */
    buf->dev = dev;
    
    /* Process packet through network stack */
    int result = network_process_packet(dev, buf);
    if (result != NET_SUCCESS) {
        dev->stats.rx_errors++;
        dev->stats.rx_dropped++;
        network_stats.packets_dropped++;
        network_stats.errors++;
    }
    
    return result;
}

int network_process_packet(netdev_t* dev, netbuf_t* buf) {
    if (!dev || !buf) {
        return NET_ERROR_INVALID;
    }
    
    /* Process based on device type */
    switch (dev->type) {
        case NETDEV_TYPE_ETHERNET:
            return eth_receive_frame(dev, buf);
            
        case NETDEV_TYPE_LOOPBACK:
            /* Loopback packets go directly to IP */
            return ip_receive_packet(dev, buf);
            
        default:
            printf("Unknown network device type: %d\n", dev->type);
            return NET_ERROR_INVALID;
    }
}

/* ================================
 * Utility Functions
 * ================================ */

uint16_t net_checksum(const void* data, uint32_t len) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;
    
    /* Sum all 16-bit words */
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    
    /* Add odd byte if present */
    if (len > 0) {
        sum += *(const uint8_t*)ptr;
    }
    
    /* Fold 32-bit sum to 16 bits */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

uint32_t ip_addr_from_string(const char* str) {
    if (!str) {
        return 0;
    }
    
    uint32_t a, b, c, d;
    if (sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return 0;
    }
    
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return 0;
    }
    
    return htonl((a << 24) | (b << 16) | (c << 8) | d);
}

char* ip_addr_to_string(ip_addr_t addr, char* buf, size_t len) {
    if (!buf || len < 16) {
        return NULL;
    }
    
    uint32_t ip = ntohl(addr.addr);
    snprintf(buf, len, "%u.%u.%u.%u",
             (ip >> 24) & 0xFF,
             (ip >> 16) & 0xFF,
             (ip >> 8) & 0xFF,
             ip & 0xFF);
    
    return buf;
}

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

/* ================================
 * Network Statistics
 * ================================ */

void network_print_stats(void) {
    printf("Network Statistics:\n");
    printf("  Packets processed: %llu\n", network_stats.packets_processed);
    printf("  Packets dropped: %llu\n", network_stats.packets_dropped);
    printf("  Bytes processed: %llu\n", network_stats.bytes_processed);
    printf("  Errors: %llu\n", network_stats.errors);
    printf("  Active devices: %u\n", netdev_count);
    printf("  Buffer pool: %u/%u free\n", netbuf_pool.free_count, netbuf_pool.total_count);
    printf("  Buffer allocation failures: %u\n", netbuf_pool.alloc_failures);
}

/* ================================
 * Network Initialization
 * ================================ */

int network_init(void) {
    if (network_initialized) {
        return NET_SUCCESS;
    }
    
    printf("Initializing IKOS Network Stack...\n");
    
    /* Initialize network buffer pool */
    int result = netbuf_pool_init();
    if (result != NET_SUCCESS) {
        printf("Failed to initialize network buffer pool: %d\n", result);
        return result;
    }
    
    /* Initialize protocol layers */
    result = ethernet_init();
    if (result != NET_SUCCESS) {
        printf("Failed to initialize Ethernet layer: %d\n", result);
        return result;
    }
    
    result = ip_init();
    if (result != NET_SUCCESS) {
        printf("Failed to initialize IP layer: %d\n", result);
        return result;
    }
    
    /* Initialize statistics */
    memset(&network_stats, 0, sizeof(network_stats));
    
    network_initialized = true;
    printf("Network stack initialized successfully\n");
    
    return NET_SUCCESS;
}

void network_shutdown(void) {
    if (!network_initialized) {
        return;
    }
    
    printf("Shutting down network stack...\n");
    
    /* Shutdown protocol layers */
    ip_shutdown();
    ethernet_shutdown();
    
    /* Clean up network devices */
    netdev_t* dev = netdev_list;
    while (dev) {
        netdev_t* next = dev->next;
        netdev_down(dev);
        netdev_unregister(dev);
        dev = next;
    }
    
    /* Clean up buffer pool */
    netbuf_t* buf = netbuf_pool.free_list;
    while (buf) {
        netbuf_t* next = buf->next;
        free(buf);
        buf = next;
    }
    
    memset(&netbuf_pool, 0, sizeof(netbuf_pool));
    network_initialized = false;
    
    printf("Network stack shutdown complete\n");
}
