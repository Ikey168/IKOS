/* IKOS Enhanced Kernel Main with Integrated Debugging - Issue #16 Enhancement
 * Demonstrates integration of runtime debugger with existing kernel systems
 */

#include "../include/kernel_debug.h"   /* New debugging system */
#include <stdint.h>
#include <stddef.h>

/* Forward declarations */
void kernel_init_with_debug(void);
void setup_debug_breakpoints(void);
void kernel_loop_with_debug(void);

/* Try to include existing systems if available */
#ifdef HAVE_KERNEL_LOG
#include "../include/kernel_log.h"     /* Existing logging system */
#else
/* Fallback logging macros */
#define KLOG_INFO(cat, ...) do { } while(0)
#define KLOG_DEBUG(cat, ...) do { } while(0)
#define KLOG_ERROR(cat, ...) do { } while(0)
#define KLOG_TRACE(cat, ...) do { } while(0)
#define LOG_CAT_KERNEL 0
#define LOG_CAT_BOOT 1
#define LOG_CAT_MEMORY 2
#define LOG_CAT_IRQ 3
#define LOG_CAT_DEVICE 4
#define LOG_CAT_SCHEDULE 5
#define LOG_CAT_PROC 6
#endif

/* Placeholder includes for demonstration */
#ifndef HAVE_FULL_KERNEL
typedef void* malloc_t;
void* malloc(size_t size) { (void)size; return (void*)0x12345678; }
void free(void* ptr) { (void)ptr; }
void memory_init(void) { }
void idt_init(void) { }
void enable_interrupts(void) { }
void pic_clear_mask(int irq) { (void)irq; }
void device_manager_init(void) { }
void pci_init(void) { }
void ide_driver_init(void) { }
void device_print_info(void) { }
void pci_print_device_info(void) { }
void kernel_process_tasks(void) { }
void kernel_panic(const char* msg) { (void)msg; while(1); }
#define IRQ_TIMER 0
#define IRQ_KEYBOARD 1
#else
#include "idt.h"
#include "interrupts.h"
#include "../include/kalloc.h"
#include "../include/syscalls.h"
#include "../include/device_manager.h"
#include "../include/pci.h"
#include "../include/ide_driver.h"
#include "../include/device_driver_test.h"
#include "../include/framebuffer.h"
#include "../include/framebuffer_test.h"
#endif

#include <stdint.h>

/* Kernel entry point with enhanced debugging */
void kernel_main(void) {
    /* Initialize enhanced debugging first */
    if (!kdebug_init()) {
        /* Fall back to basic operation if debug init fails */
    }
    
    /* Enable debugging for development builds */
    #ifdef DEBUG
    kdebug_set_enabled(true);
    KLOG_INFO(LOG_CAT_KERNEL, "Runtime kernel debugger enabled for DEBUG build");
    #endif
    
    /* Initialize kernel subsystems with debugging integration */
    kernel_init_with_debug();
    
    /* Set up initial debugging breakpoints for critical functions */
    setup_debug_breakpoints();
    
    /* Enable interrupts and start normal operation */
    enable_interrupts();
    
    /* Main kernel loop with debug support */
    kernel_loop_with_debug();
}

/**
 * Initialize all kernel subsystems with debugging integration
 */
void kernel_init_with_debug(void) {
    KLOG_INFO(LOG_CAT_BOOT, "Starting kernel initialization with debugging support");
    
    /* Set breakpoint on memory initialization for debugging */
    if (kdebug_is_enabled()) {
        kdebug_set_breakpoint((uint64_t)memory_init, "Memory subsystem initialization");
    }
    
    /* Initialize memory management */
    KLOG_DEBUG(LOG_CAT_MEMORY, "Initializing memory management...");
    memory_init();
    KLOG_INFO(LOG_CAT_MEMORY, "Memory management initialized successfully");
    
    /* Initialize interrupt handling */
    KLOG_DEBUG(LOG_CAT_IRQ, "Initializing interrupt handling...");
    idt_init();
    KLOG_INFO(LOG_CAT_IRQ, "Interrupt handling initialized successfully");
    
    /* Enable timer interrupts for scheduling */
    pic_clear_mask(IRQ_TIMER);
    KLOG_DEBUG(LOG_CAT_SCHEDULE, "Timer interrupts enabled");
    
    /* Enable keyboard interrupts for input */
    pic_clear_mask(IRQ_KEYBOARD);
    KLOG_DEBUG(LOG_CAT_DEVICE, "Keyboard interrupts enabled");
    
    /* Initialize Device Driver Framework with debug support */
    KLOG_INFO(LOG_CAT_DEVICE, "Initializing Device Driver Framework...");
    
    if (kdebug_is_enabled()) {
        kdebug_set_breakpoint((uint64_t)device_manager_init, "Device manager initialization");
    }
    
    device_manager_init();
    pci_init();
    ide_driver_init();
    
    KLOG_INFO(LOG_CAT_DEVICE, "Device Driver Framework initialized successfully");
    
    /* Display system state for debugging */
    if (kdebug_is_enabled()) {
        kdebug_display_kernel_state();
    }
    
    KLOG_INFO(LOG_CAT_BOOT, "Kernel initialization completed");
}

/**
 * Set up initial debugging breakpoints for critical system functions
 */
void setup_debug_breakpoints(void) {
    if (!kdebug_is_enabled()) {
        return;
    }
    
    KLOG_DEBUG(LOG_CAT_KERNEL, "Setting up debug breakpoints...");
    
    /* Set breakpoints on critical error conditions */
    kdebug_set_breakpoint((uint64_t)kernel_panic, "Kernel panic handler");
    
    /* Set watchpoints for critical memory regions */
    /* TODO: Add specific memory regions when memory manager is available */
    
    /* Set breakpoints on important syscalls */
    /* TODO: Add syscall breakpoints when syscall table is available */
    
    KLOG_DEBUG(LOG_CAT_KERNEL, "Debug breakpoints configured");
}

/**
 * Enhanced kernel main loop with debugging support
 */
void kernel_loop_with_debug(void) {
    KLOG_INFO(LOG_CAT_KERNEL, "Starting main kernel loop with debugging support");
    
    uint64_t loop_count = 0;
    
    while (1) {
        loop_count++;
        
        /* Periodic debug statistics display */
        if (kdebug_is_enabled() && (loop_count % 1000000) == 0) {
            KLOG_TRACE(LOG_CAT_KERNEL, "Kernel loop iteration: %lu", loop_count);
            
            /* Display debug statistics every million iterations */
            if ((loop_count % 10000000) == 0) {
                kdebug_display_statistics();
            }
        }
        
        /* Check for debug console activation */
        /* TODO: Add keyboard handler for debug console activation */
        
        /* Normal kernel processing */
        kernel_process_tasks();
        
        /* Yield CPU time */
        __asm__ volatile ("hlt");
    }
}

/**
 * Enhanced kernel panic handler with debugging integration
 */
void kernel_panic_with_debug(const char* message) {
    /* Capture register state at panic */
    kdebug_registers_t panic_registers;
    kdebug_capture_registers(&panic_registers);
    
    /* Use the debug panic handler for comprehensive debugging */
    kdebug_panic_handler(message, &panic_registers);
    
    /* This should not return, but if it does, halt */
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/**
 * Enhanced page fault handler with debugging
 */
void page_fault_handler_with_debug(uint64_t fault_address, uint64_t error_code) {
    /* Capture register state at fault */
    kdebug_registers_t fault_registers;
    kdebug_capture_registers(&fault_registers);
    
    /* Log the fault using existing logging system */
    KLOG_ERROR(LOG_CAT_MEMORY, "Page fault at 0x%016lx, error code: 0x%lx", 
               fault_address, error_code);
    
    /* Use debug handler for detailed analysis */
    if (kdebug_is_enabled()) {
        kdebug_page_fault_handler(fault_address, error_code, &fault_registers);
    }
    
    /* TODO: Implement proper page fault recovery */
}

/**
 * Enhanced general protection fault handler with debugging
 */
void gpf_handler_with_debug(uint64_t error_code) {
    /* Capture register state at fault */
    kdebug_registers_t fault_registers;
    kdebug_capture_registers(&fault_registers);
    
    /* Log the fault using existing logging system */
    KLOG_ERROR(LOG_CAT_KERNEL, "General protection fault, error code: 0x%lx", error_code);
    
    /* Use debug handler for detailed analysis */
    if (kdebug_is_enabled()) {
        kdebug_gpf_handler(error_code, &fault_registers);
    }
    
    /* For now, panic on GPF */
    kernel_panic_with_debug("General protection fault");
}

/**
 * Debug-aware memory allocation wrapper
 */
void* debug_malloc(size_t size, const char* caller) {
    void* ptr = malloc(size);  /* Use existing malloc */
    
    if (kdebug_is_enabled()) {
        KLOG_TRACE(LOG_CAT_MEMORY, "malloc(%lu) = %p called from %s", size, ptr, caller);
        
        /* Set up watchpoint for large allocations in debug mode */
        if (size > 4096) {
            kdebug_set_watchpoint((uint64_t)ptr, size, KDEBUG_BP_MEMORY_ACCESS, 
                                 "Large memory allocation");
        }
    }
    
    return ptr;
}

/**
 * Debug-aware memory deallocation wrapper
 */
void debug_free(void* ptr, const char* caller) {
    if (kdebug_is_enabled()) {
        KLOG_TRACE(LOG_CAT_MEMORY, "free(%p) called from %s", ptr, caller);
    }
    
    free(ptr);  /* Use existing free */
}

/**
 * Convenience macros for debug-aware memory management
 */
#define DEBUG_MALLOC(size) debug_malloc(size, __FUNCTION__)
#define DEBUG_FREE(ptr) debug_free(ptr, __FUNCTION__)

/**
 * Debug command handlers for kernel-specific commands
 */
bool debug_cmd_meminfo(const char* args) {
    KLOG_INFO(LOG_CAT_MEMORY, "=== Memory Information ===");
    /* TODO: Display memory manager statistics */
    KLOG_INFO(LOG_CAT_MEMORY, "Memory manager integration pending");
    return true;
}

bool debug_cmd_procinfo(const char* args) {
    KLOG_INFO(LOG_CAT_PROC, "=== Process Information ===");
    /* TODO: Display process manager information */
    KLOG_INFO(LOG_CAT_PROC, "Process manager integration pending");
    return true;
}

bool debug_cmd_devinfo(const char* args) {
    KLOG_INFO(LOG_CAT_DEVICE, "=== Device Information ===");
    /* Use existing device manager functions */
    device_print_info();
    pci_print_device_info();
    return true;
}

/**
 * Register kernel-specific debug commands
 */
void register_kernel_debug_commands(void) {
    if (!kdebug_is_enabled()) {
        return;
    }
    
    kdebug_add_command("meminfo", debug_cmd_meminfo, "Display memory information");
    kdebug_add_command("procinfo", debug_cmd_procinfo, "Display process information");
    kdebug_add_command("devinfo", debug_cmd_devinfo, "Display device information");
    
    KLOG_DEBUG(LOG_CAT_KERNEL, "Kernel-specific debug commands registered");
}

/* Test function to demonstrate debugging capabilities */
void test_debugging_features(void) {
    if (!kdebug_is_enabled()) {
        KLOG_INFO(LOG_CAT_KERNEL, "Debugging disabled, skipping debug tests");
        return;
    }
    
    KLOG_INFO(LOG_CAT_KERNEL, "=== Testing Debug Features ===");
    
    /* Test memory dump */
    KLOG_INFO(LOG_CAT_KERNEL, "Testing memory dump...");
    kdebug_memory_dump((uint64_t)&kernel_main, 64);
    
    /* Test stack trace */
    KLOG_INFO(LOG_CAT_KERNEL, "Testing stack trace...");
    kdebug_stack_trace(NULL);
    
    /* Test register capture and display */
    KLOG_INFO(LOG_CAT_KERNEL, "Testing register capture...");
    kdebug_registers_t test_regs;
    kdebug_capture_registers(&test_regs);
    kdebug_display_registers(&test_regs);
    
    /* Test breakpoint setting */
    KLOG_INFO(LOG_CAT_KERNEL, "Testing breakpoint management...");
    int bp_id = kdebug_set_breakpoint((uint64_t)test_debugging_features, "Test function");
    kdebug_list_breakpoints();
    kdebug_remove_breakpoint(bp_id);
    
    /* Display statistics */
    kdebug_display_statistics();
    
    KLOG_INFO(LOG_CAT_KERNEL, "Debug feature testing completed");
}

/* ========================== Integration Points ========================== */

/**
 * Hook for interrupt handlers to check breakpoints
 */
void debug_check_breakpoint(uint64_t address) {
    if (!kdebug_is_enabled()) {
        return;
    }
    
    /* Check if address matches any active breakpoint */
    for (int i = 0; i < KDEBUG_MAX_BREAKPOINTS; i++) {
        if (breakpoints[i].active && breakpoints[i].address == address) {
            breakpoints[i].hit_count++;
            KLOG_INFO(LOG_CAT_KERNEL, "Breakpoint %d hit at 0x%016lx: %s", 
                     i, address, breakpoints[i].description);
            kdebug_enter_console();
            break;
        }
    }
}

/**
 * Hook for memory access to check watchpoints
 */
void debug_check_memory_access(uint64_t address, uint64_t length, bool is_write) {
    if (!kdebug_is_enabled()) {
        return;
    }
    
    /* Check if access overlaps with any watchpoint */
    for (int i = 0; i < KDEBUG_MAX_WATCHPOINTS; i++) {
        if (!watchpoints[i].active) continue;
        
        uint64_t wp_start = watchpoints[i].address;
        uint64_t wp_end = wp_start + watchpoints[i].length;
        uint64_t access_end = address + length;
        
        /* Check for overlap */
        if (address < wp_end && access_end > wp_start) {
            /* Check access type */
            bool should_break = false;
            if (watchpoints[i].type == KDEBUG_BP_MEMORY_ACCESS) {
                should_break = true;
            } else if (watchpoints[i].type == KDEBUG_BP_MEMORY_WRITE && is_write) {
                should_break = true;
            } else if (watchpoints[i].type == KDEBUG_BP_MEMORY_READ && !is_write) {
                should_break = true;
            }
            
            if (should_break) {
                watchpoints[i].hit_count++;
                KLOG_INFO(LOG_CAT_MEMORY, "Watchpoint %d triggered at 0x%016lx (%s): %s",
                         i, address, is_write ? "write" : "read", watchpoints[i].description);
                kdebug_enter_console();
                break;
            }
        }
    }
}
