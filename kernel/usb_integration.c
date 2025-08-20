/* IKOS USB Framework Integration
 * 
 * Integration layer for the USB driver framework with the existing IKOS kernel.
 * This provides the necessary stubs and integration points.
 */

#include "stdio.h"
#include "memory.h"
#include "string.h"

/* Memory allocation aligned stub */
void* malloc_aligned(size_t size, size_t alignment) {
    /* Simple aligned allocation - in a real implementation would use proper alignment */
    void* ptr = malloc(size + alignment - 1);
    if (!ptr) return NULL;
    
    /* Align the pointer */
    uintptr_t aligned = ((uintptr_t)ptr + alignment - 1) & ~(alignment - 1);
    return (void*)aligned;
}

/* IRQ handler registration stubs */
int register_irq_handler(int irq, void (*handler)(int, void*), void* context) {
    /* In a real implementation, this would register the IRQ handler */
    printf("[USB] IRQ handler registered for IRQ %d\n", irq);
    return 0;
}

void unregister_irq_handler(int irq) {
    /* In a real implementation, this would unregister the IRQ handler */
    printf("[USB] IRQ handler unregistered for IRQ %d\n", irq);
}

/* System call registration stub */
void register_syscall(int syscall_num, void* handler) {
    /* In a real implementation, this would register the system call */
    printf("[USB] System call %d registered\n", syscall_num);
}

/* Process management stubs */
uint32_t get_current_pid(void) {
    /* Return a dummy PID for testing */
    return 1;
}

bool is_user_address_valid(uint32_t addr, size_t size) {
    /* Simple validation - in a real implementation would check memory map */
    return (addr != 0 && size > 0 && addr < 0x80000000);
}

int copy_to_user(void* to, const void* from, size_t size) {
    /* Simple copy - in a real implementation would handle page faults */
    if (!to || !from || size == 0) return -1;
    memcpy(to, from, size);
    return 0;
}

int copy_from_user(void* to, const void* from, size_t size) {
    /* Simple copy - in a real implementation would handle page faults */
    if (!to || !from || size == 0) return -1;
    memcpy(to, from, size);
    return 0;
}

/* Simple snprintf implementation */
int snprintf(char* str, size_t size, const char* format, ...) {
    if (!str || size == 0) return 0;
    
    /* Very simple implementation - just copy format string */
    size_t len = strlen(format);
    if (len >= size) len = size - 1;
    
    memcpy(str, format, len);
    str[len] = '\0';
    
    return len;
}

/* USB Framework test integration */
extern int usb_test_main(void);
extern void usb_test_cleanup(void);

/* Initialize USB framework for testing */
int usb_framework_init(void) {
    printf("\n=== Initializing USB Driver Framework ===\n");
    
    /* Run USB framework tests */
    int result = usb_test_main();
    
    if (result == 0) {
        printf("✓ USB Driver Framework initialized successfully\n");
    } else {
        printf("✗ USB Driver Framework initialization failed\n");
    }
    
    return result;
}

/* Cleanup USB framework */
void usb_framework_cleanup(void) {
    printf("\n=== Cleaning up USB Driver Framework ===\n");
    usb_test_cleanup();
    printf("✓ USB Driver Framework cleanup completed\n");
}
