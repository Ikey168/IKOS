/* IKOS User Space Memory Management Performance Tests
 * Stress tests and performance measurements for USMM
 */

#include "../include/user_space_memory.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* Performance test configuration */
#define PERF_VMA_COUNT 1000
#define PERF_MAPPING_COUNT 500
#define PERF_SHM_COUNT 100
#define PERF_COW_PAGES 1000
#define PERF_ITERATIONS 1000

/* Timing utilities */
static inline uint64_t get_timestamp_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

/* Performance statistics */
typedef struct {
    uint64_t total_time;
    uint64_t min_time;
    uint64_t max_time;
    uint64_t avg_time;
    uint32_t operations;
} perf_stats_t;

static void init_perf_stats(perf_stats_t* stats) {
    memset(stats, 0, sizeof(*stats));
    stats->min_time = UINT64_MAX;
}

static void update_perf_stats(perf_stats_t* stats, uint64_t time) {
    stats->total_time += time;
    stats->operations++;
    if (time < stats->min_time) stats->min_time = time;
    if (time > stats->max_time) stats->max_time = time;
    stats->avg_time = stats->total_time / stats->operations;
}

static void print_perf_stats(const char* name, perf_stats_t* stats) {
    printf("  %s Performance:\n", name);
    printf("    Operations: %u\n", stats->operations);
    printf("    Total time: %lu us\n", stats->total_time);
    printf("    Min time: %lu us\n", stats->min_time);
    printf("    Max time: %lu us\n", stats->max_time);
    printf("    Avg time: %lu us\n", stats->avg_time);
    printf("    Ops/sec: %.2f\n", 1000000.0 / stats->avg_time);
}

/* ========================== VMA Performance Tests ========================== */

int test_vma_performance(void) {
    printf("Testing VMA Management Performance...\n");
    
    mm_struct_t* mm = mm_alloc();
    if (!mm) {
        printf("Failed to allocate mm_struct\n");
        return -1;
    }
    
    perf_stats_t insert_stats, lookup_stats, remove_stats;
    init_perf_stats(&insert_stats);
    init_perf_stats(&lookup_stats);
    init_perf_stats(&remove_stats);
    
    vm_area_struct_t* vmas = malloc(PERF_VMA_COUNT * sizeof(vm_area_struct_t));
    if (!vmas) {
        mm_free(mm);
        return -1;
    }
    
    /* Test VMA insertion performance */
    for (int i = 0; i < PERF_VMA_COUNT; i++) {
        memset(&vmas[i], 0, sizeof(vm_area_struct_t));
        vmas[i].vm_start = 0x10000000 + (i * 0x1000);
        vmas[i].vm_end = vmas[i].vm_start + 0x1000;
        vmas[i].vm_flags = VM_READ | VM_WRITE;
        vmas[i].vm_prot = PROT_READ | PROT_WRITE;
        
        uint64_t start = get_timestamp_us();
        int result = insert_vm_area(mm, &vmas[i]);
        uint64_t end = get_timestamp_us();
        
        if (result == USMM_SUCCESS) {
            update_perf_stats(&insert_stats, end - start);
        } else {
            printf("VMA insertion failed for index %d\n", i);
        }
    }
    
    /* Test VMA lookup performance */
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        uint64_t addr = 0x10000000 + ((i % PERF_VMA_COUNT) * 0x1000) + 0x500;
        
        uint64_t start = get_timestamp_us();
        vm_area_struct_t* vma = find_vma(mm, addr);
        uint64_t end = get_timestamp_us();
        
        if (vma) {
            update_perf_stats(&lookup_stats, end - start);
        }
    }
    
    /* Test VMA removal performance */
    for (int i = 0; i < PERF_VMA_COUNT; i++) {
        uint64_t start = get_timestamp_us();
        int result = remove_vm_area(mm, &vmas[i]);
        uint64_t end = get_timestamp_us();
        
        if (result == USMM_SUCCESS) {
            update_perf_stats(&remove_stats, end - start);
        }
    }
    
    print_perf_stats("VMA Insertion", &insert_stats);
    print_perf_stats("VMA Lookup", &lookup_stats);
    print_perf_stats("VMA Removal", &remove_stats);
    
    free(vmas);
    mm_free(mm);
    return 0;
}

/* ========================== Memory Mapping Performance ========================== */

int test_mmap_performance(void) {
    printf("\nTesting Memory Mapping Performance...\n");
    
    perf_stats_t mmap_stats, munmap_stats, mprotect_stats;
    init_perf_stats(&mmap_stats);
    init_perf_stats(&munmap_stats);
    init_perf_stats(&mprotect_stats);
    
    void** mappings = malloc(PERF_MAPPING_COUNT * sizeof(void*));
    if (!mappings) {
        return -1;
    }
    
    /* Test mmap performance */
    for (int i = 0; i < PERF_MAPPING_COUNT; i++) {
        uint64_t start = get_timestamp_us();
        void* addr = sys_mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        uint64_t end = get_timestamp_us();
        
        if (addr != (void*)-1) {
            mappings[i] = addr;
            update_perf_stats(&mmap_stats, end - start);
        } else {
            mappings[i] = NULL;
            printf("mmap failed for index %d\n", i);
        }
    }
    
    /* Test mprotect performance */
    for (int i = 0; i < PERF_MAPPING_COUNT; i++) {
        if (mappings[i]) {
            uint64_t start = get_timestamp_us();
            int result = sys_mprotect(mappings[i], 4096, PROT_READ);
            uint64_t end = get_timestamp_us();
            
            if (result == USMM_SUCCESS) {
                update_perf_stats(&mprotect_stats, end - start);
            }
        }
    }
    
    /* Test munmap performance */
    for (int i = 0; i < PERF_MAPPING_COUNT; i++) {
        if (mappings[i]) {
            uint64_t start = get_timestamp_us();
            int result = sys_munmap(mappings[i], 4096);
            uint64_t end = get_timestamp_us();
            
            if (result == USMM_SUCCESS) {
                update_perf_stats(&munmap_stats, end - start);
            }
        }
    }
    
    print_perf_stats("mmap", &mmap_stats);
    print_perf_stats("mprotect", &mprotect_stats);
    print_perf_stats("munmap", &munmap_stats);
    
    free(mappings);
    return 0;
}

/* ========================== Shared Memory Performance ========================== */

int test_shm_performance(void) {
    printf("\nTesting Shared Memory Performance...\n");
    
    perf_stats_t shmget_stats, shmat_stats, shmdt_stats, shmctl_stats;
    init_perf_stats(&shmget_stats);
    init_perf_stats(&shmat_stats);
    init_perf_stats(&shmdt_stats);
    init_perf_stats(&shmctl_stats);
    
    int* shmids = malloc(PERF_SHM_COUNT * sizeof(int));
    void** shm_addrs = malloc(PERF_SHM_COUNT * sizeof(void*));
    if (!shmids || !shm_addrs) {
        free(shmids);
        free(shm_addrs);
        return -1;
    }
    
    /* Test shmget performance */
    for (int i = 0; i < PERF_SHM_COUNT; i++) {
        uint64_t start = get_timestamp_us();
        int shmid = sys_shmget(IPC_PRIVATE, 8192, IPC_CREAT | 0666);
        uint64_t end = get_timestamp_us();
        
        if (shmid >= 0) {
            shmids[i] = shmid;
            update_perf_stats(&shmget_stats, end - start);
        } else {
            shmids[i] = -1;
            printf("shmget failed for index %d\n", i);
        }
    }
    
    /* Test shmat performance */
    for (int i = 0; i < PERF_SHM_COUNT; i++) {
        if (shmids[i] >= 0) {
            uint64_t start = get_timestamp_us();
            void* addr = sys_shmat(shmids[i], NULL, 0);
            uint64_t end = get_timestamp_us();
            
            if (addr != (void*)-1) {
                shm_addrs[i] = addr;
                update_perf_stats(&shmat_stats, end - start);
            } else {
                shm_addrs[i] = NULL;
            }
        } else {
            shm_addrs[i] = NULL;
        }
    }
    
    /* Test shmdt performance */
    for (int i = 0; i < PERF_SHM_COUNT; i++) {
        if (shm_addrs[i]) {
            uint64_t start = get_timestamp_us();
            int result = sys_shmdt(shm_addrs[i]);
            uint64_t end = get_timestamp_us();
            
            if (result == USMM_SUCCESS) {
                update_perf_stats(&shmdt_stats, end - start);
            }
        }
    }
    
    /* Test shmctl performance */
    for (int i = 0; i < PERF_SHM_COUNT; i++) {
        if (shmids[i] >= 0) {
            uint64_t start = get_timestamp_us();
            int result = sys_shmctl(shmids[i], IPC_RMID, NULL);
            uint64_t end = get_timestamp_us();
            
            if (result == USMM_SUCCESS) {
                update_perf_stats(&shmctl_stats, end - start);
            }
        }
    }
    
    print_perf_stats("shmget", &shmget_stats);
    print_perf_stats("shmat", &shmat_stats);
    print_perf_stats("shmdt", &shmdt_stats);
    print_perf_stats("shmctl", &shmctl_stats);
    
    free(shmids);
    free(shm_addrs);
    return 0;
}

/* ========================== Copy-on-Write Performance ========================== */

int test_cow_performance(void) {
    printf("\nTesting Copy-on-Write Performance...\n");
    
    perf_stats_t cow_setup_stats, cow_fault_stats;
    init_perf_stats(&cow_setup_stats);
    init_perf_stats(&cow_fault_stats);
    
    mm_struct_t* mm = mm_alloc();
    if (!mm) {
        return -1;
    }
    
    vm_area_struct_t* vmas = malloc(PERF_COW_PAGES * sizeof(vm_area_struct_t));
    if (!vmas) {
        mm_free(mm);
        return -1;
    }
    
    /* Setup VMAs for COW testing */
    for (int i = 0; i < PERF_COW_PAGES; i++) {
        memset(&vmas[i], 0, sizeof(vm_area_struct_t));
        vmas[i].vm_start = 0x50000000 + (i * 0x1000);
        vmas[i].vm_end = vmas[i].vm_start + 0x1000;
        vmas[i].vm_flags = VM_READ | VM_WRITE;
        vmas[i].vm_prot = PROT_READ | PROT_WRITE;
        vmas[i].vm_mm = mm;
        
        insert_vm_area(mm, &vmas[i]);
    }
    
    /* Test COW setup performance */
    for (int i = 0; i < PERF_COW_PAGES; i++) {
        uint64_t start = get_timestamp_us();
        int result = setup_cow_mapping(&vmas[i]);
        uint64_t end = get_timestamp_us();
        
        if (result == USMM_SUCCESS) {
            update_perf_stats(&cow_setup_stats, end - start);
        }
    }
    
    /* Test COW fault handling performance */
    for (int i = 0; i < PERF_COW_PAGES; i++) {
        uint64_t addr = vmas[i].vm_start + 0x500;
        
        uint64_t start = get_timestamp_us();
        int result = cow_page_fault(&vmas[i], addr);
        uint64_t end = get_timestamp_us();
        
        if (result == USMM_SUCCESS || result == -USMM_EFAULT) {
            update_perf_stats(&cow_fault_stats, end - start);
        }
    }
    
    print_perf_stats("COW Setup", &cow_setup_stats);
    print_perf_stats("COW Page Fault", &cow_fault_stats);
    
    free(vmas);
    mm_free(mm);
    return 0;
}

/* ========================== Stress Tests ========================== */

int test_memory_stress(void) {
    printf("\nRunning Memory Stress Test...\n");
    
    const int stress_iterations = 10000;
    const int concurrent_mappings = 1000;
    
    void** mappings = malloc(concurrent_mappings * sizeof(void*));
    if (!mappings) {
        return -1;
    }
    
    uint64_t start_time = get_timestamp_us();
    
    for (int iter = 0; iter < stress_iterations; iter++) {
        /* Create random mappings */
        for (int i = 0; i < concurrent_mappings; i++) {
            size_t size = 4096 + (rand() % 16) * 4096; /* 4KB to 64KB */
            int flags = MAP_PRIVATE | MAP_ANONYMOUS;
            int prot = PROT_READ | PROT_WRITE;
            
            mappings[i] = sys_mmap(NULL, size, prot, flags, -1, 0);
            if (mappings[i] == (void*)-1) {
                mappings[i] = NULL;
            }
        }
        
        /* Randomly modify protections */
        for (int i = 0; i < concurrent_mappings / 2; i++) {
            int idx = rand() % concurrent_mappings;
            if (mappings[idx]) {
                sys_mprotect(mappings[idx], 4096, PROT_READ);
            }
        }
        
        /* Clean up mappings */
        for (int i = 0; i < concurrent_mappings; i++) {
            if (mappings[i]) {
                sys_munmap(mappings[i], 4096);
                mappings[i] = NULL;
            }
        }
        
        if (iter % 1000 == 0) {
            printf("  Completed %d iterations\n", iter);
        }
    }
    
    uint64_t end_time = get_timestamp_us();
    uint64_t total_time = end_time - start_time;
    
    printf("Stress test completed:\n");
    printf("  Iterations: %d\n", stress_iterations);
    printf("  Total time: %lu us (%.2f seconds)\n", total_time, total_time / 1000000.0);
    printf("  Avg time per iteration: %lu us\n", total_time / stress_iterations);
    
    free(mappings);
    return 0;
}

/* ========================== Concurrent Access Tests ========================== */

int test_concurrent_access(void) {
    printf("\nTesting Concurrent Access Patterns...\n");
    
    mm_struct_t* mm = mm_alloc();
    if (!mm) {
        return -1;
    }
    
    const int num_threads = 4;
    const int ops_per_thread = 1000;
    
    printf("Simulating %d concurrent threads with %d operations each\n", 
           num_threads, ops_per_thread);
    
    /* Simulate concurrent VMA operations */
    perf_stats_t concurrent_stats;
    init_perf_stats(&concurrent_stats);
    
    uint64_t start_time = get_timestamp_us();
    
    for (int thread = 0; thread < num_threads; thread++) {
        for (int op = 0; op < ops_per_thread; op++) {
            vm_area_struct_t vma;
            memset(&vma, 0, sizeof(vma));
            
            uint64_t base = 0x60000000 + (thread * 0x10000000);
            vma.vm_start = base + (op * 0x1000);
            vma.vm_end = vma.vm_start + 0x1000;
            vma.vm_flags = VM_READ | VM_WRITE;
            vma.vm_prot = PROT_READ | PROT_WRITE;
            
            uint64_t op_start = get_timestamp_us();
            
            /* Insert VMA */
            insert_vm_area(mm, &vma);
            
            /* Lookup VMA */
            find_vma(mm, vma.vm_start + 0x500);
            
            /* Remove VMA */
            remove_vm_area(mm, &vma);
            
            uint64_t op_end = get_timestamp_us();
            update_perf_stats(&concurrent_stats, op_end - op_start);
        }
    }
    
    uint64_t end_time = get_timestamp_us();
    
    printf("Concurrent access test completed:\n");
    printf("  Total operations: %d\n", num_threads * ops_per_thread);
    printf("  Total time: %lu us\n", end_time - start_time);
    print_perf_stats("Concurrent Operations", &concurrent_stats);
    
    mm_free(mm);
    return 0;
}

/* ========================== Main Performance Test Runner ========================== */

int main(void) {
    printf("IKOS User Space Memory Management Performance Test Suite\n");
    printf("========================================================\n\n");
    
    /* Initialize USMM */
    if (usmm_init() != USMM_SUCCESS) {
        printf("Failed to initialize USMM\n");
        return 1;
    }
    
    /* Run performance tests */
    if (test_vma_performance() != 0) {
        printf("VMA performance test failed\n");
    }
    
    if (test_mmap_performance() != 0) {
        printf("Memory mapping performance test failed\n");
    }
    
    if (test_shm_performance() != 0) {
        printf("Shared memory performance test failed\n");
    }
    
    if (test_cow_performance() != 0) {
        printf("Copy-on-Write performance test failed\n");
    }
    
    if (test_memory_stress() != 0) {
        printf("Memory stress test failed\n");
    }
    
    if (test_concurrent_access() != 0) {
        printf("Concurrent access test failed\n");
    }
    
    printf("\nPerformance testing completed\n");
    
    /* Cleanup */
    usmm_shutdown();
    
    return 0;
}

/* Helper function implementations */
#include <stdlib.h>

/* Simple malloc/free stubs if not available */
#ifndef malloc
void* malloc(size_t size) {
    static char buffer[1024 * 1024]; /* 1MB static buffer */
    static size_t offset = 0;
    
    if (offset + size > sizeof(buffer)) {
        return NULL;
    }
    
    void* ptr = buffer + offset;
    offset += (size + 7) & ~7; /* 8-byte align */
    return ptr;
}

void free(void* ptr) {
    /* No-op for static buffer */
}
#endif

/* Simple random number generator if not available */
#ifndef rand
static unsigned long rand_seed = 1;

int rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed / 65536) % 32768;
}
#endif
