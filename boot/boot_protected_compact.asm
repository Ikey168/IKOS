; IKOS Protected Mode Bootloader - Compact Implementation
; Implements A20 line enable, GDT loading, and protected mode transition

%include "include/boot.inc"
%include "include/gdt.inc"

[BITS 16]                   ; Start in 16-bit real mode
[ORG 0x7C00]               ; BIOS loads us here

start:
    ; === REAL MODE INITIALIZATION ===
    cli                     ; Disable interrupts
    cld                     ; Clear direction flag
    
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    
    sti                     ; Re-enable interrupts
    
    ; === DISPLAY BOOT MESSAGE ===
    mov si, boot_msg
    call print_string
    
    ; === ENABLE A20 LINE ===
    call enable_a20
    
    ; === SETUP AND LOAD GDT ===
    call setup_gdt
    
    ; === ENTER PROTECTED MODE ===
    call enter_protected_mode

; ============================================================================
; FUNCTIONS
; ============================================================================

; Print string function
print_string:
    pusha
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

; Enable A20 line
enable_a20:
    pusha
    
    ; Try BIOS first
    mov ax, 0x2401
    int 0x15
    jnc .done
    
    ; Fast A20 method
    in al, 0x92
    or al, 0x02
    out 0x92, al
    
.done:
    popa
    ret

; Setup GDT
setup_gdt:
    pusha
    
    ; Setup GDT descriptor
    mov word [gdt_descriptor], 23        ; GDT size (3 entries * 8 - 1)
    mov dword [gdt_descriptor + 2], gdt_start ; GDT base
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
    popa
    ret

; Enter protected mode
enter_protected_mode:
    cli
    
    ; Set PE bit in CR0
    mov eax, cr0
    or eax, 0x01
    mov cr0, eax
    
    ; Far jump to protected mode
    jmp CODE_SELECTOR:protected_mode_entry

[BITS 32]
protected_mode_entry:
    ; Setup segment registers
    mov ax, DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x7C00
    
    ; Halt in protected mode
.halt:
    hlt
    jmp .halt

[BITS 16]

; ============================================================================
; DATA
; ============================================================================

boot_msg    db 'IKOS: A20->GDT->PMODE', 0x0D, 0x0A, 0

; GDT
align 8
gdt_start:
    ; NULL descriptor
    dq 0x0000000000000000
    
    ; CODE descriptor (0x08)
    dw 0xFFFF       ; Limit 0-15
    dw 0x0000       ; Base 0-15
    db 0x00         ; Base 16-23
    db 0x9A         ; Access: Present, Ring 0, Executable, Readable
    db 0xCF         ; Granularity: 4KB, 32-bit, Limit 16-19
    db 0x00         ; Base 24-31
    
    ; DATA descriptor (0x10)
    dw 0xFFFF       ; Limit 0-15
    dw 0x0000       ; Base 0-15
    db 0x00         ; Base 16-23
    db 0x92         ; Access: Present, Ring 0, Data, Writable
    db 0xCF         ; Granularity: 4KB, 32-bit, Limit 16-19
    db 0x00         ; Base 24-31

; GDT Descriptor
gdt_descriptor:
    dw 0            ; Size
    dd 0            ; Base

; Boot signature
times 510-($-$$) db 0
dw 0xAA55
