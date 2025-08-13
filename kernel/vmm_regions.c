/* IKOS Virtual Memory Manager - Region Management
 * Handles virtual memory region operations and management
 */

#include "vmm.h"
#include "memory.h"
#include <string.h>

/**
 * Destroy a memory region
 */
int vmm_destroy_region(vm_space_t* space, uint64_t addr) {
    if (!space) {
        return VMM_ERROR_INVALID_ADDR;
    }
    
    vm_region_t* region = vmm_find_region(space, addr);
    if (!region) {
        return VMM_ERROR_NOT_FOUND;
    }
    
    // Unmap all pages in region
    for (uint64_t virt = region->start_addr; virt < region->end_addr; virt += PAGE_SIZE) {
        vmm_unmap_page(space, virt);
    }
    
    // Remove from linked list
    if (region->prev) {
        region->prev->next = region->next;
    } else {
        space->regions = region->next;
    }
    
    if (region->next) {
        region->next->prev = region->prev;
    }
    
    space->region_count--;
    kfree(region);
    
    return VMM_SUCCESS;
}

/**
 * Split a memory region at specified address
 */
int vmm_split_region(vm_space_t* space, uint64_t split_addr) {
    if (!space) {
        return VMM_ERROR_INVALID_ADDR;
    }
    
    vm_region_t* region = vmm_find_region(space, split_addr);
    if (!region || split_addr <= region->start_addr || split_addr >= region->end_addr) {
        return VMM_ERROR_NOT_FOUND;
    }
    
    // Align split address
    split_addr = vmm_align_up(split_addr, PAGE_SIZE);
    
    // Create new region for upper part
    vm_region_t* upper_region = (vm_region_t*)kmalloc(sizeof(vm_region_t));
    if (!upper_region) {
        return VMM_ERROR_NOMEM;
    }
    
    // Copy region data
    *upper_region = *region;
    upper_region->start_addr = split_addr;
    
    // Update original region
    region->end_addr = split_addr;
    
    // Insert upper region after original
    upper_region->next = region->next;
    upper_region->prev = region;
    if (region->next) {
        region->next->prev = upper_region;
    }
    region->next = upper_region;
    
    space->region_count++;
    
    return VMM_SUCCESS;
}

/**
 * Merge adjacent compatible regions
 */
int vmm_merge_regions(vm_space_t* space, vm_region_t* region1, vm_region_t* region2) {
    if (!space || !region1 || !region2) {
        return VMM_ERROR_INVALID_ADDR;
    }
    
    // Ensure regions are adjacent and compatible
    if (region1->end_addr != region2->start_addr ||
        region1->flags != region2->flags ||
        region1->type != region2->type) {
        return VMM_ERROR_INVALID_ADDR;
    }
    
    // Extend first region
    region1->end_addr = region2->end_addr;
    
    // Remove second region from list
    if (region2->prev) {
        region2->prev->next = region2->next;
    }
    if (region2->next) {
        region2->next->prev = region2->prev;
    }
    
    space->region_count--;
    kfree(region2);
    
    return VMM_SUCCESS;
}

/**
 * Change protection flags for a memory region
 */
int vmm_protect_region(vm_space_t* space, uint64_t addr, uint64_t size, uint32_t new_flags) {
    if (!space || size == 0) {
        return VMM_ERROR_INVALID_ADDR;
    }
    
    uint64_t start_addr = vmm_align_down(addr, PAGE_SIZE);
    uint64_t end_addr = vmm_align_up(addr + size, PAGE_SIZE);
    
    // Process all regions in the range
    for (uint64_t current_addr = start_addr; current_addr < end_addr; ) {
        vm_region_t* region = vmm_find_region(space, current_addr);
        if (!region) {
            return VMM_ERROR_NOT_FOUND;
        }
        
        uint64_t region_start = (current_addr > region->start_addr) ? current_addr : region->start_addr;
        uint64_t region_end = (end_addr < region->end_addr) ? end_addr : region->end_addr;
        
        // Split region if necessary
        if (region_start > region->start_addr) {
            int result = vmm_split_region(space, region_start);
            if (result != VMM_SUCCESS) {
                return result;
            }
            region = vmm_find_region(space, region_start);
        }
        
        if (region_end < region->end_addr) {
            int result = vmm_split_region(space, region_end);
            if (result != VMM_SUCCESS) {
                return result;
            }
        }
        
        // Update region flags
        region->flags = new_flags;
        
        // Update page table entries for mapped pages
        for (uint64_t page_addr = region_start; page_addr < region_end; page_addr += PAGE_SIZE) {
            pte_t* pte = vmm_get_page_table(space, page_addr, PT_LEVEL, false);
            if (pte && (*pte & PAGE_PRESENT)) {
                uint32_t page_flags = PAGE_PRESENT;
                if (new_flags & VMM_FLAG_WRITE) page_flags |= PAGE_WRITABLE;
                if (new_flags & VMM_FLAG_USER) page_flags |= PAGE_USER;
                if (!(new_flags & VMM_FLAG_EXEC)) page_flags |= PAGE_NX;
                
                *pte = (*pte & 0xFFFFFFFFFFFFF000ULL) | page_flags;
                vmm_flush_tlb_page(page_addr);
            }
        }
        
        current_addr = region_end;
    }
    
    return VMM_SUCCESS;
}

/**
 * Expand heap region
 */
void* vmm_expand_heap(vm_space_t* space, int64_t increment) {
    if (!space) {
        return (void*)-1;
    }
    
    uint64_t old_heap_end = space->heap_end;
    uint64_t new_heap_end = old_heap_end + increment;
    
    // Check for overflow or negative size
    if (increment > 0 && new_heap_end < old_heap_end) {
        return (void*)-1; // Overflow
    }
    
    if (increment < 0 && new_heap_end > old_heap_end) {
        return (void*)-1; // Underflow
    }
    
    // Find heap region
    vm_region_t* heap_region = NULL;
    vm_region_t* region = space->regions;
    while (region) {
        if (region->type == VMM_REGION_HEAP && 
            old_heap_end >= region->start_addr && old_heap_end <= region->end_addr) {
            heap_region = region;
            break;
        }
        region = region->next;
    }
    
    if (increment > 0) {
        // Expanding heap
        uint64_t pages_needed = vmm_align_up(new_heap_end, PAGE_SIZE) - 
                               vmm_align_up(old_heap_end, PAGE_SIZE);
        
        if (pages_needed > 0) {
            // Check if we can expand existing region
            if (heap_region && new_heap_end <= heap_region->end_addr) {
                // Fits in existing region - allocate pages
                for (uint64_t addr = vmm_align_up(old_heap_end, PAGE_SIZE); 
                     addr < vmm_align_up(new_heap_end, PAGE_SIZE); 
                     addr += PAGE_SIZE) {
                    
                    uint64_t phys = vmm_alloc_page();
                    if (!phys) {
                        // Out of memory - restore old heap end
                        space->heap_end = old_heap_end;
                        return (void*)-1;
                    }
                    
                    uint32_t flags = PAGE_PRESENT | PAGE_WRITABLE;
                    if (heap_region->flags & VMM_FLAG_USER) {
                        flags |= PAGE_USER;
                    }
                    
                    if (vmm_map_page(space, addr, phys, flags) != VMM_SUCCESS) {
                        vmm_free_page(phys);
                        space->heap_end = old_heap_end;
                        return (void*)-1;
                    }
                }
            } else {
                // Need to extend or create heap region
                if (!heap_region) {
                    // Create initial heap region
                    heap_region = vmm_create_region(space, space->heap_start, 
                                                   new_heap_end - space->heap_start,
                                                   VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_USER,
                                                   VMM_REGION_HEAP, "heap");
                    if (!heap_region) {
                        return (void*)-1;
                    }
                } else {
                    // Extend existing region
                    heap_region->end_addr = vmm_align_up(new_heap_end, PAGE_SIZE);
                }
            }
        }
    } else if (increment < 0) {
        // Shrinking heap
        uint64_t pages_to_free = vmm_align_up(old_heap_end, PAGE_SIZE) - 
                                vmm_align_up(new_heap_end, PAGE_SIZE);
        
        if (pages_to_free > 0) {
            // Free pages
            for (uint64_t addr = vmm_align_up(new_heap_end, PAGE_SIZE); 
                 addr < vmm_align_up(old_heap_end, PAGE_SIZE); 
                 addr += PAGE_SIZE) {
                vmm_unmap_page(space, addr);
            }
            
            // Shrink region if needed
            if (heap_region && vmm_align_up(new_heap_end, PAGE_SIZE) < heap_region->end_addr) {
                heap_region->end_addr = vmm_align_up(new_heap_end, PAGE_SIZE);
            }
        }
    }
    
    space->heap_end = new_heap_end;
    return (void*)old_heap_end;
}

/**
 * Create memory mapping (mmap-like functionality)
 */
void* vmm_mmap(vm_space_t* space, void* addr, uint64_t size, uint32_t prot, uint32_t flags) {
    if (!space || size == 0) {
        return (void*)-1;
    }
    
    size = vmm_align_up(size, PAGE_SIZE);
    uint64_t start_addr;
    
    if (addr) {
        // Fixed mapping requested
        start_addr = vmm_align_down((uint64_t)addr, PAGE_SIZE);
        
        // Check if address range is available
        for (uint64_t check_addr = start_addr; check_addr < start_addr + size; check_addr += PAGE_SIZE) {
            if (vmm_find_region(space, check_addr)) {
                if (!(flags & VMM_MMAP_FIXED)) {
                    // Find alternative address
                    start_addr = 0;
                    break;
                } else {
                    return (void*)-1; // Fixed mapping failed
                }
            }
        }
    }
    
    if (!addr || start_addr == 0) {
        // Find suitable address
        start_addr = space->mmap_start;
        
        // Simple allocation - find first available space
        while (start_addr + size < USER_STACK_TOP - 0x10000) { // Leave space for stack
            bool available = true;
            for (uint64_t check_addr = start_addr; check_addr < start_addr + size; check_addr += PAGE_SIZE) {
                if (vmm_find_region(space, check_addr)) {
                    available = false;
                    break;
                }
            }
            
            if (available) {
                break;
            }
            
            start_addr += PAGE_SIZE;
        }
        
        if (start_addr + size >= USER_STACK_TOP - 0x10000) {
            return (void*)-1; // No space available
        }
    }
    
    // Convert protection flags
    uint32_t region_flags = 0;
    if (prot & VMM_PROT_READ) region_flags |= VMM_FLAG_READ;
    if (prot & VMM_PROT_WRITE) region_flags |= VMM_FLAG_WRITE;
    if (prot & VMM_PROT_EXEC) region_flags |= VMM_FLAG_EXEC;
    region_flags |= VMM_FLAG_USER;
    
    // Create region
    vm_region_t* region = vmm_create_region(space, start_addr, size, region_flags, 
                                           VMM_REGION_MMAP, "mmap");
    if (!region) {
        return (void*)-1;
    }
    
    // Allocate and map pages if not lazy
    if (!(flags & VMM_MMAP_LAZY)) {
        for (uint64_t page_addr = start_addr; page_addr < start_addr + size; page_addr += PAGE_SIZE) {
            uint64_t phys = vmm_alloc_page();
            if (!phys) {
                vmm_destroy_region(space, start_addr);
                return (void*)-1;
            }
            
            uint32_t page_flags = PAGE_PRESENT;
            if (region_flags & VMM_FLAG_WRITE) page_flags |= PAGE_WRITABLE;
            if (region_flags & VMM_FLAG_USER) page_flags |= PAGE_USER;
            
            if (vmm_map_page(space, page_addr, phys, page_flags) != VMM_SUCCESS) {
                vmm_free_page(phys);
                vmm_destroy_region(space, start_addr);
                return (void*)-1;
            }
        }
    }
    
    // Update mmap start for next allocation
    space->mmap_start = start_addr + size;
    
    return (void*)start_addr;
}

/**
 * Unmap memory mapping
 */
int vmm_munmap(vm_space_t* space, void* addr, uint64_t size) {
    if (!space || !addr || size == 0) {
        return VMM_ERROR_INVALID_ADDR;
    }
    
    uint64_t start_addr = vmm_align_down((uint64_t)addr, PAGE_SIZE);
    uint64_t end_addr = vmm_align_up((uint64_t)addr + size, PAGE_SIZE);
    
    // Unmap pages and destroy regions in range
    for (uint64_t current_addr = start_addr; current_addr < end_addr; ) {
        vm_region_t* region = vmm_find_region(space, current_addr);
        if (!region) {
            current_addr += PAGE_SIZE;
            continue;
        }
        
        uint64_t region_start = (current_addr > region->start_addr) ? current_addr : region->start_addr;
        uint64_t region_end = (end_addr < region->end_addr) ? end_addr : region->end_addr;
        
        // Unmap pages in this region
        for (uint64_t page_addr = region_start; page_addr < region_end; page_addr += PAGE_SIZE) {
            vmm_unmap_page(space, page_addr);
        }
        
        // Handle partial unmapping
        if (region_start == region->start_addr && region_end == region->end_addr) {
            // Remove entire region
            vmm_destroy_region(space, current_addr);
        } else if (region_start == region->start_addr) {
            // Remove from start
            region->start_addr = region_end;
        } else if (region_end == region->end_addr) {
            // Remove from end
            region->end_addr = region_start;
        } else {
            // Split region
            vmm_split_region(space, region_end);
            region->end_addr = region_start;
        }
        
        current_addr = region_end;
    }
    
    return VMM_SUCCESS;
}
