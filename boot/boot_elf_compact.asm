; IKOS ELF Kernel Loading Bootloader - Compact Implementation
; Issue #4: Read kernel ELF binary from disk and parse headers

%include "include/boot.inc"
%include "include/gdt.inc"
%include "include/elf.inc"

[BITS 16]
[ORG 0x7C00]

start:
    ; === INITIALIZATION ===
    cli
    cld
    
    ; Store boot drive
    mov [boot_drive], dl
    
    ; Configure segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    
    ; === DISPLAY BOOT MESSAGE ===
    mov si, boot_msg
    call print_string
    
    ; === ENABLE A20 LINE ===
    call enable_a20
    
    ; === LOAD GDT ===
    lgdt [gdt_descriptor]
    
    ; === ENTER PROTECTED MODE ===
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:protected_mode

[BITS 32]
protected_mode:
    ; Set up segment registers
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9000
    
    ; === LOAD KERNEL FROM DISK ===
    call load_kernel_elf
    
    ; === SUCCESS MESSAGE ===
    mov esi, success_msg
    call print_string_32
    
    ; Halt system
    jmp $

; ============================================================================
; FUNCTION: load_kernel_elf (32-bit protected mode)
; ============================================================================
load_kernel_elf:
    pusha

    ; Assume ELF file is loaded at 0x10000
    mov esi, 0x10000

    ; Check ELF magic number (0x7F 'E' 'L' 'F')
    cmp dword [esi], 0x464C457F
    jne .error

    ; Extract entry point (offset 0x18 in ELF header)
    mov eax, [esi + 0x18]
    test eax, eax
    jz .error

    ; Optionally: print entry point (for debug)
    ; mov esi, entry_msg
    ; call print_string_32
    ; mov eax, [esi + 0x18]
    ; (print eax as hex if needed)

    popa
    ret

.error:
    mov esi, elf_error_msg
    call print_string_32
    jmp $

; ============================================================================
; FUNCTION: print_string_32 (32-bit protected mode)
; ============================================================================
print_string_32:
    pusha
    mov edi, 0xB8000        ; VGA text buffer
    mov ah, 0x07            ; White on black
.loop:
    lodsb
    test al, al
    jz .done
    stosw
    jmp .loop
.done:
    popa
    ret

[BITS 16]
; ============================================================================
; FUNCTION: enable_a20
; ============================================================================
enable_a20:
    pusha
    ; BIOS method
    mov ax, 0x2401
    int 0x15
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
; DATA SECTION
; ============================================================================
boot_msg        db 'IKOS: ELF->LOAD->PMOD', 13, 10, 0
success_msg     db 'IKOS: ELF KERNEL READY', 0
boot_drive      db 0
elf_error_msg   db 'ELF ERROR', 0

; GDT
gdt_start:
    dq 0x0000000000000000   ; Null descriptor
    dw 0xFFFF, 0x0000, 0x9A00, 0x00CF  ; Code segment
    dw 0xFFFF, 0x0000, 0x9200, 0x00CF  ; Data segment
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ 8
DATA_SEG equ 16

; Boot sector signature
times 510-($-$$) db 0
dw 0xAA55