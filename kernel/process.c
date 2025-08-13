/* IKOS Process Management Implementation
 * Handles user-space process creation, execution, and management
 */

#include "process.h"
#include "elf.h"
#include "vmm.h"
#include "interrupts.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Function declarations */
static void debug_print(const char* format, ...);

/* Global process management state */
static process_t processes[MAX_PROCESSES];
static process_t* ready_queue_head = NULL;
static uint32_t next_pid = 1;
static process_stats_t process_statistics = {0};

/* Process table lock (simple spinlock for now) */
static volatile int process_table_lock = 0;

/* Global current_process for assembly access */
process_t* current_process = NULL;

/* Forward declarations */
static process_t* allocate_process(void);
static void free_process(process_t* proc);
static int setup_process_memory_layout(process_t* proc);
static int load_elf_into_process(process_t* proc, const void* elf_data, size_t size);
static uint32_t allocate_pid(void);

/**
 * Initialize the process management system
 */
int process_init(void) {
    /* Clear process table */
    memset(processes, 0, sizeof(processes));
    
    /* Initialize process statistics */
    memset(&process_statistics, 0, sizeof(process_statistics));
    
    /* No processes running initially */
    ready_queue_head = NULL;
    current_process = NULL;
    next_pid = 1;
    process_table_lock = 0;
    
    debug_print("Process management system initialized\n");
    return 0;
}

/**
 * Create a new process from an ELF executable
 */
process_t* process_create_from_elf(const char* name, void* elf_data, size_t size) {
    if (!name || !elf_data || size == 0) {
        return NULL;
    }
    
    /* Validate ELF file */
    if (elf_validate(elf_data) != 0) {
        debug_print("Invalid ELF file for process %s\n", name);
        return NULL;
    }
    
    /* Allocate process structure */
    process_t* proc = allocate_process();
    if (!proc) {
        debug_print("Failed to allocate process structure\n");
        return NULL;
    }
    
    /* Initialize process structure */
    proc->pid = allocate_pid();
    proc->ppid = current_process ? current_process->pid : 0;
    strncpy(proc->name, name, MAX_PROCESS_NAME - 1);
    proc->name[MAX_PROCESS_NAME - 1] = '\0';
    
    proc->state = PROCESS_STATE_READY;
    proc->priority = PROCESS_PRIORITY_NORMAL;
    proc->time_slice = 10; /* 10ms default time slice */
    proc->total_time = 0;
    
    /* Set up memory layout */
    if (setup_process_memory_layout(proc) != 0) {
        debug_print("Failed to setup memory layout for process %s\n", name);
        free_process(proc);
        return NULL;
    }
    
    /* Load ELF into process memory */
    if (load_elf_into_process(proc, elf_data, size) != 0) {
        debug_print("Failed to load ELF for process %s\n", name);
        free_process(proc);
        return NULL;
    }
    
    /* Initialize file descriptors */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        proc->fds[i].fd = -1;
    }
    proc->next_fd = 0;
    
    /* Initialize process tree */
    proc->parent = current_process;
    proc->first_child = NULL;
    proc->next_sibling = NULL;
    
    /* Add to parent's child list if we have a parent */
    if (current_process) {
        proc->next_sibling = current_process->first_child;
        current_process->first_child = proc;
    }
    
    /* Add to ready queue */
    process_add_to_ready_queue(proc);
    
    /* Update statistics */
    process_statistics.total_processes++;
    process_statistics.active_processes++;
    
    debug_print("Created process %s (PID %d)\n", name, proc->pid);
    return proc;
}

/**
 * Create a new process by loading from a file path
 */
process_t* process_create(const char* name, const char* path) {
    /* TODO: Implement file system loading */
    /* For now, this is a placeholder that would:
     * 1. Open the file at 'path'
     * 2. Read the ELF data
     * 3. Call process_create_from_elf
     */
    debug_print("File system loading not yet implemented\n");
    return NULL;
}

/**
 * Set up the memory layout for a new process
 */
static int setup_process_memory_layout(process_t* proc) {
    /* Create new address space for the process */
    vm_space_t* address_space = vmm_create_address_space(proc->pid);
    if (!address_space) {
        debug_print("Failed to create address space for process\n");
        return -1;
    }
    
    proc->address_space = address_space;
    
    /* Set up memory regions */
    proc->virtual_memory_start = USER_SPACE_START;
    proc->virtual_memory_end = USER_SPACE_END;
    proc->heap_start = USER_HEAP_START;
    proc->heap_end = USER_HEAP_START;
    proc->stack_start = USER_STACK_TOP - USER_STACK_SIZE;
    proc->stack_end = USER_STACK_TOP;
    
    /* Map stack pages */
    size_t stack_pages = USER_STACK_SIZE / PAGE_SIZE;
    for (size_t i = 0; i < stack_pages; i++) {
        uint64_t vaddr = proc->stack_start + (i * PAGE_SIZE);
        
        /* Allocate physical page */
        uint64_t paddr = vmm_alloc_page();
        if (paddr == 0) {
            debug_print("Failed to allocate stack page\n");
            return -1;
        }
        
        /* Map with user and write permissions */
        if (vmm_map_page(address_space, vaddr, paddr, VMM_FLAG_USER | VMM_FLAG_WRITE) != 0) {
            debug_print("Failed to map stack page\n");
            return -1;
        }
    }
    
    debug_print("Process memory layout setup complete\n");
    return 0;
}

/**
 * Load ELF executable into process memory
 */
static int load_elf_into_process(process_t* proc, const void* elf_data, size_t size) {
    const elf64_header_t* header = (const elf64_header_t*)elf_data;
    
    /* Verify it's a 64-bit ELF */
    if (!ELF_IS_64BIT(header)) {
        debug_print("Only 64-bit ELF files supported\n");
        return -1;
    }
    
    if (!ELF_IS_EXECUTABLE(header)) {
        debug_print("ELF file is not executable\n");
        return -1;
    }
    
    /* Set entry point */
    proc->context.rip = header->e_entry;
    
    /* Load program segments */
    const elf64_program_header_t* phdrs = (const elf64_program_header_t*)
        ((const char*)elf_data + header->e_phoff);
    
    for (int i = 0; i < header->e_phnum; i++) {
        const elf64_program_header_t* phdr = &phdrs[i];
        
        if (phdr->p_type != PT_LOAD) {
            continue; /* Skip non-loadable segments */
        }
        
        /* Validate segment is within user space */
        if (phdr->p_vaddr < USER_SPACE_START || 
            phdr->p_vaddr + phdr->p_memsz > USER_SPACE_END) {
            debug_print("ELF segment outside user space\n");
            return -1;
        }
        
        /* Calculate number of pages needed */
        uint64_t start_page = phdr->p_vaddr & ~(PAGE_SIZE - 1);
        uint64_t end_page = (phdr->p_vaddr + phdr->p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        size_t num_pages = (end_page - start_page) / PAGE_SIZE;
        
        /* Allocate and map pages */
        for (size_t page = 0; page < num_pages; page++) {
            uint64_t vaddr = start_page + (page * PAGE_SIZE);
            
            /* Allocate physical page */
            uint64_t paddr = vmm_alloc_page();
            if (paddr == 0) {
                debug_print("Failed to allocate page for ELF segment\n");
                return -1;
            }
            
            /* Determine page flags based on segment permissions */
            uint32_t flags = VMM_FLAG_USER;
            if (phdr->p_flags & PF_W) {
                flags |= VMM_FLAG_WRITE;
            }
            if (phdr->p_flags & PF_X) {
                flags |= VMM_FLAG_EXEC;
            }
            
            /* Map the page */
            if (vmm_map_page(proc->address_space, vaddr, paddr, flags) != 0) {
                debug_print("Failed to map ELF segment page\n");
                return -1;
            }
        }
        
        /* Copy segment data */
        if (phdr->p_filesz > 0) {
            /* Switch to process address space temporarily */
            vm_space_t* old_space = vmm_get_current_space();
            vmm_switch_address_space(proc->address_space);
            
            /* Copy data from ELF file */
            const char* src = (const char*)elf_data + phdr->p_offset;
            char* dst = (char*)phdr->p_vaddr;
            
            for (size_t j = 0; j < phdr->p_filesz; j++) {
                dst[j] = src[j];
            }
            
            /* Zero out BSS section if needed */
            if (phdr->p_memsz > phdr->p_filesz) {
                for (size_t j = phdr->p_filesz; j < phdr->p_memsz; j++) {
                    dst[j] = 0;
                }
            }
            
            /* Switch back to kernel address space */
            vmm_switch_address_space(old_space);
        }
        
        debug_print("Loaded ELF segment at 0x%lX (size: %lu)\n", 
                   phdr->p_vaddr, phdr->p_memsz);
    }
    
    /* Initialize CPU context for user mode */
    memset(&proc->context, 0, sizeof(proc->context));
    proc->context.rip = header->e_entry;
    proc->context.rsp = USER_STACK_TOP - 16; /* Leave some space on stack */
    proc->context.rflags = 0x202; /* Interrupts enabled, reserved bit set */
    proc->context.cs = 0x1B; /* User code segment (GDT entry 3, DPL 3) */
    proc->context.ds = 0x23; /* User data segment (GDT entry 4, DPL 3) */
    proc->context.es = 0x23;
    proc->context.fs = 0x23;
    proc->context.gs = 0x23;
    proc->context.ss = 0x23;
    proc->context.cr3 = (uint64_t)proc->address_space;
    
    debug_print("ELF loaded successfully, entry point: 0x%lX\n", header->e_entry);
    return 0;
}

/**
 * Switch to a different process
 */
void process_switch_to(process_t* proc) {
    if (!proc || proc->state != PROCESS_STATE_READY) {
        return;
    }
    
    process_t* prev_process = current_process;
    current_process = proc;
    proc->state = PROCESS_STATE_RUNNING;
    
    /* Switch address space */
    vmm_switch_address_space(proc->address_space);
    
    /* Update statistics */
    process_statistics.context_switches++;
    
    debug_print("Switched to process %s (PID %d)\n", proc->name, proc->pid);
    
    /* TODO: Restore CPU context and jump to user mode */
    /* This would involve:
     * 1. Setting up user mode stack
     * 2. Restoring registers
     * 3. Using IRET to jump to user mode
     */
}

/**
 * Add process to ready queue
 */
void process_add_to_ready_queue(process_t* proc) {
    if (!proc) return;
    
    proc->next = ready_queue_head;
    proc->prev = NULL;
    
    if (ready_queue_head) {
        ready_queue_head->prev = proc;
    }
    
    ready_queue_head = proc;
    proc->state = PROCESS_STATE_READY;
}

/**
 * Get next ready process
 */
process_t* process_get_next_ready(void) {
    return ready_queue_head;
}

/**
 * Get current running process
 */
process_t* process_get_current(void) {
    return current_process;
}

/**
 * Allocate a new process structure
 */
static process_t* allocate_process(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == 0) {
            return &processes[i];
        }
    }
    return NULL;
}

/**
 * Free a process structure
 */
static void free_process(process_t* proc) {
    if (!proc) return;
    
    /* Clean up memory */
    if (proc->address_space) {
        vmm_destroy_address_space(proc->address_space);
    }
    
    /* Clear the structure */
    memset(proc, 0, sizeof(process_t));
    
    /* Update statistics */
    process_statistics.active_processes--;
}

/**
 * Allocate a new process ID
 */
static uint32_t allocate_pid(void) {
    return next_pid++;
}

/**
 * Get process statistics
 */
void process_get_stats(process_stats_t* stats) {
    if (stats) {
        *stats = process_statistics;
    }
}

/**
 * Simple debug print - to be replaced with proper implementation
 */
static void debug_print(const char* format, ...) {
    /* TODO: Implement proper debug printing */
    (void)format; /* Suppress unused parameter warning */
}
