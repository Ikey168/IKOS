/* Simple Advanced Memory Management Test */

#include "../include/memory_advanced.h"

/* Simple test main - just verify basic allocation works */
int _start() {
    // Test basic memory allocation
    void* ptr1 = kmalloc_new(1024, GFP_KERNEL);
    if (!ptr1) {
        return 1; // Failed
    }
    
    kfree_new(ptr1);
    
    // Test zeroed allocation
    void* ptr2 = kmalloc_zeroed(512, GFP_KERNEL);
    if (!ptr2) {
        return 2; // Failed
    }
    
    kfree_new(ptr2);
    
    return 0; // Success
}
