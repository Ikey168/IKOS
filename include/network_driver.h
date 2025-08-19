/*
 * IKOS Network Interface Driver - Header Definitions
 * Issue #45 - Network Interface Driver (Ethernet & Wi-Fi)
 * 
 * Implements network drivers for wired Ethernet and wireless Wi-Fi connectivity
 * with integration into the networking stack.
 */

#ifndef NETWORK_DRIVER_H
#define NETWORK_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================
 * Network Driver Constants
 * ================================ */

#define NETWORK_MAX_INTERFACES          8       /* Maximum network interfaces */
#define NETWORK_MAX_PACKET_SIZE         1536    /* Maximum packet size (MTU + headers) */
#define NETWORK_MAC_ADDRESS_SIZE        6       /* MAC address size in bytes */
#define NETWORK_SSID_MAX_LENGTH         32      /* Maximum Wi-Fi SSID length */
#define NETWORK_PASSWORD_MAX_LENGTH     64      /* Maximum Wi-Fi password length */
#define NETWORK_IP_ADDRESS_SIZE         4       /* IPv4 address size */
#define NETWORK_TX_QUEUE_SIZE           64      /* Transmit queue size */
#define NETWORK_RX_QUEUE_SIZE           64      /* Receive queue size */

/* Network interface types */
#define NETWORK_TYPE_ETHERNET           0x01
#define NETWORK_TYPE_WIFI               0x02
#define NETWORK_TYPE_LOOPBACK          0x03

/* Network interface states */
#define NETWORK_STATE_DOWN              0x00
#define NETWORK_STATE_UP                0x01
#define NETWORK_STATE_CONNECTING        0x02
#define NETWORK_STATE_CONNECTED         0x03
#define NETWORK_STATE_DISCONNECTING     0x04
#define NETWORK_STATE_ERROR             0x05

/* Ethernet frame types */
#define ETH_TYPE_IPV4                   0x0800
#define ETH_TYPE_ARP                    0x0806
#define ETH_TYPE_IPV6                   0x86DD

/* Wi-Fi security types */
#define WIFI_SECURITY_NONE              0x00
#define WIFI_SECURITY_WEP               0x01
#define WIFI_SECURITY_WPA               0x02
#define WIFI_SECURITY_WPA2              0x03
#define WIFI_SECURITY_WPA3              0x04

/* ================================
 * Network Data Structures
 * ================================ */

/* MAC address structure */
typedef struct {
    uint8_t addr[NETWORK_MAC_ADDRESS_SIZE];
} network_mac_addr_t;

/* IP address structure (IPv4) */
typedef struct {
    uint8_t addr[NETWORK_IP_ADDRESS_SIZE];
} network_ip_addr_t;

/* Network packet structure */
typedef struct {
    uint8_t* data;
    uint32_t length;
    uint32_t capacity;
    uint32_t offset;
    void* private_data;
} network_packet_t;

/* Ethernet header structure */
typedef struct __attribute__((packed)) {
    network_mac_addr_t dest_mac;
    network_mac_addr_t src_mac;
    uint16_t ethertype;
} ethernet_header_t;

/* Wi-Fi network information */
typedef struct {
    char ssid[NETWORK_SSID_MAX_LENGTH + 1];
    uint8_t security_type;
    int8_t signal_strength;
    uint8_t channel;
    network_mac_addr_t bssid;
    bool connected;
} wifi_network_info_t;

/* Wi-Fi configuration */
typedef struct {
    char ssid[NETWORK_SSID_MAX_LENGTH + 1];
    char password[NETWORK_PASSWORD_MAX_LENGTH + 1];
    uint8_t security_type;
    bool auto_connect;
} wifi_config_t;

/* Network interface statistics */
typedef struct {
    uint64_t tx_packets;
    uint64_t rx_packets;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint64_t tx_errors;
    uint64_t rx_errors;
    uint64_t tx_dropped;
    uint64_t rx_dropped;
} network_stats_t;

/* Forward declaration */
typedef struct network_interface network_interface_t;

/* Network driver operations */
typedef struct {
    int (*init)(network_interface_t* iface);
    int (*start)(network_interface_t* iface);
    int (*stop)(network_interface_t* iface);
    int (*send_packet)(network_interface_t* iface, network_packet_t* packet);
    int (*set_mac_address)(network_interface_t* iface, network_mac_addr_t* mac);
    int (*get_link_status)(network_interface_t* iface);
    
    /* Wi-Fi specific operations */
    int (*wifi_scan)(network_interface_t* iface, wifi_network_info_t* networks, uint32_t max_count);
    int (*wifi_connect)(network_interface_t* iface, wifi_config_t* config);
    int (*wifi_disconnect)(network_interface_t* iface);
    int (*wifi_get_status)(network_interface_t* iface, wifi_network_info_t* status);
} network_driver_ops_t;

/* Network interface structure */
struct network_interface {
    /* Identification */
    uint32_t id;
    char name[16];
    uint8_t type;
    uint8_t state;
    
    /* Hardware information */
    network_mac_addr_t mac_address;
    uint32_t mtu;
    uint16_t pci_vendor_id;
    uint16_t pci_device_id;
    
    /* Network configuration */
    network_ip_addr_t ip_address;
    network_ip_addr_t netmask;
    network_ip_addr_t gateway;
    bool dhcp_enabled;
    
    /* Driver operations */
    network_driver_ops_t* ops;
    void* private_data;
    
    /* Packet queues */
    network_packet_t tx_queue[NETWORK_TX_QUEUE_SIZE];
    network_packet_t rx_queue[NETWORK_RX_QUEUE_SIZE];
    uint32_t tx_head, tx_tail;
    uint32_t rx_head, rx_tail;
    
    /* Statistics */
    network_stats_t stats;
    
    /* Wi-Fi specific data */
    wifi_config_t wifi_config;
    wifi_network_info_t current_network;
    wifi_network_info_t available_networks[16];
    uint32_t available_network_count;
    
    /* State management */
    bool initialized;
    bool enabled;
    uint32_t last_activity;
};

/* Network driver manager */
typedef struct {
    bool initialized;
    uint32_t interface_count;
    network_interface_t interfaces[NETWORK_MAX_INTERFACES];
    network_interface_t* default_interface;
    
    /* Packet allocation pool */
    network_packet_t packet_pool[256];
    bool packet_pool_used[256];
    uint32_t packets_allocated;
    
    /* Global statistics */
    uint64_t total_tx_packets;
    uint64_t total_rx_packets;
    uint64_t total_tx_bytes;
    uint64_t total_rx_bytes;
} network_driver_manager_t;

/* ================================
 * Network Driver Core Functions
 * ================================ */

/* Initialization and cleanup */
int network_driver_init(void);
void network_driver_cleanup(void);

/* Interface management */
network_interface_t* network_register_interface(const char* name, uint8_t type, network_driver_ops_t* ops);
int network_unregister_interface(network_interface_t* iface);
network_interface_t* network_get_interface_by_name(const char* name);
network_interface_t* network_get_interface_by_id(uint32_t id);
network_interface_t* network_get_default_interface(void);

/* Interface operations */
int network_interface_up(network_interface_t* iface);
int network_interface_down(network_interface_t* iface);
int network_interface_set_ip(network_interface_t* iface, network_ip_addr_t* ip, network_ip_addr_t* netmask);
int network_interface_set_gateway(network_interface_t* iface, network_ip_addr_t* gateway);
int network_interface_enable_dhcp(network_interface_t* iface, bool enable);

/* Packet management */
network_packet_t* network_packet_alloc(uint32_t size);
void network_packet_free(network_packet_t* packet);
int network_packet_send(network_interface_t* iface, network_packet_t* packet);
network_packet_t* network_packet_receive(network_interface_t* iface);

/* Ethernet driver functions */
int ethernet_driver_init(void);
int ethernet_detect_interfaces(void);
int ethernet_send_frame(network_interface_t* iface, network_mac_addr_t* dest, uint16_t ethertype, uint8_t* data, uint32_t length);

/* Wi-Fi driver functions */
int wifi_driver_init(void);
int wifi_detect_interfaces(void);
int wifi_scan_networks(network_interface_t* iface);
int wifi_connect_network(network_interface_t* iface, const char* ssid, const char* password, uint8_t security_type);
int wifi_disconnect_network(network_interface_t* iface);
int wifi_get_signal_strength(network_interface_t* iface);

/* Network stack integration */
int network_stack_init(void);
int network_register_protocol_handler(uint16_t ethertype, int (*handler)(network_packet_t* packet));
int network_send_ip_packet(network_ip_addr_t* dest, uint8_t protocol, uint8_t* data, uint32_t length);

/* Utility functions */
void network_mac_addr_copy(network_mac_addr_t* dest, network_mac_addr_t* src);
bool network_mac_addr_equal(network_mac_addr_t* addr1, network_mac_addr_t* addr2);
void network_ip_addr_copy(network_ip_addr_t* dest, network_ip_addr_t* src);
bool network_ip_addr_equal(network_ip_addr_t* addr1, network_ip_addr_t* addr2);
char* network_mac_addr_to_string(network_mac_addr_t* mac);
char* network_ip_addr_to_string(network_ip_addr_t* ip);
int network_string_to_ip_addr(const char* str, network_ip_addr_t* ip);

/* Statistics and monitoring */
int network_get_interface_stats(network_interface_t* iface, network_stats_t* stats);
int network_get_global_stats(uint64_t* tx_packets, uint64_t* rx_packets, uint64_t* tx_bytes, uint64_t* rx_bytes);
void network_print_interface_info(network_interface_t* iface);
void network_print_all_interfaces(void);

/* Error handling */
typedef enum {
    NETWORK_SUCCESS = 0,
    NETWORK_ERROR_INVALID_PARAM = -1,
    NETWORK_ERROR_NO_MEMORY = -2,
    NETWORK_ERROR_NOT_INITIALIZED = -3,
    NETWORK_ERROR_INTERFACE_NOT_FOUND = -4,
    NETWORK_ERROR_INTERFACE_DOWN = -5,
    NETWORK_ERROR_TRANSMISSION_FAILED = -6,
    NETWORK_ERROR_NO_LINK = -7,
    NETWORK_ERROR_TIMEOUT = -8,
    NETWORK_ERROR_WIFI_NOT_CONNECTED = -9,
    NETWORK_ERROR_WIFI_SCAN_FAILED = -10,
    NETWORK_ERROR_WIFI_CONNECT_FAILED = -11,
    NETWORK_ERROR_DRIVER_ERROR = -12,
    NETWORK_ERROR_PACKET_TOO_LARGE = -13,
    NETWORK_ERROR_QUEUE_FULL = -14
} network_error_t;

const char* network_get_error_string(network_error_t error);

/* Hardware-specific driver interfaces */

/* RTL8139 Ethernet driver */
typedef struct {
    uint32_t io_base;
    uint8_t irq;
    uint32_t rx_buffer_phys;
    uint32_t tx_buffers_phys[4];
    uint8_t* rx_buffer;
    uint8_t* tx_buffers[4];
    uint32_t rx_offset;
    uint8_t tx_current;
} rtl8139_private_t;

int rtl8139_init(network_interface_t* iface);
int rtl8139_start(network_interface_t* iface);
int rtl8139_stop(network_interface_t* iface);
int rtl8139_send_packet(network_interface_t* iface, network_packet_t* packet);
void rtl8139_interrupt_handler(uint8_t irq);

/* Intel E1000 Ethernet driver */
typedef struct {
    uint32_t mmio_base;
    uint8_t irq;
    uint32_t rx_desc_base;
    uint32_t tx_desc_base;
    uint16_t rx_current;
    uint16_t tx_current;
} e1000_private_t;

int e1000_init(network_interface_t* iface);
int e1000_start(network_interface_t* iface);
int e1000_stop(network_interface_t* iface);
int e1000_send_packet(network_interface_t* iface, network_packet_t* packet);
void e1000_interrupt_handler(uint8_t irq);

/* Generic Wi-Fi driver (placeholder for specific chipsets) */
typedef struct {
    uint32_t mmio_base;
    uint8_t irq;
    uint8_t firmware_loaded;
    char current_ssid[NETWORK_SSID_MAX_LENGTH + 1];
    uint8_t connection_state;
} wifi_generic_private_t;

int wifi_generic_init(network_interface_t* iface);
int wifi_generic_scan(network_interface_t* iface, wifi_network_info_t* networks, uint32_t max_count);
int wifi_generic_connect(network_interface_t* iface, wifi_config_t* config);
int wifi_generic_disconnect(network_interface_t* iface);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_DRIVER_H */
