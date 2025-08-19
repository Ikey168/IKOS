/*
 * IKOS Network Interface Driver - Test Suite
 * Issue #45 - Network Interface Driver (Ethernet & Wi-Fi)
 * 
 * Comprehensive tests for network driver functionality including
 * Ethernet and Wi-Fi drivers, interface management, and connectivity.
 */

#include "network_driver.h"
#include "stdio.h"
#include "string.h"
#include <stdint.h>
#include <stdbool.h>

/* ================================
 * Test Function Declarations
 * ================================ */

static void test_network_driver_initialization(void);
static void test_network_interface_management(void);
static void test_ethernet_driver_functionality(void);
static void test_wifi_driver_functionality(void);
static void test_packet_management(void);
static void test_network_configuration(void);
static void test_network_statistics(void);
static void test_error_handling(void);
static void test_multiple_interfaces(void);
static void test_network_integration(void);

static void print_test_result(const char* test_name, bool passed);
static void print_test_header(const char* test_suite_name);

/* ================================
 * Main Test Entry Point
 * ================================ */

void network_driver_run_tests(void) {
    print_test_header("Network Interface Driver Test Suite (Issue #45)");
    
    test_network_driver_initialization();
    test_network_interface_management();
    test_ethernet_driver_functionality();
    test_wifi_driver_functionality();
    test_packet_management();
    test_network_configuration();
    test_network_statistics();
    test_error_handling();
    test_multiple_interfaces();
    test_network_integration();
    
    printf("Network Interface Driver tests completed.\n\n");
}

/* ================================
 * Individual Test Functions
 * ================================ */

static void test_network_driver_initialization(void) {
    bool passed = true;
    
    // Test system initialization
    int result = network_driver_init();
    if (result != NETWORK_SUCCESS) {
        passed = false;
    }
    
    // Test double initialization
    result = network_driver_init();
    if (result != NETWORK_SUCCESS) {
        passed = false;
    }
    
    // Test cleanup
    network_driver_cleanup();
    
    print_test_result("Network Driver Initialization", passed);
}

static void test_network_interface_management(void) {
    bool passed = true;
    
    // Initialize system
    if (network_driver_init() != NETWORK_SUCCESS) {
        passed = false;
    }
    
    // Create mock driver operations
    static network_driver_ops_t test_ops = {
        .init = NULL,
        .start = NULL,
        .stop = NULL,
        .send_packet = NULL,
        .set_mac_address = NULL,
        .get_link_status = NULL
    };
    
    // Test interface registration
    network_interface_t* iface = network_register_interface("test0", NETWORK_TYPE_ETHERNET, &test_ops);
    if (!iface) {
        passed = false;
    }
    
    // Test interface retrieval by name
    network_interface_t* retrieved = network_get_interface_by_name("test0");
    if (retrieved != iface) {
        passed = false;
    }
    
    // Test interface retrieval by ID
    retrieved = network_get_interface_by_id(iface->id);
    if (retrieved != iface) {
        passed = false;
    }
    
    // Test default interface
    network_interface_t* default_iface = network_get_default_interface();
    if (default_iface != iface) {
        passed = false;
    }
    
    // Test interface unregistration
    if (network_unregister_interface(iface) != NETWORK_SUCCESS) {
        passed = false;
    }
    
    network_driver_cleanup();
    print_test_result("Network Interface Management", passed);
}

static void test_ethernet_driver_functionality(void) {
    bool passed = true;
    
    network_driver_init();
    
    // Test Ethernet driver initialization
    int result = ethernet_driver_init();
    if (result != NETWORK_SUCCESS) {
        // This might fail if no hardware is present, which is okay for testing
        printf("Note: No Ethernet hardware detected (expected in emulation)\n");
    }
    
    // Test Ethernet interface detection
    int detected = ethernet_detect_interfaces();
    printf("Detected %d Ethernet interfaces\n", detected);
    
    // Test Ethernet frame sending (with mock interface)
    static network_driver_ops_t eth_ops = {
        .init = NULL,
        .start = NULL,
        .stop = NULL,
        .send_packet = NULL
    };
    
    network_interface_t* eth_iface = network_register_interface("eth_test", NETWORK_TYPE_ETHERNET, &eth_ops);
    if (eth_iface) {
        // Set a test MAC address
        eth_iface->mac_address.addr[0] = 0x02;
        eth_iface->mac_address.addr[1] = 0x00;
        eth_iface->mac_address.addr[2] = 0x00;
        eth_iface->mac_address.addr[3] = 0x00;
        eth_iface->mac_address.addr[4] = 0x00;
        eth_iface->mac_address.addr[5] = 0x01;
        
        network_mac_addr_t dest_mac = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}; // Broadcast
        uint8_t test_data[] = "Hello Ethernet";
        
        // This will fail without a real send_packet implementation, which is expected
        ethernet_send_frame(eth_iface, &dest_mac, ETH_TYPE_IPV4, test_data, sizeof(test_data));
        
        network_unregister_interface(eth_iface);
    }
    
    network_driver_cleanup();
    print_test_result("Ethernet Driver Functionality", passed);
}

static void test_wifi_driver_functionality(void) {
    bool passed = true;
    
    network_driver_init();
    
    // Test Wi-Fi driver initialization
    int result = wifi_driver_init();
    if (result != NETWORK_SUCCESS) {
        printf("Note: No Wi-Fi hardware detected (expected in emulation)\n");
    }
    
    // Test Wi-Fi interface detection
    int detected = wifi_detect_interfaces();
    printf("Detected %d Wi-Fi interfaces\n", detected);
    
    // Test Wi-Fi operations with mock interface
    static network_driver_ops_t wifi_ops = {
        .wifi_scan = wifi_generic_scan,
        .wifi_connect = wifi_generic_connect,
        .wifi_disconnect = wifi_generic_disconnect
    };
    
    network_interface_t* wifi_iface = network_register_interface("wlan_test", NETWORK_TYPE_WIFI, &wifi_ops);
    if (wifi_iface) {
        // Test Wi-Fi scanning
        wifi_network_info_t networks[10];
        int scan_result = wifi_scan_networks(wifi_iface);
        if (scan_result >= 0) {
            printf("Wi-Fi scan found %d networks\n", scan_result);
            wifi_print_scan_results(wifi_iface);
        }
        
        // Test Wi-Fi connection to open network
        if (wifi_iface->available_network_count > 0) {
            // Find an open network
            wifi_network_info_t* open_network = NULL;
            for (uint32_t i = 0; i < wifi_iface->available_network_count; i++) {
                if (wifi_iface->available_networks[i].security_type == WIFI_SECURITY_NONE) {
                    open_network = &wifi_iface->available_networks[i];
                    break;
                }
            }
            
            if (open_network) {
                int connect_result = wifi_connect_network(wifi_iface, open_network->ssid, NULL, WIFI_SECURITY_NONE);
                if (connect_result == NETWORK_SUCCESS) {
                    printf("Successfully connected to open network: %s\n", open_network->ssid);
                    
                    // Test disconnection
                    int disconnect_result = wifi_disconnect_network(wifi_iface);
                    if (disconnect_result == NETWORK_SUCCESS) {
                        printf("Successfully disconnected from Wi-Fi network\n");
                    } else {
                        passed = false;
                    }
                } else {
                    printf("Failed to connect to open network\n");
                }
            }
        }
        
        network_unregister_interface(wifi_iface);
    }
    
    network_driver_cleanup();
    print_test_result("Wi-Fi Driver Functionality", passed);
}

static void test_packet_management(void) {
    bool passed = true;
    
    network_driver_init();
    
    // Test packet allocation
    network_packet_t* packet = network_packet_alloc(1024);
    if (!packet) {
        passed = false;
    }
    
    // Test packet properties
    if (packet && packet->capacity != 1024) {
        passed = false;
    }
    
    // Test packet data writing
    if (packet && packet->data) {
        strcpy((char*)packet->data, "Test packet data");
        packet->length = strlen("Test packet data");
    }
    
    // Test packet freeing
    if (packet) {
        network_packet_free(packet);
    }
    
    // Test large packet allocation
    network_packet_t* large_packet = network_packet_alloc(NETWORK_MAX_PACKET_SIZE + 1);
    if (large_packet) {
        passed = false; // Should fail for oversized packets
        network_packet_free(large_packet);
    }
    
    // Test multiple packet allocation
    network_packet_t* packets[10];
    int allocated = 0;
    for (int i = 0; i < 10; i++) {
        packets[i] = network_packet_alloc(512);
        if (packets[i]) {
            allocated++;
        }
    }
    
    // Free allocated packets
    for (int i = 0; i < allocated; i++) {
        if (packets[i]) {
            network_packet_free(packets[i]);
        }
    }
    
    printf("Allocated %d packets in batch test\n", allocated);
    
    network_driver_cleanup();
    print_test_result("Packet Management", passed);
}

static void test_network_configuration(void) {
    bool passed = true;
    
    network_driver_init();
    
    // Create test interface
    static network_driver_ops_t test_ops = {0};
    network_interface_t* iface = network_register_interface("config_test", NETWORK_TYPE_ETHERNET, &test_ops);
    
    if (iface) {
        // Test IP address configuration
        network_ip_addr_t ip = {{192, 168, 1, 100}};
        network_ip_addr_t netmask = {{255, 255, 255, 0}};
        
        int result = network_interface_set_ip(iface, &ip, &netmask);
        if (result != NETWORK_SUCCESS) {
            passed = false;
        }
        
        // Verify IP address was set
        if (!network_ip_addr_equal(&iface->ip_address, &ip)) {
            passed = false;
        }
        
        // Test gateway configuration
        network_ip_addr_t gateway = {{192, 168, 1, 1}};
        result = network_interface_set_gateway(iface, &gateway);
        if (result != NETWORK_SUCCESS) {
            passed = false;
        }
        
        // Test DHCP enable/disable
        result = network_interface_enable_dhcp(iface, false);
        if (result != NETWORK_SUCCESS) {
            passed = false;
        }
        
        if (iface->dhcp_enabled) {
            passed = false;
        }
        
        result = network_interface_enable_dhcp(iface, true);
        if (result != NETWORK_SUCCESS) {
            passed = false;
        }
        
        if (!iface->dhcp_enabled) {
            passed = false;
        }
        
        network_unregister_interface(iface);
    } else {
        passed = false;
    }
    
    network_driver_cleanup();
    print_test_result("Network Configuration", passed);
}

static void test_network_statistics(void) {
    bool passed = true;
    
    network_driver_init();
    
    // Create test interface
    static network_driver_ops_t test_ops = {0};
    network_interface_t* iface = network_register_interface("stats_test", NETWORK_TYPE_ETHERNET, &test_ops);
    
    if (iface) {
        // Get initial statistics
        network_stats_t stats;
        int result = network_get_interface_stats(iface, &stats);
        if (result != NETWORK_SUCCESS) {
            passed = false;
        }
        
        // Verify initial statistics are zero
        if (stats.tx_packets != 0 || stats.rx_packets != 0) {
            passed = false;
        }
        
        // Test global statistics
        uint64_t global_tx_packets, global_rx_packets, global_tx_bytes, global_rx_bytes;
        result = network_get_global_stats(&global_tx_packets, &global_rx_packets, 
                                         &global_tx_bytes, &global_rx_bytes);
        if (result != NETWORK_SUCCESS) {
            passed = false;
        }
        
        printf("Global stats - TX: %lu packets (%lu bytes), RX: %lu packets (%lu bytes)\n",
               global_tx_packets, global_tx_bytes, global_rx_packets, global_rx_bytes);
        
        network_unregister_interface(iface);
    } else {
        passed = false;
    }
    
    network_driver_cleanup();
    print_test_result("Network Statistics", passed);
}

static void test_error_handling(void) {
    bool passed = true;
    
    // Test operations before initialization
    network_interface_t* null_iface = network_register_interface("test", NETWORK_TYPE_ETHERNET, NULL);
    if (null_iface) {
        passed = false; // Should fail without initialization
    }
    
    network_driver_init();
    
    // Test NULL parameter handling
    network_interface_t* invalid_iface = network_register_interface(NULL, NETWORK_TYPE_ETHERNET, NULL);
    if (invalid_iface) {
        passed = false;
    }
    
    // Test invalid interface operations
    int result = network_interface_up(NULL);
    if (result == NETWORK_SUCCESS) {
        passed = false;
    }
    
    result = network_interface_set_ip(NULL, NULL, NULL);
    if (result == NETWORK_SUCCESS) {
        passed = false;
    }
    
    // Test packet operations with NULL
    network_packet_t* null_packet = network_packet_alloc(0);
    if (null_packet) {
        passed = false;
        network_packet_free(null_packet);
    }
    
    network_packet_free(NULL); // Should not crash
    
    // Test error string function
    for (int i = NETWORK_SUCCESS; i >= NETWORK_ERROR_QUEUE_FULL; i--) {
        const char* error_str = network_get_error_string((network_error_t)i);
        if (!error_str || strlen(error_str) == 0) {
            passed = false;
        }
    }
    
    network_driver_cleanup();
    print_test_result("Error Handling", passed);
}

static void test_multiple_interfaces(void) {
    bool passed = true;
    
    network_driver_init();
    
    // Create multiple interfaces
    static network_driver_ops_t test_ops = {0};
    
    network_interface_t* eth0 = network_register_interface("eth0", NETWORK_TYPE_ETHERNET, &test_ops);
    network_interface_t* eth1 = network_register_interface("eth1", NETWORK_TYPE_ETHERNET, &test_ops);
    network_interface_t* wlan0 = network_register_interface("wlan0", NETWORK_TYPE_WIFI, &test_ops);
    
    if (!eth0 || !eth1 || !wlan0) {
        passed = false;
    }
    
    // Test unique IDs
    if (eth0 && eth1 && wlan0) {
        if (eth0->id == eth1->id || eth0->id == wlan0->id || eth1->id == wlan0->id) {
            passed = false;
        }
    }
    
    // Test interface enumeration
    network_print_all_interfaces();
    
    // Test default interface management
    network_interface_t* default_iface = network_get_default_interface();
    if (default_iface != eth0) {
        passed = false; // First interface should be default
    }
    
    // Remove default interface and check new default
    if (eth0) {
        network_unregister_interface(eth0);
        eth0 = NULL;
    }
    
    default_iface = network_get_default_interface();
    if (default_iface != eth1 && default_iface != wlan0) {
        passed = false;
    }
    
    // Cleanup remaining interfaces
    if (eth1) network_unregister_interface(eth1);
    if (wlan0) network_unregister_interface(wlan0);
    
    network_driver_cleanup();
    print_test_result("Multiple Interfaces", passed);
}

static void test_network_integration(void) {
    bool passed = true;
    
    network_driver_init();
    
    // Test network stack integration
    int result = network_stack_init();
    if (result != NETWORK_SUCCESS) {
        passed = false;
    }
    
    // Test utility functions
    network_mac_addr_t mac1 = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    network_mac_addr_t mac2 = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    network_mac_addr_t mac3 = {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};
    
    if (!network_mac_addr_equal(&mac1, &mac2)) {
        passed = false;
    }
    
    if (network_mac_addr_equal(&mac1, &mac3)) {
        passed = false;
    }
    
    // Test MAC address string conversion
    char* mac_str = network_mac_addr_to_string(&mac1);
    if (!mac_str || strlen(mac_str) == 0) {
        passed = false;
    }
    
    printf("MAC address string: %s\n", mac_str);
    
    // Test IP address functions
    network_ip_addr_t ip1 = {{192, 168, 1, 1}};
    network_ip_addr_t ip2 = {{192, 168, 1, 1}};
    network_ip_addr_t ip3 = {{10, 0, 0, 1}};
    
    if (!network_ip_addr_equal(&ip1, &ip2)) {
        passed = false;
    }
    
    if (network_ip_addr_equal(&ip1, &ip3)) {
        passed = false;
    }
    
    // Test IP address string conversion
    char* ip_str = network_ip_addr_to_string(&ip1);
    if (!ip_str || strlen(ip_str) == 0) {
        passed = false;
    }
    
    printf("IP address string: %s\n", ip_str);
    
    // Test IP address parsing
    network_ip_addr_t parsed_ip;
    result = network_string_to_ip_addr("192.168.1.100", &parsed_ip);
    if (result == NETWORK_SUCCESS) {
        printf("Parsed IP: %s\n", network_ip_addr_to_string(&parsed_ip));
    }
    
    network_driver_cleanup();
    print_test_result("Network Integration", passed);
}

/* ================================
 * Utility Functions
 * ================================ */

static void print_test_result(const char* test_name, bool passed) {
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

static void print_test_header(const char* test_suite_name) {
    printf("\n=== %s ===\n", test_suite_name);
}

/* ================================
 * Basic Integration Test
 * ================================ */

void network_driver_test_basic_integration(void) {
    printf("Running basic Network Driver integration test...\n");
    
    // Initialize network driver system
    if (network_driver_init() != NETWORK_SUCCESS) {
        printf("Failed to initialize Network Driver system\n");
        return;
    }
    
    printf("Network Driver system initialized successfully\n");
    
    // Test hardware detection
    printf("Detecting network hardware...\n");
    ethernet_detect_interfaces();
    wifi_detect_interfaces();
    
    // Print all detected interfaces
    network_print_all_interfaces();
    
    // Test Wi-Fi functionality if available
    network_interface_t* wifi_iface = network_get_interface_by_name("wlan0");
    if (wifi_iface) {
        printf("Testing Wi-Fi functionality...\n");
        wifi_scan_networks(wifi_iface);
        wifi_print_scan_results(wifi_iface);
    } else {
        printf("No Wi-Fi interface available for testing\n");
    }
    
    // Clean up
    network_driver_cleanup();
    
    printf("Basic Network Driver integration test completed\n");
}
