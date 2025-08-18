/* IKOS System Call Implementation
 * Handles system calls from user-space processes including process lifecycle
 */

#include "process.h"
#include "interrupts.h"
#include "syscall_process.h"
#include "window_manager_syscalls.h"
#include <stdint.h>

/* Function declarations */
static void debug_print(const char* format, ...);
int syscall_init(void);

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
#define SYS_WAITPID     247

/* Window Manager syscalls */
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

/* Forward declarations */
static long sys_exit_impl(int status);
static long sys_write_impl(int fd, const void* buffer, size_t count);
static long sys_read_impl(int fd, void* buffer, size_t count);
static long sys_getpid_impl(void);
static long sys_getppid_impl(void);
static void debug_print(const char* format, ...);

/**
 * Main system call handler
 * Called from assembly when user mode issues a system call
 */
long handle_system_call(interrupt_frame_t* frame) {
    if (!frame) {
        return -1;
    }
    
    /* System call number is in RAX */
    uint64_t syscall_num = frame->rax;
    long result = -1;
    
    /* Get current process */
    process_t* current = process_get_current();
    if (!current) {
        debug_print("System call from unknown process\n");
        return -1;
    }
    
    debug_print("System call %lu from process %s (PID %d)\n", 
               syscall_num, current->name, current->pid);
    
    /* Dispatch system call */
    switch (syscall_num) {
        case SYS_EXIT:
            result = sys_exit_impl((int)frame->rdi);
            break;
            
        case SYS_WRITE:
            result = sys_write_impl((int)frame->rdi, 
                                   (const void*)frame->rsi, 
                                   (size_t)frame->rdx);
            break;
            
        case SYS_READ:
            result = sys_read_impl((int)frame->rdi,
                                  (void*)frame->rsi,
                                  (size_t)frame->rdx);
            break;
            
        case SYS_GETPID:
            result = sys_getpid_impl();
            break;
            
        case SYS_GETPPID:
            result = sys_getppid_impl();
            break;
            
        case SYS_FORK:
            /* Implement fork system call */
            result = sys_fork();
            break;
            
        case SYS_EXECVE:
            /* Implement execve system call */
            {
                char* path = (char*)frame->rdi;
                char** argv = (char**)frame->rsi;
                char** envp = (char**)frame->rdx;
                result = sys_execve(path, argv, envp);
            }
            break;
            
        case SYS_WAIT:
            /* Implement wait system call */
            {
                int* status = (int*)frame->rdi;
                result = sys_wait(status);
            }
            break;
            
        case SYS_WAITPID:
            /* Implement waitpid system call */
            {
                pid_t pid = (pid_t)frame->rdi;
                int* status = (int*)frame->rsi;
                int options = (int)frame->rdx;
                result = sys_waitpid(pid, status, options);
            }
            break;
            
        /* Window Manager System Calls */
        case SYS_WM_REGISTER_APP:
            result = sys_wm_register_app((const char*)frame->rdi);
            break;
            
        case SYS_WM_UNREGISTER_APP:
            result = sys_wm_unregister_app();
            break;
            
        case SYS_WM_CREATE_WINDOW:
            result = sys_wm_create_window((wm_create_params_t*)frame->rdi);
            break;
            
        case SYS_WM_DESTROY_WINDOW:
            result = sys_wm_destroy_window((uint32_t)frame->rdi);
            break;
            
        case SYS_WM_SHOW_WINDOW:
            result = sys_wm_show_window((uint32_t)frame->rdi);
            break;
            
        case SYS_WM_HIDE_WINDOW:
            result = sys_wm_hide_window((uint32_t)frame->rdi);
            break;
            
        case SYS_WM_MOVE_WINDOW:
            result = sys_wm_move_window((uint32_t)frame->rdi, (int32_t)frame->rsi, (int32_t)frame->rdx);
            break;
            
        case SYS_WM_RESIZE_WINDOW:
            result = sys_wm_resize_window((uint32_t)frame->rdi, (uint32_t)frame->rsi, (uint32_t)frame->rdx);
            break;
            
        case SYS_WM_FOCUS_WINDOW:
            result = sys_wm_focus_window((uint32_t)frame->rdi);
            break;
            
        case SYS_WM_GET_FOCUSED_WINDOW:
            result = sys_wm_get_focused_window();
            break;
            
        case SYS_WM_SET_WINDOW_TITLE:
            result = sys_wm_set_window_title((uint32_t)frame->rdi, (const char*)frame->rsi);
            break;
            
        case SYS_WM_BRING_TO_FRONT:
            result = sys_wm_bring_to_front((uint32_t)frame->rdi);
            break;
            
        case SYS_WM_SEND_TO_BACK:
            result = sys_wm_send_to_back((uint32_t)frame->rdi);
            break;
            
        case SYS_WM_SET_WINDOW_STATE:
            result = sys_wm_set_window_state((uint32_t)frame->rdi, (wm_window_state_t)frame->rsi);
            break;
            
        case SYS_WM_GET_STATISTICS:
            result = sys_wm_get_statistics((wm_statistics_t*)frame->rdi);
            break;
            
        default:
            debug_print("Unknown system call: %lu\n", syscall_num);
            result = -1;
            break;
    }
    
    /* Return result in RAX */
    frame->rax = (uint64_t)result;
    return result;
}

/**
 * sys_exit - Terminate the current process
 */
static long sys_exit_impl(int status) {
    process_t* current = process_get_current();
    if (!current) {
        return -1;
    }
    
    debug_print("Process %s (PID %d) exiting with status %d\n",
               current->name, current->pid, status);
    
    /* Set exit code */
    current->exit_code = status;
    current->state = PROCESS_STATE_ZOMBIE;
    
    /* TODO: Clean up process resources */
    /* TODO: Notify parent process */
    /* TODO: Schedule next process */
    
    /* This should not return */
    while (1) {
        __asm__ volatile ("hlt");
    }
    
    return 0;
}

/**
 * sys_write - Write data to a file descriptor
 */
static long sys_write_impl(int fd, const void* buffer, size_t count) {
    process_t* current = process_get_current();
    if (!current || !buffer) {
        return -1;
    }
    
    /* Handle stdout/stderr for now */
    if (fd == 1 || fd == 2) {
        /* Write to console/debug output */
        const char* str = (const char*)buffer;
        
        debug_print("Process %d writes: ", current->pid);
        for (size_t i = 0; i < count; i++) {
            debug_print("%c", str[i]);
        }
        
        return (long)count;
    }
    
    /* TODO: Implement file writing */
    debug_print("File writing not yet implemented\n");
    return -1;
}

/**
 * sys_read - Read data from a file descriptor
 */
static long sys_read_impl(int fd, void* buffer, size_t count) {
    process_t* current = process_get_current();
    if (!current || !buffer) {
        return -1;
    }
    
    /* Handle stdin for now */
    if (fd == 0) {
        /* TODO: Implement reading from keyboard/console */
        debug_print("Console reading not yet implemented\n");
        return 0;
    }
    
    /* TODO: Implement file reading */
    debug_print("File reading not yet implemented\n");
    return -1;
}

/**
 * sys_getpid - Get process ID
 */
static long sys_getpid_impl(void) {
    process_t* current = process_get_current();
    if (!current) {
        return -1;
    }
    
    return (long)current->pid;
}

/**
 * sys_getppid - Get parent process ID
 */
static long sys_getppid_impl(void) {
    process_t* current = process_get_current();
    if (!current) {
        return -1;
    }
    
    return (long)current->ppid;
}

/**
 * Initialize system call handling
 */
int syscall_init(void) {
    /* TODO: Set up system call interrupt handler */
    /* This would involve setting up interrupt 0x80 or using SYSCALL instruction */
    
    debug_print("System call handling initialized\n");
    return 0;
}

/**
 * Validate user-space pointer
 * Ensures the pointer is within user space and mapped
 */
int validate_user_pointer(const void* ptr, size_t size) {
    if (!ptr) {
        return -1;
    }
    
    uint64_t addr = (uint64_t)ptr;
    uint64_t end_addr = addr + size;
    
    /* Check if address is in user space */
    if (addr < USER_SPACE_START || end_addr > USER_SPACE_END) {
        debug_print("Pointer 0x%lX outside user space\n", addr);
        return -1;
    }
    
    /* TODO: Check if memory is actually mapped and accessible */
    /* This would involve checking the page tables */
    
    return 0;
}

/**
 * Copy data from user space to kernel space safely
 */
int copy_from_user(void* kernel_ptr, const void* user_ptr, size_t size) {
    if (!kernel_ptr || !user_ptr) {
        return -1;
    }
    
    /* Validate user pointer */
    if (validate_user_pointer(user_ptr, size) != 0) {
        return -1;
    }
    
    /* Simple memory copy for now */
    /* TODO: Add proper page fault handling */
    const char* src = (const char*)user_ptr;
    char* dst = (char*)kernel_ptr;
    
    for (size_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }
    
    return 0;
}

/**
 * Copy data from kernel space to user space safely
 */
int copy_to_user(void* user_ptr, const void* kernel_ptr, size_t size) {
    if (!user_ptr || !kernel_ptr) {
        return -1;
    }
    
    /* Validate user pointer */
    if (validate_user_pointer(user_ptr, size) != 0) {
        return -1;
    }
    
    /* Simple memory copy for now */
    /* TODO: Add proper page fault handling */
    const char* src = (const char*)kernel_ptr;
    char* dst = (char*)user_ptr;
    
    for (size_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }
    
    return 0;
}

/**
 * Simple debug print - to be replaced with proper implementation
 */
static void debug_print(const char* format, ...) {
    /* TODO: Implement proper debug printing */
    (void)format; /* Suppress unused parameter warning */
}
