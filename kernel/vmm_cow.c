/* IKOS Virtual Memory Manager - Copy-on-Write Implementation
 * Handles copy-on-write semantics for efficient memory sharing
 */

#include "vmm.h"
#include "memory.h"
#include <string.h>

/* Forward declarations for functions defined in vmm.c */
extern page_frame_t* frame_database;
extern uint64_t total_frames;

/* Helper functions - static definitions to avoid conflicts */
static uint64_t cow_pte_to_phys(pte_t entry) {
    return entry & 0xFFFFFFFFFFFFF000ULL;
}

static pte_t cow_phys_to_pte(uint64_t phys, uint32_t flags) {
    return (phys & 0xFFFFFFFFFFFFF000ULL) | (flags & 0xFFF);
}

/**
 * Handle copy-on-write page fault
 */
int vmm_handle_cow_fault(vm_space_t* space, uint64_t fault_addr) {
    if (!space) {
        return VMM_ERROR_INVALID_ADDR;
    }
    
    uint64_t page_addr = vmm_align_down(fault_addr, PAGE_SIZE);
    
    // Get current page table entry
    pte_t* pte = vmm_get_page_table(space, page_addr, PT_LEVEL, false);
    if (!pte || !(*pte & PAGE_PRESENT)) {
        return VMM_ERROR_NOT_FOUND;
    }
    
    uint64_t old_phys = cow_pte_to_phys(*pte);
    
    // Get frame reference
    uint32_t frame_num = PAGE_FRAME(old_phys);
    if (frame_num >= total_frames) {
        return VMM_ERROR_INVALID_ADDR;
    }
    
    page_frame_t* frame = &frame_database[frame_num];
    
    // If reference count is 1, just remove COW flag
    if (frame->ref_count == 1) {
        *pte |= PAGE_WRITABLE;
        vmm_flush_tlb_page(page_addr);
        return VMM_SUCCESS;
    }
    
    // Allocate new page for copy
    uint64_t new_phys = vmm_alloc_page();
    if (!new_phys) {
        return VMM_ERROR_NOMEM;
    }
    
    // Copy page content
    void* old_virt = (void*)(old_phys + KERNEL_VIRTUAL_BASE);
    void* new_virt = (void*)(new_phys + KERNEL_VIRTUAL_BASE);
    memcpy(new_virt, old_virt, PAGE_SIZE);
    
    // Update page table entry
    uint32_t flags = (*pte & 0xFFF) | PAGE_WRITABLE;
    *pte = cow_phys_to_pte(new_phys, flags);
    
    // Decrease reference count of old page
    frame->ref_count--;
    
    // Flush TLB
    vmm_flush_tlb_page(page_addr);
    
    return VMM_SUCCESS;
}

/**
 * Create copy-on-write mapping
 */
int vmm_map_cow_page(vm_space_t* dst_space, vm_space_t* src_space, 
                     uint64_t virt_addr, uint32_t flags) {
    if (!dst_space || !src_space) {
        return VMM_ERROR_INVALID_ADDR;
    }
    
    uint64_t page_addr = vmm_align_down(virt_addr, PAGE_SIZE);
    
    // Get source page table entry
    pte_t* src_pte = vmm_get_page_table(src_space, page_addr, PT_LEVEL, false);
    if (!src_pte || !(*src_pte & PAGE_PRESENT)) {
        return VMM_ERROR_NOT_FOUND;
    }
    
    uint64_t phys_addr = cow_pte_to_phys(*src_pte);
    
    // Remove write permission from source page
    *src_pte &= ~PAGE_WRITABLE;
    vmm_flush_tlb_page(page_addr);
    
    // Map in destination space without write permission
    uint32_t cow_flags = (flags & ~PAGE_WRITABLE) | PAGE_PRESENT;
    int result = vmm_map_page(dst_space, page_addr, phys_addr, cow_flags);
    if (result != VMM_SUCCESS) {
        return result;
    }
    
    // Increase reference count
    uint32_t frame_num = PAGE_FRAME(phys_addr);
    page_frame_t* frame = &frame_database[frame_num];
    frame->ref_count++;
    
    return VMM_SUCCESS;
}

/**
 * Copy entire address space (for fork)
 */
vm_space_t* vmm_copy_address_space(vm_space_t* src_space, uint32_t new_pid) {
    if (!src_space) {
        return NULL;
    }
    
    // Create new address space
    vm_space_t* dst_space = vmm_create_address_space(new_pid);
    if (!dst_space) {
        return NULL;
    }
    
    // Copy all regions
    vm_region_t* region = src_space->regions;
    while (region) {
        // Create corresponding region in destination
        vm_region_t* new_region = vmm_create_region(dst_space, 
                                                    region->start_addr,
                                                    region->end_addr - region->start_addr,
                                                    region->flags | VMM_FLAG_COW,
                                                    region->type,
                                                    region->name);
        if (!new_region) {
            vmm_destroy_address_space(dst_space);
            return NULL;
        }
        
        // Copy pages with COW semantics
        for (uint64_t addr = region->start_addr; addr < region->end_addr; addr += PAGE_SIZE) {
            if (vmm_get_physical_addr(src_space, addr) != 0) {
                int result = vmm_map_cow_page(dst_space, src_space, addr, region->flags);
                if (result != VMM_SUCCESS) {
                    // Continue with best effort
                }
            }
        }
        
        region = region->next;
    }
    
    // Copy memory layout information
    dst_space->heap_start = src_space->heap_start;
    dst_space->heap_end = src_space->heap_end;
    dst_space->stack_start = src_space->stack_start;
    dst_space->mmap_start = src_space->mmap_start;
    
    return dst_space;
}
