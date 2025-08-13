; IKOS Interrupt Handler Stubs
; Assembly interrupt entry points and interrupt service routines

section .text
bits 64

; External C functions
extern interrupt_handler
extern irq_handler
extern handle_syscall

; Exception handlers without error code
%macro EXCEPTION_NOERRCODE 1
global exception_%1
exception_%1:
    push qword 0    ; Dummy error code
    push qword %1   ; Interrupt number
    jmp exception_common
%endmacro

; Exception handlers with error code
%macro EXCEPTION_ERRCODE 1
global exception_%1
exception_%1:
    push qword %1   ; Interrupt number
    jmp exception_common
%endmacro

; IRQ handlers
%macro IRQ_HANDLER 1
global irq_%1
irq_%1:
    push qword 0        ; Dummy error code
    push qword (%1+32)  ; IRQ number + offset
    jmp irq_common
%endmacro

; CPU Exception handlers
EXCEPTION_NOERRCODE 0   ; Divide by zero
EXCEPTION_NOERRCODE 1   ; Debug
EXCEPTION_NOERRCODE 2   ; Non-maskable interrupt
EXCEPTION_NOERRCODE 3   ; Breakpoint
EXCEPTION_NOERRCODE 4   ; Overflow
EXCEPTION_NOERRCODE 5   ; Bound range exceeded
EXCEPTION_NOERRCODE 6   ; Invalid opcode
EXCEPTION_NOERRCODE 7   ; Device not available
EXCEPTION_ERRCODE   8   ; Double fault
EXCEPTION_NOERRCODE 9   ; Coprocessor segment overrun
EXCEPTION_ERRCODE   10  ; Invalid TSS
EXCEPTION_ERRCODE   11  ; Segment not present
EXCEPTION_ERRCODE   12  ; Stack fault
EXCEPTION_ERRCODE   13  ; General protection fault
EXCEPTION_ERRCODE   14  ; Page fault
EXCEPTION_NOERRCODE 15  ; Reserved
EXCEPTION_NOERRCODE 16  ; FPU floating-point error
EXCEPTION_ERRCODE   17  ; Alignment check
EXCEPTION_NOERRCODE 18  ; Machine check
EXCEPTION_NOERRCODE 19  ; SIMD floating-point exception

; Named exception handlers for C code
global exception_divide_error
global exception_debug
global exception_nmi
global exception_breakpoint
global exception_overflow
global exception_bound_range
global exception_invalid_opcode
global exception_device_not_available
global exception_double_fault
global exception_invalid_tss
global exception_segment_not_present
global exception_stack_fault
global exception_general_protection
global exception_page_fault
global exception_fpu_error
global exception_alignment_check
global exception_machine_check
global exception_simd_exception

exception_divide_error:         jmp exception_0
exception_debug:                jmp exception_1
exception_nmi:                  jmp exception_2
exception_breakpoint:           jmp exception_3
exception_overflow:             jmp exception_4
exception_bound_range:          jmp exception_5
exception_invalid_opcode:       jmp exception_6
exception_device_not_available: jmp exception_7
exception_double_fault:         jmp exception_8
exception_invalid_tss:          jmp exception_10
exception_segment_not_present:  jmp exception_11
exception_stack_fault:          jmp exception_12
exception_general_protection:   jmp exception_13
exception_page_fault:           jmp exception_14
exception_fpu_error:            jmp exception_16
exception_alignment_check:      jmp exception_17
exception_machine_check:        jmp exception_18
exception_simd_exception:       jmp exception_19

; IRQ handlers
IRQ_HANDLER 0   ; Timer
IRQ_HANDLER 1   ; Keyboard
IRQ_HANDLER 2   ; Cascade
IRQ_HANDLER 3   ; COM2
IRQ_HANDLER 4   ; COM1
IRQ_HANDLER 5   ; LPT2
IRQ_HANDLER 6   ; Floppy
IRQ_HANDLER 7   ; LPT1
IRQ_HANDLER 8   ; CMOS RTC
IRQ_HANDLER 9   ; Free
IRQ_HANDLER 10  ; Free
IRQ_HANDLER 11  ; Free
IRQ_HANDLER 12  ; PS/2 Mouse
IRQ_HANDLER 13  ; FPU
IRQ_HANDLER 14  ; Primary ATA
IRQ_HANDLER 15  ; Secondary ATA

; Named IRQ handlers for C code
global irq_timer
global irq_keyboard
global irq_cascade
global irq_com2
global irq_com1
global irq_lpt2
global irq_floppy
global irq_lpt1
global irq_cmos_rtc
global irq_free1
global irq_free2
global irq_free3
global irq_ps2_mouse
global irq_fpu
global irq_primary_ata
global irq_secondary_ata

irq_timer:       jmp irq_0
irq_keyboard:    jmp irq_1
irq_cascade:     jmp irq_2
irq_com2:        jmp irq_3
irq_com1:        jmp irq_4
irq_lpt2:        jmp irq_5
irq_floppy:      jmp irq_6
irq_lpt1:        jmp irq_7
irq_cmos_rtc:    jmp irq_8
irq_free1:       jmp irq_9
irq_free2:       jmp irq_10
irq_free3:       jmp irq_11
irq_ps2_mouse:   jmp irq_12
irq_fpu:         jmp irq_13
irq_primary_ata: jmp irq_14
irq_secondary_ata: jmp irq_15

; System call handler
global syscall_handler
syscall_handler:
    push qword 0    ; Dummy error code
    push qword 128  ; System call interrupt number
    jmp syscall_common

; Common exception handler
exception_common:
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
    
    ; Set up segments
    mov ax, 0x10    ; Kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Call C handler with pointer to register frame
    mov rdi, rsp
    call interrupt_handler
    
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
    
    ; Remove error code and interrupt number
    add rsp, 16
    
    ; Return from interrupt
    iretq

; Common IRQ handler
irq_common:
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
    
    ; Set up segments
    mov ax, 0x10    ; Kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Call C IRQ handler with pointer to register frame
    mov rdi, rsp
    call irq_handler
    
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
    
    ; Remove error code and interrupt number
    add rsp, 16
    
    ; Return from interrupt
    iretq

; System call common handler
syscall_common:
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
    
    ; Set up segments
    mov ax, 0x10    ; Kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Call syscall handler - syscall number in rax, args in other registers
    mov rdi, rsp    ; Register frame
    call handle_syscall
    
    ; Restore all registers (rax contains return value)
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
    ; Don't restore rax - it contains return value
    add rsp, 8
    
    ; Remove error code and interrupt number
    add rsp, 16
    
    ; Return from interrupt
    iretq

; IDT flush function
global idt_flush
idt_flush:
    lidt [rdi]
    ret
