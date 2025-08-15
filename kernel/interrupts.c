/* IKOS Timer and Interrupt Management
 * Provides timer setup and interrupt handling for preemptive scheduling
 */

#include "interrupts.h"
#include "scheduler.h"

/* PIT (Programmable Interval Timer) ports */
#define PIT_COMMAND_PORT    0x43
#define PIT_DATA_PORT0      0x40
#define PIT_DATA_PORT1      0x41
#define PIT_DATA_PORT2      0x42

/* PIC (Programmable Interrupt Controller) ports */
#define PIC1_COMMAND        0x20
#define PIC1_DATA           0x21
#define PIC2_COMMAND        0xA0
#define PIC2_DATA           0xA1

/* IDT (Interrupt Descriptor Table) */
#define IDT_ENTRIES         256
#define TIMER_IRQ           0
#define SYSCALL_INT         0x80

/* IDT Entry structure */
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

/* IDT Pointer structure */
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

/* External assembly functions */
extern void timer_interrupt_entry(void);
extern void syscall_yield_entry(void);

/* I/O port functions */
static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t data;
    __asm__ volatile("inb %1, %0" : "=a"(data) : "Nd"(port));
    return data;
}

/**
 * Set IDT entry
 */
static void set_idt_entry(int index, uint64_t handler, uint16_t selector, uint8_t type_attr) {
    idt[index].offset_low = handler & 0xFFFF;
    idt[index].selector = selector;
    idt[index].ist = 0;
    idt[index].type_attr = type_attr;
    idt[index].offset_mid = (handler >> 16) & 0xFFFF;
    idt[index].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[index].zero = 0;
}

/**
 * Initialize PIC (Programmable Interrupt Controller)
 */
static void init_pic(void) {
    // ICW1: Initialize PIC
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);
    
    // ICW2: Set interrupt vector offsets
    outb(PIC1_DATA, 0x20);  // PIC1 starts at interrupt 0x20 (32)
    outb(PIC2_DATA, 0x28);  // PIC2 starts at interrupt 0x28 (40)
    
    // ICW3: Setup cascade
    outb(PIC1_DATA, 0x04);  // PIC1 has slave on IRQ2
    outb(PIC2_DATA, 0x02);  // PIC2 cascade identity
    
    // ICW4: Set mode
    outb(PIC1_DATA, 0x01);  // 8086 mode
    outb(PIC2_DATA, 0x01);  // 8086 mode
    
    // Mask all interrupts initially
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/**
 * Initialize PIT (Programmable Interval Timer)
 */
static void init_pit(uint32_t frequency) {
    // Calculate divisor for desired frequency
    uint32_t divisor = 1193180 / frequency;
    
    // Set command byte: Channel 0, lobyte/hibyte, rate generator
    outb(PIT_COMMAND_PORT, 0x36);
    
    // Send divisor
    outb(PIT_DATA_PORT0, divisor & 0xFF);       // Low byte
    outb(PIT_DATA_PORT0, (divisor >> 8) & 0xFF); // High byte
}

/**
 * Initialize IDT (Interrupt Descriptor Table)
 */
static void init_idt(void) {
    // Clear IDT
    for (int i = 0; i < IDT_ENTRIES; i++) {
        set_idt_entry(i, 0, 0, 0);
    }
    
    // Set timer interrupt handler (IRQ0 -> INT 32)
    set_idt_entry(32, (uint64_t)timer_interrupt_entry, 0x18, 0x8E);
    
    // Set system call handler (INT 0x80)
    set_idt_entry(0x80, (uint64_t)syscall_yield_entry, 0x18, 0xEE);
    
    // Setup IDT pointer
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)&idt;
    
    // Load IDT
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}

/**
 * Setup timer interrupt for scheduling
 */
void setup_timer_interrupt(uint32_t frequency) {
    // Initialize IDT
    init_idt();
    
    // Initialize PIC
    init_pic();
    
    // Initialize PIT
    init_pit(frequency);
    
    // Enable timer interrupt (IRQ0)
    uint8_t mask = inb(PIC1_DATA);
    mask &= ~0x01;  // Clear bit 0 (IRQ0)
    outb(PIC1_DATA, mask);
}

/**
 * Enable interrupts
 */
void enable_interrupts(void) {
    __asm__ volatile("sti");
}

/**
 * Disable interrupts
 */
void disable_interrupts(void) {
    __asm__ volatile("cli");
}

/**
 * Send End of Interrupt to PIC
 */
void send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20); // Send EOI to slave PIC
    }
    outb(PIC1_COMMAND, 0x20); // Send EOI to master PIC
}

/**
 * Set interrupt mask
 */
void set_irq_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) | (1 << irq);
    outb(port, value);
}

/**
 * Clear interrupt mask
 */
void clear_irq_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}
