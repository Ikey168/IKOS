/* IKOS Process Lifecycle System Calls - Issue #24
 * Complete implementation of fork(), execve(), and wait() system calls
 */

#ifndef SYSCALL_PROCESS_H
#define SYSCALL_PROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "process.h"

/* ========================== System Call Numbers ========================== */

#define SYSCALL_FORK        2   /* fork() - Create child process */
#define SYSCALL_EXECVE      3   /* execve() - Execute program */
#define SYSCALL_WAIT        4   /* wait() - Wait for child */
#define SYSCALL_WAITPID     5   /* waitpid() - Wait for specific child */

/* ========================== Wait Options ========================== */

#define WNOHANG     0x00000001  /* Don't block if no child available */
#define WUNTRACED   0x00000002  /* Report stopped children */
#define WCONTINUED  0x00000008  /* Report continued children */

/* ========================== Wait Status Macros ========================== */

/* Extract exit status from wait status */
#define WEXITSTATUS(status)     (((status) & 0xff00) >> 8)

/* Check if child exited normally */
#define WIFEXITED(status)       (((status) & 0x7f) == 0)

/* Check if child was terminated by signal */
#define WIFSIGNALED(status)     (((signed char)(((status) & 0x7f) + 1) >> 1) > 0)

/* Extract terminating signal */
#define WTERMSIG(status)        ((status) & 0x7f)

/* Check if child was stopped */
#define WIFSTOPPED(status)      (((status) & 0xff) == 0x7f)

/* Extract stop signal */
#define WSTOPSIG(status)        WEXITSTATUS(status)

/* Check if child was continued */
#define WIFCONTINUED(status)    ((status) == 0xffff)

/* ========================== Error Codes ========================== */

#define EAGAIN      11      /* Resource temporarily unavailable */
#define ENOMEM      12      /* Out of memory */
#define EACCES      13      /* Permission denied */
#define EFAULT      14      /* Bad address */
#define ENOTDIR     20      /* Not a directory */
#define EINVAL      22      /* Invalid argument */
#define ENFILE      23      /* File table overflow */
#define EMFILE      24      /* Too many open files */
#define ENOEXEC     8       /* Exec format error */
#define E2BIG       7       /* Argument list too long */
#define ECHILD      10      /* No child processes */

/* ========================== Process Lifecycle State ========================== */

typedef enum {
    PROC_LIFECYCLE_CREATED,     /* Process just created */
    PROC_LIFECYCLE_RUNNING,     /* Process running normally */
    PROC_LIFECYCLE_WAITING,     /* Process waiting for child */
    PROC_LIFECYCLE_ZOMBIE,      /* Process terminated, not reaped */
    PROC_LIFECYCLE_TERMINATED   /* Process fully terminated */
} process_lifecycle_state_t;

/* ========================== Fork Context ========================== */

typedef struct {
    pid_t parent_pid;           /* Parent process PID */
    pid_t child_pid;            /* Child process PID */
    uint64_t fork_time;         /* Time of fork operation */
    uint32_t fork_flags;        /* Fork operation flags */
    bool copy_on_write;         /* Use copy-on-write for memory */
    uint32_t shared_pages;      /* Number of shared memory pages */
    uint32_t copied_pages;      /* Number of copied memory pages */
} fork_context_t;

/* ========================== Exec Context ========================== */

typedef struct {
    char path[256];             /* Executable path */
    char** argv;                /* Argument vector */
    char** envp;                /* Environment vector */
    uint32_t argc;              /* Argument count */
    uint32_t envc;              /* Environment count */
    size_t args_size;           /* Total size of arguments */
    size_t env_size;            /* Total size of environment */
    uint64_t entry_point;       /* Program entry point */
    uint64_t stack_base;        /* Stack base address */
    uint64_t heap_base;         /* Heap base address */
} exec_context_t;

/* ========================== Wait Context ========================== */

typedef struct {
    pid_t wait_pid;             /* PID to wait for (-1 for any) */
    int* status_ptr;            /* Where to store exit status */
    int options;                /* Wait options (WNOHANG, etc.) */
    uint64_t wait_start_time;   /* When wait started */
    bool is_blocking;           /* True if blocking wait */
    process_t* waiting_process; /* Process doing the waiting */
} wait_context_t;

/* ========================== Process Lifecycle Statistics ========================== */

typedef struct {
    uint64_t total_forks;       /* Total fork operations */
    uint64_t successful_forks;  /* Successful forks */
    uint64_t failed_forks;      /* Failed forks */
    uint64_t total_execs;       /* Total exec operations */
    uint64_t successful_execs;  /* Successful execs */
    uint64_t failed_execs;      /* Failed execs */
    uint64_t total_waits;       /* Total wait operations */
    uint64_t successful_waits;  /* Successful waits */
    uint64_t failed_waits;      /* Failed waits */
    uint64_t zombies_created;   /* Zombie processes created */
    uint64_t zombies_reaped;    /* Zombie processes reaped */
    uint64_t orphans_adopted;   /* Orphaned processes adopted */
} process_lifecycle_stats_t;

/* ========================== System Call Prototypes ========================== */

/**
 * Fork system call - Create child process
 * Creates an exact copy of the calling process
 * 
 * @return long Child PID in parent, 0 in child, -1 on error
 */
long sys_fork(void);

/**
 * Execve system call - Execute program
 * Replace current process image with new program
 * 
 * @param path Path to executable file
 * @param argv Argument vector (NULL-terminated)
 * @param envp Environment vector (NULL-terminated) 
 * @return long Does not return on success, -1 on error
 */
long sys_execve(const char* path, char* const argv[], char* const envp[]);

/**
 * Wait system call - Wait for child termination
 * Block until any child process terminates
 * 
 * @param status Pointer to store exit status (can be NULL)
 * @return long PID of terminated child, -1 on error
 */
long sys_wait(int* status);

/**
 * Waitpid system call - Wait for specific child
 * Wait for specific child process or any child with options
 * 
 * @param pid Process ID to wait for (-1 for any child)
 * @param status Pointer to store exit status (can be NULL)
 * @param options Wait options (WNOHANG, WUNTRACED, etc.)
 * @return long PID of child, 0 if WNOHANG and no child, -1 on error
 */
long sys_waitpid(pid_t pid, int* status, int options);

/* ========================== Internal Helper Functions ========================== */

/**
 * Initialize process lifecycle system
 */
int process_lifecycle_init(void);

/**
 * Shutdown process lifecycle system  
 */
void process_lifecycle_shutdown(void);

/**
 * Create fork context for process duplication
 */
fork_context_t* create_fork_context(process_t* parent);

/**
 * Destroy fork context
 */
void destroy_fork_context(fork_context_t* ctx);

/**
 * Create exec context for program execution
 */
exec_context_t* create_exec_context(const char* path, char* const argv[], char* const envp[]);

/**
 * Destroy exec context
 */
void destroy_exec_context(exec_context_t* ctx);

/**
 * Create wait context for child waiting
 */
wait_context_t* create_wait_context(pid_t pid, int* status, int options);

/**
 * Destroy wait context
 */
void destroy_wait_context(wait_context_t* ctx);

/**
 * Duplicate process memory space (with COW)
 */
int duplicate_process_memory(process_t* parent, process_t* child, bool copy_on_write);

/**
 * Replace process memory space with new program
 */
int replace_process_memory(process_t* proc, exec_context_t* ctx);

/**
 * Duplicate file descriptor table
 */
int duplicate_fd_table(process_t* parent, process_t* child);

/**
 * Handle close-on-exec for file descriptors
 */
int process_close_on_exec(process_t* proc);

/**
 * Add child to parent's child list
 */
int add_child_process(process_t* parent, process_t* child);

/**
 * Remove child from parent's child list
 */
int remove_child_process(process_t* parent, process_t* child);

/**
 * Find child process by PID
 */
process_t* find_child_process(process_t* parent, pid_t pid);

/**
 * Create zombie process entry
 */
int create_zombie_process(process_t* child, int exit_status);

/**
 * Reap zombie process
 */
int reap_zombie_process(process_t* parent, process_t* zombie);

/**
 * Handle orphaned processes
 */
int handle_orphaned_processes(process_t* terminated_parent);

/**
 * Validate executable file
 */
int validate_executable(const char* path);

/**
 * Load ELF binary into memory
 */
int load_elf_binary(process_t* proc, const char* path, uint64_t* entry_point);

/**
 * Setup process arguments and environment
 */
int setup_process_args_env(process_t* proc, char* const argv[], char* const envp[]);

/**
 * Copy arguments to user space
 */
int copy_args_to_user(process_t* proc, char* const argv[], uint64_t* argv_ptr);

/**
 * Copy environment to user space
 */
int copy_env_to_user(process_t* proc, char* const envp[], uint64_t* envp_ptr);

/**
 * Get process lifecycle statistics
 */
void get_process_lifecycle_stats(process_lifecycle_stats_t* stats);

/**
 * Reset process lifecycle statistics
 */
void reset_process_lifecycle_stats(void);

/* ========================== Copy-on-Write Support ========================== */

/**
 * Mark page as copy-on-write
 */
int mark_page_cow(uint64_t virtual_addr);

/**
 * Handle copy-on-write page fault
 */
int handle_cow_page_fault(uint64_t virtual_addr, process_t* proc);

/**
 * Copy page for COW
 */
int copy_cow_page(uint64_t virtual_addr, process_t* proc);

/* ========================== Process Tree Management ========================== */

/**
 * Update process tree after fork
 */
int update_process_tree_fork(process_t* parent, process_t* child);

/**
 * Update process tree after process termination
 */
int update_process_tree_exit(process_t* proc);

/**
 * Find process by PID in process tree
 */
process_t* find_process_by_pid(pid_t pid);

/**
 * Get process children count
 */
uint32_t get_process_children_count(process_t* proc);

/**
 * Check if process has zombie children
 */
bool has_zombie_children(process_t* proc);

/**
 * Get next zombie child
 */
process_t* get_next_zombie_child(process_t* parent);

#endif /* SYSCALL_PROCESS_H */
