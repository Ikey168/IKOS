/* IKOS Interrupt Management Header
 * Defines interrupt handling functions and constants
 */

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>
#include <stdbool.h>

/* Interrupt frame structure */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax;
    uint64_t int_no, error_code;
    uint64_t rip, cs, rflags, user_rsp, ss;
} interrupt_frame_t;

/* Function declarations */
void setup_timer_interrupt(uint32_t frequency);
void enable_interrupts(void);
void disable_interrupts(void);
void send_eoi(uint8_t irq);
void set_irq_mask(uint8_t irq);
void clear_irq_mask(uint8_t irq);

/* Timer interrupt handler (called from assembly) */
void timer_interrupt_handler(void);

/* Assembly context switching functions */
void save_task_context(void* task);
void restore_task_context(void* task);
void switch_task_context(void* prev_task, void* next_task);

/* Memory management functions (should be defined elsewhere) */
void* kmalloc(uint32_t size);
void kfree(void* ptr);
int task_setup_memory(void* task, uint32_t stack_size);
void task_free_stack(void* stack_base, uint32_t stack_size);

/* Standard library functions (should be defined elsewhere) */
void* memset(void* ptr, int value, uint32_t size);
char* strncpy(char* dest, const char* src, uint32_t n);
char* strcpy(char* dest, const char* src);

#endif /* INTERRUPTS_H */
