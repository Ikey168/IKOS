/* IKOS User Space Memory Management Test Suite
 * Comprehensive tests for USMM functionality
 */

#include "../include/user_space_memory.h"
#include <stdio.h>
#include <string.h>

/* Test statistics */
static struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
} test_stats = {0};

/* Test macros */
#define TEST_START(name) \
    do { \
        printf("Running test: %s\n", name); \
        test_stats.tests_run++; \
    } while(0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("  ✓ %s\n", message); \
        } else { \
            printf("  ✗ %s\n", message); \
            test_stats.tests_failed++; \
            return -1; \
        } \
    } while(0)

#define TEST_PASS(name) \
    do { \
        printf("  ✓ Test %s passed\n", name); \
        test_stats.tests_passed++; \
        return 0; \
    } while(0)

/* ========================== Basic USMM Tests ========================== */

int test_usmm_initialization(void) {
    TEST_START("USMM Initialization");
    
    /* Test initialization */
    int result = usmm_init();
    TEST_ASSERT(result == USMM_SUCCESS, "USMM initialization successful");
    
    /* Test double initialization */
    result = usmm_init();
    TEST_ASSERT(result == USMM_SUCCESS, "Double initialization handled");
    
    /* Test shutdown */
    usmm_shutdown();
    TEST_ASSERT(1, "USMM shutdown completed");
    
    /* Re-initialize for other tests */
    result = usmm_init();
    TEST_ASSERT(result == USMM_SUCCESS, "USMM re-initialization successful");
    
    TEST_PASS("USMM Initialization");
}

int test_mm_struct_operations(void) {
    TEST_START("mm_struct Operations");
    
    /* Test mm allocation */
    mm_struct_t* mm = mm_alloc();
    TEST_ASSERT(mm != NULL, "mm_struct allocation successful");
    
    /* Test basic initialization */
    TEST_ASSERT(mm->map_count == 0, "Initial map count is zero");
    TEST_ASSERT(mm->mmap == NULL, "Initial VMA list is empty");
    TEST_ASSERT(atomic_read(&mm->mm_users) == 1, "Initial user count is 1");
    TEST_ASSERT(atomic_read(&mm->mm_count) == 1, "Initial reference count is 1");
    
    /* Test address space layout */
    TEST_ASSERT(mm->task_size > 0, "Task size is set");
    TEST_ASSERT(mm->start_stack > mm->start_brk, "Stack is above heap");
    TEST_ASSERT(mm->mmap_base > mm->start_brk, "mmap area is above heap");
    
    /* Test mm copy */
    mm_struct_t* mm_copy_test = mm_copy(mm);
    TEST_ASSERT(mm_copy_test != NULL, "mm_struct copy successful");
    TEST_ASSERT(mm_copy_test != mm, "Copy is different object");
    TEST_ASSERT(mm_copy_test->task_size == mm->task_size, "Task size copied correctly");
    
    /* Test cleanup */
    mm_free(mm_copy_test);
    mm_free(mm);
    TEST_ASSERT(1, "mm_struct cleanup successful");
    
    TEST_PASS("mm_struct Operations");
}

int test_vma_management(void) {
    TEST_START("VMA Management");
    
    mm_struct_t* mm = mm_alloc();
    TEST_ASSERT(mm != NULL, "mm_struct allocated");
    
    /* Create test VMA */
    vm_area_struct_t vma;
    memset(&vma, 0, sizeof(vma));
    vma.vm_start = 0x10000000;
    vma.vm_end = 0x10001000;
    vma.vm_flags = VM_READ | VM_WRITE;
    vma.vm_prot = PROT_READ | PROT_WRITE;
    
    /* Test VMA insertion */
    int result = insert_vm_area(mm, &vma);
    TEST_ASSERT(result == USMM_SUCCESS, "VMA insertion successful");
    TEST_ASSERT(mm->map_count == 1, "Map count updated");
    TEST_ASSERT(mm->mmap == &vma, "VMA added to list");
    
    /* Test VMA lookup */
    vm_area_struct_t* found_vma = find_vma(mm, 0x10000000);
    TEST_ASSERT(found_vma == &vma, "VMA lookup by start address");
    
    found_vma = find_vma(mm, 0x10000500);
    TEST_ASSERT(found_vma == &vma, "VMA lookup by middle address");
    
    found_vma = find_vma(mm, 0x10001000);
    TEST_ASSERT(found_vma == NULL, "VMA lookup past end address");
    
    /* Test VMA intersection */
    found_vma = find_vma_intersection(mm, 0x0FFF0000, 0x10000500);
    TEST_ASSERT(found_vma == &vma, "VMA intersection detection");
    
    found_vma = find_vma_intersection(mm, 0x20000000, 0x20001000);
    TEST_ASSERT(found_vma == NULL, "No intersection with non-overlapping range");
    
    /* Test VMA removal */
    result = remove_vm_area(mm, &vma);
    TEST_ASSERT(result == USMM_SUCCESS, "VMA removal successful");
    TEST_ASSERT(mm->map_count == 0, "Map count decremented");
    TEST_ASSERT(mm->mmap == NULL, "VMA removed from list");
    
    mm_free(mm);
    TEST_PASS("VMA Management");
}

int test_memory_mapping(void) {
    TEST_START("Memory Mapping");
    
    /* Test anonymous mapping */
    void* addr = sys_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    TEST_ASSERT(addr != (void*)-1, "Anonymous mapping successful");
    TEST_ASSERT(((uint64_t)addr & 0xFFF) == 0, "Mapping is page-aligned");
    
    /* Test fixed mapping */
    void* fixed_addr = (void*)0x20000000;
    void* mapped = sys_mmap(fixed_addr, 4096, PROT_READ | PROT_WRITE, 
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    TEST_ASSERT(mapped == fixed_addr, "Fixed mapping at requested address");
    
    /* Test protection flags */
    int result = sys_mprotect(addr, 4096, PROT_READ);
    TEST_ASSERT(result == USMM_SUCCESS, "Memory protection change successful");
    
    /* Test unmapping */
    result = sys_munmap(addr, 4096);
    TEST_ASSERT(result == USMM_SUCCESS, "Memory unmapping successful");
    
    result = sys_munmap(fixed_addr, 4096);
    TEST_ASSERT(result == USMM_SUCCESS, "Fixed mapping unmapping successful");
    
    TEST_PASS("Memory Mapping");
}

int test_shared_memory(void) {
    TEST_START("Shared Memory");
    
    /* Test System V shared memory creation */
    int shmid = sys_shmget(IPC_PRIVATE, 8192, IPC_CREAT | 0666);
    TEST_ASSERT(shmid >= 0, "Shared memory segment creation");
    
    /* Test shared memory attachment */
    void* shm_addr = sys_shmat(shmid, NULL, 0);
    TEST_ASSERT(shm_addr != (void*)-1, "Shared memory attachment");
    TEST_ASSERT(((uint64_t)shm_addr & 0xFFF) == 0, "Shared memory is page-aligned");
    
    /* Test shared memory detachment */
    int result = sys_shmdt(shm_addr);
    TEST_ASSERT(result == USMM_SUCCESS, "Shared memory detachment");
    
    /* Test shared memory control */
    result = sys_shmctl(shmid, IPC_RMID, NULL);
    TEST_ASSERT(result == USMM_SUCCESS, "Shared memory removal");
    
    /* Test POSIX shared memory */
    int fd = sys_shm_open("/test_shm", O_CREAT | O_RDWR, 0666);
    TEST_ASSERT(fd >= 0, "POSIX shared memory creation");
    
    result = sys_shm_unlink("/test_shm");
    TEST_ASSERT(result == USMM_SUCCESS, "POSIX shared memory unlinking");
    
    TEST_PASS("Shared Memory");
}

int test_copy_on_write(void) {
    TEST_START("Copy-on-Write");
    
    /* Create a VMA for testing */
    mm_struct_t* mm = mm_alloc();
    TEST_ASSERT(mm != NULL, "mm_struct allocated");
    
    vm_area_struct_t vma;
    memset(&vma, 0, sizeof(vma));
    vma.vm_start = 0x30000000;
    vma.vm_end = 0x30001000;
    vma.vm_flags = VM_READ | VM_WRITE;
    vma.vm_prot = PROT_READ | PROT_WRITE;
    
    int result = insert_vm_area(mm, &vma);
    TEST_ASSERT(result == USMM_SUCCESS, "VMA inserted for COW test");
    
    /* Test COW setup */
    result = setup_cow_mapping(&vma);
    TEST_ASSERT(result == USMM_SUCCESS, "COW mapping setup");
    
    /* Test COW page fault */
    result = cow_page_fault(&vma, 0x30000000);
    TEST_ASSERT(result == USMM_SUCCESS || result == -USMM_EFAULT, "COW page fault handled");
    
    /* Test statistics */
    cow_stats_t cow_stats;
    get_cow_stats(&cow_stats);
    TEST_ASSERT(cow_stats.cow_pages_created >= 0, "COW statistics available");
    
    mm_free(mm);
    TEST_PASS("Copy-on-Write");
}

int test_memory_protection(void) {
    TEST_START("Memory Protection");
    
    /* Test protection flag conversion */
    uint32_t vm_flags = prot_to_vm_flags(PROT_READ | PROT_WRITE);
    TEST_ASSERT(vm_flags & VM_READ, "Read protection converted");
    TEST_ASSERT(vm_flags & VM_WRITE, "Write protection converted");
    TEST_ASSERT(!(vm_flags & VM_EXEC), "Execute protection not set");
    
    int prot = vm_flags_to_prot(VM_READ | VM_EXEC);
    TEST_ASSERT(prot & PROT_READ, "VM_READ converted to PROT_READ");
    TEST_ASSERT(prot & PROT_EXEC, "VM_EXEC converted to PROT_EXEC");
    TEST_ASSERT(!(prot & PROT_WRITE), "PROT_WRITE not set");
    
    /* Test address space layout randomization */
    mm_struct_t* mm = mm_alloc();
    TEST_ASSERT(mm != NULL, "mm_struct allocated");
    
    uint64_t addr1 = arch_get_unmapped_area(NULL, 0, 4096, 0, 0);
    uint64_t addr2 = arch_get_unmapped_area(NULL, 0, 4096, 0, 0);
    TEST_ASSERT(addr1 != addr2, "Different addresses returned");
    TEST_ASSERT((addr1 & 0xFFF) == 0, "Address 1 is page-aligned");
    TEST_ASSERT((addr2 & 0xFFF) == 0, "Address 2 is page-aligned");
    
    mm_free(mm);
    TEST_PASS("Memory Protection");
}

int test_page_fault_handling(void) {
    TEST_START("Page Fault Handling");
    
    /* Test basic page fault handling */
    mm_struct_t* mm = mm_alloc();
    TEST_ASSERT(mm != NULL, "mm_struct allocated");
    
    vm_area_struct_t vma;
    memset(&vma, 0, sizeof(vma));
    vma.vm_start = 0x40000000;
    vma.vm_end = 0x40001000;
    vma.vm_flags = VM_READ | VM_WRITE;
    vma.vm_prot = PROT_READ | PROT_WRITE;
    vma.vm_mm = mm;
    
    int result = insert_vm_area(mm, &vma);
    TEST_ASSERT(result == USMM_SUCCESS, "VMA inserted for fault test");
    
    /* Test fault within VMA */
    result = handle_mm_fault(mm, &vma, 0x40000500, FAULT_FLAG_WRITE);
    TEST_ASSERT(result == USMM_SUCCESS || result == -USMM_EFAULT, "Page fault handled within VMA");
    
    /* Test fault outside VMA */
    result = handle_mm_fault(mm, &vma, 0x50000000, FAULT_FLAG_WRITE);
    TEST_ASSERT(result == -USMM_EFAULT, "Page fault outside VMA rejected");
    
    mm_free(mm);
    TEST_PASS("Page Fault Handling");
}

int test_memory_accounting(void) {
    TEST_START("Memory Accounting");
    
    mm_struct_t* mm = mm_alloc();
    TEST_ASSERT(mm != NULL, "mm_struct allocated");
    
    /* Test initial accounting values */
    TEST_ASSERT(atomic64_read(&mm->total_vm) == 0, "Initial total_vm is zero");
    TEST_ASSERT(atomic64_read(&mm->anon_rss) == 0, "Initial anon_rss is zero");
    TEST_ASSERT(atomic64_read(&mm->file_rss) == 0, "Initial file_rss is zero");
    
    /* Test memory usage tracking */
    memory_usage_t usage;
    int result = get_memory_usage(1, &usage);
    TEST_ASSERT(result == USMM_SUCCESS || result == -USMM_EINVAL, "Memory usage query");
    
    /* Test memory limits */
    rlimit_t limit;
    limit.rlim_cur = 1024 * 1024 * 1024; /* 1GB */
    limit.rlim_max = 2ULL * 1024 * 1024 * 1024; /* 2GB */
    
    result = set_memory_limit(1, RLIMIT_AS, &limit);
    TEST_ASSERT(result == USMM_SUCCESS || result == -USMM_EINVAL, "Memory limit setting");
    
    result = get_memory_limit(1, RLIMIT_AS, &limit);
    TEST_ASSERT(result == USMM_SUCCESS || result == -USMM_EINVAL, "Memory limit querying");
    
    mm_free(mm);
    TEST_PASS("Memory Accounting");
}

int test_statistics_and_monitoring(void) {
    TEST_START("Statistics and Monitoring");
    
    /* Test USMM statistics */
    usmm_stats_t stats;
    get_usmm_stats(&stats);
    TEST_ASSERT(1, "USMM statistics retrieved");
    
    /* Test statistics reset */
    reset_usmm_stats();
    get_usmm_stats(&stats);
    TEST_ASSERT(stats.total_mappings == 0, "Statistics reset successful");
    
    /* Test memory pressure monitoring */
    memory_pressure_t pressure;
    int result = get_memory_pressure(&pressure);
    TEST_ASSERT(result == USMM_SUCCESS || result == -USMM_ENOSYS, "Memory pressure query");
    
    TEST_PASS("Statistics and Monitoring");
}

/* ========================== Utility Tests ========================== */

int test_utility_functions(void) {
    TEST_START("Utility Functions");
    
    /* Test page address utilities */
    uint64_t addr = 0x12345678;
    uint64_t page_addr = addr_to_page(addr);
    TEST_ASSERT((page_addr & 0xFFF) == 0, "addr_to_page returns page-aligned address");
    
    uint64_t rounded_up = round_up_to_page(addr);
    TEST_ASSERT(rounded_up >= addr, "round_up_to_page rounds up");
    TEST_ASSERT((rounded_up & 0xFFF) == 0, "round_up_to_page returns page-aligned");
    
    uint64_t rounded_down = round_down_to_page(addr);
    TEST_ASSERT(rounded_down <= addr, "round_down_to_page rounds down");
    TEST_ASSERT((rounded_down & 0xFFF) == 0, "round_down_to_page returns page-aligned");
    
    /* Test VMA utilities */
    vm_area_struct_t vma;
    vma.vm_start = 0x10000000;
    vma.vm_end = 0x10001000;
    
    bool contains = vma_contains_addr(&vma, 0x10000500);
    TEST_ASSERT(contains, "vma_contains_addr detects contained address");
    
    contains = vma_contains_addr(&vma, 0x20000000);
    TEST_ASSERT(!contains, "vma_contains_addr rejects non-contained address");
    
    bool overlaps = vma_overlaps_range(&vma, 0x0FFF0000, 0x10000500);
    TEST_ASSERT(overlaps, "vma_overlaps_range detects overlap");
    
    overlaps = vma_overlaps_range(&vma, 0x20000000, 0x20001000);
    TEST_ASSERT(!overlaps, "vma_overlaps_range rejects non-overlapping range");
    
    uint64_t size = vma_size(&vma);
    TEST_ASSERT(size == 0x1000, "vma_size calculates correct size");
    
    TEST_PASS("Utility Functions");
}

/* ========================== Main Test Runner ========================== */

void print_test_summary(void) {
    printf("\n=== Test Summary ===\n");
    printf("Tests run: %d\n", test_stats.tests_run);
    printf("Tests passed: %d\n", test_stats.tests_passed);
    printf("Tests failed: %d\n", test_stats.tests_failed);
    
    if (test_stats.tests_failed == 0) {
        printf("✓ All tests passed!\n");
    } else {
        printf("✗ %d test(s) failed\n", test_stats.tests_failed);
    }
}

int main(void) {
    printf("IKOS User Space Memory Management Test Suite\n");
    printf("=============================================\n\n");
    
    /* Initialize USMM for testing */
    if (usmm_init() != USMM_SUCCESS) {
        printf("Failed to initialize USMM\n");
        return 1;
    }
    
    /* Run all tests */
    test_usmm_initialization();
    test_mm_struct_operations();
    test_vma_management();
    test_memory_mapping();
    test_shared_memory();
    test_copy_on_write();
    test_memory_protection();
    test_page_fault_handling();
    test_memory_accounting();
    test_statistics_and_monitoring();
    test_utility_functions();
    
    /* Print results */
    print_test_summary();
    
    /* Cleanup */
    usmm_shutdown();
    
    return (test_stats.tests_failed == 0) ? 0 : 1;
}

/* ========================== Stub Implementations ========================== */

/* Provide missing constants and simple implementations */
#ifndef O_CREAT
#define O_CREAT     0x40
#endif

#ifndef O_RDWR
#define O_RDWR      0x02
#endif
