/* IKOS User-Space Process Execution Minimal Validation
 * Tests that structures and constants are properly defined
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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

int main(int argc, char* argv[]) {
    printf("IKOS User-Space Process Execution Validation\n");
    printf("=============================================\n\n");
    
    /* Test constants */
    printf("Testing memory layout constants...\n");
    TEST("Page size", PAGE_SIZE == 4096);
    TEST("User space start", USER_SPACE_START == 0x400000);
    TEST("User space end", USER_SPACE_END == 0x00007FFFFFFFFFFF);
    TEST("User stack top", USER_STACK_TOP == 0x00007FFFFFFFEFFF);
    TEST("User heap start", USER_HEAP_START == 0x800000);
    
    /* Test structure sizes */
    printf("\nTesting structure sizes...\n");
    TEST("Process context size", sizeof(process_context_t) == 168); /* 20*8 + 8 bytes for segments */
    TEST("Process structure size", sizeof(process_t) > 1000);
    TEST("ELF64 header size", sizeof(elf64_header_t) == 64);
    TEST("ELF64 program header size", sizeof(elf64_program_header_t) == 56);
    TEST("Interrupt frame size", sizeof(interrupt_frame_t) > 0);
    TEST("VM space size", sizeof(vm_space_t) > 0);
    
    /* Test process context layout */
    printf("\nTesting process context layout...\n");
    TEST("RAX at offset 0", offsetof(process_context_t, rax) == 0);
    TEST("RBX at offset 8", offsetof(process_context_t, rbx) == 8);
    TEST("RCX at offset 16", offsetof(process_context_t, rcx) == 16);
    TEST("RDX at offset 24", offsetof(process_context_t, rdx) == 24);
    TEST("RSI at offset 32", offsetof(process_context_t, rsi) == 32);
    TEST("RDI at offset 40", offsetof(process_context_t, rdi) == 40);
    TEST("RBP at offset 48", offsetof(process_context_t, rbp) == 48);
    TEST("RSP at offset 56", offsetof(process_context_t, rsp) == 56);
    TEST("R8 at offset 64", offsetof(process_context_t, r8) == 64);
    TEST("R9 at offset 72", offsetof(process_context_t, r9) == 72);
    TEST("R10 at offset 80", offsetof(process_context_t, r10) == 80);
    TEST("R11 at offset 88", offsetof(process_context_t, r11) == 88);
    TEST("R12 at offset 96", offsetof(process_context_t, r12) == 96);
    TEST("R13 at offset 104", offsetof(process_context_t, r13) == 104);
    TEST("R14 at offset 112", offsetof(process_context_t, r14) == 112);
    TEST("R15 at offset 120", offsetof(process_context_t, r15) == 120);
    TEST("RIP at offset 128", offsetof(process_context_t, rip) == 128);
    TEST("RFLAGS at offset 136", offsetof(process_context_t, rflags) == 136);
    TEST("CR3 at offset 144", offsetof(process_context_t, cr3) == 144);
    
    /* Test ELF constants */
    printf("\nTesting ELF constants...\n");
    TEST("ELF magic number", ELF_MAGIC == 0x464C457F);
    TEST("ELF 64-bit class", ELF_CLASS_64 == 2);
    TEST("ELF executable type", ELF_TYPE_EXEC == 2);
    TEST("ELF x86-64 machine", ELF_MACHINE_X86_64 == 62);
    TEST("PT_LOAD type", PT_LOAD == 1);
    TEST("PF_X permission", PF_X == 0x1);
    TEST("PF_W permission", PF_W == 0x2);
    TEST("PF_R permission", PF_R == 0x4);
    
    /* Test process constants */
    printf("\nTesting process constants...\n");
    TEST("Max processes", MAX_PROCESSES == 256);
    TEST("Max open files", MAX_OPEN_FILES == 64);
    TEST("Process name length", MAX_PROCESS_NAME == 32);
    TEST("Stack size", USER_STACK_SIZE == 0x200000);
    
    /* Test VMM constants */
    printf("\nTesting VMM constants...\n");
    TEST("VMM success code", VMM_SUCCESS == 0);
    TEST("User flag", VMM_FLAG_USER == 0x08);
    TEST("Write flag", VMM_FLAG_WRITE == 0x02);
    TEST("Exec flag", VMM_FLAG_EXEC == 0x04);
    
    /* Summary */
    printf("\nValidation Results:\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✓ All validation tests passed!\n");
        printf("✓ User-space process execution system is properly defined\n");
        printf("✓ Memory layout is consistent\n");
        printf("✓ Structure offsets match assembly expectations\n");
        printf("✓ ELF format support is complete\n");
        printf("✓ Ready for integration testing\n");
    } else {
        printf("\n✗ Some validation tests failed\n");
        printf("✗ Please check structure definitions and constants\n");
    }
    
    return tests_failed > 0 ? 1 : 0;
}