/*
 * IKOS TLS Syscalls Interface
 * Issue #48: Secure Network Communication (TLS/SSL)
 *
 * System call interface for TLS/SSL operations in IKOS.
 * Provides bridge between user-space TLS API and kernel TLS implementation.
 */

#ifndef TLS_SYSCALLS_H
#define TLS_SYSCALLS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ================================
 * TLS Syscall Numbers
 * ================================ */

/* TLS syscalls start at 830 (after DNS syscalls) */
#define SYS_TLS_INIT                   830
#define SYS_TLS_CLEANUP                831
#define SYS_TLS_CLIENT_CONNECT         832
#define SYS_TLS_SERVER_CREATE          833
#define SYS_TLS_SERVER_ACCEPT          834
#define SYS_TLS_SEND                   835
#define SYS_TLS_RECV                   836
#define SYS_TLS_CLOSE                  837
#define SYS_TLS_SHUTDOWN               838
#define SYS_TLS_HANDSHAKE              839
#define SYS_TLS_SET_CONFIG             840
#define SYS_TLS_GET_CONFIG             841
#define SYS_TLS_GET_CONNECTION_INFO    842
#define SYS_TLS_GET_PEER_CERT_INFO     843
#define SYS_TLS_VERIFY_CERTIFICATE     844
#define SYS_TLS_SET_CERTIFICATE        845
#define SYS_TLS_ADD_CA_CERT           846
#define SYS_TLS_SESSION_SAVE          847
#define SYS_TLS_SESSION_RESUME        848
#define SYS_TLS_GET_STATISTICS        849
#define SYS_TLS_RESET_STATISTICS      850

/* ================================
 * TLS Syscall Error Codes
 * ================================ */

#define TLS_SYSCALL_SUCCESS                 0
#define TLS_SYSCALL_ERROR                  -1
#define TLS_SYSCALL_INVALID_PARAMETER      -2
#define TLS_SYSCALL_OUT_OF_MEMORY          -3
#define TLS_SYSCALL_SOCKET_ERROR           -4
#define TLS_SYSCALL_HANDSHAKE_FAILED       -5
#define TLS_SYSCALL_CERTIFICATE_ERROR      -6
#define TLS_SYSCALL_TIMEOUT                -7
#define TLS_SYSCALL_CONNECTION_CLOSED      -8
#define TLS_SYSCALL_BUFFER_TOO_SMALL       -9
#define TLS_SYSCALL_NOT_INITIALIZED       -10
#define TLS_SYSCALL_PERMISSION_DENIED     -11
#define TLS_SYSCALL_INVALID_SOCKET        -12
#define TLS_SYSCALL_CRYPTO_ERROR          -13
#define TLS_SYSCALL_PROTOCOL_ERROR        -14

/* ================================
 * TLS Syscall Data Structures
 * ================================ */

/* TLS Configuration for syscalls */
typedef struct {
    uint16_t min_version;
    uint16_t max_version;
    
    char certificate_path[512];
    char private_key_path[512];
    char ca_certificate_path[512];
    
    bool verify_peer;
    bool verify_hostname;
    
    uint32_t handshake_timeout;
    uint32_t io_timeout;
    
    bool prefer_strong_ciphers;
    bool allow_weak_ciphers;
    bool enable_session_resumption;
    uint32_t session_timeout;
} tls_syscall_config_t;

/* TLS Connection Information for syscalls */
typedef struct {
    char hostname[254];
    char cipher_suite_name[64];
    char protocol_version[16];
    bool is_verified;
    bool is_encrypted;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t connection_time;
    int socket_fd;
    uint32_t connection_id;
} tls_syscall_connection_info_t;

/* TLS Certificate Information for syscalls */
typedef struct {
    char subject[256];
    char issuer[256];
    char serial_number[64];
    uint64_t valid_from;
    uint64_t valid_to;
    char signature_algorithm[64];
    char public_key_algorithm[64];
    uint32_t key_size;
    bool is_valid;
    bool is_expired;
    bool is_self_signed;
    bool is_ca;
} tls_syscall_certificate_info_t;

/* TLS Statistics for syscalls */
typedef struct {
    uint64_t total_connections;
    uint64_t successful_handshakes;
    uint64_t failed_handshakes;
    uint64_t bytes_encrypted;
    uint64_t bytes_decrypted;
    uint64_t certificates_verified;
    uint64_t session_resumptions;
    uint32_t active_connections;
    uint64_t handshake_time_total;
    uint64_t throughput_total;
} tls_syscall_statistics_t;

/* TLS Session Data for syscalls */
typedef struct {
    uint8_t session_id[32];
    uint8_t session_id_length;
    uint8_t master_secret[48];
    uint16_t cipher_suite;
    uint64_t creation_time;
    uint32_t timeout;
    char server_name[254];
    uint16_t server_port;
} tls_syscall_session_data_t;

/* ================================
 * TLS Syscall Parameter Structures
 * ================================ */

/* Parameters for TLS client connect syscall */
typedef struct {
    const char* hostname;
    uint16_t port;
    const tls_syscall_config_t* config;
    int tcp_socket_fd;  /* -1 to create new socket */
} tls_syscall_client_connect_params_t;

/* Parameters for TLS server create syscall */
typedef struct {
    uint16_t port;
    const tls_syscall_config_t* config;
    int tcp_socket_fd;  /* -1 to create new socket */
} tls_syscall_server_create_params_t;

/* Parameters for TLS send/recv syscalls */
typedef struct {
    int tls_socket;
    void* buffer;
    size_t length;
    int flags;
} tls_syscall_io_params_t;

/* Parameters for certificate operations */
typedef struct {
    const char* certificate_path;
    const char* private_key_path;
    const char* password;
} tls_syscall_certificate_params_t;

/* Parameters for session operations */
typedef struct {
    int tls_socket;
    tls_syscall_session_data_t* session_data;
    size_t session_data_size;
} tls_syscall_session_params_t;

/* ================================
 * TLS Syscall Function Prototypes
 * ================================ */

#ifdef __KERNEL__

/* Kernel-side syscall implementations */
int sys_tls_init(void);
int sys_tls_cleanup(void);

int sys_tls_client_connect(const tls_syscall_client_connect_params_t* params);
int sys_tls_server_create(const tls_syscall_server_create_params_t* params);
int sys_tls_server_accept(int server_socket, void* client_addr, size_t* addr_len);

int sys_tls_send(const tls_syscall_io_params_t* params);
int sys_tls_recv(const tls_syscall_io_params_t* params);

int sys_tls_close(int tls_socket);
int sys_tls_shutdown(int tls_socket, int how);
int sys_tls_handshake(int tls_socket);

int sys_tls_set_config(int tls_socket, const tls_syscall_config_t* config);
int sys_tls_get_config(int tls_socket, tls_syscall_config_t* config);

int sys_tls_get_connection_info(int tls_socket, tls_syscall_connection_info_t* info);
int sys_tls_get_peer_cert_info(int tls_socket, tls_syscall_certificate_info_t* cert_info);

int sys_tls_verify_certificate(const char* cert_path, const char* ca_path);
int sys_tls_set_certificate(const tls_syscall_certificate_params_t* params);
int sys_tls_add_ca_cert(const char* ca_cert_path);

int sys_tls_session_save(const tls_syscall_session_params_t* params);
int sys_tls_session_resume(const tls_syscall_session_params_t* params);

int sys_tls_get_statistics(tls_syscall_statistics_t* stats);
int sys_tls_reset_statistics(void);

#else

/* User-space syscall wrappers */
static inline int syscall_tls_init(void) {
    return syscall(SYS_TLS_INIT);
}

static inline int syscall_tls_cleanup(void) {
    return syscall(SYS_TLS_CLEANUP);
}

static inline int syscall_tls_client_connect(const tls_syscall_client_connect_params_t* params) {
    return syscall(SYS_TLS_CLIENT_CONNECT, params);
}

static inline int syscall_tls_server_create(const tls_syscall_server_create_params_t* params) {
    return syscall(SYS_TLS_SERVER_CREATE, params);
}

static inline int syscall_tls_server_accept(int server_socket, void* client_addr, size_t* addr_len) {
    return syscall(SYS_TLS_SERVER_ACCEPT, server_socket, client_addr, addr_len);
}

static inline int syscall_tls_send(const tls_syscall_io_params_t* params) {
    return syscall(SYS_TLS_SEND, params);
}

static inline int syscall_tls_recv(const tls_syscall_io_params_t* params) {
    return syscall(SYS_TLS_RECV, params);
}

static inline int syscall_tls_close(int tls_socket) {
    return syscall(SYS_TLS_CLOSE, tls_socket);
}

static inline int syscall_tls_shutdown(int tls_socket, int how) {
    return syscall(SYS_TLS_SHUTDOWN, tls_socket, how);
}

static inline int syscall_tls_handshake(int tls_socket) {
    return syscall(SYS_TLS_HANDSHAKE, tls_socket);
}

static inline int syscall_tls_set_config(int tls_socket, const tls_syscall_config_t* config) {
    return syscall(SYS_TLS_SET_CONFIG, tls_socket, config);
}

static inline int syscall_tls_get_config(int tls_socket, tls_syscall_config_t* config) {
    return syscall(SYS_TLS_GET_CONFIG, tls_socket, config);
}

static inline int syscall_tls_get_connection_info(int tls_socket, tls_syscall_connection_info_t* info) {
    return syscall(SYS_TLS_GET_CONNECTION_INFO, tls_socket, info);
}

static inline int syscall_tls_get_peer_cert_info(int tls_socket, tls_syscall_certificate_info_t* cert_info) {
    return syscall(SYS_TLS_GET_PEER_CERT_INFO, tls_socket, cert_info);
}

static inline int syscall_tls_verify_certificate(const char* cert_path, const char* ca_path) {
    return syscall(SYS_TLS_VERIFY_CERTIFICATE, cert_path, ca_path);
}

static inline int syscall_tls_set_certificate(const tls_syscall_certificate_params_t* params) {
    return syscall(SYS_TLS_SET_CERTIFICATE, params);
}

static inline int syscall_tls_add_ca_cert(const char* ca_cert_path) {
    return syscall(SYS_TLS_ADD_CA_CERT, ca_cert_path);
}

static inline int syscall_tls_session_save(const tls_syscall_session_params_t* params) {
    return syscall(SYS_TLS_SESSION_SAVE, params);
}

static inline int syscall_tls_session_resume(const tls_syscall_session_params_t* params) {
    return syscall(SYS_TLS_SESSION_RESUME, params);
}

static inline int syscall_tls_get_statistics(tls_syscall_statistics_t* stats) {
    return syscall(SYS_TLS_GET_STATISTICS, stats);
}

static inline int syscall_tls_reset_statistics(void) {
    return syscall(SYS_TLS_RESET_STATISTICS);
}

#endif /* __KERNEL__ */

/* ================================
 * TLS Syscall Utility Functions
 * ================================ */

/**
 * Convert TLS syscall error code to string
 * @param error_code TLS syscall error code
 * @return Error description string
 */
const char* tls_syscall_error_string(int error_code);

/**
 * Validate TLS syscall configuration structure
 * @param config Configuration to validate
 * @return TLS_SYSCALL_SUCCESS if valid, error code otherwise
 */
int tls_syscall_validate_config(const tls_syscall_config_t* config);

/**
 * Copy user configuration to syscall configuration
 * @param user_config User-space configuration
 * @param syscall_config Syscall configuration to populate
 * @return TLS_SYSCALL_SUCCESS on success, error code on failure
 */
int tls_syscall_config_from_user(const void* user_config, tls_syscall_config_t* syscall_config);

/**
 * Copy syscall configuration to user configuration
 * @param syscall_config Syscall configuration
 * @param user_config User-space configuration to populate
 * @return TLS_SYSCALL_SUCCESS on success, error code on failure
 */
int tls_syscall_config_to_user(const tls_syscall_config_t* syscall_config, void* user_config);

/* ================================
 * TLS Socket Management
 * ================================ */

#define TLS_SOCKET_FLAG_CLIENT      0x01
#define TLS_SOCKET_FLAG_SERVER      0x02
#define TLS_SOCKET_FLAG_CONNECTED   0x04
#define TLS_SOCKET_FLAG_ENCRYPTED   0x08
#define TLS_SOCKET_FLAG_VERIFIED    0x10
#define TLS_SOCKET_FLAG_NONBLOCKING 0x20

/* TLS socket descriptor validation */
bool tls_syscall_is_valid_socket(int tls_socket);
int tls_syscall_get_socket_flags(int tls_socket);
int tls_syscall_set_socket_flags(int tls_socket, int flags);

/* ================================
 * TLS Security Levels
 * ================================ */

#define TLS_SECURITY_LEVEL_LOW      0  /* Allow weak ciphers */
#define TLS_SECURITY_LEVEL_MEDIUM   1  /* Balanced security */
#define TLS_SECURITY_LEVEL_HIGH     2  /* Strong security only */
#define TLS_SECURITY_LEVEL_ULTRA    3  /* Maximum security */

int tls_syscall_set_security_level(int level);
int tls_syscall_get_security_level(void);

/* ================================
 * TLS Debug and Logging
 * ================================ */

#define TLS_LOG_LEVEL_ERROR    0
#define TLS_LOG_LEVEL_WARN     1
#define TLS_LOG_LEVEL_INFO     2
#define TLS_LOG_LEVEL_DEBUG    3
#define TLS_LOG_LEVEL_TRACE    4

int tls_syscall_set_log_level(int level);
int tls_syscall_get_log_level(void);

/* ================================
 * TLS Performance Monitoring
 * ================================ */

typedef struct {
    uint64_t handshake_time_min;
    uint64_t handshake_time_max;
    uint64_t handshake_time_avg;
    uint64_t throughput_min;
    uint64_t throughput_max;
    uint64_t throughput_avg;
    uint32_t connection_failures;
    uint32_t certificate_failures;
    uint32_t protocol_failures;
} tls_syscall_performance_t;

int tls_syscall_get_performance_stats(tls_syscall_performance_t* perf);
int tls_syscall_reset_performance_stats(void);

#endif /* TLS_SYSCALLS_H */
