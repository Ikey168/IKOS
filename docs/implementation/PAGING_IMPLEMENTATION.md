# IKOS Paging Implementation

## Overview
This document describes the paging structures and implementation for the IKOS kernel bootloader, providing the foundation for kernel execution in long mode (x86_64).

## Paging Structure Setup

### Memory Layout
The paging structures are established at the following physical addresses:

```
0x1000 - PML4 (Page Map Level 4)       - 4KB
0x2000 - PDPT (Page Directory Pointer Table) - 4KB  
0x3000 - PD (Page Directory)            - 4KB
0x4000 - PT (Page Table) [Optional]     - 4KB
```

### Paging Hierarchy
```
Virtual Address (48-bit)
    ↓
PML4 (bits 47-39) → PDPT (bits 38-30) → PD (bits 29-21) → PT (bits 20-12) → Offset (bits 11-0)
```

## Current Implementation

### 1. Page Table Setup (`setup_paging` in `boot_longmode.asm`)

#### Memory Clearing
- Clears 20KB of memory starting at 0x1000
- Ensures all page table entries start as zero

#### PML4 Setup (0x1000)
- Entry 0: Points to PDPT at 0x2000 with PRESENT | WRITABLE flags
- Maps the first 512GB of virtual address space

#### PDPT Setup (0x2000)  
- Entry 0: Points to PD at 0x3000 with PRESENT | WRITABLE flags
- Maps the first 1GB of virtual address space

#### PD Setup (0x3000)
- Entry 0: Identity maps 0x000000-0x1FFFFF (first 2MB) using 2MB large page
- Entry 1: Identity maps 0x200000-0x3FFFFF (second 2MB) using 2MB large page
- Uses PRESENT | WRITABLE | LARGE flags for 2MB pages

### 2. Register Configuration

#### CR4 Register
- Sets PAE bit (bit 5) to enable Physical Address Extension
- Required for long mode operation

#### CR3 Register  
- Loaded with PML4 base address (0x1000)
- Establishes the root of the page table hierarchy

#### EFER MSR
- Sets LME bit (bit 8) to enable Long Mode Extensions
- Required before enabling paging for long mode

#### CR0 Register
- Sets PG bit (bit 31) to enable paging
- Activates long mode when combined with LME and PAE

### 3. Memory Mapping

#### Identity Mapping
- **0x000000-0x1FFFFF**: Boot code, GDT, IDT, page tables
- **0x200000-0x3FFFFF**: Kernel code and data sections

#### Virtual Memory Layout
- Currently uses identity mapping (virtual = physical)
- Can be extended to map kernel at higher virtual addresses (e.g., 0xFFFFFFFF80000000)

## Page Flags

### Standard Flags
- `PAGE_PRESENT (0x01)`: Page is present in memory
- `PAGE_WRITABLE (0x02)`: Page is writable
- `PAGE_USER (0x04)`: User mode access allowed
- `PAGE_LARGE (0x80)`: Large page (2MB for PD entries)

### Extended Flags (Available)
- `PAGE_WRITETHROUGH (0x08)`: Write-through caching
- `PAGE_CACHEDISABLE (0x10)`: Cache disabled
- `PAGE_ACCESSED (0x20)`: Page has been accessed
- `PAGE_DIRTY (0x40)`: Page has been written to
- `PAGE_GLOBAL (0x100)`: Global page (not flushed on CR3 reload)

## Features Implemented

✅ **Set up page tables (PML4, PDPT, PDT)**
- Complete 4-level page table hierarchy
- Proper flag settings for each level

✅ **Enable paging in CR0 and CR4 registers**
- PAE enabled in CR4
- Paging enabled in CR0
- Long mode enabled via EFER MSR

✅ **Map kernel sections into virtual memory**
- Identity mapping for first 4MB
- Kernel code and data accessible
- Boot structures preserved

## Extension Points

### Fine-Grained Mapping
To use 4KB pages instead of 2MB large pages:
1. Uncomment PT setup code in `setup_paging`
2. Replace PD entries to point to PT instead of using large pages
3. Map individual 4KB pages in PT structure

### Higher Virtual Addresses
To map kernel at higher virtual addresses:
1. Add entries to upper PML4 slots
2. Create additional PDPT/PD structures
3. Map physical kernel to virtual addresses like 0xFFFFFFFF80000000

### Multiple Address Spaces
For user/kernel separation:
1. Create separate page tables for user processes
2. Use CR3 switching for context changes
3. Implement proper privilege level separation

## Testing

The implementation has been successfully built and is ready for testing in:
- `build/ikos_longmode.img` - Bootable disk image
- Can be tested in QEMU, VirtualBox, or real hardware

## Files Modified

1. **`boot/boot_longmode.asm`**
   - Enhanced `setup_paging` function
   - Added paging constants and detailed comments
   - Improved page table structure setup

2. **`include/memory.h`**
   - Added paging structure definitions
   - Added page flag constants
   - Added virtual memory layout definitions

## Next Steps

1. **Test in emulator**: Verify paging works correctly
2. **Add error handling**: Check for paging support and handle failures
3. **Expand mapping**: Map more memory regions as needed
4. **Implement memory management**: Add heap allocation and virtual memory management
