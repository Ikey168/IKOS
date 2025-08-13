; IKOS Long Mode Bootloader with Debugging - Compact Version
; This is a compact version with minimal debugging for boot sector constraints

[BITS 16]
[ORG 0x7C00]

start:
    cli
    cld
    
    ; Initialize segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    
    sti
    
    ; Quick serial init for debugging
    call quick_serial_init
    
    ; Debug: Boot start
    mov si, msg_start
    call debug_out
    
    ; Display boot message
    mov si, boot_msg
    call print_string
    
    ; Enable A20 line
    call enable_a20
    mov si, msg_a20
    call debug_out
    
    ; Load GDT
    lgdt [gdt_descriptor]
    mov si, msg_gdt
    call debug_out
    
    ; Enter protected mode
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEL:protected_mode

[BITS 32]
protected_mode:
    ; Set up 32-bit segment registers
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9000
    
    ; Enable PAE
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax
    
    ; Set up page tables
    call setup_paging
    
    ; Load PML4 into CR3
    mov eax, 0x1000
    mov cr3, eax
    
    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x100
    wrmsr
    
    ; Enable paging
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    
    jmp CODE64_SEL:long_mode

; Compact debug functions
[BITS 16]
quick_serial_init:
    push ax
    push dx
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 0x03
    out dx, al
    mov dx, 0x3F9
    mov al, 0x00
    out dx, al
    mov dx, 0x3FB
    mov al, 0x03
    out dx, al
    pop dx
    pop ax
    ret

debug_out:
    push ax
    push si
.loop:
    lodsb
    test al, al
    jz .done
    call debug_char
    jmp .loop
.done:
    mov al, 13
    call debug_char
    mov al, 10
    call debug_char
    pop si
    pop ax
    ret

debug_char:
    push dx
    mov dx, 0x3FD
.wait:
    in al, dx
    test al, 0x20
    jz .wait
    pop dx
    push dx
    mov dx, 0x3F8
    out dx, al
    pop dx
    ret

print_string:
    push ax
    push bx
    push si
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    pop si
    pop bx
    pop ax
    ret

enable_a20:
    push ax
    in al, 0x92
    or al, 2
    out 0x92, al
    pop ax
    ret

; Paging setup (compact version)
[BITS 32]
setup_paging:
    push eax
    push ecx
    push edi
    
    ; Clear page tables
    mov edi, 0x1000
    mov ecx, 0x1000
    xor eax, eax
    rep stosd
    
    ; Set up page tables
    mov dword [0x1000], 0x2003
    mov dword [0x2000], 0x3003
    mov dword [0x3000], 0x000083
    mov dword [0x3008], 0x200083
    
    pop edi
    pop ecx
    pop eax
    ret

[BITS 64]
long_mode:
    mov ax, DATA64_SEL
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    mov rsi, success_msg
    call print_string_64
    
    cli
    hlt
    jmp $

[BITS 64]
print_string_64:
    push rax
    push rbx
    push rsi
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    pop rsi
    pop rbx
    pop rax
    ret

; Data section
boot_msg     db 'IKOS: Debug-Enabled Boot', 13, 10, 0
success_msg  db 'SUCCESS: Long Mode + Debug Ready!', 0

; Debug messages (compact)
msg_start    db 'BOOT', 0
msg_a20      db 'A20', 0
msg_gdt      db 'GDT', 0

; GDT
align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
    dq 0x00209A0000000000
    dq 0x0000920000000000
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEL    equ 0x08
DATA_SEL    equ 0x10
CODE64_SEL  equ 0x18
DATA64_SEL  equ 0x20

times 510-($-$$) db 0
dw 0xAA55
