/*
 * IKOS TLS User-Space API
 * Issue #48: Secure Network Communication (TLS/SSL)
 *
 * User-space interface for TLS/SSL secure communication.
 * Provides POSIX-compatible secure socket operations.
 */

#ifndef TLS_USER_API_H
#define TLS_USER_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ================================
 * TLS User API Constants
 * ================================ */

/* TLS User API Error Codes */
#define TLS_USER_SUCCESS                 0
#define TLS_USER_ERROR                  -1
#define TLS_USER_INVALID_PARAMETER      -2
#define TLS_USER_OUT_OF_MEMORY          -3
#define TLS_USER_SOCKET_ERROR           -4
#define TLS_USER_HANDSHAKE_FAILED       -5
#define TLS_USER_CERTIFICATE_ERROR      -6
#define TLS_USER_TIMEOUT                -7
#define TLS_USER_CONNECTION_CLOSED      -8
#define TLS_USER_BUFFER_TOO_SMALL       -9
#define TLS_USER_NOT_INITIALIZED       -10

/* TLS Protocol Versions */
#define TLS_USER_VERSION_1_0    0x0301
#define TLS_USER_VERSION_1_1    0x0302
#define TLS_USER_VERSION_1_2    0x0303
#define TLS_USER_VERSION_1_3    0x0304

/* Default configuration values */
#define TLS_USER_DEFAULT_TIMEOUT        30000  /* 30 seconds */
#define TLS_USER_MAX_HOSTNAME_LENGTH    253
#define TLS_USER_MAX_CERT_PATH_LENGTH   512

/* ================================
 * TLS User Configuration
 * ================================ */

typedef struct {
    /* TLS version preferences */
    uint16_t min_version;
    uint16_t max_version;
    
    /* Certificate configuration */
    char certificate_file[TLS_USER_MAX_CERT_PATH_LENGTH];
    char private_key_file[TLS_USER_MAX_CERT_PATH_LENGTH];
    char ca_certificate_file[TLS_USER_MAX_CERT_PATH_LENGTH];
    
    /* Security options */
    bool verify_peer;
    bool verify_hostname;
    
    /* Timeouts (in milliseconds) */
    uint32_t handshake_timeout;
    uint32_t io_timeout;
    
    /* Cipher suite preferences */
    bool prefer_strong_ciphers;
    bool allow_weak_ciphers;
    
    /* Session management */
    bool enable_session_resumption;
    uint32_t session_timeout;
} tls_user_config_t;

/* ================================
 * TLS Connection Information
 * ================================ */

typedef struct {
    char hostname[TLS_USER_MAX_HOSTNAME_LENGTH + 1];
    char cipher_suite_name[64];
    char protocol_version[16];
    bool is_verified;
    bool is_encrypted;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t connection_time;
} tls_user_connection_info_t;

/* ================================
 * TLS Certificate Information
 * ================================ */

typedef struct {
    char subject[256];
    char issuer[256];
    char serial_number[64];
    char valid_from[32];
    char valid_to[32];
    char signature_algorithm[64];
    char public_key_algorithm[64];
    uint32_t key_size;
    bool is_valid;
    bool is_expired;
    bool is_self_signed;
} tls_user_certificate_info_t;

/* ================================
 * TLS User Statistics
 * ================================ */

typedef struct {
    uint64_t total_connections;
    uint64_t successful_handshakes;
    uint64_t failed_handshakes;
    uint64_t bytes_encrypted;
    uint64_t bytes_decrypted;
    uint64_t certificates_verified;
    uint64_t session_resumptions;
    uint32_t active_connections;
    double average_handshake_time;
    double average_throughput;
} tls_user_statistics_t;

/* ================================
 * TLS Library Management
 * ================================ */

/**
 * Initialize the TLS user library
 * @return TLS_USER_SUCCESS on success, error code on failure
 */
int tls_user_init(void);

/**
 * Cleanup the TLS user library
 */
void tls_user_cleanup(void);

/**
 * Check if TLS user library is initialized
 * @return true if initialized, false otherwise
 */
bool tls_user_is_initialized(void);

/* ================================
 * TLS Configuration Management
 * ================================ */

/**
 * Create a new TLS configuration with default values
 * @param config Pointer to configuration structure to initialize
 * @return TLS_USER_SUCCESS on success, error code on failure
 */
int tls_user_config_init(tls_user_config_t* config);

/**
 * Set TLS version range
 * @param config Configuration structure
 * @param min_version Minimum TLS version
 * @param max_version Maximum TLS version
 * @return TLS_USER_SUCCESS on success, error code on failure
 */
int tls_user_config_set_version(tls_user_config_t* config, uint16_t min_version, uint16_t max_version);

/**
 * Set certificate files for client/server authentication
 * @param config Configuration structure
 * @param cert_file Path to certificate file
 * @param key_file Path to private key file
 * @return TLS_USER_SUCCESS on success, error code on failure
 */
int tls_user_config_set_certificate(tls_user_config_t* config, const char* cert_file, const char* key_file);

/**
 * Set CA certificate file for peer verification
 * @param config Configuration structure
 * @param ca_file Path to CA certificate file
 * @return TLS_USER_SUCCESS on success, error code on failure
 */
int tls_user_config_set_ca_certificate(tls_user_config_t* config, const char* ca_file);

/**
 * Enable or disable peer certificate verification
 * @param config Configuration structure
 * @param verify_peer Whether to verify peer certificate
 * @param verify_hostname Whether to verify hostname in certificate
 * @return TLS_USER_SUCCESS on success, error code on failure
 */
int tls_user_config_set_verification(tls_user_config_t* config, bool verify_peer, bool verify_hostname);

/**
 * Set timeout values
 * @param config Configuration structure
 * @param handshake_timeout Handshake timeout in milliseconds
 * @param io_timeout I/O operation timeout in milliseconds
 * @return TLS_USER_SUCCESS on success, error code on failure
 */
int tls_user_config_set_timeouts(tls_user_config_t* config, uint32_t handshake_timeout, uint32_t io_timeout);

/* ================================
 * TLS Client Operations
 * ================================ */

/**
 * Create a secure client connection
 * @param hostname Target hostname
 * @param port Target port
 * @param config TLS configuration (NULL for default)
 * @return TLS socket descriptor on success, negative error code on failure
 */
int tls_client_connect(const char* hostname, uint16_t port, const tls_user_config_t* config);

/**
 * Create a secure connection from an existing TCP socket
 * @param tcp_socket Existing TCP socket descriptor
 * @param hostname Target hostname for verification
 * @param config TLS configuration (NULL for default)
 * @return TLS socket descriptor on success, negative error code on failure
 */
int tls_client_connect_socket(int tcp_socket, const char* hostname, const tls_user_config_t* config);

/* ================================
 * TLS Server Operations
 * ================================ */

/**
 * Create a secure server socket
 * @param port Port to bind to
 * @param config TLS configuration (must include certificate)
 * @return TLS server socket descriptor on success, negative error code on failure
 */
int tls_server_create(uint16_t port, const tls_user_config_t* config);

/**
 * Accept a secure client connection
 * @param server_socket TLS server socket descriptor
 * @param client_addr Client address structure (optional)
 * @param addr_len Address structure length
 * @return Client TLS socket descriptor on success, negative error code on failure
 */
int tls_server_accept(int server_socket, void* client_addr, size_t* addr_len);

/**
 * Create a secure server from an existing TCP socket
 * @param tcp_socket Existing TCP server socket
 * @param config TLS configuration (must include certificate)
 * @return TLS server socket descriptor on success, negative error code on failure
 */
int tls_server_create_socket(int tcp_socket, const tls_user_config_t* config);

/* ================================
 * TLS I/O Operations
 * ================================ */

/**
 * Send data over a TLS connection
 * @param tls_socket TLS socket descriptor
 * @param buffer Data to send
 * @param length Number of bytes to send
 * @return Number of bytes sent on success, negative error code on failure
 */
int tls_send(int tls_socket, const void* buffer, size_t length);

/**
 * Receive data from a TLS connection
 * @param tls_socket TLS socket descriptor
 * @param buffer Buffer to store received data
 * @param length Maximum number of bytes to receive
 * @return Number of bytes received on success, negative error code on failure
 */
int tls_recv(int tls_socket, void* buffer, size_t length);

/**
 * Send all data over a TLS connection
 * @param tls_socket TLS socket descriptor
 * @param buffer Data to send
 * @param length Number of bytes to send
 * @return TLS_USER_SUCCESS on success, negative error code on failure
 */
int tls_send_all(int tls_socket, const void* buffer, size_t length);

/**
 * Receive exact amount of data from a TLS connection
 * @param tls_socket TLS socket descriptor
 * @param buffer Buffer to store received data
 * @param length Exact number of bytes to receive
 * @return TLS_USER_SUCCESS on success, negative error code on failure
 */
int tls_recv_all(int tls_socket, void* buffer, size_t length);

/**
 * Check if data is available for reading
 * @param tls_socket TLS socket descriptor
 * @return Number of bytes available, 0 if none, negative error code on failure
 */
int tls_pending(int tls_socket);

/* ================================
 * TLS Connection Management
 * ================================ */

/**
 * Close a TLS connection
 * @param tls_socket TLS socket descriptor
 * @return TLS_USER_SUCCESS on success, negative error code on failure
 */
int tls_close(int tls_socket);

/**
 * Shutdown a TLS connection gracefully
 * @param tls_socket TLS socket descriptor
 * @param how Shutdown direction (similar to socket shutdown)
 * @return TLS_USER_SUCCESS on success, negative error code on failure
 */
int tls_shutdown(int tls_socket, int how);

/**
 * Renegotiate a TLS connection
 * @param tls_socket TLS socket descriptor
 * @return TLS_USER_SUCCESS on success, negative error code on failure
 */
int tls_renegotiate(int tls_socket);

/* ================================
 * TLS Information and Status
 * ================================ */

/**
 * Get connection information
 * @param tls_socket TLS socket descriptor
 * @param info Structure to store connection information
 * @return TLS_USER_SUCCESS on success, negative error code on failure
 */
int tls_get_connection_info(int tls_socket, tls_user_connection_info_t* info);

/**
 * Get peer certificate information
 * @param tls_socket TLS socket descriptor
 * @param cert_info Structure to store certificate information
 * @return TLS_USER_SUCCESS on success, negative error code on failure
 */
int tls_get_peer_certificate_info(int tls_socket, tls_user_certificate_info_t* cert_info);

/**
 * Check if connection is secure and verified
 * @param tls_socket TLS socket descriptor
 * @return 1 if secure and verified, 0 if not verified, negative error code on failure
 */
int tls_is_verified(int tls_socket);

/**
 * Get the cipher suite in use
 * @param tls_socket TLS socket descriptor
 * @param cipher_name Buffer to store cipher suite name
 * @param buffer_size Size of the buffer
 * @return TLS_USER_SUCCESS on success, negative error code on failure
 */
int tls_get_cipher_suite(int tls_socket, char* cipher_name, size_t buffer_size);

/* ================================
 * TLS Utility Functions
 * ================================ */

/**
 * Convert error code to human-readable string
 * @param error_code TLS error code
 * @return Error description string
 */
const char* tls_user_error_string(int error_code);

/**
 * Validate hostname format
 * @param hostname Hostname to validate
 * @return true if valid, false otherwise
 */
bool tls_user_is_valid_hostname(const char* hostname);

/**
 * Check if a certificate file is valid
 * @param cert_file Path to certificate file
 * @return TLS_USER_SUCCESS if valid, error code otherwise
 */
int tls_user_validate_certificate_file(const char* cert_file);

/**
 * Check if a private key file is valid
 * @param key_file Path to private key file
 * @return TLS_USER_SUCCESS if valid, error code otherwise
 */
int tls_user_validate_private_key_file(const char* key_file);

/* ================================
 * TLS Statistics and Monitoring
 * ================================ */

/**
 * Get TLS usage statistics
 * @param stats Structure to store statistics
 * @return TLS_USER_SUCCESS on success, negative error code on failure
 */
int tls_user_get_statistics(tls_user_statistics_t* stats);

/**
 * Reset TLS usage statistics
 * @return TLS_USER_SUCCESS on success, negative error code on failure
 */
int tls_user_reset_statistics(void);

/* ================================
 * Advanced TLS Features
 * ================================ */

/**
 * Set socket options for TLS connection
 * @param tls_socket TLS socket descriptor
 * @param level Socket level
 * @param optname Option name
 * @param optval Option value
 * @param optlen Option value length
 * @return TLS_USER_SUCCESS on success, negative error code on failure
 */
int tls_setsockopt(int tls_socket, int level, int optname, const void* optval, size_t optlen);

/**
 * Get socket options for TLS connection
 * @param tls_socket TLS socket descriptor
 * @param level Socket level
 * @param optname Option name
 * @param optval Buffer to store option value
 * @param optlen Option value buffer length
 * @return TLS_USER_SUCCESS on success, negative error code on failure
 */
int tls_getsockopt(int tls_socket, int level, int optname, void* optval, size_t* optlen);

/**
 * Enable non-blocking mode for TLS socket
 * @param tls_socket TLS socket descriptor
 * @param non_blocking Whether to enable non-blocking mode
 * @return TLS_USER_SUCCESS on success, negative error code on failure
 */
int tls_set_nonblocking(int tls_socket, bool non_blocking);

/* ================================
 * TLS Session Management
 * ================================ */

/**
 * Save TLS session for resumption
 * @param tls_socket TLS socket descriptor
 * @param session_data Buffer to store session data
 * @param session_data_size Size of session data buffer
 * @return Number of bytes written on success, negative error code on failure
 */
int tls_save_session(int tls_socket, void* session_data, size_t session_data_size);

/**
 * Resume TLS session from saved data
 * @param hostname Target hostname
 * @param port Target port
 * @param session_data Saved session data
 * @param session_data_size Size of session data
 * @param config TLS configuration (NULL for default)
 * @return TLS socket descriptor on success, negative error code on failure
 */
int tls_resume_session(const char* hostname, uint16_t port, 
                      const void* session_data, size_t session_data_size,
                      const tls_user_config_t* config);

/* ================================
 * Compatibility Macros
 * ================================ */

/* SSL compatibility aliases */
#define ssl_user_init()                 tls_user_init()
#define ssl_user_cleanup()              tls_user_cleanup()
#define ssl_client_connect(h, p, c)     tls_client_connect(h, p, c)
#define ssl_server_create(p, c)         tls_server_create(p, c)
#define ssl_send(s, b, l)               tls_send(s, b, l)
#define ssl_recv(s, b, l)               tls_recv(s, b, l)
#define ssl_close(s)                    tls_close(s)

/* POSIX-style function names */
#define secure_socket(h, p)             tls_client_connect(h, p, NULL)
#define secure_bind(p)                  tls_server_create(p, NULL)
#define secure_send(s, b, l)            tls_send(s, b, l)
#define secure_recv(s, b, l)            tls_recv(s, b, l)
#define secure_close(s)                 tls_close(s)

#endif /* TLS_USER_API_H */
