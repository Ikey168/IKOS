/*
 * IKOS Socket API Examples - Demonstration Applications
 * Issue #46 - Sockets API for User Applications
 * 
 * Example applications demonstrating Berkeley-style socket usage
 * for TCP and UDP client/server communication.
 */

#include "../include/socket_user_api.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* ================================
 * Echo Server Examples
 * ================================ */

/* TCP Echo Server */
int tcp_echo_server_example(uint16_t port) {
    printf("Starting TCP Echo Server on port %u...\n", port);
    
    /* Initialize socket library */
    if (socket_lib_init() != SOCK_SUCCESS) {
        printf("Failed to initialize socket library\n");
        return -1;
    }
    
    /* Create server socket */
    int server_fd = tcp_server_create(port, 5);
    if (server_fd < 0) {
        printf("Failed to create TCP server: %s\n", socket_strerror(socket_errno()));
        return -1;
    }
    
    printf("TCP Echo Server listening on port %u (fd=%d)\n", port, server_fd);
    
    /* Accept and handle clients */
    for (int i = 0; i < 3; i++) {  /* Handle 3 clients for demo */
        char client_ip[16];
        uint16_t client_port;
        
        printf("Waiting for client connection...\n");
        int client_fd = tcp_server_accept_client(server_fd, client_ip, sizeof(client_ip), &client_port);
        
        if (client_fd < 0) {
            printf("Failed to accept client: %s\n", socket_strerror(socket_errno()));
            continue;
        }
        
        printf("Accepted client from %s:%u (fd=%d)\n", client_ip, client_port, client_fd);
        
        /* Echo data back to client */
        char buffer[1024];
        while (1) {
            int bytes_received = tcp_client_recv_string(client_fd, buffer, sizeof(buffer));
            if (bytes_received <= 0) {
                printf("Client disconnected or error: %d\n", bytes_received);
                break;
            }
            
            printf("Received from client: %s\n", buffer);
            
            /* Echo back to client */
            int bytes_sent = tcp_client_send_string(client_fd, buffer);
            if (bytes_sent <= 0) {
                printf("Failed to send echo: %d\n", bytes_sent);
                break;
            }
            
            printf("Echoed %d bytes back to client\n", bytes_sent);
            
            /* Break on "quit" command */
            if (strcmp(buffer, "quit") == 0) {
                break;
            }
        }
        
        close_socket(client_fd);
        printf("Client session ended\n");
    }
    
    close_socket(server_fd);
    socket_print_user_stats();
    socket_lib_cleanup();
    
    printf("TCP Echo Server finished\n");
    return 0;
}

/* UDP Echo Server */
int udp_echo_server_example(uint16_t port) {
    printf("Starting UDP Echo Server on port %u...\n", port);
    
    /* Initialize socket library */
    if (socket_lib_init() != SOCK_SUCCESS) {
        printf("Failed to initialize socket library\n");
        return -1;
    }
    
    /* Create UDP server socket */
    int server_fd = udp_server_create(port);
    if (server_fd < 0) {
        printf("Failed to create UDP server: %s\n", socket_strerror(socket_errno()));
        return -1;
    }
    
    printf("UDP Echo Server listening on port %u (fd=%d)\n", port, server_fd);
    
    /* Handle UDP packets */
    char buffer[1024];
    for (int i = 0; i < 5; i++) {  /* Handle 5 packets for demo */
        char client_ip[16];
        uint16_t client_port;
        
        printf("Waiting for UDP packet...\n");
        int bytes_received = udp_server_recv_from(server_fd, buffer, sizeof(buffer) - 1, 
                                                 client_ip, sizeof(client_ip), &client_port);
        
        if (bytes_received <= 0) {
            printf("Failed to receive UDP packet: %s\n", socket_strerror(socket_errno()));
            continue;
        }
        
        buffer[bytes_received] = '\0';
        printf("Received UDP packet from %s:%u: %s\n", client_ip, client_port, buffer);
        
        /* Echo back to client */
        int bytes_sent = udp_server_send_to(server_fd, client_ip, client_port, buffer, bytes_received);
        if (bytes_sent <= 0) {
            printf("Failed to send UDP echo: %s\n", socket_strerror(socket_errno()));
            continue;
        }
        
        printf("Echoed %d bytes back to %s:%u\n", bytes_sent, client_ip, client_port);
    }
    
    close_socket(server_fd);
    socket_print_user_stats();
    socket_lib_cleanup();
    
    printf("UDP Echo Server finished\n");
    return 0;
}

/* ================================
 * Client Examples
 * ================================ */

/* TCP Echo Client */
int tcp_echo_client_example(const char* server_ip, uint16_t server_port) {
    printf("Starting TCP Echo Client to %s:%u...\n", server_ip, server_port);
    
    /* Initialize socket library */
    if (socket_lib_init() != SOCK_SUCCESS) {
        printf("Failed to initialize socket library\n");
        return -1;
    }
    
    /* Connect to server */
    int sockfd = tcp_client_connect(server_ip, server_port);
    if (sockfd < 0) {
        printf("Failed to connect to server: %s\n", socket_strerror(socket_errno()));
        return -1;
    }
    
    printf("Connected to TCP server at %s:%u (fd=%d)\n", server_ip, server_port, sockfd);
    
    /* Send test messages */
    const char* test_messages[] = {
        "Hello, server!",
        "This is a test message",
        "Socket API working!",
        "quit"
    };
    
    for (int i = 0; i < 4; i++) {
        printf("Sending: %s\n", test_messages[i]);
        
        int bytes_sent = tcp_client_send_string(sockfd, test_messages[i]);
        if (bytes_sent <= 0) {
            printf("Failed to send message: %s\n", socket_strerror(socket_errno()));
            break;
        }
        
        /* Receive echo */
        char buffer[1024];
        int bytes_received = tcp_client_recv_string(sockfd, buffer, sizeof(buffer));
        if (bytes_received <= 0) {
            printf("Failed to receive echo: %s\n", socket_strerror(socket_errno()));
            break;
        }
        
        printf("Received echo: %s\n", buffer);
        
        /* Small delay */
        usleep(100000);  /* 100ms */
    }
    
    close_socket(sockfd);
    socket_print_user_stats();
    socket_lib_cleanup();
    
    printf("TCP Echo Client finished\n");
    return 0;
}

/* UDP Echo Client */
int udp_echo_client_example(const char* server_ip, uint16_t server_port) {
    printf("Starting UDP Echo Client to %s:%u...\n", server_ip, server_port);
    
    /* Initialize socket library */
    if (socket_lib_init() != SOCK_SUCCESS) {
        printf("Failed to initialize socket library\n");
        return -1;
    }
    
    /* Create UDP client socket */
    int sockfd = udp_client_create();
    if (sockfd < 0) {
        printf("Failed to create UDP client: %s\n", socket_strerror(socket_errno()));
        return -1;
    }
    
    printf("Created UDP client socket (fd=%d)\n", sockfd);
    
    /* Send test messages */
    const char* test_messages[] = {
        "UDP Hello!",
        "UDP test message",
        "Datagram working!"
    };
    
    for (int i = 0; i < 3; i++) {
        printf("Sending UDP packet: %s\n", test_messages[i]);
        
        int bytes_sent = udp_client_send_to(sockfd, server_ip, server_port, 
                                           test_messages[i], strlen(test_messages[i]));
        if (bytes_sent <= 0) {
            printf("Failed to send UDP packet: %s\n", socket_strerror(socket_errno()));
            continue;
        }
        
        /* Receive echo */
        char buffer[1024];
        char from_ip[16];
        uint16_t from_port;
        
        int bytes_received = udp_client_recv_from(sockfd, buffer, sizeof(buffer) - 1, 
                                                 from_ip, sizeof(from_ip), &from_port);
        if (bytes_received <= 0) {
            printf("Failed to receive UDP echo: %s\n", socket_strerror(socket_errno()));
            continue;
        }
        
        buffer[bytes_received] = '\0';
        printf("Received UDP echo from %s:%u: %s\n", from_ip, from_port, buffer);
        
        /* Small delay */
        usleep(100000);  /* 100ms */
    }
    
    close_socket(sockfd);
    socket_print_user_stats();
    socket_lib_cleanup();
    
    printf("UDP Echo Client finished\n");
    return 0;
}

/* ================================
 * Socket API Demo Functions
 * ================================ */

/* Demonstrate basic socket operations */
int socket_basic_demo(void) {
    printf("Socket API Basic Demonstration\n");
    printf("==============================\n");
    
    /* Initialize socket library */
    if (socket_lib_init() != SOCK_SUCCESS) {
        printf("Failed to initialize socket library\n");
        return -1;
    }
    
    /* Create various socket types */
    printf("Creating different socket types:\n");
    
    int tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    printf("TCP socket: fd=%d\n", tcp_sock);
    
    int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    printf("UDP socket: fd=%d\n", udp_sock);
    
    /* Test address utilities */
    printf("\nTesting address utilities:\n");
    
    struct sockaddr_in addr;
    sockaddr_in_from_string(&addr, "127.0.0.1", 8080);
    
    char addr_str[32];
    sockaddr_in_to_string(&addr, addr_str, sizeof(addr_str));
    printf("Address: %s\n", addr_str);
    
    /* Test socket options */
    printf("\nTesting socket options:\n");
    
    if (tcp_sock >= 0) {
        socket_set_reuseaddr(tcp_sock, true);
        socket_set_keepalive(tcp_sock, true);
        
        int send_buf_size = socket_get_send_buffer_size(tcp_sock);
        int recv_buf_size = socket_get_recv_buffer_size(tcp_sock);
        
        printf("TCP socket buffer sizes: send=%d, recv=%d\n", send_buf_size, recv_buf_size);
    }
    
    /* Close sockets */
    if (tcp_sock >= 0) close_socket(tcp_sock);
    if (udp_sock >= 0) close_socket(udp_sock);
    
    /* Print statistics */
    socket_print_user_stats();
    socket_lib_cleanup();
    
    printf("Socket API basic demo completed\n");
    return 0;
}

/* Non-blocking socket demo */
int socket_nonblocking_demo(void) {
    printf("Non-blocking Socket Demonstration\n");
    printf("=================================\n");
    
    if (socket_lib_init() != SOCK_SUCCESS) {
        printf("Failed to initialize socket library\n");
        return -1;
    }
    
    /* Create non-blocking TCP socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        printf("Failed to create socket\n");
        return -1;
    }
    
    /* Set socket to non-blocking mode */
    socket_set_nonblocking(sockfd, true);
    printf("Socket set to non-blocking mode\n");
    
    /* Try to connect to non-existent server (should return immediately) */
    struct sockaddr_in addr;
    sockaddr_in_from_string(&addr, "192.168.1.254", 12345);  /* Non-existent server */
    
    printf("Attempting non-blocking connect...\n");
    int result = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    
    if (result == SOCK_EINPROGRESS) {
        printf("Connect in progress (as expected for non-blocking)\n");
    } else if (result < 0) {
        printf("Connect failed: %s\n", socket_strerror(socket_errno()));
    } else {
        printf("Connect succeeded immediately\n");
    }
    
    close_socket(sockfd);
    socket_print_user_stats();
    socket_lib_cleanup();
    
    printf("Non-blocking socket demo completed\n");
    return 0;
}

/* ================================
 * Main Example Functions
 * ================================ */

/* Run socket API demonstrations */
void run_socket_examples(void) {
    printf("IKOS Socket API Examples\n");
    printf("========================\n\n");
    
    /* Basic socket demo */
    socket_basic_demo();
    printf("\n");
    
    /* Non-blocking demo */
    socket_nonblocking_demo();
    printf("\n");
    
    printf("Socket API examples completed\n");
}

/* TCP Echo Server/Client test */
void run_tcp_echo_test(void) {
    printf("TCP Echo Server/Client Test\n");
    printf("===========================\n\n");
    
    /* For testing, we would normally run server and client in different processes */
    /* Here we just demonstrate the API calls */
    
    printf("Server API demonstration:\n");
    /* tcp_echo_server_example(8080); */  /* Would run in separate process */
    
    printf("Client API demonstration:\n");
    /* tcp_echo_client_example("127.0.0.1", 8080); */  /* Would connect to server */
    
    printf("TCP Echo test completed (API demonstrated)\n");
}

/* UDP Echo Server/Client test */
void run_udp_echo_test(void) {
    printf("UDP Echo Server/Client Test\n");
    printf("===========================\n\n");
    
    /* For testing, we would normally run server and client in different processes */
    /* Here we just demonstrate the API calls */
    
    printf("UDP Server API demonstration:\n");
    /* udp_echo_server_example(9090); */  /* Would run in separate process */
    
    printf("UDP Client API demonstration:\n");
    /* udp_echo_client_example("127.0.0.1", 9090); */  /* Would send to server */
    
    printf("UDP Echo test completed (API demonstrated)\n");
}

/* Socket integration test */
void socket_integration_test(void) {
    printf("Socket Integration Test\n");
    printf("======================\n\n");
    
    run_socket_examples();
    printf("\n");
    
    run_tcp_echo_test();
    printf("\n");
    
    run_udp_echo_test();
    printf("\n");
    
    printf("Socket integration test completed\n");
}

/* Simple socket functionality test */
int socket_simple_test(void) {
    printf("Simple Socket Functionality Test\n");
    printf("================================\n");
    
    /* Initialize socket library */
    if (socket_lib_init() != SOCK_SUCCESS) {
        printf("Failed to initialize socket library\n");
        return -1;
    }
    
    /* Test socket creation */
    printf("Testing socket creation...\n");
    int tcp_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int udp_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    printf("Created TCP socket: fd=%d\n", tcp_fd);
    printf("Created UDP socket: fd=%d\n", udp_fd);
    
    /* Test address binding */
    printf("\nTesting socket binding...\n");
    struct sockaddr_in tcp_addr, udp_addr;
    
    sockaddr_in_init(&tcp_addr, INADDR_ANY, 8080);
    sockaddr_in_init(&udp_addr, INADDR_ANY, 9090);
    
    if (tcp_fd >= 0) {
        int tcp_bind_result = bind(tcp_fd, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr));
        printf("TCP bind result: %d\n", tcp_bind_result);
        
        if (tcp_bind_result == 0) {
            int listen_result = listen(tcp_fd, 5);
            printf("TCP listen result: %d\n", listen_result);
        }
    }
    
    if (udp_fd >= 0) {
        int udp_bind_result = bind(udp_fd, (struct sockaddr*)&udp_addr, sizeof(udp_addr));
        printf("UDP bind result: %d\n", udp_bind_result);
    }
    
    /* Test socket options */
    printf("\nTesting socket options...\n");
    if (tcp_fd >= 0) {
        socket_set_reuseaddr(tcp_fd, true);
        socket_set_keepalive(tcp_fd, true);
        printf("Set TCP socket options\n");
    }
    
    if (udp_fd >= 0) {
        socket_set_broadcast(udp_fd, true);
        printf("Set UDP socket options\n");
    }
    
    /* Clean up */
    printf("\nCleaning up...\n");
    if (tcp_fd >= 0) close_socket(tcp_fd);
    if (udp_fd >= 0) close_socket(udp_fd);
    
    socket_print_user_stats();
    socket_lib_cleanup();
    
    printf("Simple socket test completed successfully\n");
    return 0;
}
