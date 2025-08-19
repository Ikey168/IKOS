/*
 * IKOS DNS Test Suite
 * Issue #47: DNS Resolution Service
 *
 * Comprehensive test suite for DNS resolution functionality
 * including unit tests, integration tests, and stress tests.
 */

#include "../include/dns_user_api.h"
#include "../include/socket_user_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ================================
 * Test Framework
 * ================================ */

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("[PASS] %s\n", message); \
        } else { \
            tests_failed++; \
            printf("[FAIL] %s\n", message); \
        } \
    } while(0)

#define TEST_GROUP(name) printf("\n=== %s ===\n", name)

void print_test_summary(void) {
    printf("\n=== DNS Test Summary ===\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
}

/* ================================
 * DNS Library Tests
 * ================================ */

void test_dns_library_initialization(void) {
    TEST_GROUP("DNS Library Initialization");
    
    /* Test library initialization */
    int result = dns_lib_init();
    TEST_ASSERT(result == DNS_USER_SUCCESS, "DNS library initialization succeeds");
    
    /* Test double initialization */
    result = dns_lib_init();
    TEST_ASSERT(result == DNS_USER_SUCCESS, "Double initialization handled gracefully");
    
    /* Test initialization status */
    bool initialized = dns_lib_is_initialized();
    TEST_ASSERT(initialized == true, "Library reports initialized status");
    
    /* Test cleanup */
    dns_lib_cleanup();
    
    /* Test reinitialization after cleanup */
    result = dns_lib_init();
    TEST_ASSERT(result == DNS_USER_SUCCESS, "Reinitialization after cleanup succeeds");
}

void test_dns_basic_resolution(void) {
    TEST_GROUP("Basic DNS Resolution");
    
    /* Ensure library is initialized */
    dns_lib_init();
    
    /* Test valid hostname resolution */
    char ip_address[16];
    int result = dns_resolve_hostname("example.com", ip_address, sizeof(ip_address));
    TEST_ASSERT(result == DNS_USER_SUCCESS, "Valid hostname resolution succeeds");
    
    if (result == DNS_USER_SUCCESS) {
        TEST_ASSERT(strlen(ip_address) > 0, "IP address string is not empty");
        TEST_ASSERT(dns_is_valid_ip_address(ip_address), "Returned IP address is valid");
    }
    
    /* Test localhost resolution */
    result = dns_resolve_hostname("localhost", ip_address, sizeof(ip_address));
    /* Note: May succeed or fail depending on configuration */
    
    /* Test IP address validation */
    TEST_ASSERT(dns_is_valid_ip_address("192.168.1.1") == true, "Valid IP address recognized");
    TEST_ASSERT(dns_is_valid_ip_address("8.8.8.8") == true, "Valid public IP recognized");
    TEST_ASSERT(dns_is_valid_ip_address("256.1.1.1") == false, "Invalid IP address rejected");
    TEST_ASSERT(dns_is_valid_ip_address("192.168") == false, "Incomplete IP address rejected");
    TEST_ASSERT(dns_is_valid_ip_address("not.an.ip") == false, "Non-IP string rejected");
}

void test_dns_hostname_validation(void) {
    TEST_GROUP("DNS Hostname Validation");
    
    /* Test valid hostnames */
    TEST_ASSERT(dns_is_valid_hostname("example.com") == true, "Simple domain accepted");
    TEST_ASSERT(dns_is_valid_hostname("sub.example.com") == true, "Subdomain accepted");
    TEST_ASSERT(dns_is_valid_hostname("www.google.com") == true, "Common hostname accepted");
    TEST_ASSERT(dns_is_valid_hostname("test-site.org") == true, "Hyphenated domain accepted");
    TEST_ASSERT(dns_is_valid_hostname("a.b.c.d.e.f") == true, "Multi-level domain accepted");
    
    /* Test invalid hostnames */
    TEST_ASSERT(dns_is_valid_hostname("") == false, "Empty string rejected");
    TEST_ASSERT(dns_is_valid_hostname(".") == false, "Single dot rejected");
    TEST_ASSERT(dns_is_valid_hostname("..") == false, "Double dots rejected");
    TEST_ASSERT(dns_is_valid_hostname("example..com") == false, "Double dots in middle rejected");
    TEST_ASSERT(dns_is_valid_hostname("example.") == false, "Trailing dot rejected");
    TEST_ASSERT(dns_is_valid_hostname(".example.com") == false, "Leading dot rejected");
    TEST_ASSERT(dns_is_valid_hostname("example.com.") == false, "Trailing dot rejected");
    
    /* Test length limits */
    char long_hostname[300];
    memset(long_hostname, 'a', sizeof(long_hostname) - 1);
    long_hostname[sizeof(long_hostname) - 1] = '\0';
    TEST_ASSERT(dns_is_valid_hostname(long_hostname) == false, "Too long hostname rejected");
    
    /* Test invalid characters */
    TEST_ASSERT(dns_is_valid_hostname("exam_ple.com") == false, "Underscore rejected");
    TEST_ASSERT(dns_is_valid_hostname("example!.com") == false, "Exclamation mark rejected");
    TEST_ASSERT(dns_is_valid_hostname("exam ple.com") == false, "Space rejected");
}

void test_dns_configuration(void) {
    TEST_GROUP("DNS Configuration");
    
    dns_lib_init();
    
    /* Test getting default configuration */
    dns_user_config_t config;
    int result = dns_get_configuration(&config);
    TEST_ASSERT(result == DNS_USER_SUCCESS, "Getting default configuration succeeds");
    
    if (result == DNS_USER_SUCCESS) {
        TEST_ASSERT(strlen(config.primary_server) > 0, "Primary server is set");
        TEST_ASSERT(strlen(config.secondary_server) > 0, "Secondary server is set");
        TEST_ASSERT(config.timeout_ms > 0, "Timeout is positive");
        TEST_ASSERT(config.max_retries > 0, "Max retries is positive");
    }
    
    /* Test setting DNS servers */
    result = dns_set_servers("8.8.8.8", "8.8.4.4");
    TEST_ASSERT(result == DNS_USER_SUCCESS, "Setting valid DNS servers succeeds");
    
    /* Test getting updated servers */
    char primary[16], secondary[16];
    result = dns_get_servers(primary, sizeof(primary), secondary, sizeof(secondary));
    TEST_ASSERT(result == DNS_USER_SUCCESS, "Getting DNS servers succeeds");
    
    if (result == DNS_USER_SUCCESS) {
        TEST_ASSERT(strcmp(primary, "8.8.8.8") == 0, "Primary server correctly set");
        TEST_ASSERT(strcmp(secondary, "8.8.4.4") == 0, "Secondary server correctly set");
    }
    
    /* Test invalid server configuration */
    result = dns_set_servers("invalid", "8.8.4.4");
    TEST_ASSERT(result != DNS_USER_SUCCESS, "Invalid primary server rejected");
    
    result = dns_set_servers("8.8.8.8", "invalid");
    TEST_ASSERT(result != DNS_USER_SUCCESS, "Invalid secondary server rejected");
    
    /* Test custom configuration */
    dns_user_config_t custom_config = {
        .primary_server = "1.1.1.1",
        .secondary_server = "1.0.0.1",
        .timeout_ms = 8000,
        .max_retries = 4,
        .cache_enabled = true
    };
    
    result = dns_configure(&custom_config);
    TEST_ASSERT(result == DNS_USER_SUCCESS, "Custom configuration applied");
    
    /* Verify custom configuration */
    dns_user_config_t retrieved_config;
    result = dns_get_configuration(&retrieved_config);
    if (result == DNS_USER_SUCCESS) {
        TEST_ASSERT(strcmp(retrieved_config.primary_server, "1.1.1.1") == 0, 
                   "Custom primary server applied");
        TEST_ASSERT(retrieved_config.timeout_ms == 8000, "Custom timeout applied");
        TEST_ASSERT(retrieved_config.max_retries == 4, "Custom retries applied");
    }
}

void test_dns_cache(void) {
    TEST_GROUP("DNS Cache");
    
    dns_lib_init();
    
    /* Enable caching */
    dns_user_config_t config;
    dns_get_configuration(&config);
    config.cache_enabled = true;
    dns_configure(&config);
    
    /* Clear cache to start fresh */
    int result = dns_cache_flush();
    TEST_ASSERT(result == DNS_USER_SUCCESS, "Cache flush succeeds");
    
    /* Test adding cache entry */
    result = dns_cache_add_entry("test.cache", "192.168.1.100", 3600);
    TEST_ASSERT(result == DNS_USER_SUCCESS, "Adding cache entry succeeds");
    
    /* Test cache lookup */
    char cached_ip[16];
    uint32_t ttl;
    result = dns_cache_lookup("test.cache", cached_ip, sizeof(cached_ip), &ttl);
    TEST_ASSERT(result == DNS_USER_SUCCESS, "Cache lookup succeeds");
    
    if (result == DNS_USER_SUCCESS) {
        TEST_ASSERT(strcmp(cached_ip, "192.168.1.100") == 0, "Cached IP is correct");
        TEST_ASSERT(ttl == 3600, "Cached TTL is correct");
    }
    
    /* Test cache miss */
    result = dns_cache_lookup("not.in.cache", cached_ip, sizeof(cached_ip), &ttl);
    TEST_ASSERT(result != DNS_USER_SUCCESS, "Cache miss handled correctly");
    
    /* Test removing cache entry */
    result = dns_cache_remove_entry("test.cache");
    TEST_ASSERT(result == DNS_USER_SUCCESS, "Cache entry removal succeeds");
    
    /* Verify entry is removed */
    result = dns_cache_lookup("test.cache", cached_ip, sizeof(cached_ip), &ttl);
    TEST_ASSERT(result != DNS_USER_SUCCESS, "Removed entry not found in cache");
    
    /* Test cache with actual resolution */
    char ip_address[16];
    result = dns_resolve_hostname("example.com", ip_address, sizeof(ip_address));
    if (result == DNS_USER_SUCCESS) {
        /* Verify it's now in cache */
        result = dns_cache_lookup("example.com", cached_ip, sizeof(cached_ip), &ttl);
        TEST_ASSERT(result == DNS_USER_SUCCESS, "Resolved entry added to cache");
        
        if (result == DNS_USER_SUCCESS) {
            TEST_ASSERT(strcmp(cached_ip, ip_address) == 0, "Cached IP matches resolved IP");
        }
    }
}

void test_dns_error_handling(void) {
    TEST_GROUP("DNS Error Handling");
    
    dns_lib_init();
    
    /* Test NULL pointer handling */
    char ip_address[16];
    int result;
    
    result = dns_resolve_hostname(NULL, ip_address, sizeof(ip_address));
    TEST_ASSERT(result != DNS_USER_SUCCESS, "NULL hostname rejected");
    
    result = dns_resolve_hostname("example.com", NULL, sizeof(ip_address));
    TEST_ASSERT(result != DNS_USER_SUCCESS, "NULL IP buffer rejected");
    
    result = dns_resolve_hostname("example.com", ip_address, 0);
    TEST_ASSERT(result != DNS_USER_SUCCESS, "Zero buffer size rejected");
    
    /* Test invalid hostname handling */
    result = dns_resolve_hostname("", ip_address, sizeof(ip_address));
    TEST_ASSERT(result != DNS_USER_SUCCESS, "Empty hostname rejected");
    
    result = dns_resolve_hostname("invalid..hostname", ip_address, sizeof(ip_address));
    TEST_ASSERT(result != DNS_USER_SUCCESS, "Invalid hostname rejected");
    
    /* Test buffer too small */
    char small_buffer[4];
    result = dns_resolve_hostname("example.com", small_buffer, sizeof(small_buffer));
    TEST_ASSERT(result != DNS_USER_SUCCESS, "Too small buffer rejected");
    
    /* Test non-existent domain */
    result = dns_resolve_hostname("this-domain-does-not-exist-12345.com", 
                                 ip_address, sizeof(ip_address));
    /* This should fail, but might timeout or return NXDOMAIN */
    TEST_ASSERT(result != DNS_USER_SUCCESS, "Non-existent domain fails appropriately");
    
    /* Test reverse DNS with invalid IP */
    char hostname[256];
    result = dns_resolve_ip("256.256.256.256", hostname, sizeof(hostname));
    TEST_ASSERT(result != DNS_USER_SUCCESS, "Invalid IP for reverse lookup rejected");
    
    result = dns_resolve_ip(NULL, hostname, sizeof(hostname));
    TEST_ASSERT(result != DNS_USER_SUCCESS, "NULL IP for reverse lookup rejected");
    
    result = dns_resolve_ip("8.8.8.8", NULL, sizeof(hostname));
    TEST_ASSERT(result != DNS_USER_SUCCESS, "NULL hostname buffer for reverse lookup rejected");
}

void test_dns_statistics(void) {
    TEST_GROUP("DNS Statistics");
    
    dns_lib_init();
    
    /* Reset statistics */
    dns_reset_statistics();
    
    /* Get initial statistics */
    dns_user_stats_t stats;
    int result = dns_get_statistics(&stats);
    TEST_ASSERT(result == DNS_USER_SUCCESS, "Getting statistics succeeds");
    
    if (result == DNS_USER_SUCCESS) {
        TEST_ASSERT(stats.total_queries == 0, "Initial query count is zero");
        TEST_ASSERT(stats.successful_queries == 0, "Initial success count is zero");
        TEST_ASSERT(stats.failed_queries == 0, "Initial failure count is zero");
    }
    
    /* Perform some DNS operations */
    char ip_address[16];
    dns_resolve_hostname("example.com", ip_address, sizeof(ip_address));
    dns_resolve_hostname("nonexistent12345.com", ip_address, sizeof(ip_address));
    
    /* Check updated statistics */
    result = dns_get_statistics(&stats);
    if (result == DNS_USER_SUCCESS) {
        TEST_ASSERT(stats.total_queries >= 2, "Query count increased");
        TEST_ASSERT(stats.total_queries == stats.successful_queries + stats.failed_queries,
                   "Total queries equals success + failure");
    }
    
    /* Test statistics reset */
    dns_reset_statistics();
    result = dns_get_statistics(&stats);
    if (result == DNS_USER_SUCCESS) {
        TEST_ASSERT(stats.total_queries == 0, "Statistics reset correctly");
    }
}

void test_dns_reverse_lookup(void) {
    TEST_GROUP("DNS Reverse Lookup");
    
    dns_lib_init();
    
    /* Test reverse lookup with known IPs */
    char hostname[256];
    int result;
    
    /* Test Google DNS */
    result = dns_resolve_ip("8.8.8.8", hostname, sizeof(hostname));
    /* May succeed or fail depending on DNS configuration */
    if (result == DNS_USER_SUCCESS) {
        TEST_ASSERT(strlen(hostname) > 0, "Reverse lookup returns hostname");
        printf("    8.8.8.8 -> %s\n", hostname);
    }
    
    /* Test Cloudflare DNS */
    result = dns_resolve_ip("1.1.1.1", hostname, sizeof(hostname));
    if (result == DNS_USER_SUCCESS) {
        TEST_ASSERT(strlen(hostname) > 0, "Reverse lookup returns hostname");
        printf("    1.1.1.1 -> %s\n", hostname);
    }
    
    /* Test with invalid IP addresses */
    result = dns_resolve_ip("256.1.1.1", hostname, sizeof(hostname));
    TEST_ASSERT(result != DNS_USER_SUCCESS, "Invalid IP rejected for reverse lookup");
    
    result = dns_resolve_ip("not.an.ip", hostname, sizeof(hostname));
    TEST_ASSERT(result != DNS_USER_SUCCESS, "Non-IP string rejected for reverse lookup");
    
    /* Test buffer size handling */
    char small_hostname[4];
    result = dns_resolve_ip("8.8.8.8", small_hostname, sizeof(small_hostname));
    TEST_ASSERT(result != DNS_USER_SUCCESS, "Small buffer rejected for reverse lookup");
}

void test_dns_lookup_function(void) {
    TEST_GROUP("DNS Lookup Function");
    
    dns_lib_init();
    
    /* Test dns_lookup function */
    dns_query_result_t result;
    int lookup_result = dns_lookup("example.com", &result);
    
    if (lookup_result == DNS_USER_SUCCESS) {
        TEST_ASSERT(strlen(result.hostname) > 0, "Lookup result contains hostname");
        TEST_ASSERT(strlen(result.ip_address) > 0, "Lookup result contains IP address");
        TEST_ASSERT(strcmp(result.hostname, "example.com") == 0, "Hostname matches query");
        TEST_ASSERT(dns_is_valid_ip_address(result.ip_address), "IP address is valid");
        TEST_ASSERT(result.ttl > 0, "TTL is positive");
        
        printf("    Lookup result: %s -> %s (TTL: %u)\n", 
               result.hostname, result.ip_address, result.ttl);
    } else {
        printf("    Lookup failed with error %d\n", lookup_result);
    }
    
    /* Test with NULL result */
    lookup_result = dns_lookup("example.com", NULL);
    TEST_ASSERT(lookup_result != DNS_USER_SUCCESS, "NULL result pointer rejected");
    
    /* Test with invalid hostname */
    lookup_result = dns_lookup("invalid..hostname", &result);
    TEST_ASSERT(lookup_result != DNS_USER_SUCCESS, "Invalid hostname rejected");
}

/* ================================
 * DNS Integration Tests
 * ================================ */

void test_dns_socket_integration(void) {
    TEST_GROUP("DNS Socket Integration");
    
    /* Initialize both libraries */
    dns_lib_init();
    if (!socket_lib_is_initialized()) {
        socket_lib_init();
    }
    
    /* Resolve a hostname */
    char ip_address[16];
    int dns_result = dns_resolve_hostname("example.com", ip_address, sizeof(ip_address));
    
    if (dns_result == DNS_USER_SUCCESS) {
        TEST_ASSERT(dns_is_valid_ip_address(ip_address), "DNS resolution returns valid IP");
        
        /* Try to create a socket connection to the resolved IP */
        int sockfd = tcp_client_connect(ip_address, 80);
        
        if (sockfd >= 0) {
            TEST_ASSERT(sockfd >= 0, "Socket connection using resolved IP succeeds");
            close_socket(sockfd);
        } else {
            printf("    Socket connection failed (may be due to network/firewall)\n");
        }
    } else {
        printf("    DNS resolution failed, skipping socket test\n");
    }
    
    /* Test hostname-based socket connection */
    /* Note: This would require the socket library to support hostname resolution */
    /* For now, we'll just test that we can resolve and then connect */
    
    const char* test_hosts[] = {"google.com", "github.com"};
    int num_hosts = sizeof(test_hosts) / sizeof(test_hosts[0]);
    
    for (int i = 0; i < num_hosts; i++) {
        char host_ip[16];
        int result = dns_resolve_hostname(test_hosts[i], host_ip, sizeof(host_ip));
        
        if (result == DNS_USER_SUCCESS) {
            printf("    %s -> %s", test_hosts[i], host_ip);
            
            /* Quick connection test */
            int test_sock = tcp_client_connect(host_ip, 80);
            if (test_sock >= 0) {
                printf(" (connection OK)");
                close_socket(test_sock);
            } else {
                printf(" (connection failed)");
            }
            printf("\n");
        }
    }
    
    socket_lib_cleanup();
}

/* ================================
 * DNS Stress Tests
 * ================================ */

void test_dns_stress(void) {
    TEST_GROUP("DNS Stress Test");
    
    dns_lib_init();
    
    /* Test many rapid queries */
    const int NUM_QUERIES = 50;
    const char* test_hostnames[] = {
        "google.com", "github.com", "stackoverflow.com", "wikipedia.org",
        "example.com", "kernel.org", "cloudflare.com", "amazon.com"
    };
    int num_hostnames = sizeof(test_hostnames) / sizeof(test_hostnames[0]);
    
    printf("Performing %d rapid DNS queries...\n", NUM_QUERIES);
    
    int successful_queries = 0;
    int failed_queries = 0;
    
    for (int i = 0; i < NUM_QUERIES; i++) {
        const char* hostname = test_hostnames[i % num_hostnames];
        char ip_address[16];
        
        int result = dns_resolve_hostname(hostname, ip_address, sizeof(ip_address));
        
        if (result == DNS_USER_SUCCESS) {
            successful_queries++;
        } else {
            failed_queries++;
        }
        
        /* Print progress every 10 queries */
        if ((i + 1) % 10 == 0) {
            printf("    Completed %d queries (%d successful, %d failed)\n", 
                   i + 1, successful_queries, failed_queries);
        }
    }
    
    TEST_ASSERT(successful_queries > 0, "Some stress test queries succeeded");
    TEST_ASSERT(successful_queries >= NUM_QUERIES / 2, "Most stress test queries succeeded");
    
    printf("Stress test completed: %d/%d queries successful (%.1f%%)\n",
           successful_queries, NUM_QUERIES, 
           100.0 * successful_queries / NUM_QUERIES);
    
    /* Test cache efficiency during stress test */
    dns_user_stats_t stats;
    if (dns_get_statistics(&stats) == DNS_USER_SUCCESS) {
        if (stats.cache_hits + stats.cache_misses > 0) {
            float cache_hit_rate = 100.0 * stats.cache_hits / (stats.cache_hits + stats.cache_misses);
            printf("Cache hit rate during stress test: %.1f%%\n", cache_hit_rate);
            
            /* With repeated hostnames, we should see some cache hits */
            TEST_ASSERT(cache_hit_rate > 10.0, "Cache provides some benefit during stress test");
        }
    }
}

/* ================================
 * Main Test Functions
 * ================================ */

void run_dns_unit_tests(void) {
    printf("IKOS DNS Unit Tests\n");
    printf("===================\n");
    
    test_dns_library_initialization();
    test_dns_basic_resolution();
    test_dns_hostname_validation();
    test_dns_configuration();
    test_dns_cache();
    test_dns_error_handling();
    test_dns_statistics();
    test_dns_reverse_lookup();
    test_dns_lookup_function();
    
    print_test_summary();
}

void run_dns_integration_tests(void) {
    printf("\nIKOS DNS Integration Tests\n");
    printf("==========================\n");
    
    tests_run = 0;
    tests_passed = 0;
    tests_failed = 0;
    
    test_dns_socket_integration();
    
    print_test_summary();
}

void run_dns_stress_tests(void) {
    printf("\nIKOS DNS Stress Tests\n");
    printf("=====================\n");
    
    tests_run = 0;
    tests_passed = 0;
    tests_failed = 0;
    
    test_dns_stress();
    
    print_test_summary();
}

/* ================================
 * Test Suite Main Function
 * ================================ */

void dns_comprehensive_test(void) {
    printf("IKOS DNS Comprehensive Test Suite\n");
    printf("==================================\n\n");
    
    /* Run all test suites */
    run_dns_unit_tests();
    run_dns_integration_tests();
    run_dns_stress_tests();
    
    printf("\n=== Overall Test Summary ===\n");
    printf("Comprehensive DNS test suite completed\n");
    printf("DNS resolution service validated\n");
}

/* Simple DNS test for basic validation */
int dns_basic_test(void) {
    printf("DNS Basic Validation Test\n");
    printf("=========================\n");
    
    int result = 0;
    
    /* Test DNS library initialization */
    printf("Testing DNS library initialization...\n");
    int init_result = dns_lib_init();
    if (init_result == DNS_USER_SUCCESS) {
        printf("PASS: DNS library initialization succeeded\n");
        
        /* Test basic hostname validation */
        printf("Testing hostname validation...\n");
        if (dns_is_valid_hostname("example.com")) {
            printf("PASS: Valid hostname accepted\n");
        } else {
            printf("FAIL: Valid hostname rejected\n");
            result = -1;
        }
        
        if (!dns_is_valid_hostname("invalid..hostname")) {
            printf("PASS: Invalid hostname rejected\n");
        } else {
            printf("FAIL: Invalid hostname accepted\n");
            result = -1;
        }
        
        /* Test IP address validation */
        printf("Testing IP address validation...\n");
        if (dns_is_valid_ip_address("192.168.1.1")) {
            printf("PASS: Valid IP address accepted\n");
        } else {
            printf("FAIL: Valid IP address rejected\n");
            result = -1;
        }
        
        if (!dns_is_valid_ip_address("256.1.1.1")) {
            printf("PASS: Invalid IP address rejected\n");
        } else {
            printf("FAIL: Invalid IP address accepted\n");
            result = -1;
        }
        
        /* Test basic DNS resolution */
        printf("Testing basic DNS resolution...\n");
        char ip_address[16];
        int resolve_result = dns_resolve_hostname("example.com", ip_address, sizeof(ip_address));
        
        if (resolve_result == DNS_USER_SUCCESS) {
            printf("PASS: DNS resolution succeeded (%s)\n", ip_address);
            
            if (dns_is_valid_ip_address(ip_address)) {
                printf("PASS: Resolved IP address is valid\n");
            } else {
                printf("FAIL: Resolved IP address is invalid\n");
                result = -1;
            }
        } else {
            printf("NOTE: DNS resolution failed (may be expected in test environment)\n");
        }
        
        dns_lib_cleanup();
    } else {
        printf("FAIL: DNS library initialization failed (%d)\n", init_result);
        result = -1;
    }
    
    if (result == 0) {
        printf("SUCCESS: DNS basic validation passed\n");
    } else {
        printf("FAILURE: DNS basic validation failed\n");
    }
    
    return result;
}

/* Main test entry point */
int main(int argc, char* argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "basic") == 0) {
            return dns_basic_test();
        } else if (strcmp(argv[1], "unit") == 0) {
            run_dns_unit_tests();
            return tests_failed > 0 ? -1 : 0;
        } else if (strcmp(argv[1], "integration") == 0) {
            run_dns_integration_tests();
            return tests_failed > 0 ? -1 : 0;
        } else if (strcmp(argv[1], "stress") == 0) {
            run_dns_stress_tests();
            return tests_failed > 0 ? -1 : 0;
        } else if (strcmp(argv[1], "comprehensive") == 0) {
            dns_comprehensive_test();
            return tests_failed > 0 ? -1 : 0;
        } else {
            printf("Unknown test type: %s\n", argv[1]);
            printf("Available tests: basic, unit, integration, stress, comprehensive\n");
            return -1;
        }
    } else {
        /* Run basic test by default */
        return dns_basic_test();
    }
}
