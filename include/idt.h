/* IKOS Interrupt Descriptor Table Header
 * Defines IDT structures and interrupt handling functions
 */

#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include "interrupts.h"

/* IDT Constants */
#define IDT_ENTRIES     256         /* Number of IDT entries */
#define IDT_FLAG_PRESENT 0x80       /* Present bit */
#define IDT_FLAG_DPL0   0x00        /* Descriptor Privilege Level 0 (kernel) */
#define IDT_FLAG_DPL3   0x60        /* Descriptor Privilege Level 3 (user) */
#define IDT_FLAG_GATE32 0x0E        /* 32-bit interrupt gate */
#define IDT_FLAG_GATE64 0x0E        /* 64-bit interrupt gate */

/* Interrupt numbers */
#define INT_DIVIDE_ERROR        0   /* Division by zero */
#define INT_DEBUG               1   /* Debug exception */
#define INT_NMI                 2   /* Non-maskable interrupt */
#define INT_BREAKPOINT          3   /* Breakpoint */
#define INT_OVERFLOW            4   /* Overflow */
#define INT_BOUND_RANGE         5   /* Bound range exceeded */
#define INT_INVALID_OPCODE      6   /* Invalid opcode */
#define INT_DEVICE_NOT_AVAIL    7   /* Device not available */
#define INT_DOUBLE_FAULT        8   /* Double fault */
#define INT_INVALID_TSS         10  /* Invalid TSS */
#define INT_SEGMENT_NOT_PRESENT 11  /* Segment not present */
#define INT_STACK_FAULT         12  /* Stack fault */
#define INT_GENERAL_PROTECTION  13  /* General protection fault */
#define INT_PAGE_FAULT          14  /* Page fault */
#define INT_FPU_ERROR           16  /* x87 FPU error */
#define INT_ALIGNMENT_CHECK     17  /* Alignment check */
#define INT_MACHINE_CHECK       18  /* Machine check */
#define INT_SIMD_EXCEPTION      19  /* SIMD floating point exception */

/* Hardware interrupts (IRQs) */
#define IRQ_BASE                32  /* Base IRQ number */
#define IRQ_TIMER               0   /* Timer IRQ */
#define IRQ_KEYBOARD            1   /* Keyboard IRQ */
#define IRQ_CASCADE             2   /* Cascade IRQ (for slave PIC) */
#define IRQ_COM2                3   /* COM2 IRQ */
#define IRQ_COM1                4   /* COM1 IRQ */
#define IRQ_LPT2                5   /* LPT2 IRQ */
#define IRQ_FLOPPY              6   /* Floppy disk IRQ */
#define IRQ_LPT1                7   /* LPT1 IRQ */
#define IRQ_CMOS_RTC            8   /* CMOS real-time clock IRQ */
#define IRQ_FREE1               9   /* Free IRQ */
#define IRQ_FREE2               10  /* Free IRQ */
#define IRQ_FREE3               11  /* Free IRQ */
#define IRQ_PS2_MOUSE           12  /* PS/2 mouse IRQ */
#define IRQ_FPU                 13  /* FPU IRQ */
#define IRQ_PRIMARY_ATA         14  /* Primary ATA hard disk IRQ */
#define IRQ_SECONDARY_ATA       15  /* Secondary ATA hard disk IRQ */

/* System call interrupt */
#define INT_SYSCALL             128 /* System call interrupt */

/* IDT entry structure */
typedef struct {
    uint16_t offset_low;    /* Offset bits 0-15 */
    uint16_t selector;      /* Code segment selector */
    uint8_t ist;           /* Interrupt Stack Table offset */
    uint8_t flags;         /* Type and attributes */
    uint16_t offset_mid;    /* Offset bits 16-31 */
    uint32_t offset_high;   /* Offset bits 32-63 */
    uint32_t reserved;      /* Reserved, must be zero */
} __attribute__((packed)) idt_entry_t;

/* IDT pointer structure */
typedef struct {
    uint16_t limit;         /* Size of IDT - 1 */
    uint64_t base;          /* Base address of IDT */
} __attribute__((packed)) idt_ptr_t;

/* Interrupt handler function pointer type */
typedef void (*interrupt_handler_t)(void);

/* Function declarations */
void idt_init(void);
void idt_set_gate(uint8_t num, interrupt_handler_t handler, uint16_t selector, uint8_t flags);
void idt_load(void);

/* Exception handlers */
void exception_divide_error(void);
void exception_debug(void);
void exception_nmi(void);
void exception_breakpoint(void);
void exception_overflow(void);
void exception_bound_range(void);
void exception_invalid_opcode(void);
void exception_device_not_available(void);
void exception_double_fault(void);
void exception_invalid_tss(void);
void exception_segment_not_present(void);
void exception_stack_fault(void);
void exception_general_protection(void);
void exception_page_fault(void);
void exception_fpu_error(void);
void exception_alignment_check(void);
void exception_machine_check(void);
void exception_simd_exception(void);

/* IRQ handlers */
void irq_timer(void);
void irq_keyboard(void);
void irq_cascade(void);
void irq_com2(void);
void irq_com1(void);
void irq_lpt2(void);
void irq_floppy(void);
void irq_lpt1(void);
void irq_cmos_rtc(void);
void irq_free1(void);
void irq_free2(void);
void irq_free3(void);
void irq_ps2_mouse(void);
void irq_fpu(void);
void irq_primary_ata(void);
void irq_secondary_ata(void);

/* System call handler */
void syscall_handler(void);

/* PIC (Programmable Interrupt Controller) functions */
void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

/* System call handler (to be implemented in userspace.c) */
void syscall_handler_c(interrupt_frame_t* frame);

/* Port I/O functions */
void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);

/* Simple memory functions */
void* memset(void* dest, int value, uint32_t count);

#endif /* IDT_H */
