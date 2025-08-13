/* IKOS Interrupt Handlers
 * High-level C interrupt and IRQ handlers
 */

#include "idt.h"
#include "interrupts.h"
#include <stdint.h>

/* Function declarations */
void handle_exception(interrupt_frame_t* frame);
void handle_irq(interrupt_frame_t* frame);
void handle_syscall(interrupt_frame_t* frame);
void handle_unknown_interrupt(interrupt_frame_t* frame);
void handle_timer_irq(interrupt_frame_t* frame);
void handle_keyboard_irq(interrupt_frame_t* frame);
void handle_page_fault(interrupt_frame_t* frame);
void handle_general_protection_fault(interrupt_frame_t* frame);
void handle_double_fault(interrupt_frame_t* frame);
char scancode_to_ascii(uint8_t scancode);
void debug_print(const char* format, ...);

/* Timer interrupt counter */
static volatile uint64_t timer_ticks = 0;

/* Keyboard buffer */
#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t kb_read_pos = 0;
static volatile uint32_t kb_write_pos = 0;

/* Interrupt statistics */
static uint64_t interrupt_counts[256] = {0};

/* Exception names for debugging */
static const char* exception_names[] = {
    "Divide by Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Exception"
};

/**
 * Generic interrupt handler
 */
void interrupt_handler(interrupt_frame_t* frame) {
    uint8_t int_no = frame->int_no;
    
    /* Update interrupt statistics */
    interrupt_counts[int_no]++;
    
    /* Handle CPU exceptions */
    if (int_no < 32) {
        handle_exception(frame);
        return;
    }
    
    /* Handle IRQs */
    if (int_no >= 32 && int_no < 48) {
        handle_irq(frame);
        return;
    }
    
    /* Handle system calls */
    if (int_no == 128) {
        handle_syscall(frame);
        return;
    }
    
    /* Unknown interrupt */
    handle_unknown_interrupt(frame);
}

/**
 * Handle CPU exceptions
 */
void handle_exception(interrupt_frame_t* frame) {
    uint8_t int_no = frame->int_no;
    
    /* Print exception information */
    if (int_no < 20) {
        debug_print("EXCEPTION: %s (INT 0x%02X)\n", exception_names[int_no], int_no);
    } else {
        debug_print("EXCEPTION: Unknown Exception (INT 0x%02X)\n", int_no);
    }
    
    debug_print("Error Code: 0x%016lX\n", frame->error_code);
    debug_print("RIP: 0x%016lX\n", frame->rip);
    debug_print("CS: 0x%016lX\n", frame->cs);
    debug_print("RFLAGS: 0x%016lX\n", frame->rflags);
    debug_print("RSP: 0x%016lX\n", frame->rsp);
    debug_print("SS: 0x%016lX\n", frame->ss);
    
    /* Handle specific exceptions */
    switch (int_no) {
        case INT_PAGE_FAULT:
            handle_page_fault(frame);
            break;
            
        case INT_GENERAL_PROTECTION:
            handle_general_protection_fault(frame);
            break;
            
        case INT_DOUBLE_FAULT:
            handle_double_fault(frame);
            break;
            
        default:
            /* For now, halt on unhandled exceptions */
            debug_print("Unhandled exception - halting system\n");
            while (1) {
                __asm__ volatile ("hlt");
            }
    }
}

/**
 * IRQ handler dispatcher
 */
void irq_handler(interrupt_frame_t* frame) {
    handle_irq(frame);
}

/**
 * Handle IRQ interrupts
 */
void handle_irq(interrupt_frame_t* frame) {
    uint8_t irq_no = frame->int_no - 32;
    
    /* Update IRQ statistics */
    interrupt_counts[frame->int_no]++;
    
    /* Dispatch to specific IRQ handlers */
    switch (irq_no) {
        case IRQ_TIMER:
            handle_timer_irq(frame);
            break;
            
        case IRQ_KEYBOARD:
            handle_keyboard_irq(frame);
            break;
            
        default:
            debug_print("Unhandled IRQ: %d\n", irq_no);
            break;
    }
    
    /* Send End of Interrupt signal */
    pic_send_eoi(irq_no);
}

/**
 * Timer interrupt handler
 */
void handle_timer_irq(interrupt_frame_t* frame) {
    timer_ticks++;
    
    /* Simple task switching trigger every 10ms (assuming 100Hz timer) */
    if (timer_ticks % 1 == 0) {
        /* TODO: Call scheduler here */
        /* schedule_next_task(); */
    }
}

/**
 * Keyboard interrupt handler
 */
void handle_keyboard_irq(interrupt_frame_t* frame) {
    uint8_t scancode = inb(0x60);  /* Read scancode from keyboard controller */
    
    /* Simple scancode to ASCII conversion (very basic) */
    char ascii = scancode_to_ascii(scancode);
    if (ascii != 0) {
        /* Add to keyboard buffer */
        uint32_t next_write = (kb_write_pos + 1) % KEYBOARD_BUFFER_SIZE;
        if (next_write != kb_read_pos) {
            keyboard_buffer[kb_write_pos] = ascii;
            kb_write_pos = next_write;
        }
    }
}

/**
 * Page fault handler
 */
void handle_page_fault(interrupt_frame_t* frame) {
    uint64_t fault_addr;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));
    
    debug_print("PAGE FAULT at address 0x%016lX\n", fault_addr);
    debug_print("Error code: 0x%016lX\n", frame->error_code);
    
    /* Decode error code */
    debug_print("Fault was: %s %s %s %s\n",
                (frame->error_code & 0x01) ? "Protection violation" : "Page not present",
                (frame->error_code & 0x02) ? "Write" : "Read",
                (frame->error_code & 0x04) ? "User mode" : "Kernel mode",
                (frame->error_code & 0x08) ? "Reserved bit set" : "");
    
    /* TODO: Handle page fault properly - allocate page, swap, etc. */
    debug_print("Page fault handling not implemented - halting\n");
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/**
 * General protection fault handler
 */
void handle_general_protection_fault(interrupt_frame_t* frame) {
    debug_print("GENERAL PROTECTION FAULT\n");
    debug_print("Error code: 0x%016lX\n", frame->error_code);
    
    if (frame->error_code != 0) {
        debug_print("Segment selector: 0x%04lX\n", frame->error_code & 0xFFFF);
        debug_print("IDT: %s, GDT/LDT: %s, Table: %s\n",
                    (frame->error_code & 0x02) ? "Yes" : "No",
                    (frame->error_code & 0x01) ? "LDT" : "GDT",
                    (frame->error_code & 0x02) ? "IDT" : ((frame->error_code & 0x01) ? "LDT" : "GDT"));
    }
    
    /* Halt system */
    debug_print("General protection fault - halting system\n");
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/**
 * Double fault handler
 */
void handle_double_fault(interrupt_frame_t* frame) {
    debug_print("DOUBLE FAULT - SYSTEM CRITICAL ERROR\n");
    debug_print("Error code: 0x%016lX\n", frame->error_code);
    
    /* Double fault is unrecoverable */
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/**
 * System call handler
 */
void handle_syscall(interrupt_frame_t* frame) {
    /* System call number is in rax (preserved in frame) */
    uint64_t syscall_no = frame->rax;
    
    debug_print("System call: %lu\n", syscall_no);
    
    /* TODO: Implement system call dispatch table */
    switch (syscall_no) {
        case 0:  /* SYS_EXIT */
            /* TODO: Exit process */
            break;
            
        case 1:  /* SYS_WRITE */
            /* TODO: Write system call */
            break;
            
        default:
            debug_print("Unknown system call: %lu\n", syscall_no);
            frame->rax = -1;  /* Return error */
            break;
    }
}

/**
 * Handle unknown interrupts
 */
void handle_unknown_interrupt(interrupt_frame_t* frame) {
    debug_print("Unknown interrupt: 0x%02X\n", frame->int_no);
}

/**
 * Simple scancode to ASCII conversion
 */
char scancode_to_ascii(uint8_t scancode) {
    /* Simple US keyboard layout - only handle printable characters */
    static const char scancode_ascii[] = {
        0,   0,   '1', '2', '3', '4', '5', '6',     /* 0x00-0x07 */
        '7', '8', '9', '0', '-', '=', '\b', '\t',   /* 0x08-0x0F */
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',    /* 0x10-0x17 */
        'o', 'p', '[', ']', '\n', 0,  'a', 's',    /* 0x18-0x1F */
        'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',    /* 0x20-0x27 */
        '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',   /* 0x28-0x2F */
        'b', 'n', 'm', ',', '.', '/', 0,   '*',    /* 0x30-0x37 */
        0,   ' ', 0,   0,   0,   0,   0,   0,      /* 0x38-0x3F */
    };
    
    /* Only handle key press (not release) */
    if (scancode & 0x80) {
        return 0;  /* Key release */
    }
    
    if (scancode < sizeof(scancode_ascii)) {
        return scancode_ascii[scancode];
    }
    
    return 0;
}

/**
 * Get timer ticks
 */
uint64_t get_timer_ticks(void) {
    return timer_ticks;
}

/**
 * Read character from keyboard buffer
 */
char keyboard_getchar(void) {
    if (kb_read_pos == kb_write_pos) {
        return 0;  /* Buffer empty */
    }
    
    char c = keyboard_buffer[kb_read_pos];
    kb_read_pos = (kb_read_pos + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

/**
 * Check if keyboard has data
 */
int keyboard_has_data(void) {
    return kb_read_pos != kb_write_pos;
}

/**
 * Get interrupt statistics
 */
uint64_t get_interrupt_count(uint8_t int_no) {
    return interrupt_counts[int_no];
}

/**
 * Enable interrupts
 */
void enable_interrupts(void) {
    __asm__ volatile ("sti");
}

/**
 * Disable interrupts
 */
void disable_interrupts(void) {
    __asm__ volatile ("cli");
}

/* Port I/O function declarations - implemented elsewhere */
extern uint8_t inb(uint16_t port);

/**
 * Simple debug print function - replace with actual implementation
 */
void debug_print(const char* format, ...) {
    /* TODO: Implement proper formatted printing */
    /* For now, this is a placeholder */
}
