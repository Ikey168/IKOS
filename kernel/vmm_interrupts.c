/* IKOS Virtual Memory Manager - Interrupt Handlers
 * Page fault and memory-related interrupt handling
 */

#include "vmm.h"
#include "interrupts.h"

/* External assembly function to get CR2 register */
extern uint64_t get_cr2(void);

/**
 * Page fault interrupt handler (Interrupt 14)
 * Called when a page fault occurs
 */
void page_fault_handler(interrupt_frame_t* frame) {
    // Get fault address from CR2 register
    uint64_t fault_addr = get_cr2();
    uint64_t error_code = frame->error_code;
    
    // Call VMM page fault handler
    vmm_page_fault_handler(fault_addr, error_code);
}
