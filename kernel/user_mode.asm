; IKOS User-Space Context Switching
; Assembly routines for switching between kernel and user mode

section .text
bits 64

; External C functions
extern current_process
extern handle_system_call

; =============================================================================
; CONTEXT SWITCHING ROUTINES
; =============================================================================

; Switch to user mode
; Parameters: RDI = process context pointer
global switch_to_user_mode
switch_to_user_mode:
    ; Save current kernel stack
    mov rsp, rdi
    
    ; Load user segments
    mov ax, 0x23        ; User data segment (GDT entry 4, DPL 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Set up stack for IRET
    ; Stack layout for IRET to user mode:
    ; [SS]     - User stack segment
    ; [RSP]    - User stack pointer  
    ; [RFLAGS] - CPU flags
    ; [CS]     - User code segment
    ; [RIP]    - User instruction pointer
    
    ; Get process context
    mov rax, [rdi + 0x88]   ; context.ss
    push rax                ; Push SS
    
    mov rax, [rdi + 0x20]   ; context.rsp
    push rax                ; Push RSP
    
    mov rax, [rdi + 0x70]   ; context.rflags
    or rax, 0x200           ; Ensure interrupts enabled
    push rax                ; Push RFLAGS
    
    mov rax, [rdi + 0x78]   ; context.cs
    push rax                ; Push CS
    
    mov rax, [rdi + 0x68]   ; context.rip
    push rax                ; Push RIP
    
    ; Restore general purpose registers
    mov rax, [rdi + 0x00]   ; rax
    mov rbx, [rdi + 0x08]   ; rbx
    mov rcx, [rdi + 0x10]   ; rcx
    mov rdx, [rdi + 0x18]   ; rdx
    mov rsi, [rdi + 0x28]   ; rsi
    mov rbp, [rdi + 0x30]   ; rbp
    mov r8,  [rdi + 0x40]   ; r8
    mov r9,  [rdi + 0x48]   ; r9
    mov r10, [rdi + 0x50]   ; r10
    mov r11, [rdi + 0x58]   ; r11
    mov r12, [rdi + 0x60]   ; r12
    mov r13, [rdi + 0x68]   ; r13
    mov r14, [rdi + 0x70]   ; r14
    mov r15, [rdi + 0x78]   ; r15
    
    ; Load RDI last since we've been using it
    mov rdi, [rdi + 0x38]   ; rdi
    
    ; Switch to user mode
    iretq

; Save user context and return to kernel
; This is called from interrupt handlers when returning from user mode
global save_user_context
save_user_context:
    ; RDI points to interrupt frame
    ; We need to save the user context to the current process
    
    ; Get current process pointer
    mov rax, [current_process]
    test rax, rax
    jz .no_process
    
    ; Save general purpose registers from interrupt frame
    ; The interrupt frame contains registers in this order:
    ; r15, r14, r13, r12, r11, r10, r9, r8
    ; rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax
    ; int_no, error_code
    ; rip, cs, rflags, user_rsp, ss
    
    ; Save registers to process context
    mov rbx, [rdi + 0x78]   ; rax from frame
    mov [rax + 0x00], rbx   ; save to context.rax
    
    mov rbx, [rdi + 0x70]   ; rcx from frame
    mov [rax + 0x10], rbx   ; save to context.rcx
    
    mov rbx, [rdi + 0x68]   ; rdx from frame
    mov [rax + 0x18], rbx   ; save to context.rdx
    
    mov rbx, [rdi + 0x60]   ; rbx from frame
    mov [rax + 0x08], rbx   ; save to context.rbx
    
    mov rbx, [rdi + 0x50]   ; rsp from frame (kernel rsp during interrupt)
    mov [rax + 0x20], rbx   ; save to context.rsp
    
    mov rbx, [rdi + 0x48]   ; rbp from frame
    mov [rax + 0x30], rbx   ; save to context.rbp
    
    mov rbx, [rdi + 0x40]   ; rsi from frame
    mov [rax + 0x28], rbx   ; save to context.rsi
    
    mov rbx, [rdi + 0x38]   ; rdi from frame
    mov [rax + 0x38], rbx   ; save to context.rdi
    
    ; Save extended registers
    mov rbx, [rdi + 0x30]   ; r8 from frame
    mov [rax + 0x40], rbx   ; save to context.r8
    
    mov rbx, [rdi + 0x28]   ; r9 from frame
    mov [rax + 0x48], rbx   ; save to context.r9
    
    mov rbx, [rdi + 0x20]   ; r10 from frame
    mov [rax + 0x50], rbx   ; save to context.r10
    
    mov rbx, [rdi + 0x18]   ; r11 from frame
    mov [rax + 0x58], rbx   ; save to context.r11
    
    mov rbx, [rdi + 0x10]   ; r12 from frame
    mov [rax + 0x60], rbx   ; save to context.r12
    
    mov rbx, [rdi + 0x08]   ; r13 from frame
    mov [rax + 0x68], rbx   ; save to context.r13
    
    mov rbx, [rdi + 0x00]   ; r14 from frame
    mov [rax + 0x70], rbx   ; save to context.r14
    
    mov rbx, [rdi + 0x08]   ; r15 from frame  
    mov [rax + 0x78], rbx   ; save to context.r15
    
    ; Save control registers
    mov rbx, [rdi + 0x90]   ; rip from frame
    mov [rax + 0x68], rbx   ; save to context.rip
    
    mov rbx, [rdi + 0xA0]   ; rflags from frame
    mov [rax + 0x70], rbx   ; save to context.rflags
    
    mov rbx, [rdi + 0x98]   ; cs from frame
    mov [rax + 0x78], rbx   ; save to context.cs
    
    mov rbx, [rdi + 0xB0]   ; user_rsp from frame (actual user stack)
    mov [rax + 0x20], rbx   ; save to context.rsp (overwrite kernel rsp)
    
    mov rbx, [rdi + 0xB8]   ; ss from frame
    mov [rax + 0x88], rbx   ; save to context.ss
    
    ; Save current CR3
    mov rbx, cr3
    mov [rax + 0x80], rbx   ; save to context.cr3
    
.no_process:
    ret

; Restore user context and switch back to user mode
; Called when resuming a user process
global restore_user_context
restore_user_context:
    ; RDI points to process structure
    test rdi, rdi
    jz .no_process
    
    ; Switch to process address space
    mov rax, [rdi + 0x80]   ; context.cr3
    mov cr3, rax
    
    ; Set up user mode return
    ; We'll use the same IRET mechanism as switch_to_user_mode
    jmp switch_to_user_mode
    
.no_process:
    ret

; System call entry point
; Called when user mode issues a system call
global syscall_entry
syscall_entry:
    ; Save user state
    push rax    ; System call number
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
    
    ; Switch to kernel segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Call system call handler
    ; System call number is in original RAX (now on stack)
    mov rdi, rsp        ; Pass stack pointer as argument
    call handle_system_call
    
    ; Restore user state
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
    add rsp, 8          ; Remove system call number (result in RAX)
    
    ; Return to user mode
    iretq

; Handle kernel-to-user mode transition
; Used when starting a new process
global enter_user_mode
enter_user_mode:
    ; RDI = entry point, RSI = user stack pointer
    
    ; Set up segments for user mode
    mov ax, 0x23        ; User data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Build IRET frame
    push 0x23           ; SS (user stack segment)
    push rsi            ; RSP (user stack pointer)
    push 0x202          ; RFLAGS (interrupts enabled)
    push 0x1B           ; CS (user code segment)
    push rdi            ; RIP (entry point)
    
    ; Clear all registers for clean start
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    
    ; Enter user mode
    iretq

; =============================================================================
; UTILITY FUNCTIONS
; =============================================================================

; Get current stack pointer
global get_stack_pointer
get_stack_pointer:
    mov rax, rsp
    ret

; Get current code segment
global get_code_segment
get_code_segment:
    mov rax, cs
    ret
