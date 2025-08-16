/* IKOS Runtime Kernel Debugger - Issue #16 Enhancement
 * Real-time debugging interface with interactive capabilities
 * This complements the existing kernel_log.h system
 */

#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H

#include <stdint.h>
#include <stdbool.h>

/* Debug Interface Configuration */
#define KDEBUG_MAX_BREAKPOINTS    8
#define KDEBUG_MAX_WATCHPOINTS    4
#define KDEBUG_STACK_TRACE_DEPTH  16
#define KDEBUG_CMD_BUFFER_SIZE    256

/* Debug States */
typedef enum {
    KDEBUG_STATE_DISABLED,
    KDEBUG_STATE_ENABLED,
    KDEBUG_STATE_PAUSED,
    KDEBUG_STATE_STEPPING
} kdebug_state_t;

/* Breakpoint Types */
typedef enum {
    KDEBUG_BP_EXECUTION,
    KDEBUG_BP_MEMORY_READ,
    KDEBUG_BP_MEMORY_WRITE,
    KDEBUG_BP_MEMORY_ACCESS
} kdebug_breakpoint_type_t;

/* Debug Breakpoint Structure */
typedef struct {
    bool active;
    kdebug_breakpoint_type_t type;
    uint64_t address;
    uint64_t length;
    uint64_t hit_count;
    char description[64];
} kdebug_breakpoint_t;

/* Register Context for Debugging */
typedef struct {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip, rflags;
    uint16_t cs, ds, es, fs, gs, ss;
    uint64_t cr0, cr2, cr3, cr4;
} kdebug_registers_t;

/* Stack Frame Structure */
typedef struct {
    uint64_t rip;
    uint64_t rbp;
    char symbol[64];
} kdebug_stack_frame_t;

/* Debug Statistics */
typedef struct {
    uint64_t total_breakpoints_hit;
    uint64_t memory_accesses_tracked;
    uint64_t debug_commands_processed;
    uint64_t stack_traces_generated;
    uint64_t memory_dumps_performed;
} kdebug_stats_t;

/* =========================== Core Debug Interface =========================== */

/**
 * Initialize the runtime kernel debugger
 * @return true if initialization successful, false otherwise
 */
bool kdebug_init(void);

/**
 * Enable/disable the kernel debugger
 * @param enabled true to enable, false to disable
 */
void kdebug_set_enabled(bool enabled);

/**
 * Check if debugger is currently enabled
 * @return true if enabled, false otherwise
 */
bool kdebug_is_enabled(void);

/**
 * Get current debugger state
 * @return current state
 */
kdebug_state_t kdebug_get_state(void);

/* =========================== Breakpoint Management =========================== */

/**
 * Set an execution breakpoint at specified address
 * @param address memory address to break at
 * @param description optional description of breakpoint
 * @return breakpoint ID on success, -1 on failure
 */
int kdebug_set_breakpoint(uint64_t address, const char* description);

/**
 * Set a memory watchpoint
 * @param address memory address to watch
 * @param length number of bytes to watch
 * @param type type of access to break on
 * @param description optional description
 * @return watchpoint ID on success, -1 on failure
 */
int kdebug_set_watchpoint(uint64_t address, uint64_t length, 
                         kdebug_breakpoint_type_t type, const char* description);

/**
 * Remove a breakpoint or watchpoint
 * @param id breakpoint/watchpoint ID to remove
 * @return true if removed successfully, false otherwise
 */
bool kdebug_remove_breakpoint(int id);

/**
 * List all active breakpoints and watchpoints
 */
void kdebug_list_breakpoints(void);

/**
 * Clear all breakpoints and watchpoints
 */
void kdebug_clear_all_breakpoints(void);

/* =========================== Memory Debugging =========================== */

/**
 * Dump memory contents in hex and ASCII format
 * @param address starting memory address
 * @param length number of bytes to dump
 */
void kdebug_memory_dump(uint64_t address, uint64_t length);

/**
 * Search for a pattern in memory
 * @param start_address starting address for search
 * @param end_address ending address for search
 * @param pattern pattern to search for
 * @param pattern_length length of pattern in bytes
 * @return address of first match, or 0 if not found
 */
uint64_t kdebug_memory_search(uint64_t start_address, uint64_t end_address,
                             const uint8_t* pattern, uint64_t pattern_length);

/**
 * Read memory safely with error checking
 * @param address memory address to read from
 * @param buffer buffer to store read data
 * @param length number of bytes to read
 * @return true if read successful, false on page fault or invalid address
 */
bool kdebug_memory_read(uint64_t address, void* buffer, uint64_t length);

/**
 * Write memory safely with error checking
 * @param address memory address to write to
 * @param buffer data to write
 * @param length number of bytes to write
 * @return true if write successful, false on page fault or read-only memory
 */
bool kdebug_memory_write(uint64_t address, const void* buffer, uint64_t length);

/* =========================== Stack Tracing =========================== */

/**
 * Generate and display a stack trace
 * @param registers current register state (optional, can be NULL)
 */
void kdebug_stack_trace(const kdebug_registers_t* registers);

/**
 * Get stack trace frames without displaying
 * @param frames array to store stack frames
 * @param max_frames maximum number of frames to capture
 * @param registers current register state (optional)
 * @return number of frames captured
 */
int kdebug_get_stack_frames(kdebug_stack_frame_t* frames, int max_frames,
                           const kdebug_registers_t* registers);

/**
 * Lookup symbol name for an address
 * @param address memory address
 * @param symbol_name buffer to store symbol name
 * @param buffer_size size of symbol_name buffer
 * @return true if symbol found, false otherwise
 */
bool kdebug_lookup_symbol(uint64_t address, char* symbol_name, int buffer_size);

/* =========================== Register and State Inspection =========================== */

/**
 * Capture current register state
 * @param registers structure to store register values
 */
void kdebug_capture_registers(kdebug_registers_t* registers);

/**
 * Display current register state
 * @param registers register structure to display
 */
void kdebug_display_registers(const kdebug_registers_t* registers);

/**
 * Display current kernel state summary
 */
void kdebug_display_kernel_state(void);

/**
 * Display process and thread information
 */
void kdebug_display_process_info(void);

/* =========================== Interactive Debug Console =========================== */

/**
 * Start interactive debug console
 * This pauses kernel execution and provides command-line interface
 */
void kdebug_enter_console(void);

/**
 * Process a debug command
 * @param command command string to process
 * @return true if command processed successfully
 */
bool kdebug_process_command(const char* command);

/**
 * Add a custom debug command handler
 * @param command command name
 * @param handler function to handle the command
 * @param help_text help text for the command
 */
void kdebug_add_command(const char* command, 
                       bool (*handler)(const char* args),
                       const char* help_text);

/* =========================== Panic and Exception Handling =========================== */

/**
 * Handle kernel panic with debugging information
 * @param message panic message
 * @param registers register state at panic
 */
void kdebug_panic_handler(const char* message, const kdebug_registers_t* registers);

/**
 * Handle page fault with debugging information
 * @param fault_address address that caused the fault
 * @param error_code page fault error code
 * @param registers register state at fault
 */
void kdebug_page_fault_handler(uint64_t fault_address, uint64_t error_code,
                              const kdebug_registers_t* registers);

/**
 * Handle general protection fault with debugging
 * @param error_code fault error code
 * @param registers register state at fault
 */
void kdebug_gpf_handler(uint64_t error_code, const kdebug_registers_t* registers);

/* =========================== Statistics and Monitoring =========================== */

/**
 * Get debugging statistics
 * @return pointer to current statistics
 */
const kdebug_stats_t* kdebug_get_statistics(void);

/**
 * Reset debugging statistics
 */
void kdebug_reset_statistics(void);

/**
 * Display debugging statistics
 */
void kdebug_display_statistics(void);

/* =========================== Convenience Macros =========================== */

/* Break into debugger if enabled */
#define KDEBUG_BREAK() do { \
    if (kdebug_is_enabled()) { \
        kdebug_enter_console(); \
    } \
} while(0)

/* Assert with debugger break */
#define KDEBUG_ASSERT(condition) do { \
    if (!(condition) && kdebug_is_enabled()) { \
        kdebug_registers_t regs; \
        kdebug_capture_registers(&regs); \
        kdebug_panic_handler("Assertion failed: " #condition, &regs); \
    } \
} while(0)

/* Memory dump convenience macro */
#define KDEBUG_DUMP_MEMORY(addr, size) do { \
    if (kdebug_is_enabled()) { \
        kdebug_memory_dump((uint64_t)(addr), (uint64_t)(size)); \
    } \
} while(0)

/* Stack trace convenience macro */
#define KDEBUG_STACK_TRACE() do { \
    if (kdebug_is_enabled()) { \
        kdebug_stack_trace(NULL); \
    } \
} while(0)

/* Conditional breakpoint macro */
#define KDEBUG_BREAK_IF(condition) do { \
    if ((condition) && kdebug_is_enabled()) { \
        kdebug_enter_console(); \
    } \
} while(0)

/* External declarations for integration */
extern kdebug_breakpoint_t breakpoints[];
extern kdebug_breakpoint_t watchpoints[];

#endif /* KERNEL_DEBUG_H */
