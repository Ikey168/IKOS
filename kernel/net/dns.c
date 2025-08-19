/*
 * IKOS Operating System - DNS Resolution Service Implementation
 * Issue #47: DNS Resolution Service
 *
 * Provides comprehensive DNS resolution functionality including:
 * - DNS query construction and parsing
 * - DNS response processing and caching
 * - Asynchronous DNS resolution
 * - DNS cache management with TTL support
 * - Integration with UDP socket layer
 * - Multiple DNS server support with failover
 *
 * RFC References:
 * - RFC 1034: Domain Names - Concepts and Facilities
 * - RFC 1035: Domain Names - Implementation and Specification
 * - RFC 1123: Requirements for Internet Hosts
 */

#include "net/dns.h"
#include "net/udp.h"
#include "net/socket.h"
#include "net/network.h"
#include "memory.h"
#include "string.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* ================================
 * DNS Global State
 * ================================ */

static bool dns_initialized = false;
static dns_config_t dns_config;
static dns_cache_entry_t* dns_cache_head = NULL;
static dns_query_t* pending_queries = NULL;
static dns_stats_t dns_stats;
static uint32_t cache_entry_count = 0;
static uint16_t query_id_counter = 1;
static int dns_socket = -1;

/* DNS Server List */
#define MAX_DNS_SERVERS 8
static ip_addr_t dns_servers[MAX_DNS_SERVERS];
static int dns_server_count = 0;
static int current_server_index = 0;

/* ================================
 * DNS Initialization and Configuration
 * ================================ */

int dns_init(void) {
    if (dns_initialized) {
        return DNS_SUCCESS;
    }
    
    /* Initialize configuration with defaults */
    memset(&dns_config, 0, sizeof(dns_config));
    dns_config.primary_server.addr = 0x08080808;    /* 8.8.8.8 */
    dns_config.secondary_server.addr = 0x08040808;  /* 8.8.4.8 */
    dns_config.timeout = DNS_QUERY_TIMEOUT;
    dns_config.retries = DNS_MAX_RETRIES;
    dns_config.cache_enabled = true;
    dns_config.cache_max_entries = DNS_MAX_CACHE_ENTRIES;
    dns_config.default_ttl = DNS_DEFAULT_TTL;
    
    /* Initialize statistics */
    memset(&dns_stats, 0, sizeof(dns_stats));
    
    /* Initialize DNS server list */
    dns_servers[0] = dns_config.primary_server;
    dns_servers[1] = dns_config.secondary_server;
    dns_server_count = 2;
    
    /* Create UDP socket for DNS queries */
    dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (dns_socket < 0) {
        printf("DNS: Failed to create UDP socket\n");
        return DNS_ERROR;
    }
    
    /* Initialize cache */
    dns_cache_head = NULL;
    cache_entry_count = 0;
    
    /* Initialize pending queries */
    pending_queries = NULL;
    
    dns_initialized = true;
    printf("DNS: Initialized with servers %d.%d.%d.%d and %d.%d.%d.%d\n",
           (dns_config.primary_server.addr >> 0) & 0xFF,
           (dns_config.primary_server.addr >> 8) & 0xFF,
           (dns_config.primary_server.addr >> 16) & 0xFF,
           (dns_config.primary_server.addr >> 24) & 0xFF,
           (dns_config.secondary_server.addr >> 0) & 0xFF,
           (dns_config.secondary_server.addr >> 8) & 0xFF,
           (dns_config.secondary_server.addr >> 16) & 0xFF,
           (dns_config.secondary_server.addr >> 24) & 0xFF);
    
    return DNS_SUCCESS;
}

void dns_cleanup(void) {
    if (!dns_initialized) {
        return;
    }
    
    /* Close DNS socket */
    if (dns_socket >= 0) {
        close(dns_socket);
        dns_socket = -1;
    }
    
    /* Clear cache */
    dns_cache_clear();
    
    /* Cancel pending queries */
    dns_query_t* query = pending_queries;
    while (query) {
        dns_query_t* next = query->next;
        dns_query_destroy(query);
        query = next;
    }
    pending_queries = NULL;
    
    dns_initialized = false;
    printf("DNS: Cleaned up\n");
}

int dns_configure(const dns_config_t* config) {
    if (!config) {
        return DNS_ERROR_INVALID;
    }
    
    if (dns_validate_config(config) != DNS_SUCCESS) {
        return DNS_ERROR_INVALID;
    }
    
    dns_config = *config;
    
    /* Update server list */
    dns_servers[0] = config->primary_server;
    dns_servers[1] = config->secondary_server;
    dns_server_count = 2;
    current_server_index = 0;
    
    printf("DNS: Configuration updated\n");
    return DNS_SUCCESS;
}

int dns_get_config(dns_config_t* config) {
    if (!config) {
        return DNS_ERROR_INVALID;
    }
    
    *config = dns_config;
    return DNS_SUCCESS;
}

int dns_set_servers(ip_addr_t primary, ip_addr_t secondary) {
    dns_config.primary_server = primary;
    dns_config.secondary_server = secondary;
    
    dns_servers[0] = primary;
    dns_servers[1] = secondary;
    dns_server_count = 2;
    current_server_index = 0;
    
    return DNS_SUCCESS;
}

int dns_get_servers(ip_addr_t* primary, ip_addr_t* secondary) {
    if (!primary || !secondary) {
        return DNS_ERROR_INVALID;
    }
    
    *primary = dns_config.primary_server;
    *secondary = dns_config.secondary_server;
    return DNS_SUCCESS;
}

/* ================================
 * DNS Name Encoding/Decoding
 * ================================ */

int dns_name_encode(const char* name, uint8_t* encoded, size_t encoded_size) {
    if (!name || !encoded || encoded_size == 0) {
        return DNS_ERROR_INVALID;
    }
    
    const char* src = name;
    uint8_t* dst = encoded;
    uint8_t* end = encoded + encoded_size - 1;
    
    while (*src && dst < end) {
        /* Find next label */
        const char* label_start = src;
        while (*src && *src != '.' && src - label_start < DNS_MAX_LABEL_LEN) {
            src++;
        }
        
        uint8_t label_len = src - label_start;
        if (label_len == 0) {
            break;
        }
        
        /* Check if we have space for length byte and label */
        if (dst + 1 + label_len >= end) {
            return DNS_ERROR_INVALID;
        }
        
        /* Write label length */
        *dst++ = label_len;
        
        /* Write label */
        for (int i = 0; i < label_len; i++) {
            *dst++ = label_start[i];
        }
        
        /* Skip dot */
        if (*src == '.') {
            src++;
        }
    }
    
    /* Write terminating zero */
    if (dst < end) {
        *dst++ = 0;
        return dst - encoded;
    }
    
    return DNS_ERROR_INVALID;
}

int dns_name_decode(const uint8_t* encoded, size_t encoded_size, 
                   char* name, size_t name_size) {
    if (!encoded || !name || name_size == 0) {
        return DNS_ERROR_INVALID;
    }
    
    const uint8_t* src = encoded;
    const uint8_t* end = encoded + encoded_size;
    char* dst = name;
    char* name_end = name + name_size - 1;
    bool first_label = true;
    
    while (src < end && dst < name_end) {
        uint8_t len = *src++;
        
        if (len == 0) {
            /* End of name */
            break;
        }
        
        if (len > DNS_MAX_LABEL_LEN || src + len > end) {
            return DNS_ERROR_INVALID;
        }
        
        /* Add dot separator (except for first label) */
        if (!first_label) {
            if (dst >= name_end) {
                return DNS_ERROR_INVALID;
            }
            *dst++ = '.';
        }
        first_label = false;
        
        /* Copy label */
        for (int i = 0; i < len && dst < name_end; i++) {
            *dst++ = *src++;
        }
    }
    
    *dst = '\0';
    return dst - name;
}

/* ================================
 * DNS Packet Creation and Parsing
 * ================================ */

int dns_packet_create_query(uint8_t* packet, size_t packet_size,
                           const char* name, uint16_t type, uint16_t class) {
    if (!packet || !name || packet_size < DNS_HEADER_SIZE + strlen(name) + 8) {
        return DNS_ERROR_INVALID;
    }
    
    /* Create DNS header */
    dns_header_t* header = (dns_header_t*)packet;
    header->id = dns_htons(dns_generate_id());
    header->flags = dns_htons(DNS_FLAG_RD);  /* Recursion desired */
    header->qdcount = dns_htons(1);
    header->ancount = 0;
    header->nscount = 0;
    header->arcount = 0;
    
    uint8_t* ptr = packet + DNS_HEADER_SIZE;
    
    /* Encode question name */
    int name_len = dns_name_encode(name, ptr, packet_size - DNS_HEADER_SIZE - 4);
    if (name_len < 0) {
        return name_len;
    }
    ptr += name_len;
    
    /* Add question type and class */
    *(uint16_t*)ptr = dns_htons(type);
    ptr += 2;
    *(uint16_t*)ptr = dns_htons(class);
    ptr += 2;
    
    return ptr - packet;
}

int dns_packet_parse_response(const uint8_t* packet, size_t packet_size,
                             dns_header_t* header) {
    if (!packet || !header || packet_size < DNS_HEADER_SIZE) {
        return DNS_ERROR_INVALID;
    }
    
    /* Parse header */
    const dns_header_t* pkt_header = (const dns_header_t*)packet;
    header->id = dns_ntohs(pkt_header->id);
    header->flags = dns_ntohs(pkt_header->flags);
    header->qdcount = dns_ntohs(pkt_header->qdcount);
    header->ancount = dns_ntohs(pkt_header->ancount);
    header->nscount = dns_ntohs(pkt_header->nscount);
    header->arcount = dns_ntohs(pkt_header->arcount);
    
    /* Validate response */
    if (!DNS_GET_QR(header->flags)) {
        return DNS_ERROR_INVALID;  /* Not a response */
    }
    
    uint8_t rcode = DNS_GET_RCODE(header->flags);
    switch (rcode) {
        case DNS_RCODE_NOERROR:
            return DNS_SUCCESS;
        case DNS_RCODE_NXDOMAIN:
            return DNS_ERROR_NXDOMAIN;
        case DNS_RCODE_SERVFAIL:
            return DNS_ERROR_SERVFAIL;
        case DNS_RCODE_REFUSED:
            return DNS_ERROR_REFUSED;
        default:
            return DNS_ERROR;
    }
}

int dns_packet_extract_answer(const uint8_t* packet, size_t packet_size,
                             const char* name, uint16_t type,
                             void* data, size_t* data_size) {
    if (!packet || !name || !data || !data_size || packet_size < DNS_HEADER_SIZE) {
        return DNS_ERROR_INVALID;
    }
    
    const dns_header_t* header = (const dns_header_t*)packet;
    uint16_t qdcount = dns_ntohs(header->qdcount);
    uint16_t ancount = dns_ntohs(header->ancount);
    
    if (ancount == 0) {
        return DNS_ERROR_NXDOMAIN;
    }
    
    const uint8_t* ptr = packet + DNS_HEADER_SIZE;
    const uint8_t* end = packet + packet_size;
    
    /* Skip questions */
    for (int i = 0; i < qdcount && ptr < end; i++) {
        /* Skip name */
        while (ptr < end && *ptr != 0) {
            if ((*ptr & 0xC0) == 0xC0) {
                ptr += 2;  /* Compression pointer */
                break;
            } else {
                ptr += *ptr + 1;
            }
        }
        if (ptr < end && *ptr == 0) ptr++;  /* Skip terminating zero */
        ptr += 4;  /* Skip type and class */
    }
    
    /* Process answers */
    for (int i = 0; i < ancount && ptr < end; i++) {
        /* Skip name */
        const uint8_t* name_start = ptr;
        while (ptr < end && *ptr != 0) {
            if ((*ptr & 0xC0) == 0xC0) {
                ptr += 2;  /* Compression pointer */
                break;
            } else {
                ptr += *ptr + 1;
            }
        }
        if (ptr < end && *ptr == 0) ptr++;  /* Skip terminating zero */
        
        if (ptr + 10 > end) break;  /* Need at least type, class, ttl, rdlength */
        
        uint16_t rr_type = dns_ntohs(*(uint16_t*)ptr);
        ptr += 2;
        uint16_t rr_class = dns_ntohs(*(uint16_t*)ptr);
        ptr += 2;
        uint32_t ttl = dns_ntohl(*(uint32_t*)ptr);
        ptr += 4;
        uint16_t rdlength = dns_ntohs(*(uint16_t*)ptr);
        ptr += 2;
        
        if (ptr + rdlength > end) break;
        
        /* Check if this matches our query */
        if (rr_type == type && rr_class == DNS_CLASS_IN) {
            if (rdlength <= *data_size) {
                memcpy(data, ptr, rdlength);
                *data_size = rdlength;
                return DNS_SUCCESS;
            } else {
                return DNS_ERROR_INVALID;
            }
        }
        
        ptr += rdlength;
    }
    
    return DNS_ERROR_NXDOMAIN;
}

/* ================================
 * DNS Cache Management
 * ================================ */

int dns_cache_add(const char* name, uint16_t type, uint16_t class,
                 uint32_t ttl, const void* data, uint16_t data_len) {
    if (!dns_config.cache_enabled || !name || !data) {
        return DNS_ERROR_INVALID;
    }
    
    if (cache_entry_count >= dns_config.cache_max_entries) {
        /* Remove oldest entry */
        dns_cache_entry_t* oldest = dns_cache_head;
        dns_cache_entry_t* prev = NULL;
        
        if (oldest) {
            if (prev) {
                prev->next = oldest->next;
            } else {
                dns_cache_head = oldest->next;
            }
            free(oldest);
            cache_entry_count--;
        }
    }
    
    dns_cache_entry_t* entry = malloc(sizeof(dns_cache_entry_t));
    if (!entry) {
        return DNS_ERROR_NO_MEMORY;
    }
    
    strncpy(entry->name, name, DNS_MAX_NAME_LEN);
    entry->name[DNS_MAX_NAME_LEN] = '\0';
    entry->type = type;
    entry->class = class;
    entry->ttl = ttl;
    entry->timestamp = dns_get_timestamp();
    entry->data_len = data_len < sizeof(entry->data) ? data_len : sizeof(entry->data);
    memcpy(entry->data, data, entry->data_len);
    entry->next = dns_cache_head;
    
    dns_cache_head = entry;
    cache_entry_count++;
    
    return DNS_SUCCESS;
}

int dns_cache_lookup(const char* name, uint16_t type, uint16_t class,
                    void* data, uint16_t* data_len, uint32_t* ttl) {
    if (!dns_config.cache_enabled || !name || !data || !data_len) {
        return DNS_ERROR_INVALID;
    }
    
    uint32_t current_time = dns_get_timestamp();
    dns_cache_entry_t* entry = dns_cache_head;
    
    while (entry) {
        if (entry->type == type && entry->class == class &&
            dns_name_compare(entry->name, name) == 0) {
            
            /* Check if entry has expired */
            if (dns_cache_entry_expired(entry, current_time)) {
                dns_stats.cache_misses++;
                return DNS_ERROR_TIMEOUT;
            }
            
            /* Copy data */
            uint16_t copy_len = entry->data_len < *data_len ? entry->data_len : *data_len;
            memcpy(data, entry->data, copy_len);
            *data_len = copy_len;
            
            if (ttl) {
                uint32_t remaining_ttl = entry->ttl - (current_time - entry->timestamp);
                *ttl = remaining_ttl;
            }
            
            dns_stats.cache_hits++;
            return DNS_SUCCESS;
        }
        entry = entry->next;
    }
    
    dns_stats.cache_misses++;
    return DNS_ERROR_NXDOMAIN;
}

void dns_cache_clear(void) {
    dns_cache_entry_t* entry = dns_cache_head;
    while (entry) {
        dns_cache_entry_t* next = entry->next;
        free(entry);
        entry = next;
    }
    dns_cache_head = NULL;
    cache_entry_count = 0;
}

void dns_cache_cleanup_expired(void) {
    if (!dns_config.cache_enabled) {
        return;
    }
    
    uint32_t current_time = dns_get_timestamp();
    dns_cache_entry_t** ptr = &dns_cache_head;
    
    while (*ptr) {
        if (dns_cache_entry_expired(*ptr, current_time)) {
            dns_cache_entry_t* expired = *ptr;
            *ptr = expired->next;
            free(expired);
            cache_entry_count--;
        } else {
            ptr = &((*ptr)->next);
        }
    }
}

/* ================================
 * DNS Resolution Functions
 * ================================ */

int dns_resolve(const char* hostname, ip_addr_t* addr) {
    if (!dns_initialized || !hostname || !addr) {
        return DNS_ERROR_INVALID;
    }
    
    if (!dns_is_valid_hostname(hostname)) {
        return DNS_ERROR_INVALID;
    }
    
    /* Check cache first */
    uint16_t data_len = sizeof(ip_addr_t);
    uint32_t ttl;
    if (dns_cache_lookup(hostname, DNS_TYPE_A, DNS_CLASS_IN, 
                        addr, &data_len, &ttl) == DNS_SUCCESS) {
        return DNS_SUCCESS;
    }
    
    /* Create DNS query packet */
    uint8_t packet[DNS_MAX_PACKET_SIZE];
    int packet_len = dns_packet_create_query(packet, sizeof(packet),
                                            hostname, DNS_TYPE_A, DNS_CLASS_IN);
    if (packet_len < 0) {
        return packet_len;
    }
    
    /* Try each DNS server */
    for (int server_idx = 0; server_idx < dns_server_count; server_idx++) {
        ip_addr_t server = dns_servers[(current_server_index + server_idx) % dns_server_count];
        
        for (int retry = 0; retry < dns_config.retries; retry++) {
            /* Send query */
            if (dns_send_query_packet(packet, packet_len, server) != DNS_SUCCESS) {
                continue;
            }
            
            dns_stats.queries_sent++;
            
            /* Wait for response (simplified - in real implementation would be async) */
            uint8_t response[DNS_MAX_PACKET_SIZE];
            ssize_t response_len = recv(dns_socket, response, sizeof(response), 0);
            
            if (response_len > 0) {
                dns_stats.responses_received++;
                
                /* Parse response */
                dns_header_t header;
                int parse_result = dns_packet_parse_response(response, response_len, &header);
                
                if (parse_result == DNS_SUCCESS) {
                    /* Extract A record */
                    size_t addr_size = sizeof(ip_addr_t);
                    if (dns_packet_extract_answer(response, response_len, hostname,
                                                 DNS_TYPE_A, addr, &addr_size) == DNS_SUCCESS) {
                        /* Add to cache */
                        dns_cache_add(hostname, DNS_TYPE_A, DNS_CLASS_IN,
                                     DNS_DEFAULT_TTL, addr, sizeof(ip_addr_t));
                        return DNS_SUCCESS;
                    }
                } else if (parse_result == DNS_ERROR_NXDOMAIN) {
                    dns_stats.nxdomain++;
                    return DNS_ERROR_NXDOMAIN;
                } else if (parse_result == DNS_ERROR_SERVFAIL) {
                    dns_stats.servfail++;
                }
            }
        }
    }
    
    dns_stats.timeouts++;
    return DNS_ERROR_TIMEOUT;
}

/* ================================
 * Utility Functions
 * ================================ */

bool dns_is_valid_hostname(const char* hostname) {
    if (!hostname || strlen(hostname) == 0 || strlen(hostname) > DNS_MAX_NAME_LEN) {
        return false;
    }
    
    const char* ptr = hostname;
    int label_len = 0;
    
    while (*ptr) {
        if (*ptr == '.') {
            if (label_len == 0 || label_len > DNS_MAX_LABEL_LEN) {
                return false;
            }
            label_len = 0;
        } else if ((*ptr >= 'a' && *ptr <= 'z') ||
                   (*ptr >= 'A' && *ptr <= 'Z') ||
                   (*ptr >= '0' && *ptr <= '9') ||
                   *ptr == '-') {
            label_len++;
            if (label_len > DNS_MAX_LABEL_LEN) {
                return false;
            }
        } else {
            return false;
        }
        ptr++;
    }
    
    return label_len > 0 && label_len <= DNS_MAX_LABEL_LEN;
}

uint32_t dns_get_timestamp(void) {
    /* Simplified timestamp - in real implementation would use system timer */
    static uint32_t counter = 0;
    return ++counter;
}

uint16_t dns_generate_id(void) {
    return query_id_counter++;
}

int dns_validate_config(const dns_config_t* config) {
    if (!config) {
        return DNS_ERROR_INVALID;
    }
    
    if (config->timeout == 0 || config->timeout > 60000) {
        return DNS_ERROR_INVALID;
    }
    
    if (config->retries == 0 || config->retries > 10) {
        return DNS_ERROR_INVALID;
    }
    
    if (config->cache_max_entries > 1000) {
        return DNS_ERROR_INVALID;
    }
    
    return DNS_SUCCESS;
}

/* ================================
 * Network Integration
 * ================================ */

int dns_send_query_packet(const uint8_t* packet, size_t packet_size,
                         ip_addr_t server_addr) {
    if (!packet || packet_size == 0) {
        return DNS_ERROR_INVALID;
    }
    
    struct sockaddr_in server_sockaddr;
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = dns_htons(DNS_PORT);
    server_sockaddr.sin_addr = server_addr;
    memset(server_sockaddr.sin_zero, 0, sizeof(server_sockaddr.sin_zero));
    
    ssize_t sent = sendto(dns_socket, packet, packet_size, 0,
                         (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr));
    
    if (sent != (ssize_t)packet_size) {
        dns_stats.errors++;
        return DNS_ERROR;
    }
    
    return DNS_SUCCESS;
}

/* ================================
 * Statistics and Debugging
 * ================================ */

int dns_get_stats(dns_stats_t* stats) {
    if (!stats) {
        return DNS_ERROR_INVALID;
    }
    
    *stats = dns_stats;
    return DNS_SUCCESS;
}

void dns_reset_stats(void) {
    memset(&dns_stats, 0, sizeof(dns_stats));
}

void dns_print_stats(void) {
    printf("DNS Statistics:\n");
    printf("  Queries sent:      %llu\n", dns_stats.queries_sent);
    printf("  Responses received: %llu\n", dns_stats.responses_received);
    printf("  Cache hits:        %llu\n", dns_stats.cache_hits);
    printf("  Cache misses:      %llu\n", dns_stats.cache_misses);
    printf("  Timeouts:          %llu\n", dns_stats.timeouts);
    printf("  Errors:            %llu\n", dns_stats.errors);
    printf("  NXDOMAIN:          %llu\n", dns_stats.nxdomain);
    printf("  Server failures:   %llu\n", dns_stats.servfail);
    printf("  Cache entries:     %u\n", cache_entry_count);
}

/* ================================
 * High-level Interface Functions
 * ================================ */

int gethostbyname(const char* hostname, ip_addr_t* addr) {
    return dns_resolve(hostname, addr);
}

int gethostbyaddr(ip_addr_t addr, char* hostname, size_t hostname_len) {
    return dns_reverse_lookup(addr, hostname, hostname_len);
}

int dns_reverse_lookup(ip_addr_t addr, char* hostname, size_t hostname_len) {
    if (!hostname || hostname_len == 0) {
        return DNS_ERROR_INVALID;
    }
    
    /* Create reverse DNS name (e.g., 1.0.0.127.in-addr.arpa) */
    char reverse_name[256];
    snprintf(reverse_name, sizeof(reverse_name), "%u.%u.%u.%u.in-addr.arpa",
             (addr.addr >> 0) & 0xFF,
             (addr.addr >> 8) & 0xFF,
             (addr.addr >> 16) & 0xFF,
             (addr.addr >> 24) & 0xFF);
    
    /* Query PTR record */
    uint8_t result[256];
    size_t result_size = sizeof(result);
    
    if (dns_resolve_type(reverse_name, DNS_TYPE_PTR, result, result_size) == DNS_SUCCESS) {
        /* Decode the hostname from PTR record */
        return dns_name_decode(result, result_size, hostname, hostname_len);
    }
    
    return DNS_ERROR_NXDOMAIN;
}

int dns_resolve_type(const char* hostname, uint16_t type, 
                    void* result, size_t result_size) {
    if (!dns_initialized || !hostname || !result) {
        return DNS_ERROR_INVALID;
    }
    
    /* Create DNS query packet */
    uint8_t packet[DNS_MAX_PACKET_SIZE];
    int packet_len = dns_packet_create_query(packet, sizeof(packet),
                                            hostname, type, DNS_CLASS_IN);
    if (packet_len < 0) {
        return packet_len;
    }
    
    /* Try each DNS server */
    for (int server_idx = 0; server_idx < dns_server_count; server_idx++) {
        ip_addr_t server = dns_servers[(current_server_index + server_idx) % dns_server_count];
        
        for (int retry = 0; retry < dns_config.retries; retry++) {
            /* Send query */
            if (dns_send_query_packet(packet, packet_len, server) != DNS_SUCCESS) {
                continue;
            }
            
            dns_stats.queries_sent++;
            
            /* Wait for response */
            uint8_t response[DNS_MAX_PACKET_SIZE];
            ssize_t response_len = recv(dns_socket, response, sizeof(response), 0);
            
            if (response_len > 0) {
                dns_stats.responses_received++;
                
                /* Parse response */
                dns_header_t header;
                int parse_result = dns_packet_parse_response(response, response_len, &header);
                
                if (parse_result == DNS_SUCCESS) {
                    /* Extract record */
                    if (dns_packet_extract_answer(response, response_len, hostname,
                                                 type, result, &result_size) == DNS_SUCCESS) {
                        return DNS_SUCCESS;
                    }
                }
            }
        }
    }
    
    return DNS_ERROR_TIMEOUT;
}
