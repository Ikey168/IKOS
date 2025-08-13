; IKOS Context Switching Assembly Functions
; Provides low-level CPU context save/restore for task switching

section .text
global save_task_context
global restore_task_context
global switch_task_context

; Save current CPU context to task structure
; RDI = pointer to task_t structure
save_task_context:
    ; Save general purpose registers
    mov [rdi + 0x00], rax    ; context.rax
    mov [rdi + 0x08], rbx    ; context.rbx
    mov [rdi + 0x10], rcx    ; context.rcx
    mov [rdi + 0x18], rdx    ; context.rdx
    mov [rdi + 0x20], rsi    ; context.rsi
    mov [rdi + 0x28], rdi    ; context.rdi (save original RDI)
    mov [rdi + 0x30], rbp    ; context.rbp
    mov [rdi + 0x38], rsp    ; context.rsp
    mov [rdi + 0x40], r8     ; context.r8
    mov [rdi + 0x48], r9     ; context.r9
    mov [rdi + 0x50], r10    ; context.r10
    mov [rdi + 0x58], r11    ; context.r11
    mov [rdi + 0x60], r12    ; context.r12
    mov [rdi + 0x68], r13    ; context.r13
    mov [rdi + 0x70], r14    ; context.r14
    mov [rdi + 0x78], r15    ; context.r15
    
    ; Save instruction pointer (return address)
    mov rax, [rsp]           ; Get return address from stack
    mov [rdi + 0x80], rax    ; context.rip
    
    ; Save flags register
    pushfq
    pop rax
    mov [rdi + 0x88], rax    ; context.rflags
    
    ; Save segment registers
    mov ax, cs
    mov [rdi + 0x90], ax     ; context.cs
    mov ax, ds
    mov [rdi + 0x92], ax     ; context.ds
    mov ax, es
    mov [rdi + 0x94], ax     ; context.es
    mov ax, fs
    mov [rdi + 0x96], ax     ; context.fs
    mov ax, gs
    mov [rdi + 0x98], ax     ; context.gs
    mov ax, ss
    mov [rdi + 0x9A], ax     ; context.ss
    
    ; Save control registers
    mov rax, cr3
    mov [rdi + 0xA0], rax    ; context.cr3
    
    ret

; Restore CPU context from task structure
; RDI = pointer to task_t structure
restore_task_context:
    ; Restore control registers first
    mov rax, [rdi + 0xA0]    ; context.cr3
    mov cr3, rax
    
    ; Restore segment registers
    mov ax, [rdi + 0x92]     ; context.ds
    mov ds, ax
    mov ax, [rdi + 0x94]     ; context.es
    mov es, ax
    mov ax, [rdi + 0x96]     ; context.fs
    mov fs, ax
    mov ax, [rdi + 0x98]     ; context.gs
    mov gs, ax
    mov ax, [rdi + 0x9A]     ; context.ss
    mov ss, ax
    
    ; Set up stack for IRET
    mov rax, [rdi + 0x38]    ; context.rsp
    mov rsp, rax
    
    ; Push values for IRET (SS, RSP, RFLAGS, CS, RIP)
    mov ax, [rdi + 0x9A]     ; context.ss
    push rax
    mov rax, [rdi + 0x38]    ; context.rsp
    push rax
    mov rax, [rdi + 0x88]    ; context.rflags
    push rax
    mov ax, [rdi + 0x90]     ; context.cs
    push rax
    mov rax, [rdi + 0x80]    ; context.rip
    push rax
    
    ; Restore general purpose registers
    mov rax, [rdi + 0x00]    ; context.rax
    mov rbx, [rdi + 0x08]    ; context.rbx
    mov rcx, [rdi + 0x10]    ; context.rcx
    mov rdx, [rdi + 0x18]    ; context.rdx
    mov rsi, [rdi + 0x20]    ; context.rsi
    mov rbp, [rdi + 0x30]    ; context.rbp
    mov r8,  [rdi + 0x40]    ; context.r8
    mov r9,  [rdi + 0x48]    ; context.r9
    mov r10, [rdi + 0x50]    ; context.r10
    mov r11, [rdi + 0x58]    ; context.r11
    mov r12, [rdi + 0x60]    ; context.r12
    mov r13, [rdi + 0x68]    ; context.r13
    mov r14, [rdi + 0x70]    ; context.r14
    mov r15, [rdi + 0x78]    ; context.r15
    
    ; Restore RDI last
    mov rdi, [rdi + 0x28]    ; context.rdi
    
    ; Return to new task
    iretq

; Complete context switch between two tasks
; RDI = previous task pointer
; RSI = next task pointer
switch_task_context:
    ; Save current task context if previous task exists
    test rdi, rdi
    jz .restore_next
    
    ; Save context of previous task
    push rsi                 ; Preserve next task pointer
    call save_task_context
    pop rsi                  ; Restore next task pointer
    
.restore_next:
    ; Restore context of next task
    mov rdi, rsi
    call restore_task_context
    
    ; This point should never be reached due to iretq
    ret

; Timer interrupt entry point
global timer_interrupt_entry
timer_interrupt_entry:
    ; Save all registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Call C timer handler
    extern timer_interrupt_handler
    call timer_interrupt_handler
    
    ; Restore all registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; Return from interrupt
    iretq

; System call entry point for yield
global syscall_yield_entry
syscall_yield_entry:
    ; Save caller's context
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Call C yield function
    extern sys_yield
    call sys_yield
    
    ; Restore context
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; Return to caller
    ret

; Get current stack pointer
global get_current_rsp
get_current_rsp:
    mov rax, rsp
    ret

; Set stack pointer
global set_rsp
set_rsp:
    mov rsp, rdi
    ret
