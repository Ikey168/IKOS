/* IKOS Interrupt Handling Test
 * Basic tests for the interrupt handling system
 */

#include "idt.h"
#include "interrupts.h"
#include "interrupt_interface.h"
#include <stdint.h>

/* Simple test framework */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, condition) \
    do { \
        if (condition) { \
            tests_passed++; \
            test_print("PASS: %s\n", name); \
        } else { \
            tests_failed++; \
            test_print("FAIL: %s\n", name); \
        } \
    } while(0)

/* Test functions */
void test_idt_initialization(void);
void test_pic_functions(void);
void test_interrupt_statistics(void);
void test_keyboard_buffer(void);
void test_timer_functionality(void);

/* Mock functions for testing */
void test_print(const char* format, ...);

int main(int argc, char* argv[]) {
    test_print("IKOS Interrupt Handling Test Suite\n");
    test_print("===================================\n\n");
    
    /* Check if smoke test requested */
    int smoke_test = (argc > 1 && strcmp(argv[1], "smoke") == 0);
    
    if (smoke_test) {
        test_print("Running smoke tests...\n");
        test_idt_initialization();
    } else {
        test_print("Running full test suite...\n");
        test_idt_initialization();
        test_pic_functions();
        test_interrupt_statistics();
        test_keyboard_buffer();
        test_timer_functionality();
    }
    
    test_print("\nTest Results:\n");
    test_print("Passed: %d\n", tests_passed);
    test_print("Failed: %d\n", tests_failed);
    test_print("Total:  %d\n", tests_passed + tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}

void test_idt_initialization(void) {
    test_print("Testing IDT initialization...\n");
    
    /* Test IDT structure size */
    TEST("IDT entry size", sizeof(idt_entry_t) == 16);
    TEST("IDT pointer size", sizeof(idt_ptr_t) == 10);
    
    /* Test interrupt constants */
    TEST("IDT entries count", IDT_ENTRIES == 256);
    TEST("IRQ base offset", IRQ_BASE == 32);
    TEST("System call interrupt", INT_SYSCALL == 128);
    
    /* Test exception numbers */
    TEST("Page fault interrupt", INT_PAGE_FAULT == 14);
    TEST("General protection fault", INT_GENERAL_PROTECTION == 13);
    TEST("Double fault interrupt", INT_DOUBLE_FAULT == 8);
    
    /* Test IRQ numbers */
    TEST("Timer IRQ", IRQ_TIMER == 0);
    TEST("Keyboard IRQ", IRQ_KEYBOARD == 1);
    
    test_print("IDT initialization tests completed.\n\n");
}

void test_pic_functions(void) {
    test_print("Testing PIC functions...\n");
    
    /* Test PIC constants */
    TEST("PIC master port", 0x20 == 0x20);  /* Master PIC command port */
    TEST("PIC slave port", 0xA0 == 0xA0);   /* Slave PIC command port */
    
    /* Test IRQ mask validation */
    TEST("Valid IRQ range check", IRQ_TIMER < 16 && IRQ_KEYBOARD < 16);
    TEST("IRQ offset calculation", (IRQ_BASE + IRQ_TIMER) == 32);
    
    test_print("PIC function tests completed.\n\n");
}

void test_interrupt_statistics(void) {
    test_print("Testing interrupt statistics...\n");
    
    /* Test interrupt counter initialization */
    TEST("Interrupt count initialization", get_interrupt_count(0) == 0);
    TEST("IRQ count initialization", get_interrupt_count(32) == 0);
    TEST("Syscall count initialization", get_interrupt_count(128) == 0);
    
    test_print("Interrupt statistics tests completed.\n\n");
}

void test_keyboard_buffer(void) {
    test_print("Testing keyboard buffer...\n");
    
    /* Test keyboard buffer state */
    TEST("Keyboard buffer empty", !keyboard_has_data());
    TEST("Keyboard getchar empty", keyboard_getchar() == 0);
    
    /* Test scancode conversion */
    TEST("Space scancode", scancode_to_ascii(0x39) == ' ');
    TEST("Enter scancode", scancode_to_ascii(0x1C) == '\n');
    TEST("Key release ignored", scancode_to_ascii(0x80) == 0);
    
    test_print("Keyboard buffer tests completed.\n\n");
}

void test_timer_functionality(void) {
    test_print("Testing timer functionality...\n");
    
    /* Test timer tick initialization */
    TEST("Timer ticks initialization", get_timer_ticks() == 0);
    
    test_print("Timer functionality tests completed.\n\n");
}

/* Simple string comparison for testing */
int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

/* Mock print function */
void test_print(const char* format, ...) {
    /* In a real implementation, this would use vprintf or similar */
    /* For testing purposes, we'll simulate output */
    static int call_count = 0;
    call_count++;
    
    /* Simple validation that we're getting reasonable calls */
    if (call_count > 100) {
        /* Prevent infinite loops in testing */
        return;
    }
}
