/* IKOS Basic C Library Implementation
 * Minimal C library functions for kernel use
 * Updated to use new kalloc system for memory allocation
 */

#include "memory.h"
#include "../include/kalloc.h"
#include <stdint.h>
#include <stddef.h>

/* Legacy allocator state - now unused but kept for reference */
static uint8_t old_heap[1024 * 1024]; // 1MB heap (deprecated)
static size_t old_heap_pos = 0;       // deprecated

/* Legacy memory allocation wrappers - redirect to new kalloc system */
void* kmalloc(size_t size) {
    /* Use new kalloc system if available */
    extern void* kalloc(size_t size);
    void* ptr = kalloc(size);
    if (ptr) {
        return ptr;
    }
    
    /* Fallback to old allocator if kalloc not initialized */
    if (old_heap_pos + size >= sizeof(old_heap)) {
        return NULL; // Out of memory
    }
    
    ptr = &old_heap[old_heap_pos];
    old_heap_pos += size;
    
    /* Align to 8-byte boundary */
    old_heap_pos = (old_heap_pos + 7) & ~7;
    
    return ptr;
}

void kfree(void* ptr) {
    /* Try new kalloc system first */
    extern bool kalloc_is_valid_pointer(void* ptr);
    extern void kalloc_kfree(void* ptr);  /* Use wrapper to avoid conflict */
    
    if (kalloc_is_valid_pointer && kalloc_is_valid_pointer(ptr)) {
        kalloc_kfree(ptr);
        return;
    }
    
    /* Old simple allocator doesn't support free - just ignore */
    (void)ptr;
}

void* memset(void* dest, int value, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    for (size_t i = 0; i < count; i++) {
        d[i] = (uint8_t)value;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < count; i++) {
        d[i] = s[i];
    }
    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, size_t count) {
    const uint8_t* p1 = (const uint8_t*)ptr1;
    const uint8_t* p2 = (const uint8_t*)ptr2;
    for (size_t i = 0; i < count; i++) {
        if (p1[i] < p2[i]) return -1;
        if (p1[i] > p2[i]) return 1;
    }
    return 0;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* orig_dest = dest;
    while ((*dest++ = *src++));
    return orig_dest;
}

char* strncpy(char* dest, const char* src, size_t count) {
    char* orig_dest = dest;
    while (count-- && (*dest++ = *src++));
    while (count--) *dest++ = '\0';
    return orig_dest;
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

int strncmp(const char* str1, const char* str2, size_t count) {
    while (count-- && *str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    if (count == SIZE_MAX) return 0;
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

/* Basic printf implementation */
static void putchar(char c) {
    /* Simple VGA text mode output at 0xB8000 */
    static uint16_t* vga = (uint16_t*)0xB8000;
    static int pos = 0;
    
    if (c == '\n') {
        pos = (pos / 80 + 1) * 80;
    } else {
        vga[pos++] = (uint16_t)c | 0x0F00; // White on black
    }
    
    if (pos >= 80 * 25) {
        pos = 0; // Wrap around
    }
}

static void print_string(const char* str) {
    while (*str) {
        putchar(*str++);
    }
}

static void print_number(unsigned long num, int base) {
    char buffer[32];
    int i = 0;
    
    if (num == 0) {
        putchar('0');
        return;
    }
    
    while (num > 0) {
        int digit = num % base;
        buffer[i++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        num /= base;
    }
    
    while (--i >= 0) {
        putchar(buffer[i]);
    }
}

int printf(const char* format, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, format);
    
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd':
                    print_number(__builtin_va_arg(args, int), 10);
                    break;
                case 'u':
                    print_number(__builtin_va_arg(args, unsigned int), 10);
                    break;
                case 'x':
                    print_number(__builtin_va_arg(args, unsigned int), 16);
                    break;
                case 'l':
                    format++;
                    if (*format == 'u') {
                        print_number(__builtin_va_arg(args, unsigned long), 10);
                    } else if (*format == 'd') {
                        print_number(__builtin_va_arg(args, long), 10);
                    }
                    break;
                case 's':
                    print_string(__builtin_va_arg(args, const char*));
                    break;
                case 'c':
                    putchar(__builtin_va_arg(args, int));
                    break;
                default:
                    putchar(*format);
                    break;
            }
        } else {
            putchar(*format);
        }
        format++;
    }
    
    __builtin_va_end(args);
    return 0;
}
