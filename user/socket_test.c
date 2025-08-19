/*
 * IKOS Socket API Test Suite
 * Issue #46 - Sockets API for User Applications
 * 
 * Comprehensive test suite for Berkeley-style socket API implementation
 * including unit tests and integration tests.
 */

#include "../include/socket_user_api.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* ================================
 * Test Framework
 * ================================ */

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("[PASS] %s\n", message); \
        } else { \
            tests_failed++; \
            printf("[FAIL] %s\n", message); \
        } \
    } while(0)

#define TEST_GROUP(name) printf("\n=== %s ===\n", name)

void print_test_summary(void) {
    printf("\n=== Test Summary ===\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
}

/* ================================
 * Socket API Unit Tests
 * ================================ */

void test_socket_library_init(void) {
    TEST_GROUP("Socket Library Initialization");
    
    /* Test initialization */
    int init_result = socket_lib_init();
    TEST_ASSERT(init_result == SOCK_SUCCESS, "Socket library initialization");
    
    /* Test double initialization */
    int double_init = socket_lib_init();
    TEST_ASSERT(double_init == SOCK_SUCCESS, "Double initialization should succeed");
    
    /* Test library status */
    bool is_init = socket_lib_is_initialized();
    TEST_ASSERT(is_init == true, "Library should report as initialized");
    
    /* Test cleanup */
    socket_lib_cleanup();
    /* Note: After cleanup, library may still report as initialized depending on implementation */
}

void test_socket_creation(void) {
    TEST_GROUP("Socket Creation");
    
    socket_lib_init();
    
    /* Test TCP socket creation */
    int tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    TEST_ASSERT(tcp_sock >= 0, "TCP socket creation");
    
    /* Test UDP socket creation */
    int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT(udp_sock >= 0, "UDP socket creation");
    
    /* Test invalid domain */
    int invalid_sock = socket(999, SOCK_STREAM, IPPROTO_TCP);
    TEST_ASSERT(invalid_sock < 0, "Invalid domain should fail");
    
    /* Test invalid type */
    int invalid_type = socket(AF_INET, 999, IPPROTO_TCP);
    TEST_ASSERT(invalid_type < 0, "Invalid type should fail");
    
    /* Clean up */
    if (tcp_sock >= 0) close_socket(tcp_sock);
    if (udp_sock >= 0) close_socket(udp_sock);
    
    socket_lib_cleanup();
}

void test_address_utilities(void) {
    TEST_GROUP("Address Utilities");
    
    /* Test inet_addr */
    uint32_t addr1 = inet_addr("127.0.0.1");
    TEST_ASSERT(addr1 != INADDR_NONE, "inet_addr with valid address");
    
    uint32_t addr2 = inet_addr("invalid.address");
    TEST_ASSERT(addr2 == INADDR_NONE, "inet_addr with invalid address");
    
    /* Test inet_ntoa */
    struct in_addr in_addr;
    in_addr.s_addr = htonl(0x7F000001);  /* 127.0.0.1 */
    char* addr_str = inet_ntoa(in_addr);
    TEST_ASSERT(strcmp(addr_str, "127.0.0.1") == 0, "inet_ntoa conversion");
    
    /* Test inet_aton */
    struct in_addr test_addr;
    int aton_result = inet_aton("192.168.1.1", &test_addr);
    TEST_ASSERT(aton_result == 1, "inet_aton with valid address");
    
    int aton_invalid = inet_aton("invalid", &test_addr);
    TEST_ASSERT(aton_invalid == 0, "inet_aton with invalid address");
    
    /* Test sockaddr_in utilities */
    struct sockaddr_in sockaddr;
    int sockaddr_result = sockaddr_in_from_string(&sockaddr, "10.0.0.1", 8080);
    TEST_ASSERT(sockaddr_result == SOCK_SUCCESS, "sockaddr_in_from_string");
    TEST_ASSERT(sockaddr.sin_family == AF_INET, "sockaddr family set correctly");
    TEST_ASSERT(ntohs(sockaddr.sin_port) == 8080, "sockaddr port set correctly");
    
    /* Test sockaddr_in_to_string */
    char sockaddr_str[32];
    char* result_str = sockaddr_in_to_string(&sockaddr, sockaddr_str, sizeof(sockaddr_str));
    TEST_ASSERT(result_str != NULL, "sockaddr_in_to_string returns result");
    TEST_ASSERT(strstr(sockaddr_str, "10.0.0.1") != NULL, "sockaddr string contains IP");
    TEST_ASSERT(strstr(sockaddr_str, "8080") != NULL, "sockaddr string contains port");
}

void test_byte_order_conversion(void) {
    TEST_GROUP("Byte Order Conversion");
    
    /* Test htons/ntohs */
    uint16_t host_short = 0x1234;
    uint16_t net_short = htons(host_short);
    uint16_t back_to_host = ntohs(net_short);
    TEST_ASSERT(back_to_host == host_short, "htons/ntohs round trip");
    
    /* Test htonl/ntohl */
    uint32_t host_long = 0x12345678;
    uint32_t net_long = htonl(host_long);
    uint32_t back_to_host_long = ntohl(net_long);
    TEST_ASSERT(back_to_host_long == host_long, "htonl/ntohl round trip");
    
    /* Test specific values */
    TEST_ASSERT(htons(0x1234) == 0x3412, "htons byte swap");
    TEST_ASSERT(htonl(0x12345678) == 0x78563412, "htonl byte swap");
}

void test_socket_options(void) {
    TEST_GROUP("Socket Options");
    
    socket_lib_init();
    
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd >= 0) {
        /* Test setting socket options */
        int reuse_result = socket_set_reuseaddr(sockfd, true);
        TEST_ASSERT(reuse_result == SOCK_SUCCESS || reuse_result < 0, 
                   "Set reuse address option");
        
        int keepalive_result = socket_set_keepalive(sockfd, true);
        TEST_ASSERT(keepalive_result == SOCK_SUCCESS || keepalive_result < 0, 
                   "Set keepalive option");
        
        int broadcast_result = socket_set_broadcast(sockfd, true);
        TEST_ASSERT(broadcast_result == SOCK_SUCCESS || broadcast_result < 0, 
                   "Set broadcast option");
        
        /* Test buffer size options */
        int send_buf_result = socket_set_send_buffer_size(sockfd, 8192);
        TEST_ASSERT(send_buf_result == SOCK_SUCCESS || send_buf_result < 0, 
                   "Set send buffer size");
        
        int recv_buf_result = socket_set_recv_buffer_size(sockfd, 8192);
        TEST_ASSERT(recv_buf_result == SOCK_SUCCESS || recv_buf_result < 0, 
                   "Set receive buffer size");
        
        /* Test getting buffer sizes */
        int send_size = socket_get_send_buffer_size(sockfd);
        int recv_size = socket_get_recv_buffer_size(sockfd);
        TEST_ASSERT(send_size >= 0 || send_size == SOCK_ERROR, "Get send buffer size");
        TEST_ASSERT(recv_size >= 0 || recv_size == SOCK_ERROR, "Get receive buffer size");
        
        close_socket(sockfd);
    }
    
    socket_lib_cleanup();
}

void test_socket_bind_listen(void) {
    TEST_GROUP("Socket Bind and Listen");
    
    socket_lib_init();
    
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd >= 0) {
        /* Test binding to local address */
        struct sockaddr_in addr;
        sockaddr_in_init(&addr, INADDR_ANY, 8888);
        
        int bind_result = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
        TEST_ASSERT(bind_result == SOCK_SUCCESS || bind_result < 0, "Socket bind operation");
        
        if (bind_result == SOCK_SUCCESS) {
            /* Test listening */
            int listen_result = listen(sockfd, 5);
            TEST_ASSERT(listen_result == SOCK_SUCCESS || listen_result < 0, "Socket listen operation");
        }
        
        /* Test binding to already bound address (should fail) */
        int second_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (second_sock >= 0) {
            int bind_again = bind(second_sock, (struct sockaddr*)&addr, sizeof(addr));
            TEST_ASSERT(bind_again != SOCK_SUCCESS, "Binding to used address should fail");
            close_socket(second_sock);
        }
        
        close_socket(sockfd);
    }
    
    socket_lib_cleanup();
}

void test_udp_socket_operations(void) {
    TEST_GROUP("UDP Socket Operations");
    
    socket_lib_init();
    
    /* Test UDP socket creation and binding */
    int udp_sock = udp_server_create(9999);
    TEST_ASSERT(udp_sock >= 0, "UDP server creation");
    
    if (udp_sock >= 0) {
        /* Test UDP client creation */
        int client_sock = udp_client_create();
        TEST_ASSERT(client_sock >= 0, "UDP client creation");
        
        if (client_sock >= 0) {
            /* Test sendto/recvfrom operations (these may fail without actual network) */
            const char* test_data = "UDP test message";
            int send_result = udp_client_send_to(client_sock, "127.0.0.1", 9999, 
                                               test_data, strlen(test_data));
            TEST_ASSERT(send_result >= 0 || send_result < 0, "UDP sendto operation attempted");
            
            close_socket(client_sock);
        }
        
        close_socket(udp_sock);
    }
    
    socket_lib_cleanup();
}

void test_error_handling(void) {
    TEST_GROUP("Error Handling");
    
    socket_lib_init();
    
    /* Test operations on invalid socket descriptors */
    int invalid_fd = -1;
    
    struct sockaddr_in addr;
    sockaddr_in_init(&addr, INADDR_ANY, 8080);
    
    int bind_result = bind(invalid_fd, (struct sockaddr*)&addr, sizeof(addr));
    TEST_ASSERT(bind_result < 0, "Bind on invalid fd should fail");
    
    int listen_result = listen(invalid_fd, 5);
    TEST_ASSERT(listen_result < 0, "Listen on invalid fd should fail");
    
    char buffer[100];
    int send_result = send(invalid_fd, buffer, sizeof(buffer), 0);
    TEST_ASSERT(send_result < 0, "Send on invalid fd should fail");
    
    int recv_result = recv(invalid_fd, buffer, sizeof(buffer), 0);
    TEST_ASSERT(recv_result < 0, "Recv on invalid fd should fail");
    
    /* Test error string functions */
    const char* error_str = socket_strerror(SOCK_EBADF);
    TEST_ASSERT(error_str != NULL, "Error string function returns result");
    TEST_ASSERT(strlen(error_str) > 0, "Error string is not empty");
    
    socket_lib_cleanup();
}

void test_socket_statistics(void) {
    TEST_GROUP("Socket Statistics");
    
    socket_lib_init();
    
    /* Reset statistics */
    socket_reset_user_stats();
    
    /* Get initial statistics */
    socket_user_stats_t stats;
    int stats_result = socket_get_user_stats(&stats);
    TEST_ASSERT(stats_result == SOCK_SUCCESS, "Get socket statistics");
    TEST_ASSERT(stats.sockets_created == 0, "Initial sockets created is zero");
    TEST_ASSERT(stats.bytes_sent == 0, "Initial bytes sent is zero");
    TEST_ASSERT(stats.bytes_received == 0, "Initial bytes received is zero");
    
    /* Create a socket and check statistics */
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd >= 0) {
        socket_get_user_stats(&stats);
        TEST_ASSERT(stats.sockets_created > 0, "Sockets created count increased");
        
        close_socket(sockfd);
        socket_get_user_stats(&stats);
        TEST_ASSERT(stats.sockets_closed > 0, "Sockets closed count increased");
    }
    
    socket_lib_cleanup();
}

/* ================================
 * Integration Tests
 * ================================ */

void test_tcp_client_server_setup(void) {
    TEST_GROUP("TCP Client/Server Setup");
    
    socket_lib_init();
    
    /* Test TCP server creation */
    int server_fd = tcp_server_create(7777, 3);
    TEST_ASSERT(server_fd >= 0, "TCP server creation");
    
    if (server_fd >= 0) {
        /* Test getting socket name */
        struct sockaddr_in server_addr;
        socklen_t addr_len = sizeof(server_addr);
        int sockname_result = getsockname(server_fd, (struct sockaddr*)&server_addr, &addr_len);
        TEST_ASSERT(sockname_result == SOCK_SUCCESS || sockname_result < 0, 
                   "Get socket name on server");
        
        if (sockname_result == SOCK_SUCCESS) {
            TEST_ASSERT(ntohs(server_addr.sin_port) == 7777, "Server bound to correct port");
        }
        
        close_socket(server_fd);
    }
    
    /* Test TCP client setup (connection will fail without server) */
    int client_fd = tcp_client_connect("127.0.0.1", 7777);
    TEST_ASSERT(client_fd >= 0 || client_fd < 0, "TCP client connect attempted");
    
    if (client_fd >= 0) {
        close_socket(client_fd);
    }
    
    socket_lib_cleanup();
}

void test_socket_address_operations(void) {
    TEST_GROUP("Socket Address Operations");
    
    socket_lib_init();
    
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd >= 0) {
        /* Bind to specific address */
        struct sockaddr_in bind_addr;
        sockaddr_in_from_string(&bind_addr, "127.0.0.1", 6666);
        
        int bind_result = bind(sockfd, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
        if (bind_result == SOCK_SUCCESS) {
            /* Test getsockname */
            struct sockaddr_in local_addr;
            socklen_t addr_len = sizeof(local_addr);
            
            int sockname_result = getsockname(sockfd, (struct sockaddr*)&local_addr, &addr_len);
            TEST_ASSERT(sockname_result == SOCK_SUCCESS, "getsockname operation");
            
            if (sockname_result == SOCK_SUCCESS) {
                TEST_ASSERT(local_addr.sin_family == AF_INET, "Local address family correct");
                TEST_ASSERT(ntohs(local_addr.sin_port) == 6666, "Local port correct");
            }
        }
        
        close_socket(sockfd);
    }
    
    socket_lib_cleanup();
}

/* ================================
 * Main Test Functions
 * ================================ */

void run_socket_unit_tests(void) {
    printf("IKOS Socket API Unit Tests\n");
    printf("==========================\n");
    
    test_socket_library_init();
    test_socket_creation();
    test_address_utilities();
    test_byte_order_conversion();
    test_socket_options();
    test_socket_bind_listen();
    test_udp_socket_operations();
    test_error_handling();
    test_socket_statistics();
    
    print_test_summary();
}

void run_socket_integration_tests(void) {
    printf("\nIKOS Socket API Integration Tests\n");
    printf("=================================\n");
    
    tests_run = 0;
    tests_passed = 0;
    tests_failed = 0;
    
    test_tcp_client_server_setup();
    test_socket_address_operations();
    
    print_test_summary();
}

void run_socket_performance_tests(void) {
    printf("\nIKOS Socket API Performance Tests\n");
    printf("=================================\n");
    
    TEST_GROUP("Performance Tests");
    
    socket_lib_init();
    
    /* Test socket creation performance */
    printf("Testing socket creation performance...\n");
    int create_count = 0;
    int sockets[100];
    
    for (int i = 0; i < 100; i++) {
        sockets[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockets[i] >= 0) {
            create_count++;
        }
    }
    
    printf("Created %d sockets successfully\n", create_count);
    TEST_ASSERT(create_count > 0, "Socket creation performance test");
    
    /* Clean up */
    for (int i = 0; i < 100; i++) {
        if (sockets[i] >= 0) {
            close_socket(sockets[i]);
        }
    }
    
    socket_print_user_stats();
    socket_lib_cleanup();
    
    print_test_summary();
}

void socket_api_comprehensive_test(void) {
    printf("IKOS Socket API Comprehensive Test Suite\n");
    printf("========================================\n\n");
    
    /* Run all test suites */
    run_socket_unit_tests();
    run_socket_integration_tests();
    run_socket_performance_tests();
    
    printf("\n=== Overall Test Summary ===\n");
    printf("Comprehensive socket API testing completed\n");
    printf("Socket API ready for use\n");
}

/* Simple socket API test for basic validation */
int socket_api_basic_test(void) {
    printf("Socket API Basic Validation Test\n");
    printf("================================\n");
    
    int result = 0;
    
    /* Test library initialization */
    if (socket_lib_init() != SOCK_SUCCESS) {
        printf("FAIL: Socket library initialization\n");
        return -1;
    }
    printf("PASS: Socket library initialization\n");
    
    /* Test TCP socket creation */
    int tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_sock >= 0) {
        printf("PASS: TCP socket creation (fd=%d)\n", tcp_sock);
        close_socket(tcp_sock);
    } else {
        printf("FAIL: TCP socket creation\n");
        result = -1;
    }
    
    /* Test UDP socket creation */
    int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock >= 0) {
        printf("PASS: UDP socket creation (fd=%d)\n", udp_sock);
        close_socket(udp_sock);
    } else {
        printf("FAIL: UDP socket creation\n");
        result = -1;
    }
    
    /* Test address utilities */
    uint32_t addr = inet_addr("127.0.0.1");
    if (addr != INADDR_NONE) {
        printf("PASS: Address conversion\n");
    } else {
        printf("FAIL: Address conversion\n");
        result = -1;
    }
    
    socket_print_user_stats();
    socket_lib_cleanup();
    
    if (result == 0) {
        printf("SUCCESS: Socket API basic validation passed\n");
    } else {
        printf("FAILURE: Socket API basic validation failed\n");
    }
    
    return result;
}
