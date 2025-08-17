/* IKOS Network Stack - Core Network Definitions
 * Issue #35 - Network Stack Implementation
 * 
 * Provides core networking definitions, constants, and data structures
 * for the IKOS network stack implementation.
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ================================
 * Network Constants
 * ================================ */

/* Protocol Numbers */
#define IPPROTO_ICMP        1
#define IPPROTO_TCP         6
#define IPPROTO_UDP         17

/* Port Numbers */
#define PORT_ECHO           7
#define PORT_DISCARD        9
#define PORT_DAYTIME        13
#define PORT_CHARGEN        19
#define PORT_FTP_DATA       20
#define PORT_FTP_CONTROL    21
#define PORT_SSH            22
#define PORT_TELNET         23
#define PORT_SMTP           25
#define PORT_DNS            53
#define PORT_DHCP_SERVER    67
#define PORT_DHCP_CLIENT    68
#define PORT_HTTP           80
#define PORT_POP3           110
#define PORT_IMAP           143
#define PORT_HTTPS          443

/* Network Buffer Sizes */
#define NET_MAX_PACKET_SIZE     1500    /* Maximum Ethernet payload */
#define NET_MIN_PACKET_SIZE     60      /* Minimum Ethernet frame size */
#define NET_BUFFER_POOL_SIZE    256     /* Number of network buffers */
#define NET_MAX_INTERFACES      8       /* Maximum network interfaces */

/* Address Lengths */
#define ETH_ADDR_LEN           6        /* Ethernet address length */
#define IP_ADDR_LEN            4        /* IPv4 address length */

/* Network Errors */
#define NET_SUCCESS            0
#define NET_ERROR_INVALID      -1
#define NET_ERROR_NOMEM        -2
#define NET_ERROR_NODEV        -3
#define NET_ERROR_TIMEOUT      -4
#define NET_ERROR_AGAIN        -5
#define NET_ERROR_CONNRESET    -6
#define NET_ERROR_CONNREFUSED  -7
#define NET_ERROR_HOSTUNREACH  -8
#define NET_ERROR_NETUNREACH   -9

/* ================================
 * Network Address Structures
 * ================================ */

/* Ethernet Address */
typedef struct {
    uint8_t addr[ETH_ADDR_LEN];
} eth_addr_t;

/* IPv4 Address */
typedef struct {
    uint32_t addr;              /* Network byte order */
} ip_addr_t;

/* Socket Address Structures - Defined in socket.h */

/* ================================
 * Network Buffer Management
 * ================================ */

/* Network Buffer Structure */
typedef struct netbuf {
    uint8_t* data;              /* Pointer to data */
    uint32_t size;              /* Total buffer size */
    uint32_t len;               /* Current data length */
    uint32_t head;              /* Offset to data start */
    uint32_t tail;              /* Offset to data end */
    uint32_t protocol;          /* Protocol type */
    struct netdev* dev;         /* Associated network device */
    struct netbuf* next;        /* Next buffer in chain */
    void* private_data;         /* Protocol-specific data */
    uint8_t buffer[];           /* Actual buffer data */
} netbuf_t;

/* Network Buffer Pool */
typedef struct {
    netbuf_t* free_list;        /* Free buffer list */
    uint32_t free_count;        /* Number of free buffers */
    uint32_t total_count;       /* Total number of buffers */
    uint32_t alloc_count;       /* Number of allocated buffers */
    uint32_t alloc_failures;    /* Allocation failure count */
    bool initialized;           /* Pool initialization status */
} netbuf_pool_t;

/* ================================
 * Network Device Interface
 * ================================ */

/* Network Device Types */
typedef enum {
    NETDEV_TYPE_ETHERNET = 1,
    NETDEV_TYPE_LOOPBACK = 2,
    NETDEV_TYPE_PPP = 3,
    NETDEV_TYPE_TUNNEL = 4
} netdev_type_t;

/* Network Device Flags */
#define NETDEV_FLAG_UP          (1 << 0)    /* Interface is up */
#define NETDEV_FLAG_BROADCAST   (1 << 1)    /* Supports broadcast */
#define NETDEV_FLAG_LOOPBACK    (1 << 2)    /* Loopback interface */
#define NETDEV_FLAG_MULTICAST   (1 << 3)    /* Supports multicast */
#define NETDEV_FLAG_PROMISC     (1 << 4)    /* Promiscuous mode */

/* Network Device Statistics */
typedef struct {
    uint64_t rx_packets;        /* Received packets */
    uint64_t tx_packets;        /* Transmitted packets */
    uint64_t rx_bytes;          /* Received bytes */
    uint64_t tx_bytes;          /* Transmitted bytes */
    uint64_t rx_errors;         /* Receive errors */
    uint64_t tx_errors;         /* Transmit errors */
    uint64_t rx_dropped;        /* Dropped received packets */
    uint64_t tx_dropped;        /* Dropped transmitted packets */
    uint64_t collisions;        /* Collision count */
} netdev_stats_t;

/* Network Device Operations */
struct netdev_ops {
    int (*open)(struct netdev* dev);
    int (*close)(struct netdev* dev);
    int (*start_xmit)(struct netdev* dev, netbuf_t* buf);
    int (*set_config)(struct netdev* dev, void* config);
    netdev_stats_t* (*get_stats)(struct netdev* dev);
    int (*set_mac_addr)(struct netdev* dev, eth_addr_t* addr);
    int (*ioctl)(struct netdev* dev, uint32_t cmd, void* arg);
};

/* Network Device Structure */
typedef struct netdev {
    char name[16];              /* Device name (e.g., "eth0") */
    netdev_type_t type;         /* Device type */
    uint32_t flags;             /* Device flags */
    uint32_t mtu;               /* Maximum Transmission Unit */
    
    /* Addresses */
    eth_addr_t hw_addr;         /* Hardware (MAC) address */
    ip_addr_t ip_addr;          /* IP address */
    ip_addr_t netmask;          /* Network mask */
    ip_addr_t gateway;          /* Default gateway */
    
    /* Device operations */
    struct netdev_ops* ops;     /* Device operations */
    void* private_data;         /* Driver private data */
    
    /* Statistics */
    netdev_stats_t stats;       /* Device statistics */
    
    /* List management */
    struct netdev* next;        /* Next device in list */
} netdev_t;

/* ================================
 * Network Stack Functions
 * ================================ */

/* Network Initialization */
int network_init(void);
void network_shutdown(void);

/* Network Buffer Management */
int netbuf_pool_init(void);
netbuf_t* netbuf_alloc(uint32_t size);
void netbuf_free(netbuf_t* buf);
int netbuf_put(netbuf_t* buf, uint32_t len);
int netbuf_pull(netbuf_t* buf, uint32_t len);
int netbuf_push(netbuf_t* buf, uint32_t len);
int netbuf_reserve(netbuf_t* buf, uint32_t len);

/* Network Device Management */
int netdev_register(netdev_t* dev);
int netdev_unregister(netdev_t* dev);
netdev_t* netdev_get_by_name(const char* name);
netdev_t* netdev_get_by_index(uint32_t index);
int netdev_up(netdev_t* dev);
int netdev_down(netdev_t* dev);
int netdev_transmit(netdev_t* dev, netbuf_t* buf);

/* Packet Processing */
int netdev_receive_packet(netdev_t* dev, netbuf_t* buf);
int network_process_packet(netdev_t* dev, netbuf_t* buf);

/* Utility Functions */
uint16_t net_checksum(const void* data, uint32_t len);
uint32_t ip_addr_from_string(const char* str);
char* ip_addr_to_string(ip_addr_t addr, char* buf, size_t len);
int eth_addr_from_string(const char* str, eth_addr_t* addr);
char* eth_addr_to_string(const eth_addr_t* addr, char* buf, size_t len);

/* Network Byte Order Conversion */
#define htons(x) ((uint16_t)((((x) & 0xff) << 8) | (((x) >> 8) & 0xff)))
#define ntohs(x) htons(x)
#define htonl(x) ((uint32_t)((((x) & 0xff) << 24) | (((x) & 0xff00) << 8) | \
                            (((x) >> 8) & 0xff00) | (((x) >> 24) & 0xff)))
#define ntohl(x) htonl(x)

/* Address Manipulation Macros */
#define IP_ADDR(a,b,c,d) ((uint32_t)(((a) << 24) | ((b) << 16) | ((c) << 8) | (d)))
#define IP_ADDR_ANY      0x00000000
#define IP_ADDR_LOOPBACK 0x7f000001
#define IP_ADDR_BROADCAST 0xffffffff

/* Ethernet Address Macros */
#define ETH_ADDR_BROADCAST { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
#define ETH_ADDR_ZERO      { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

/* ================================
 * Address Family Constants
 * ================================ */

#define AF_UNSPEC          0
#define AF_INET            2
#define AF_INET6           10

/* Socket Types */
#define SOCK_STREAM        1        /* TCP */
#define SOCK_DGRAM         2        /* UDP */
#define SOCK_RAW           3        /* Raw sockets */

/* Socket Options */
#define SOL_SOCKET         1        /* Socket level options */
#define SO_REUSEADDR       2        /* Reuse address */
#define SO_KEEPALIVE       9        /* Keep connections alive */
#define SO_BROADCAST       6        /* Permit broadcast */
#define SO_RCVBUF          8        /* Receive buffer size */
#define SO_SNDBUF          7        /* Send buffer size */

#endif /* NETWORK_H */
