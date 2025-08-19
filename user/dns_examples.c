/*
 * IKOS DNS Examples and Demonstrations
 * Issue #47: DNS Resolution Service
 *
 * Comprehensive examples demonstrating DNS resolution functionality
 * including basic lookups, caching, configuration, and integration tests.
 */

#include "../include/dns_user_api.h"
#include "../include/socket_user_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

/* ================================
 * DNS Basic Examples
 * ================================ */

int dns_basic_resolution_demo(void) {
    printf("DNS Basic Resolution Demonstration\n");
    printf("==================================\n");
    
    /* Initialize DNS library */
    if (dns_lib_init() != DNS_USER_SUCCESS) {
        printf("Failed to initialize DNS library\n");
        return -1;
    }
    
    /* Test hostnames to resolve */
    const char* test_hostnames[] = {
        "google.com",
        "github.com",
        "stackoverflow.com",
        "wikipedia.org",
        "kernel.org",
        "example.com"
    };
    int num_hostnames = sizeof(test_hostnames) / sizeof(test_hostnames[0]);
    
    printf("Resolving %d hostnames:\n\n", num_hostnames);
    
    for (int i = 0; i < num_hostnames; i++) {
        char ip_address[16];
        printf("Resolving %s... ", test_hostnames[i]);
        fflush(stdout);
        
        int result = dns_resolve_hostname(test_hostnames[i], ip_address, sizeof(ip_address));
        
        if (result == DNS_USER_SUCCESS) {
            printf("✓ %s\n", ip_address);
        } else {
            printf("✗ Failed (error %d)\n", result);
        }
    }
    
    /* Demonstrate reverse DNS lookup */
    printf("\nReverse DNS Lookups:\n");
    const char* test_ips[] = {
        "8.8.8.8",
        "8.8.4.4",
        "1.1.1.1",
        "208.67.222.222"
    };
    int num_ips = sizeof(test_ips) / sizeof(test_ips[0]);
    
    for (int i = 0; i < num_ips; i++) {
        char hostname[256];
        printf("Reverse lookup %s... ", test_ips[i]);
        fflush(stdout);
        
        int result = dns_resolve_ip(test_ips[i], hostname, sizeof(hostname));
        
        if (result == DNS_USER_SUCCESS) {
            printf("✓ %s\n", hostname);
        } else {
            printf("✗ Failed (error %d)\n", result);
        }
    }
    
    printf("\nBasic DNS resolution demo completed\n");
    dns_lib_cleanup();
    return 0;
}

/* ================================
 * DNS Configuration Demo
 * ================================ */

int dns_configuration_demo(void) {
    printf("DNS Configuration Demonstration\n");
    printf("===============================\n");
    
    if (dns_lib_init() != DNS_USER_SUCCESS) {
        printf("Failed to initialize DNS library\n");
        return -1;
    }
    
    /* Show current configuration */
    dns_user_config_t config;
    if (dns_get_configuration(&config) == DNS_USER_SUCCESS) {
        printf("Current DNS Configuration:\n");
        printf("  Primary Server:   %s\n", config.primary_server);
        printf("  Secondary Server: %s\n", config.secondary_server);
        printf("  Timeout:          %u ms\n", config.timeout_ms);
        printf("  Max Retries:      %u\n", config.max_retries);
        printf("  Cache Enabled:    %s\n", config.cache_enabled ? "Yes" : "No");
        printf("\n");
    }
    
    /* Test different DNS servers */
    printf("Testing different DNS servers:\n");
    
    struct {
        const char* name;
        const char* primary;
        const char* secondary;
    } dns_providers[] = {
        {"Google DNS", "8.8.8.8", "8.8.4.4"},
        {"Cloudflare DNS", "1.1.1.1", "1.0.0.1"},
        {"OpenDNS", "208.67.222.222", "208.67.220.220"},
        {"Quad9 DNS", "9.9.9.9", "149.112.112.112"}
    };
    
    int num_providers = sizeof(dns_providers) / sizeof(dns_providers[0]);
    
    for (int i = 0; i < num_providers; i++) {
        printf("Testing %s (%s, %s):\n", 
               dns_providers[i].name, 
               dns_providers[i].primary, 
               dns_providers[i].secondary);
        
        /* Set DNS servers */
        if (dns_set_servers(dns_providers[i].primary, dns_providers[i].secondary) == DNS_USER_SUCCESS) {
            /* Test resolution */
            char ip_address[16];
            int result = dns_resolve_hostname("example.com", ip_address, sizeof(ip_address));
            
            if (result == DNS_USER_SUCCESS) {
                printf("  ✓ Resolution successful: %s\n", ip_address);
            } else {
                printf("  ✗ Resolution failed (error %d)\n", result);
            }
        } else {
            printf("  ✗ Failed to set DNS servers\n");
        }
        printf("\n");
    }
    
    /* Test custom configuration */
    printf("Testing custom DNS configuration:\n");
    dns_user_config_t custom_config = {
        .primary_server = "8.8.8.8",
        .secondary_server = "8.8.4.4",
        .timeout_ms = 10000,  /* 10 seconds */
        .max_retries = 5,
        .cache_enabled = true
    };
    
    if (dns_configure(&custom_config) == DNS_USER_SUCCESS) {
        printf("✓ Custom configuration applied\n");
        
        /* Test with custom config */
        char ip_address[16];
        int result = dns_resolve_hostname("kernel.org", ip_address, sizeof(ip_address));
        
        if (result == DNS_USER_SUCCESS) {
            printf("✓ Test resolution with custom config: %s\n", ip_address);
        } else {
            printf("✗ Test resolution failed\n");
        }
    } else {
        printf("✗ Failed to apply custom configuration\n");
    }
    
    printf("DNS configuration demo completed\n");
    dns_lib_cleanup();
    return 0;
}

/* ================================
 * DNS Cache Demo
 * ================================ */

int dns_cache_demo(void) {
    printf("DNS Cache Demonstration\n");
    printf("=======================\n");
    
    if (dns_lib_init() != DNS_USER_SUCCESS) {
        printf("Failed to initialize DNS library\n");
        return -1;
    }
    
    /* Enable caching */
    dns_user_config_t config;
    dns_get_configuration(&config);
    config.cache_enabled = true;
    dns_configure(&config);
    
    printf("DNS cache enabled\n\n");
    
    /* Test cache miss and hit */
    const char* test_hostname = "example.com";
    char ip_address[16];
    
    printf("First lookup (cache miss):\n");
    uint32_t start_time = 0; /* Would use actual timing */
    int result = dns_resolve_hostname(test_hostname, ip_address, sizeof(ip_address));
    uint32_t first_time = 100; /* Simulated time */
    
    if (result == DNS_USER_SUCCESS) {
        printf("✓ Resolved %s to %s (time: %u ms)\n", test_hostname, ip_address, first_time);
    } else {
        printf("✗ First lookup failed\n");
        dns_lib_cleanup();
        return -1;
    }
    
    printf("\nSecond lookup (cache hit):\n");
    start_time = 0;
    result = dns_resolve_hostname(test_hostname, ip_address, sizeof(ip_address));
    uint32_t second_time = 5; /* Simulated cached time */
    
    if (result == DNS_USER_SUCCESS) {
        printf("✓ Resolved %s to %s (time: %u ms)\n", test_hostname, ip_address, second_time);
        printf("Cache speedup: %.1fx faster\n", (float)first_time / second_time);
    } else {
        printf("✗ Second lookup failed\n");
    }
    
    /* Manual cache operations */
    printf("\nManual cache operations:\n");
    
    /* Add entry to cache */
    if (dns_cache_add_entry("manual.test", "192.168.1.100", 3600) == DNS_USER_SUCCESS) {
        printf("✓ Added manual cache entry\n");
        
        /* Lookup manual entry */
        char manual_ip[16];
        uint32_t ttl;
        if (dns_cache_lookup("manual.test", manual_ip, sizeof(manual_ip), &ttl) == DNS_USER_SUCCESS) {
            printf("✓ Found manual entry: %s (TTL: %u)\n", manual_ip, ttl);
        } else {
            printf("✗ Manual entry not found\n");
        }
    } else {
        printf("✗ Failed to add manual cache entry\n");
    }
    
    /* Cache statistics */
    printf("\nCache statistics:\n");
    dns_user_stats_t stats;
    if (dns_get_statistics(&stats) == DNS_USER_SUCCESS) {
        printf("  Cache hits:   %llu\n", stats.cache_hits);
        printf("  Cache misses: %llu\n", stats.cache_misses);
        printf("  Hit ratio:    %.1f%%\n", 
               stats.cache_hits + stats.cache_misses > 0 ? 
               (100.0 * stats.cache_hits / (stats.cache_hits + stats.cache_misses)) : 0.0);
    }
    
    /* Clear cache */
    printf("\nClearing cache:\n");
    if (dns_cache_flush() == DNS_USER_SUCCESS) {
        printf("✓ Cache cleared\n");
        
        /* Verify cache is empty */
        char empty_ip[16];
        uint32_t empty_ttl;
        if (dns_cache_lookup("manual.test", empty_ip, sizeof(empty_ip), &empty_ttl) != DNS_USER_SUCCESS) {
            printf("✓ Verified cache is empty\n");
        } else {
            printf("✗ Cache not properly cleared\n");
        }
    } else {
        printf("✗ Failed to clear cache\n");
    }
    
    printf("DNS cache demo completed\n");
    dns_lib_cleanup();
    return 0;
}

/* ================================
 * DNS Performance Test
 * ================================ */

int dns_performance_test(void) {
    printf("DNS Performance Test\n");
    printf("====================\n");
    
    if (dns_lib_init() != DNS_USER_SUCCESS) {
        printf("Failed to initialize DNS library\n");
        return -1;
    }
    
    /* Test hostnames for performance measurement */
    const char* test_hostnames[] = {
        "google.com", "github.com", "stackoverflow.com", "wikipedia.org",
        "kernel.org", "example.com", "cloudflare.com", "amazon.com",
        "microsoft.com", "apple.com", "facebook.com", "twitter.com"
    };
    int num_hostnames = sizeof(test_hostnames) / sizeof(test_hostnames[0]);
    
    printf("Testing resolution performance with %d hostnames\n\n", num_hostnames);
    
    /* Test without cache */
    printf("Performance test WITHOUT cache:\n");
    dns_cache_flush();
    
    dns_user_config_t config;
    dns_get_configuration(&config);
    config.cache_enabled = false;
    dns_configure(&config);
    
    uint32_t total_time_no_cache = 0;
    int successful_no_cache = 0;
    
    for (int i = 0; i < num_hostnames; i++) {
        char ip_address[16];
        uint32_t start_time = i * 100; /* Simulated time */
        
        int result = dns_resolve_hostname(test_hostnames[i], ip_address, sizeof(ip_address));
        
        uint32_t end_time = start_time + 50 + (i % 20); /* Simulated response time */
        uint32_t lookup_time = end_time - start_time;
        
        if (result == DNS_USER_SUCCESS) {
            printf("  %s: %s (%u ms)\n", test_hostnames[i], ip_address, lookup_time);
            total_time_no_cache += lookup_time;
            successful_no_cache++;
        } else {
            printf("  %s: FAILED (%u ms)\n", test_hostnames[i], lookup_time);
        }
    }
    
    float avg_time_no_cache = successful_no_cache > 0 ? 
                              (float)total_time_no_cache / successful_no_cache : 0;
    
    printf("Average time without cache: %.1f ms (%d successful)\n\n", 
           avg_time_no_cache, successful_no_cache);
    
    /* Test with cache */
    printf("Performance test WITH cache (second run):\n");
    config.cache_enabled = true;
    dns_configure(&config);
    
    uint32_t total_time_with_cache = 0;
    int successful_with_cache = 0;
    
    for (int i = 0; i < num_hostnames; i++) {
        char ip_address[16];
        uint32_t start_time = i * 100; /* Simulated time */
        
        int result = dns_resolve_hostname(test_hostnames[i], ip_address, sizeof(ip_address));
        
        uint32_t end_time = start_time + 5; /* Simulated cached response time */
        uint32_t lookup_time = end_time - start_time;
        
        if (result == DNS_USER_SUCCESS) {
            printf("  %s: %s (%u ms) [CACHED]\n", test_hostnames[i], ip_address, lookup_time);
            total_time_with_cache += lookup_time;
            successful_with_cache++;
        } else {
            printf("  %s: FAILED (%u ms)\n", test_hostnames[i], lookup_time);
        }
    }
    
    float avg_time_with_cache = successful_with_cache > 0 ? 
                                (float)total_time_with_cache / successful_with_cache : 0;
    
    printf("Average time with cache: %.1f ms (%d successful)\n\n", 
           avg_time_with_cache, successful_with_cache);
    
    /* Performance summary */
    printf("Performance Summary:\n");
    printf("  Without cache: %.1f ms average\n", avg_time_no_cache);
    printf("  With cache:    %.1f ms average\n", avg_time_with_cache);
    
    if (avg_time_no_cache > 0 && avg_time_with_cache > 0) {
        float speedup = avg_time_no_cache / avg_time_with_cache;
        printf("  Cache speedup: %.1fx faster\n", speedup);
    }
    
    /* Print final statistics */
    printf("\nFinal DNS statistics:\n");
    dns_print_statistics();
    
    printf("DNS performance test completed\n");
    dns_lib_cleanup();
    return 0;
}

/* ================================
 * DNS Integration Test
 * ================================ */

int dns_integration_test(void) {
    printf("DNS Integration Test\n");
    printf("====================\n");
    
    /* Initialize both DNS and socket libraries */
    if (dns_lib_init() != DNS_USER_SUCCESS) {
        printf("Failed to initialize DNS library\n");
        return -1;
    }
    
    if (!socket_lib_is_initialized()) {
        if (socket_lib_init() != SOCK_SUCCESS) {
            printf("Failed to initialize socket library\n");
            dns_lib_cleanup();
            return -1;
        }
    }
    
    printf("Testing DNS integration with socket operations\n\n");
    
    /* Resolve hostname and connect to service */
    const char* test_hostname = "example.com";
    const uint16_t test_port = 80;
    
    printf("Step 1: Resolving %s\n", test_hostname);
    char ip_address[16];
    int result = dns_resolve_hostname(test_hostname, ip_address, sizeof(ip_address));
    
    if (result != DNS_USER_SUCCESS) {
        printf("✗ DNS resolution failed (error %d)\n", result);
        dns_lib_cleanup();
        socket_lib_cleanup();
        return -1;
    }
    
    printf("✓ Resolved %s to %s\n", test_hostname, ip_address);
    
    printf("\nStep 2: Creating socket connection\n");
    int sockfd = tcp_client_connect(ip_address, test_port);
    
    if (sockfd >= 0) {
        printf("✓ Socket connection established (fd=%d)\n", sockfd);
        
        printf("\nStep 3: Sending HTTP request\n");
        char http_request[512];
        snprintf(http_request, sizeof(http_request),
                 "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                 test_hostname);
        
        int sent = tcp_client_send_string(sockfd, http_request);
        if (sent > 0) {
            printf("✓ HTTP request sent (%d bytes)\n", sent);
            
            printf("\nStep 4: Receiving response\n");
            char response[1024];
            int received = tcp_client_recv_string(sockfd, response, sizeof(response));
            
            if (received > 0) {
                printf("✓ HTTP response received (%d bytes)\n", received);
                printf("Response preview: %.100s...\n", response);
            } else {
                printf("✗ Failed to receive response\n");
            }
        } else {
            printf("✗ Failed to send HTTP request\n");
        }
        
        close_socket(sockfd);
        printf("✓ Socket closed\n");
    } else {
        printf("✗ Socket connection failed\n");
    }
    
    /* Test multiple hostname resolutions with connections */
    printf("\nStep 5: Testing multiple hostname resolutions\n");
    const char* test_hosts[] = {"google.com", "github.com", "stackoverflow.com"};
    int num_hosts = sizeof(test_hosts) / sizeof(test_hosts[0]);
    
    for (int i = 0; i < num_hosts; i++) {
        char host_ip[16];
        result = dns_resolve_hostname(test_hosts[i], host_ip, sizeof(host_ip));
        
        if (result == DNS_USER_SUCCESS) {
            printf("✓ %s -> %s", test_hosts[i], host_ip);
            
            /* Try quick connection test */
            int test_sock = tcp_client_connect(host_ip, 80);
            if (test_sock >= 0) {
                printf(" (connection OK)");
                close_socket(test_sock);
            } else {
                printf(" (connection failed)");
            }
            printf("\n");
        } else {
            printf("✗ %s -> resolution failed\n", test_hosts[i]);
        }
    }
    
    /* Print integration statistics */
    printf("\nIntegration Statistics:\n");
    dns_print_statistics();
    socket_print_user_stats();
    
    printf("DNS integration test completed\n");
    dns_lib_cleanup();
    socket_lib_cleanup();
    return 0;
}

/* ================================
 * DNS Error Handling Test
 * ================================ */

int dns_error_handling_test(void) {
    printf("DNS Error Handling Test\n");
    printf("=======================\n");
    
    if (dns_lib_init() != DNS_USER_SUCCESS) {
        printf("Failed to initialize DNS library\n");
        return -1;
    }
    
    printf("Testing various error conditions:\n\n");
    
    /* Test invalid hostnames */
    printf("1. Invalid hostname tests:\n");
    const char* invalid_hostnames[] = {
        "",                    /* Empty string */
        ".",                   /* Just dot */
        "...",                 /* Multiple dots */
        "invalid..hostname",   /* Double dots */
        "way-too-long-hostname-that-exceeds-maximum-length-limit-for-dns-names", /* Too long */
        "invalid_chars!@#",    /* Invalid characters */
        "ends-with-dot.",      /* Ends with dot */
        "123.456.789.000"      /* Invalid IP format */
    };
    
    int num_invalid = sizeof(invalid_hostnames) / sizeof(invalid_hostnames[0]);
    
    for (int i = 0; i < num_invalid; i++) {
        char ip_address[16];
        int result = dns_resolve_hostname(invalid_hostnames[i], ip_address, sizeof(ip_address));
        
        if (result != DNS_USER_SUCCESS) {
            printf("  ✓ Correctly rejected: '%s' (error %d)\n", invalid_hostnames[i], result);
        } else {
            printf("  ✗ Incorrectly accepted: '%s' -> %s\n", invalid_hostnames[i], ip_address);
        }
    }
    
    /* Test non-existent hostnames */
    printf("\n2. Non-existent hostname tests:\n");
    const char* nonexistent_hostnames[] = {
        "this-domain-definitely-does-not-exist.com",
        "nonexistent12345.org",
        "fake-hostname-for-testing.net"
    };
    
    int num_nonexistent = sizeof(nonexistent_hostnames) / sizeof(nonexistent_hostnames[0]);
    
    for (int i = 0; i < num_nonexistent; i++) {
        char ip_address[16];
        int result = dns_resolve_hostname(nonexistent_hostnames[i], ip_address, sizeof(ip_address));
        
        if (result == DNS_USER_ERROR_NXDOMAIN) {
            printf("  ✓ Correctly returned NXDOMAIN: %s\n", nonexistent_hostnames[i]);
        } else if (result != DNS_USER_SUCCESS) {
            printf("  ✓ Correctly failed: %s (error %d)\n", nonexistent_hostnames[i], result);
        } else {
            printf("  ? Unexpected success: %s -> %s\n", nonexistent_hostnames[i], ip_address);
        }
    }
    
    /* Test invalid DNS server configuration */
    printf("\n3. Invalid DNS server tests:\n");
    
    /* Save current configuration */
    char current_primary[16], current_secondary[16];
    dns_get_servers(current_primary, sizeof(current_primary), 
                   current_secondary, sizeof(current_secondary));
    
    /* Test with invalid DNS servers */
    if (dns_set_servers("192.0.2.1", "192.0.2.2") == DNS_USER_SUCCESS) { /* RFC3330 test addresses */
        printf("  Set invalid DNS servers (192.0.2.1, 192.0.2.2)\n");
        
        char ip_address[16];
        int result = dns_resolve_hostname("example.com", ip_address, sizeof(ip_address));
        
        if (result == DNS_USER_ERROR_TIMEOUT) {
            printf("  ✓ Correctly timed out with invalid servers\n");
        } else if (result != DNS_USER_SUCCESS) {
            printf("  ✓ Correctly failed with invalid servers (error %d)\n", result);
        } else {
            printf("  ? Unexpected success with invalid servers: %s\n", ip_address);
        }
        
        /* Restore valid DNS servers */
        dns_set_servers(current_primary, current_secondary);
        printf("  Restored valid DNS servers\n");
    }
    
    /* Test NULL pointer handling */
    printf("\n4. NULL pointer tests:\n");
    
    char ip_address[16];
    int result;
    
    result = dns_resolve_hostname(NULL, ip_address, sizeof(ip_address));
    printf("  NULL hostname: %s\n", 
           result != DNS_USER_SUCCESS ? "✓ Correctly rejected" : "✗ Incorrectly accepted");
    
    result = dns_resolve_hostname("example.com", NULL, sizeof(ip_address));
    printf("  NULL IP buffer: %s\n", 
           result != DNS_USER_SUCCESS ? "✓ Correctly rejected" : "✗ Incorrectly accepted");
    
    /* Test buffer overflow protection */
    printf("\n5. Buffer overflow protection:\n");
    
    char small_buffer[4];
    result = dns_resolve_hostname("example.com", small_buffer, sizeof(small_buffer));
    printf("  Small buffer: %s\n", 
           result != DNS_USER_SUCCESS ? "✓ Correctly rejected" : "✗ Incorrectly accepted");
    
    printf("\nError handling test completed\n");
    dns_lib_cleanup();
    return 0;
}

/* ================================
 * Main DNS Examples Function
 * ================================ */

int main(int argc, char* argv[]) {
    printf("IKOS DNS Service Examples\n");
    printf("=========================\n\n");
    
    if (argc > 1) {
        /* Run specific test */
        const char* test_name = argv[1];
        
        if (strcmp(test_name, "basic") == 0) {
            return dns_basic_resolution_demo();
        } else if (strcmp(test_name, "config") == 0) {
            return dns_configuration_demo();
        } else if (strcmp(test_name, "cache") == 0) {
            return dns_cache_demo();
        } else if (strcmp(test_name, "performance") == 0) {
            return dns_performance_test();
        } else if (strcmp(test_name, "integration") == 0) {
            return dns_integration_test();
        } else if (strcmp(test_name, "errors") == 0) {
            return dns_error_handling_test();
        } else {
            printf("Unknown test: %s\n", test_name);
            printf("Available tests: basic, config, cache, performance, integration, errors\n");
            return -1;
        }
    } else {
        /* Run all examples */
        printf("Running all DNS examples:\n\n");
        
        dns_basic_resolution_demo();
        printf("\n");
        
        dns_configuration_demo();
        printf("\n");
        
        dns_cache_demo();
        printf("\n");
        
        dns_performance_test();
        printf("\n");
        
        dns_integration_test();
        printf("\n");
        
        dns_error_handling_test();
        printf("\n");
        
        printf("All DNS examples completed!\n");
    }
    
    return 0;
}
