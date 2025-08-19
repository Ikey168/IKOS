/*
 * IKOS Network Interface Driver - Core Implementation
 * Issue #45 - Network Interface Driver (Ethernet & Wi-Fi)
 * 
 * Implements network drivers for wired Ethernet and wireless Wi-Fi connectivity
 * with integration into the networking stack.
 */

#include "network_driver.h"
#include "memory.h"
#include "stdio.h"
#include "string.h"
#include "pci.h"
#include "interrupts.h"
#include <stddef.h>
#include <stdlib.h>

/* ================================
 * Global Network Driver Manager
 * ================================ */

static network_driver_manager_t g_network_manager = {0};

/* ================================
 * Static Function Declarations
 * ================================ */

static int network_allocate_interface_id(void);
static void network_update_statistics(network_interface_t* iface, bool tx, uint32_t bytes, bool error);
static int network_queue_packet(network_interface_t* iface, network_packet_t* packet, bool tx);
static network_packet_t* network_dequeue_packet(network_interface_t* iface, bool tx);
static void network_process_received_packet(network_interface_t* iface, network_packet_t* packet);
static int network_detect_hardware(void);

/* PCI device IDs for common network cards */
static const struct {
    uint16_t vendor_id;
    uint16_t device_id;
    const char* name;
    uint8_t type;
} network_pci_devices[] = {
    /* RTL8139 Ethernet */
    {0x10EC, 0x8139, "Realtek RTL8139", NETWORK_TYPE_ETHERNET},
    /* Intel E1000 series */
    {0x8086, 0x100E, "Intel 82540EM", NETWORK_TYPE_ETHERNET},
    {0x8086, 0x1004, "Intel 82543GC", NETWORK_TYPE_ETHERNET},
    {0x8086, 0x100F, "Intel 82545EM", NETWORK_TYPE_ETHERNET},
    /* Broadcom Wi-Fi */
    {0x14E4, 0x4315, "Broadcom BCM4315", NETWORK_TYPE_WIFI},
    {0x14E4, 0x4318, "Broadcom BCM4318", NETWORK_TYPE_WIFI},
    /* Intel Wi-Fi */
    {0x8086, 0x4222, "Intel PRO/Wireless 3945ABG", NETWORK_TYPE_WIFI},
    {0x8086, 0x4229, "Intel PRO/Wireless 4965AGN", NETWORK_TYPE_WIFI},
    {0, 0, NULL, 0} /* Terminator */
};

/* ================================
 * Core Network Driver Functions
 * ================================ */

int network_driver_init(void) {
    if (g_network_manager.initialized) {
        return NETWORK_SUCCESS;
    }
    
    // Clear manager structure
    memset(&g_network_manager, 0, sizeof(network_driver_manager_t));
    
    // Initialize packet pool
    for (uint32_t i = 0; i < 256; i++) {
        g_network_manager.packet_pool_used[i] = false;
    }
    
    // Mark as initialized
    g_network_manager.initialized = true;
    
    // Detect and initialize hardware
    if (network_detect_hardware() != NETWORK_SUCCESS) {
        printf("Warning: No network hardware detected\n");
    }
    
    // Initialize networking stack
    if (network_stack_init() != NETWORK_SUCCESS) {
        printf("Failed to initialize network stack\n");
        return NETWORK_ERROR_DRIVER_ERROR;
    }
    
    printf("Network driver system initialized\n");
    return NETWORK_SUCCESS;
}

void network_driver_cleanup(void) {
    if (!g_network_manager.initialized) {
        return;
    }
    
    // Stop and cleanup all interfaces
    for (uint32_t i = 0; i < NETWORK_MAX_INTERFACES; i++) {
        network_interface_t* iface = &g_network_manager.interfaces[i];
        if (iface->initialized) {
            network_interface_down(iface);
            if (iface->ops && iface->ops->stop) {
                iface->ops->stop(iface);
            }
        }
    }
    
    // Free allocated packets
    for (uint32_t i = 0; i < 256; i++) {
        if (g_network_manager.packet_pool_used[i]) {
            network_packet_t* packet = &g_network_manager.packet_pool[i];
            if (packet->data) {
                free(packet->data);
            }
        }
    }
    
    // Clear manager
    memset(&g_network_manager, 0, sizeof(network_driver_manager_t));
}

network_interface_t* network_register_interface(const char* name, uint8_t type, network_driver_ops_t* ops) {
    if (!g_network_manager.initialized || !name || !ops) {
        return NULL;
    }
    
    if (g_network_manager.interface_count >= NETWORK_MAX_INTERFACES) {
        return NULL;
    }
    
    // Find free interface slot
    network_interface_t* iface = NULL;
    for (uint32_t i = 0; i < NETWORK_MAX_INTERFACES; i++) {
        if (!g_network_manager.interfaces[i].initialized) {
            iface = &g_network_manager.interfaces[i];
            break;
        }
    }
    
    if (!iface) {
        return NULL;
    }
    
    // Initialize interface
    memset(iface, 0, sizeof(network_interface_t));
    iface->id = network_allocate_interface_id();
    strncpy(iface->name, name, sizeof(iface->name) - 1);
    iface->type = type;
    iface->state = NETWORK_STATE_DOWN;
    iface->ops = ops;
    iface->mtu = 1500; // Standard Ethernet MTU
    iface->dhcp_enabled = true;
    iface->initialized = true;
    
    // Set as default interface if it's the first one
    if (!g_network_manager.default_interface) {
        g_network_manager.default_interface = iface;
    }
    
    g_network_manager.interface_count++;
    
    printf("Network interface '%s' registered (ID: %u, Type: %s)\n", 
           name, iface->id, 
           type == NETWORK_TYPE_ETHERNET ? "Ethernet" : 
           type == NETWORK_TYPE_WIFI ? "Wi-Fi" : "Unknown");
    
    return iface;
}

int network_unregister_interface(network_interface_t* iface) {
    if (!iface || !iface->initialized) {
        return NETWORK_ERROR_INTERFACE_NOT_FOUND;
    }
    
    // Stop interface if running
    if (iface->state != NETWORK_STATE_DOWN) {
        network_interface_down(iface);
    }
    
    // Update default interface if needed
    if (g_network_manager.default_interface == iface) {
        g_network_manager.default_interface = NULL;
        // Find new default interface
        for (uint32_t i = 0; i < NETWORK_MAX_INTERFACES; i++) {
            if (g_network_manager.interfaces[i].initialized && 
                &g_network_manager.interfaces[i] != iface) {
                g_network_manager.default_interface = &g_network_manager.interfaces[i];
                break;
            }
        }
    }
    
    // Clear interface
    memset(iface, 0, sizeof(network_interface_t));
    g_network_manager.interface_count--;
    
    return NETWORK_SUCCESS;
}

network_interface_t* network_get_interface_by_name(const char* name) {
    if (!name) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < NETWORK_MAX_INTERFACES; i++) {
        network_interface_t* iface = &g_network_manager.interfaces[i];
        if (iface->initialized && strcmp(iface->name, name) == 0) {
            return iface;
        }
    }
    
    return NULL;
}

network_interface_t* network_get_interface_by_id(uint32_t id) {
    for (uint32_t i = 0; i < NETWORK_MAX_INTERFACES; i++) {
        network_interface_t* iface = &g_network_manager.interfaces[i];
        if (iface->initialized && iface->id == id) {
            return iface;
        }
    }
    
    return NULL;
}

network_interface_t* network_get_default_interface(void) {
    return g_network_manager.default_interface;
}

/* ================================
 * Interface Operations
 * ================================ */

int network_interface_up(network_interface_t* iface) {
    if (!iface || !iface->initialized) {
        return NETWORK_ERROR_INTERFACE_NOT_FOUND;
    }
    
    if (iface->state == NETWORK_STATE_UP) {
        return NETWORK_SUCCESS;
    }
    
    // Initialize driver if needed
    if (iface->ops->init) {
        int result = iface->ops->init(iface);
        if (result != NETWORK_SUCCESS) {
            return result;
        }
    }
    
    // Start interface
    if (iface->ops->start) {
        int result = iface->ops->start(iface);
        if (result != NETWORK_SUCCESS) {
            return result;
        }
    }
    
    iface->state = NETWORK_STATE_UP;
    iface->enabled = true;
    
    printf("Network interface '%s' is up\n", iface->name);
    return NETWORK_SUCCESS;
}

int network_interface_down(network_interface_t* iface) {
    if (!iface || !iface->initialized) {
        return NETWORK_ERROR_INTERFACE_NOT_FOUND;
    }
    
    if (iface->state == NETWORK_STATE_DOWN) {
        return NETWORK_SUCCESS;
    }
    
    // Stop interface
    if (iface->ops->stop) {
        iface->ops->stop(iface);
    }
    
    iface->state = NETWORK_STATE_DOWN;
    iface->enabled = false;
    
    printf("Network interface '%s' is down\n", iface->name);
    return NETWORK_SUCCESS;
}

int network_interface_set_ip(network_interface_t* iface, network_ip_addr_t* ip, network_ip_addr_t* netmask) {
    if (!iface || !iface->initialized || !ip || !netmask) {
        return NETWORK_ERROR_INVALID_PARAM;
    }
    
    network_ip_addr_copy(&iface->ip_address, ip);
    network_ip_addr_copy(&iface->netmask, netmask);
    
    printf("Network interface '%s' IP set to %s\n", 
           iface->name, network_ip_addr_to_string(ip));
    
    return NETWORK_SUCCESS;
}

/* ================================
 * Packet Management
 * ================================ */

network_packet_t* network_packet_alloc(uint32_t size) {
    if (size > NETWORK_MAX_PACKET_SIZE) {
        return NULL;
    }
    
    // Find free packet in pool
    for (uint32_t i = 0; i < 256; i++) {
        if (!g_network_manager.packet_pool_used[i]) {
            network_packet_t* packet = &g_network_manager.packet_pool[i];
            
            // Allocate data buffer
            packet->data = malloc(size);
            if (!packet->data) {
                return NULL;
            }
            
            packet->length = 0;
            packet->capacity = size;
            packet->offset = 0;
            packet->private_data = NULL;
            
            g_network_manager.packet_pool_used[i] = true;
            g_network_manager.packets_allocated++;
            
            return packet;
        }
    }
    
    return NULL; // No free packets
}

void network_packet_free(network_packet_t* packet) {
    if (!packet) {
        return;
    }
    
    // Find packet in pool
    for (uint32_t i = 0; i < 256; i++) {
        if (&g_network_manager.packet_pool[i] == packet) {
            if (packet->data) {
                free(packet->data);
            }
            
            memset(packet, 0, sizeof(network_packet_t));
            g_network_manager.packet_pool_used[i] = false;
            g_network_manager.packets_allocated--;
            return;
        }
    }
}

int network_packet_send(network_interface_t* iface, network_packet_t* packet) {
    if (!iface || !iface->initialized || !packet) {
        return NETWORK_ERROR_INVALID_PARAM;
    }
    
    if (iface->state != NETWORK_STATE_UP) {
        return NETWORK_ERROR_INTERFACE_DOWN;
    }
    
    if (!iface->ops->send_packet) {
        return NETWORK_ERROR_DRIVER_ERROR;
    }
    
    // Update statistics
    network_update_statistics(iface, true, packet->length, false);
    
    // Send packet
    int result = iface->ops->send_packet(iface, packet);
    if (result != NETWORK_SUCCESS) {
        network_update_statistics(iface, true, 0, true);
        return result;
    }
    
    return NETWORK_SUCCESS;
}

/* ================================
 * Ethernet Driver Functions
 * ================================ */

int ethernet_driver_init(void) {
    printf("Initializing Ethernet drivers...\n");
    
    // Initialize specific Ethernet drivers
    rtl8139_init(NULL); // Will scan for RTL8139 cards
    e1000_init(NULL);   // Will scan for E1000 cards
    
    return NETWORK_SUCCESS;
}

int ethernet_detect_interfaces(void) {
    int detected = 0;
    
    // Scan PCI bus for Ethernet devices
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t device = 0; device < 32; device++) {
            for (uint32_t function = 0; function < 8; function++) {
                uint16_t vendor_id = pci_read_word(bus, device, function, 0x00);
                if (vendor_id == 0xFFFF) continue;
                
                uint16_t device_id = pci_read_word(bus, device, function, 0x02);
                
                // Check if this is a known network device
                for (int i = 0; network_pci_devices[i].vendor_id != 0; i++) {
                    if (network_pci_devices[i].vendor_id == vendor_id &&
                        network_pci_devices[i].device_id == device_id &&
                        network_pci_devices[i].type == NETWORK_TYPE_ETHERNET) {
                        
                        printf("Found Ethernet device: %s (VID:%04X DID:%04X)\n",
                               network_pci_devices[i].name, vendor_id, device_id);
                        detected++;
                        
                        // TODO: Initialize specific driver based on device ID
                        break;
                    }
                }
            }
        }
    }
    
    return detected;
}

int ethernet_send_frame(network_interface_t* iface, network_mac_addr_t* dest, uint16_t ethertype, uint8_t* data, uint32_t length) {
    if (!iface || !dest || !data || length == 0) {
        return NETWORK_ERROR_INVALID_PARAM;
    }
    
    // Allocate packet
    network_packet_t* packet = network_packet_alloc(sizeof(ethernet_header_t) + length);
    if (!packet) {
        return NETWORK_ERROR_NO_MEMORY;
    }
    
    // Build Ethernet header
    ethernet_header_t* eth_hdr = (ethernet_header_t*)packet->data;
    network_mac_addr_copy(&eth_hdr->dest_mac, dest);
    network_mac_addr_copy(&eth_hdr->src_mac, &iface->mac_address);
    eth_hdr->ethertype = htons(ethertype);
    
    // Copy payload
    memcpy(packet->data + sizeof(ethernet_header_t), data, length);
    packet->length = sizeof(ethernet_header_t) + length;
    
    // Send packet
    int result = network_packet_send(iface, packet);
    network_packet_free(packet);
    
    return result;
}

/* ================================
 * Wi-Fi Driver Functions
 * ================================ */

int wifi_driver_init(void) {
    printf("Initializing Wi-Fi drivers...\n");
    
    // Initialize generic Wi-Fi driver
    wifi_generic_init(NULL); // Will scan for Wi-Fi devices
    
    return NETWORK_SUCCESS;
}

int wifi_detect_interfaces(void) {
    int detected = 0;
    
    // Scan PCI bus for Wi-Fi devices
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t device = 0; device < 32; device++) {
            for (uint32_t function = 0; function < 8; function++) {
                uint16_t vendor_id = pci_read_word(bus, device, function, 0x00);
                if (vendor_id == 0xFFFF) continue;
                
                uint16_t device_id = pci_read_word(bus, device, function, 0x02);
                
                // Check if this is a known Wi-Fi device
                for (int i = 0; network_pci_devices[i].vendor_id != 0; i++) {
                    if (network_pci_devices[i].vendor_id == vendor_id &&
                        network_pci_devices[i].device_id == device_id &&
                        network_pci_devices[i].type == NETWORK_TYPE_WIFI) {
                        
                        printf("Found Wi-Fi device: %s (VID:%04X DID:%04X)\n",
                               network_pci_devices[i].name, vendor_id, device_id);
                        detected++;
                        
                        // TODO: Initialize specific driver based on device ID
                        break;
                    }
                }
            }
        }
    }
    
    return detected;
}

int wifi_scan_networks(network_interface_t* iface) {
    if (!iface || iface->type != NETWORK_TYPE_WIFI) {
        return NETWORK_ERROR_INVALID_PARAM;
    }
    
    if (!iface->ops->wifi_scan) {
        return NETWORK_ERROR_DRIVER_ERROR;
    }
    
    return iface->ops->wifi_scan(iface, iface->available_networks, 16);
}

int wifi_connect_network(network_interface_t* iface, const char* ssid, const char* password, uint8_t security_type) {
    if (!iface || iface->type != NETWORK_TYPE_WIFI || !ssid) {
        return NETWORK_ERROR_INVALID_PARAM;
    }
    
    if (!iface->ops->wifi_connect) {
        return NETWORK_ERROR_DRIVER_ERROR;
    }
    
    wifi_config_t config;
    strncpy(config.ssid, ssid, NETWORK_SSID_MAX_LENGTH);
    if (password) {
        strncpy(config.password, password, NETWORK_PASSWORD_MAX_LENGTH);
    } else {
        config.password[0] = '\0';
    }
    config.security_type = security_type;
    config.auto_connect = true;
    
    iface->state = NETWORK_STATE_CONNECTING;
    int result = iface->ops->wifi_connect(iface, &config);
    
    if (result == NETWORK_SUCCESS) {
        iface->state = NETWORK_STATE_CONNECTED;
        iface->wifi_config = config;
        printf("Connected to Wi-Fi network: %s\n", ssid);
    } else {
        iface->state = NETWORK_STATE_DOWN;
        printf("Failed to connect to Wi-Fi network: %s\n", ssid);
    }
    
    return result;
}

/* ================================
 * Network Stack Integration
 * ================================ */

int network_stack_init(void) {
    printf("Initializing network stack...\n");
    
    // Initialize protocol handlers
    // TODO: Register IP, ARP, and other protocol handlers
    
    return NETWORK_SUCCESS;
}

/* ================================
 * Utility Functions
 * ================================ */

void network_mac_addr_copy(network_mac_addr_t* dest, network_mac_addr_t* src) {
    if (dest && src) {
        memcpy(dest->addr, src->addr, NETWORK_MAC_ADDRESS_SIZE);
    }
}

bool network_mac_addr_equal(network_mac_addr_t* addr1, network_mac_addr_t* addr2) {
    if (!addr1 || !addr2) {
        return false;
    }
    return memcmp(addr1->addr, addr2->addr, NETWORK_MAC_ADDRESS_SIZE) == 0;
}

void network_ip_addr_copy(network_ip_addr_t* dest, network_ip_addr_t* src) {
    if (dest && src) {
        memcpy(dest->addr, src->addr, NETWORK_IP_ADDRESS_SIZE);
    }
}

bool network_ip_addr_equal(network_ip_addr_t* addr1, network_ip_addr_t* addr2) {
    if (!addr1 || !addr2) {
        return false;
    }
    return memcmp(addr1->addr, addr2->addr, NETWORK_IP_ADDRESS_SIZE) == 0;
}

char* network_mac_addr_to_string(network_mac_addr_t* mac) {
    static char buffer[18];
    if (!mac) {
        return "00:00:00:00:00:00";
    }
    
    snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac->addr[0], mac->addr[1], mac->addr[2],
             mac->addr[3], mac->addr[4], mac->addr[5]);
    
    return buffer;
}

char* network_ip_addr_to_string(network_ip_addr_t* ip) {
    static char buffer[16];
    if (!ip) {
        return "0.0.0.0";
    }
    
    snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u",
             ip->addr[0], ip->addr[1], ip->addr[2], ip->addr[3]);
    
    return buffer;
}

void network_print_interface_info(network_interface_t* iface) {
    if (!iface || !iface->initialized) {
        return;
    }
    
    printf("\nInterface: %s (ID: %u)\n", iface->name, iface->id);
    printf("  Type: %s\n", 
           iface->type == NETWORK_TYPE_ETHERNET ? "Ethernet" :
           iface->type == NETWORK_TYPE_WIFI ? "Wi-Fi" : "Unknown");
    printf("  State: %s\n",
           iface->state == NETWORK_STATE_UP ? "UP" :
           iface->state == NETWORK_STATE_DOWN ? "DOWN" :
           iface->state == NETWORK_STATE_CONNECTING ? "CONNECTING" : "UNKNOWN");
    printf("  MAC Address: %s\n", network_mac_addr_to_string(&iface->mac_address));
    printf("  IP Address: %s\n", network_ip_addr_to_string(&iface->ip_address));
    printf("  MTU: %u\n", iface->mtu);
    printf("  TX Packets: %lu, RX Packets: %lu\n", iface->stats.tx_packets, iface->stats.rx_packets);
    printf("  TX Bytes: %lu, RX Bytes: %lu\n", iface->stats.tx_bytes, iface->stats.rx_bytes);
    
    if (iface->type == NETWORK_TYPE_WIFI && iface->current_network.connected) {
        printf("  Connected to: %s\n", iface->current_network.ssid);
        printf("  Signal Strength: %d dBm\n", iface->current_network.signal_strength);
    }
}

void network_print_all_interfaces(void) {
    printf("\n=== Network Interfaces ===\n");
    
    for (uint32_t i = 0; i < NETWORK_MAX_INTERFACES; i++) {
        network_interface_t* iface = &g_network_manager.interfaces[i];
        if (iface->initialized) {
            network_print_interface_info(iface);
        }
    }
    
    printf("\nTotal interfaces: %u\n", g_network_manager.interface_count);
    printf("Packets allocated: %u\n", g_network_manager.packets_allocated);
}

/* ================================
 * Static Helper Functions
 * ================================ */

static int network_allocate_interface_id(void) {
    static uint32_t next_id = 1;
    return next_id++;
}

static void network_update_statistics(network_interface_t* iface, bool tx, uint32_t bytes, bool error) {
    if (!iface) {
        return;
    }
    
    if (tx) {
        if (error) {
            iface->stats.tx_errors++;
        } else {
            iface->stats.tx_packets++;
            iface->stats.tx_bytes += bytes;
            g_network_manager.total_tx_packets++;
            g_network_manager.total_tx_bytes += bytes;
        }
    } else {
        if (error) {
            iface->stats.rx_errors++;
        } else {
            iface->stats.rx_packets++;
            iface->stats.rx_bytes += bytes;
            g_network_manager.total_rx_packets++;
            g_network_manager.total_rx_bytes += bytes;
        }
    }
    
    iface->last_activity = get_system_time();
}

static int network_detect_hardware(void) {
    printf("Detecting network hardware...\n");
    
    int ethernet_count = ethernet_detect_interfaces();
    int wifi_count = wifi_detect_interfaces();
    
    printf("Found %d Ethernet interface(s)\n", ethernet_count);
    printf("Found %d Wi-Fi interface(s)\n", wifi_count);
    
    return (ethernet_count + wifi_count > 0) ? NETWORK_SUCCESS : NETWORK_ERROR_DRIVER_ERROR;
}

/* ================================
 * Error Handling
 * ================================ */

const char* network_get_error_string(network_error_t error) {
    switch (error) {
        case NETWORK_SUCCESS: return "Success";
        case NETWORK_ERROR_INVALID_PARAM: return "Invalid parameter";
        case NETWORK_ERROR_NO_MEMORY: return "Out of memory";
        case NETWORK_ERROR_NOT_INITIALIZED: return "Network system not initialized";
        case NETWORK_ERROR_INTERFACE_NOT_FOUND: return "Network interface not found";
        case NETWORK_ERROR_INTERFACE_DOWN: return "Network interface is down";
        case NETWORK_ERROR_TRANSMISSION_FAILED: return "Packet transmission failed";
        case NETWORK_ERROR_NO_LINK: return "No network link";
        case NETWORK_ERROR_TIMEOUT: return "Operation timed out";
        case NETWORK_ERROR_WIFI_NOT_CONNECTED: return "Wi-Fi not connected";
        case NETWORK_ERROR_WIFI_SCAN_FAILED: return "Wi-Fi scan failed";
        case NETWORK_ERROR_WIFI_CONNECT_FAILED: return "Wi-Fi connection failed";
        case NETWORK_ERROR_DRIVER_ERROR: return "Network driver error";
        case NETWORK_ERROR_PACKET_TOO_LARGE: return "Packet too large";
        case NETWORK_ERROR_QUEUE_FULL: return "Packet queue full";
        default: return "Unknown error";
    }
}
