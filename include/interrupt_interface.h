/* IKOS Interrupt Interface Header
 * Function declarations for interrupt system functionality
 */

#ifndef INTERRUPT_INTERFACE_H
#define INTERRUPT_INTERFACE_H

#include <stdint.h>

/* Forward declaration */
struct interrupt_frame;
typedef struct interrupt_frame interrupt_frame_t;

/* Core interrupt functions */
void irq_handler(interrupt_frame_t* frame);

/* Timer functions */
uint64_t get_timer_ticks(void);

/* Keyboard functions */
char keyboard_getchar(void);
int keyboard_has_data(void);

/* Interrupt statistics */
uint64_t get_interrupt_count(uint8_t int_no);

/* Scancode conversion */
char scancode_to_ascii(uint8_t scancode);

/* String functions */
int strcmp(const char* str1, const char* str2);

#endif /* INTERRUPT_INTERFACE_H */
