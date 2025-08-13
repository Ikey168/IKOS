/* IKOS Interrupt Descriptor Table Implementation
 * Sets up the IDT and handles interrupt registration
 */

#include "idt.h"
#include "interrupts.h"
#include <stdint.h>

/* IDT table and pointer */
static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

/* External assembly functions */
extern void idt_flush(uint64_t);

/**
 * Initialize the Interrupt Descriptor Table
 */
void idt_init(void) {
    /* Set up IDT pointer */
    idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
    idt_ptr.base = (uint64_t)&idt;
    
    /* Clear the IDT */
    memset(&idt, 0, sizeof(idt_entry_t) * IDT_ENTRIES);
    
    /* Install exception handlers */
    idt_set_gate(INT_DIVIDE_ERROR, exception_divide_error, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_DEBUG, exception_debug, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_NMI, exception_nmi, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_BREAKPOINT, exception_breakpoint, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL3 | IDT_FLAG_GATE64);
    idt_set_gate(INT_OVERFLOW, exception_overflow, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_BOUND_RANGE, exception_bound_range, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_INVALID_OPCODE, exception_invalid_opcode, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_DEVICE_NOT_AVAIL, exception_device_not_available, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_DOUBLE_FAULT, exception_double_fault, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_INVALID_TSS, exception_invalid_tss, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_SEGMENT_NOT_PRESENT, exception_segment_not_present, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_STACK_FAULT, exception_stack_fault, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_GENERAL_PROTECTION, exception_general_protection, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_PAGE_FAULT, exception_page_fault, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_FPU_ERROR, exception_fpu_error, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_ALIGNMENT_CHECK, exception_alignment_check, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_MACHINE_CHECK, exception_machine_check, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(INT_SIMD_EXCEPTION, exception_simd_exception, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    
    /* Install IRQ handlers */
    idt_set_gate(IRQ_BASE + IRQ_TIMER, irq_timer, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_KEYBOARD, irq_keyboard, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_CASCADE, irq_cascade, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_COM2, irq_com2, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_COM1, irq_com1, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_LPT2, irq_lpt2, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_FLOPPY, irq_floppy, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_LPT1, irq_lpt1, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_CMOS_RTC, irq_cmos_rtc, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_FREE1, irq_free1, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_FREE2, irq_free2, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_FREE3, irq_free3, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_PS2_MOUSE, irq_ps2_mouse, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_FPU, irq_fpu, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_PRIMARY_ATA, irq_primary_ata, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    idt_set_gate(IRQ_BASE + IRQ_SECONDARY_ATA, irq_secondary_ata, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE64);
    
    /* Install system call handler */
    idt_set_gate(INT_SYSCALL, syscall_handler, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_DPL3 | IDT_FLAG_GATE64);
    
    /* Initialize PIC */
    pic_init();
    
    /* Load the IDT */
    idt_load();
}

/**
 * Set an IDT gate entry
 */
void idt_set_gate(uint8_t num, interrupt_handler_t handler, uint16_t selector, uint8_t flags) {
    uint64_t base = (uint64_t)handler;
    
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_mid = (base >> 16) & 0xFFFF;
    idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].selector = selector;
    idt[num].ist = 0;  /* No IST for now */
    idt[num].flags = flags;
    idt[num].reserved = 0;
}

/**
 * Load the IDT
 */
void idt_load(void) {
    idt_flush((uint64_t)&idt_ptr);
}

/**
 * Initialize the Programmable Interrupt Controller (PIC)
 */
void pic_init(void) {
    /* Start initialization sequence */
    outb(0x20, 0x11);    /* ICW1: Init + expect ICW4 */
    outb(0xA0, 0x11);    /* ICW1: Init + expect ICW4 */
    
    /* Set interrupt vector offsets */
    outb(0x21, IRQ_BASE);         /* ICW2: Master PIC vector offset */
    outb(0xA1, IRQ_BASE + 8);     /* ICW2: Slave PIC vector offset */
    
    /* Set up cascading */
    outb(0x21, 0x04);    /* ICW3: Tell master PIC that slave is at IRQ2 */
    outb(0xA1, 0x02);    /* ICW3: Tell slave PIC its cascade identity */
    
    /* Set mode */
    outb(0x21, 0x01);    /* ICW4: 8086 mode */
    outb(0xA1, 0x01);    /* ICW4: 8086 mode */
    
    /* Mask all interrupts initially */
    outb(0x21, 0xFF);    /* Mask all interrupts on master PIC */
    outb(0xA1, 0xFF);    /* Mask all interrupts on slave PIC */
}

/**
 * Send End of Interrupt signal to PIC
 */
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(0xA0, 0x20);  /* Send EOI to slave PIC */
    }
    outb(0x20, 0x20);      /* Send EOI to master PIC */
}

/**
 * Set IRQ mask (disable interrupt)
 */
void pic_set_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = 0x21;  /* Master PIC */
    } else {
        port = 0xA1;  /* Slave PIC */
        irq -= 8;
    }
    
    value = inb(port) | (1 << irq);
    outb(port, value);
}

/**
 * Clear IRQ mask (enable interrupt)
 */
void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = 0x21;  /* Master PIC */
    } else {
        port = 0xA1;  /* Slave PIC */
        irq -= 8;
    }
    
    value = inb(port) & ~(1 << irq);
    outb(port, value);
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

/* Simple memset implementation */
void* memset(void* dest, int value, uint32_t count) {
    unsigned char* d = (unsigned char*)dest;
    while (count--) {
        *d++ = (unsigned char)value;
    }
    return dest;
}
