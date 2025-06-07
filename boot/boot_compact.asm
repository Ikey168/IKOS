; IKOS Compact Enhanced Bootloader - Real Mode Initialization
; Optimized version that fits in 512 bytes

[BITS 16]                   ; 16-bit real mode
[ORG 0x7C00]               ; BIOS loads us here

start:
    ; === DISABLE INTERRUPTS AND CLEAR REGISTERS ===
    cli                     ; Clear interrupts
    cld                     ; Clear direction flag
    
    ; === CONFIGURE SEGMENT REGISTERS ===
    xor ax, ax              ; Clear AX (AX = 0)
    mov ds, ax              ; Data Segment = 0
    mov es, ax              ; Extra Segment = 0
    mov fs, ax              ; FS Segment = 0  
    mov gs, ax              ; GS Segment = 0
    mov ss, ax              ; Stack Segment = 0
    
    ; === SET UP STACK POINTER ===
    mov sp, 0x7C00          ; Stack pointer at bootloader base
    mov bp, sp              ; Base pointer
    
    ; === RE-ENABLE INTERRUPTS ===
    sti                     ; Safe to enable interrupts now
    
    ; === INITIALIZE VIDEO MODE ===
    mov ah, 0x00            ; Set video mode function
    mov al, 0x03            ; 80x25 16-color text mode
    int 0x10                ; BIOS video interrupt
    
    ; === DISPLAY BOOT MESSAGE ===
    mov si, boot_msg
    call print_string
    
    ; === DETECT MEMORY ===
    call detect_memory
    
    ; === DISPLAY COMPLETION ===
    mov si, complete_msg
    call print_string
    
    ; === HALT SYSTEM ===
    hlt

; ============================================================================
; FUNCTION: print_string
; ============================================================================
print_string:
    pusha
    mov ah, 0x0E            ; Teletype function
    
.loop:
    lodsb                   ; Load character
    test al, al             ; Check for null
    jz .done                ; Jump if zero
    int 0x10                ; Print character
    jmp .loop               ; Continue
    
.done:
    popa
    ret

; ============================================================================
; FUNCTION: print_hex
; ============================================================================
print_hex:
    pusha
    mov cx, 4               ; 4 hex digits
    
.loop:
    rol ax, 4               ; Rotate left by 4 bits
    mov dx, ax              ; Save AX
    and al, 0x0F            ; Mask lower 4 bits
    add al, '0'             ; Convert to ASCII
    cmp al, '9'             ; Check if > 9
    jle .print              ; Jump if <= 9
    add al, 7               ; Convert A-F
    
.print:
    mov ah, 0x0E            ; Teletype function
    int 0x10                ; Print character
    mov ax, dx              ; Restore AX
    loop .loop              ; Continue
    
    popa
    ret

; ============================================================================
; FUNCTION: detect_memory
; ============================================================================
detect_memory:
    pusha
    
    ; Get conventional memory size
    int 0x12                ; Returns KB in AX
    mov [conv_mem], ax
    
    mov si, conv_msg
    call print_string
    mov ax, [conv_mem]
    call print_hex
    mov si, kb_msg
    call print_string
    
    ; Get extended memory
    mov ah, 0x88
    clc
    int 0x15
    jc .done
    mov [ext_mem], ax
    
    mov si, ext_msg
    call print_string
    mov ax, [ext_mem]
    call print_hex
    mov si, kb_msg
    call print_string
    
.done:
    popa
    ret

; ============================================================================
; DATA SECTION
; ============================================================================
boot_msg        db 'IKOS Bootloader - Real Mode Init', 0x0D, 0x0A, 0
conv_msg        db 'Conv: ', 0
ext_msg         db 'Ext: ', 0
kb_msg          db 'KB', 0x0D, 0x0A, 0
complete_msg    db 'Init complete!', 0x0D, 0x0A, 0

; Memory storage
conv_mem        dw 0
ext_mem         dw 0

; Pad to 510 bytes and add boot signature
times 510-($-$$) db 0
dw 0xAA55
