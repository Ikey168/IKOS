/* IKOS DNS Syscalls - Kernel DNS Service Interface
 * Issue #47 - DNS Resolution Service
 * 
 * Provides syscall interface between user-space DNS library
 * and kernel DNS service for domain name resolution.
 */

#ifndef DNS_SYSCALLS_H
#define DNS_SYSCALLS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../include/net/network.h"

/* ================================
 * DNS Syscall Numbers
 * ================================ */

#define SYS_DNS_RESOLVE         810    /* Resolve hostname to IP */
#define SYS_DNS_REVERSE         811    /* Reverse DNS lookup */
#define SYS_DNS_CONFIGURE       812    /* Configure DNS settings */
#define SYS_DNS_GET_CONFIG      813    /* Get DNS configuration */
#define SYS_DNS_CACHE_ADD       814    /* Add cache entry */
#define SYS_DNS_CACHE_LOOKUP    815    /* Lookup cache entry */
#define SYS_DNS_CACHE_REMOVE    816    /* Remove cache entry */
#define SYS_DNS_CACHE_CLEAR     817    /* Clear DNS cache */
#define SYS_DNS_GET_STATS       818    /* Get DNS statistics */
#define SYS_DNS_RESET_STATS     819    /* Reset DNS statistics */
#define SYS_DNS_SET_SERVERS     820    /* Set DNS servers */
#define SYS_DNS_GET_SERVERS     821    /* Get DNS servers */

/* ================================
 * DNS Syscall Error Codes
 * ================================ */

#define DNS_SYSCALL_SUCCESS         0
#define DNS_SYSCALL_ERROR          -1
#define DNS_SYSCALL_INVALID        -2
#define DNS_SYSCALL_TIMEOUT        -3
#define DNS_SYSCALL_NXDOMAIN       -4
#define DNS_SYSCALL_SERVFAIL       -5
#define DNS_SYSCALL_REFUSED        -6
#define DNS_SYSCALL_NO_MEMORY      -7
#define DNS_SYSCALL_CACHE_FULL     -8

/* ================================
 * DNS Syscall Structures
 * ================================ */

/* DNS Configuration Structure for Syscalls */
typedef struct {
    uint32_t primary_server;    /* Primary DNS server IP */
    uint32_t secondary_server;  /* Secondary DNS server IP */
    uint32_t timeout_ms;        /* Query timeout in milliseconds */
    uint32_t max_retries;       /* Maximum retry attempts */
    uint32_t cache_enabled;     /* Enable DNS caching (0/1) */
    uint32_t cache_max_entries; /* Maximum cache entries */
    uint32_t default_ttl;       /* Default TTL for cache entries */
} dns_syscall_config_t;

/* DNS Statistics Structure for Syscalls */
typedef struct {
    uint64_t queries_sent;      /* Total queries sent */
    uint64_t responses_received; /* Total responses received */
    uint64_t cache_hits;        /* Cache hits */
    uint64_t cache_misses;      /* Cache misses */
    uint64_t timeouts;          /* Query timeouts */
    uint64_t errors;            /* Query errors */
    uint64_t nxdomain;          /* NXDOMAIN responses */
    uint64_t servfail;          /* Server failure responses */
    uint32_t cache_entries;     /* Current cache entries */
    uint32_t memory_used;       /* Memory used by cache */
} dns_syscall_stats_t;

/* DNS Server List Structure */
typedef struct {
    uint32_t servers[8];        /* DNS server IPs */
    uint32_t count;             /* Number of servers */
} dns_syscall_servers_t;

/* DNS Cache Entry Structure for Syscalls */
typedef struct {
    char hostname[256];         /* Hostname */
    uint16_t type;              /* Record type */
    uint16_t class;             /* Record class */
    uint32_t ttl;               /* Time to live */
    uint16_t data_len;          /* Data length */
    uint8_t data[256];          /* Record data */
} dns_syscall_cache_entry_t;

/* ================================
 * DNS Syscall Function Prototypes
 * ================================ */

/* DNS Resolution Syscalls */
long sys_dns_resolve(const char* hostname, uint32_t* ip_addr);
long sys_dns_reverse(uint32_t ip_addr, char* hostname, size_t hostname_len);

/* DNS Configuration Syscalls */
long sys_dns_configure(const dns_syscall_config_t* config);
long sys_dns_get_config(dns_syscall_config_t* config);
long sys_dns_set_servers(const dns_syscall_servers_t* servers);
long sys_dns_get_servers(dns_syscall_servers_t* servers);

/* DNS Cache Management Syscalls */
long sys_dns_cache_add(const dns_syscall_cache_entry_t* entry);
long sys_dns_cache_lookup(const char* hostname, uint16_t type, uint16_t class,
                          dns_syscall_cache_entry_t* entry);
long sys_dns_cache_remove(const char* hostname, uint16_t type, uint16_t class);
long sys_dns_cache_clear(void);

/* DNS Statistics Syscalls */
long sys_dns_get_stats(dns_syscall_stats_t* stats);
long sys_dns_reset_stats(void);

/* DNS Syscall Validation Functions */
bool is_valid_dns_hostname(const char* hostname);
bool is_valid_dns_config(const dns_syscall_config_t* config);
bool is_valid_dns_servers(const dns_syscall_servers_t* servers);

/* DNS Syscall Helper Functions */
int dns_syscall_copy_from_user(void* dest, const void* src, size_t size);
int dns_syscall_copy_to_user(void* dest, const void* src, size_t size);
int dns_syscall_validate_string(const char* str, size_t max_len);

#endif /* DNS_SYSCALLS_H */
