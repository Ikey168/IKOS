/* IKOS Process Management Header
 * Defines user-space process execution and management
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "vmm.h"
#include "interrupts.h"

/* Define ssize_t if not available */
#ifndef _SSIZE_T_DEFINED
typedef long long ssize_t;
#define _SSIZE_T_DEFINED
#endif

/* Process states */
typedef enum {
    PROCESS_STATE_READY,        /* Ready to run */
    PROCESS_STATE_RUNNING,      /* Currently running */
    PROCESS_STATE_BLOCKED,      /* Waiting for I/O or event */
    PROCESS_STATE_ZOMBIE,       /* Terminated but not cleaned up */
    PROCESS_STATE_TERMINATED    /* Fully terminated */
} process_state_t;

/* Process priorities */
typedef enum {
    PROCESS_PRIORITY_IDLE = 0,      /* Lowest priority */
    PROCESS_PRIORITY_LOW = 1,       /* Low priority */
    PROCESS_PRIORITY_NORMAL = 2,    /* Normal priority */
    PROCESS_PRIORITY_HIGH = 3,      /* High priority */
    PROCESS_PRIORITY_REALTIME = 4   /* Highest priority */
} process_priority_t;

/* User-space memory layout constants */
#define USER_SPACE_START        0x400000        /* 4MB - Start of user space */
#define USER_SPACE_END          0x800000000UL   /* 32GB - End of user space */
#define USER_STACK_SIZE         0x200000        /* 2MB default stack size */
#define USER_HEAP_START         0x600000        /* 6MB - Start of user heap */
#define USER_CODE_LOAD_ADDR     0x400000        /* 4MB - Default code load address */

/* Process limits */
#define MAX_PROCESSES           256             /* Maximum number of processes */
#define MAX_OPEN_FILES          64              /* Maximum open files per process */
#define MAX_PROCESS_NAME        32              /* Maximum process name length */
#define MAX_COMMAND_LINE        256             /* Maximum command line length */

/* File descriptor structure */
typedef struct {
    int fd;                     /* File descriptor number */
    uint64_t offset;            /* Current file offset */
    uint32_t flags;             /* File flags (read/write/etc) */
    void* file_data;            /* Pointer to file data structure */
} file_descriptor_t;

/* Process context structure - saved during context switches */
typedef struct {
    /* General purpose registers */
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    
    /* Control registers */
    uint64_t rip;               /* Instruction pointer */
    uint64_t rflags;            /* CPU flags */
    uint64_t cr3;               /* Page directory base */
    
    /* Segment registers */
    uint16_t cs, ds, es, fs, gs, ss;
    
    /* FPU/SSE state would go here if needed */
} process_context_t;

/* Process Control Block (PCB) */
typedef struct process {
    /* Process identification */
    uint32_t pid;                       /* Process ID */
    uint32_t ppid;                      /* Parent process ID */
    char name[MAX_PROCESS_NAME];        /* Process name */
    char cmdline[MAX_COMMAND_LINE];     /* Command line */
    
    /* Process state */
    process_state_t state;              /* Current state */
    process_priority_t priority;        /* Process priority */
    uint64_t time_slice;                /* Remaining time slice */
    uint64_t total_time;                /* Total CPU time used */
    
    /* Memory management */
    vm_space_t* address_space;          /* Address space pointer */
    uint64_t virtual_memory_start;      /* Start of virtual memory */
    uint64_t virtual_memory_end;        /* End of virtual memory */
    uint64_t heap_start;                /* Start of heap */
    uint64_t heap_end;                  /* End of heap */
    uint64_t stack_start;               /* Start of stack */
    uint64_t stack_end;                 /* End of stack */
    uint64_t entry_point;               /* Process entry point address */
    uint64_t stack_size;                /* Size of the stack */
    
    /* Context */
    process_context_t context;          /* Saved CPU context */
    
    /* File descriptors */
    file_descriptor_t fds[MAX_OPEN_FILES];
    int next_fd;                        /* Next available file descriptor */
    
    /* Process tree */
    struct process* parent;             /* Parent process */
    struct process* first_child;        /* First child process */
    struct process* next_sibling;       /* Next sibling process */
    
    /* Scheduling */
    struct process* next;               /* Next process in ready queue */
    struct process* prev;               /* Previous process in ready queue */
    
    /* Exit information */
    int exit_code;                      /* Exit code when terminated */
} process_t;

/* Process management functions */
int process_init(void);
process_t* process_create(const char* name, const char* path);
process_t* process_create_from_elf(const char* name, void* elf_data, size_t size);
int process_exec(process_t* proc, const char* path, char* const argv[]);
void process_exit(process_t* proc, int exit_code);
void process_kill(process_t* proc, int signal);
process_t* process_get_by_pid(uint32_t pid);
process_t* process_get_current(void);

/* Memory management for processes */
int process_setup_memory(process_t* proc);
int process_map_memory(process_t* proc, uint64_t vaddr, uint64_t paddr, size_t size, uint32_t flags);
int process_unmap_memory(process_t* proc, uint64_t vaddr, size_t size);
void* process_allocate_pages(process_t* proc, size_t count);
void process_free_pages(process_t* proc, void* addr, size_t count);

/* Context switching */
void process_switch_to(process_t* proc);
void process_save_context(process_t* proc, interrupt_frame_t* frame);
void process_restore_context(process_t* proc, interrupt_frame_t* frame);

/* File descriptor management */
int process_open_file(process_t* proc, const char* path, int flags);
int process_close_file(process_t* proc, int fd);
ssize_t process_read_file(process_t* proc, int fd, void* buffer, size_t count);
ssize_t process_write_file(process_t* proc, int fd, const void* buffer, size_t count);

/* Process list management */
void process_add_to_ready_queue(process_t* proc);
void process_remove_from_ready_queue(process_t* proc);
process_t* process_get_next_ready(void);

/* System call interface */
int sys_fork(void);
int sys_exec(const char* path, char* const argv[]);
void sys_exit(int status);
int sys_wait(int* status);
int sys_getpid(void);
int sys_getppid(void);

/* Process statistics */
typedef struct {
    uint32_t total_processes;           /* Total processes created */
    uint32_t active_processes;          /* Currently active processes */
    uint32_t zombie_processes;          /* Zombie processes */
    uint64_t context_switches;          /* Total context switches */
    uint64_t page_faults;               /* Total page faults */
} process_stats_t;

void process_get_stats(process_stats_t* stats);

/* Process debugging */
void process_dump_info(process_t* proc);
void process_dump_all(void);

/* Process lookup and management functions */
process_t* process_find_by_pid(uint32_t pid);
process_t* process_get_by_pid(uint32_t pid);  /* Alias for compatibility */
void process_terminate(process_t* proc);

/* External variables */
extern process_t* current_process;
/* Note: processes array is defined as static in process.c */

/* Assembly routine declarations (implemented in user_mode.asm) */
extern void switch_to_user_mode_asm(process_context_t* context);

#endif /* PROCESS_H */
