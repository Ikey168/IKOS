/* VMM Stub Implementation for Testing
 * Provides minimal VMM functionality for user-space process execution tests
 */

#include "vmm.h"
#include <stdint.h>
#include <stddef.h>

/* Mock global variables for VMM */
static uint64_t mock_physical_memory = 0x10000000; /* 256MB mock memory */
static vm_space_t mock_address_spaces[16];
static int next_space_id = 0;

/* Kernel symbols that VMM expects */
uint64_t kernel_pml4_table = 0;
uint64_t total_frames = 65536; /* 256MB / 4KB */
void* frame_database = &total_frames; /* Just point to something valid */

/* VMM initialization and management */
int vmm_init(uint64_t memory_size) {
    (void)memory_size;
    return VMM_SUCCESS;
}

void vmm_shutdown(void) {
    /* Nothing to do */
}

/* Address space management */
vm_space_t* vmm_create_address_space(uint32_t pid) {
    if (next_space_id >= 16) {
        return NULL;
    }
    
    vm_space_t* space = &mock_address_spaces[next_space_id++];
    space->owner_pid = pid;
    space->pml4_phys = 0x1000000 + (pid * 0x1000); /* Mock physical address */
    space->pml4_virt = (pte_t*)space->pml4_phys;
    space->regions = NULL;
    space->heap_start = USER_HEAP_BASE;
    space->heap_end = USER_HEAP_BASE;
    space->stack_start = USER_STACK_TOP - 0x200000;
    space->mmap_start = 0x40000000;
    space->region_count = 0;
    space->page_count = 0;
    
    return space;
}

void vmm_destroy_address_space(vm_space_t* space) {
    if (space) {
        /* Clear the space */
        space->owner_pid = 0;
        space->pml4_phys = 0;
        space->pml4_virt = NULL;
    }
}

int vmm_switch_address_space(vm_space_t* space) {
    (void)space;
    return VMM_SUCCESS;
}

vm_space_t* vmm_get_current_space(void) {
    return &mock_address_spaces[0]; /* Return first space */
}

/* Page allocation and mapping */
uint64_t vmm_alloc_page(void) {
    static uint64_t next_page = 0x2000000; /* Start at 32MB */
    uint64_t page = next_page;
    next_page += PAGE_SIZE;
    return page;
}

void vmm_free_page(uint64_t phys_addr) {
    (void)phys_addr;
    /* Nothing to do in mock */
}

int vmm_map_page(vm_space_t* space, uint64_t virt_addr, uint64_t phys_addr, uint32_t flags) {
    (void)space;
    (void)virt_addr;
    (void)phys_addr;
    (void)flags;
    return VMM_SUCCESS; /* Always succeed in mock */
}

int vmm_unmap_page(vm_space_t* space, uint64_t virt_addr) {
    (void)space;
    (void)virt_addr;
    return VMM_SUCCESS;
}

uint64_t vmm_get_physical_addr(vm_space_t* space, uint64_t virt_addr) {
    (void)space;
    return virt_addr; /* Identity mapping in mock */
}

/* Other required functions with minimal implementations */
vm_region_t* vmm_create_region(vm_space_t* space, uint64_t start, uint64_t size, 
                               uint32_t flags, vmm_region_type_t type, const char* name) {
    (void)space; (void)start; (void)size; (void)flags; (void)type; (void)name;
    return NULL; /* Not needed for basic tests */
}

int vmm_destroy_region(vm_space_t* space, uint64_t start) {
    (void)space; (void)start;
    return VMM_SUCCESS;
}

vm_region_t* vmm_find_region(vm_space_t* space, uint64_t addr) {
    (void)space; (void)addr;
    return NULL;
}

vmm_stats_t* vmm_get_stats(void) {
    static vmm_stats_t stats = {
        .total_pages = 65536,
        .free_pages = 32768,
        .allocated_pages = 32768,
        .page_faults = 0
    };
    return &stats;
}

/* Smoke test specific function */
void vmm_smoke_test(void) {
    /* Minimal smoke test implementation */
}