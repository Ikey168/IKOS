/*
 * IKOS TLS/SSL Implementation
 * Issue #48: Secure Network Communication (TLS/SSL)
 *
 * This header defines the TLS/SSL protocol implementation for secure
 * network communication in the IKOS operating system.
 */

#ifndef NET_TLS_H
#define NET_TLS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ================================
 * TLS Version Support
 * ================================ */

#define TLS_VERSION_1_0     0x0301
#define TLS_VERSION_1_1     0x0302
#define TLS_VERSION_1_2     0x0303
#define TLS_VERSION_1_3     0x0304

/* Default supported version */
#define TLS_DEFAULT_VERSION TLS_VERSION_1_2

/* ================================
 * TLS Record Layer
 * ================================ */

/* TLS Content Types */
#define TLS_CONTENT_CHANGE_CIPHER_SPEC  20
#define TLS_CONTENT_ALERT              21
#define TLS_CONTENT_HANDSHAKE          22
#define TLS_CONTENT_APPLICATION_DATA   23

/* TLS Record Header */
typedef struct {
    uint8_t content_type;
    uint16_t version;
    uint16_t length;
} __attribute__((packed)) tls_record_header_t;

#define TLS_RECORD_HEADER_SIZE 5
#define TLS_MAX_RECORD_SIZE    16384

/* ================================
 * TLS Handshake Protocol
 * ================================ */

/* Handshake Message Types */
#define TLS_HANDSHAKE_HELLO_REQUEST         0
#define TLS_HANDSHAKE_CLIENT_HELLO          1
#define TLS_HANDSHAKE_SERVER_HELLO          2
#define TLS_HANDSHAKE_CERTIFICATE          11
#define TLS_HANDSHAKE_SERVER_KEY_EXCHANGE  12
#define TLS_HANDSHAKE_CERTIFICATE_REQUEST  13
#define TLS_HANDSHAKE_SERVER_HELLO_DONE    14
#define TLS_HANDSHAKE_CERTIFICATE_VERIFY   15
#define TLS_HANDSHAKE_CLIENT_KEY_EXCHANGE  16
#define TLS_HANDSHAKE_FINISHED             20

/* Handshake Message Header */
typedef struct {
    uint8_t msg_type;
    uint32_t length : 24;
} __attribute__((packed)) tls_handshake_header_t;

/* Client Hello Message */
typedef struct {
    uint16_t version;
    uint8_t random[32];
    uint8_t session_id_length;
    /* Variable length fields follow */
} __attribute__((packed)) tls_client_hello_t;

/* Server Hello Message */
typedef struct {
    uint16_t version;
    uint8_t random[32];
    uint8_t session_id_length;
    /* Variable length fields follow */
} __attribute__((packed)) tls_server_hello_t;

/* ================================
 * TLS Cipher Suites
 * ================================ */

/* Supported Cipher Suites */
#define TLS_RSA_WITH_AES_128_CBC_SHA256         0x003C
#define TLS_RSA_WITH_AES_256_CBC_SHA256         0x003D
#define TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256   0xC02F
#define TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384   0xC030

/* Cipher Suite Information */
typedef struct {
    uint16_t suite_id;
    const char* name;
    uint8_t key_exchange;
    uint8_t bulk_cipher;
    uint8_t mac_algorithm;
    uint16_t key_length;
    uint16_t iv_length;
    uint16_t mac_length;
} tls_cipher_suite_info_t;

/* Key Exchange Methods */
#define TLS_KX_RSA      1
#define TLS_KX_ECDHE    2

/* Bulk Ciphers */
#define TLS_CIPHER_AES_128_CBC  1
#define TLS_CIPHER_AES_256_CBC  2
#define TLS_CIPHER_AES_128_GCM  3
#define TLS_CIPHER_AES_256_GCM  4

/* MAC Algorithms */
#define TLS_MAC_SHA256  1
#define TLS_MAC_SHA384  2

/* ================================
 * TLS Connection State
 * ================================ */

/* TLS Connection States */
typedef enum {
    TLS_STATE_INIT = 0,
    TLS_STATE_CLIENT_HELLO_SENT,
    TLS_STATE_SERVER_HELLO_RECEIVED,
    TLS_STATE_CERTIFICATE_RECEIVED,
    TLS_STATE_KEY_EXCHANGE_RECEIVED,
    TLS_STATE_SERVER_HELLO_DONE_RECEIVED,
    TLS_STATE_CLIENT_KEY_EXCHANGE_SENT,
    TLS_STATE_CHANGE_CIPHER_SPEC_SENT,
    TLS_STATE_FINISHED_SENT,
    TLS_STATE_CHANGE_CIPHER_SPEC_RECEIVED,
    TLS_STATE_FINISHED_RECEIVED,
    TLS_STATE_ESTABLISHED,
    TLS_STATE_ALERT_SENT,
    TLS_STATE_CLOSED,
    TLS_STATE_ERROR
} tls_connection_state_t;

/* TLS Security Parameters */
typedef struct {
    uint16_t cipher_suite;
    uint8_t compression_method;
    uint8_t master_secret[48];
    uint8_t client_random[32];
    uint8_t server_random[32];
    uint8_t session_id[32];
    uint8_t session_id_length;
} tls_security_parameters_t;

/* TLS Key Material */
typedef struct {
    uint8_t* client_write_mac_key;
    uint8_t* server_write_mac_key;
    uint8_t* client_write_key;
    uint8_t* server_write_key;
    uint8_t* client_write_iv;
    uint8_t* server_write_iv;
    size_t mac_key_length;
    size_t key_length;
    size_t iv_length;
} tls_key_material_t;

/* TLS Connection Context */
typedef struct {
    int socket_fd;
    bool is_server;
    tls_connection_state_t state;
    uint16_t version;
    
    /* Security parameters */
    tls_security_parameters_t security_params;
    tls_key_material_t key_material;
    
    /* Handshake tracking */
    uint8_t* handshake_messages;
    size_t handshake_messages_length;
    
    /* Record layer */
    uint64_t read_sequence_number;
    uint64_t write_sequence_number;
    
    /* Buffers */
    uint8_t* read_buffer;
    size_t read_buffer_size;
    size_t read_buffer_pos;
    
    uint8_t* write_buffer;
    size_t write_buffer_size;
    size_t write_buffer_pos;
    
    /* Certificate chain */
    struct tls_certificate* certificate_chain;
    size_t certificate_chain_length;
    
    /* Session management */
    bool session_resumption;
    uint32_t session_timeout;
    
    /* Error information */
    int last_error;
    char error_message[256];
} tls_connection_t;

/* ================================
 * TLS Certificate Structure
 * ================================ */

/* X.509 Certificate (simplified) */
typedef struct tls_certificate {
    uint8_t* der_data;
    size_t der_length;
    
    /* Parsed certificate fields */
    char subject[256];
    char issuer[256];
    char serial_number[64];
    uint64_t not_before;
    uint64_t not_after;
    
    /* Public key information */
    uint8_t* public_key;
    size_t public_key_length;
    uint8_t public_key_algorithm;
    
    /* Signature */
    uint8_t* signature;
    size_t signature_length;
    uint8_t signature_algorithm;
    
    struct tls_certificate* next;
} tls_certificate_t;

/* ================================
 * TLS Alert Protocol
 * ================================ */

/* Alert Levels */
#define TLS_ALERT_WARNING  1
#define TLS_ALERT_FATAL    2

/* Alert Descriptions */
#define TLS_ALERT_CLOSE_NOTIFY               0
#define TLS_ALERT_UNEXPECTED_MESSAGE        10
#define TLS_ALERT_BAD_RECORD_MAC            20
#define TLS_ALERT_DECRYPTION_FAILED         21
#define TLS_ALERT_RECORD_OVERFLOW           22
#define TLS_ALERT_DECOMPRESSION_FAILURE     30
#define TLS_ALERT_HANDSHAKE_FAILURE         40
#define TLS_ALERT_NO_CERTIFICATE            41
#define TLS_ALERT_BAD_CERTIFICATE           42
#define TLS_ALERT_UNSUPPORTED_CERTIFICATE   43
#define TLS_ALERT_CERTIFICATE_REVOKED       44
#define TLS_ALERT_CERTIFICATE_EXPIRED       45
#define TLS_ALERT_CERTIFICATE_UNKNOWN       46
#define TLS_ALERT_ILLEGAL_PARAMETER         47
#define TLS_ALERT_UNKNOWN_CA                48
#define TLS_ALERT_ACCESS_DENIED             49
#define TLS_ALERT_DECODE_ERROR              50
#define TLS_ALERT_DECRYPT_ERROR             51
#define TLS_ALERT_PROTOCOL_VERSION          70
#define TLS_ALERT_INSUFFICIENT_SECURITY     71
#define TLS_ALERT_INTERNAL_ERROR            80
#define TLS_ALERT_USER_CANCELED             90
#define TLS_ALERT_NO_RENEGOTIATION         100

/* Alert Message */
typedef struct {
    uint8_t level;
    uint8_t description;
} __attribute__((packed)) tls_alert_t;

/* ================================
 * TLS Configuration
 * ================================ */

typedef struct {
    /* Supported versions */
    uint16_t min_version;
    uint16_t max_version;
    
    /* Supported cipher suites */
    uint16_t* cipher_suites;
    size_t cipher_suites_count;
    
    /* Certificate and private key */
    tls_certificate_t* certificate;
    uint8_t* private_key;
    size_t private_key_length;
    
    /* CA certificates for verification */
    tls_certificate_t* ca_certificates;
    size_t ca_certificates_count;
    
    /* Session configuration */
    uint32_t session_timeout;
    bool session_cache_enabled;
    
    /* Security options */
    bool verify_peer;
    bool verify_hostname;
    
    /* Buffer sizes */
    size_t read_buffer_size;
    size_t write_buffer_size;
    
    /* Timeouts */
    uint32_t handshake_timeout;
    uint32_t io_timeout;
} tls_config_t;

/* ================================
 * TLS Error Codes
 * ================================ */

#define TLS_SUCCESS                      0
#define TLS_ERROR_GENERIC               -1
#define TLS_ERROR_INVALID_PARAMETER     -2
#define TLS_ERROR_OUT_OF_MEMORY         -3
#define TLS_ERROR_SOCKET_ERROR          -4
#define TLS_ERROR_HANDSHAKE_FAILED      -5
#define TLS_ERROR_CERTIFICATE_INVALID   -6
#define TLS_ERROR_CERTIFICATE_EXPIRED   -7
#define TLS_ERROR_UNKNOWN_CA            -8
#define TLS_ERROR_PROTOCOL_VERSION      -9
#define TLS_ERROR_CIPHER_SUITE         -10
#define TLS_ERROR_DECODE_ERROR         -11
#define TLS_ERROR_ENCRYPT_ERROR        -12
#define TLS_ERROR_DECRYPT_ERROR        -13
#define TLS_ERROR_MAC_VERIFY_FAILED    -14
#define TLS_ERROR_TIMEOUT              -15
#define TLS_ERROR_CONNECTION_CLOSED    -16
#define TLS_ERROR_ALERT_RECEIVED       -17
#define TLS_ERROR_BUFFER_TOO_SMALL     -18
#define TLS_ERROR_INVALID_STATE        -19
#define TLS_ERROR_COMPRESSION_FAILED   -20

/* ================================
 * TLS Core Functions
 * ================================ */

/* TLS Library Management */
int tls_init(void);
void tls_cleanup(void);
bool tls_is_initialized(void);

/* TLS Configuration */
tls_config_t* tls_config_new(void);
void tls_config_free(tls_config_t* config);
int tls_config_set_version(tls_config_t* config, uint16_t min_version, uint16_t max_version);
int tls_config_set_cipher_suites(tls_config_t* config, uint16_t* suites, size_t count);
int tls_config_set_certificate(tls_config_t* config, const char* cert_file, const char* key_file);
int tls_config_add_ca_certificate(tls_config_t* config, const char* ca_file);
int tls_config_set_verify_peer(tls_config_t* config, bool verify);

/* TLS Connection Management */
tls_connection_t* tls_connection_new(int socket_fd, bool is_server);
void tls_connection_free(tls_connection_t* conn);
int tls_connection_configure(tls_connection_t* conn, const tls_config_t* config);

/* TLS Handshake */
int tls_handshake(tls_connection_t* conn);
int tls_handshake_client(tls_connection_t* conn);
int tls_handshake_server(tls_connection_t* conn);

/* TLS I/O Operations */
int tls_read(tls_connection_t* conn, void* buffer, size_t length);
int tls_write(tls_connection_t* conn, const void* buffer, size_t length);
int tls_pending(tls_connection_t* conn);

/* TLS Connection Control */
int tls_close(tls_connection_t* conn);
int tls_shutdown(tls_connection_t* conn);
int tls_renegotiate(tls_connection_t* conn);

/* ================================
 * TLS Record Layer Functions
 * ================================ */

int tls_record_send(tls_connection_t* conn, uint8_t content_type, 
                   const void* data, size_t length);
int tls_record_receive(tls_connection_t* conn, uint8_t* content_type, 
                      void* data, size_t* length);

/* ================================
 * TLS Handshake Message Functions
 * ================================ */

int tls_send_client_hello(tls_connection_t* conn);
int tls_send_server_hello(tls_connection_t* conn);
int tls_send_certificate(tls_connection_t* conn);
int tls_send_server_key_exchange(tls_connection_t* conn);
int tls_send_server_hello_done(tls_connection_t* conn);
int tls_send_client_key_exchange(tls_connection_t* conn);
int tls_send_change_cipher_spec(tls_connection_t* conn);
int tls_send_finished(tls_connection_t* conn);

int tls_process_client_hello(tls_connection_t* conn, const uint8_t* data, size_t length);
int tls_process_server_hello(tls_connection_t* conn, const uint8_t* data, size_t length);
int tls_process_certificate(tls_connection_t* conn, const uint8_t* data, size_t length);
int tls_process_server_key_exchange(tls_connection_t* conn, const uint8_t* data, size_t length);
int tls_process_server_hello_done(tls_connection_t* conn, const uint8_t* data, size_t length);
int tls_process_client_key_exchange(tls_connection_t* conn, const uint8_t* data, size_t length);
int tls_process_change_cipher_spec(tls_connection_t* conn, const uint8_t* data, size_t length);
int tls_process_finished(tls_connection_t* conn, const uint8_t* data, size_t length);

/* ================================
 * TLS Cryptographic Functions
 * ================================ */

/* Key derivation */
int tls_derive_keys(tls_connection_t* conn);
int tls_generate_master_secret(tls_connection_t* conn, const uint8_t* premaster_secret, size_t premaster_length);
int tls_generate_key_material(tls_connection_t* conn);

/* PRF (Pseudo-Random Function) */
int tls_prf(const uint8_t* secret, size_t secret_length,
           const char* label,
           const uint8_t* seed, size_t seed_length,
           uint8_t* output, size_t output_length);

/* HMAC operations */
int tls_hmac_sha256(const uint8_t* key, size_t key_length,
                   const uint8_t* data, size_t data_length,
                   uint8_t* output);

/* AES operations */
int tls_aes_encrypt(const uint8_t* key, size_t key_length,
                   const uint8_t* iv, const uint8_t* input, 
                   uint8_t* output, size_t length);
int tls_aes_decrypt(const uint8_t* key, size_t key_length,
                   const uint8_t* iv, const uint8_t* input, 
                   uint8_t* output, size_t length);

/* ================================
 * TLS Certificate Functions
 * ================================ */

tls_certificate_t* tls_certificate_parse(const uint8_t* der_data, size_t der_length);
void tls_certificate_free(tls_certificate_t* cert);
int tls_certificate_verify(const tls_certificate_t* cert, const tls_certificate_t* ca_cert);
int tls_certificate_check_hostname(const tls_certificate_t* cert, const char* hostname);
bool tls_certificate_is_valid_time(const tls_certificate_t* cert);

/* ================================
 * TLS Utility Functions
 * ================================ */

const char* tls_error_string(int error_code);
const char* tls_state_string(tls_connection_state_t state);
const char* tls_cipher_suite_name(uint16_t cipher_suite);
const char* tls_version_string(uint16_t version);

/* TLS connection information */
int tls_get_cipher_suite(const tls_connection_t* conn);
int tls_get_version(const tls_connection_t* conn);
const tls_certificate_t* tls_get_peer_certificate(const tls_connection_t* conn);

/* TLS statistics */
typedef struct {
    uint64_t connections_created;
    uint64_t handshakes_completed;
    uint64_t handshakes_failed;
    uint64_t bytes_encrypted;
    uint64_t bytes_decrypted;
    uint64_t alerts_sent;
    uint64_t alerts_received;
    uint32_t active_connections;
} tls_statistics_t;

int tls_get_statistics(tls_statistics_t* stats);
void tls_reset_statistics(void);

/* ================================
 * TLS Session Management
 * ================================ */

typedef struct {
    uint8_t session_id[32];
    uint8_t session_id_length;
    uint8_t master_secret[48];
    uint16_t cipher_suite;
    uint64_t creation_time;
    uint32_t timeout;
    bool valid;
} tls_session_t;

int tls_session_cache_add(const tls_session_t* session);
int tls_session_cache_lookup(const uint8_t* session_id, uint8_t session_id_length, 
                            tls_session_t* session);
int tls_session_cache_remove(const uint8_t* session_id, uint8_t session_id_length);
void tls_session_cache_cleanup(void);

/* ================================
 * TLS Extensions Support
 * ================================ */

/* Extension Types */
#define TLS_EXT_SERVER_NAME             0
#define TLS_EXT_STATUS_REQUEST          5
#define TLS_EXT_SUPPORTED_GROUPS       10
#define TLS_EXT_EC_POINT_FORMATS       11
#define TLS_EXT_SIGNATURE_ALGORITHMS   13
#define TLS_EXT_APPLICATION_LAYER_PROTOCOL_NEGOTIATION 16

/* Extension processing */
int tls_process_extensions(tls_connection_t* conn, const uint8_t* extensions, size_t length);
int tls_add_server_name_extension(uint8_t* buffer, size_t* length, const char* hostname);
int tls_add_signature_algorithms_extension(uint8_t* buffer, size_t* length);

#endif /* NET_TLS_H */
