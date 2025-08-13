# IKOS Virtual Memory Manager (VMM) - Issue #11

## Overview

The IKOS Virtual Memory Manager provides paging-based virtual memory management with isolated address spaces for each process. This implementation includes page allocation, address space management, copy-on-write semantics, and comprehensive memory region handling.

## Architecture

### Core Components

1. **vmm.c** - Main VMM implementation with core functionality
2. **vmm_cow.c** - Copy-on-write implementation for efficient memory sharing
3. **vmm_regions.c** - Memory region management and operations
4. **vmm_interrupts.c** - Page fault interrupt handling
5. **vmm_asm.asm** - Low-level assembly support functions
6. **vmm.h** - VMM interface and data structures

### Key Features

#### Address Space Management
- Isolated virtual address spaces per process
- 64-bit virtual addressing with canonical addresses
- User space: 0x0000000000000000 - 0x00007FFFFFFFFFFF
- Kernel space: 0xFFFF800000000000 - 0xFFFFFFFFFFFFFFFF

#### Page Management
- 4KB page size with 4-level page tables (PML4, PDPT, PD, PT)
- Physical page frame allocation and reference counting
- Lazy allocation support for efficient memory usage
- Page fault handling for demand paging

#### Memory Regions
- Categorized memory regions (code, data, heap, stack, mmap)
- Dynamic region splitting and merging
- Protection flags (read, write, execute)
- Named regions for debugging

#### Copy-on-Write (COW)
- Efficient memory sharing between processes
- Automatic copy on first write access
- Reference counting for shared pages
- Support for fork() semantics

## Data Structures

### Address Space (`vm_space_t`)
```c
typedef struct vm_space {
    uint32_t owner_pid;              /* Process ID owning this space */
    pte_t* pml4_virt;               /* Virtual address of PML4 table */
    uint64_t pml4_phys;             /* Physical address of PML4 table */
    vm_region_t* regions;           /* Linked list of memory regions */
    uint32_t region_count;          /* Number of regions */
    uint64_t heap_start;            /* Heap start address */
    uint64_t heap_end;              /* Current heap end */
    uint64_t stack_start;           /* Stack start address */
    uint64_t mmap_start;            /* Next mmap allocation address */
} vm_space_t;
```

### Memory Region (`vm_region_t`)
```c
typedef struct vm_region {
    uint64_t start_addr;            /* Start virtual address */
    uint64_t end_addr;              /* End virtual address */
    uint32_t flags;                 /* Protection and behavior flags */
    vmm_region_type_t type;         /* Region type */
    char name[32];                  /* Region name for debugging */
    struct vm_region* next;         /* Next region in list */
    struct vm_region* prev;         /* Previous region in list */
} vm_region_t;
```

### Page Frame (`page_frame_t`)
```c
typedef struct page_frame {
    uint32_t frame_number;          /* Physical frame number */
    uint32_t ref_count;             /* Reference count for COW */
    uint32_t flags;                 /* Frame flags */
    uint32_t owner_pid;             /* Owning process ID */
    struct page_frame* next;        /* Next in free list */
} page_frame_t;
```

## API Reference

### Initialization
- `int vmm_init(uint64_t memory_size)` - Initialize VMM system
- `void vmm_shutdown(void)` - Shutdown VMM system

### Address Space Management
- `vm_space_t* vmm_create_address_space(uint32_t pid)` - Create new address space
- `void vmm_destroy_address_space(vm_space_t* space)` - Destroy address space
- `int vmm_switch_address_space(vm_space_t* space)` - Switch to address space
- `vm_space_t* vmm_get_current_space(void)` - Get current address space

### Page Management
- `uint64_t vmm_alloc_page(void)` - Allocate physical page
- `void vmm_free_page(uint64_t phys_addr)` - Free physical page
- `int vmm_map_page(vm_space_t* space, uint64_t virt, uint64_t phys, uint32_t flags)` - Map page
- `int vmm_unmap_page(vm_space_t* space, uint64_t virt_addr)` - Unmap page
- `uint64_t vmm_get_physical_addr(vm_space_t* space, uint64_t virt_addr)` - Get physical address

### Memory Allocation
- `void* vmm_alloc_virtual(vm_space_t* space, uint64_t size, uint32_t flags)` - Allocate virtual memory
- `void vmm_free_virtual(vm_space_t* space, void* addr, uint64_t size)` - Free virtual memory
- `void* vmm_expand_heap(vm_space_t* space, int64_t increment)` - Expand heap (sbrk-like)
- `void* vmm_mmap(vm_space_t* space, void* addr, uint64_t size, uint32_t prot, uint32_t flags)` - Memory mapping
- `int vmm_munmap(vm_space_t* space, void* addr, uint64_t size)` - Unmap memory

### Region Management
- `vm_region_t* vmm_create_region(vm_space_t* space, uint64_t start, uint64_t size, uint32_t flags, vmm_region_type_t type, const char* name)` - Create region
- `vm_region_t* vmm_find_region(vm_space_t* space, uint64_t addr)` - Find region by address
- `int vmm_destroy_region(vm_space_t* space, uint64_t addr)` - Destroy region
- `int vmm_protect_region(vm_space_t* space, uint64_t addr, uint64_t size, uint32_t new_flags)` - Change protection

### Copy-on-Write
- `int vmm_handle_cow_fault(vm_space_t* space, uint64_t fault_addr)` - Handle COW page fault
- `vm_space_t* vmm_copy_address_space(vm_space_t* src_space, uint32_t new_pid)` - Copy address space (fork)

### Page Fault Handling
- `int vmm_handle_page_fault(page_fault_info_t* fault_info)` - Handle page fault
- `void vmm_page_fault_handler(uint64_t fault_addr, uint64_t error_code)` - Low-level page fault handler

### TLB Management
- `void vmm_flush_tlb(void)` - Flush entire TLB
- `void vmm_flush_tlb_page(uint64_t virt_addr)` - Flush single TLB entry

### Statistics and Utilities
- `vmm_stats_t* vmm_get_stats(void)` - Get VMM statistics
- `bool vmm_is_user_addr(uint64_t addr)` - Check if address is in user space
- `bool vmm_is_kernel_addr(uint64_t addr)` - Check if address is in kernel space
- `uint64_t vmm_align_down(uint64_t addr, uint64_t alignment)` - Align address down
- `uint64_t vmm_align_up(uint64_t addr, uint64_t alignment)` - Align address up

## Flags and Constants

### Page Flags
- `PAGE_PRESENT` - Page is present in memory
- `PAGE_WRITABLE` - Page is writable
- `PAGE_USER` - Page is accessible from user mode
- `PAGE_NX` - No-execute bit

### VMM Flags
- `VMM_FLAG_READ` - Memory is readable
- `VMM_FLAG_WRITE` - Memory is writable
- `VMM_FLAG_EXEC` - Memory is executable
- `VMM_FLAG_USER` - User mode access
- `VMM_FLAG_LAZY` - Lazy allocation
- `VMM_FLAG_COW` - Copy-on-write

### Memory Protection
- `VMM_PROT_READ` - Memory is readable
- `VMM_PROT_WRITE` - Memory is writable
- `VMM_PROT_EXEC` - Memory is executable
- `VMM_PROT_NONE` - Memory is not accessible

### Error Codes
- `VMM_SUCCESS` - Operation successful
- `VMM_ERROR_NOMEM` - Out of memory
- `VMM_ERROR_INVALID_ADDR` - Invalid address
- `VMM_ERROR_NOT_FOUND` - Resource not found
- `VMM_ERROR_EXISTS` - Resource already exists
- `VMM_ERROR_PERM_DENIED` - Permission denied
- `VMM_ERROR_FAULT` - Page fault or general error

## Testing

### Test Suite (`test_vmm.c`)
Comprehensive test suite covering:
- VMM initialization
- Address space management
- Physical memory allocation
- Virtual memory allocation
- Memory regions
- Page mapping/unmapping
- Heap expansion
- Copy-on-write functionality
- Memory mapping (mmap/munmap)
- Error conditions
- Performance testing

### Usage
```bash
# Build VMM components
make vmm

# Run full test suite
make test-vmm

# Run smoke test
make vmm-smoke
```

## Implementation Notes

### Memory Layout
- Physical memory starts at 1MB (after bootloader/BIOS area)
- Kernel virtual space at -2GB (0xFFFFFFFF80000000)
- User virtual space from 0x0 to 0x00007FFFFFFFFFFF
- Page tables use recursive mapping for easy access

### Performance Considerations
- Lazy allocation reduces memory footprint
- Copy-on-write minimizes memory copying
- TLB flushing optimized for single pages when possible
- Reference counting enables efficient shared memory

### Security Features
- Process isolation through separate address spaces
- Page-level protection (read/write/execute)
- User/kernel mode separation
- Canonical address checking

### Limitations
- Fixed 4KB page size (2MB large pages not implemented)
- Simple physical memory allocator (no NUMA awareness)
- Basic memory compaction (no sophisticated algorithms)

## Integration with Other Subsystems

### Scheduler Integration
- Address space switching during context switches
- Process memory statistics tracking
- Memory pressure handling

### IPC Integration
- Shared memory regions for efficient IPC
- Memory mapping for file-backed IPC
- COW semantics for message passing optimization

### Future Enhancements
- NUMA-aware memory allocation
- Memory compression and swapping
- Large page support (2MB/1GB pages)
- Memory hotplug support
- Advanced memory reclamation algorithms

## Compilation

The VMM is compiled as part of the kernel build system:

```makefile
VMM_SOURCES = kernel/vmm.c kernel/vmm_cow.c kernel/vmm_regions.c \
              kernel/vmm_interrupts.c kernel/vmm_asm.asm
VMM_OBJECTS = build/vmm.o build/vmm_cow.o build/vmm_regions.o \
              build/vmm_interrupts.o build/vmm_asm.o
```

Compiler flags ensure proper kernel compilation:
- `-ffreestanding` - Freestanding environment
- `-mcmodel=large` - Large memory model for kernel
- `-mno-red-zone` - Disable red zone for interrupt safety
- `-mno-sse*` - Disable SSE instructions in kernel

This implementation provides a solid foundation for virtual memory management in the IKOS kernel, supporting modern features like COW, demand paging, and efficient memory sharing while maintaining security and performance.
