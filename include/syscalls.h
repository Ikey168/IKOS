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

/* Threading and concurrency syscalls (Issue #52) */
#define SYS_THREAD_CREATE       720
#define SYS_THREAD_EXIT         721
#define SYS_THREAD_JOIN         722
#define SYS_THREAD_DETACH       723
#define SYS_THREAD_SELF         724
#define SYS_THREAD_YIELD        725
#define SYS_THREAD_SLEEP        726
#define SYS_THREAD_CANCEL       727
#define SYS_THREAD_KILL         728
#define SYS_THREAD_SETNAME      729

#define SYS_MUTEX_INIT          730
#define SYS_MUTEX_DESTROY       731
#define SYS_MUTEX_LOCK          732
#define SYS_MUTEX_TRYLOCK       733
#define SYS_MUTEX_UNLOCK        734
#define SYS_MUTEX_TIMEDLOCK     735

#define SYS_COND_INIT           740
#define SYS_COND_DESTROY        741
#define SYS_COND_WAIT           742
#define SYS_COND_TIMEDWAIT      743
#define SYS_COND_SIGNAL         744
#define SYS_COND_BROADCAST      745

#define SYS_SEM_INIT            750
#define SYS_SEM_DESTROY         751
#define SYS_SEM_WAIT            752
#define SYS_SEM_TRYWAIT         753
#define SYS_SEM_POST            754
#define SYS_SEM_GETVALUE        755
#define SYS_SEM_TIMEDWAIT       756

#define SYS_RWLOCK_INIT         760
#define SYS_RWLOCK_DESTROY      761
#define SYS_RWLOCK_RDLOCK       762
#define SYS_RWLOCK_WRLOCK       763
#define SYS_RWLOCK_UNLOCK       764
#define SYS_RWLOCK_TRYRDLOCK    765
#define SYS_RWLOCK_TRYWRLOCK    766

#define SYS_BARRIER_INIT        770
#define SYS_BARRIER_DESTROY     771
#define SYS_BARRIER_WAIT        772

#define SYS_SPINLOCK_INIT       780
#define SYS_SPINLOCK_DESTROY    781
#define SYS_SPINLOCK_LOCK       782
#define SYS_SPINLOCK_TRYLOCK    783
#define SYS_SPINLOCK_UNLOCK     784

#define SYS_TLS_CREATE_KEY      790
#define SYS_TLS_DELETE_KEY      791
#define SYS_TLS_GET_VALUE       792
#define SYS_TLS_SET_VALUE       793

#define SYS_THREAD_STATS        800
#define SYS_THREAD_LIST         801
#define SYS_THREAD_INFO         802

/* DNS Resolution Service syscalls (Issue #47) */
#define SYS_DNS_RESOLVE_HOSTNAME    810
#define SYS_DNS_RESOLVE_IP          811
#define SYS_DNS_SET_SERVERS         812
#define SYS_DNS_GET_SERVERS         813
#define SYS_DNS_CONFIGURE           814
#define SYS_DNS_GET_CONFIG          815
#define SYS_DNS_CACHE_LOOKUP        816
#define SYS_DNS_CACHE_ADD           817
#define SYS_DNS_CACHE_REMOVE        818
#define SYS_DNS_CACHE_FLUSH         819
#define SYS_DNS_GET_STATS           820
#define SYS_DNS_RESET_STATS         821

/* TLS/SSL Secure Communication syscalls (Issue #48) */
#define SYS_TLS_INIT                830
#define SYS_TLS_CLEANUP             831
#define SYS_TLS_CLIENT_CONNECT      832
#define SYS_TLS_SERVER_CREATE       833
#define SYS_TLS_SERVER_ACCEPT       834
#define SYS_TLS_SEND                835
#define SYS_TLS_RECV                836
#define SYS_TLS_CLOSE               837
#define SYS_TLS_SHUTDOWN            838
#define SYS_TLS_HANDSHAKE           839
#define SYS_TLS_SET_CONFIG          840
#define SYS_TLS_GET_CONFIG          841
#define SYS_TLS_GET_CONNECTION_INFO 842
#define SYS_TLS_GET_PEER_CERT_INFO  843
#define SYS_TLS_VERIFY_CERTIFICATE  844
#define SYS_TLS_SET_CERTIFICATE     845
#define SYS_TLS_ADD_CA_CERT         846
#define SYS_TLS_SESSION_SAVE        847
#define SYS_TLS_SESSION_RESUME      848
#define SYS_TLS_GET_STATISTICS      849
#define SYS_TLS_RESET_STATISTICS    850

/* Threading error codes */
#define THREAD_SUCCESS          0
#define THREAD_ERROR           -1
#define THREAD_EAGAIN          -11
#define THREAD_EINVAL          -22

/* DNS error codes */
#define DNS_SUCCESS             0
#define DNS_ERROR              -1
#define DNS_INVALID_HOSTNAME   -2
#define DNS_INVALID_IP         -3
#define DNS_TIMEOUT            -4
#define DNS_NO_SERVER          -5
#define DNS_CACHE_MISS         -6
#define DNS_BUFFER_TOO_SMALL   -7

/* TLS error codes */
#define TLS_SUCCESS                     0
#define TLS_ERROR                      -1
#define TLS_INVALID_PARAMETER          -2
#define TLS_OUT_OF_MEMORY              -3
#define TLS_SOCKET_ERROR               -4
#define TLS_HANDSHAKE_FAILED           -5
#define TLS_CERTIFICATE_ERROR          -6
#define TLS_TIMEOUT                    -7
#define TLS_CONNECTION_CLOSED          -8
#define TLS_BUFFER_TOO_SMALL           -9
#define TLS_NOT_INITIALIZED           -10
#define THREAD_EPERM           -1
#define THREAD_ESRCH           -3
#define THREAD_EDEADLK         -35
#define THREAD_ENOMEM          -12
#define THREAD_EBUSY           -16
#define THREAD_ETIMEDOUT       -110
#define THREAD_ENOTSUP         -95

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
