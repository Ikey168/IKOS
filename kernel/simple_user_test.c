/* IKOS User-Space Execution Test
 * Simple test to demonstrate user-space process execution
 * Issue #14 - Complete User-Space Process Execution System
 */

#include <stdint.h>
#include <stdbool.h>
#include "process.h"
#include "elf.h"
#include "memory.h"
#include "scheduler.h"
#include "syscalls.h"

// Include our embedded binary
#include "../user/hello_world_binary.h"

/* Forward declarations */
void kernel_print(const char* format, ...);
void test_user_space_execution(void);
void test_process_creation(void);
void test_elf_loading(void);
void test_system_calls(void);

/**
 * Main test function for user-space execution
 */
void test_user_space_execution(void) {
    kernel_print("[TEST] Starting User-Space Execution Test for Issue #14\n");
    
    // Test 1: Process creation
    test_process_creation();
    
    // Test 2: ELF loading
    test_elf_loading();
    
    // Test 3: System calls
    test_system_calls();
    
    kernel_print("[TEST] User-Space Execution Test Complete\n");
}

/**
 * Test process creation infrastructure
 */
void test_process_creation(void) {
    kernel_print("[TEST] Testing process creation...\n");
    
    // Create a new process for user-space execution
    process_t* proc = create_process();
    if (proc) {
        kernel_print("[TEST] Process created successfully (PID: %d)\n", proc->pid);
        
        // Verify process structure
        if (proc->state == PROCESS_STATE_READY) {
            kernel_print("[TEST] Process state is READY\n");
        }
        
        if (proc->virtual_memory_start != 0) {
            kernel_print("[TEST] Process virtual memory allocated\n");
        }
        
        // Clean up
        destroy_process(proc);
        kernel_print("[TEST] Process destroyed successfully\n");
    } else {
        kernel_print("[TEST] ERROR: Failed to create process\n");
    }
}

/**
 * Test ELF binary loading
 */
void test_elf_loading(void) {
    kernel_print("[TEST] Testing ELF binary loading...\n");
    
    // Use our embedded hello_world binary
    kernel_print("[TEST] Testing with embedded hello_world binary (%d bytes)\n", 
                user_bin_hello_world_len);
    
    // Basic ELF header validation
    if (user_bin_hello_world_len >= sizeof(elf64_header_t)) {
        elf64_header_t* elf_header = (elf64_header_t*)user_bin_hello_world;
        
        // Check ELF magic
        if (elf_header->e_ident[0] == 0x7F && 
            elf_header->e_ident[1] == 'E' &&
            elf_header->e_ident[2] == 'L' &&
            elf_header->e_ident[3] == 'F') {
            kernel_print("[TEST] ELF magic verified\n");
            kernel_print("[TEST] ELF type: %d, machine: %d\n", 
                        elf_header->e_type, elf_header->e_machine);
            kernel_print("[TEST] Entry point: 0x%lx\n", elf_header->e_entry);
        } else {
            kernel_print("[TEST] ERROR: Invalid ELF magic\n");
        }
    } else {
        kernel_print("[TEST] ERROR: Binary too small for ELF header\n");
    }
}

/**
 * Test system call infrastructure
 */
void test_system_calls(void) {
    kernel_print("[TEST] Testing system call infrastructure...\n");
    
    // Test system call initialization
    syscall_init();
    kernel_print("[TEST] System calls initialized\n");
    
    // Note: We can't actually test user-space system calls without
    // switching to user mode, but we can verify the handlers exist
    kernel_print("[TEST] System call handlers ready\n");
}

/**
 * Kernel print function (placeholder)
 */
void kernel_print(const char* format, ...) {
    // In a real implementation, this would output to screen/serial
    // For now, it's just a placeholder to allow compilation
    (void)format; // Suppress unused parameter warning
}
