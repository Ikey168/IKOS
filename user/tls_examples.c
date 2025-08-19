/*
 * IKOS TLS Examples and Demonstrations
 * Issue #48: Secure Network Communication (TLS/SSL)
 *
 * Comprehensive examples demonstrating TLS/SSL functionality
 * in the IKOS operating system.
 */

#include "../include/tls_user_api.h"
#include "../include/socket_user_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ================================
 * TLS Client Examples
 * ================================ */

/**
 * Basic TLS client connection example
 */
int tls_client_basic_example(void) {
    printf("=== TLS Client Basic Example ===\n");
    
    // Initialize TLS library
    int result = tls_user_init();
    if (result != TLS_USER_SUCCESS) {
        printf("Failed to initialize TLS library: %s\n", tls_user_error_string(result));
        return -1;
    }
    
    // Create TLS client connection
    printf("Connecting to secure server...\n");
    int tls_socket = tls_client_connect("example.com", 443, NULL);
    if (tls_socket < 0) {
        printf("Failed to connect to server: %s\n", tls_user_error_string(tls_socket));
        tls_user_cleanup();
        return -1;
    }
    
    printf("TLS connection established (socket: %d)\n", tls_socket);
    
    // Get connection information
    tls_user_connection_info_t conn_info;
    result = tls_get_connection_info(tls_socket, &conn_info);
    if (result == TLS_USER_SUCCESS) {
        printf("Connected to: %s\n", conn_info.hostname);
        printf("Protocol: %s\n", conn_info.protocol_version);
        printf("Cipher Suite: %s\n", conn_info.cipher_suite_name);
        printf("Verified: %s\n", conn_info.is_verified ? "Yes" : "No");
        printf("Encrypted: %s\n", conn_info.is_encrypted ? "Yes" : "No");
    }
    
    // Send HTTP request
    const char* http_request = 
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: IKOS-TLS-Client/1.0\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    printf("Sending HTTP request...\n");
    result = tls_send_all(tls_socket, http_request, strlen(http_request));
    if (result != TLS_USER_SUCCESS) {
        printf("Failed to send request: %s\n", tls_user_error_string(result));
    } else {
        printf("HTTP request sent successfully\n");
        
        // Receive response
        char response[4096];
        int received = tls_recv(tls_socket, response, sizeof(response) - 1);
        if (received > 0) {
            response[received] = '\0';
            printf("Received %d bytes:\n", received);
            printf("--- Response ---\n");
            printf("%s\n", response);
            printf("--- End Response ---\n");
        } else {
            printf("Failed to receive response: %s\n", tls_user_error_string(received));
        }
    }
    
    // Close connection
    tls_close(tls_socket);
    printf("TLS connection closed\n");
    
    // Cleanup
    tls_user_cleanup();
    
    printf("TLS client basic example completed\n\n");
    return 0;
}

/**
 * TLS client with custom configuration
 */
int tls_client_custom_config_example(void) {
    printf("=== TLS Client Custom Configuration Example ===\n");
    
    // Initialize TLS library
    int result = tls_user_init();
    if (result != TLS_USER_SUCCESS) {
        printf("Failed to initialize TLS library: %s\n", tls_user_error_string(result));
        return -1;
    }
    
    // Create custom TLS configuration
    tls_user_config_t config;
    result = tls_user_config_init(&config);
    if (result != TLS_USER_SUCCESS) {
        printf("Failed to initialize configuration: %s\n", tls_user_error_string(result));
        tls_user_cleanup();
        return -1;
    }
    
    // Set custom configuration
    tls_user_config_set_version(&config, TLS_USER_VERSION_1_2, TLS_USER_VERSION_1_2);
    tls_user_config_set_verification(&config, true, true);
    tls_user_config_set_timeouts(&config, 15000, 10000); // 15s handshake, 10s I/O
    
    printf("Custom configuration:\n");
    printf("  TLS Version: 1.2\n");
    printf("  Verify Peer: Yes\n");
    printf("  Verify Hostname: Yes\n");
    printf("  Handshake Timeout: 15 seconds\n");
    printf("  I/O Timeout: 10 seconds\n");
    
    // Connect with custom configuration
    printf("Connecting to secure server with custom config...\n");
    int tls_socket = tls_client_connect("github.com", 443, &config);
    if (tls_socket < 0) {
        printf("Failed to connect to server: %s\n", tls_user_error_string(tls_socket));
        tls_user_cleanup();
        return -1;
    }
    
    printf("TLS connection established with custom configuration\n");
    
    // Verify connection is secure
    int verified = tls_is_verified(tls_socket);
    if (verified == 1) {
        printf("Connection is verified and secure\n");
        
        // Get peer certificate information
        tls_user_certificate_info_t cert_info;
        result = tls_get_peer_certificate_info(tls_socket, &cert_info);
        if (result == TLS_USER_SUCCESS) {
            printf("Peer Certificate Information:\n");
            printf("  Subject: %s\n", cert_info.subject);
            printf("  Issuer: %s\n", cert_info.issuer);
            printf("  Serial: %s\n", cert_info.serial_number);
            printf("  Valid From: %s\n", cert_info.valid_from);
            printf("  Valid To: %s\n", cert_info.valid_to);
            printf("  Key Size: %u bits\n", cert_info.key_size);
            printf("  Is Valid: %s\n", cert_info.is_valid ? "Yes" : "No");
            printf("  Is Expired: %s\n", cert_info.is_expired ? "Yes" : "No");
            printf("  Is Self-Signed: %s\n", cert_info.is_self_signed ? "Yes" : "No");
        }
    } else if (verified == 0) {
        printf("Warning: Connection is not verified\n");
    } else {
        printf("Error checking verification status: %s\n", tls_user_error_string(verified));
    }
    
    // Close connection
    tls_close(tls_socket);
    printf("TLS connection closed\n");
    
    // Cleanup
    tls_user_cleanup();
    
    printf("TLS client custom configuration example completed\n\n");
    return 0;
}

/**
 * TLS client using existing TCP socket
 */
int tls_client_existing_socket_example(void) {
    printf("=== TLS Client Existing Socket Example ===\n");
    
    // Initialize libraries
    socket_user_init();
    tls_user_init();
    
    // Create TCP socket manually
    printf("Creating TCP socket...\n");
    int tcp_socket = tcp_client_connect("httpbin.org", 443);
    if (tcp_socket < 0) {
        printf("Failed to create TCP connection: %s\n", socket_user_error_string(tcp_socket));
        socket_user_cleanup();
        tls_user_cleanup();
        return -1;
    }
    
    printf("TCP connection established (socket: %d)\n", tcp_socket);
    
    // Upgrade to TLS
    printf("Upgrading to TLS...\n");
    int tls_socket = tls_client_connect_socket(tcp_socket, "httpbin.org", NULL);
    if (tls_socket < 0) {
        printf("Failed to upgrade to TLS: %s\n", tls_user_error_string(tls_socket));
        close_socket(tcp_socket);
        socket_user_cleanup();
        tls_user_cleanup();
        return -1;
    }
    
    printf("Socket upgraded to TLS successfully\n");
    
    // Test secure communication
    const char* http_request = 
        "GET /get HTTP/1.1\r\n"
        "Host: httpbin.org\r\n"
        "User-Agent: IKOS-TLS-Test/1.0\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    printf("Sending secure HTTP request...\n");
    int result = tls_send_all(tls_socket, http_request, strlen(http_request));
    if (result == TLS_USER_SUCCESS) {
        printf("Request sent successfully\n");
        
        // Receive response headers
        char response[2048];
        int received = tls_recv(tls_socket, response, sizeof(response) - 1);
        if (received > 0) {
            response[received] = '\0';
            printf("Received secure response (%d bytes)\n", received);
            
            // Show first few lines
            char* line = strtok(response, "\r\n");
            int line_count = 0;
            while (line && line_count < 5) {
                printf("  %s\n", line);
                line = strtok(NULL, "\r\n");
                line_count++;
            }
            if (line) {
                printf("  ... (response truncated)\n");
            }
        }
    }
    
    // Close TLS connection
    tls_close(tls_socket);
    printf("TLS connection closed\n");
    
    // Cleanup
    socket_user_cleanup();
    tls_user_cleanup();
    
    printf("TLS client existing socket example completed\n\n");
    return 0;
}

/* ================================
 * TLS Server Examples
 * ================================ */

/**
 * Basic TLS server example
 */
int tls_server_basic_example(void) {
    printf("=== TLS Server Basic Example ===\n");
    
    // Initialize TLS library
    int result = tls_user_init();
    if (result != TLS_USER_SUCCESS) {
        printf("Failed to initialize TLS library: %s\n", tls_user_error_string(result));
        return -1;
    }
    
    // Create server configuration with certificate
    tls_user_config_t config;
    result = tls_user_config_init(&config);
    if (result != TLS_USER_SUCCESS) {
        printf("Failed to initialize configuration: %s\n", tls_user_error_string(result));
        tls_user_cleanup();
        return -1;
    }
    
    // Set certificate files (would be real files in production)
    result = tls_user_config_set_certificate(&config, "/etc/ssl/server.crt", "/etc/ssl/server.key");
    if (result != TLS_USER_SUCCESS) {
        printf("Note: Using placeholder certificate paths\n");
    }
    
    printf("Creating TLS server on port 8443...\n");
    int server_socket = tls_server_create(8443, &config);
    if (server_socket < 0) {
        printf("Failed to create TLS server: %s\n", tls_user_error_string(server_socket));
        tls_user_cleanup();
        return -1;
    }
    
    printf("TLS server created successfully (socket: %d)\n", server_socket);
    printf("Server listening on port 8443\n");
    printf("Note: In a real implementation, this would accept client connections\n");
    
    // Simulate accepting a client (simplified)
    printf("Simulating client connection...\n");
    
    // In a real server, you would:
    // 1. Accept client connections in a loop
    // 2. Handle each client in a separate thread
    // 3. Process TLS handshake
    // 4. Handle application data
    
    printf("Server example simulation:\n");
    printf("  - Client connects\n");
    printf("  - TLS handshake performed\n");
    printf("  - Secure communication established\n");
    printf("  - Client disconnects\n");
    
    // Close server
    tls_close(server_socket);
    printf("TLS server closed\n");
    
    // Cleanup
    tls_user_cleanup();
    
    printf("TLS server basic example completed\n\n");
    return 0;
}

/* ================================
 * TLS Performance Examples
 * ================================ */

/**
 * TLS performance and throughput test
 */
int tls_performance_example(void) {
    printf("=== TLS Performance Example ===\n");
    
    // Initialize TLS library
    int result = tls_user_init();
    if (result != TLS_USER_SUCCESS) {
        printf("Failed to initialize TLS library: %s\n", tls_user_error_string(result));
        return -1;
    }
    
    printf("Testing TLS performance characteristics...\n");
    
    // Test multiple connections
    const int num_connections = 5;
    printf("Creating %d TLS connections...\n", num_connections);
    
    int successful_connections = 0;
    int failed_connections = 0;
    
    for (int i = 0; i < num_connections; i++) {
        printf("Connection %d/%d: ", i + 1, num_connections);
        
        int tls_socket = tls_client_connect("example.com", 443, NULL);
        if (tls_socket >= 0) {
            printf("SUCCESS (socket: %d)\n", tls_socket);
            successful_connections++;
            
            // Test small data transfer
            const char* test_data = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
            int sent = tls_send(tls_socket, test_data, strlen(test_data));
            if (sent > 0) {
                printf("  Sent %d bytes\n", sent);
            }
            
            // Close connection
            tls_close(tls_socket);
        } else {
            printf("FAILED (%s)\n", tls_user_error_string(tls_socket));
            failed_connections++;
        }
    }
    
    printf("Connection Summary:\n");
    printf("  Successful: %d/%d\n", successful_connections, num_connections);
    printf("  Failed: %d/%d\n", failed_connections, num_connections);
    printf("  Success Rate: %.1f%%\n", 
           100.0 * successful_connections / num_connections);
    
    // Get TLS statistics
    tls_user_statistics_t stats;
    result = tls_user_get_statistics(&stats);
    if (result == TLS_USER_SUCCESS) {
        printf("TLS Statistics:\n");
        printf("  Total Connections: %lu\n", stats.total_connections);
        printf("  Successful Handshakes: %lu\n", stats.successful_handshakes);
        printf("  Failed Handshakes: %lu\n", stats.failed_handshakes);
        printf("  Bytes Encrypted: %lu\n", stats.bytes_encrypted);
        printf("  Bytes Decrypted: %lu\n", stats.bytes_decrypted);
        printf("  Active Connections: %u\n", stats.active_connections);
        printf("  Average Handshake Time: %.2f ms\n", stats.average_handshake_time);
        printf("  Average Throughput: %.2f KB/s\n", stats.average_throughput / 1024);
    }
    
    // Reset statistics
    tls_user_reset_statistics();
    printf("Statistics reset\n");
    
    // Cleanup
    tls_user_cleanup();
    
    printf("TLS performance example completed\n\n");
    return 0;
}

/* ================================
 * TLS Security Examples
 * ================================ */

/**
 * TLS certificate verification example
 */
int tls_certificate_verification_example(void) {
    printf("=== TLS Certificate Verification Example ===\n");
    
    // Initialize TLS library
    int result = tls_user_init();
    if (result != TLS_USER_SUCCESS) {
        printf("Failed to initialize TLS library: %s\n", tls_user_error_string(result));
        return -1;
    }
    
    // Test certificate verification with different settings
    const char* test_hosts[] = {
        "github.com",
        "google.com",
        "stackoverflow.com"
    };
    int num_hosts = sizeof(test_hosts) / sizeof(test_hosts[0]);
    
    for (int i = 0; i < num_hosts; i++) {
        printf("Testing certificate verification for %s:\n", test_hosts[i]);
        
        // Configuration with strict verification
        tls_user_config_t strict_config;
        tls_user_config_init(&strict_config);
        tls_user_config_set_verification(&strict_config, true, true);
        
        printf("  Strict verification: ");
        int tls_socket = tls_client_connect(test_hosts[i], 443, &strict_config);
        if (tls_socket >= 0) {
            int verified = tls_is_verified(tls_socket);
            printf("%s\n", verified == 1 ? "VERIFIED" : "NOT VERIFIED");
            
            if (verified == 1) {
                tls_user_certificate_info_t cert_info;
                result = tls_get_peer_certificate_info(tls_socket, &cert_info);
                if (result == TLS_USER_SUCCESS) {
                    printf("    Subject: %s\n", cert_info.subject);
                    printf("    Valid: %s\n", cert_info.is_valid ? "Yes" : "No");
                    printf("    Expired: %s\n", cert_info.is_expired ? "Yes" : "No");
                }
            }
            
            tls_close(tls_socket);
        } else {
            printf("FAILED (%s)\n", tls_user_error_string(tls_socket));
        }
        
        printf("\n");
    }
    
    // Cleanup
    tls_user_cleanup();
    
    printf("TLS certificate verification example completed\n\n");
    return 0;
}

/* ================================
 * TLS Error Handling Examples
 * ================================ */

/**
 * TLS error handling and recovery example
 */
int tls_error_handling_example(void) {
    printf("=== TLS Error Handling Example ===\n");
    
    // Initialize TLS library
    int result = tls_user_init();
    if (result != TLS_USER_SUCCESS) {
        printf("Failed to initialize TLS library: %s\n", tls_user_error_string(result));
        return -1;
    }
    
    printf("Testing various error conditions...\n");
    
    // Test 1: Invalid hostname
    printf("Test 1 - Invalid hostname: ");
    int tls_socket = tls_client_connect("", 443, NULL);
    if (tls_socket < 0) {
        printf("EXPECTED ERROR: %s\n", tls_user_error_string(tls_socket));
    } else {
        printf("UNEXPECTED SUCCESS\n");
        tls_close(tls_socket);
    }
    
    // Test 2: Invalid port
    printf("Test 2 - Invalid port: ");
    tls_socket = tls_client_connect("example.com", 0, NULL);
    if (tls_socket < 0) {
        printf("EXPECTED ERROR: %s\n", tls_user_error_string(tls_socket));
    } else {
        printf("UNEXPECTED SUCCESS\n");
        tls_close(tls_socket);
    }
    
    // Test 3: Non-existent host
    printf("Test 3 - Non-existent host: ");
    tls_socket = tls_client_connect("nonexistent.invalid.domain", 443, NULL);
    if (tls_socket < 0) {
        printf("EXPECTED ERROR: %s\n", tls_user_error_string(tls_socket));
    } else {
        printf("UNEXPECTED SUCCESS\n");
        tls_close(tls_socket);
    }
    
    // Test 4: Invalid TLS socket operations
    printf("Test 4 - Invalid socket operations:\n");
    
    printf("  Send on invalid socket: ");
    result = tls_send(-1, "test", 4);
    if (result < 0) {
        printf("EXPECTED ERROR: %s\n", tls_user_error_string(result));
    } else {
        printf("UNEXPECTED SUCCESS\n");
    }
    
    printf("  Receive on invalid socket: ");
    char buffer[100];
    result = tls_recv(-1, buffer, sizeof(buffer));
    if (result < 0) {
        printf("EXPECTED ERROR: %s\n", tls_user_error_string(result));
    } else {
        printf("UNEXPECTED SUCCESS\n");
    }
    
    printf("  Close invalid socket: ");
    result = tls_close(-1);
    if (result != TLS_USER_SUCCESS) {
        printf("EXPECTED ERROR: %s\n", tls_user_error_string(result));
    } else {
        printf("HANDLED GRACEFULLY\n");
    }
    
    // Test 5: Configuration validation
    printf("Test 5 - Configuration validation:\n");
    
    tls_user_config_t config;
    tls_user_config_init(&config);
    
    printf("  Invalid version range: ");
    result = tls_user_config_set_version(&config, TLS_USER_VERSION_1_2, TLS_USER_VERSION_1_0);
    if (result != TLS_USER_SUCCESS) {
        printf("EXPECTED ERROR: %s\n", tls_user_error_string(result));
    } else {
        printf("UNEXPECTED SUCCESS\n");
    }
    
    printf("  Invalid timeout: ");
    result = tls_user_config_set_timeouts(&config, 0, 5000);
    if (result != TLS_USER_SUCCESS) {
        printf("EXPECTED ERROR: %s\n", tls_user_error_string(result));
    } else {
        printf("UNEXPECTED SUCCESS\n");
    }
    
    // Cleanup
    tls_user_cleanup();
    
    printf("TLS error handling example completed\n\n");
    return 0;
}

/* ================================
 * Main Example Functions
 * ================================ */

/**
 * Run all TLS examples
 */
void tls_run_all_examples(void) {
    printf("IKOS TLS/SSL Examples\n");
    printf("=====================\n\n");
    
    tls_client_basic_example();
    tls_client_custom_config_example();
    tls_client_existing_socket_example();
    tls_server_basic_example();
    tls_performance_example();
    tls_certificate_verification_example();
    tls_error_handling_example();
    
    printf("All TLS examples completed successfully!\n");
}

/**
 * Simple TLS functionality test
 */
int tls_simple_test(void) {
    printf("TLS Simple Functionality Test\n");
    printf("==============================\n");
    
    // Test library initialization
    printf("Testing TLS library initialization...\n");
    int result = tls_user_init();
    if (result == TLS_USER_SUCCESS) {
        printf("PASS: TLS library initialization succeeded\n");
        
        // Test configuration
        printf("Testing TLS configuration...\n");
        tls_user_config_t config;
        result = tls_user_config_init(&config);
        if (result == TLS_USER_SUCCESS) {
            printf("PASS: TLS configuration initialization succeeded\n");
            
            // Test hostname validation
            printf("Testing hostname validation...\n");
            if (tls_user_is_valid_hostname("example.com")) {
                printf("PASS: Valid hostname accepted\n");
            } else {
                printf("FAIL: Valid hostname rejected\n");
                return -1;
            }
            
            if (!tls_user_is_valid_hostname("invalid..hostname")) {
                printf("PASS: Invalid hostname rejected\n");
            } else {
                printf("FAIL: Invalid hostname accepted\n");
                return -1;
            }
            
            // Test error string conversion
            printf("Testing error string conversion...\n");
            const char* error_str = tls_user_error_string(TLS_USER_INVALID_PARAMETER);
            if (error_str && strlen(error_str) > 0) {
                printf("PASS: Error string conversion works (%s)\n", error_str);
            } else {
                printf("FAIL: Error string conversion failed\n");
                return -1;
            }
            
        } else {
            printf("FAIL: TLS configuration initialization failed\n");
            return -1;
        }
        
        // Cleanup
        tls_user_cleanup();
        printf("PASS: TLS library cleanup succeeded\n");
        
    } else {
        printf("FAIL: TLS library initialization failed: %s\n", tls_user_error_string(result));
        return -1;
    }
    
    printf("SUCCESS: TLS simple functionality test passed\n");
    return 0;
}

/**
 * Main function for TLS examples
 */
int main(int argc, char* argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "simple") == 0) {
            return tls_simple_test();
        } else if (strcmp(argv[1], "client") == 0) {
            return tls_client_basic_example();
        } else if (strcmp(argv[1], "server") == 0) {
            return tls_server_basic_example();
        } else if (strcmp(argv[1], "performance") == 0) {
            return tls_performance_example();
        } else if (strcmp(argv[1], "security") == 0) {
            return tls_certificate_verification_example();
        } else if (strcmp(argv[1], "errors") == 0) {
            return tls_error_handling_example();
        } else if (strcmp(argv[1], "all") == 0) {
            tls_run_all_examples();
            return 0;
        } else {
            printf("Unknown example type: %s\n", argv[1]);
            printf("Available examples: simple, client, server, performance, security, errors, all\n");
            return -1;
        }
    } else {
        // Run simple test by default
        return tls_simple_test();
    }
}
