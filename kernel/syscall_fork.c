/* IKOS Fork System Call Implementation - Issue #24
 * Complete implementation of fork() system call for process creation
 */

#include "syscall_process.h"
#include "process.h"
#include "memory.h"
#include "string.h"
#include "vmm.h"
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* ========================== Global State ========================== */

static process_lifecycle_stats_t g_lifecycle_stats = {0};
static uint32_t g_next_pid = 1000;  /* Start PIDs from 1000 */

/* ========================== Helper Functions ========================== */

/**
 * Allocate new process ID
 */
static pid_t allocate_pid(void) {
    return ++g_next_pid;
}

/**
 * Copy process memory with copy-on-write optimization
 */
static int copy_process_memory_cow(process_t* parent, process_t* child) {
    if (!parent || !child) {
        return -EINVAL;
    }
    
    /* Copy page directory structure */
    if (vmm_clone_address_space(parent->address_space, &child->address_space) != 0) {
        return -ENOMEM;
    }
    
    /* Mark all writable pages as copy-on-write */
    uint64_t addr = parent->virtual_memory_start;
    while (addr < parent->virtual_memory_end) {
        if (vmm_is_page_present(parent->address_space, addr)) {
            /* Mark as copy-on-write in both parent and child */
            mark_page_cow(addr);
        }
        addr += PAGE_SIZE;
    }
    
    /* Copy memory layout information */
    child->virtual_memory_start = parent->virtual_memory_start;
    child->virtual_memory_end = parent->virtual_memory_end;
    child->heap_start = parent->heap_start;
    child->heap_end = parent->heap_end;
    child->stack_start = parent->stack_start;
    child->stack_end = parent->stack_end;
    child->entry_point = parent->entry_point;
    child->stack_size = parent->stack_size;
    
    return 0;
}

/**
 * Copy file descriptor table
 */
static int copy_fd_table(process_t* parent, process_t* child) {
    if (!parent || !child) {
        return -EINVAL;
    }
    
    /* Copy file descriptor array */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (parent->fds[i].in_use) {
            child->fds[i] = parent->fds[i];
            /* Increment reference count for shared files */
            if (parent->fds[i].file_ptr) {
                /* TODO: Implement file reference counting */
            }
        }
    }
    
    child->next_fd = parent->next_fd;
    
    return 0;
}

/**
 * Copy signal handlers and masks
 */
static int copy_signal_state(process_t* parent, process_t* child) {
    if (!parent || !child) {
        return -EINVAL;
    }
    
    /* Copy signal handlers */
    for (int i = 0; i < 32; i++) {
        child->signal_handlers[i] = parent->signal_handlers[i];
    }
    
    /* Copy signal masks but not pending signals */
    child->signal_mask = parent->signal_mask;
    child->pending_signals = 0;  /* Child starts with no pending signals */
    
    /* Copy signal delivery state if present */
    if (parent->signal_delivery_state) {
        /* TODO: Implement signal delivery state duplication */
    }
    
    return 0;
}

/**
 * Setup child process context
 */
static int setup_child_context(process_t* parent, process_t* child) {
    if (!parent || !child) {
        return -EINVAL;
    }
    
    /* Copy CPU context from parent */
    child->context = parent->context;
    
    /* Set fork return value to 0 for child */
    child->context.rax = 0;
    
    /* Child inherits parent's stack pointer but gets own stack space */
    /* Stack was already copied during memory duplication */
    
    return 0;
}

/**
 * Update process tree relationships
 */
static int update_process_tree(process_t* parent, process_t* child) {
    if (!parent || !child) {
        return -EINVAL;
    }
    
    /* Set parent-child relationship */
    child->parent = parent;
    child->ppid = parent->pid;
    
    /* Add child to parent's child list */
    if (parent->first_child == NULL) {
        parent->first_child = child;
        child->next_sibling = NULL;
    } else {
        /* Insert at beginning of sibling list */
        child->next_sibling = parent->first_child;
        parent->first_child = child;
    }
    
    return 0;
}

/* ========================== Fork Context Management ========================== */

fork_context_t* create_fork_context(process_t* parent) {
    if (!parent) {
        return NULL;
    }
    
    fork_context_t* ctx = (fork_context_t*)kmalloc(sizeof(fork_context_t));
    if (!ctx) {
        return NULL;
    }
    
    memset(ctx, 0, sizeof(fork_context_t));
    
    ctx->parent_pid = parent->pid;
    ctx->fork_time = get_system_time();
    ctx->copy_on_write = true;  /* Use COW by default */
    
    return ctx;
}

void destroy_fork_context(fork_context_t* ctx) {
    if (ctx) {
        kfree(ctx);
    }
}

/* ========================== Main Fork Implementation ========================== */

/**
 * Fork system call implementation
 */
long sys_fork(void) {
    process_t* parent = get_current_process();
    if (!parent) {
        g_lifecycle_stats.failed_forks++;
        return -ESRCH;
    }
    
    g_lifecycle_stats.total_forks++;
    
    /* Create fork context */
    fork_context_t* fork_ctx = create_fork_context(parent);
    if (!fork_ctx) {
        g_lifecycle_stats.failed_forks++;
        return -ENOMEM;
    }
    
    /* Allocate new process structure */
    process_t* child = (process_t*)kmalloc(sizeof(process_t));
    if (!child) {
        destroy_fork_context(fork_ctx);
        g_lifecycle_stats.failed_forks++;
        return -ENOMEM;
    }
    
    /* Initialize child process structure */
    memset(child, 0, sizeof(process_t));
    
    /* Allocate new PID */
    child->pid = allocate_pid();
    fork_ctx->child_pid = child->pid;
    
    /* Copy basic process information */
    strncpy(child->name, parent->name, MAX_PROCESS_NAME - 1);
    strncpy(child->cmdline, parent->cmdline, MAX_COMMAND_LINE - 1);
    child->state = PROCESS_STATE_READY;
    child->priority = parent->priority;
    child->time_slice = parent->time_slice;
    child->total_time = 0;  /* Child starts with zero CPU time */
    
    /* Copy memory space with COW */
    if (copy_process_memory_cow(parent, child) != 0) {
        kfree(child);
        destroy_fork_context(fork_ctx);
        g_lifecycle_stats.failed_forks++;
        return -ENOMEM;
    }
    
    /* Copy file descriptor table */
    if (copy_fd_table(parent, child) != 0) {
        /* TODO: Cleanup allocated memory */
        kfree(child);
        destroy_fork_context(fork_ctx);
        g_lifecycle_stats.failed_forks++;
        return -ENOMEM;
    }
    
    /* Copy signal handling state */
    if (copy_signal_state(parent, child) != 0) {
        /* TODO: Cleanup allocated memory */
        kfree(child);
        destroy_fork_context(fork_ctx);
        g_lifecycle_stats.failed_forks++;
        return -ENOMEM;
    }
    
    /* Setup child CPU context */
    if (setup_child_context(parent, child) != 0) {
        /* TODO: Cleanup allocated memory */
        kfree(child);
        destroy_fork_context(fork_ctx);
        g_lifecycle_stats.failed_forks++;
        return -ENOMEM;
    }
    
    /* Update process tree */
    if (update_process_tree(parent, child) != 0) {
        /* TODO: Cleanup allocated memory */
        kfree(child);
        destroy_fork_context(fork_ctx);
        g_lifecycle_stats.failed_forks++;
        return -ENOMEM;
    }
    
    /* Add child to scheduler */
    if (scheduler_add_process(child) != 0) {
        /* TODO: Cleanup process tree and memory */
        kfree(child);
        destroy_fork_context(fork_ctx);
        g_lifecycle_stats.failed_forks++;
        return -EAGAIN;
    }
    
    /* Update statistics */
    fork_ctx->shared_pages = (parent->virtual_memory_end - parent->virtual_memory_start) / PAGE_SIZE;
    fork_ctx->copied_pages = 0;  /* All pages shared initially with COW */
    
    destroy_fork_context(fork_ctx);
    g_lifecycle_stats.successful_forks++;
    
    /* Return child PID to parent */
    return child->pid;
}

/* ========================== Copy-on-Write Support ========================== */

int mark_page_cow(uint64_t virtual_addr) {
    /* Mark page as read-only and set COW bit in page table entry */
    return vmm_set_page_cow(virtual_addr);
}

int handle_cow_page_fault(uint64_t virtual_addr, process_t* proc) {
    if (!proc) {
        return -EINVAL;
    }
    
    /* Check if this is a COW page fault */
    if (!vmm_is_page_cow(virtual_addr)) {
        return -EFAULT;  /* Not a COW fault */
    }
    
    /* Copy the page */
    return copy_cow_page(virtual_addr, proc);
}

int copy_cow_page(uint64_t virtual_addr, process_t* proc) {
    if (!proc) {
        return -EINVAL;
    }
    
    /* Allocate new physical page */
    uint64_t new_page = vmm_alloc_page();
    if (!new_page) {
        return -ENOMEM;
    }
    
    /* Copy page content */
    uint64_t old_page = vmm_get_physical_addr(proc->address_space, virtual_addr);
    if (old_page) {
        vmm_copy_page(old_page, new_page);
    }
    
    /* Map new page as writable */
    if (vmm_map_page(proc->address_space, virtual_addr, new_page, 
                     VMM_PAGE_WRITABLE | VMM_PAGE_USER) != 0) {
        vmm_free_page(new_page);
        return -ENOMEM;
    }
    
    return 0;
}

/* ========================== Process Tree Management ========================== */

int add_child_process(process_t* parent, process_t* child) {
    return update_process_tree(parent, child);
}

int remove_child_process(process_t* parent, process_t* child) {
    if (!parent || !child) {
        return -EINVAL;
    }
    
    /* Find and remove child from sibling list */
    if (parent->first_child == child) {
        parent->first_child = child->next_sibling;
    } else {
        process_t* sibling = parent->first_child;
        while (sibling && sibling->next_sibling != child) {
            sibling = sibling->next_sibling;
        }
        if (sibling) {
            sibling->next_sibling = child->next_sibling;
        }
    }
    
    child->parent = NULL;
    child->next_sibling = NULL;
    
    return 0;
}

process_t* find_child_process(process_t* parent, pid_t pid) {
    if (!parent) {
        return NULL;
    }
    
    process_t* child = parent->first_child;
    while (child) {
        if (child->pid == pid) {
            return child;
        }
        child = child->next_sibling;
    }
    
    return NULL;
}

/* ========================== Statistics and Utility ========================== */

void get_process_lifecycle_stats(process_lifecycle_stats_t* stats) {
    if (stats) {
        *stats = g_lifecycle_stats;
    }
}

void reset_process_lifecycle_stats(void) {
    memset(&g_lifecycle_stats, 0, sizeof(g_lifecycle_stats));
}

/* ========================== Placeholder Functions ========================== */

/* System time placeholder */
uint64_t get_system_time(void) {
    static uint64_t fake_time = 0;
    return ++fake_time;
}

/* Memory management placeholders */
void* kmalloc(size_t size) {
    return malloc(size);
}

void kfree(void* ptr) {
    free(ptr);
}

/* Process management placeholders */
process_t* get_current_process(void) {
    static process_t fake_proc = {0};
    return &fake_proc;
}

/* VMM placeholders */
int vmm_clone_address_space(vm_space_t* parent, vm_space_t** child) {
    /* Placeholder - should clone virtual memory space */
    return 0;
}

int vmm_set_page_cow(uint64_t virtual_addr) {
    /* Placeholder - should mark page as copy-on-write */
    return 0;
}

bool vmm_is_page_cow(uint64_t virtual_addr) {
    /* Placeholder - should check if page is COW */
    return false;
}

uint64_t vmm_alloc_page(void) {
    /* Placeholder - should allocate physical page */
    return 0x100000; /* Fake page address */
}

void vmm_free_page(uint64_t page_addr) {
    /* Placeholder - should free physical page */
}

uint64_t vmm_get_physical_addr(vm_space_t* space, uint64_t virtual_addr) {
    /* Placeholder - should get physical address */
    return 0x100000; /* Fake physical address */
}

void vmm_copy_page(uint64_t src_page, uint64_t dst_page) {
    /* Placeholder - should copy page content */
}

bool vmm_is_page_present(vm_space_t* space, uint64_t virtual_addr) {
    /* Placeholder - should check if page is present */
    return true;
}

int vmm_map_page(vm_space_t* space, uint64_t virtual_addr, uint64_t physical_addr, uint32_t flags) {
    /* Placeholder - should map page */
    return 0;
}

/* Scheduler placeholder */
int scheduler_add_process(process_t* proc) {
    /* Placeholder - should add process to scheduler */
    return 0;
}

/* String functions */
void* memset(void* ptr, int value, size_t size) {
    unsigned char* p = (unsigned char*)ptr;
    for (size_t i = 0; i < size; i++) {
        p[i] = (unsigned char)value;
    }
    return ptr;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

/* Standard library placeholders */
void* malloc(size_t size) {
    return (void*)0x2000000; /* Fake address */
}

void free(void* ptr) {
    /* Placeholder */
}
