/* IKOS Advanced Memory Management Test Suite - Issue #27
 * Comprehensive testing for advanced memory management features
 */

#include "memory_advanced.h"
#include "advanced_memory_manager.h"
#include "buddy_allocator.h"
#include "slab_allocator.h"
#include "numa_allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

/* Test configuration */
#define TEST_ITERATIONS         1000
#define TEST_MAX_ALLOC_SIZE     (1024 * 1024)  /* 1MB */
#define TEST_NUM_CACHES         10
#define TEST_STRESS_DURATION    10000           /* 10 seconds */

/* Test statistics */
typedef struct test_stats {
    uint32_t    tests_run;
    uint32_t    tests_passed;
    uint32_t    tests_failed;
    uint64_t    total_time;
    uint64_t    start_time;
} test_stats_t;

static test_stats_t g_test_stats = {0};

/* Test framework macros */
#define TEST_START(name) \
    do { \
        printf("Running test: %s\n", name); \
        g_test_stats.tests_run++; \
        g_test_stats.start_time = get_test_time(); \
    } while(0)

#define TEST_END() \
    do { \
        uint64_t elapsed = get_test_time() - g_test_stats.start_time; \
        g_test_stats.total_time += elapsed; \
        printf("  Test completed in %lu ms\n", elapsed); \
    } while(0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("  FAIL: %s\n", message); \
            g_test_stats.tests_failed++; \
            return false; \
        } \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("  PASS\n"); \
        g_test_stats.tests_passed++; \
        return true; \
    } while(0)

/* ========================== Helper Functions ========================== */

/**
 * Get current time in milliseconds
 */
static uint64_t get_test_time(void) {
    /* TODO: Implement actual time reading */
    static uint64_t counter = 0;
    return ++counter;
}

/**
 * Generate random size for allocations
 */
static size_t get_random_size(size_t min_size, size_t max_size) {
    if (min_size >= max_size) {
        return min_size;
    }
    
    /* Simple pseudo-random generator */
    static uint32_t seed = 12345;
    seed = seed * 1103515245 + 12345;
    
    size_t range = max_size - min_size;
    return min_size + (seed % range);
}

/**
 * Fill memory with test pattern
 */
static void fill_test_pattern(void* ptr, size_t size, uint8_t pattern) {
    uint8_t* bytes = (uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) {
        bytes[i] = pattern ^ (i & 0xFF);
    }
}

/**
 * Verify test pattern
 */
static bool verify_test_pattern(void* ptr, size_t size, uint8_t pattern) {
    uint8_t* bytes = (uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != (pattern ^ (i & 0xFF))) {
            return false;
        }
    }
    return true;
}

/* ========================== Basic Memory Manager Tests ========================== */

/**
 * Test memory manager initialization
 */
static bool test_memory_manager_init(void) {
    TEST_START("Memory Manager Initialization");
    
    /* Test initialization */
    int result = advanced_memory_manager_init();
    TEST_ASSERT(result == 0, "Memory manager initialization failed");
    
    /* Test state */
    memory_state_t state = memory_get_state();
    TEST_ASSERT(state == MEMORY_STATE_RUNNING || state == MEMORY_STATE_DEGRADED,
                "Invalid memory manager state after initialization");
    
    TEST_END();
    TEST_PASS();
}

/**
 * Test basic memory allocation and deallocation
 */
static bool test_basic_allocation(void) {
    TEST_START("Basic Memory Allocation");
    
    /* Test small allocation */
    void* ptr1 = memory_alloc(64, GFP_KERNEL);
    TEST_ASSERT(ptr1 != NULL, "Small allocation failed");
    
    /* Test pattern write and verify */
    fill_test_pattern(ptr1, 64, 0xAA);
    TEST_ASSERT(verify_test_pattern(ptr1, 64, 0xAA), "Memory corruption detected");
    
    /* Test large allocation */
    void* ptr2 = memory_alloc(PAGE_SIZE * 4, GFP_KERNEL);
    TEST_ASSERT(ptr2 != NULL, "Large allocation failed");
    
    /* Test pattern write and verify */
    fill_test_pattern(ptr2, PAGE_SIZE * 4, 0x55);
    TEST_ASSERT(verify_test_pattern(ptr2, PAGE_SIZE * 4, 0x55), "Memory corruption detected");
    
    /* Test deallocation */
    memory_free(ptr1, 64);
    memory_free(ptr2, PAGE_SIZE * 4);
    
    TEST_END();
    TEST_PASS();
}

/**
 * Test allocation alignment
 */
static bool test_allocation_alignment(void) {
    TEST_START("Memory Allocation Alignment");
    
    /* Test various alignments */
    size_t alignments[] = {8, 16, 32, 64, 128, 256, 512, 1024, 4096};
    int num_alignments = sizeof(alignments) / sizeof(alignments[0]);
    
    for (int i = 0; i < num_alignments; i++) {
        size_t align = alignments[i];
        void* ptr = memory_alloc(align * 2, GFP_KERNEL);
        TEST_ASSERT(ptr != NULL, "Aligned allocation failed");
        
        /* Check alignment */
        uintptr_t addr = (uintptr_t)ptr;
        TEST_ASSERT((addr & (align - 1)) == 0, "Allocation not properly aligned");
        
        memory_free(ptr, align * 2);
    }
    
    TEST_END();
    TEST_PASS();
}

/* ========================== Buddy Allocator Tests ========================== */

/**
 * Test buddy allocator functionality
 */
static bool test_buddy_allocator(void) {
    TEST_START("Buddy Allocator");
    
    /* Test various orders */
    for (unsigned int order = 0; order <= 5; order++) {
        page_frame_t* pages = buddy_alloc_pages(order, GFP_KERNEL);
        TEST_ASSERT(pages != NULL, "Buddy allocation failed");
        
        /* Test page access */
        void* ptr = (void*)(pages->pfn * PAGE_SIZE);
        fill_test_pattern(ptr, PAGE_SIZE * (1 << order), 0xCC);
        TEST_ASSERT(verify_test_pattern(ptr, PAGE_SIZE * (1 << order), 0xCC),
                    "Buddy allocated memory corruption");
        
        buddy_free_pages(pages, order);
    }
    
    TEST_END();
    TEST_PASS();
}

/**
 * Test buddy allocator fragmentation
 */
static bool test_buddy_fragmentation(void) {
    TEST_START("Buddy Allocator Fragmentation");
    
    /* Allocate many small blocks to create fragmentation */
    page_frame_t* pages[100];
    int allocated = 0;
    
    for (int i = 0; i < 100; i++) {
        pages[i] = buddy_alloc_pages(0, GFP_KERNEL);  /* Single pages */
        if (pages[i]) {
            allocated++;
        }
    }
    
    TEST_ASSERT(allocated > 50, "Failed to allocate sufficient pages for fragmentation test");
    
    /* Free every other page to create fragmentation */
    for (int i = 0; i < allocated; i += 2) {
        if (pages[i]) {
            buddy_free_pages(pages[i], 0);
            pages[i] = NULL;
        }
    }
    
    /* Try to allocate larger blocks */
    page_frame_t* large_page = buddy_alloc_pages(3, GFP_KERNEL);  /* 8 pages */
    if (large_page) {
        buddy_free_pages(large_page, 3);
    }
    
    /* Free remaining pages */
    for (int i = 1; i < allocated; i += 2) {
        if (pages[i]) {
            buddy_free_pages(pages[i], 0);
        }
    }
    
    TEST_END();
    TEST_PASS();
}

/* ========================== Slab Allocator Tests ========================== */

/**
 * Test slab cache creation and destruction
 */
static bool test_slab_cache_management(void) {
    TEST_START("Slab Cache Management");
    
    /* Create test cache */
    kmem_cache_t* cache = kmem_cache_create("test_cache", 128, 8, 0, NULL);
    TEST_ASSERT(cache != NULL, "Failed to create slab cache");
    
    /* Test cache properties */
    TEST_ASSERT(cache->object_size == 128, "Incorrect object size");
    TEST_ASSERT(cache->align == 8, "Incorrect alignment");
    TEST_ASSERT(strcmp(cache->name, "test_cache") == 0, "Incorrect cache name");
    
    /* TODO: Destroy cache when implemented */
    /* kmem_cache_destroy(cache); */
    
    TEST_END();
    TEST_PASS();
}

/**
 * Test slab object allocation and deallocation
 */
static bool test_slab_allocation(void) {
    TEST_START("Slab Object Allocation");
    
    /* Create test cache */
    kmem_cache_t* cache = kmem_cache_create("alloc_test", 256, 16, 0, NULL);
    TEST_ASSERT(cache != NULL, "Failed to create test cache");
    
    /* Allocate multiple objects */
    void* objects[50];
    int allocated = 0;
    
    for (int i = 0; i < 50; i++) {
        objects[i] = kmem_cache_alloc(cache, GFP_KERNEL);
        if (objects[i]) {
            allocated++;
            fill_test_pattern(objects[i], 256, 0xDD);
        }
    }
    
    TEST_ASSERT(allocated > 20, "Failed to allocate sufficient objects");
    
    /* Verify object integrity */
    for (int i = 0; i < allocated; i++) {
        if (objects[i]) {
            TEST_ASSERT(verify_test_pattern(objects[i], 256, 0xDD),
                        "Object memory corruption detected");
        }
    }
    
    /* Free objects */
    for (int i = 0; i < allocated; i++) {
        if (objects[i]) {
            kmem_cache_free(cache, objects[i]);
        }
    }
    
    TEST_END();
    TEST_PASS();
}

/* ========================== NUMA Allocator Tests ========================== */

/**
 * Test NUMA allocator initialization
 */
static bool test_numa_init(void) {
    TEST_START("NUMA Allocator Initialization");
    
    /* NUMA allocator should already be initialized by memory manager */
    /* Just test NUMA policy setting */
    int result = numa_set_policy(NUMA_POLICY_PREFERRED);
    TEST_ASSERT(result == 0 || result == -1, "NUMA policy setting returned unexpected result");
    
    numa_policy_t policy = numa_get_policy();
    TEST_ASSERT(policy >= NUMA_POLICY_DEFAULT && policy <= NUMA_POLICY_LOCAL,
                "Invalid NUMA policy returned");
    
    TEST_END();
    TEST_PASS();
}

/**
 * Test NUMA-aware allocation
 */
static bool test_numa_allocation(void) {
    TEST_START("NUMA-Aware Allocation");
    
    /* Test NUMA page allocation */
    page_frame_t* pages = numa_alloc_pages(2, GFP_KERNEL, NUMA_POLICY_PREFERRED);
    if (pages) {
        /* Test page access */
        void* ptr = (void*)(pages->pfn * PAGE_SIZE);
        fill_test_pattern(ptr, PAGE_SIZE * 4, 0xEE);
        TEST_ASSERT(verify_test_pattern(ptr, PAGE_SIZE * 4, 0xEE),
                    "NUMA allocated memory corruption");
        
        numa_free_pages(pages, 2);
    }
    
    /* Test interleave policy */
    page_frame_t* interleave_pages = numa_alloc_pages(1, GFP_KERNEL, NUMA_POLICY_INTERLEAVE);
    if (interleave_pages) {
        numa_free_pages(interleave_pages, 1);
    }
    
    TEST_END();
    TEST_PASS();
}

/* ========================== Memory Pool Tests ========================== */

/**
 * Test memory pool creation and management
 */
static bool test_memory_pools(void) {
    TEST_START("Memory Pool Management");
    
    /* Create test pool */
    int pool_id = memory_pool_create("test_pool", PAGE_SIZE * 16, MEMORY_POOL_CONTIGUOUS);
    TEST_ASSERT(pool_id >= 0, "Failed to create memory pool");
    
    /* Create another pool */
    int pool_id2 = memory_pool_create("test_pool2", PAGE_SIZE * 8, 0);
    TEST_ASSERT(pool_id2 >= 0, "Failed to create second memory pool");
    TEST_ASSERT(pool_id2 != pool_id, "Pool IDs should be different");
    
    /* Destroy pools */
    memory_pool_destroy(pool_id);
    memory_pool_destroy(pool_id2);
    
    TEST_END();
    TEST_PASS();
}

/* ========================== Stress Tests ========================== */

/**
 * Random allocation stress test
 */
static bool test_random_allocation_stress(void) {
    TEST_START("Random Allocation Stress Test");
    
    void* allocations[TEST_ITERATIONS];
    size_t sizes[TEST_ITERATIONS];
    int num_allocations = 0;
    
    /* Random allocation phase */
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        size_t size = get_random_size(16, TEST_MAX_ALLOC_SIZE);
        void* ptr = memory_alloc(size, GFP_KERNEL);
        
        if (ptr) {
            allocations[num_allocations] = ptr;
            sizes[num_allocations] = size;
            fill_test_pattern(ptr, size, (uint8_t)(i & 0xFF));
            num_allocations++;
        }
        
        /* Randomly free some allocations */
        if (num_allocations > 100 && (i % 17) == 0) {
            int free_idx = i % num_allocations;
            if (allocations[free_idx]) {
                TEST_ASSERT(verify_test_pattern(allocations[free_idx], sizes[free_idx], 
                                               (uint8_t)(free_idx & 0xFF)),
                           "Memory corruption during stress test");
                memory_free(allocations[free_idx], sizes[free_idx]);
                allocations[free_idx] = NULL;
            }
        }
    }
    
    TEST_ASSERT(num_allocations > TEST_ITERATIONS / 2, 
                "Too many allocation failures during stress test");
    
    /* Cleanup phase */
    for (int i = 0; i < num_allocations; i++) {
        if (allocations[i]) {
            memory_free(allocations[i], sizes[i]);
        }
    }
    
    TEST_END();
    TEST_PASS();
}

/**
 * Multi-cache stress test
 */
static bool test_multi_cache_stress(void) {
    TEST_START("Multi-Cache Stress Test");
    
    /* Create multiple caches */
    kmem_cache_t* caches[TEST_NUM_CACHES];
    size_t cache_sizes[] = {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
    
    for (int i = 0; i < TEST_NUM_CACHES; i++) {
        char cache_name[32];
        snprintf(cache_name, sizeof(cache_name), "stress_cache_%d", i);
        caches[i] = kmem_cache_create(cache_name, cache_sizes[i], 8, 0, NULL);
        TEST_ASSERT(caches[i] != NULL, "Failed to create stress test cache");
    }
    
    /* Allocate from multiple caches simultaneously */
    void* objects[TEST_NUM_CACHES][50];
    int allocated_counts[TEST_NUM_CACHES] = {0};
    
    for (int round = 0; round < 10; round++) {
        for (int cache_idx = 0; cache_idx < TEST_NUM_CACHES; cache_idx++) {
            for (int obj = 0; obj < 5; obj++) {
                int idx = allocated_counts[cache_idx];
                if (idx < 50) {
                    objects[cache_idx][idx] = kmem_cache_alloc(caches[cache_idx], GFP_KERNEL);
                    if (objects[cache_idx][idx]) {
                        fill_test_pattern(objects[cache_idx][idx], cache_sizes[cache_idx], 
                                         (uint8_t)(cache_idx + round));
                        allocated_counts[cache_idx]++;
                    }
                }
            }
        }
    }
    
    /* Verify and free objects */
    for (int cache_idx = 0; cache_idx < TEST_NUM_CACHES; cache_idx++) {
        for (int obj = 0; obj < allocated_counts[cache_idx]; obj++) {
            if (objects[cache_idx][obj]) {
                TEST_ASSERT(verify_test_pattern(objects[cache_idx][obj], cache_sizes[cache_idx],
                                               (uint8_t)(cache_idx + (obj / 5))),
                           "Multi-cache object corruption");
                kmem_cache_free(caches[cache_idx], objects[cache_idx][obj]);
            }
        }
    }
    
    TEST_END();
    TEST_PASS();
}

/* ========================== Integration Tests ========================== */

/**
 * Test memory manager integration
 */
static bool test_memory_manager_integration(void) {
    TEST_START("Memory Manager Integration");
    
    /* Test statistics collection */
    memory_print_stats();
    
    /* Test garbage collection */
    memory_gc();
    
    /* Test state checking */
    memory_state_t state = memory_get_state();
    TEST_ASSERT(state == MEMORY_STATE_RUNNING || state == MEMORY_STATE_DEGRADED,
                "Invalid memory manager state");
    
    /* Test mixed allocations */
    void* small_ptr = memory_alloc(128, GFP_KERNEL);
    void* large_ptr = memory_alloc(PAGE_SIZE * 8, GFP_KERNEL);
    
    if (small_ptr && large_ptr) {
        fill_test_pattern(small_ptr, 128, 0xFF);
        fill_test_pattern(large_ptr, PAGE_SIZE * 8, 0x00);
        
        TEST_ASSERT(verify_test_pattern(small_ptr, 128, 0xFF),
                    "Small allocation corruption");
        TEST_ASSERT(verify_test_pattern(large_ptr, PAGE_SIZE * 8, 0x00),
                    "Large allocation corruption");
        
        memory_free(small_ptr, 128);
        memory_free(large_ptr, PAGE_SIZE * 8);
    }
    
    TEST_END();
    TEST_PASS();
}

/* ========================== Test Suite Runner ========================== */

/**
 * Run all memory management tests
 */
static void run_all_tests(void) {
    printf("=== IKOS Advanced Memory Management Test Suite ===\n\n");
    
    /* Basic tests */
    test_memory_manager_init();
    test_basic_allocation();
    test_allocation_alignment();
    
    /* Subsystem tests */
    test_buddy_allocator();
    test_buddy_fragmentation();
    test_slab_cache_management();
    test_slab_allocation();
    test_numa_init();
    test_numa_allocation();
    test_memory_pools();
    
    /* Stress tests */
    test_random_allocation_stress();
    test_multi_cache_stress();
    
    /* Integration tests */
    test_memory_manager_integration();
}

/**
 * Print test results summary
 */
static void print_test_summary(void) {
    printf("\n=== Test Results Summary ===\n");
    printf("Tests run: %u\n", g_test_stats.tests_run);
    printf("Tests passed: %u\n", g_test_stats.tests_passed);
    printf("Tests failed: %u\n", g_test_stats.tests_failed);
    printf("Success rate: %.1f%%\n", 
           g_test_stats.tests_run > 0 ? 
           (double)g_test_stats.tests_passed * 100.0 / g_test_stats.tests_run : 0.0);
    printf("Total time: %lu ms\n", g_test_stats.total_time);
    
    if (g_test_stats.tests_failed == 0) {
        printf("All tests PASSED! ✓\n");
    } else {
        printf("Some tests FAILED! ✗\n");
    }
}

/**
 * Main test entry point
 */
int main(void) {
    printf("Starting IKOS Advanced Memory Management Tests...\n\n");
    
    /* Run test suite */
    run_all_tests();
    
    /* Print summary */
    print_test_summary();
    
    /* Cleanup */
    printf("\nShutting down memory manager...\n");
    advanced_memory_manager_shutdown();
    
    return (g_test_stats.tests_failed == 0) ? 0 : 1;
}

/* ========================== Placeholder Functions ========================== */

/* These would normally be provided by the kernel */
void* kmalloc(size_t size) {
    return malloc(size);  /* Use libc for testing */
}

void kfree(void* ptr) {
    free(ptr);  /* Use libc for testing */
}

page_frame_t* vmm_alloc_pages(size_t count, uint32_t flags) {
    (void)count;
    (void)flags;
    return NULL;  /* Not implemented for testing */
}

void vmm_free_pages(page_frame_t* pages, size_t count) {
    (void)pages;
    (void)count;
    /* Not implemented for testing */
}
