/* IKOS ELF Loader Implementation
 * Handles loading and validation of ELF executables
 */

#include "elf.h"
#include "process.h"
#include "vmm.h"
#include <stdint.h>

/* Function declarations */
static void debug_print(const char* format, ...);

/**
 * Validate an ELF file
 */
int elf_validate(const void* elf_data) {
    if (!elf_data) {
        return -1;
    }
    
    const elf64_header_t* header = (const elf64_header_t*)elf_data;
    
    /* Check ELF magic number */
    if (!ELF_IS_VALID(header)) {
        debug_print("Invalid ELF magic number\n");
        return -1;
    }
    
    /* Check if it's 64-bit */
    if (!ELF_IS_64BIT(header)) {
        debug_print("Only 64-bit ELF files supported\n");
        return -1;
    }
    
    /* Check endianness */
    if (!ELF_IS_LITTLE_ENDIAN(header)) {
        debug_print("Only little-endian ELF files supported\n");
        return -1;
    }
    
    /* Check if it's executable */
    if (!ELF_IS_EXECUTABLE(header)) {
        debug_print("ELF file is not executable\n");
        return -1;
    }
    
    /* Check architecture */
    if (header->e_machine != ELF_MACHINE_X86_64) {
        debug_print("ELF file is not for x86-64 architecture\n");
        return -1;
    }
    
    /* Check version */
    if (header->e_version != ELF_VERSION_CURRENT) {
        debug_print("Unsupported ELF version\n");
        return -1;
    }
    
    /* Validate program header */
    if (header->e_phnum == 0 || header->e_phentsize != sizeof(elf64_program_header_t)) {
        debug_print("Invalid program header table\n");
        return -1;
    }
    
    /* Basic sanity checks */
    if (header->e_entry == 0) {
        debug_print("ELF has no entry point\n");
        return -1;
    }
    
    if (header->e_entry < USER_SPACE_START || header->e_entry >= USER_SPACE_END) {
        debug_print("ELF entry point outside user space\n");
        return -1;
    }
    
    debug_print("ELF validation successful\n");
    return 0;
}

/**
 * Load an ELF process and return the entry point
 */
int elf_load_process(const void* elf_data, size_t size, uint64_t* entry_point) {
    if (!elf_data || !entry_point || size < sizeof(elf64_header_t)) {
        return -1;
    }
    
    /* Validate the ELF file */
    if (elf_validate(elf_data) != 0) {
        return -1;
    }
    
    const elf64_header_t* header = (const elf64_header_t*)elf_data;
    *entry_point = header->e_entry;
    
    debug_print("ELF process loaded, entry point: 0x%lX\n", *entry_point);
    return 0;
}

/**
 * Parse ELF program headers
 */
int elf64_parse_headers(const elf64_header_t* header, elf64_program_header_t** phdrs) {
    if (!header || !phdrs) {
        return -1;
    }
    
    /* Validate header */
    if (elf_validate(header) != 0) {
        return -1;
    }
    
    /* Get program headers */
    *phdrs = (elf64_program_header_t*)((const char*)header + header->e_phoff);
    
    return header->e_phnum;
}

/**
 * Load a single ELF segment
 */
int elf64_load_segment(const void* elf_data, const elf64_program_header_t* phdr, uint64_t base_addr) {
    if (!elf_data || !phdr) {
        return -1;
    }
    
    /* Only handle loadable segments */
    if (phdr->p_type != PT_LOAD) {
        return 0; /* Not an error, just skip */
    }
    
    /* Calculate target address */
    uint64_t target_addr = base_addr + phdr->p_vaddr;
    
    /* Validate target address is in user space */
    if (target_addr < USER_SPACE_START || 
        target_addr + phdr->p_memsz > USER_SPACE_END) {
        debug_print("Segment target address outside user space\n");
        return -1;
    }
    
    /* Calculate page alignment */
    uint64_t page_start = target_addr & ~(PAGE_SIZE - 1);
    uint64_t page_end = (target_addr + phdr->p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t num_pages = (page_end - page_start) / PAGE_SIZE;
    
    debug_print("Loading segment: vaddr=0x%lX, size=%lu, pages=%zu\n",
               target_addr, phdr->p_memsz, num_pages);
    
        /* Allocate pages for the segment */
        for (size_t i = 0; i < num_pages; i++) {
            uint64_t page_vaddr = page_start + (i * PAGE_SIZE);
            
            /* Allocate physical page */
            uint64_t page_paddr = vmm_alloc_page();
            if (page_paddr == 0) {
                debug_print("Failed to allocate physical page\n");
                return -1;
            }
            
            /* Determine page permissions */
            uint32_t flags = VMM_FLAG_USER;
            if (phdr->p_flags & PF_W) {
                flags |= VMM_FLAG_WRITE;
            }
            if (phdr->p_flags & PF_X) {
                flags |= VMM_FLAG_EXEC;
            }
            
            /* Map the page */
            vm_space_t* current_space = vmm_get_current_space();
            if (vmm_map_page(current_space, page_vaddr, page_paddr, flags) != 0) {
                debug_print("Failed to map page for segment\n");
                return -1;
            }
        }    /* Copy segment data if it exists in file */
    if (phdr->p_filesz > 0) {
        const char* src = (const char*)elf_data + phdr->p_offset;
        char* dst = (char*)target_addr;
        
        /* Copy file content */
        for (size_t i = 0; i < phdr->p_filesz; i++) {
            dst[i] = src[i];
        }
        
        debug_print("Copied %lu bytes to 0x%lX\n", phdr->p_filesz, target_addr);
    }
    
    /* Zero out BSS section (p_memsz > p_filesz) */
    if (phdr->p_memsz > phdr->p_filesz) {
        char* bss_start = (char*)(target_addr + phdr->p_filesz);
        size_t bss_size = phdr->p_memsz - phdr->p_filesz;
        
        for (size_t i = 0; i < bss_size; i++) {
            bss_start[i] = 0;
        }
        
        debug_print("Zeroed %zu bytes BSS at 0x%lX\n", bss_size, 
                   (uint64_t)bss_start);
    }
    
    return 0;
}

/**
 * Create a simple ELF file for testing
 * This creates a minimal "Hello World" user program
 */
int elf_create_test_program(void** elf_data, size_t* elf_size) {
    /* Simple x86-64 program that writes "Hello" and exits */
    static const unsigned char test_elf[] = {
        /* ELF Header */
        0x7f, 0x45, 0x4c, 0x46, /* Magic number */
        0x02,                   /* 64-bit */
        0x01,                   /* Little endian */
        0x01,                   /* Version */
        0x00,                   /* System V ABI */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Padding */
        0x02, 0x00,             /* Executable file */
        0x3e, 0x00,             /* x86-64 */
        0x01, 0x00, 0x00, 0x00, /* Version */
        0x78, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, /* Entry point */
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Program header offset */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Section header offset */
        0x00, 0x00, 0x00, 0x00, /* Flags */
        0x40, 0x00,             /* ELF header size */
        0x38, 0x00,             /* Program header size */
        0x01, 0x00,             /* Program header count */
        0x00, 0x00,             /* Section header size */
        0x00, 0x00,             /* Section header count */
        0x00, 0x00,             /* String table index */
        
        /* Program Header */
        0x01, 0x00, 0x00, 0x00, /* PT_LOAD */
        0x05, 0x00, 0x00, 0x00, /* Read + Execute */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* File offset */
        0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, /* Virtual address */
        0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, /* Physical address */
        0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* File size */
        0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Memory size */
        0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Alignment */
        
        /* Program code */
        0x48, 0xc7, 0xc0, 0x01, 0x00, 0x00, 0x00, /* mov rax, 1 (sys_write) */
        0x48, 0xc7, 0xc7, 0x01, 0x00, 0x00, 0x00, /* mov rdi, 1 (stdout) */
        0x48, 0xc7, 0xc6, 0x00, 0x01, 0x40, 0x00, /* mov rsi, message */
        0x48, 0xc7, 0xc2, 0x05, 0x00, 0x00, 0x00, /* mov rdx, 5 (length) */
        0x0f, 0x05,                               /* syscall */
        0x48, 0xc7, 0xc0, 0x3c, 0x00, 0x00, 0x00, /* mov rax, 60 (sys_exit) */
        0x48, 0xc7, 0xc7, 0x00, 0x00, 0x00, 0x00, /* mov rdi, 0 (status) */
        0x0f, 0x05,                               /* syscall */
        
        /* String data */
        'H', 'e', 'l', 'l', 'o'
    };
    
    *elf_data = (void*)test_elf;
    *elf_size = sizeof(test_elf);
    
    debug_print("Created test ELF program (%zu bytes)\n", sizeof(test_elf));
    return 0;
}

/**
 * Simple debug print - to be replaced with proper implementation
 */
static void debug_print(const char* format, ...) {
    /* TODO: Implement proper debug printing */
    (void)format; /* Suppress unused parameter warning */
}
