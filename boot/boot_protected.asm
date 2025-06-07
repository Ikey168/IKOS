; IKOS Protected Mode Bootloader - Issue #3 Implementation
; Implements A20 line enable, GDT loading, and protected mode transition

%include "include/boot.inc"
%include "include/gdt.inc"

[BITS 16]                   ; Start in 16-bit real mode
[ORG 0x7C00]               ; BIOS loads us here

global start

start:
    ; === STAGE 1: REAL MODE INITIALIZATION ===
    cli                     ; Disable interrupts
    cld                     ; Clear direction flag
    
    ; Configure segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack
    mov sp, 0x7C00
    mov bp, sp
    
    sti                     ; Re-enable interrupts
    
    ; === STAGE 2: DISPLAY BOOT MESSAGE ===
    call init_video
    mov si, boot_banner
    call print_string
    
    ; === STAGE 3: ENABLE A20 LINE ===
    mov si, a20_msg
    call print_string
    call enable_a20
    call test_a20
    
    ; === STAGE 4: LOAD GLOBAL DESCRIPTOR TABLE ===
    mov si, gdt_msg
    call print_string
    call setup_gdt
    call load_gdt
    
    ; === STAGE 5: SWITCH TO PROTECTED MODE ===
    mov si, pmode_msg
    call print_string
    call enter_protected_mode
    
    ; Should not reach here
    jmp system_halt

; ============================================================================
; FUNCTION: init_video - Initialize video mode
; ============================================================================
init_video:
    pusha
    mov ah, 0x00
    mov al, 0x03            ; 80x25 color text mode
    int 0x10
    popa
    ret

; ============================================================================
; FUNCTION: print_string - Print null-terminated string
; Input: SI = pointer to string
; ============================================================================
print_string:
    pusha
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
    
.done:
    popa
    ret

; ============================================================================
; FUNCTION: enable_a20 - Enable A20 line using multiple methods
; ============================================================================
enable_a20:
    pusha
    
    ; Method 1: Try BIOS first
    mov ax, 0x2401
    int 0x15
    jnc .test_enabled
    
    ; Method 2: Keyboard controller method
    call enable_a20_kbd
    
    ; Method 3: Fast A20 method
    call enable_a20_fast
    
.test_enabled:
    popa
    ret

; ============================================================================
; FUNCTION: enable_a20_kbd - Enable A20 via keyboard controller
; ============================================================================
enable_a20_kbd:
    pusha
    
    ; Disable keyboard
    call kbd_wait_input
    mov al, 0xAD
    out 0x64, al
    
    ; Read output port
    call kbd_wait_input
    mov al, 0xD0
    out 0x64, al
    
    call kbd_wait_output
    in al, 0x60
    mov bl, al              ; Save current value
    
    ; Write output port with A20 enabled
    call kbd_wait_input
    mov al, 0xD1
    out 0x64, al
    
    call kbd_wait_input
    mov al, bl
    or al, 0x02             ; Enable A20
    out 0x60, al
    
    ; Enable keyboard
    call kbd_wait_input
    mov al, 0xAE
    out 0x64, al
    
    call kbd_wait_input
    
    popa
    ret

; ============================================================================
; FUNCTION: enable_a20_fast - Enable A20 via system control port
; ============================================================================
enable_a20_fast:
    pusha
    
    in al, 0x92
    or al, 0x02
    and al, 0xFE
    out 0x92, al
    
    popa
    ret

; ============================================================================
; FUNCTION: kbd_wait_input - Wait for keyboard controller input buffer empty
; ============================================================================
kbd_wait_input:
    in al, 0x64
    test al, 0x02
    jnz kbd_wait_input
    ret

; ============================================================================
; FUNCTION: kbd_wait_output - Wait for keyboard controller output buffer full
; ============================================================================
kbd_wait_output:
    in al, 0x64
    test al, 0x01
    jz kbd_wait_output
    ret

; ============================================================================
; FUNCTION: test_a20 - Test if A20 line is enabled
; ============================================================================
test_a20:
    pusha
    
    ; Set up test values
    mov ax, 0x0000
    mov ds, ax
    mov ax, 0xFFFF
    mov es, ax
    
    ; Write test pattern
    mov word [ds:0x7C00], 0x1234
    mov word [es:0x7C10], 0x5678
    
    ; Check if A20 is enabled
    cmp word [ds:0x7C00], 0x1234
    jne .a20_disabled
    cmp word [es:0x7C10], 0x5678
    jne .a20_disabled
    
    ; A20 is enabled
    mov si, a20_enabled_msg
    call print_string
    jmp .done
    
.a20_disabled:
    mov si, a20_disabled_msg
    call print_string
    
.done:
    ; Restore segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    popa
    ret

; ============================================================================
; FUNCTION: setup_gdt - Set up Global Descriptor Table
; ============================================================================
setup_gdt:
    pusha
    
    ; Clear GDT area
    mov di, gdt_start
    mov cx, 24              ; 3 entries * 8 bytes
    xor al, al
    rep stosb
    
    ; Setup NULL descriptor (already zero)
    
    ; Setup CODE descriptor
    mov di, gdt_code
    mov ax, 0xFFFF          ; Limit 0-15
    stosw
    mov ax, 0x0000          ; Base 0-15
    stosw
    mov al, 0x00            ; Base 16-23
    stosb
    mov al, GDT_CODE_ACCESS ; Access byte
    stosb
    mov al, GDT_CODE_GRANULARITY ; Granularity
    stosb
    mov al, 0x00            ; Base 24-31
    stosb
    
    ; Setup DATA descriptor
    mov di, gdt_data
    mov ax, 0xFFFF          ; Limit 0-15
    stosw
    mov ax, 0x0000          ; Base 0-15
    stosw
    mov al, 0x00            ; Base 16-23
    stosb
    mov al, GDT_DATA_ACCESS ; Access byte
    stosb
    mov al, GDT_DATA_GRANULARITY ; Granularity
    stosb
    mov al, 0x00            ; Base 24-31
    stosb
    
    popa
    ret

; ============================================================================
; FUNCTION: load_gdt - Load the Global Descriptor Table
; ============================================================================
load_gdt:
    pusha
    
    ; Set up GDT descriptor
    mov word [gdt_descriptor], 23    ; GDT size (3 entries * 8 - 1)
    mov dword [gdt_descriptor + 2], gdt_start ; GDT base address
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
    popa
    ret

; ============================================================================
; FUNCTION: enter_protected_mode - Switch to protected mode
; ============================================================================
enter_protected_mode:
    cli                     ; Disable interrupts
    
    ; Set PE bit in CR0
    mov eax, cr0
    or eax, 0x01
    mov cr0, eax
    
    ; Far jump to flush prefetch queue and load CS
    jmp CODE_SELECTOR:protected_mode_entry

[BITS 32]                   ; Now in 32-bit protected mode
protected_mode_entry:
    ; Set up segment registers for protected mode
    mov ax, DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up protected mode stack
    mov esp, 0x7C00
    
    ; Display success message (simplified for protected mode)
    ; In real implementation, would need protected mode video drivers
    
    ; Halt in protected mode
protected_mode_halt:
    hlt
    jmp protected_mode_halt

[BITS 16]                   ; Back to 16-bit for data and halt function

; ============================================================================
; FUNCTION: system_halt - Halt system with error message
; ============================================================================
system_halt:
    mov si, error_msg
    call print_string
    
.halt_loop:
    hlt
    jmp .halt_loop

; ============================================================================
; DATA SECTION
; ============================================================================

; Boot messages
boot_banner         db 'IKOS Bootloader - Protected Mode Transition', 0x0D, 0x0A
                    db '============================================', 0x0D, 0x0A, 0

a20_msg             db 'Enabling A20 line...', 0x0D, 0x0A, 0
a20_enabled_msg     db 'A20 line enabled successfully!', 0x0D, 0x0A, 0
a20_disabled_msg    db 'Warning: A20 line may not be enabled!', 0x0D, 0x0A, 0

gdt_msg             db 'Setting up Global Descriptor Table...', 0x0D, 0x0A, 0

pmode_msg           db 'Switching to Protected Mode...', 0x0D, 0x0A, 0

error_msg           db 'Error: Protected mode transition failed!', 0x0D, 0x0A, 0

; GDT Structure
align 8
gdt_start:
gdt_null:           ; NULL descriptor
    dq 0x0000000000000000

gdt_code:           ; CODE descriptor  
    dq 0x0000000000000000   ; Will be filled by setup_gdt

gdt_data:           ; DATA descriptor
    dq 0x0000000000000000   ; Will be filled by setup_gdt

gdt_end:

; GDT Descriptor
gdt_descriptor:
    dw 0                    ; GDT size
    dd 0                    ; GDT base address

; Pad bootloader to 510 bytes and add boot signature
times 510-($-$$) db 0
dw 0xAA55
