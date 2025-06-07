; IKOS ELF Kernel Loading Bootloader - Issue #4 Implementation
; Extends protected mode bootloader with ELF kernel loading capability

%include "include/boot.inc"
%include "include/gdt.inc" 
%include "include/elf.inc"

[BITS 16]                   ; Start in 16-bit real mode
[ORG 0x7C00]               ; BIOS loads us here

start:
    ; === STAGE 1: REAL MODE INITIALIZATION ===
    cli                     ; Disable interrupts
    cld                     ; Clear direction flag
    
    ; Store boot drive
    mov [boot_drive], dl
    
    ; Configure segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x7C00
    
    sti                     ; Re-enable interrupts
    
    ; === STAGE 2: DISPLAY BOOT MESSAGE ===
    call init_video
    mov si, boot_banner
    call print_string
    
    ; === STAGE 3: ENABLE A20 LINE ===
    mov si, a20_msg
    call print_string
    call enable_a20
    
    ; === STAGE 4: SETUP GDT ===
    mov si, gdt_msg
    call print_string
    call setup_gdt
    
    ; === STAGE 5: READ KERNEL FROM DISK ===
    mov si, disk_msg
    call print_string
    call read_kernel_from_disk
    jc .disk_error
    
    ; === STAGE 6: PARSE ELF HEADERS ===
    mov si, elf_msg
    call print_string
    call parse_elf_header
    jc .elf_error
    
    ; === STAGE 7: ENTER PROTECTED MODE ===
    mov si, pmode_msg
    call print_string
    call enter_protected_mode
    
    ; Should not reach here
    jmp system_halt

.disk_error:
    mov si, disk_error_msg
    call print_string
    jmp system_halt

.elf_error:
    mov si, elf_error_msg
    call print_string
    jmp system_halt

; ============================================================================
; INCLUDE FILES
; ============================================================================
%include "boot/disk.asm"

; ============================================================================
; FUNCTION: init_video
; ============================================================================
init_video:
    pusha
    mov ah, 0x00
    mov al, 0x03            ; 80x25 color text mode
    int 0x10
    popa
    ret

; ============================================================================
; FUNCTION: print_string
; ============================================================================
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

; ============================================================================
; FUNCTION: enable_a20
; ============================================================================
enable_a20:
    pusha
    
    ; Try BIOS method first
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

; ============================================================================
; FUNCTION: setup_gdt
; ============================================================================
setup_gdt:
    pusha
    
    ; Setup GDT descriptor
    mov word [gdt_descriptor], 23        ; GDT size (3 entries * 8 - 1)
    mov dword [gdt_descriptor + 2], gdt_start ; GDT base
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
    popa
    ret

; ============================================================================
; FUNCTION: read_kernel_from_disk
; ============================================================================
read_kernel_from_disk:
    pusha
    
    ; Set buffer to kernel buffer area
    mov ax, KERNEL_BUFFER >> 4  ; Segment for kernel buffer
    mov es, ax
    mov bx, 0                   ; Offset 0
    
    ; Read kernel sectors
    mov eax, KERNEL_START_SECTOR
    call lba_to_chs
    
    mov byte [sectors_to_read], 20  ; Read 20 sectors (10KB) for full ELF
    call read_sectors
    
    popa
    ret

; ============================================================================
; FUNCTION: parse_elf_header
; ============================================================================
parse_elf_header:
    pusha
    
    ; Point to ELF header in kernel buffer
    mov ax, KERNEL_BUFFER >> 4
    mov es, ax
    mov bx, 0
    
    ; Check ELF magic number
    mov al, [es:bx + 0]         ; First byte should be 0x7F
    cmp al, 0x7F
    jne .invalid_elf
    
    mov al, [es:bx + 1]         ; Second byte should be 'E'
    cmp al, 'E'
    jne .invalid_elf
    
    mov al, [es:bx + 2]         ; Third byte should be 'L'
    cmp al, 'L'
    jne .invalid_elf
    
    mov al, [es:bx + 3]         ; Fourth byte should be 'F'
    cmp al, 'F'
    jne .invalid_elf
    
    ; ELF header is valid
    mov si, elf_valid_msg
    call print_string
    
    ; Extract entry point (at offset 24)
    mov eax, [es:bx + ELF_E_ENTRY]
    mov [kernel_entry_point], eax
    
    mov si, entry_point_msg
    call print_string
    call print_hex_dword
    mov si, newline
    call print_string
    
    clc                         ; Clear carry (success)
    jmp .done
    
.invalid_elf:
    mov si, elf_invalid_msg
    call print_string
    stc                         ; Set carry (error)
    
.done:
    popa
    ret

; ============================================================================
; FUNCTION: print_hex_dword
; Input: EAX = 32-bit value to print
; ============================================================================
print_hex_dword:
    pusha
    mov cx, 8                   ; 8 hex digits for 32-bit
    
.loop:
    rol eax, 4                  ; Rotate left by 4 bits
    mov edx, eax                ; Save EAX
    and al, 0x0F                ; Mask lower 4 bits
    add al, '0'                 ; Convert to ASCII
    cmp al, '9'
    jle .print
    add al, 7                   ; Convert A-F
    
.print:
    mov ah, 0x0E
    int 0x10
    mov eax, edx                ; Restore EAX
    loop .loop
    
    popa
    ret

; ============================================================================
; FUNCTION: enter_protected_mode
; ============================================================================
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
    mov esp, 0x9000             ; Protected mode stack
    
    ; In a real implementation, we would:
    ; 1. Copy kernel segments to their virtual addresses
    ; 2. Setup paging if needed
    ; 3. Jump to kernel entry point
    
    ; For now, just halt
.halt:
    hlt
    jmp .halt

[BITS 16]

; ============================================================================
; FUNCTION: system_halt
; ============================================================================
system_halt:
    mov si, halt_msg
    call print_string
.halt:
    hlt
    jmp .halt

; ============================================================================
; DATA SECTION
; ============================================================================

; Boot messages
boot_banner         db 'IKOS Bootloader - ELF Kernel Loading', 0x0D, 0x0A
                    db '====================================', 0x0D, 0x0A, 0
a20_msg             db 'Enabling A20 line...', 0x0D, 0x0A, 0
gdt_msg             db 'Setting up GDT...', 0x0D, 0x0A, 0
disk_msg            db 'Reading kernel from disk...', 0x0D, 0x0A, 0
elf_msg             db 'Parsing ELF headers...', 0x0D, 0x0A, 0
pmode_msg           db 'Entering protected mode...', 0x0D, 0x0A, 0

; Error messages
disk_error_msg      db 'ERROR: Failed to read kernel from disk!', 0x0D, 0x0A, 0
elf_error_msg       db 'ERROR: Invalid ELF format!', 0x0D, 0x0A, 0
halt_msg            db 'System halted.', 0x0D, 0x0A, 0

; ELF parsing messages
elf_valid_msg       db 'Valid ELF header found!', 0x0D, 0x0A, 0
elf_invalid_msg     db 'Invalid ELF header!', 0x0D, 0x0A, 0
entry_point_msg     db 'Kernel entry point: 0x', 0
newline             db 0x0D, 0x0A, 0

; GDT
align 8
gdt_start:
    ; NULL descriptor
    dq 0x0000000000000000
    
    ; CODE descriptor (0x08)
    dw 0xFFFF, 0x0000       ; Limit, Base low
    db 0x00, 0x9A, 0xCF, 0x00 ; Base mid, Access, Granularity, Base high
    
    ; DATA descriptor (0x10)
    dw 0xFFFF, 0x0000       ; Limit, Base low
    db 0x00, 0x92, 0xCF, 0x00 ; Base mid, Access, Granularity, Base high

; GDT Descriptor
gdt_descriptor:
    dw 0                    ; Size
    dd 0                    ; Base

; Kernel information
kernel_entry_point      dd 0            ; Kernel entry point address

; Include the disk reading constants and variables from disk.asm
; (They are included via %include above)

; Boot signature
times 510-($-$$) db 0
dw 0xAA55
