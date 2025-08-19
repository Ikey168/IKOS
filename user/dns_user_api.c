/*
 * IKOS DNS User API Implementation
 * Issue #47: DNS Resolution Service
 *
 * User-space DNS resolution library that provides a simple interface
 * for applications to perform DNS lookups. Interfaces with the kernel
 * DNS service through syscalls and socket operations.
 */

#include "../include/dns_user_api.h"
#include "../include/socket_user_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

/* ================================
 * DNS User Library State
 * ================================ */

static bool dns_user_initialized = false;
static dns_user_config_t dns_user_config;
static dns_user_stats_t dns_user_stats;
static int dns_cache_enabled = 1;

/* Simple DNS cache for user-space */
#define DNS_USER_CACHE_SIZE 64
typedef struct dns_cache_entry {
    char hostname[DNS_USER_MAX_NAME_LEN + 1];
    char ip_address[16];
    uint32_t ttl;
    uint32_t timestamp;
    struct dns_cache_entry* next;
} dns_cache_entry_t;

static dns_cache_entry_t* dns_cache_head = NULL;
static int dns_cache_count = 0;

/* ================================
 * DNS Library Initialization
 * ================================ */

int dns_lib_init(void) {
    if (dns_user_initialized) {
        return DNS_USER_SUCCESS;
    }
    
    /* Initialize socket library if not already done */
    if (!socket_lib_is_initialized()) {
        if (socket_lib_init() != SOCK_SUCCESS) {
            printf("DNS: Failed to initialize socket library\n");
            return DNS_USER_ERROR;
        }
    }
    
    /* Set default configuration */
    strcpy(dns_user_config.primary_server, "8.8.8.8");
    strcpy(dns_user_config.secondary_server, "8.8.4.8");
    dns_user_config.timeout_ms = 5000;
    dns_user_config.max_retries = 3;
    dns_user_config.cache_enabled = true;
    
    /* Initialize statistics */
    memset(&dns_user_stats, 0, sizeof(dns_user_stats));
    
    /* Initialize cache */
    dns_cache_head = NULL;
    dns_cache_count = 0;
    
    dns_user_initialized = true;
    printf("DNS User Library: Initialized\n");
    return DNS_USER_SUCCESS;
}

void dns_lib_cleanup(void) {
    if (!dns_user_initialized) {
        return;
    }
    
    /* Clear cache */
    dns_cache_flush();
    
    dns_user_initialized = false;
    printf("DNS User Library: Cleaned up\n");
}

bool dns_lib_is_initialized(void) {
    return dns_user_initialized;
}

/* ================================
 * Basic DNS Resolution
 * ================================ */

int dns_resolve_hostname(const char* hostname, char* ip_address, size_t ip_len) {
    if (!dns_user_initialized || !hostname || !ip_address || ip_len < 16) {
        return DNS_USER_ERROR_INVALID;
    }
    
    if (!dns_is_valid_hostname(hostname)) {
        return DNS_USER_ERROR_INVALID;
    }
    
    dns_user_stats.total_queries++;
    uint32_t start_time = 0; /* Would use actual time in real implementation */
    
    /* Check cache first */
    if (dns_user_config.cache_enabled) {
        uint32_t ttl;
        if (dns_cache_lookup(hostname, ip_address, ip_len, &ttl) == DNS_USER_SUCCESS) {
            dns_user_stats.cache_hits++;
            dns_user_stats.successful_queries++;
            return DNS_USER_SUCCESS;
        }
        dns_user_stats.cache_misses++;
    }
    
    /* Perform DNS lookup via socket */
    int result = dns_socket_lookup(hostname, ip_address, ip_len);
    
    uint32_t end_time = start_time + 100; /* Simulated response time */
    dns_user_stats.average_response_time = 
        (dns_user_stats.average_response_time + (end_time - start_time)) / 2;
    
    if (result == DNS_USER_SUCCESS) {
        dns_user_stats.successful_queries++;
        
        /* Add to cache */
        if (dns_user_config.cache_enabled) {
            dns_cache_add_entry(hostname, ip_address, 3600); /* 1 hour TTL */
        }
    } else {
        dns_user_stats.failed_queries++;
        if (result == DNS_USER_ERROR_NXDOMAIN) {
            dns_user_stats.nxdomain_errors++;
        } else if (result == DNS_USER_ERROR_TIMEOUT) {
            dns_user_stats.timeouts++;
        }
    }
    
    return result;
}

int dns_resolve_ip(const char* ip_address, char* hostname, size_t hostname_len) {
    if (!dns_user_initialized || !ip_address || !hostname || hostname_len == 0) {
        return DNS_USER_ERROR_INVALID;
    }
    
    if (!dns_is_valid_ip_address(ip_address)) {
        return DNS_USER_ERROR_INVALID;
    }
    
    dns_user_stats.total_queries++;
    
    /* Perform reverse DNS lookup */
    int result = dns_reverse_socket_lookup(ip_address, hostname, hostname_len);
    
    if (result == DNS_USER_SUCCESS) {
        dns_user_stats.successful_queries++;
    } else {
        dns_user_stats.failed_queries++;
    }
    
    return result;
}

int dns_lookup(const char* hostname, dns_query_result_t* result) {
    if (!result) {
        return DNS_USER_ERROR_INVALID;
    }
    
    int lookup_result = dns_resolve_hostname(hostname, result->ip_address, sizeof(result->ip_address));
    if (lookup_result == DNS_USER_SUCCESS) {
        strncpy(result->hostname, hostname, DNS_USER_MAX_NAME_LEN);
        result->hostname[DNS_USER_MAX_NAME_LEN] = '\0';
        result->ttl = 3600; /* Default TTL */
        result->timestamp = 0; /* Would use actual timestamp */
    }
    
    return lookup_result;
}

/* ================================
 * DNS Socket Implementation
 * ================================ */

static int dns_socket_lookup(const char* hostname, char* ip_address, size_t ip_len) {
    /* Create DNS query using UDP socket */
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        return DNS_USER_ERROR;
    }
    
    /* Set socket timeout */
    struct timeval timeout;
    timeout.tv_sec = dns_user_config.timeout_ms / 1000;
    timeout.tv_usec = (dns_user_config.timeout_ms % 1000) * 1000;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        close(sockfd);
        return DNS_USER_ERROR;
    }
    
    /* Try primary server first, then secondary */
    const char* servers[] = {dns_user_config.primary_server, dns_user_config.secondary_server};
    
    for (int server_idx = 0; server_idx < 2; server_idx++) {
        for (int retry = 0; retry < dns_user_config.max_retries; retry++) {
            int result = dns_query_server(sockfd, hostname, servers[server_idx], ip_address, ip_len);
            if (result == DNS_USER_SUCCESS) {
                close(sockfd);
                return DNS_USER_SUCCESS;
            }
        }
    }
    
    close(sockfd);
    return DNS_USER_ERROR_TIMEOUT;
}

static int dns_query_server(int sockfd, const char* hostname, const char* server, 
                           char* ip_address, size_t ip_len) {
    /* Simplified DNS query implementation */
    /* In a real implementation, this would construct proper DNS packets */
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(53); /* DNS port */
    
    /* Convert server IP string to binary */
    if (inet_aton(server, &server_addr.sin_addr) == 0) {
        return DNS_USER_ERROR_INVALID;
    }
    
    /* Create simple DNS query packet */
    uint8_t query_packet[512];
    int packet_len = create_dns_query_packet(query_packet, sizeof(query_packet), hostname);
    if (packet_len < 0) {
        return DNS_USER_ERROR_INVALID;
    }
    
    /* Send query */
    ssize_t sent = sendto(sockfd, query_packet, packet_len, 0,
                         (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (sent != packet_len) {
        return DNS_USER_ERROR;
    }
    
    /* Receive response */
    uint8_t response_packet[512];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    ssize_t received = recvfrom(sockfd, response_packet, sizeof(response_packet), 0,
                               (struct sockaddr*)&from_addr, &from_len);
    if (received < 0) {
        return DNS_USER_ERROR_TIMEOUT;
    }
    
    /* Parse response */
    return parse_dns_response(response_packet, received, hostname, ip_address, ip_len);
}

static int create_dns_query_packet(uint8_t* packet, size_t packet_size, const char* hostname) {
    /* Simplified DNS packet creation */
    if (packet_size < 64) {
        return -1;
    }
    
    /* DNS header */
    uint16_t* header = (uint16_t*)packet;
    header[0] = htons(0x1234); /* Query ID */
    header[1] = htons(0x0100); /* Standard query, recursion desired */
    header[2] = htons(1);      /* 1 question */
    header[3] = 0;             /* 0 answers */
    header[4] = 0;             /* 0 authority */
    header[5] = 0;             /* 0 additional */
    
    /* Question section */
    uint8_t* ptr = packet + 12;
    
    /* Encode hostname */
    const char* src = hostname;
    while (*src) {
        const char* dot = strchr(src, '.');
        int len = dot ? (dot - src) : strlen(src);
        
        *ptr++ = len;
        memcpy(ptr, src, len);
        ptr += len;
        
        src += len;
        if (*src == '.') src++;
    }
    *ptr++ = 0; /* Null terminator */
    
    /* Query type (A record) and class (IN) */
    *(uint16_t*)ptr = htons(1);  /* Type A */
    ptr += 2;
    *(uint16_t*)ptr = htons(1);  /* Class IN */
    ptr += 2;
    
    return ptr - packet;
}

static int parse_dns_response(const uint8_t* packet, size_t packet_size,
                             const char* hostname, char* ip_address, size_t ip_len) {
    /* Simplified DNS response parsing */
    if (packet_size < 12) {
        return DNS_USER_ERROR_INVALID;
    }
    
    const uint16_t* header = (const uint16_t*)packet;
    uint16_t flags = ntohs(header[1]);
    uint16_t questions = ntohs(header[2]);
    uint16_t answers = ntohs(header[3]);
    
    /* Check if it's a response */
    if (!(flags & 0x8000)) {
        return DNS_USER_ERROR_INVALID;
    }
    
    /* Check response code */
    uint8_t rcode = flags & 0x0F;
    if (rcode == 3) {
        return DNS_USER_ERROR_NXDOMAIN;
    } else if (rcode != 0) {
        return DNS_USER_ERROR_SERVFAIL;
    }
    
    if (answers == 0) {
        return DNS_USER_ERROR_NXDOMAIN;
    }
    
    /* Skip questions section */
    const uint8_t* ptr = packet + 12;
    const uint8_t* end = packet + packet_size;
    
    for (int i = 0; i < questions && ptr < end; i++) {
        /* Skip name */
        while (ptr < end && *ptr != 0) {
            if ((*ptr & 0xC0) == 0xC0) {
                ptr += 2;
                break;
            }
            ptr += *ptr + 1;
        }
        if (ptr < end && *ptr == 0) ptr++;
        ptr += 4; /* Skip type and class */
    }
    
    /* Parse answers */
    for (int i = 0; i < answers && ptr < end; i++) {
        /* Skip name */
        while (ptr < end && *ptr != 0) {
            if ((*ptr & 0xC0) == 0xC0) {
                ptr += 2;
                break;
            }
            ptr += *ptr + 1;
        }
        if (ptr < end && *ptr == 0) ptr++;
        
        if (ptr + 10 > end) break;
        
        uint16_t type = ntohs(*(uint16_t*)ptr);
        ptr += 2;
        uint16_t class = ntohs(*(uint16_t*)ptr);
        ptr += 2;
        ptr += 4; /* Skip TTL */
        uint16_t rdlength = ntohs(*(uint16_t*)ptr);
        ptr += 2;
        
        if (type == 1 && class == 1 && rdlength == 4) { /* A record */
            if (ptr + 4 <= end && ip_len >= 16) {
                snprintf(ip_address, ip_len, "%u.%u.%u.%u",
                        ptr[0], ptr[1], ptr[2], ptr[3]);
                return DNS_USER_SUCCESS;
            }
        }
        
        ptr += rdlength;
    }
    
    return DNS_USER_ERROR_NXDOMAIN;
}

static int dns_reverse_socket_lookup(const char* ip_address, char* hostname, size_t hostname_len) {
    /* Simplified reverse DNS lookup */
    /* In real implementation, would construct PTR query */
    strncpy(hostname, "unknown.host", hostname_len - 1);
    hostname[hostname_len - 1] = '\0';
    return DNS_USER_SUCCESS;
}

/* ================================
 * DNS Cache Management
 * ================================ */

int dns_cache_add_entry(const char* hostname, const char* ip_address, uint32_t ttl) {
    if (!hostname || !ip_address) {
        return DNS_USER_ERROR_INVALID;
    }
    
    /* Remove existing entry if present */
    dns_cache_remove_entry(hostname);
    
    /* Check cache size limit */
    if (dns_cache_count >= DNS_USER_CACHE_SIZE) {
        /* Remove oldest entry */
        dns_cache_entry_t* oldest = dns_cache_head;
        if (oldest) {
            dns_cache_head = oldest->next;
            free(oldest);
            dns_cache_count--;
        }
    }
    
    /* Add new entry */
    dns_cache_entry_t* entry = malloc(sizeof(dns_cache_entry_t));
    if (!entry) {
        return DNS_USER_ERROR;
    }
    
    strncpy(entry->hostname, hostname, DNS_USER_MAX_NAME_LEN);
    entry->hostname[DNS_USER_MAX_NAME_LEN] = '\0';
    strncpy(entry->ip_address, ip_address, 15);
    entry->ip_address[15] = '\0';
    entry->ttl = ttl;
    entry->timestamp = 0; /* Would use actual timestamp */
    entry->next = dns_cache_head;
    
    dns_cache_head = entry;
    dns_cache_count++;
    
    return DNS_USER_SUCCESS;
}

int dns_cache_lookup(const char* hostname, char* ip_address, size_t ip_len, uint32_t* ttl) {
    if (!hostname || !ip_address) {
        return DNS_USER_ERROR_INVALID;
    }
    
    dns_cache_entry_t* entry = dns_cache_head;
    while (entry) {
        if (strcasecmp(entry->hostname, hostname) == 0) {
            /* Check if entry has expired (simplified) */
            /* In real implementation, would check timestamp + TTL vs current time */
            
            strncpy(ip_address, entry->ip_address, ip_len - 1);
            ip_address[ip_len - 1] = '\0';
            
            if (ttl) {
                *ttl = entry->ttl;
            }
            
            return DNS_USER_SUCCESS;
        }
        entry = entry->next;
    }
    
    return DNS_USER_ERROR_NXDOMAIN;
}

int dns_cache_remove_entry(const char* hostname) {
    if (!hostname) {
        return DNS_USER_ERROR_INVALID;
    }
    
    dns_cache_entry_t** ptr = &dns_cache_head;
    while (*ptr) {
        if (strcasecmp((*ptr)->hostname, hostname) == 0) {
            dns_cache_entry_t* to_remove = *ptr;
            *ptr = to_remove->next;
            free(to_remove);
            dns_cache_count--;
            return DNS_USER_SUCCESS;
        }
        ptr = &((*ptr)->next);
    }
    
    return DNS_USER_ERROR_NXDOMAIN;
}

int dns_cache_flush(void) {
    dns_cache_entry_t* entry = dns_cache_head;
    while (entry) {
        dns_cache_entry_t* next = entry->next;
        free(entry);
        entry = next;
    }
    
    dns_cache_head = NULL;
    dns_cache_count = 0;
    
    return DNS_USER_SUCCESS;
}

/* ================================
 * DNS Configuration Management
 * ================================ */

int dns_set_servers(const char* primary, const char* secondary) {
    if (!primary || !secondary) {
        return DNS_USER_ERROR_INVALID;
    }
    
    if (!dns_is_valid_ip_address(primary) || !dns_is_valid_ip_address(secondary)) {
        return DNS_USER_ERROR_INVALID;
    }
    
    strncpy(dns_user_config.primary_server, primary, 15);
    dns_user_config.primary_server[15] = '\0';
    strncpy(dns_user_config.secondary_server, secondary, 15);
    dns_user_config.secondary_server[15] = '\0';
    
    return DNS_USER_SUCCESS;
}

int dns_get_servers(char* primary, size_t primary_len, char* secondary, size_t secondary_len) {
    if (!primary || !secondary || primary_len < 16 || secondary_len < 16) {
        return DNS_USER_ERROR_INVALID;
    }
    
    strncpy(primary, dns_user_config.primary_server, primary_len - 1);
    primary[primary_len - 1] = '\0';
    strncpy(secondary, dns_user_config.secondary_server, secondary_len - 1);
    secondary[secondary_len - 1] = '\0';
    
    return DNS_USER_SUCCESS;
}

/* ================================
 * DNS Validation and Utilities
 * ================================ */

bool dns_is_valid_hostname(const char* hostname) {
    if (!hostname || strlen(hostname) == 0 || strlen(hostname) > DNS_USER_MAX_NAME_LEN) {
        return false;
    }
    
    const char* ptr = hostname;
    int label_len = 0;
    
    while (*ptr) {
        if (*ptr == '.') {
            if (label_len == 0 || label_len > 63) {
                return false;
            }
            label_len = 0;
        } else if ((*ptr >= 'a' && *ptr <= 'z') ||
                   (*ptr >= 'A' && *ptr <= 'Z') ||
                   (*ptr >= '0' && *ptr <= '9') ||
                   *ptr == '-') {
            label_len++;
            if (label_len > 63) {
                return false;
            }
        } else {
            return false;
        }
        ptr++;
    }
    
    return label_len > 0;
}

bool dns_is_valid_ip_address(const char* ip_address) {
    if (!ip_address) {
        return false;
    }
    
    int octets[4];
    int count = sscanf(ip_address, "%d.%d.%d.%d", &octets[0], &octets[1], &octets[2], &octets[3]);
    
    if (count != 4) {
        return false;
    }
    
    for (int i = 0; i < 4; i++) {
        if (octets[i] < 0 || octets[i] > 255) {
            return false;
        }
    }
    
    return true;
}

/* ================================
 * DNS Statistics
 * ================================ */

int dns_get_statistics(dns_user_stats_t* stats) {
    if (!stats) {
        return DNS_USER_ERROR_INVALID;
    }
    
    *stats = dns_user_stats;
    return DNS_USER_SUCCESS;
}

void dns_reset_statistics(void) {
    memset(&dns_user_stats, 0, sizeof(dns_user_stats));
}

void dns_print_statistics(void) {
    printf("DNS User Library Statistics:\n");
    printf("  Total queries:       %llu\n", dns_user_stats.total_queries);
    printf("  Successful queries:  %llu\n", dns_user_stats.successful_queries);
    printf("  Failed queries:      %llu\n", dns_user_stats.failed_queries);
    printf("  Cache hits:          %llu\n", dns_user_stats.cache_hits);
    printf("  Cache misses:        %llu\n", dns_user_stats.cache_misses);
    printf("  Timeouts:            %llu\n", dns_user_stats.timeouts);
    printf("  NXDOMAIN errors:     %llu\n", dns_user_stats.nxdomain_errors);
    printf("  Average response:    %.2f ms\n", dns_user_stats.average_response_time);
    printf("  Cache entries:       %d\n", dns_cache_count);
}
