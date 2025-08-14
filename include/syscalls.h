/* IKOS System Call Interface
 * Defines system call numbers and common syscall infrastructure
 */

#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include "interrupts.h"

/* System call numbers */
#define SYS_EXIT        60
#define SYS_WRITE       1
#define SYS_READ        0
#define SYS_OPEN        2
#define SYS_CLOSE       3
#define SYS_FORK        57
#define SYS_EXECVE      59
#define SYS_GETPID      39
#define SYS_GETPPID     110
#define SYS_WAIT        61

/* Keyboard syscalls */
#define SYS_KEYBOARD_READ     140
#define SYS_KEYBOARD_POLL     141
#define SYS_KEYBOARD_IOCTL    142
#define SYS_KEYBOARD_GETCHAR  143

/* IPC syscalls */
#define SYS_IPC_SEND      200
#define SYS_IPC_RECEIVE   201

/* Process manager syscalls */
#define SYS_PS            300
#define SYS_KILL          301

/* VFS syscalls */
#define SYS_VFS_MOUNT     400
#define SYS_VFS_UNMOUNT   401

/* System call parameter structure */
typedef struct {
    uint64_t param1;
    uint64_t param2;
    uint64_t param3;
    uint64_t param4;
    uint64_t param5;
    uint64_t param6;
} syscall_params_t;

/* Function declarations */
extern long handle_system_call(interrupt_frame_t* frame);
extern int syscall_init(void);

/* Syscall registration functions */
typedef long (*syscall_handler_t)(uint64_t arg1, uint64_t arg2, uint64_t arg3, 
                                   uint64_t arg4, uint64_t arg5, uint64_t arg6);

int register_syscall_handler(uint32_t syscall_num, syscall_handler_t handler);
int unregister_syscall_handler(uint32_t syscall_num);

/* User-space syscall interface (inline assembly) */
static inline long syscall0(long number) {
    long result;
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (number)
        : "memory"
    );
    return result;
}

static inline long syscall1(long number, long arg1) {
    long result;
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (number), "D" (arg1)
        : "memory"
    );
    return result;
}

static inline long syscall2(long number, long arg1, long arg2) {
    long result;
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (number), "D" (arg1), "S" (arg2)
        : "memory"
    );
    return result;
}

static inline long syscall3(long number, long arg1, long arg2, long arg3) {
    long result;
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (number), "D" (arg1), "S" (arg2), "d" (arg3)
        : "memory"
    );
    return result;
}

static inline long syscall6(long number, long arg1, long arg2, long arg3, 
                           long arg4, long arg5, long arg6) {
    long result;
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (number), "D" (arg1), "S" (arg2), "d" (arg3), 
          "r" (arg4), "r" (arg5), "r" (arg6)
        : "memory"
    );
    return result;
}

/* Generic syscall function */
static inline long syscall(long number, ...) {
    /* For simplicity, assume maximum 6 arguments */
    return syscall6(number, 0, 0, 0, 0, 0, 0);
}

#endif /* SYSCALLS_H */
