# IKOS User-Space Process Execution Implementation

## Overview

This document describes the implementation of user-space process execution in the IKOS kernel, which enables the creation, loading, and execution of user-space applications. This is a critical milestone that bridges the gap between kernel-space and user-space, allowing the OS to run actual applications.

## Implementation Details

### 1. Process Management Framework (`process.h`, `process.c`)

#### Process Control Block (PCB)
The kernel maintains a comprehensive Process Control Block for each process:

```c
typedef struct process {
    uint32_t pid;                       // Process ID
    uint32_t ppid;                      // Parent process ID
    char name[MAX_PROCESS_NAME];        // Process name
    process_state_t state;              // Current state (READY, RUNNING, etc.)
    process_priority_t priority;        // Process priority
    vm_space_t* address_space;          // Virtual memory space
    process_context_t context;          // Saved CPU context
    file_descriptor_t fds[MAX_OPEN_FILES]; // File descriptors
    // ... additional fields for process tree, scheduling, etc.
} process_t;
```

#### Key Features
- **Process States**: READY, RUNNING, BLOCKED, ZOMBIE, TERMINATED
- **Priority Levels**: IDLE, LOW, NORMAL, HIGH, REALTIME
- **Process Tree**: Parent-child relationships with sibling links
- **File Descriptors**: Support for up to 64 open files per process
- **Memory Layout**: User space (4MB-32GB), stack (2MB), heap (expandable)

#### Process Creation
```c
process_t* process_create_from_elf(const char* name, void* elf_data, size_t size);
```

1. Validates ELF executable
2. Allocates process structure and assigns PID
3. Sets up virtual memory layout
4. Loads ELF segments into process memory
5. Initializes CPU context for user mode
6. Adds process to ready queue

### 2. ELF Loader (`elf.h`, `elf_loader.c`)

#### 64-bit ELF Support
Extended the existing 32-bit ELF support to handle 64-bit executables:

```c
typedef struct elf64_header {
    unsigned char e_ident[16];          // ELF identification
    unsigned short e_type;              // Object file type
    unsigned short e_machine;           // Architecture
    unsigned long e_entry;              // Entry point virtual address
    // ... additional fields
} elf64_header_t;
```

#### Loading Process
1. **Validation**: Checks ELF magic number, architecture (x86-64), and executable type
2. **Segment Loading**: Maps PT_LOAD segments into process virtual memory
3. **Permission Setting**: Applies proper read/write/execute permissions
4. **BSS Initialization**: Zeros out uninitialized data sections
5. **Entry Point**: Sets up initial CPU context with program entry point

### 3. System Call Interface (`syscalls.c`)

#### Supported System Calls
- `SYS_EXIT` (60): Terminate process
- `SYS_WRITE` (1): Write to file descriptor
- `SYS_READ` (0): Read from file descriptor
- `SYS_GETPID` (39): Get process ID
- `SYS_GETPPID` (110): Get parent process ID

#### System Call Handler
```c
long handle_system_call(interrupt_frame_t* frame);
```

The system call dispatcher:
1. Extracts system call number from RAX register
2. Validates current process context
3. Dispatches to appropriate handler function
4. Returns result in RAX register

#### Safety Features
- **Pointer Validation**: Ensures user pointers are within user space
- **Copy Functions**: Safe data transfer between user and kernel space
- **Error Handling**: Proper error codes for invalid operations

### 4. Context Switching (`user_mode.asm`)

#### Assembly Routines
- `switch_to_user_mode`: Transitions from kernel to user mode
- `save_user_context`: Saves user context during interrupts
- `restore_user_context`: Restores user context when resuming
- `enter_user_mode`: Initial entry into user mode for new processes

#### Context Switch Process
1. **Save State**: Preserve all CPU registers and flags
2. **Address Space Switch**: Load new page directory (CR3)
3. **Segment Setup**: Configure user-mode segments
4. **Stack Switch**: Switch to user-mode stack
5. **IRET**: Use interrupt return to enter user mode with proper privilege level

#### CPU Context Structure
```c
typedef struct {
    uint64_t rax, rbx, rcx, rdx;       // General purpose registers
    uint64_t rsi, rdi, rbp, rsp;       // Pointer registers
    uint64_t r8, r9, r10, r11;         // Extended registers
    uint64_t r12, r13, r14, r15;       // Extended registers
    uint64_t rip;                      // Instruction pointer
    uint64_t rflags;                   // CPU flags
    uint64_t cr3;                      // Page directory base
    uint16_t cs, ds, es, fs, gs, ss;   // Segment registers
} process_context_t;
```

## Memory Layout

### User-Space Virtual Memory Map
```
0x7FFFFFFFFFFF  ┌─────────────────┐
                │   User Stack    │ 2MB (grows down)
0x7FFFFFFFFFDF  ├─────────────────┤
                │                 │
                │   Available     │
                │   Virtual       │
                │   Memory        │
                │                 │
0x800000000     ├─────────────────┤
                │                 │
                │   User Heap     │ (grows up)
                │                 │
0x600000        ├─────────────────┤
                │                 │
                │   Program       │
                │   Segments      │
                │   (.text,       │
                │    .data,       │
                │    .bss)        │
                │                 │
0x400000        └─────────────────┘
```

### Memory Protection
- **User Space Isolation**: User processes cannot access kernel memory
- **Page-Level Protection**: Read/write/execute permissions enforced by MMU
- **Stack Guard**: Stack overflow protection (future enhancement)
- **ASLR Ready**: Address layout randomization support structure

## Integration with Existing Systems

### Virtual Memory Manager (VMM)
- Uses `vmm_create_address_space()` for process virtual memory
- Leverages `vmm_map_page()` for segment mapping
- Integrates with `vmm_switch_address_space()` for context switching

### Interrupt System
- System calls handled through interrupt mechanism
- Context switching integrated with timer interrupts
- Proper interrupt frame handling for user-mode transitions

### Scheduler Integration
- Process ready queue management
- Timer-based preemptive scheduling support
- Process state transitions

## Testing and Validation

### Test Suite (`test_user_space.c`)
- Process management initialization tests
- ELF validation and loading tests
- Memory layout verification
- Context switching structure validation
- System call interface testing

### Build System
- Added `userspace` make target for building components
- Added `userspace-smoke` for basic testing
- Integration with existing kernel build process

## Current Limitations

1. **File System**: File loading not yet implemented (requires file system)
2. **Fork/Exec**: Process creation from existing processes not implemented
3. **Signal Handling**: User-space signal delivery not implemented
4. **Shared Memory**: Inter-process shared memory not implemented
5. **Dynamic Loading**: Shared library support not implemented

## Future Enhancements

1. **Process Scheduler**: Full preemptive scheduling implementation
2. **Memory Management**: Advanced features like copy-on-write, demand paging
3. **File System Integration**: Loading executables from disk
4. **IPC Mechanisms**: Pipes, message queues, shared memory
5. **Signal System**: POSIX-style signal delivery
6. **Multi-threading**: Thread support within processes

## Security Considerations

1. **Privilege Separation**: Proper kernel/user mode separation
2. **Memory Protection**: Page-level access control
3. **System Call Validation**: Input sanitization and bounds checking
4. **Resource Limits**: Process resource quotas (future)
5. **Capability System**: Fine-grained permission control (future)

## Performance Characteristics

- **Context Switch Time**: Optimized assembly routines for minimal overhead
- **Memory Overhead**: Efficient process structure layout
- **Scheduling Overhead**: O(1) ready queue operations
- **System Call Latency**: Direct interrupt-based dispatching

## Conclusion

The user-space process execution system provides a solid foundation for running user applications in the IKOS kernel. It implements the essential components needed for process management, ELF loading, system calls, and context switching, while maintaining proper security boundaries between kernel and user space.

This implementation represents a significant milestone in kernel development, enabling the transition from a basic kernel to a functional operating system capable of running real applications.
