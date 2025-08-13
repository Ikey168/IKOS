/* IKOS Virtual Memory Manager Tests
 * Comprehensive testing for VMM functionality
 */

#include "vmm.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, msg) \
    do { \
        if (condition) { \
            printf("PASS: %s\n", msg); \
            tests_passed++; \
        } else { \
            printf("FAIL: %s\n", msg); \
            tests_failed++; \
        } \
    } while(0)

/* Forward declarations for missing functions */
extern uint64_t kmalloc_simple(size_t size);
extern void kfree_simple(void* ptr);

/* Wrapper functions for testing */
static void* test_kmalloc(size_t size) {
    return (void*)kmalloc_simple(size);
}

static void test_kfree(void* ptr) {
    kfree_simple(ptr);
}

/**
 * Test VMM initialization
 */
static void test_vmm_init(void) {
    printf("\n=== Testing VMM Initialization ===\n");
    
    // Test initialization
    int result = vmm_init(0x10000000); // 256MB
    TEST_ASSERT(result == VMM_SUCCESS, "VMM initialization");
    
    // Test statistics
    vmm_stats_t* stats = vmm_get_stats();
    TEST_ASSERT(stats != NULL, "VMM statistics available");
    TEST_ASSERT(stats->total_pages > 0, "Total pages reported");
    TEST_ASSERT(stats->free_pages > 0, "Free pages available");
    
    printf("Total pages: %lu, Free pages: %lu\n", 
           (unsigned long)stats->total_pages, (unsigned long)stats->free_pages);
}

/**
 * Test address space management
 */
static void test_address_space_management(void) {
    printf("\n=== Testing Address Space Management ===\n");
    
    // Create address space
    vm_space_t* space = vmm_create_address_space(123);
    TEST_ASSERT(space != NULL, "Address space creation");
    TEST_ASSERT(space->owner_pid == 123, "Address space owner PID");
    TEST_ASSERT(space->pml4_virt != NULL, "PML4 table allocated");
    TEST_ASSERT(space->pml4_phys != 0, "PML4 physical address");
    
    // Test switching
    int result = vmm_switch_address_space(space);
    TEST_ASSERT(result == VMM_SUCCESS, "Address space switching");
    
    vm_space_t* current = vmm_get_current_space();
    TEST_ASSERT(current == space, "Current address space retrieval");
    
    // Cleanup
    vmm_destroy_address_space(space);
}

/**
 * Test physical memory allocation
 */
static void test_physical_memory(void) {
    printf("\n=== Testing Physical Memory Management ===\n");
    
    vmm_stats_t* stats_before = vmm_get_stats();
    uint64_t free_before = stats_before->free_pages;
    
    // Allocate some pages
    uint64_t page1 = vmm_alloc_page();
    uint64_t page2 = vmm_alloc_page();
    uint64_t page3 = vmm_alloc_page();
    
    TEST_ASSERT(page1 != 0, "Physical page allocation 1");
    TEST_ASSERT(page2 != 0, "Physical page allocation 2");
    TEST_ASSERT(page3 != 0, "Physical page allocation 3");
    TEST_ASSERT(page1 != page2, "Allocated pages are different");
    TEST_ASSERT(page2 != page3, "Allocated pages are unique");
    
    // Check alignment
    TEST_ASSERT((page1 & (PAGE_SIZE - 1)) == 0, "Page 1 alignment");
    TEST_ASSERT((page2 & (PAGE_SIZE - 1)) == 0, "Page 2 alignment");
    TEST_ASSERT((page3 & (PAGE_SIZE - 1)) == 0, "Page 3 alignment");
    
    vmm_stats_t* stats_after = vmm_get_stats();
    TEST_ASSERT(stats_after->free_pages == free_before - 3, "Free page count decreased");
    
    // Free pages
    vmm_free_page(page1);
    vmm_free_page(page2);
    vmm_free_page(page3);
    
    vmm_stats_t* stats_final = vmm_get_stats();
    TEST_ASSERT(stats_final->free_pages == free_before, "Free page count restored");
}

/**
 * Test virtual memory allocation
 */
static void test_virtual_memory(void) {
    printf("\n=== Testing Virtual Memory Allocation ===\n");
    
    vm_space_t* space = vmm_create_address_space(456);
    TEST_ASSERT(space != NULL, "Test address space creation");
    
    // Switch to test space
    vmm_switch_address_space(space);
    
    // Allocate virtual memory
    void* mem1 = vmm_alloc_virtual(space, 4096, VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_USER);
    void* mem2 = vmm_alloc_virtual(space, 8192, VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_USER);
    
    TEST_ASSERT(mem1 != NULL, "Virtual memory allocation 1");
    TEST_ASSERT(mem2 != NULL, "Virtual memory allocation 2");
    TEST_ASSERT(mem1 != mem2, "Virtual allocations are different");
    
    // Test memory access (basic)
    uint64_t phys1 = vmm_get_physical_addr(space, (uint64_t)mem1);
    uint64_t phys2 = vmm_get_physical_addr(space, (uint64_t)mem2);
    
    TEST_ASSERT(phys1 != 0, "Physical mapping for virtual memory 1");
    TEST_ASSERT(phys2 != 0, "Physical mapping for virtual memory 2");
    TEST_ASSERT(phys1 != phys2, "Different physical pages mapped");
    
    // Free virtual memory
    vmm_free_virtual(space, mem1, 4096);
    vmm_free_virtual(space, mem2, 8192);
    
    // Verify unmapping
    uint64_t phys1_after = vmm_get_physical_addr(space, (uint64_t)mem1);
    uint64_t phys2_after = vmm_get_physical_addr(space, (uint64_t)mem2);
    
    TEST_ASSERT(phys1_after == 0, "Virtual memory 1 unmapped");
    TEST_ASSERT(phys2_after == 0, "Virtual memory 2 unmapped");
    
    vmm_destroy_address_space(space);
}

/**
 * Test memory regions
 */
static void test_memory_regions(void) {
    printf("\n=== Testing Memory Regions ===\n");
    
    vm_space_t* space = vmm_create_address_space(789);
    TEST_ASSERT(space != NULL, "Test address space creation");
    
    // Create regions
    vm_region_t* region1 = vmm_create_region(space, 0x100000, 0x10000, 
                                             VMM_FLAG_READ | VMM_FLAG_WRITE,
                                             VMM_REGION_HEAP, "test_heap");
    
    vm_region_t* region2 = vmm_create_region(space, 0x200000, 0x20000,
                                             VMM_FLAG_READ | VMM_FLAG_EXEC,
                                             VMM_REGION_CODE, "test_code");
    
    TEST_ASSERT(region1 != NULL, "Heap region creation");
    TEST_ASSERT(region2 != NULL, "Code region creation");
    TEST_ASSERT(space->region_count == 2, "Region count");
    
    // Test region finding
    vm_region_t* found1 = vmm_find_region(space, 0x105000);
    vm_region_t* found2 = vmm_find_region(space, 0x210000);
    vm_region_t* not_found = vmm_find_region(space, 0x300000);
    
    TEST_ASSERT(found1 == region1, "Find heap region");
    TEST_ASSERT(found2 == region2, "Find code region");
    TEST_ASSERT(not_found == NULL, "Non-existent region not found");
    
    // Test region properties
    TEST_ASSERT(strcmp(region1->name, "test_heap") == 0, "Heap region name");
    TEST_ASSERT(strcmp(region2->name, "test_code") == 0, "Code region name");
    TEST_ASSERT(region1->type == VMM_REGION_HEAP, "Heap region type");
    TEST_ASSERT(region2->type == VMM_REGION_CODE, "Code region type");
    
    vmm_destroy_address_space(space);
}

/**
 * Test page mapping and unmapping
 */
static void test_page_mapping(void) {
    printf("\n=== Testing Page Mapping ===\n");
    
    vm_space_t* space = vmm_create_address_space(101112);
    TEST_ASSERT(space != NULL, "Test address space creation");
    
    // Allocate physical page
    uint64_t phys = vmm_alloc_page();
    TEST_ASSERT(phys != 0, "Physical page allocation");
    
    // Map page
    uint64_t virt = 0x400000;
    int result = vmm_map_page(space, virt, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    TEST_ASSERT(result == VMM_SUCCESS, "Page mapping");
    
    // Verify mapping
    uint64_t mapped_phys = vmm_get_physical_addr(space, virt);
    TEST_ASSERT(mapped_phys == phys, "Physical address retrieval");
    
    // Test page alignment
    uint64_t unaligned_virt = 0x400123;
    uint64_t retrieved_phys = vmm_get_physical_addr(space, unaligned_virt);
    TEST_ASSERT(retrieved_phys == phys + 0x123, "Unaligned address mapping");
    
    // Unmap page
    result = vmm_unmap_page(space, virt);
    TEST_ASSERT(result == VMM_SUCCESS, "Page unmapping");
    
    // Verify unmapping
    uint64_t unmapped_phys = vmm_get_physical_addr(space, virt);
    TEST_ASSERT(unmapped_phys == 0, "Page unmapped verification");
    
    vmm_destroy_address_space(space);
}

/**
 * Test heap expansion
 */
static void test_heap_expansion(void) {
    printf("\n=== Testing Heap Expansion ===\n");
    
    vm_space_t* space = vmm_create_address_space(131415);
    TEST_ASSERT(space != NULL, "Test address space creation");
    
    // Initial heap state
    uint64_t initial_heap = space->heap_end;
    
    // Expand heap
    void* old_end = vmm_expand_heap(space, 0x10000);
    TEST_ASSERT(old_end == (void*)initial_heap, "Heap expansion returns old end");
    TEST_ASSERT(space->heap_end == initial_heap + 0x10000, "Heap end updated");
    
    // Expand again
    void* old_end2 = vmm_expand_heap(space, 0x5000);
    TEST_ASSERT(old_end2 == (void*)(initial_heap + 0x10000), "Second expansion");
    TEST_ASSERT(space->heap_end == initial_heap + 0x15000, "Heap end updated again");
    
    // Shrink heap
    void* old_end3 = vmm_expand_heap(space, -0x8000);
    TEST_ASSERT(old_end3 == (void*)(initial_heap + 0x15000), "Heap shrinking");
    TEST_ASSERT(space->heap_end == initial_heap + 0xD000, "Heap end after shrinking");
    
    vmm_destroy_address_space(space);
}

/**
 * Test copy-on-write functionality
 */
static void test_copy_on_write(void) {
    printf("\n=== Testing Copy-on-Write ===\n");
    
    vm_space_t* parent_space = vmm_create_address_space(161718);
    TEST_ASSERT(parent_space != NULL, "Parent address space creation");
    
    // Allocate and map a page in parent
    uint64_t phys = vmm_alloc_page();
    uint64_t virt = 0x500000;
    int result = vmm_map_page(parent_space, virt, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    TEST_ASSERT(result == VMM_SUCCESS, "Parent page mapping");
    
    // Create child address space with COW
    vm_space_t* child_space = vmm_copy_address_space(parent_space, 192021);
    TEST_ASSERT(child_space != NULL, "Child address space creation");
    
    // Both should map to same physical page initially
    uint64_t parent_phys = vmm_get_physical_addr(parent_space, virt);
    uint64_t child_phys = vmm_get_physical_addr(child_space, virt);
    TEST_ASSERT(parent_phys == child_phys, "COW mapping to same physical page");
    
    // Simulate COW fault in child
    result = vmm_handle_cow_fault(child_space, virt);
    TEST_ASSERT(result == VMM_SUCCESS, "COW fault handling");
    
    // Now they should map to different physical pages
    uint64_t new_parent_phys = vmm_get_physical_addr(parent_space, virt);
    uint64_t new_child_phys = vmm_get_physical_addr(child_space, virt);
    TEST_ASSERT(new_parent_phys == parent_phys, "Parent physical page unchanged");
    TEST_ASSERT(new_child_phys != parent_phys, "Child has new physical page");
    
    vmm_destroy_address_space(parent_space);
    vmm_destroy_address_space(child_space);
}

/**
 * Test memory mapping (mmap-like)
 */
static void test_memory_mapping(void) {
    printf("\n=== Testing Memory Mapping ===\n");
    
    vm_space_t* space = vmm_create_address_space(222324);
    TEST_ASSERT(space != NULL, "Test address space creation");
    
    // Test anonymous mapping
    void* mapped = vmm_mmap(space, NULL, 0x10000, 
                           VMM_PROT_READ | VMM_PROT_WRITE, 0);
    TEST_ASSERT(mapped != (void*)-1, "Anonymous memory mapping");
    TEST_ASSERT(vmm_is_user_addr((uint64_t)mapped), "Mapped address in user space");
    
    // Test fixed mapping
    void* fixed_addr = (void*)0x600000;
    void* fixed_mapped = vmm_mmap(space, fixed_addr, 0x8000,
                                 VMM_PROT_READ | VMM_PROT_WRITE, VMM_MMAP_FIXED);
    TEST_ASSERT(fixed_mapped == fixed_addr, "Fixed memory mapping");
    
    // Test unmapping
    int result = vmm_munmap(space, mapped, 0x10000);
    TEST_ASSERT(result == VMM_SUCCESS, "Memory unmapping");
    
    result = vmm_munmap(space, fixed_mapped, 0x8000);
    TEST_ASSERT(result == VMM_SUCCESS, "Fixed memory unmapping");
    
    vmm_destroy_address_space(space);
}

/**
 * Test address utilities
 */
static void test_address_utilities(void) {
    printf("\n=== Testing Address Utilities ===\n");
    
    // Test alignment functions
    TEST_ASSERT(vmm_align_down(0x12345678, 0x1000) == 0x12345000, "Align down");
    TEST_ASSERT(vmm_align_up(0x12345678, 0x1000) == 0x12346000, "Align up");
    TEST_ASSERT(vmm_align_down(0x12345000, 0x1000) == 0x12345000, "Align down (already aligned)");
    TEST_ASSERT(vmm_align_up(0x12345000, 0x1000) == 0x12345000, "Align up (already aligned)");
    
    // Test address space checks
    TEST_ASSERT(vmm_is_user_addr(0x00400000), "User address detection");
    TEST_ASSERT(!vmm_is_user_addr(0xFFFF800000000000), "Kernel address not user");
    TEST_ASSERT(vmm_is_kernel_addr(0xFFFF800000000000), "Kernel address detection");
    TEST_ASSERT(!vmm_is_kernel_addr(0x00400000), "User address not kernel");
}

/**
 * Test error conditions
 */
static void test_error_conditions(void) {
    printf("\n=== Testing Error Conditions ===\n");
    
    // Test NULL pointer handling
    TEST_ASSERT(vmm_create_region(NULL, 0, 0x1000, 0, VMM_REGION_HEAP, "test") == NULL, 
                "NULL space region creation");
    TEST_ASSERT(vmm_find_region(NULL, 0x1000) == NULL, "NULL space region finding");
    TEST_ASSERT(vmm_alloc_virtual(NULL, 0x1000, 0) == NULL, "NULL space virtual allocation");
    
    vm_space_t* space = vmm_create_address_space(252627);
    
    // Test invalid size allocations
    TEST_ASSERT(vmm_create_region(space, 0x1000, 0, 0, VMM_REGION_HEAP, "test") == NULL,
                "Zero size region creation");
    TEST_ASSERT(vmm_alloc_virtual(space, 0, 0) == NULL, "Zero size virtual allocation");
    
    // Test invalid address unmapping
    int result = vmm_unmap_page(space, 0x999000);
    TEST_ASSERT(result == VMM_ERROR_NOT_FOUND, "Unmapping non-existent page");
    
    vmm_destroy_address_space(space);
}

/**
 * Performance test - allocate and free many pages
 */
static void test_performance(void) {
    printf("\n=== Testing Performance ===\n");
    
    const int num_pages = 1000;
    uint64_t pages[num_pages];
    
    // Time allocation (simplified timing)
    printf("Allocating %d pages...\n", num_pages);
    
    int allocated = 0;
    for (int i = 0; i < num_pages; i++) {
        pages[i] = vmm_alloc_page();
        if (pages[i] != 0) {
            allocated++;
        }
    }
    
    TEST_ASSERT(allocated > 0, "Performance test allocation");
    printf("Successfully allocated %d pages\n", allocated);
    
    // Free allocated pages
    printf("Freeing allocated pages...\n");
    int freed = 0;
    for (int i = 0; i < allocated; i++) {
        if (pages[i] != 0) {
            vmm_free_page(pages[i]);
            freed++;
        }
    }
    
    TEST_ASSERT(freed == allocated, "Performance test deallocation");
    printf("Successfully freed %d pages\n", freed);
}

/**
 * Run all VMM tests
 */
void vmm_run_tests(void) {
    printf("=== IKOS Virtual Memory Manager Tests ===\n");
    
    tests_passed = 0;
    tests_failed = 0;
    
    test_vmm_init();
    test_address_space_management();
    test_physical_memory();
    test_virtual_memory();
    test_memory_regions();
    test_page_mapping();
    test_heap_expansion();
    test_copy_on_write();
    test_memory_mapping();
    test_address_utilities();
    test_error_conditions();
    test_performance();
    
    printf("\n=== Test Results ===\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Total tests: %d\n", tests_passed + tests_failed);
    
    if (tests_failed == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("Some tests FAILED!\n");
    }
}

/**
 * Quick smoke test for basic functionality
 */
void vmm_smoke_test(void) {
    printf("=== VMM Smoke Test ===\n");
    
    // Initialize VMM
    if (vmm_init(0x10000000) != VMM_SUCCESS) {
        printf("FAIL: VMM initialization failed\n");
        return;
    }
    
    // Create address space
    vm_space_t* space = vmm_create_address_space(999);
    if (!space) {
        printf("FAIL: Address space creation failed\n");
        return;
    }
    
    // Allocate virtual memory
    void* mem = vmm_alloc_virtual(space, 4096, VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_USER);
    if (!mem) {
        printf("FAIL: Virtual memory allocation failed\n");
        vmm_destroy_address_space(space);
        return;
    }
    
    // Check physical mapping
    uint64_t phys = vmm_get_physical_addr(space, (uint64_t)mem);
    if (phys == 0) {
        printf("FAIL: Physical mapping not found\n");
        vmm_free_virtual(space, mem, 4096);
        vmm_destroy_address_space(space);
        return;
    }
    
    // Cleanup
    vmm_free_virtual(space, mem, 4096);
    vmm_destroy_address_space(space);
    
    printf("PASS: VMM smoke test completed successfully\n");
}

/**
 * Main function for standalone test execution
 */
int main(int argc, char* argv[]) {
    printf("=== IKOS Virtual Memory Manager Test Suite ===\n");
    
    if (argc > 1 && strcmp(argv[1], "smoke") == 0) {
        vmm_smoke_test();
        return 0;
    }
    
    vmm_run_tests();
    
    if (tests_failed == 0) {
        printf("\n=== ALL TESTS PASSED ===\n");
        return 0;
    } else {
        printf("\n=== SOME TESTS FAILED ===\n");
        return 1;
    }
}
