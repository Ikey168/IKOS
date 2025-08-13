/* Simple stubs for testing process manager without full kernel */

#include "vmm.h"
#include "process.h"
#include <stdint.h>
#include <stddef.h>

/* Memory allocation stubs */
void* kmalloc(size_t size) {
    static char fake_heap[64 * 1024]; /* 64KB fake heap */
    static size_t offset = 0;
    
    if (offset + size > sizeof(fake_heap)) {
        return NULL; /* Out of memory */
    }
    
    void* ptr = &fake_heap[offset];
    offset += size;
    return ptr;
}

void kfree(void* ptr) {
    /* Simple stub - in real kernel would free memory */
    (void)ptr;
}

/* VMM stubs */
vm_space_t* vmm_create_address_space(uint32_t pid) {
    static vm_space_t fake_space = {0};
    fake_space.owner_pid = pid;
    return &fake_space;
}

void vmm_destroy_address_space(vm_space_t* space) {
    (void)space;
}

int vmm_map_page(vm_space_t* space, uint64_t vaddr, uint64_t paddr, uint32_t flags) {
    (void)space; (void)vaddr; (void)paddr; (void)flags;
    return 0; /* Success */
}

uint64_t vmm_alloc_page(void) {
    static uint64_t fake_page_counter = 0x100000; /* Start at 1MB */
    return fake_page_counter += 0x1000; /* Return next 4KB page */
}

vm_space_t* vmm_get_current_space(void) {
    static vm_space_t fake_current = {0};
    return &fake_current;
}

int vmm_switch_address_space(vm_space_t* space) {
    (void)space;
    return 0; /* Success */
}

/* Process management stubs */
void process_exit(process_t* proc, int exit_code) {
    if (proc) {
        proc->state = PROCESS_STATE_TERMINATED;
        proc->exit_code = exit_code;
    }
}

int process_init(void) {
    return 0; /* Success */
}

process_t* process_create(const char* name, const char* path) {
    static process_t fake_process = {0};
    static uint32_t fake_pid_counter = 1;
    
    fake_process.pid = fake_pid_counter++;
    fake_process.state = PROCESS_STATE_READY;
    fake_process.priority = PROCESS_PRIORITY_NORMAL;
    
    /* Copy name */
    for (int i = 0; i < MAX_PROCESS_NAME - 1 && name[i]; i++) {
        fake_process.name[i] = name[i];
    }
    
    (void)path; /* Unused */
    return &fake_process;
}

process_t* process_create_from_elf(const char* name, void* elf_data, size_t size) {
    static process_t fake_elf_process = {0};
    static uint32_t fake_elf_pid_counter = 100;
    
    fake_elf_process.pid = fake_elf_pid_counter++;
    fake_elf_process.state = PROCESS_STATE_READY;
    fake_elf_process.priority = PROCESS_PRIORITY_NORMAL;
    
    /* Copy name */
    for (int i = 0; i < MAX_PROCESS_NAME - 1 && name[i]; i++) {
        fake_elf_process.name[i] = name[i];
    }
    
    (void)elf_data; (void)size; /* Unused */
    return &fake_elf_process;
}

process_t* process_get_current(void) {
    static process_t fake_current = {0};
    fake_current.pid = 1;
    fake_current.state = PROCESS_STATE_RUNNING;
    return &fake_current;
}

/* Global variables stubs */
uint64_t total_frames = 1024;
void* frame_database = NULL;
void* kernel_pml4_table = NULL;
