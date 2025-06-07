; IKOS Enhanced Bootloader - Real Mode Initialization
; Comprehensive real mode setup with detailed memory mapping and system initialization

%include "include/boot.inc"

[BITS 16]                   ; 16-bit real mode
[ORG 0x7C00]               ; BIOS loads us here

global start

start:
    ; === STAGE 1: DISABLE INTERRUPTS AND CLEAR REGISTERS ===
    cli                     ; Clear interrupts for safe initialization
    cld                     ; Clear direction flag (forward string operations)
    
    ; === STAGE 2: CONFIGURE SEGMENT REGISTERS ===
    ; Set up flat memory model with all segments pointing to 0
    xor ax, ax              ; Clear AX (AX = 0)
    mov ds, ax              ; Data Segment = 0
    mov es, ax              ; Extra Segment = 0
    mov fs, ax              ; FS Segment = 0  
    mov gs, ax              ; GS Segment = 0
    mov ss, ax              ; Stack Segment = 0
    
    ; === STAGE 3: SET UP STACK POINTER ===
    ; Configure stack to grow downward from bootloader location
    mov sp, 0x7C00          ; Stack pointer at bootloader base
    mov bp, sp              ; Base pointer for stack frame operations
    
    ; === STAGE 4: RE-ENABLE INTERRUPTS ===
    sti                     ; Safe to enable interrupts now
    
    ; === STAGE 5: INITIALIZE VIDEO MODE ===
    call init_video
    
    ; === STAGE 6: DISPLAY BOOT MESSAGE ===
    mov si, boot_banner
    call print_string
    
    ; === STAGE 7: MEMORY DETECTION AND MAPPING ===
    call detect_memory
    call init_memory_map
    
    ; === STAGE 8: SYSTEM INFORMATION DISPLAY ===
    call display_system_info
    
    ; === STAGE 9: COMPLETION MESSAGE ===
    mov si, init_complete_msg
    call print_string
    
    ; === STAGE 10: HALT SYSTEM ===
    ; In a real OS, this would jump to kernel loading
    call system_halt

; ============================================================================
; FUNCTION: init_video
; Initialize video mode and clear screen
; ============================================================================
init_video:
    pusha
    
    ; Set video mode to 80x25 color text mode
    mov ah, 0x00            ; Set video mode function
    mov al, 0x03            ; 80x25 16-color text mode
    int 0x10                ; BIOS video interrupt
    
    ; Clear screen
    mov ah, 0x06            ; Scroll window function
    mov al, 0x00            ; Clear entire window
    mov bh, 0x07            ; White on black attribute
    mov cx, 0x0000          ; Upper left corner (0,0)
    mov dx, 0x184F          ; Lower right corner (24,79)
    int 0x10
    
    ; Set cursor to top-left
    mov ah, 0x02            ; Set cursor position
    mov bh, 0x00            ; Page 0
    mov dx, 0x0000          ; Row 0, Column 0
    int 0x10
    
    popa
    ret

; ============================================================================
; FUNCTION: print_string
; Print null-terminated string with color support
; Input: SI = pointer to string
; ============================================================================
print_string:
    pusha
    mov ah, 0x0E            ; Teletype function
    mov bh, 0x00            ; Page 0
    mov bl, 0x07            ; Light gray text
    
.loop:
    lodsb                   ; Load character from [SI] into AL
    test al, al             ; Check for null terminator
    jz .done                ; Jump if zero (end of string)
    int 0x10                ; Print character
    jmp .loop               ; Continue with next character
    
.done:
    popa
    ret

; ============================================================================
; FUNCTION: print_hex
; Print 16-bit value in hexadecimal
; Input: AX = value to print
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
    loop .loop              ; Continue for all digits
    
    popa
    ret

; ============================================================================
; FUNCTION: detect_memory
; Detect system memory using BIOS functions
; ============================================================================
detect_memory:
    pusha
    
    mov si, memory_detect_msg
    call print_string
    
    ; Get conventional memory size (INT 0x12)
    int 0x12                ; Returns KB in AX
    mov [conventional_memory], ax
    
    mov si, conventional_msg
    call print_string
    mov ax, [conventional_memory]
    call print_hex
    mov si, kb_suffix
    call print_string
    
    ; Get extended memory size (INT 0x15, AH=0x88)
    mov ah, 0x88
    clc                     ; Clear carry flag
    int 0x15
    jc .no_extended         ; Jump if carry set (error)
    mov [extended_memory], ax
    
    mov si, extended_msg
    call print_string
    mov ax, [extended_memory]
    call print_hex
    mov si, kb_suffix
    call print_string
    jmp .done
    
.no_extended:
    mov si, no_extended_msg
    call print_string
    
.done:
    popa
    ret

; ============================================================================
; FUNCTION: init_memory_map
; Create detailed memory map using BIOS INT 0x15, AX=0xE820
; ============================================================================
init_memory_map:
    pusha
    
    mov si, memory_map_msg
    call print_string
    
    ; Initialize for memory map detection
    mov di, memory_map_buffer   ; Buffer for memory map entries
    xor ebx, ebx               ; Continuation value (start with 0)
    xor bp, bp                 ; Entry counter
    mov edx, 0x534D4150        ; "SMAP" signature
    
.loop:
    mov eax, 0xE820            ; Function code
    mov ecx, 24                ; Buffer size for each entry
    int 0x15                   ; Call BIOS
    
    ; Check for errors
    jc .error                  ; Carry flag indicates error
    cmp eax, 0x534D4150        ; Check if signature returned
    jne .error                 ; Different signature = error
    
    ; Process this entry
    inc bp                     ; Increment entry counter
    add di, 24                 ; Move to next entry in buffer
    
    ; Check if more entries available
    test ebx, ebx              ; EBX = 0 means last entry
    jz .done                   ; No more entries
    
    ; Prevent buffer overflow
    cmp bp, 8                  ; Maximum 8 entries
    jge .done                  ; Too many entries, stop
    
    jmp .loop                  ; Get next entry
    
.done:
    mov si, memory_map_success_msg
    call print_string
    mov ax, bp                 ; Number of entries
    add al, '0'                ; Convert to ASCII
    mov ah, 0x0E
    int 0x10                   ; Print count
    mov si, entries_suffix
    call print_string
    jmp .exit
    
.error:
    mov si, memory_map_error_msg
    call print_string
    
.exit:
    popa
    ret

; ============================================================================
; FUNCTION: display_system_info
; Display comprehensive system information
; ============================================================================
display_system_info:
    pusha
    
    mov si, system_info_msg
    call print_string
    
    ; Display segment register values
    mov si, segments_msg
    call print_string
    
    ; Show DS register
    mov si, ds_msg
    call print_string
    mov ax, ds
    call print_hex
    mov si, newline
    call print_string
    
    ; Show ES register
    mov si, es_msg
    call print_string
    mov ax, es
    call print_hex
    mov si, newline
    call print_string
    
    ; Show SS register  
    mov si, ss_msg
    call print_string
    mov ax, ss
    call print_hex
    mov si, newline
    call print_string
    
    ; Show stack pointer
    mov si, sp_msg
    call print_string
    mov ax, sp
    call print_hex
    mov si, newline
    call print_string
    
    popa
    ret

; ============================================================================
; FUNCTION: system_halt
; Halt the system safely
; ============================================================================
system_halt:
    mov si, halt_msg
    call print_string
    
.halt_loop:
    hlt                     ; Halt until next interrupt
    jmp .halt_loop          ; In case of spurious interrupt

; ============================================================================
; DATA SECTION
; ============================================================================

; Boot messages
boot_banner         db 'IKOS Operating System Bootloader v1.0', 0x0D, 0x0A
                    db '======================================', 0x0D, 0x0A
                    db 'Real Mode Initialization Starting...', 0x0D, 0x0A, 0x0A, 0

memory_detect_msg   db 'Detecting system memory...', 0x0D, 0x0A, 0
conventional_msg    db 'Conventional Memory: ', 0
extended_msg        db 'Extended Memory: ', 0
no_extended_msg     db 'Extended Memory: Not Available', 0x0D, 0x0A, 0
kb_suffix           db ' KB', 0x0D, 0x0A, 0

memory_map_msg      db 'Creating memory map...', 0x0D, 0x0A, 0
memory_map_success_msg db 'Memory map created successfully! Entries: ', 0
memory_map_error_msg db 'Error: Could not create memory map!', 0x0D, 0x0A, 0
entries_suffix      db ' entries', 0x0D, 0x0A, 0

system_info_msg     db 0x0D, 0x0A, 'System Information:', 0x0D, 0x0A
                    db '==================', 0x0D, 0x0A, 0
segments_msg        db 'Segment Registers:', 0x0D, 0x0A, 0
ds_msg              db '  DS: 0x', 0
es_msg              db '  ES: 0x', 0
ss_msg              db '  SS: 0x', 0
sp_msg              db '  SP: 0x', 0

init_complete_msg   db 0x0D, 0x0A, 'Real mode initialization completed successfully!', 0x0D, 0x0A
                    db 'All tasks completed:', 0x0D, 0x0A
                    db '  [X] Configure segment registers', 0x0D, 0x0A
                    db '  [X] Set up the stack pointer', 0x0D, 0x0A  
                    db '  [X] Prepare memory map for the bootloader', 0x0D, 0x0A, 0x0A, 0

halt_msg            db 'System halted. Ready for kernel loading...', 0x0D, 0x0A, 0
newline             db 0x0D, 0x0A, 0

; Memory information storage
conventional_memory dw 0        ; Conventional memory in KB
extended_memory     dw 0        ; Extended memory in KB

; Memory map buffer (8 entries max, 24 bytes each = 192 bytes)
memory_map_buffer:
    times 192 db 0

; Pad bootloader to 510 bytes and add boot signature
times 510-($-$$) db 0
dw 0xAA55                       ; Boot sector signature
