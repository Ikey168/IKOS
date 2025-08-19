/*
 * IKOS TLS Syscalls Implementation
 * Issue #48: Secure Network Communication (TLS/SSL)
 *
 * Kernel syscall implementations for TLS/SSL operations.
 * Provides bridge between user-space TLS API and kernel TLS implementation.
 */

#include "../include/tls_syscalls.h"
#include "../include/net/tls.h"
#include "../include/memory.h"
#include "../include/process.h"
#include "../include/assert.h"
#include <string.h>
#include <stdio.h>

/* ================================
 * TLS Syscall State Management
 * ================================ */

/* TLS socket descriptor table */
#define MAX_TLS_SOCKETS 256
static tls_connection_t* tls_socket_table[MAX_TLS_SOCKETS];
static bool tls_socket_used[MAX_TLS_SOCKETS];
static int next_tls_socket = 0;

/* TLS syscall statistics */
static tls_syscall_statistics_t tls_syscall_stats;

/* ================================
 * TLS Socket Management
 * ================================ */

/**
 * Allocate a TLS socket descriptor
 */
static int tls_allocate_socket(tls_connection_t* conn) {
    for (int i = 0; i < MAX_TLS_SOCKETS; i++) {
        int socket_id = (next_tls_socket + i) % MAX_TLS_SOCKETS;
        if (!tls_socket_used[socket_id]) {
            tls_socket_table[socket_id] = conn;
            tls_socket_used[socket_id] = true;
            next_tls_socket = (socket_id + 1) % MAX_TLS_SOCKETS;
            return socket_id;
        }
    }
    return -1; // No available sockets
}

/**
 * Get TLS connection from socket descriptor
 */
static tls_connection_t* tls_get_connection(int tls_socket) {
    if (tls_socket < 0 || tls_socket >= MAX_TLS_SOCKETS) {
        return NULL;
    }
    
    if (!tls_socket_used[tls_socket]) {
        return NULL;
    }
    
    return tls_socket_table[tls_socket];
}

/**
 * Free TLS socket descriptor
 */
static void tls_free_socket(int tls_socket) {
    if (tls_socket >= 0 && tls_socket < MAX_TLS_SOCKETS) {
        tls_socket_table[tls_socket] = NULL;
        tls_socket_used[tls_socket] = false;
    }
}

bool tls_syscall_is_valid_socket(int tls_socket) {
    return tls_get_connection(tls_socket) != NULL;
}

/* ================================
 * TLS Syscall Implementations
 * ================================ */

int sys_tls_init(void) {
    // Initialize TLS socket table
    memset(tls_socket_table, 0, sizeof(tls_socket_table));
    memset(tls_socket_used, 0, sizeof(tls_socket_used));
    next_tls_socket = 0;
    
    // Initialize syscall statistics
    memset(&tls_syscall_stats, 0, sizeof(tls_syscall_stats));
    
    // Initialize TLS library
    int result = tls_init();
    if (result == TLS_SUCCESS) {
        printf("TLS syscall interface initialized\n");
        return TLS_SYSCALL_SUCCESS;
    }
    
    return TLS_SYSCALL_ERROR;
}

int sys_tls_cleanup(void) {
    // Close all active TLS connections
    for (int i = 0; i < MAX_TLS_SOCKETS; i++) {
        if (tls_socket_used[i] && tls_socket_table[i]) {
            tls_connection_free(tls_socket_table[i]);
            tls_socket_table[i] = NULL;
            tls_socket_used[i] = false;
        }
    }
    
    // Cleanup TLS library
    tls_cleanup();
    
    printf("TLS syscall interface cleaned up\n");
    return TLS_SYSCALL_SUCCESS;
}

int sys_tls_client_connect(const tls_syscall_client_connect_params_t* params) {
    if (!params || !params->hostname) {
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    // Validate hostname
    if (strlen(params->hostname) == 0 || strlen(params->hostname) > 253) {
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    // Validate port
    if (params->port == 0) {
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    int tcp_socket = params->tcp_socket_fd;
    
    // Create TCP socket if not provided
    if (tcp_socket < 0) {
        tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_socket < 0) {
            return TLS_SYSCALL_SOCKET_ERROR;
        }
        
        // Connect to server
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(params->port);
        
        // Resolve hostname (simplified - would use DNS in production)
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Placeholder
        
        if (connect(tcp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(tcp_socket);
            return TLS_SYSCALL_SOCKET_ERROR;
        }
    }
    
    // Create TLS connection
    tls_connection_t* conn = tls_connection_new(tcp_socket, false);
    if (!conn) {
        if (params->tcp_socket_fd < 0) {
            close(tcp_socket);
        }
        return TLS_SYSCALL_OUT_OF_MEMORY;
    }
    
    // Configure connection if config provided
    if (params->config) {
        // Apply configuration (simplified)
        conn->version = params->config->max_version;
        strncpy(conn->hostname, params->hostname, sizeof(conn->hostname) - 1);
    }
    
    // Allocate TLS socket descriptor
    int tls_socket = tls_allocate_socket(conn);
    if (tls_socket < 0) {
        tls_connection_free(conn);
        if (params->tcp_socket_fd < 0) {
            close(tcp_socket);
        }
        return TLS_SYSCALL_OUT_OF_MEMORY;
    }
    
    // Perform TLS handshake
    int result = tls_handshake_client(conn);
    if (result != TLS_SUCCESS) {
        tls_free_socket(tls_socket);
        tls_connection_free(conn);
        if (params->tcp_socket_fd < 0) {
            close(tcp_socket);
        }
        
        tls_syscall_stats.failed_handshakes++;
        return TLS_SYSCALL_HANDSHAKE_FAILED;
    }
    
    tls_syscall_stats.total_connections++;
    tls_syscall_stats.successful_handshakes++;
    
    return tls_socket;
}

int sys_tls_server_create(const tls_syscall_server_create_params_t* params) {
    if (!params || !params->config) {
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    // Validate port
    if (params->port == 0) {
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    // Server must have certificate
    if (strlen(params->config->certificate_path) == 0) {
        return TLS_SYSCALL_CERTIFICATE_ERROR;
    }
    
    int tcp_socket = params->tcp_socket_fd;
    
    // Create TCP socket if not provided
    if (tcp_socket < 0) {
        tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_socket < 0) {
            return TLS_SYSCALL_SOCKET_ERROR;
        }
        
        // Enable address reuse
        int opt = 1;
        setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // Bind to port
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(params->port);
        
        if (bind(tcp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(tcp_socket);
            return TLS_SYSCALL_SOCKET_ERROR;
        }
        
        // Listen for connections
        if (listen(tcp_socket, 10) < 0) {
            close(tcp_socket);
            return TLS_SYSCALL_SOCKET_ERROR;
        }
    }
    
    // Create TLS server connection wrapper
    tls_connection_t* server_conn = tls_connection_new(tcp_socket, true);
    if (!server_conn) {
        if (params->tcp_socket_fd < 0) {
            close(tcp_socket);
        }
        return TLS_SYSCALL_OUT_OF_MEMORY;
    }
    
    // Configure server
    server_conn->version = params->config->max_version;
    
    // Allocate TLS socket descriptor
    int tls_socket = tls_allocate_socket(server_conn);
    if (tls_socket < 0) {
        tls_connection_free(server_conn);
        if (params->tcp_socket_fd < 0) {
            close(tcp_socket);
        }
        return TLS_SYSCALL_OUT_OF_MEMORY;
    }
    
    return tls_socket;
}

int sys_tls_server_accept(int server_socket, void* client_addr, size_t* addr_len) {
    tls_connection_t* server_conn = tls_get_connection(server_socket);
    if (!server_conn || !server_conn->is_server) {
        return TLS_SYSCALL_INVALID_SOCKET;
    }
    
    // Accept TCP connection
    int client_tcp_socket = accept(server_conn->socket_fd, 
                                  (struct sockaddr*)client_addr, 
                                  (socklen_t*)addr_len);
    if (client_tcp_socket < 0) {
        return TLS_SYSCALL_SOCKET_ERROR;
    }
    
    // Create TLS connection for client
    tls_connection_t* client_conn = tls_connection_new(client_tcp_socket, true);
    if (!client_conn) {
        close(client_tcp_socket);
        return TLS_SYSCALL_OUT_OF_MEMORY;
    }
    
    // Copy server configuration to client connection
    client_conn->version = server_conn->version;
    
    // Allocate TLS socket descriptor for client
    int client_tls_socket = tls_allocate_socket(client_conn);
    if (client_tls_socket < 0) {
        tls_connection_free(client_conn);
        close(client_tcp_socket);
        return TLS_SYSCALL_OUT_OF_MEMORY;
    }
    
    // Perform server-side TLS handshake
    int result = tls_handshake_server(client_conn);
    if (result != TLS_SUCCESS) {
        tls_free_socket(client_tls_socket);
        tls_connection_free(client_conn);
        close(client_tcp_socket);
        
        tls_syscall_stats.failed_handshakes++;
        return TLS_SYSCALL_HANDSHAKE_FAILED;
    }
    
    tls_syscall_stats.total_connections++;
    tls_syscall_stats.successful_handshakes++;
    
    return client_tls_socket;
}

int sys_tls_send(const tls_syscall_io_params_t* params) {
    if (!params || !params->buffer) {
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    tls_connection_t* conn = tls_get_connection(params->tls_socket);
    if (!conn) {
        return TLS_SYSCALL_INVALID_SOCKET;
    }
    
    int result = tls_write(conn, params->buffer, params->length);
    if (result > 0) {
        tls_syscall_stats.bytes_encrypted += result;
    }
    
    return result;
}

int sys_tls_recv(const tls_syscall_io_params_t* params) {
    if (!params || !params->buffer) {
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    tls_connection_t* conn = tls_get_connection(params->tls_socket);
    if (!conn) {
        return TLS_SYSCALL_INVALID_SOCKET;
    }
    
    int result = tls_read(conn, params->buffer, params->length);
    if (result > 0) {
        tls_syscall_stats.bytes_decrypted += result;
    }
    
    return result;
}

int sys_tls_close(int tls_socket) {
    tls_connection_t* conn = tls_get_connection(tls_socket);
    if (!conn) {
        return TLS_SYSCALL_INVALID_SOCKET;
    }
    
    // Close underlying TCP socket
    if (conn->socket_fd >= 0) {
        close(conn->socket_fd);
    }
    
    // Free TLS connection
    tls_connection_free(conn);
    
    // Free socket descriptor
    tls_free_socket(tls_socket);
    
    return TLS_SYSCALL_SUCCESS;
}

int sys_tls_shutdown(int tls_socket, int how) {
    tls_connection_t* conn = tls_get_connection(tls_socket);
    if (!conn) {
        return TLS_SYSCALL_INVALID_SOCKET;
    }
    
    // Send close notify alert
    tls_alert_t alert;
    alert.level = TLS_ALERT_WARNING;
    alert.description = TLS_ALERT_CLOSE_NOTIFY;
    
    tls_record_send(conn, TLS_CONTENT_ALERT, &alert, sizeof(alert));
    
    // Shutdown underlying TCP socket
    shutdown(conn->socket_fd, how);
    
    conn->state = TLS_STATE_CLOSED;
    
    return TLS_SYSCALL_SUCCESS;
}

int sys_tls_handshake(int tls_socket) {
    tls_connection_t* conn = tls_get_connection(tls_socket);
    if (!conn) {
        return TLS_SYSCALL_INVALID_SOCKET;
    }
    
    int result = tls_handshake(conn);
    if (result == TLS_SUCCESS) {
        return TLS_SYSCALL_SUCCESS;
    }
    
    return TLS_SYSCALL_HANDSHAKE_FAILED;
}

int sys_tls_get_connection_info(int tls_socket, tls_syscall_connection_info_t* info) {
    if (!info) {
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    tls_connection_t* conn = tls_get_connection(tls_socket);
    if (!conn) {
        return TLS_SYSCALL_INVALID_SOCKET;
    }
    
    // Fill connection information
    strncpy(info->hostname, conn->hostname, sizeof(info->hostname) - 1);
    strncpy(info->cipher_suite_name, 
           tls_cipher_suite_name(conn->security_params.cipher_suite),
           sizeof(info->cipher_suite_name) - 1);
    strncpy(info->protocol_version, 
           tls_version_string(conn->version),
           sizeof(info->protocol_version) - 1);
    
    info->is_verified = (conn->state == TLS_STATE_ESTABLISHED);
    info->is_encrypted = (conn->state == TLS_STATE_ESTABLISHED);
    info->bytes_sent = conn->write_sequence_number * 1024; // Approximate
    info->bytes_received = conn->read_sequence_number * 1024; // Approximate
    info->connection_time = (uint32_t)(tls_get_time_ms() / 1000);
    info->socket_fd = conn->socket_fd;
    info->connection_id = conn->connection_id;
    
    return TLS_SYSCALL_SUCCESS;
}

int sys_tls_get_peer_cert_info(int tls_socket, tls_syscall_certificate_info_t* cert_info) {
    if (!cert_info) {
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    tls_connection_t* conn = tls_get_connection(tls_socket);
    if (!conn) {
        return TLS_SYSCALL_INVALID_SOCKET;
    }
    
    if (!conn->certificate_chain) {
        return TLS_SYSCALL_CERTIFICATE_ERROR;
    }
    
    tls_certificate_t* cert = conn->certificate_chain;
    
    // Fill certificate information
    strncpy(cert_info->subject, cert->subject, sizeof(cert_info->subject) - 1);
    strncpy(cert_info->issuer, cert->issuer, sizeof(cert_info->issuer) - 1);
    strncpy(cert_info->serial_number, cert->serial_number, sizeof(cert_info->serial_number) - 1);
    
    cert_info->valid_from = cert->not_before;
    cert_info->valid_to = cert->not_after;
    cert_info->key_size = cert->public_key_length * 8; // Convert to bits
    cert_info->is_valid = tls_certificate_is_valid_time(cert);
    cert_info->is_expired = (cert->not_after < tls_get_time_ms() / 1000);
    cert_info->is_self_signed = (strcmp(cert->subject, cert->issuer) == 0);
    cert_info->is_ca = false; // Simplified
    
    strncpy(cert_info->signature_algorithm, "RSA-SHA256", sizeof(cert_info->signature_algorithm) - 1);
    strncpy(cert_info->public_key_algorithm, "RSA", sizeof(cert_info->public_key_algorithm) - 1);
    
    return TLS_SYSCALL_SUCCESS;
}

int sys_tls_get_statistics(tls_syscall_statistics_t* stats) {
    if (!stats) {
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    *stats = tls_syscall_stats;
    
    // Get additional statistics from TLS library
    tls_statistics_t tls_lib_stats;
    if (tls_get_statistics(&tls_lib_stats) == TLS_SUCCESS) {
        stats->total_connections = tls_lib_stats.connections_created;
        stats->active_connections = tls_lib_stats.active_connections;
        stats->bytes_encrypted = tls_lib_stats.bytes_encrypted;
        stats->bytes_decrypted = tls_lib_stats.bytes_decrypted;
    }
    
    return TLS_SYSCALL_SUCCESS;
}

int sys_tls_reset_statistics(void) {
    memset(&tls_syscall_stats, 0, sizeof(tls_syscall_stats));
    tls_reset_statistics();
    return TLS_SYSCALL_SUCCESS;
}

/* ================================
 * TLS Syscall Utility Functions
 * ================================ */

const char* tls_syscall_error_string(int error_code) {
    switch (error_code) {
        case TLS_SYSCALL_SUCCESS: return "Success";
        case TLS_SYSCALL_ERROR: return "Generic error";
        case TLS_SYSCALL_INVALID_PARAMETER: return "Invalid parameter";
        case TLS_SYSCALL_OUT_OF_MEMORY: return "Out of memory";
        case TLS_SYSCALL_SOCKET_ERROR: return "Socket error";
        case TLS_SYSCALL_HANDSHAKE_FAILED: return "Handshake failed";
        case TLS_SYSCALL_CERTIFICATE_ERROR: return "Certificate error";
        case TLS_SYSCALL_TIMEOUT: return "Operation timeout";
        case TLS_SYSCALL_CONNECTION_CLOSED: return "Connection closed";
        case TLS_SYSCALL_BUFFER_TOO_SMALL: return "Buffer too small";
        case TLS_SYSCALL_NOT_INITIALIZED: return "TLS not initialized";
        case TLS_SYSCALL_PERMISSION_DENIED: return "Permission denied";
        case TLS_SYSCALL_INVALID_SOCKET: return "Invalid TLS socket";
        case TLS_SYSCALL_CRYPTO_ERROR: return "Cryptographic error";
        case TLS_SYSCALL_PROTOCOL_ERROR: return "Protocol error";
        default: return "Unknown error";
    }
}

int tls_syscall_validate_config(const tls_syscall_config_t* config) {
    if (!config) {
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    // Validate TLS versions
    if (config->min_version > config->max_version) {
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    if (config->min_version < TLS_VERSION_1_0 || config->max_version > TLS_VERSION_1_3) {
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    // Validate timeouts
    if (config->handshake_timeout == 0 || config->handshake_timeout > 300000) { // Max 5 minutes
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    if (config->io_timeout == 0 || config->io_timeout > 60000) { // Max 1 minute
        return TLS_SYSCALL_INVALID_PARAMETER;
    }
    
    return TLS_SYSCALL_SUCCESS;
}

/* ================================
 * Placeholder Implementations
 * ================================ */

// These would be fully implemented in production
int sys_tls_set_config(int tls_socket, const tls_syscall_config_t* config) {
    return TLS_SYSCALL_SUCCESS; // Placeholder
}

int sys_tls_get_config(int tls_socket, tls_syscall_config_t* config) {
    return TLS_SYSCALL_SUCCESS; // Placeholder
}

int sys_tls_verify_certificate(const char* cert_path, const char* ca_path) {
    return TLS_SYSCALL_SUCCESS; // Placeholder
}

int sys_tls_set_certificate(const tls_syscall_certificate_params_t* params) {
    return TLS_SYSCALL_SUCCESS; // Placeholder
}

int sys_tls_add_ca_cert(const char* ca_cert_path) {
    return TLS_SYSCALL_SUCCESS; // Placeholder
}

int sys_tls_session_save(const tls_syscall_session_params_t* params) {
    return TLS_SYSCALL_SUCCESS; // Placeholder
}

int sys_tls_session_resume(const tls_syscall_session_params_t* params) {
    return TLS_SYSCALL_SUCCESS; // Placeholder
}
