/*
 * IKOS Operating System - DNS Syscalls Implementation
 * Issue #47: DNS Resolution Service
 *
 * Implements syscall interface for DNS resolution services,
 * bridging user-space DNS requests with kernel DNS functionality.
 */

#include "../include/dns_syscalls.h"
#include "../include/net/dns.h"
#include "../include/syscalls.h"
#include "../include/process.h"
#include "../include/memory.h"
#include <stdio.h>
#include <string.h>

/* ================================
 * DNS Syscall Implementation
 * ================================ */

long sys_dns_resolve(const char* hostname, uint32_t* ip_addr) {
    if (!hostname || !ip_addr) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Validate hostname string */
    char kernel_hostname[DNS_MAX_NAME_LEN + 1];
    if (dns_syscall_copy_from_user(kernel_hostname, hostname, sizeof(kernel_hostname)) != 0) {
        return DNS_SYSCALL_INVALID;
    }
    kernel_hostname[DNS_MAX_NAME_LEN] = '\0';
    
    if (!is_valid_dns_hostname(kernel_hostname)) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Perform DNS resolution */
    ip_addr_t resolved_addr;
    int result = dns_resolve(kernel_hostname, &resolved_addr);
    
    /* Convert result codes */
    switch (result) {
        case DNS_SUCCESS:
            if (dns_syscall_copy_to_user(ip_addr, &resolved_addr.addr, sizeof(uint32_t)) != 0) {
                return DNS_SYSCALL_ERROR;
            }
            return DNS_SYSCALL_SUCCESS;
        case DNS_ERROR_NXDOMAIN:
            return DNS_SYSCALL_NXDOMAIN;
        case DNS_ERROR_TIMEOUT:
            return DNS_SYSCALL_TIMEOUT;
        case DNS_ERROR_SERVFAIL:
            return DNS_SYSCALL_SERVFAIL;
        case DNS_ERROR_REFUSED:
            return DNS_SYSCALL_REFUSED;
        default:
            return DNS_SYSCALL_ERROR;
    }
}

long sys_dns_reverse(uint32_t ip_addr, char* hostname, size_t hostname_len) {
    if (!hostname || hostname_len == 0 || hostname_len > DNS_MAX_NAME_LEN) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Perform reverse DNS lookup */
    ip_addr_t addr;
    addr.addr = ip_addr;
    
    char kernel_hostname[DNS_MAX_NAME_LEN + 1];
    int result = dns_reverse_lookup(addr, kernel_hostname, sizeof(kernel_hostname));
    
    if (result == DNS_SUCCESS) {
        size_t copy_len = strlen(kernel_hostname) + 1;
        if (copy_len > hostname_len) {
            copy_len = hostname_len;
            kernel_hostname[hostname_len - 1] = '\0';
        }
        
        if (dns_syscall_copy_to_user(hostname, kernel_hostname, copy_len) != 0) {
            return DNS_SYSCALL_ERROR;
        }
        
        return DNS_SYSCALL_SUCCESS;
    }
    
    /* Convert error codes */
    switch (result) {
        case DNS_ERROR_NXDOMAIN:
            return DNS_SYSCALL_NXDOMAIN;
        case DNS_ERROR_TIMEOUT:
            return DNS_SYSCALL_TIMEOUT;
        case DNS_ERROR_SERVFAIL:
            return DNS_SYSCALL_SERVFAIL;
        default:
            return DNS_SYSCALL_ERROR;
    }
}

long sys_dns_configure(const dns_syscall_config_t* config) {
    if (!config) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Copy configuration from user space */
    dns_syscall_config_t kernel_config;
    if (dns_syscall_copy_from_user(&kernel_config, config, sizeof(kernel_config)) != 0) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Validate configuration */
    if (!is_valid_dns_config(&kernel_config)) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Convert to kernel DNS configuration */
    dns_config_t dns_config;
    dns_config.primary_server.addr = kernel_config.primary_server;
    dns_config.secondary_server.addr = kernel_config.secondary_server;
    dns_config.timeout = kernel_config.timeout_ms;
    dns_config.retries = kernel_config.max_retries;
    dns_config.cache_enabled = kernel_config.cache_enabled ? true : false;
    dns_config.cache_max_entries = kernel_config.cache_max_entries;
    dns_config.default_ttl = kernel_config.default_ttl;
    
    /* Apply configuration */
    int result = dns_configure(&dns_config);
    if (result == DNS_SUCCESS) {
        return DNS_SYSCALL_SUCCESS;
    } else {
        return DNS_SYSCALL_ERROR;
    }
}

long sys_dns_get_config(dns_syscall_config_t* config) {
    if (!config) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Get kernel DNS configuration */
    dns_config_t dns_config;
    int result = dns_get_config(&dns_config);
    if (result != DNS_SUCCESS) {
        return DNS_SYSCALL_ERROR;
    }
    
    /* Convert to syscall configuration */
    dns_syscall_config_t kernel_config;
    kernel_config.primary_server = dns_config.primary_server.addr;
    kernel_config.secondary_server = dns_config.secondary_server.addr;
    kernel_config.timeout_ms = dns_config.timeout;
    kernel_config.max_retries = dns_config.retries;
    kernel_config.cache_enabled = dns_config.cache_enabled ? 1 : 0;
    kernel_config.cache_max_entries = dns_config.cache_max_entries;
    kernel_config.default_ttl = dns_config.default_ttl;
    
    /* Copy to user space */
    if (dns_syscall_copy_to_user(config, &kernel_config, sizeof(kernel_config)) != 0) {
        return DNS_SYSCALL_ERROR;
    }
    
    return DNS_SYSCALL_SUCCESS;
}

long sys_dns_set_servers(const dns_syscall_servers_t* servers) {
    if (!servers) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Copy servers from user space */
    dns_syscall_servers_t kernel_servers;
    if (dns_syscall_copy_from_user(&kernel_servers, servers, sizeof(kernel_servers)) != 0) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Validate servers */
    if (!is_valid_dns_servers(&kernel_servers)) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Set primary and secondary servers */
    if (kernel_servers.count >= 2) {
        ip_addr_t primary, secondary;
        primary.addr = kernel_servers.servers[0];
        secondary.addr = kernel_servers.servers[1];
        
        int result = dns_set_servers(primary, secondary);
        if (result == DNS_SUCCESS) {
            return DNS_SYSCALL_SUCCESS;
        }
    }
    
    return DNS_SYSCALL_ERROR;
}

long sys_dns_get_servers(dns_syscall_servers_t* servers) {
    if (!servers) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Get DNS servers */
    ip_addr_t primary, secondary;
    int result = dns_get_servers(&primary, &secondary);
    if (result != DNS_SUCCESS) {
        return DNS_SYSCALL_ERROR;
    }
    
    /* Convert to syscall format */
    dns_syscall_servers_t kernel_servers;
    kernel_servers.servers[0] = primary.addr;
    kernel_servers.servers[1] = secondary.addr;
    kernel_servers.count = 2;
    
    /* Copy to user space */
    if (dns_syscall_copy_to_user(servers, &kernel_servers, sizeof(kernel_servers)) != 0) {
        return DNS_SYSCALL_ERROR;
    }
    
    return DNS_SYSCALL_SUCCESS;
}

long sys_dns_cache_add(const dns_syscall_cache_entry_t* entry) {
    if (!entry) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Copy cache entry from user space */
    dns_syscall_cache_entry_t kernel_entry;
    if (dns_syscall_copy_from_user(&kernel_entry, entry, sizeof(kernel_entry)) != 0) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Validate entry */
    kernel_entry.hostname[sizeof(kernel_entry.hostname) - 1] = '\0';
    if (!is_valid_dns_hostname(kernel_entry.hostname)) {
        return DNS_SYSCALL_INVALID;
    }
    
    if (kernel_entry.data_len > sizeof(kernel_entry.data)) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Add to DNS cache */
    int result = dns_cache_add(kernel_entry.hostname, kernel_entry.type, kernel_entry.class,
                              kernel_entry.ttl, kernel_entry.data, kernel_entry.data_len);
    
    switch (result) {
        case DNS_SUCCESS:
            return DNS_SYSCALL_SUCCESS;
        case DNS_ERROR_CACHE_FULL:
            return DNS_SYSCALL_CACHE_FULL;
        case DNS_ERROR_NO_MEMORY:
            return DNS_SYSCALL_NO_MEMORY;
        default:
            return DNS_SYSCALL_ERROR;
    }
}

long sys_dns_cache_lookup(const char* hostname, uint16_t type, uint16_t class,
                          dns_syscall_cache_entry_t* entry) {
    if (!hostname || !entry) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Copy hostname from user space */
    char kernel_hostname[DNS_MAX_NAME_LEN + 1];
    if (dns_syscall_copy_from_user(kernel_hostname, hostname, sizeof(kernel_hostname)) != 0) {
        return DNS_SYSCALL_INVALID;
    }
    kernel_hostname[DNS_MAX_NAME_LEN] = '\0';
    
    if (!is_valid_dns_hostname(kernel_hostname)) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Lookup in DNS cache */
    uint8_t data[256];
    uint16_t data_len = sizeof(data);
    uint32_t ttl;
    
    int result = dns_cache_lookup(kernel_hostname, type, class, data, &data_len, &ttl);
    
    if (result == DNS_SUCCESS) {
        /* Prepare cache entry */
        dns_syscall_cache_entry_t kernel_entry;
        strncpy(kernel_entry.hostname, kernel_hostname, sizeof(kernel_entry.hostname) - 1);
        kernel_entry.hostname[sizeof(kernel_entry.hostname) - 1] = '\0';
        kernel_entry.type = type;
        kernel_entry.class = class;
        kernel_entry.ttl = ttl;
        kernel_entry.data_len = data_len;
        memcpy(kernel_entry.data, data, data_len < sizeof(kernel_entry.data) ? data_len : sizeof(kernel_entry.data));
        
        /* Copy to user space */
        if (dns_syscall_copy_to_user(entry, &kernel_entry, sizeof(kernel_entry)) != 0) {
            return DNS_SYSCALL_ERROR;
        }
        
        return DNS_SYSCALL_SUCCESS;
    }
    
    switch (result) {
        case DNS_ERROR_NXDOMAIN:
            return DNS_SYSCALL_NXDOMAIN;
        case DNS_ERROR_TIMEOUT:
            return DNS_SYSCALL_TIMEOUT;
        default:
            return DNS_SYSCALL_ERROR;
    }
}

long sys_dns_cache_remove(const char* hostname, uint16_t type, uint16_t class) {
    if (!hostname) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Copy hostname from user space */
    char kernel_hostname[DNS_MAX_NAME_LEN + 1];
    if (dns_syscall_copy_from_user(kernel_hostname, hostname, sizeof(kernel_hostname)) != 0) {
        return DNS_SYSCALL_INVALID;
    }
    kernel_hostname[DNS_MAX_NAME_LEN] = '\0';
    
    if (!is_valid_dns_hostname(kernel_hostname)) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Remove from DNS cache */
    int result = dns_cache_remove(kernel_hostname, type, class);
    
    if (result == DNS_SUCCESS) {
        return DNS_SYSCALL_SUCCESS;
    } else {
        return DNS_SYSCALL_ERROR;
    }
}

long sys_dns_cache_clear(void) {
    dns_cache_clear();
    return DNS_SYSCALL_SUCCESS;
}

long sys_dns_get_stats(dns_syscall_stats_t* stats) {
    if (!stats) {
        return DNS_SYSCALL_INVALID;
    }
    
    /* Get kernel DNS statistics */
    dns_stats_t dns_stats;
    int result = dns_get_stats(&dns_stats);
    if (result != DNS_SUCCESS) {
        return DNS_SYSCALL_ERROR;
    }
    
    /* Convert to syscall format */
    dns_syscall_stats_t kernel_stats;
    kernel_stats.queries_sent = dns_stats.queries_sent;
    kernel_stats.responses_received = dns_stats.responses_received;
    kernel_stats.cache_hits = dns_stats.cache_hits;
    kernel_stats.cache_misses = dns_stats.cache_misses;
    kernel_stats.timeouts = dns_stats.timeouts;
    kernel_stats.errors = dns_stats.errors;
    kernel_stats.nxdomain = dns_stats.nxdomain;
    kernel_stats.servfail = dns_stats.servfail;
    
    /* Get cache statistics */
    uint32_t cache_entries, memory_used;
    dns_cache_get_stats(&cache_entries, &memory_used);
    kernel_stats.cache_entries = cache_entries;
    kernel_stats.memory_used = memory_used;
    
    /* Copy to user space */
    if (dns_syscall_copy_to_user(stats, &kernel_stats, sizeof(kernel_stats)) != 0) {
        return DNS_SYSCALL_ERROR;
    }
    
    return DNS_SYSCALL_SUCCESS;
}

long sys_dns_reset_stats(void) {
    dns_reset_stats();
    return DNS_SYSCALL_SUCCESS;
}

/* ================================
 * DNS Syscall Validation Functions
 * ================================ */

bool is_valid_dns_hostname(const char* hostname) {
    if (!hostname) {
        return false;
    }
    
    size_t len = strlen(hostname);
    if (len == 0 || len > DNS_MAX_NAME_LEN) {
        return false;
    }
    
    /* Use kernel DNS validation */
    return dns_is_valid_hostname(hostname);
}

bool is_valid_dns_config(const dns_syscall_config_t* config) {
    if (!config) {
        return false;
    }
    
    /* Validate timeout */
    if (config->timeout_ms == 0 || config->timeout_ms > 60000) {
        return false;
    }
    
    /* Validate retries */
    if (config->max_retries == 0 || config->max_retries > 10) {
        return false;
    }
    
    /* Validate cache settings */
    if (config->cache_max_entries > 1000) {
        return false;
    }
    
    /* Validate TTL */
    if (config->default_ttl == 0 || config->default_ttl > 86400) { /* Max 1 day */
        return false;
    }
    
    return true;
}

bool is_valid_dns_servers(const dns_syscall_servers_t* servers) {
    if (!servers) {
        return false;
    }
    
    if (servers->count == 0 || servers->count > 8) {
        return false;
    }
    
    /* Validate server addresses (basic check for non-zero) */
    for (uint32_t i = 0; i < servers->count; i++) {
        if (servers->servers[i] == 0) {
            return false;
        }
    }
    
    return true;
}

/* ================================
 * DNS Syscall Helper Functions
 * ================================ */

int dns_syscall_copy_from_user(void* dest, const void* src, size_t size) {
    /* Simplified copy function - in real implementation would validate user memory */
    if (!dest || !src || size == 0) {
        return -1;
    }
    
    memcpy(dest, src, size);
    return 0;
}

int dns_syscall_copy_to_user(void* dest, const void* src, size_t size) {
    /* Simplified copy function - in real implementation would validate user memory */
    if (!dest || !src || size == 0) {
        return -1;
    }
    
    memcpy(dest, src, size);
    return 0;
}

int dns_syscall_validate_string(const char* str, size_t max_len) {
    if (!str) {
        return -1;
    }
    
    /* Check string length without causing page faults */
    for (size_t i = 0; i < max_len; i++) {
        if (str[i] == '\0') {
            return i;
        }
    }
    
    return -1; /* String too long or not null-terminated */
}
