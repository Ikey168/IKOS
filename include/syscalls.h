/* IKOS System Call Interface
 * Defines system call numbers and common syscall infrastructure
 */

#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include "interrupts.h"

/* Framebuffer syscalls */
#define SYS_FB_GET_INFO         600
#define SYS_FB_SET_PIXEL        601
#define SYS_FB_FILL_RECT        602
#define SYS_FB_COPY_RECT        603
#define SYS_FB_DRAW_LINE        604
#define SYS_FB_CLEAR_SCREEN     605

/* Socket API syscalls (Issue #46) */
#define SYS_SOCKET              700
#define SYS_BIND                701
#define SYS_LISTEN              702
#define SYS_ACCEPT              703
#define SYS_CONNECT             704
#define SYS_SEND                705
#define SYS_RECV                706
#define SYS_SENDTO              707
#define SYS_RECVFROM            708
#define SYS_SHUTDOWN            709
#define SYS_SETSOCKOPT          710
#define SYS_GETSOCKOPT          711
#define SYS_GETSOCKNAME         712
#define SYS_GETPEERNAME         713

/* Socket error codes */
#define SOCKET_SUCCESS          0
#define SOCKET_ERROR           -1
#define SOCKET_EBADF           -9      /* Bad file descriptor */
#define SOCKET_ENOTSOCK        -88     /* Socket operation on non-socket */
#define SOCKET_EADDRINUSE      -98     /* Address already in use */
#define SOCKET_EADDRNOTAVAIL   -99     /* Cannot assign requested address */
#define SOCKET_ENETDOWN        -100    /* Network is down */
#define SOCKET_ENETUNREACH     -101    /* Network is unreachable */
#define SOCKET_ECONNABORTED    -103    /* Software caused connection abort */
#define SOCKET_ECONNRESET      -104    /* Connection reset by peer */
#define SOCKET_ENOBUFS         -105    /* No buffer space available */
#define SOCKET_EISCONN         -106    /* Transport endpoint is already connected */
#define SOCKET_ENOTCONN        -107    /* Transport endpoint is not connected */
#define SOCKET_ETIMEDOUT       -110    /* Connection timed out */
#define SOCKET_ECONNREFUSED    -111    /* Connection refused */
#define SOCKET_EAGAIN          -11     /* Try again */
#define SOCKET_EINPROGRESS     -115    /* Operation now in progress */

/* System call handler type */
#define SYS_KEYBOARD_READ     140
#define SYS_KEYBOARD_POLL     141
#define SYS_KEYBOARD_IOCTL    142
#define SYS_KEYBOARD_GETCHAR  143

/* IPC syscalls */
#define SYS_IPC_SEND      200
#define SYS_IPC_RECEIVE   201

/* Process manager syscalls */
#define SYS_PS            300
/* Note: SYS_KILL is now defined above as 62 for POSIX compatibility */

/* VFS syscalls */
#define SYS_VFS_MOUNT     400
#define SYS_VFS_UNMOUNT   401

/* Window Manager syscalls (Issue #39) */
#define SYS_WM_REGISTER_APP     500
#define SYS_WM_UNREGISTER_APP   501
#define SYS_WM_CREATE_WINDOW    502
#define SYS_WM_DESTROY_WINDOW   503
#define SYS_WM_SHOW_WINDOW      504
#define SYS_WM_HIDE_WINDOW      505
#define SYS_WM_MOVE_WINDOW      506
#define SYS_WM_RESIZE_WINDOW    507
#define SYS_WM_FOCUS_WINDOW     508
#define SYS_WM_GET_FOCUSED_WINDOW 509
#define SYS_WM_SET_WINDOW_TITLE 510
#define SYS_WM_BRING_TO_FRONT   511
#define SYS_WM_SEND_TO_BACK     512
#define SYS_WM_SET_WINDOW_STATE 513
#define SYS_WM_GET_STATISTICS   514

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

/* User-space execution testing - Issue #14 */
void test_user_space_execution(void);
void init_user_space_execution(void);
void run_user_space_demo(void);

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
