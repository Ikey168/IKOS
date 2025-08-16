/* IKOS Runtime Kernel Debugger Implementation - Issue #16 Enhancement
 * Real-time debugging interface with interactive capabilities
 * This complements the existing kernel_log.h system
 */

#include "../include/kernel_debug.h"

/* Try to include existing logging system if available */
#ifdef HAVE_KERNEL_LOG
#include "../include/kernel_log.h"
#else
/* Fallback logging macros if kernel_log.h not available */
#define KLOG_INFO(cat, ...) do { } while(0)
#define KLOG_DEBUG(cat, ...) do { } while(0)
#define KLOG_ERROR(cat, ...) do { } while(0)
#define KLOG_PANIC(cat, ...) do { } while(0)
#define LOG_CAT_KERNEL 0
#define LOG_CAT_MEMORY 1
#define LOG_CAT_PROC 2
#define LOG_CAT_DEVICE 3
#define LOG_CAT_IPC 4
#endif

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Basic implementations for standalone compilation */
#ifndef HAVE_FULL_KERNEL
int snprintf(char *str, size_t size, const char *format, ...) {
    /* Simplified implementation */
    (void)str; (void)size; (void)format;
    return 0;
}

char* strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

void* memcpy(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}
#endif

/* ================================ Global State ================================ */

static kdebug_state_t debug_state = KDEBUG_STATE_DISABLED;
kdebug_breakpoint_t breakpoints[KDEBUG_MAX_BREAKPOINTS];     /* Non-static for external access */
kdebug_breakpoint_t watchpoints[KDEBUG_MAX_WATCHPOINTS];     /* Non-static for external access */
static kdebug_stats_t debug_stats = {0};
static bool debug_initialized = false;

/* Command buffer for interactive console */
static char command_buffer[KDEBUG_CMD_BUFFER_SIZE];
static int command_length = 0;

/* ================================ Core Interface ================================ */

bool kdebug_init(void) {
    if (debug_initialized) {
        return true;
    }
    
    /* Clear all breakpoints and watchpoints */
    memset(breakpoints, 0, sizeof(breakpoints));
    memset(watchpoints, 0, sizeof(watchpoints));
    memset(&debug_stats, 0, sizeof(debug_stats));
    
    /* Clear command buffer */
    memset(command_buffer, 0, sizeof(command_buffer));
    command_length = 0;
    
    debug_state = KDEBUG_STATE_DISABLED;
    debug_initialized = true;
    
    KLOG_INFO(LOG_CAT_KERNEL, "Runtime kernel debugger initialized");
    return true;
}

void kdebug_set_enabled(bool enabled) {
    if (!debug_initialized) {
        kdebug_init();
    }
    
    debug_state = enabled ? KDEBUG_STATE_ENABLED : KDEBUG_STATE_DISABLED;
    KLOG_INFO(LOG_CAT_KERNEL, "Kernel debugger %s", enabled ? "enabled" : "disabled");
}

bool kdebug_is_enabled(void) {
    return debug_state != KDEBUG_STATE_DISABLED;
}

kdebug_state_t kdebug_get_state(void) {
    return debug_state;
}

/* ================================ Breakpoint Management ================================ */

int kdebug_set_breakpoint(uint64_t address, const char* description) {
    if (!kdebug_is_enabled()) {
        return -1;
    }
    
    /* Find free breakpoint slot */
    for (int i = 0; i < KDEBUG_MAX_BREAKPOINTS; i++) {
        if (!breakpoints[i].active) {
            breakpoints[i].active = true;
            breakpoints[i].type = KDEBUG_BP_EXECUTION;
            breakpoints[i].address = address;
            breakpoints[i].length = 1;
            breakpoints[i].hit_count = 0;
            
            if (description) {
                strncpy(breakpoints[i].description, description, 63);
                breakpoints[i].description[63] = '\0';
            } else {
                snprintf(breakpoints[i].description, 64, "Breakpoint at 0x%lx", address);
            }
            
            KLOG_DEBUG(LOG_CAT_KERNEL, "Set breakpoint %d at 0x%lx: %s", 
                      i, address, breakpoints[i].description);
            return i;
        }
    }
    
    KLOG_ERROR(LOG_CAT_KERNEL, "No free breakpoint slots available");
    return -1;
}

int kdebug_set_watchpoint(uint64_t address, uint64_t length, 
                         kdebug_breakpoint_type_t type, const char* description) {
    if (!kdebug_is_enabled()) {
        return -1;
    }
    
    /* Find free watchpoint slot */
    for (int i = 0; i < KDEBUG_MAX_WATCHPOINTS; i++) {
        if (!watchpoints[i].active) {
            watchpoints[i].active = true;
            watchpoints[i].type = type;
            watchpoints[i].address = address;
            watchpoints[i].length = length;
            watchpoints[i].hit_count = 0;
            
            if (description) {
                strncpy(watchpoints[i].description, description, 63);
                watchpoints[i].description[63] = '\0';
            } else {
                const char* type_str = (type == KDEBUG_BP_MEMORY_READ) ? "read" :
                                      (type == KDEBUG_BP_MEMORY_WRITE) ? "write" : "access";
                snprintf(watchpoints[i].description, 64, "Watch %s at 0x%lx (%lu bytes)", 
                        type_str, address, length);
            }
            
            KLOG_DEBUG(LOG_CAT_KERNEL, "Set watchpoint %d: %s", i, watchpoints[i].description);
            return i;
        }
    }
    
    KLOG_ERROR(LOG_CAT_KERNEL, "No free watchpoint slots available");
    return -1;
}

bool kdebug_remove_breakpoint(int id) {
    if (id >= 0 && id < KDEBUG_MAX_BREAKPOINTS && breakpoints[id].active) {
        KLOG_DEBUG(LOG_CAT_KERNEL, "Removed breakpoint %d: %s", id, breakpoints[id].description);
        breakpoints[id].active = false;
        return true;
    }
    
    if (id >= 0 && id < KDEBUG_MAX_WATCHPOINTS && watchpoints[id].active) {
        KLOG_DEBUG(LOG_CAT_KERNEL, "Removed watchpoint %d: %s", id, watchpoints[id].description);
        watchpoints[id].active = false;
        return true;
    }
    
    return false;
}

void kdebug_list_breakpoints(void) {
    if (!kdebug_is_enabled()) {
        return;
    }
    
    KLOG_INFO(LOG_CAT_KERNEL, "=== Active Breakpoints ===");
    bool found_any = false;
    
    for (int i = 0; i < KDEBUG_MAX_BREAKPOINTS; i++) {
        if (breakpoints[i].active) {
            KLOG_INFO(LOG_CAT_KERNEL, "BP %d: 0x%016lx (hits: %lu) - %s",
                     i, breakpoints[i].address, breakpoints[i].hit_count,
                     breakpoints[i].description);
            found_any = true;
        }
    }
    
    KLOG_INFO(LOG_CAT_KERNEL, "=== Active Watchpoints ===");
    for (int i = 0; i < KDEBUG_MAX_WATCHPOINTS; i++) {
        if (watchpoints[i].active) {
            const char* type_str = (watchpoints[i].type == KDEBUG_BP_MEMORY_READ) ? "READ" :
                                  (watchpoints[i].type == KDEBUG_BP_MEMORY_WRITE) ? "WRITE" : "ACCESS";
            KLOG_INFO(LOG_CAT_KERNEL, "WP %d: 0x%016lx (%s, %lu bytes, hits: %lu) - %s",
                     i, watchpoints[i].address, type_str, watchpoints[i].length,
                     watchpoints[i].hit_count, watchpoints[i].description);
            found_any = true;
        }
    }
    
    if (!found_any) {
        KLOG_INFO(LOG_CAT_KERNEL, "No active breakpoints or watchpoints");
    }
}

void kdebug_clear_all_breakpoints(void) {
    int cleared_count = 0;
    
    for (int i = 0; i < KDEBUG_MAX_BREAKPOINTS; i++) {
        if (breakpoints[i].active) {
            breakpoints[i].active = false;
            cleared_count++;
        }
    }
    
    for (int i = 0; i < KDEBUG_MAX_WATCHPOINTS; i++) {
        if (watchpoints[i].active) {
            watchpoints[i].active = false;
            cleared_count++;
        }
    }
    
    KLOG_INFO(LOG_CAT_KERNEL, "Cleared %d breakpoints and watchpoints", cleared_count);
}

/* ================================ Memory Debugging ================================ */

void kdebug_memory_dump(uint64_t address, uint64_t length) {
    if (!kdebug_is_enabled()) {
        return;
    }
    
    debug_stats.memory_dumps_performed++;
    
    KLOG_INFO(LOG_CAT_KERNEL, "=== Memory Dump: 0x%016lx (%lu bytes) ===", address, length);
    
    uint8_t* ptr = (uint8_t*)address;
    for (uint64_t offset = 0; offset < length; offset += 16) {
        char hex_line[64] = {0};
        char ascii_line[20] = {0};
        int hex_pos = 0;
        int ascii_pos = 0;
        
        /* Build hex representation */
        for (int i = 0; i < 16 && (offset + i) < length; i++) {
            uint8_t byte = ptr[offset + i];
            hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, "%02x ", byte);
            ascii_line[ascii_pos++] = (byte >= 32 && byte <= 126) ? byte : '.';
        }
        
        /* Pad hex line if needed */
        while (hex_pos < 48) {
            hex_line[hex_pos++] = ' ';
        }
        hex_line[hex_pos] = '\0';
        ascii_line[ascii_pos] = '\0';
        
        KLOG_INFO(LOG_CAT_KERNEL, "%016lx: %s |%s|", address + offset, hex_line, ascii_line);
    }
}

uint64_t kdebug_memory_search(uint64_t start_address, uint64_t end_address,
                             const uint8_t* pattern, uint64_t pattern_length) {
    if (!kdebug_is_enabled() || pattern_length == 0) {
        return 0;
    }
    
    uint8_t* search_ptr = (uint8_t*)start_address;
    uint64_t search_length = end_address - start_address;
    
    for (uint64_t offset = 0; offset <= search_length - pattern_length; offset++) {
        if (memcmp(search_ptr + offset, pattern, pattern_length) == 0) {
            KLOG_INFO(LOG_CAT_KERNEL, "Pattern found at 0x%016lx", start_address + offset);
            return start_address + offset;
        }
    }
    
    KLOG_INFO(LOG_CAT_KERNEL, "Pattern not found in range 0x%016lx - 0x%016lx", 
             start_address, end_address);
    return 0;
}

bool kdebug_memory_read(uint64_t address, void* buffer, uint64_t length) {
    if (!kdebug_is_enabled()) {
        return false;
    }
    
    /* TODO: Add page fault handling for safe memory access */
    /* For now, assume all kernel memory is accessible */
    memcpy(buffer, (void*)address, length);
    return true;
}

bool kdebug_memory_write(uint64_t address, const void* buffer, uint64_t length) {
    if (!kdebug_is_enabled()) {
        return false;
    }
    
    /* TODO: Add page fault handling and write protection checks */
    /* For now, assume all kernel memory is writable */
    memcpy((void*)address, buffer, length);
    return true;
}

/* ================================ Stack Tracing ================================ */

void kdebug_stack_trace(const kdebug_registers_t* registers) {
    if (!kdebug_is_enabled()) {
        return;
    }
    
    debug_stats.stack_traces_generated++;
    
    kdebug_stack_frame_t frames[KDEBUG_STACK_TRACE_DEPTH];
    int frame_count = kdebug_get_stack_frames(frames, KDEBUG_STACK_TRACE_DEPTH, registers);
    
    KLOG_INFO(LOG_CAT_KERNEL, "=== Stack Trace (%d frames) ===", frame_count);
    for (int i = 0; i < frame_count; i++) {
        KLOG_INFO(LOG_CAT_KERNEL, "#%-2d 0x%016lx in %s (rbp=0x%016lx)", 
                 i, frames[i].rip, frames[i].symbol, frames[i].rbp);
    }
}

int kdebug_get_stack_frames(kdebug_stack_frame_t* frames, int max_frames,
                           const kdebug_registers_t* registers) {
    if (!frames || max_frames <= 0) {
        return 0;
    }
    
    uint64_t current_rbp;
    uint64_t current_rip;
    
    if (registers) {
        current_rbp = registers->rbp;
        current_rip = registers->rip;
    } else {
        /* Get current frame pointer */
        __asm__ volatile ("mov %%rbp, %0" : "=r" (current_rbp));
        __asm__ volatile ("lea (%%rip), %0" : "=r" (current_rip));
    }
    
    int frame_count = 0;
    
    /* First frame is current function */
    if (frame_count < max_frames) {
        frames[frame_count].rip = current_rip;
        frames[frame_count].rbp = current_rbp;
        kdebug_lookup_symbol(current_rip, frames[frame_count].symbol, 64);
        frame_count++;
    }
    
    /* Walk the stack */
    while (frame_count < max_frames && current_rbp != 0) {
        /* Each stack frame has structure: [saved_rbp][return_address] */
        uint64_t* frame_ptr = (uint64_t*)current_rbp;
        
        /* Validate frame pointer */
        if ((uint64_t)frame_ptr < 0x1000 || (uint64_t)frame_ptr > 0x7FFFFFFFFFFF) {
            break;
        }
        
        uint64_t saved_rbp = frame_ptr[0];
        uint64_t return_address = frame_ptr[1];
        
        /* Validate return address */
        if (return_address < 0x100000 || return_address > 0x7FFFFFFFFFFF) {
            break;
        }
        
        frames[frame_count].rip = return_address;
        frames[frame_count].rbp = saved_rbp;
        kdebug_lookup_symbol(return_address, frames[frame_count].symbol, 64);
        
        frame_count++;
        current_rbp = saved_rbp;
        
        /* Prevent infinite loops */
        if (saved_rbp <= current_rbp) {
            break;
        }
    }
    
    return frame_count;
}

bool kdebug_lookup_symbol(uint64_t address, char* symbol_name, int buffer_size) {
    /* TODO: Implement symbol table lookup */
    /* For now, just show the address */
    snprintf(symbol_name, buffer_size, "<kernel+0x%lx>", address);
    return false;
}

/* ================================ Register and State Inspection ================================ */

void kdebug_capture_registers(kdebug_registers_t* registers) {
    if (!registers) {
        return;
    }
    
    /* Capture general purpose registers */
    __asm__ volatile (
        "mov %%rax, %0\n\t"
        "mov %%rbx, %1\n\t"
        "mov %%rcx, %2\n\t"
        "mov %%rdx, %3\n\t"
        "mov %%rsi, %4\n\t"
        "mov %%rdi, %5\n\t"
        "mov %%rbp, %6\n\t"
        "mov %%rsp, %7\n\t"
        "mov %%r8, %8\n\t"
        "mov %%r9, %9\n\t"
        "mov %%r10, %10\n\t"
        "mov %%r11, %11\n\t"
        "mov %%r12, %12\n\t"
        "mov %%r13, %13\n\t"
        "mov %%r14, %14\n\t"
        "mov %%r15, %15"
        : "=m" (registers->rax), "=m" (registers->rbx), "=m" (registers->rcx),
          "=m" (registers->rdx), "=m" (registers->rsi), "=m" (registers->rdi),
          "=m" (registers->rbp), "=m" (registers->rsp), "=m" (registers->r8),
          "=m" (registers->r9), "=m" (registers->r10), "=m" (registers->r11),
          "=m" (registers->r12), "=m" (registers->r13), "=m" (registers->r14),
          "=m" (registers->r15)
    );
    
    /* Capture instruction pointer and flags */
    __asm__ volatile ("lea (%%rip), %0" : "=r" (registers->rip));
    __asm__ volatile ("pushfq; pop %0" : "=r" (registers->rflags));
    
    /* Capture segment registers */
    __asm__ volatile ("mov %%cs, %0" : "=r" (registers->cs));
    __asm__ volatile ("mov %%ds, %0" : "=r" (registers->ds));
    __asm__ volatile ("mov %%es, %0" : "=r" (registers->es));
    __asm__ volatile ("mov %%fs, %0" : "=r" (registers->fs));
    __asm__ volatile ("mov %%gs, %0" : "=r" (registers->gs));
    __asm__ volatile ("mov %%ss, %0" : "=r" (registers->ss));
    
    /* Capture control registers */
    __asm__ volatile ("mov %%cr0, %0" : "=r" (registers->cr0));
    __asm__ volatile ("mov %%cr2, %0" : "=r" (registers->cr2));
    __asm__ volatile ("mov %%cr3, %0" : "=r" (registers->cr3));
    __asm__ volatile ("mov %%cr4, %0" : "=r" (registers->cr4));
}

void kdebug_display_registers(const kdebug_registers_t* registers) {
    if (!kdebug_is_enabled() || !registers) {
        return;
    }
    
    KLOG_INFO(LOG_CAT_KERNEL, "=== Register State ===");
    KLOG_INFO(LOG_CAT_KERNEL, "RAX: 0x%016lx  RBX: 0x%016lx  RCX: 0x%016lx  RDX: 0x%016lx",
             registers->rax, registers->rbx, registers->rcx, registers->rdx);
    KLOG_INFO(LOG_CAT_KERNEL, "RSI: 0x%016lx  RDI: 0x%016lx  RBP: 0x%016lx  RSP: 0x%016lx",
             registers->rsi, registers->rdi, registers->rbp, registers->rsp);
    KLOG_INFO(LOG_CAT_KERNEL, "R8:  0x%016lx  R9:  0x%016lx  R10: 0x%016lx  R11: 0x%016lx",
             registers->r8, registers->r9, registers->r10, registers->r11);
    KLOG_INFO(LOG_CAT_KERNEL, "R12: 0x%016lx  R13: 0x%016lx  R14: 0x%016lx  R15: 0x%016lx",
             registers->r12, registers->r13, registers->r14, registers->r15);
    KLOG_INFO(LOG_CAT_KERNEL, "RIP: 0x%016lx  RFLAGS: 0x%016lx", registers->rip, registers->rflags);
    KLOG_INFO(LOG_CAT_KERNEL, "CS: 0x%04x  DS: 0x%04x  ES: 0x%04x  FS: 0x%04x  GS: 0x%04x  SS: 0x%04x",
             registers->cs, registers->ds, registers->es, registers->fs, registers->gs, registers->ss);
    KLOG_INFO(LOG_CAT_KERNEL, "CR0: 0x%016lx  CR2: 0x%016lx  CR3: 0x%016lx  CR4: 0x%016lx",
             registers->cr0, registers->cr2, registers->cr3, registers->cr4);
}

void kdebug_display_kernel_state(void) {
    if (!kdebug_is_enabled()) {
        return;
    }
    
    KLOG_INFO(LOG_CAT_KERNEL, "=== Kernel State Summary ===");
    KLOG_INFO(LOG_CAT_KERNEL, "Debugger State: %s", 
             (debug_state == KDEBUG_STATE_ENABLED) ? "ENABLED" :
             (debug_state == KDEBUG_STATE_PAUSED) ? "PAUSED" :
             (debug_state == KDEBUG_STATE_STEPPING) ? "STEPPING" : "DISABLED");
    
    /* Display memory usage if available */
    /* TODO: Integrate with memory manager */
    
    /* Display interrupt state */
    uint64_t rflags;
    __asm__ volatile ("pushfq; pop %0" : "=r" (rflags));
    KLOG_INFO(LOG_CAT_KERNEL, "Interrupts: %s", (rflags & 0x200) ? "ENABLED" : "DISABLED");
    
    /* Display active breakpoints count */
    int active_bp = 0, active_wp = 0;
    for (int i = 0; i < KDEBUG_MAX_BREAKPOINTS; i++) {
        if (breakpoints[i].active) active_bp++;
    }
    for (int i = 0; i < KDEBUG_MAX_WATCHPOINTS; i++) {
        if (watchpoints[i].active) active_wp++;
    }
    KLOG_INFO(LOG_CAT_KERNEL, "Active Breakpoints: %d, Watchpoints: %d", active_bp, active_wp);
}

void kdebug_display_process_info(void) {
    if (!kdebug_is_enabled()) {
        return;
    }
    
    KLOG_INFO(LOG_CAT_KERNEL, "=== Process Information ===");
    /* TODO: Integrate with process manager */
    KLOG_INFO(LOG_CAT_KERNEL, "Current Process: kernel (PID 0)");
    KLOG_INFO(LOG_CAT_KERNEL, "Process State: running");
}

/* ================================ Exception Handlers ================================ */

void kdebug_panic_handler(const char* message, const kdebug_registers_t* registers) {
    if (!debug_initialized) {
        kdebug_init();
        kdebug_set_enabled(true);
    }
    
    KLOG_PANIC(LOG_CAT_KERNEL, "KERNEL PANIC: %s", message);
    
    if (registers) {
        kdebug_display_registers(registers);
        kdebug_stack_trace(registers);
    }
    
    kdebug_display_kernel_state();
    
    KLOG_PANIC(LOG_CAT_KERNEL, "System halted due to panic");
    
    /* Enter debug console if enabled */
    if (kdebug_is_enabled()) {
        kdebug_enter_console();
    }
    
    /* Halt the system */
    while (1) {
        __asm__ volatile ("hlt");
    }
}

void kdebug_page_fault_handler(uint64_t fault_address, uint64_t error_code,
                              const kdebug_registers_t* registers) {
    if (!kdebug_is_enabled()) {
        return;
    }
    
    KLOG_ERROR(LOG_CAT_MEMORY, "Page Fault at 0x%016lx (error: 0x%lx)", fault_address, error_code);
    
    /* Decode error code */
    const char* fault_type = (error_code & 1) ? "protection violation" : "page not present";
    const char* access_type = (error_code & 2) ? "write" : "read";
    const char* privilege = (error_code & 4) ? "user" : "supervisor";
    
    KLOG_ERROR(LOG_CAT_MEMORY, "Fault Type: %s (%s access from %s mode)", 
              fault_type, access_type, privilege);
    
    if (registers) {
        kdebug_display_registers(registers);
        kdebug_stack_trace(registers);
    }
    
    /* Enter debug console for investigation */
    kdebug_enter_console();
}

void kdebug_gpf_handler(uint64_t error_code, const kdebug_registers_t* registers) {
    if (!kdebug_is_enabled()) {
        return;
    }
    
    KLOG_ERROR(LOG_CAT_KERNEL, "General Protection Fault (error: 0x%lx)", error_code);
    
    if (registers) {
        kdebug_display_registers(registers);
        kdebug_stack_trace(registers);
    }
    
    /* Enter debug console for investigation */
    kdebug_enter_console();
}

/* ================================ Statistics ================================ */

const kdebug_stats_t* kdebug_get_statistics(void) {
    return &debug_stats;
}

void kdebug_reset_statistics(void) {
    memset(&debug_stats, 0, sizeof(debug_stats));
    KLOG_INFO(LOG_CAT_KERNEL, "Debug statistics reset");
}

void kdebug_display_statistics(void) {
    if (!kdebug_is_enabled()) {
        return;
    }
    
    KLOG_INFO(LOG_CAT_KERNEL, "=== Debug Statistics ===");
    KLOG_INFO(LOG_CAT_KERNEL, "Breakpoints Hit: %lu", debug_stats.total_breakpoints_hit);
    KLOG_INFO(LOG_CAT_KERNEL, "Memory Accesses Tracked: %lu", debug_stats.memory_accesses_tracked);
    KLOG_INFO(LOG_CAT_KERNEL, "Debug Commands Processed: %lu", debug_stats.debug_commands_processed);
    KLOG_INFO(LOG_CAT_KERNEL, "Stack Traces Generated: %lu", debug_stats.stack_traces_generated);
    KLOG_INFO(LOG_CAT_KERNEL, "Memory Dumps Performed: %lu", debug_stats.memory_dumps_performed);
}

/* ================================ Interactive Console ================================ */

void kdebug_enter_console(void) {
    if (!kdebug_is_enabled()) {
        return;
    }
    
    debug_state = KDEBUG_STATE_PAUSED;
    KLOG_INFO(LOG_CAT_KERNEL, "=== KERNEL DEBUG CONSOLE ===");
    KLOG_INFO(LOG_CAT_KERNEL, "Type 'help' for available commands, 'continue' to resume");
    
    /* TODO: Implement actual console input handling */
    /* For now, just simulate some basic commands */
    
    /* Display current state */
    kdebug_display_kernel_state();
    
    /* Resume execution for now */
    debug_state = KDEBUG_STATE_ENABLED;
}

bool kdebug_process_command(const char* command) {
    if (!command || !kdebug_is_enabled()) {
        return false;
    }
    
    debug_stats.debug_commands_processed++;
    
    /* Basic command parsing */
    if (strcmp(command, "help") == 0) {
        KLOG_INFO(LOG_CAT_KERNEL, "Available commands:");
        KLOG_INFO(LOG_CAT_KERNEL, "  help     - Show this help");
        KLOG_INFO(LOG_CAT_KERNEL, "  continue - Resume execution");
        KLOG_INFO(LOG_CAT_KERNEL, "  regs     - Show registers");
        KLOG_INFO(LOG_CAT_KERNEL, "  stack    - Show stack trace");
        KLOG_INFO(LOG_CAT_KERNEL, "  bp       - List breakpoints");
        KLOG_INFO(LOG_CAT_KERNEL, "  stats    - Show statistics");
        return true;
    }
    
    if (strcmp(command, "continue") == 0) {
        debug_state = KDEBUG_STATE_ENABLED;
        KLOG_INFO(LOG_CAT_KERNEL, "Resuming execution...");
        return true;
    }
    
    if (strcmp(command, "regs") == 0) {
        kdebug_registers_t regs;
        kdebug_capture_registers(&regs);
        kdebug_display_registers(&regs);
        return true;
    }
    
    if (strcmp(command, "stack") == 0) {
        kdebug_stack_trace(NULL);
        return true;
    }
    
    if (strcmp(command, "bp") == 0) {
        kdebug_list_breakpoints();
        return true;
    }
    
    if (strcmp(command, "stats") == 0) {
        kdebug_display_statistics();
        return true;
    }
    
    KLOG_ERROR(LOG_CAT_KERNEL, "Unknown command: %s", command);
    return false;
}

void kdebug_add_command(const char* command, 
                       bool (*handler)(const char* args),
                       const char* help_text) {
    /* TODO: Implement custom command registration */
    KLOG_INFO(LOG_CAT_KERNEL, "Custom command '%s' registered: %s", command, help_text);
}
