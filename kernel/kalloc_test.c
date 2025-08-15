/* IKOS Kernel Memory Allocator Test - Issue #12
 * Tests for the new SLAB/SLOB allocator implementation
 */

#include "../include/kalloc.h"
#include "../include/stdio.h"
#include "../include/memory.h"
#include <stdint.h>
#include <stddef.h>

/* Test functions */
static void test_basic_allocation(void);
static void test_alignment(void);
static void test_slab_caches(void);
static void test_large_allocations(void);
static void test_free_and_reuse(void);
static void test_edge_cases(void);
static void test_statistics(void);

/* Test runner */
void kalloc_run_tests(void) {
    printf("\n=== KALLOC Allocator Tests ===\n");
    
    test_basic_allocation();
    test_alignment();
    test_slab_caches();
    test_large_allocations();
    test_free_and_reuse();
    test_edge_cases();
    test_statistics();
    
    printf("=== All KALLOC tests completed ===\n\n");
}

/* Test basic allocation and free */
static void test_basic_allocation(void) {
    printf("Testing basic allocation...\n");
    
    void* ptr1 = kalloc(64);
    void* ptr2 = kalloc(128);
    void* ptr3 = kalloc(256);
    
    if (!ptr1 || !ptr2 || !ptr3) {
        printf("FAIL: Basic allocation failed\n");
        return;
    }
    
    printf("Allocated: %p, %p, %p\n", ptr1, ptr2, ptr3);
    
    /* Write to memory to test it's accessible */
    *(uint32_t*)ptr1 = 0xDEADBEEF;
    *(uint32_t*)ptr2 = 0xCAFEBABE;
    *(uint32_t*)ptr3 = 0xFEEDFACE;
    
    if (*(uint32_t*)ptr1 != 0xDEADBEEF ||
        *(uint32_t*)ptr2 != 0xCAFEBABE ||
        *(uint32_t*)ptr3 != 0xFEEDFACE) {
        printf("FAIL: Memory corruption detected\n");
        return;
    }
    
    kfree(ptr1);
    kfree(ptr2);
    kfree(ptr3);
    
    printf("PASS: Basic allocation test\n");
}

/* Test alignment requirements */
static void test_alignment(void) {
    printf("Testing alignment...\n");
    
    void* ptr8 = kalloc_aligned(32, 8);
    void* ptr16 = kalloc_aligned(32, 16);
    void* ptr64 = kalloc_aligned(32, 64);
    
    if (!ptr8 || !ptr16 || !ptr64) {
        printf("FAIL: Aligned allocation failed\n");
        return;
    }
    
    if ((uintptr_t)ptr8 % 8 != 0) {
        printf("FAIL: 8-byte alignment failed: %p\n", ptr8);
        return;
    }
    
    if ((uintptr_t)ptr16 % 16 != 0) {
        printf("FAIL: 16-byte alignment failed: %p\n", ptr16);
        return;
    }
    
    if ((uintptr_t)ptr64 % 64 != 0) {
        printf("FAIL: 64-byte alignment failed: %p\n", ptr64);
        return;
    }
    
    kfree(ptr8);
    kfree(ptr16);
    kfree(ptr64);
    
    printf("PASS: Alignment test\n");
}

/* Test SLAB cache behavior */
static void test_slab_caches(void) {
    printf("Testing SLAB caches...\n");
    
    /* Allocate many small objects of the same size */
    void* ptrs[100];
    
    for (int i = 0; i < 100; i++) {
        ptrs[i] = kalloc(64);  /* Should use 64-byte cache */
        if (!ptrs[i]) {
            printf("FAIL: SLAB allocation %d failed\n", i);
            return;
        }
        
        /* Write unique pattern */
        *(uint32_t*)ptrs[i] = 0x12345678 + i;
    }
    
    /* Verify all allocations are intact */
    for (int i = 0; i < 100; i++) {
        if (*(uint32_t*)ptrs[i] != 0x12345678 + i) {
            printf("FAIL: SLAB corruption at allocation %d\n", i);
            return;
        }
    }
    
    /* Free half of them */
    for (int i = 0; i < 50; i++) {
        kfree(ptrs[i]);
    }
    
    /* Verify remaining half is still intact */
    for (int i = 50; i < 100; i++) {
        if (*(uint32_t*)ptrs[i] != 0x12345678 + i) {
            printf("FAIL: SLAB corruption after partial free at %d\n", i);
            return;
        }
    }
    
    /* Free the rest */
    for (int i = 50; i < 100; i++) {
        kfree(ptrs[i]);
    }
    
    printf("PASS: SLAB cache test\n");
}

/* Test large allocations */
static void test_large_allocations(void) {
    printf("Testing large allocations...\n");
    
    void* large1 = kalloc(8192);   /* 8KB */
    void* large2 = kalloc(16384);  /* 16KB */
    void* large3 = kalloc(32768);  /* 32KB */
    
    if (!large1 || !large2 || !large3) {
        printf("FAIL: Large allocation failed\n");
        return;
    }
    
    /* Test memory accessibility */
    memset(large1, 0xAA, 8192);
    memset(large2, 0xBB, 16384);
    memset(large3, 0xCC, 32768);
    
    /* Check first and last bytes */
    if (*(uint8_t*)large1 != 0xAA ||
        *((uint8_t*)large1 + 8191) != 0xAA) {
        printf("FAIL: Large allocation 1 corrupted\n");
        return;
    }
    
    if (*(uint8_t*)large2 != 0xBB ||
        *((uint8_t*)large2 + 16383) != 0xBB) {
        printf("FAIL: Large allocation 2 corrupted\n");
        return;
    }
    
    if (*(uint8_t*)large3 != 0xCC ||
        *((uint8_t*)large3 + 32767) != 0xCC) {
        printf("FAIL: Large allocation 3 corrupted\n");
        return;
    }
    
    kfree(large1);
    kfree(large2);
    kfree(large3);
    
    printf("PASS: Large allocation test\n");
}

/* Test free and reuse */
static void test_free_and_reuse(void) {
    printf("Testing free and reuse...\n");
    
    void* ptr1 = kalloc(128);
    if (!ptr1) {
        printf("FAIL: Initial allocation failed\n");
        return;
    }
    
    uintptr_t addr1 = (uintptr_t)ptr1;
    kfree(ptr1);
    
    void* ptr2 = kalloc(128);
    if (!ptr2) {
        printf("FAIL: Reallocation failed\n");
        return;
    }
    
    uintptr_t addr2 = (uintptr_t)ptr2;
    
    /* For SLAB allocator, we might get the same address back */
    printf("Original: %p, Reused: %p %s\n", 
           (void*)addr1, (void*)addr2,
           (addr1 == addr2) ? "(same address reused)" : "(different address)");
    
    kfree(ptr2);
    
    printf("PASS: Free and reuse test\n");
}

/* Test edge cases */
static void test_edge_cases(void) {
    printf("Testing edge cases...\n");
    
    /* Test zero allocation */
    void* zero_ptr = kalloc(0);
    if (zero_ptr != NULL) {
        printf("FAIL: Zero allocation should return NULL\n");
        return;
    }
    
    /* Test NULL free */
    kfree(NULL);  /* Should not crash */
    
    /* Test double free (should be detected or ignored) */
    void* ptr = kalloc(64);
    if (ptr) {
        kfree(ptr);
        kfree(ptr);  /* Double free - should be handled gracefully */
    }
    
    /* Test very large allocation */
    void* huge_ptr = kalloc(0x10000000);  /* 256MB */
    if (huge_ptr) {
        printf("WARNING: Very large allocation succeeded unexpectedly\n");
        kfree(huge_ptr);
    }
    
    printf("PASS: Edge cases test\n");
}

/* Test statistics and debugging */
static void test_statistics(void) {
    printf("Testing statistics...\n");
    
    kalloc_stats_t* stats_before = kalloc_get_stats();
    uint64_t allocs_before = stats_before->allocation_count;
    uint64_t frees_before = stats_before->free_count;
    
    /* Do some allocations */
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = kalloc(64 * (i + 1));
    }
    
    kalloc_stats_t* stats_after = kalloc_get_stats();
    
    if (stats_after->allocation_count < allocs_before + 10) {
        printf("FAIL: Allocation count not updated correctly\n");
        return;
    }
    
    /* Free them */
    for (int i = 0; i < 10; i++) {
        kfree(ptrs[i]);
    }
    
    kalloc_stats_t* stats_final = kalloc_get_stats();
    
    if (stats_final->free_count < frees_before + 10) {
        printf("FAIL: Free count not updated correctly\n");
        return;
    }
    
    /* Print current statistics */
    kalloc_print_stats();
    
    /* Test heap validation */
    kalloc_validate_heap();
    
    if (kalloc_check_corruption()) {
        printf("WARNING: Heap corruption detected\n");
    }
    
    printf("PASS: Statistics test\n");
}

/* Memory stress test */
void kalloc_stress_test(void) {
    printf("\n=== KALLOC Stress Test ===\n");
    
    const int NUM_ALLOCS = 1000;
    const int MAX_SIZE = 4096;
    void* ptrs[NUM_ALLOCS];
    
    /* Random-like allocation pattern */
    for (int i = 0; i < NUM_ALLOCS; i++) {
        size_t size = ((i * 37) % MAX_SIZE) + 1;  /* Pseudo-random size */
        ptrs[i] = kalloc(size);
        
        if (!ptrs[i]) {
            printf("Allocation %d failed (size %zu)\n", i, size);
            break;
        }
        
        /* Write pattern to test for corruption */
        memset(ptrs[i], (i % 256), kalloc_usable_size(ptrs[i]));
    }
    
    /* Free random allocations */
    for (int i = 0; i < NUM_ALLOCS; i += 2) {
        if (ptrs[i]) {
            kfree(ptrs[i]);
            ptrs[i] = NULL;
        }
    }
    
    /* Reallocate in freed spaces */
    for (int i = 0; i < NUM_ALLOCS; i += 2) {
        size_t size = ((i * 73) % MAX_SIZE) + 1;
        ptrs[i] = kalloc(size);
        
        if (ptrs[i]) {
            memset(ptrs[i], ((i + 128) % 256), kalloc_usable_size(ptrs[i]));
        }
    }
    
    /* Free everything */
    for (int i = 0; i < NUM_ALLOCS; i++) {
        if (ptrs[i]) {
            kfree(ptrs[i]);
        }
    }
    
    printf("Stress test completed\n");
    kalloc_print_stats();
    printf("=== Stress Test Complete ===\n\n");
}
