/* IKOS DNS User API - User-space DNS Resolution Interface
 * Issue #47 - DNS Resolution Service
 * 
 * Provides user-space DNS resolution functions and utilities
 * that interface with the kernel DNS service through syscalls.
 */

#ifndef DNS_USER_API_H
#define DNS_USER_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../include/net/network.h"

/* ================================
 * DNS User API Constants
 * ================================ */

#define DNS_USER_SUCCESS            0
#define DNS_USER_ERROR             -1
#define DNS_USER_ERROR_INVALID     -2
#define DNS_USER_ERROR_TIMEOUT     -3
#define DNS_USER_ERROR_NXDOMAIN    -4
#define DNS_USER_ERROR_SERVFAIL    -5

#define DNS_USER_MAX_NAME_LEN      255
#define DNS_USER_MAX_SERVERS       8

/* ================================
 * DNS User API Structures
 * ================================ */

/* DNS Configuration for User Applications */
typedef struct {
    char primary_server[16];    /* Primary DNS server IP string */
    char secondary_server[16];  /* Secondary DNS server IP string */
    uint32_t timeout_ms;        /* Query timeout in milliseconds */
    uint32_t max_retries;       /* Maximum retry attempts */
    bool cache_enabled;         /* Enable DNS caching */
} dns_user_config_t;

/* DNS Query Result */
typedef struct {
    char hostname[DNS_USER_MAX_NAME_LEN + 1];
    char ip_address[16];        /* IP address string */
    uint32_t ttl;               /* Time to live */
    uint32_t timestamp;         /* Query timestamp */
} dns_query_result_t;

/* DNS Statistics for User Applications */
typedef struct {
    uint64_t total_queries;
    uint64_t successful_queries;
    uint64_t failed_queries;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t timeouts;
    uint64_t nxdomain_errors;
    double average_response_time;
} dns_user_stats_t;

/* ================================
 * DNS User API Functions
 * ================================ */

/* DNS Library Initialization */
int dns_lib_init(void);
void dns_lib_cleanup(void);
bool dns_lib_is_initialized(void);

/* Basic DNS Resolution */
int dns_resolve_hostname(const char* hostname, char* ip_address, size_t ip_len);
int dns_resolve_ip(const char* ip_address, char* hostname, size_t hostname_len);
int dns_lookup(const char* hostname, dns_query_result_t* result);

/* DNS Configuration Management */
int dns_set_servers(const char* primary, const char* secondary);
int dns_get_servers(char* primary, size_t primary_len, 
                   char* secondary, size_t secondary_len);
int dns_configure(const dns_user_config_t* config);
int dns_get_configuration(dns_user_config_t* config);

/* DNS Cache Management */
int dns_cache_flush(void);
int dns_cache_add_entry(const char* hostname, const char* ip_address, uint32_t ttl);
int dns_cache_remove_entry(const char* hostname);
int dns_cache_lookup(const char* hostname, char* ip_address, size_t ip_len, uint32_t* ttl);

/* DNS Statistics and Monitoring */
int dns_get_statistics(dns_user_stats_t* stats);
void dns_reset_statistics(void);
void dns_print_statistics(void);

/* DNS Validation and Utilities */
bool dns_is_valid_hostname(const char* hostname);
bool dns_is_valid_ip_address(const char* ip_address);
int dns_hostname_to_ip(const char* hostname, uint32_t* ip_addr);
int dns_ip_to_hostname(uint32_t ip_addr, char* hostname, size_t hostname_len);

/* DNS Server Management */
int dns_add_server(const char* server_ip);
int dns_remove_server(const char* server_ip);
int dns_list_servers(char servers[][16], int max_servers);
int dns_test_server(const char* server_ip, uint32_t* response_time_ms);

/* Advanced DNS Functions */
int dns_resolve_mx(const char* domain, char mx_records[][256], int max_records);
int dns_resolve_txt(const char* domain, char* txt_record, size_t txt_len);
int dns_resolve_ns(const char* domain, char ns_records[][256], int max_records);

#endif /* DNS_USER_API_H */
