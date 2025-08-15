/* IKOS Kernel Main Entry Point
 * Initializes core kernel subsystems including interrupt handling
 */

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
#include <stdint.h>

/* Kernel entry point called from bootloader */
void kernel_main(void) {
    /* Initialize kernel subsystems */
    kernel_init();
    
    /* Enable interrupts and start normal operation */
    enable_interrupts();
    
    /* Main kernel loop */
    kernel_loop();
}

/**
 * Initialize all kernel subsystems
 */
void kernel_init(void) {
    /* Initialize memory management */
    memory_init();
    
    /* Initialize interrupt handling */
    idt_init();
    
    /* Enable timer interrupts for scheduling */
    pic_clear_mask(IRQ_TIMER);
    
    /* Enable keyboard interrupts for input */
    pic_clear_mask(IRQ_KEYBOARD);
    
    /* Initialize Device Driver Framework (Issue #15) */
    kernel_print("Initializing Device Driver Framework...\n");
    device_manager_init();
    pci_init();
    ide_driver_init();
    
    /* Run Device Driver Framework tests */
    kernel_print("Running Device Driver Framework tests...\n");
    test_device_driver_framework();
    
    /* Initialize Framebuffer Driver (Issue #26) */
    kernel_print("Initializing Framebuffer Driver...\n");
    fb_init();
    
    /* Run Framebuffer Driver tests */
    kernel_print("Running Framebuffer Driver tests...\n");
    test_framebuffer_driver();
    
    /* TODO: Initialize other subsystems */
    /* scheduler_init(); */
    /* process_init(); */
    /* vfs_init(); */
    
    kernel_print("IKOS kernel initialized successfully\n");
}

/**
 * Main kernel execution loop
 */
void kernel_loop(void) {
    kernel_print("IKOS kernel started\n");
    kernel_print("Interrupt handling system active\n");
    
    uint64_t last_ticks = 0;
    char c;
    
    while (1) {
        /* Handle keyboard input */
        if (keyboard_has_data()) {
            c = keyboard_getchar();
            if (c != 0) {
                kernel_print("Key pressed: '%c' (0x%02X)\n", c, c);
                
                /* Simple command processing */
                switch (c) {
                    case 'h':
                        show_help();
                        break;
                    case 's':
                        show_statistics();
                        break;
                    case 't':
                        show_timer_info();
                        break;
                    case 'd':
                        show_device_info();
                        break;
                    case 'f':
                        show_framebuffer_info();
                        break;
                    case 'r':
                        kernel_print("Rebooting system...\n");
                        reboot_system();
                        break;
                }
            }
        }
        
        /* Show timer updates every second (assuming 100Hz timer) */
        uint64_t current_ticks = get_timer_ticks();
        if (current_ticks - last_ticks >= 100) {
            kernel_print("Timer: %lu ticks\n", current_ticks);
            last_ticks = current_ticks;
        }
        
        /* Yield CPU to prevent busy waiting */
        __asm__ volatile ("hlt");
    }
}

/**
 * Show help information
 */
void show_help(void) {
    kernel_print("\nIKOS Kernel Commands:\n");
    kernel_print("h - Show this help\n");
    kernel_print("s - Show interrupt statistics\n");
    kernel_print("t - Show timer information\n");
    kernel_print("d - Show device driver framework info\n");
    kernel_print("f - Show framebuffer driver info\n");
    kernel_print("r - Reboot system\n");
    kernel_print("\n");
}

/**
 * Show interrupt statistics
 */
void show_statistics(void) {
    kernel_print("\nInterrupt Statistics:\n");
    kernel_print("Timer interrupts: %lu\n", get_interrupt_count(IRQ_BASE + IRQ_TIMER));
    kernel_print("Keyboard interrupts: %lu\n", get_interrupt_count(IRQ_BASE + IRQ_KEYBOARD));
    kernel_print("Page faults: %lu\n", get_interrupt_count(INT_PAGE_FAULT));
    kernel_print("General protection faults: %lu\n", get_interrupt_count(INT_GENERAL_PROTECTION));
    kernel_print("System calls: %lu\n", get_interrupt_count(INT_SYSCALL));
    kernel_print("\n");
}

/**
 * Show timer information
 */
void show_timer_info(void) {
    uint64_t ticks = get_timer_ticks();
    uint64_t seconds = ticks / 100;  /* Assuming 100Hz timer */
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;
    
    kernel_print("\nTimer Information:\n");
    kernel_print("Total ticks: %lu\n", ticks);
    kernel_print("Uptime: %lu:%02lu:%02lu\n", hours, minutes % 60, seconds % 60);
    kernel_print("\n");
}

/**
 * Reboot the system
 */
void reboot_system(void) {
    /* Disable interrupts */
    disable_interrupts();
    
    /* Use keyboard controller to reset */
    uint8_t temp;
    do {
        temp = inb(0x64);
        if (temp & 0x01) {
            inb(0x60);
        }
    } while (temp & 0x02);
    
    outb(0x64, 0xFE);  /* Reset command */
    
    /* If that fails, triple fault */
    __asm__ volatile ("int $0x00");
}

/**
 * Show device driver framework information
 */
void show_device_info(void) {
    device_manager_stats_t dev_stats;
    device_manager_get_stats(&dev_stats);
    
    kernel_print("\nDevice Driver Framework Status:\n");
    kernel_print("Total Devices: %u\n", dev_stats.total_devices);
    kernel_print("Active Devices: %u\n", dev_stats.active_devices);
    kernel_print("Total Drivers: %u\n", dev_stats.total_drivers);
    kernel_print("Loaded Drivers: %u\n", dev_stats.loaded_drivers);
    
    pci_stats_t pci_stats;
    pci_get_stats(&pci_stats);
    
    kernel_print("\nPCI Bus Information:\n");
    kernel_print("Total PCI Devices: %u\n", pci_stats.total_devices);
    kernel_print("Buses Scanned: %u\n", pci_stats.buses_scanned);
    kernel_print("Storage Devices: %u\n", pci_stats.storage_devices);
    kernel_print("Network Devices: %u\n", pci_stats.network_devices);
    
    ide_stats_t ide_stats;
    ide_get_stats(&ide_stats);
    
    kernel_print("\nIDE Driver Information:\n");
    kernel_print("Controllers Found: %u\n", ide_stats.controllers_found);
    kernel_print("Drives Found: %u\n", ide_stats.drives_found);
    kernel_print("Total Reads: %u\n", ide_stats.total_reads);
    kernel_print("Total Writes: %u\n", ide_stats.total_writes);
    
    kernel_print("\n");
}

/**
 * Show framebuffer driver information
 */
void show_framebuffer_info(void) {
    fb_info_t* info = fb_get_info();
    
    kernel_print("\nFramebuffer Driver Status:\n");
    if (info && info->initialized) {
        kernel_print("Status: Initialized\n");
        kernel_print("Mode: %d\n", (int)info->mode);
        kernel_print("Resolution: %ux%u\n", info->width, info->height);
        kernel_print("Bits per pixel: %u\n", info->bpp);
        kernel_print("Pitch: %u bytes\n", info->pitch);
        kernel_print("Buffer size: %u bytes\n", info->size);
        kernel_print("Buffer address: 0x%p\n", info->buffer);
        kernel_print("Double buffered: %s\n", info->double_buffered ? "Yes" : "No");
        kernel_print("Color format: %d\n", (int)info->format);
    } else {
        kernel_print("Status: Not initialized\n");
    }
    
    fb_stats_t stats;
    fb_get_stats(&stats);
    
    kernel_print("\nFramebuffer Statistics:\n");
    kernel_print("Pixels drawn: %lu\n", stats.pixels_drawn);
    kernel_print("Lines drawn: %lu\n", stats.lines_drawn);
    kernel_print("Rectangles drawn: %lu\n", stats.rects_drawn);
    kernel_print("Characters drawn: %lu\n", stats.chars_drawn);
    kernel_print("Buffer swaps: %lu\n", stats.buffer_swaps);
    
    kernel_print("\n");
}

/**
 * Simple memory initialization placeholder
 */
void memory_init(void) {
    /* Initialize the new kalloc memory management system */
    extern uint8_t _kernel_end;  /* Linker symbol for end of kernel */
    
    /* Start heap after kernel + some safety margin */
    void* heap_start = (void*)0x400000; /* 4MB - after kernel space */
    size_t heap_size = 0x800000;        /* 8MB heap size */
    
    int result = kalloc_init(heap_start, heap_size);
    if (result == KALLOC_SUCCESS) {
        kernel_print("KALLOC: Memory allocator initialized with %d MB heap\n", 
                    heap_size / (1024 * 1024));
        
        /* Run allocator tests */
        kalloc_run_tests();
        
        kernel_print("KALLOC: All tests completed successfully\n");
    } else {
        kernel_print("KALLOC: Failed to initialize memory allocator (error %d)\n", result);
    }
    
    /* Initialize user-space execution system - Issue #14 */
    init_user_space_execution();
    
    kernel_print("Memory management initialized\n");
}

/**
 * Simple kernel print function
 */
void kernel_print(const char* format, ...) {
    /* TODO: Implement proper formatted printing to console */
    /* For now, this is a placeholder that would output to VGA or serial */
    
    /* Simple character output for demonstration */
    static char* video_memory = (char*)0xB8000;
    static int cursor_pos = 0;
    
    /* Very basic string output - just for testing */
    const char* str = format;
    while (*str && cursor_pos < 80 * 25) {
        if (*str != '%' && *str != '\\') {
            video_memory[cursor_pos * 2] = *str;
            video_memory[cursor_pos * 2 + 1] = 0x07;  /* White on black */
            cursor_pos++;
        }
        str++;
    }
}

/* Port I/O functions */
void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
