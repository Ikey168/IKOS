/* IKOS Kernel Main Entry Point
 * Initializes core kernel subsystems including interrupt handling
 */

#include "idt.h"
#include "interrupts.h"
#include "../include/kalloc.h"
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
