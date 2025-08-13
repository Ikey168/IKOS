/* IKOS Virtual Memory Manager Implementation
 * Provides paging-based virtual memory management with isolated address spaces
 */

#include "vmm.h"
#include "memory.h"
#include "scheduler.h"
#include <string.h>

/* Global VMM state */
static bool vmm_initialized = false;
static vm_space_t* current_space = NULL;
static vm_space_t* kernel_space = NULL;

/* Physical memory management */
static page_frame_t* free_frames = NULL;
static page_frame_t* frame_database = NULL;
static uint64_t total_frames = 0;
static uint64_t free_frame_count = 0;

/* VMM statistics */
static vmm_stats_t vmm_statistics;

/* Initial memory mappings */
extern uint64_t kernel_pml4_table;  /* From bootloader */

/* Forward declarations */
static int setup_kernel_mappings(void);
static pte_t* allocate_page_table(void);
static void free_page_table(pte_t* table);
static uint64_t pte_to_phys(pte_t entry);
static pte_t phys_to_pte(uint64_t phys, uint32_t flags);
static int map_page_internal(vm_space_t* space, uint64_t virt, uint64_t phys, uint32_t flags);

/**
 * Initialize the Virtual Memory Manager
 */
int vmm_init(uint64_t memory_size) {
    if (vmm_initialized) {
        return VMM_SUCCESS;
    }
    
    // Initialize statistics
    memset(&vmm_statistics, 0, sizeof(vmm_statistics));
    
    // Initialize physical memory management
    if (vmm_init_physical_memory(memory_size) != VMM_SUCCESS) {
        return VMM_ERROR_NOMEM;
    }
    
    // Create kernel address space
    kernel_space = vmm_create_address_space(0); // PID 0 for kernel
    if (!kernel_space) {
        return VMM_ERROR_NOMEM;
    }
    
    // Setup initial kernel mappings
    if (setup_kernel_mappings() != VMM_SUCCESS) {
        return VMM_ERROR_FAULT;
    }
    
    current_space = kernel_space;
    vmm_initialized = true;
    
    return VMM_SUCCESS;
}

/**
 * Shutdown VMM
 */
void vmm_shutdown(void) {
    if (!vmm_initialized) {
        return;
    }
    
    // Cleanup would go here
    vmm_initialized = false;
}

/**
 * Create a new address space for a process
 */
vm_space_t* vmm_create_address_space(uint32_t pid) {
    vm_space_t* space = (vm_space_t*)kmalloc(sizeof(vm_space_t));
    if (!space) {
        return NULL;
    }
    
    memset(space, 0, sizeof(vm_space_t));
    
    // Allocate PML4 table
    space->pml4_virt = allocate_page_table();
    if (!space->pml4_virt) {
        kfree(space);
        return NULL;
    }
    
    space->pml4_phys = vmm_get_physical_addr(kernel_space, (uint64_t)space->pml4_virt);
    space->owner_pid = pid;
    
    // Initialize address space layout
    space->heap_start = USER_HEAP_BASE;
    space->heap_end = USER_HEAP_BASE;
    space->stack_start = USER_STACK_TOP;
    space->mmap_start = USER_STACK_TOP - 0x40000000ULL; // 1GB below stack
    
    // If this is a user space, copy kernel mappings
    if (pid != 0 && kernel_space) {
        // Copy upper half (kernel mappings) from kernel space
        for (int i = 256; i < 512; i++) {
            space->pml4_virt[i] = kernel_space->pml4_virt[i];
        }
    }
    
    vmm_statistics.allocated_pages++;
    
    return space;
}

/**
 * Destroy an address space
 */
void vmm_destroy_address_space(vm_space_t* space) {
    if (!space || space == kernel_space) {
        return;
    }
    
    // Free all regions
    vm_region_t* region = space->regions;
    while (region) {
        vm_region_t* next = region->next;
        
        // Unmap all pages in the region
        for (uint64_t addr = region->start_addr; addr < region->end_addr; addr += PAGE_SIZE) {
            vmm_unmap_page(space, addr);
        }
        
        kfree(region);
        region = next;
    }
    
    // Free page tables (except kernel mappings)
    for (int i = 0; i < 256; i++) {
        if (space->pml4_virt[i] & PAGE_PRESENT) {
            pte_t* pdpt = (pte_t*)pte_to_phys(space->pml4_virt[i]);
            // Would recursively free page tables here
            // Simplified for this implementation
        }
    }
    
    // Free PML4 table
    free_page_table(space->pml4_virt);
    
    // Free space structure
    kfree(space);
    
    vmm_statistics.allocated_pages--;
}

/**
 * Switch to a different address space
 */
int vmm_switch_address_space(vm_space_t* space) {
    if (!space) {
        return VMM_ERROR_INVALID_ADDR;
    }
    
    current_space = space;
    
    // Load CR3 with new page directory
    __asm__ volatile("mov %0, %%cr3" : : "r"(space->pml4_phys) : "memory");
    
    return VMM_SUCCESS;
}

/**
 * Get current address space
 */
vm_space_t* vmm_get_current_space(void) {
    return current_space;
}

/**
 * Create a memory region
 */
vm_region_t* vmm_create_region(vm_space_t* space, uint64_t start, uint64_t size, 
                               uint32_t flags, vmm_region_type_t type, const char* name) {
    if (!space || size == 0) {
        return NULL;
    }
    
    // Align addresses
    start = vmm_align_down(start, PAGE_SIZE);
    uint64_t end = vmm_align_up(start + size, PAGE_SIZE);
    
    // Check for overlap with existing regions
    vm_region_t* existing = vmm_find_region(space, start);
    if (existing && start < existing->end_addr) {
        return NULL; // Overlap
    }
    
    // Allocate region structure
    vm_region_t* region = (vm_region_t*)kmalloc(sizeof(vm_region_t));
    if (!region) {
        return NULL;
    }
    
    // Initialize region
    memset(region, 0, sizeof(vm_region_t));
    region->start_addr = start;
    region->end_addr = end;
    region->flags = flags;
    region->type = type;
    if (name) {
        strncpy(region->name, name, sizeof(region->name) - 1);
    }
    
    // Insert into region list (sorted by address)
    if (!space->regions || start < space->regions->start_addr) {
        region->next = space->regions;
        if (space->regions) {
            space->regions->prev = region;
        }
        space->regions = region;
    } else {
        vm_region_t* current = space->regions;
        while (current->next && current->next->start_addr < start) {
            current = current->next;
        }
        region->next = current->next;
        region->prev = current;
        if (current->next) {
            current->next->prev = region;
        }
        current->next = region;
    }
    
    space->region_count++;
    
    return region;
}

/**
 * Find memory region containing address
 */
vm_region_t* vmm_find_region(vm_space_t* space, uint64_t addr) {
    if (!space) {
        return NULL;
    }
    
    vm_region_t* region = space->regions;
    while (region) {
        if (addr >= region->start_addr && addr < region->end_addr) {
            return region;
        }
        region = region->next;
    }
    
    return NULL;
}

/**
 * Allocate a physical page frame
 */
uint64_t vmm_alloc_page(void) {
    if (!free_frames || free_frame_count == 0) {
        return 0; // Out of memory
    }
    
    page_frame_t* frame = free_frames;
    free_frames = frame->next;
    free_frame_count--;
    
    frame->ref_count = 1;
    frame->next = NULL;
    
    vmm_statistics.allocated_pages++;
    vmm_statistics.free_pages--;
    
    return FRAME_ADDR(frame->frame_number);
}

/**
 * Free a physical page frame
 */
void vmm_free_page(uint64_t phys_addr) {
    if (phys_addr == 0) {
        return;
    }
    
    uint32_t frame_num = PAGE_FRAME(phys_addr);
    if (frame_num >= total_frames) {
        return; // Invalid frame
    }
    
    page_frame_t* frame = &frame_database[frame_num];
    
    if (frame->ref_count > 0) {
        frame->ref_count--;
    }
    
    if (frame->ref_count == 0) {
        // Add to free list
        frame->next = free_frames;
        free_frames = frame;
        free_frame_count++;
        
        vmm_statistics.allocated_pages--;
        vmm_statistics.free_pages++;
    }
}

/**
 * Map a virtual page to physical page
 */
int vmm_map_page(vm_space_t* space, uint64_t virt_addr, uint64_t phys_addr, uint32_t flags) {
    if (!space) {
        return VMM_ERROR_INVALID_ADDR;
    }
    
    return map_page_internal(space, virt_addr, phys_addr, flags);
}

/**
 * Unmap a virtual page
 */
int vmm_unmap_page(vm_space_t* space, uint64_t virt_addr) {
    if (!space) {
        return VMM_ERROR_INVALID_ADDR;
    }
    
    virt_addr = vmm_align_down(virt_addr, PAGE_SIZE);
    
    // Get page table entry
    pte_t* pte = vmm_get_page_table(space, virt_addr, PT_LEVEL, false);
    if (!pte || !(*pte & PAGE_PRESENT)) {
        return VMM_ERROR_NOT_FOUND;
    }
    
    // Free physical page
    uint64_t phys_addr = pte_to_phys(*pte);
    vmm_free_page(phys_addr);
    
    // Clear page table entry
    *pte = 0;
    
    // Invalidate TLB entry
    vmm_flush_tlb_page(virt_addr);
    
    return VMM_SUCCESS;
}

/**
 * Get physical address for virtual address
 */
uint64_t vmm_get_physical_addr(vm_space_t* space, uint64_t virt_addr) {
    if (!space) {
        return 0;
    }
    
    pte_t* pte = vmm_get_page_table(space, virt_addr, PT_LEVEL, false);
    if (!pte || !(*pte & PAGE_PRESENT)) {
        return 0;
    }
    
    uint64_t phys_base = pte_to_phys(*pte);
    uint64_t offset = virt_addr & (PAGE_SIZE - 1);
    
    return phys_base + offset;
}

/**
 * Allocate virtual memory
 */
void* vmm_alloc_virtual(vm_space_t* space, uint64_t size, uint32_t flags) {
    if (!space || size == 0) {
        return NULL;
    }
    
    size = vmm_align_up(size, PAGE_SIZE);
    
    // Find free virtual address space
    uint64_t start_addr = USER_VIRTUAL_BASE;
    if (space->regions) {
        // Simple allocation after last region
        vm_region_t* last = space->regions;
        while (last->next) {
            last = last->next;
        }
        start_addr = vmm_align_up(last->end_addr, PAGE_SIZE);
    }
    
    // Create region
    vm_region_t* region = vmm_create_region(space, start_addr, size, flags, 
                                            VMM_REGION_HEAP, "heap");
    if (!region) {
        return NULL;
    }
    
    // Allocate and map pages if not lazy
    if (!(flags & VMM_FLAG_LAZY)) {
        for (uint64_t addr = start_addr; addr < start_addr + size; addr += PAGE_SIZE) {
            uint64_t phys = vmm_alloc_page();
            if (!phys) {
                // Cleanup on failure
                vmm_free_virtual(space, (void*)start_addr, addr - start_addr);
                return NULL;
            }
            
            uint32_t page_flags = PAGE_PRESENT | PAGE_WRITABLE;
            if (flags & VMM_FLAG_USER) {
                page_flags |= PAGE_USER;
            }
            
            if (vmm_map_page(space, addr, phys, page_flags) != VMM_SUCCESS) {
                vmm_free_page(phys);
                vmm_free_virtual(space, (void*)start_addr, addr - start_addr);
                return NULL;
            }
        }
    }
    
    return (void*)start_addr;
}

/**
 * Free virtual memory
 */
void vmm_free_virtual(vm_space_t* space, void* addr, uint64_t size) {
    if (!space || !addr || size == 0) {
        return;
    }
    
    uint64_t start = vmm_align_down((uint64_t)addr, PAGE_SIZE);
    uint64_t end = vmm_align_up(start + size, PAGE_SIZE);
    
    // Unmap all pages in range
    for (uint64_t virt = start; virt < end; virt += PAGE_SIZE) {
        vmm_unmap_page(space, virt);
    }
    
    // Remove region
    vmm_destroy_region(space, start);
}

/**
 * Get page table for virtual address
 */
pte_t* vmm_get_page_table(vm_space_t* space, uint64_t virt_addr, int level, bool create) {
    if (!space) {
        return NULL;
    }
    
    pte_t* table = space->pml4_virt;
    uint64_t addr = virt_addr;
    
    // Walk through page table levels
    for (int current_level = PML4_LEVEL; current_level > level; current_level--) {
        int shift = 12 + (current_level * 9);
        int index = (addr >> shift) & 0x1FF;
        
        pte_t entry = table[index];
        
        if (!(entry & PAGE_PRESENT)) {
            if (!create) {
                return NULL;
            }
            
            // Allocate new page table
            pte_t* new_table = allocate_page_table();
            if (!new_table) {
                return NULL;
            }
            
            uint64_t phys = vmm_get_physical_addr(kernel_space, (uint64_t)new_table);
            table[index] = phys_to_pte(phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
            entry = table[index];
        }
        
        table = (pte_t*)pte_to_phys(entry);
        
        // For the kernel space, convert physical to virtual
        if (space == kernel_space) {
            table = (pte_t*)((uint64_t)table + KERNEL_VIRTUAL_BASE);
        }
    }
    
    // Return pointer to specific entry
    int shift = 12 + (level * 9);
    int index = (addr >> shift) & 0x1FF;
    return &table[index];
}

/**
 * Handle page fault
 */
int vmm_handle_page_fault(page_fault_info_t* fault_info) {
    if (!fault_info) {
        return VMM_ERROR_FAULT;
    }
    
    vmm_statistics.page_faults++;
    
    vm_space_t* space = vmm_get_current_space();
    if (!space) {
        return VMM_ERROR_FAULT;
    }
    
    uint64_t fault_addr = fault_info->fault_addr;
    
    // Find the region containing the fault address
    vm_region_t* region = vmm_find_region(space, fault_addr);
    if (!region) {
        vmm_statistics.major_faults++;
        return VMM_ERROR_NOT_FOUND; // Segmentation fault
    }
    
    // Check permissions
    if (fault_info->is_write && !(region->flags & VMM_FLAG_WRITE)) {
        return VMM_ERROR_PERM_DENIED;
    }
    
    if (fault_info->is_instruction_fetch && !(region->flags & VMM_FLAG_EXEC)) {
        return VMM_ERROR_PERM_DENIED;
    }
    
    // Handle copy-on-write
    if (fault_info->is_write && (region->flags & VMM_FLAG_COW)) {
        return vmm_handle_cow_fault(space, fault_addr);
    }
    
    // Handle lazy allocation
    if (region->flags & VMM_FLAG_LAZY) {
        uint64_t page_addr = vmm_align_down(fault_addr, PAGE_SIZE);
        uint64_t phys = vmm_alloc_page();
        if (!phys) {
            return VMM_ERROR_NOMEM;
        }
        
        uint32_t page_flags = PAGE_PRESENT;
        if (region->flags & VMM_FLAG_WRITE) page_flags |= PAGE_WRITABLE;
        if (region->flags & VMM_FLAG_USER) page_flags |= PAGE_USER;
        
        int result = vmm_map_page(space, page_addr, phys, page_flags);
        if (result != VMM_SUCCESS) {
            vmm_free_page(phys);
            return result;
        }
        
        vmm_statistics.minor_faults++;
        return VMM_SUCCESS;
    }
    
    vmm_statistics.major_faults++;
    return VMM_ERROR_FAULT;
}

/**
 * Page fault handler (called from interrupt)
 */
void vmm_page_fault_handler(uint64_t fault_addr, uint64_t error_code) {
    page_fault_info_t fault_info;
    
    fault_info.fault_addr = fault_addr;
    fault_info.error_code = error_code;
    fault_info.is_present = (error_code & 0x1) != 0;
    fault_info.is_write = (error_code & 0x2) != 0;
    fault_info.is_user = (error_code & 0x4) != 0;
    fault_info.is_instruction_fetch = (error_code & 0x10) != 0;
    
    // Get instruction pointer
    __asm__ volatile("mov %%rsp, %0" : "=r"(fault_info.instruction_ptr));
    
    int result = vmm_handle_page_fault(&fault_info);
    
    if (result != VMM_SUCCESS) {
        // Page fault could not be resolved - this would typically
        // result in process termination or kernel panic
        // For now, just record the error
    }
}

/**
 * Initialize physical memory management
 */
int vmm_init_physical_memory(uint64_t memory_size) {
    total_frames = memory_size / PAGE_SIZE;
    
    // Allocate frame database
    frame_database = (page_frame_t*)kmalloc(total_frames * sizeof(page_frame_t));
    if (!frame_database) {
        return VMM_ERROR_NOMEM;
    }
    
    // Initialize frame database
    for (uint64_t i = 0; i < total_frames; i++) {
        frame_database[i].frame_number = i;
        frame_database[i].ref_count = 0;
        frame_database[i].flags = 0;
        frame_database[i].owner_pid = 0;
        
        // Add to free list (skip first 1MB for kernel/BIOS)
        if (i >= 256) { // Skip first 1MB
            frame_database[i].next = free_frames;
            free_frames = &frame_database[i];
            free_frame_count++;
        }
    }
    
    vmm_statistics.total_pages = total_frames;
    vmm_statistics.free_pages = free_frame_count;
    
    return VMM_SUCCESS;
}

/**
 * Flush TLB
 */
void vmm_flush_tlb(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

/**
 * Flush single TLB page
 */
void vmm_flush_tlb_page(uint64_t virt_addr) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

/**
 * Get VMM statistics
 */
vmm_stats_t* vmm_get_stats(void) {
    vmm_statistics.memory_usage = (vmm_statistics.allocated_pages * PAGE_SIZE);
    return &vmm_statistics;
}

/**
 * Check if address is in user space
 */
bool vmm_is_user_addr(uint64_t addr) {
    return addr >= USER_VIRTUAL_BASE && addr < USER_VIRTUAL_END;
}

/**
 * Check if address is in kernel space
 */
bool vmm_is_kernel_addr(uint64_t addr) {
    return addr >= KERNEL_VIRTUAL_BASE;
}

/**
 * Align address down
 */
uint64_t vmm_align_down(uint64_t addr, uint64_t alignment) {
    return addr & ~(alignment - 1);
}

/**
 * Align address up
 */
uint64_t vmm_align_up(uint64_t addr, uint64_t alignment) {
    return (addr + alignment - 1) & ~(alignment - 1);
}

/* Helper Functions */

/**
 * Setup initial kernel mappings
 */
static int setup_kernel_mappings(void) {
    // Use existing PML4 from bootloader
    kernel_space->pml4_phys = (uint64_t)&kernel_pml4_table;
    kernel_space->pml4_virt = (pte_t*)((uint64_t)&kernel_pml4_table + KERNEL_VIRTUAL_BASE);
    
    // Create kernel regions
    vmm_create_region(kernel_space, KERNEL_VIRTUAL_BASE, 0x200000, 
                      VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_EXEC,
                      VMM_REGION_KERNEL, "kernel_code");
    
    return VMM_SUCCESS;
}

/**
 * Allocate a page table
 */
static pte_t* allocate_page_table(void) {
    uint64_t phys = vmm_alloc_page();
    if (!phys) {
        return NULL;
    }
    
    // Convert to virtual address for kernel access
    pte_t* table = (pte_t*)(phys + KERNEL_VIRTUAL_BASE);
    
    // Clear the table
    memset(table, 0, PAGE_SIZE);
    
    return table;
}

/**
 * Free a page table
 */
static void free_page_table(pte_t* table) {
    if (!table) {
        return;
    }
    
    uint64_t phys = (uint64_t)table - KERNEL_VIRTUAL_BASE;
    vmm_free_page(phys);
}

/**
 * Extract physical address from page table entry
 */
static uint64_t pte_to_phys(pte_t entry) {
    return entry & 0xFFFFFFFFFFFFF000ULL;
}

/**
 * Create page table entry from physical address and flags
 */
static pte_t phys_to_pte(uint64_t phys, uint32_t flags) {
    return (phys & 0xFFFFFFFFFFFFF000ULL) | (flags & 0xFFF);
}

/**
 * Internal page mapping function
 */
static int map_page_internal(vm_space_t* space, uint64_t virt, uint64_t phys, uint32_t flags) {
    virt = vmm_align_down(virt, PAGE_SIZE);
    phys = vmm_align_down(phys, PAGE_SIZE);
    
    pte_t* pte = vmm_get_page_table(space, virt, PT_LEVEL, true);
    if (!pte) {
        return VMM_ERROR_NOMEM;
    }
    
    // Check if page is already mapped
    if (*pte & PAGE_PRESENT) {
        return VMM_ERROR_EXISTS;
    }
    
    // Set page table entry
    *pte = phys_to_pte(phys, flags);
    
    // Invalidate TLB entry
    vmm_flush_tlb_page(virt);
    
    return VMM_SUCCESS;
}
