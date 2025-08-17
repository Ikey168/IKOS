/* IKOS Network Stack - Test Program
 * Issue #35 - Network Stack Implementation
 * 
 * Comprehensive test suite for network stack functionality
 * including buffer management, device operations, and protocol processing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/net/network.h"
#include "../include/net/ethernet.h"
#include "../include/net/ip.h"
#include "../include/net/socket.h"

/* Test results */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test macros */
#define TEST_START(name) \
    do { \
        printf("Running test: %s...", name); \
        tests_run++; \
    } while(0)

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf(" FAILED\n"); \
            printf("  Assertion failed: %s\n", #condition); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define TEST_END() \
    do { \
        printf(" PASSED\n"); \
        tests_passed++; \
    } while(0)

/* Mock Network Device */
typedef struct {
    netdev_t netdev;
    bool is_open;
    netbuf_t* last_tx_buf;
    uint32_t tx_count;
} mock_netdev_t;

static int mock_netdev_open(netdev_t* dev) {
    mock_netdev_t* mock = (mock_netdev_t*)dev->private_data;
    mock->is_open = true;
    return NET_SUCCESS;
}

static int mock_netdev_close(netdev_t* dev) {
    mock_netdev_t* mock = (mock_netdev_t*)dev->private_data;
    mock->is_open = false;
    return NET_SUCCESS;
}

static int mock_netdev_start_xmit(netdev_t* dev, netbuf_t* buf) {
    mock_netdev_t* mock = (mock_netdev_t*)dev->private_data;
    mock->last_tx_buf = buf;
    mock->tx_count++;
    return NET_SUCCESS;
}

static struct netdev_ops mock_netdev_ops = {
    .open = mock_netdev_open,
    .close = mock_netdev_close,
    .start_xmit = mock_netdev_start_xmit,
    .set_config = NULL,
    .get_stats = NULL,
    .set_mac_addr = NULL,
    .ioctl = NULL
};

static mock_netdev_t* create_mock_netdev(const char* name) {
    mock_netdev_t* mock = malloc(sizeof(mock_netdev_t));
    if (!mock) return NULL;
    
    memset(mock, 0, sizeof(mock_netdev_t));
    
    /* Initialize netdev */
    strncpy(mock->netdev.name, name, sizeof(mock->netdev.name) - 1);
    mock->netdev.type = NETDEV_TYPE_ETHERNET;
    mock->netdev.mtu = 1500;
    mock->netdev.ops = &mock_netdev_ops;
    mock->netdev.private_data = mock;
    
    /* Set MAC address */
    mock->netdev.hw_addr.addr[0] = 0x02;
    mock->netdev.hw_addr.addr[1] = 0x00;
    mock->netdev.hw_addr.addr[2] = 0x00;
    mock->netdev.hw_addr.addr[3] = 0x12;
    mock->netdev.hw_addr.addr[4] = 0x34;
    mock->netdev.hw_addr.addr[5] = 0x56;
    
    return mock;
}

/* ================================
 * Network Buffer Tests
 * ================================ */

void test_netbuf_allocation(void) {
    TEST_START("Network Buffer Allocation");
    
    /* Test buffer allocation */
    netbuf_t* buf = netbuf_alloc(1500);
    TEST_ASSERT(buf != NULL);
    TEST_ASSERT(buf->size >= 1500);
    TEST_ASSERT(buf->len == 0);
    TEST_ASSERT(buf->data != NULL);
    
    /* Test buffer operations */
    TEST_ASSERT(netbuf_reserve(buf, 64) == NET_SUCCESS);
    TEST_ASSERT(buf->head == 64);
    TEST_ASSERT(buf->tail == 64);
    
    /* Test putting data */
    TEST_ASSERT(netbuf_put(buf, 100) == NET_SUCCESS);
    TEST_ASSERT(buf->len == 100);
    TEST_ASSERT(buf->tail == 164);
    
    /* Test pulling data */
    TEST_ASSERT(netbuf_pull(buf, 20) == NET_SUCCESS);
    TEST_ASSERT(buf->len == 80);
    TEST_ASSERT(buf->head == 84);
    
    /* Test pushing data */
    TEST_ASSERT(netbuf_push(buf, 10) == NET_SUCCESS);
    TEST_ASSERT(buf->len == 90);
    TEST_ASSERT(buf->head == 74);
    
    /* Free buffer */
    netbuf_free(buf);
    
    TEST_END();
}

void test_netbuf_pool(void) {
    TEST_START("Network Buffer Pool");
    
    /* Allocate multiple buffers */
    netbuf_t* buffers[10];
    for (int i = 0; i < 10; i++) {
        buffers[i] = netbuf_alloc(1000);
        TEST_ASSERT(buffers[i] != NULL);
    }
    
    /* Free all buffers */
    for (int i = 0; i < 10; i++) {
        netbuf_free(buffers[i]);
    }
    
    /* Reallocate to test recycling */
    netbuf_t* buf = netbuf_alloc(1000);
    TEST_ASSERT(buf != NULL);
    netbuf_free(buf);
    
    TEST_END();
}

/* ================================
 * Network Device Tests
 * ================================ */

void test_netdev_registration(void) {
    TEST_START("Network Device Registration");
    
    /* Create mock device */
    mock_netdev_t* mock = create_mock_netdev("eth0");
    TEST_ASSERT(mock != NULL);
    
    /* Register device */
    TEST_ASSERT(netdev_register(&mock->netdev) == NET_SUCCESS);
    
    /* Find device by name */
    netdev_t* dev = netdev_get_by_name("eth0");
    TEST_ASSERT(dev == &mock->netdev);
    
    /* Find device by index */
    dev = netdev_get_by_index(0);
    TEST_ASSERT(dev == &mock->netdev);
    
    /* Unregister device */
    TEST_ASSERT(netdev_unregister(&mock->netdev) == NET_SUCCESS);
    
    /* Device should no longer be found */
    dev = netdev_get_by_name("eth0");
    TEST_ASSERT(dev == NULL);
    
    free(mock);
    
    TEST_END();
}

void test_netdev_operations(void) {
    TEST_START("Network Device Operations");
    
    /* Create and register mock device */
    mock_netdev_t* mock = create_mock_netdev("eth1");
    TEST_ASSERT(mock != NULL);
    TEST_ASSERT(netdev_register(&mock->netdev) == NET_SUCCESS);
    
    /* Test device up/down */
    TEST_ASSERT(netdev_up(&mock->netdev) == NET_SUCCESS);
    TEST_ASSERT(mock->is_open == true);
    TEST_ASSERT(mock->netdev.flags & NETDEV_FLAG_UP);
    
    TEST_ASSERT(netdev_down(&mock->netdev) == NET_SUCCESS);
    TEST_ASSERT(mock->is_open == false);
    TEST_ASSERT(!(mock->netdev.flags & NETDEV_FLAG_UP));
    
    /* Test transmission */
    netdev_up(&mock->netdev);
    netbuf_t* buf = netbuf_alloc(100);
    TEST_ASSERT(buf != NULL);
    netbuf_put(buf, 64);
    
    TEST_ASSERT(netdev_transmit(&mock->netdev, buf) == NET_SUCCESS);
    TEST_ASSERT(mock->tx_count == 1);
    TEST_ASSERT(mock->last_tx_buf == buf);
    
    netbuf_free(buf);
    netdev_unregister(&mock->netdev);
    free(mock);
    
    TEST_END();
}

/* ================================
 * Ethernet Tests
 * ================================ */

void test_ethernet_addresses(void) {
    TEST_START("Ethernet Address Operations");
    
    /* Test address parsing */
    eth_addr_t addr;
    TEST_ASSERT(eth_addr_from_string("02:00:00:12:34:56", &addr) == NET_SUCCESS);
    TEST_ASSERT(addr.addr[0] == 0x02);
    TEST_ASSERT(addr.addr[1] == 0x00);
    TEST_ASSERT(addr.addr[5] == 0x56);
    
    /* Test address formatting */
    char buf[18];
    TEST_ASSERT(eth_addr_to_string(&addr, buf, sizeof(buf)) != NULL);
    TEST_ASSERT(strcmp(buf, "02:00:00:12:34:56") == 0);
    
    /* Test address types */
    eth_addr_t broadcast = ETH_ADDR_BROADCAST;
    TEST_ASSERT(eth_addr_is_broadcast(&broadcast));
    TEST_ASSERT(!eth_addr_is_multicast(&broadcast));
    TEST_ASSERT(!eth_addr_is_unicast(&broadcast));
    
    eth_addr_t multicast = {{0x01, 0x00, 0x5e, 0x00, 0x00, 0x01}};
    TEST_ASSERT(!eth_addr_is_broadcast(&multicast));
    TEST_ASSERT(eth_addr_is_multicast(&multicast));
    TEST_ASSERT(!eth_addr_is_unicast(&multicast));
    
    TEST_ASSERT(!eth_addr_is_broadcast(&addr));
    TEST_ASSERT(!eth_addr_is_multicast(&addr));
    TEST_ASSERT(eth_addr_is_unicast(&addr));
    
    TEST_END();
}

void test_ethernet_frame_processing(void) {
    TEST_START("Ethernet Frame Processing");
    
    /* Create mock device */
    mock_netdev_t* mock = create_mock_netdev("eth2");
    TEST_ASSERT(mock != NULL);
    TEST_ASSERT(netdev_register(&mock->netdev) == NET_SUCCESS);
    TEST_ASSERT(netdev_up(&mock->netdev) == NET_SUCCESS);
    
    /* Test frame transmission */
    eth_addr_t dest = {{0x02, 0x00, 0x00, 0x12, 0x34, 0x57}};
    const char* data = "Hello, Network!";
    
    TEST_ASSERT(eth_send_packet(&mock->netdev, &dest, ETH_TYPE_IP, 
                               data, strlen(data)) == NET_SUCCESS);
    TEST_ASSERT(mock->tx_count == 1);
    
    /* Cleanup */
    netdev_unregister(&mock->netdev);
    free(mock);
    
    TEST_END();
}

/* ================================
 * IP Address Tests
 * ================================ */

void test_ip_addresses(void) {
    TEST_START("IP Address Operations");
    
    /* Test IP address parsing */
    uint32_t ip = ip_addr_from_string("192.168.1.100");
    TEST_ASSERT(ip != 0);
    
    /* Test IP address formatting */
    char buf[16];
    ip_addr_t addr = {ip};
    TEST_ASSERT(ip_addr_to_string(addr, buf, sizeof(buf)) != NULL);
    TEST_ASSERT(strcmp(buf, "192.168.1.100") == 0);
    
    /* Test special addresses */
    ip_addr_t loopback = {htonl(IP_ADDR_LOOPBACK)};
    char loopback_str[16];
    ip_addr_to_string(loopback, loopback_str, sizeof(loopback_str));
    TEST_ASSERT(strcmp(loopback_str, "127.0.0.1") == 0);
    
    TEST_END();
}

/* ================================
 * Socket Tests
 * ================================ */

void test_socket_creation(void) {
    TEST_START("Socket Creation");
    
    /* Test socket creation */
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    TEST_ASSERT(sockfd >= 0);
    
    /* Test socket closure */
    TEST_ASSERT(close(sockfd) == 0);
    
    /* Test UDP socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(sockfd >= 0);
    TEST_ASSERT(close(sockfd) == 0);
    
    TEST_END();
}

/* ================================
 * Integration Tests
 * ================================ */

void test_network_stack_integration(void) {
    TEST_START("Network Stack Integration");
    
    /* Test basic initialization */
    TEST_ASSERT(network_init() == NET_SUCCESS);
    
    /* Create and register a device */
    mock_netdev_t* mock = create_mock_netdev("eth3");
    TEST_ASSERT(mock != NULL);
    TEST_ASSERT(netdev_register(&mock->netdev) == NET_SUCCESS);
    
    /* Configure device */
    mock->netdev.ip_addr.addr = ip_addr_from_string("10.0.0.1");
    mock->netdev.netmask.addr = ip_addr_from_string("255.255.255.0");
    mock->netdev.gateway.addr = ip_addr_from_string("10.0.0.1");
    
    TEST_ASSERT(netdev_up(&mock->netdev) == NET_SUCCESS);
    
    /* Test packet processing path */
    netbuf_t* buf = netbuf_alloc(64);
    TEST_ASSERT(buf != NULL);
    netbuf_put(buf, 64);
    
    /* Simulate packet reception */
    TEST_ASSERT(netdev_receive_packet(&mock->netdev, buf) != NET_ERROR_INVALID);
    
    netbuf_free(buf);
    netdev_unregister(&mock->netdev);
    free(mock);
    
    TEST_END();
}

/* ================================
 * Performance Tests
 * ================================ */

void test_performance(void) {
    TEST_START("Performance Tests");
    
    /* Test buffer allocation/deallocation performance */
    const int iterations = 1000;
    netbuf_t* buffers[10];
    
    for (int i = 0; i < iterations; i++) {
        /* Allocate buffers */
        for (int j = 0; j < 10; j++) {
            buffers[j] = netbuf_alloc(1500);
            TEST_ASSERT(buffers[j] != NULL);
        }
        
        /* Free buffers */
        for (int j = 0; j < 10; j++) {
            netbuf_free(buffers[j]);
        }
    }
    
    TEST_END();
}

/* ================================
 * Error Handling Tests
 * ================================ */

void test_error_handling(void) {
    TEST_START("Error Handling");
    
    /* Test invalid parameters */
    TEST_ASSERT(netbuf_alloc(0) == NULL);
    TEST_ASSERT(netdev_register(NULL) == NET_ERROR_INVALID);
    TEST_ASSERT(netdev_get_by_name(NULL) == NULL);
    TEST_ASSERT(netdev_get_by_name("nonexistent") == NULL);
    
    /* Test invalid socket operations */
    TEST_ASSERT(socket(-1, SOCK_STREAM, IPPROTO_TCP) == INVALID_SOCKET);
    TEST_ASSERT(close(-1) != 0);
    
    /* Test invalid Ethernet addresses */
    eth_addr_t invalid_addr;
    TEST_ASSERT(eth_addr_from_string("invalid", &invalid_addr) != NET_SUCCESS);
    TEST_ASSERT(eth_addr_from_string("FF:FF:FF:FF:FF:GG", &invalid_addr) != NET_SUCCESS);
    
    /* Test invalid IP addresses */
    TEST_ASSERT(ip_addr_from_string("invalid") == 0);
    TEST_ASSERT(ip_addr_from_string("256.1.1.1") == 0);
    
    TEST_END();
}

/* ================================
 * Test Runner
 * ================================ */

void run_all_tests(void) {
    printf("IKOS Network Stack Test Suite\n");
    printf("==============================\n\n");
    
    /* Initialize network stack */
    if (network_init() != NET_SUCCESS) {
        printf("Failed to initialize network stack\n");
        return;
    }
    
    /* Run tests */
    test_netbuf_allocation();
    test_netbuf_pool();
    test_netdev_registration();
    test_netdev_operations();
    test_ethernet_addresses();
    test_ethernet_frame_processing();
    test_ip_addresses();
    test_socket_creation();
    test_network_stack_integration();
    test_performance();
    test_error_handling();
    
    /* Print results */
    printf("\nTest Results:\n");
    printf("=============\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\nAll tests PASSED! ✅\n");
    } else {
        printf("\n%d tests FAILED! ❌\n", tests_failed);
    }
    
    /* Cleanup */
    network_shutdown();
}

/* ================================
 * Main Function
 * ================================ */

int main(void) {
    run_all_tests();
    return tests_failed > 0 ? 1 : 0;
}
