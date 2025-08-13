/* IKOS User-Space Process Execution Simple Test
 * Tests basic constants and data structure definitions
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Include kernel headers */
#include "process.h"
#include "elf.h"
#include "vmm.h"

/* Test framework */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, condition) \
    do { \
        if (condition) { \
            tests_passed++; \
            printf("PASS: %s\n", name); \
        } else { \
            tests_failed++; \
            printf("FAIL: %s\n", name); \
        } \
    } while(0)

/* Get offset of member in structure */
#ifndef offsetof
#define offsetof(type, member) ((size_t)&(((type*)0)->member))
#endif

void test_constants(void);
void test_data_structures(void);

int main(int argc, char* argv[]) {
    printf("IKOS User-Space Process Execution Simple Test\n");
    printf("==============================================\n\n");
    
    /* Check if smoke test requested */
    int smoke_test = (argc > 1 && strcmp(argv[1], "smoke") == 0);
    
    if (smoke_test) {
        printf("Running smoke tests...\n");
        test_constants();
        test_data_structures();
    } else {
        printf("Running basic structure tests...\n");
        test_constants();
        test_data_structures();
    }
    
    printf("\nTest Results:\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}

void test_constants(void) {
    printf("Testing constants and memory layout...\n");
    
    /* Test VMM constants */
    TEST("Page size", PAGE_SIZE == 4096);
    TEST("User virtual base", USER_VIRTUAL_BASE == 0x400000);
    TEST("User stack top", USER_STACK_TOP > USER_VIRTUAL_BASE);
    
    /* Test process constants */
    TEST("User space start", USER_SPACE_START == USER_VIRTUAL_BASE);
    TEST("User space end", USER_SPACE_END == USER_VIRTUAL_END);
    TEST("User heap start", USER_HEAP_START == USER_HEAP_BASE);
    TEST("Max processes", MAX_PROCESSES == 256);
    TEST("Max open files", MAX_OPEN_FILES == 64);
    TEST("User stack size", USER_STACK_SIZE == 0x200000);
    
    /* Test ELF constants */
    TEST("ELF magic", ELF_MAGIC == 0x464C457F);
    TEST("ELF class 64", ELF_CLASS_64 == 2);
    TEST("ELF executable type", ELF_TYPE_EXEC == 2);
    TEST("ELF x86-64 machine", ELF_MACHINE_X86_64 == 62);
    
    /* Test program header types */
    TEST("PT_LOAD constant", PT_LOAD == 1);
    TEST("PT_DYNAMIC constant", PT_DYNAMIC == 2);
    
    /* Test program header flags */
    TEST("PF_X flag", PF_X == 0x1);
    TEST("PF_W flag", PF_W == 0x2);
    TEST("PF_R flag", PF_R == 0x4);
    
    printf("Constants tests completed.\n\n");
}

void test_data_structures(void) {
    printf("Testing data structure sizes and layouts...\n");
    
    /* Test ELF header sizes */
    TEST("ELF64 header size", sizeof(elf64_header_t) == 64);
    TEST("ELF64 program header size", sizeof(elf64_program_header_t) == 56);
    
    /* Test process structure sizes */
    TEST("Process context size", sizeof(process_context_t) > 0);
    TEST("Process structure size", sizeof(process_t) > 0);
    TEST("Process stats size", sizeof(process_stats_t) > 0);
    
    /* Test structure layouts */
    process_context_t ctx;
    TEST("Context structure allocation", sizeof(ctx) > 0);
    TEST("RAX offset", offsetof(process_context_t, rax) == 0);
    TEST("RIP in context", offsetof(process_context_t, rip) > 0);
    TEST("CR3 in context", offsetof(process_context_t, cr3) > 0);
    
    /* Test VMM structures */
    TEST("VMM page size matches", PAGE_SIZE == 4096);
    TEST("VM space structure", sizeof(vm_space_t) > 0);
    TEST("VM region structure", sizeof(vm_region_t) > 0);
    
    /* Test memory layout consistency */
    TEST("User space size", USER_SPACE_END > USER_SPACE_START);
    TEST("Stack size reasonable", USER_STACK_SIZE >= 0x100000); /* At least 1MB */
    TEST("Code load address", USER_CODE_LOAD_ADDR == USER_SPACE_START);
    printf("DEBUG: USER_SPACE_END=0x%lX, USER_STACK_TOP=0x%lX\n", USER_SPACE_END, USER_STACK_TOP);
    TEST("User space includes stack area", USER_SPACE_END >= USER_STACK_TOP - USER_STACK_SIZE);
    TEST("Heap after code", USER_HEAP_START > USER_CODE_LOAD_ADDR);
    
    printf("Data structure tests completed.\n\n");
}

/* Simple string comparison for strcmp if not available */
int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}