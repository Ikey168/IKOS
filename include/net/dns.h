/* IKOS Network Stack - DNS Resolution Service
 * Issue #47 - DNS Resolution Service
 * 
 * Provides Domain Name System (DNS) resolution functionality
 * for converting domain names to IP addresses and vice versa.
 * 
 * RFC References:
 * - RFC 1034: Domain Names - Concepts and Facilities
 * - RFC 1035: Domain Names - Implementation and Specification
 * - RFC 1123: Requirements for Internet Hosts
 */

#ifndef DNS_H
#define DNS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "network.h"
#include "ip.h"

/* ================================
 * DNS Constants
 * ================================ */

#define DNS_PORT                53      /* Standard DNS port */
#define DNS_MAX_NAME_LEN        255     /* Maximum domain name length */
#define DNS_MAX_LABEL_LEN       63      /* Maximum label length */
#define DNS_MAX_PACKET_SIZE     512     /* Maximum UDP DNS packet size */
#define DNS_MAX_TCP_SIZE        65535   /* Maximum TCP DNS packet size */
#define DNS_HEADER_SIZE         12      /* DNS header size */
#define DNS_MAX_CACHE_ENTRIES   256     /* Maximum cache entries */
#define DNS_DEFAULT_TTL         3600    /* Default TTL (1 hour) */
#define DNS_MAX_RETRIES         3       /* Maximum query retries */
#define DNS_QUERY_TIMEOUT       5000    /* Query timeout in ms */

/* DNS Classes */
#define DNS_CLASS_IN            1       /* Internet class */
#define DNS_CLASS_CS            2       /* CSNET class (obsolete) */
#define DNS_CLASS_CH            3       /* CHAOS class */
#define DNS_CLASS_HS            4       /* Hesiod class */
#define DNS_CLASS_ANY           255     /* Any class */

/* DNS Types */
#define DNS_TYPE_A              1       /* IPv4 address */
#define DNS_TYPE_NS             2       /* Name server */
#define DNS_TYPE_CNAME          5       /* Canonical name */
#define DNS_TYPE_SOA            6       /* Start of authority */
#define DNS_TYPE_PTR            12      /* Pointer record */
#define DNS_TYPE_MX             15      /* Mail exchange */
#define DNS_TYPE_TXT            16      /* Text record */
#define DNS_TYPE_AAAA           28      /* IPv6 address */
#define DNS_TYPE_SRV            33      /* Service record */
#define DNS_TYPE_ANY            255     /* Any type */

/* DNS Response Codes */
#define DNS_RCODE_NOERROR       0       /* No error */
#define DNS_RCODE_FORMERR       1       /* Format error */
#define DNS_RCODE_SERVFAIL      2       /* Server failure */
#define DNS_RCODE_NXDOMAIN      3       /* Name error */
#define DNS_RCODE_NOTIMP        4       /* Not implemented */
#define DNS_RCODE_REFUSED       5       /* Query refused */

/* DNS Header Flags */
#define DNS_FLAG_QR             0x8000  /* Query/Response flag */
#define DNS_FLAG_AA             0x0400  /* Authoritative answer */
#define DNS_FLAG_TC             0x0200  /* Truncation flag */
#define DNS_FLAG_RD             0x0100  /* Recursion desired */
#define DNS_FLAG_RA             0x0080  /* Recursion available */
#define DNS_FLAG_AD             0x0020  /* Authentic data */
#define DNS_FLAG_CD             0x0010  /* Checking disabled */

/* DNS Opcode */
#define DNS_OPCODE_QUERY        0       /* Standard query */
#define DNS_OPCODE_IQUERY       1       /* Inverse query */
#define DNS_OPCODE_STATUS       2       /* Server status request */

/* Error codes */
#define DNS_SUCCESS             0
#define DNS_ERROR               -1
#define DNS_ERROR_INVALID       -2
#define DNS_ERROR_TIMEOUT       -3
#define DNS_ERROR_NXDOMAIN      -4
#define DNS_ERROR_SERVFAIL      -5
#define DNS_ERROR_REFUSED       -6
#define DNS_ERROR_CACHE_FULL    -7
#define DNS_ERROR_NO_MEMORY     -8

/* ================================
 * DNS Data Structures
 * ================================ */

/* DNS Header */
typedef struct __attribute__((packed)) {
    uint16_t id;               /* Query ID */
    uint16_t flags;            /* Flags and response codes */
    uint16_t qdcount;          /* Number of questions */
    uint16_t ancount;          /* Number of answers */
    uint16_t nscount;          /* Number of authority records */
    uint16_t arcount;          /* Number of additional records */
} dns_header_t;

/* DNS Question */
typedef struct __attribute__((packed)) {
    /* Variable length name followed by: */
    uint16_t qtype;            /* Question type */
    uint16_t qclass;           /* Question class */
} dns_question_t;

/* DNS Resource Record */
typedef struct __attribute__((packed)) {
    /* Variable length name followed by: */
    uint16_t type;             /* Record type */
    uint16_t class;            /* Record class */
    uint32_t ttl;              /* Time to live */
    uint16_t rdlength;         /* Resource data length */
    /* Variable length resource data follows */
} dns_rr_t;

/* DNS Cache Entry */
typedef struct dns_cache_entry {
    char name[DNS_MAX_NAME_LEN + 1];    /* Domain name */
    uint16_t type;                      /* Record type */
    uint16_t class;                     /* Record class */
    uint32_t ttl;                       /* Time to live */
    uint32_t timestamp;                 /* Cache timestamp */
    uint16_t data_len;                  /* Data length */
    uint8_t data[256];                  /* Record data */
    struct dns_cache_entry* next;       /* Next cache entry */
} dns_cache_entry_t;

/* DNS Query Context */
typedef struct dns_query {
    uint16_t id;                        /* Query ID */
    char name[DNS_MAX_NAME_LEN + 1];    /* Query name */
    uint16_t type;                      /* Query type */
    uint16_t class;                     /* Query class */
    uint32_t timestamp;                 /* Query timestamp */
    int retries;                        /* Retry count */
    void (*callback)(struct dns_query* query, int result, ip_addr_t* addr);
    void* user_data;                    /* User data */
    struct dns_query* next;             /* Next pending query */
} dns_query_t;

/* DNS Configuration */
typedef struct {
    ip_addr_t primary_server;           /* Primary DNS server */
    ip_addr_t secondary_server;         /* Secondary DNS server */
    uint32_t timeout;                   /* Query timeout in ms */
    uint32_t retries;                   /* Maximum retries */
    bool cache_enabled;                 /* Enable DNS caching */
    uint32_t cache_max_entries;         /* Maximum cache entries */
    uint32_t default_ttl;               /* Default TTL for cache */
} dns_config_t;

/* DNS Statistics */
typedef struct {
    uint64_t queries_sent;              /* Total queries sent */
    uint64_t responses_received;        /* Total responses received */
    uint64_t cache_hits;                /* Cache hits */
    uint64_t cache_misses;              /* Cache misses */
    uint64_t timeouts;                  /* Query timeouts */
    uint64_t errors;                    /* Query errors */
    uint64_t nxdomain;                  /* NXDOMAIN responses */
    uint64_t servfail;                  /* Server failure responses */
} dns_stats_t;

/* ================================
 * DNS Function Prototypes
 * ================================ */

/* Initialization and Configuration */
int dns_init(void);
void dns_cleanup(void);
int dns_configure(const dns_config_t* config);
int dns_get_config(dns_config_t* config);
int dns_set_servers(ip_addr_t primary, ip_addr_t secondary);
int dns_get_servers(ip_addr_t* primary, ip_addr_t* secondary);

/* Domain Name Resolution */
int dns_resolve(const char* hostname, ip_addr_t* addr);
int dns_resolve_async(const char* hostname, 
                     void (*callback)(dns_query_t* query, int result, ip_addr_t* addr),
                     void* user_data);
int dns_resolve_type(const char* hostname, uint16_t type, 
                    void* result, size_t result_size);

/* Reverse DNS Lookup */
int dns_reverse_lookup(ip_addr_t addr, char* hostname, size_t hostname_len);
int dns_reverse_lookup_async(ip_addr_t addr,
                            void (*callback)(dns_query_t* query, int result, const char* hostname),
                            void* user_data);

/* Cache Management */
int dns_cache_add(const char* name, uint16_t type, uint16_t class,
                 uint32_t ttl, const void* data, uint16_t data_len);
int dns_cache_lookup(const char* name, uint16_t type, uint16_t class,
                    void* data, uint16_t* data_len, uint32_t* ttl);
int dns_cache_remove(const char* name, uint16_t type, uint16_t class);
void dns_cache_clear(void);
void dns_cache_cleanup_expired(void);
int dns_cache_get_stats(uint32_t* entries, uint32_t* memory_used);

/* Query Management */
dns_query_t* dns_query_create(const char* name, uint16_t type, uint16_t class);
void dns_query_destroy(dns_query_t* query);
int dns_query_send(dns_query_t* query);
int dns_query_cancel(uint16_t query_id);
void dns_query_process_pending(void);

/* Packet Processing */
int dns_packet_create_query(uint8_t* packet, size_t packet_size,
                           const char* name, uint16_t type, uint16_t class);
int dns_packet_parse_response(const uint8_t* packet, size_t packet_size,
                             dns_header_t* header);
int dns_packet_extract_answer(const uint8_t* packet, size_t packet_size,
                             const char* name, uint16_t type,
                             void* data, size_t* data_size);

/* Name Encoding/Decoding */
int dns_name_encode(const char* name, uint8_t* encoded, size_t encoded_size);
int dns_name_decode(const uint8_t* encoded, size_t encoded_size, 
                   char* name, size_t name_size);
int dns_name_compress(const uint8_t* packet, size_t packet_size,
                     const char* name, uint8_t* compressed, size_t* compressed_size);
int dns_name_decompress(const uint8_t* packet, size_t packet_size,
                       size_t offset, char* name, size_t name_size);

/* Utility Functions */
bool dns_is_valid_hostname(const char* hostname);
bool dns_is_valid_label(const char* label);
int dns_hostname_to_labels(const char* hostname, char labels[][DNS_MAX_LABEL_LEN + 1]);
int dns_labels_to_hostname(char labels[][DNS_MAX_LABEL_LEN + 1], int count, char* hostname);
uint32_t dns_get_timestamp(void);
uint16_t dns_generate_id(void);

/* Statistics and Debugging */
int dns_get_stats(dns_stats_t* stats);
void dns_reset_stats(void);
void dns_print_stats(void);
void dns_print_cache(void);
int dns_validate_config(const dns_config_t* config);

/* Integration with Network Stack */
int dns_register_protocol(void);
void dns_handle_packet(const uint8_t* packet, size_t packet_size,
                      ip_addr_t src_addr, uint16_t src_port);
int dns_send_query_packet(const uint8_t* packet, size_t packet_size,
                         ip_addr_t server_addr);

/* High-level Interface Functions */
int gethostbyname(const char* hostname, ip_addr_t* addr);
int gethostbyaddr(ip_addr_t addr, char* hostname, size_t hostname_len);
int getaddrinfo(const char* hostname, const char* service,
               const void* hints, void* result);
void freeaddrinfo(void* ai);

/* DNS Server List Management */
int dns_add_server(ip_addr_t server_addr);
int dns_remove_server(ip_addr_t server_addr);
int dns_get_server_list(ip_addr_t* servers, size_t* count);
void dns_rotate_servers(void);

/* ================================
 * DNS Macros and Inline Functions
 * ================================ */

/* Extract DNS header flags */
#define DNS_GET_QR(flags)       (((flags) & DNS_FLAG_QR) ? 1 : 0)
#define DNS_GET_OPCODE(flags)   (((flags) >> 11) & 0x0F)
#define DNS_GET_AA(flags)       (((flags) & DNS_FLAG_AA) ? 1 : 0)
#define DNS_GET_TC(flags)       (((flags) & DNS_FLAG_TC) ? 1 : 0)
#define DNS_GET_RD(flags)       (((flags) & DNS_FLAG_RD) ? 1 : 0)
#define DNS_GET_RA(flags)       (((flags) & DNS_FLAG_RA) ? 1 : 0)
#define DNS_GET_RCODE(flags)    ((flags) & 0x0F)

/* Set DNS header flags */
#define DNS_SET_QR(flags, val)      ((flags) = ((flags) & ~DNS_FLAG_QR) | ((val) ? DNS_FLAG_QR : 0))
#define DNS_SET_OPCODE(flags, val)  ((flags) = ((flags) & ~(0x0F << 11)) | (((val) & 0x0F) << 11))
#define DNS_SET_AA(flags, val)      ((flags) = ((flags) & ~DNS_FLAG_AA) | ((val) ? DNS_FLAG_AA : 0))
#define DNS_SET_TC(flags, val)      ((flags) = ((flags) & ~DNS_FLAG_TC) | ((val) ? DNS_FLAG_TC : 0))
#define DNS_SET_RD(flags, val)      ((flags) = ((flags) & ~DNS_FLAG_RD) | ((val) ? DNS_FLAG_RD : 0))
#define DNS_SET_RA(flags, val)      ((flags) = ((flags) & ~DNS_FLAG_RA) | ((val) ? DNS_FLAG_RA : 0))
#define DNS_SET_RCODE(flags, val)   ((flags) = ((flags) & ~0x0F) | ((val) & 0x0F))

/* Network byte order conversion for DNS */
static inline uint16_t dns_htons(uint16_t val) {
    return ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
}

static inline uint32_t dns_htonl(uint32_t val) {
    return ((val & 0xFF) << 24) | (((val >> 8) & 0xFF) << 16) |
           (((val >> 16) & 0xFF) << 8) | ((val >> 24) & 0xFF);
}

static inline uint16_t dns_ntohs(uint16_t val) {
    return dns_htons(val);
}

static inline uint32_t dns_ntohl(uint32_t val) {
    return dns_htonl(val);
}

/* DNS cache entry expiration check */
static inline bool dns_cache_entry_expired(const dns_cache_entry_t* entry, uint32_t current_time) {
    return (current_time - entry->timestamp) > entry->ttl;
}

/* DNS name comparison (case-insensitive) */
static inline int dns_name_compare(const char* name1, const char* name2) {
    while (*name1 && *name2) {
        char c1 = (*name1 >= 'A' && *name1 <= 'Z') ? (*name1 + 32) : *name1;
        char c2 = (*name2 >= 'A' && *name2 <= 'Z') ? (*name2 + 32) : *name2;
        if (c1 != c2) return c1 - c2;
        name1++;
        name2++;
    }
    return *name1 - *name2;
}

#endif /* DNS_H */
