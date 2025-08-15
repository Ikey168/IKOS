/* IKOS User-Space Hello World Test Program - Issue #14
 * Simple test program to demonstrate user-space execution
 */

#include <stddef.h>  /* For size_t */

/* IKOS User-Space API */
long sys_write(int fd, const char* buffer, size_t count);
void sys_exit(int status);
long sys_getpid(void);

/* System call implementations */
long sys_write(int fd, const char* buffer, size_t count) {
    long result;
    __asm__ volatile (
        "mov $1, %%rax\n\t"     /* SYS_WRITE = 1 */
        "mov %1, %%rdi\n\t"     /* fd */
        "mov %2, %%rsi\n\t"     /* buffer */
        "mov %3, %%rdx\n\t"     /* count */
        "int $0x80\n\t"         /* System call interrupt */
        "mov %%rax, %0\n\t"     /* result */
        : "=r" (result)
        : "r" ((long)fd), "r" (buffer), "r" (count)
        : "rax", "rdi", "rsi", "rdx"
    );
    return result;
}

void sys_exit(int status) {
    __asm__ volatile (
        "mov $60, %%rax\n\t"    /* SYS_EXIT = 60 */
        "mov %0, %%rdi\n\t"     /* status */
        "int $0x80\n\t"         /* System call interrupt */
        :
        : "r" ((long)status)
        : "rax", "rdi"
    );
    /* Should never return */
    while(1);
}

long sys_getpid(void) {
    long result;
    __asm__ volatile (
        "mov $39, %%rax\n\t"    /* SYS_GETPID = 39 */
        "int $0x80\n\t"         /* System call interrupt */
        "mov %%rax, %0\n\t"     /* result */
        : "=r" (result)
        :
        : "rax"
    );
    return result;
}

/* String functions */
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

/* Simple printf-like function using sys_write */
void print(const char* str) {
    sys_write(1, str, strlen(str));  /* fd=1 is stdout */
}

void print_number(long num) {
    char buffer[32];
    char* ptr = buffer + sizeof(buffer) - 1;
    *ptr = '\0';
    
    if (num == 0) {
        *(--ptr) = '0';
    } else {
        int negative = 0;
        if (num < 0) {
            negative = 1;
            num = -num;
        }
        
        while (num > 0) {
            *(--ptr) = '0' + (num % 10);
            num /= 10;
        }
        
        if (negative) {
            *(--ptr) = '-';
        }
    }
    
    print(ptr);
}

/* Main program */
int main(void) {
    print("Hello from IKOS user-space!\n");
    print("This is a simple test program running in user mode.\n");
    
    print("Process ID: ");
    print_number(sys_getpid());
    print("\n");
    
    print("Testing system calls...\n");
    
    /* Test multiple writes */
    for (int i = 1; i <= 5; i++) {
        print("Count: ");
        print_number(i);
        print("\n");
    }
    
    print("User-space test completed successfully!\n");
    print("Exiting gracefully...\n");
    
    sys_exit(0);
    return 0;  /* Should never reach here */
}

/* Entry point that sets up a minimal environment */
void _start(void) {
    /* Simple initialization - in a real program we'd set up stack, etc. */
    int result = main();
    sys_exit(result);
}
