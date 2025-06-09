; IKOS Long Mode Bootloader - 64-bit Transition Implementation
; Implements transition from real mode -> protected mode -> long mode (64-bit)

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
    
    ; Display boot message
    mov si, boot_msg
    call print_string
    
    ; Enable A20 line
    call enable_a20
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
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
    
    ; === Task 1: Enable PAE (Physical Address Extension) ===
    mov eax, cr4
    or eax, 0x20          ; Set PAE bit (bit 5)
    mov cr4, eax
    
    ; === Task 2: Configure and load PML4 ===
    ; Set up page tables at 0x1000
    call setup_paging
    
    ; Load PML4 into CR3
    mov eax, 0x1000       ; PML4 table address
    mov cr3, eax
    
    ; === Task 3: Enable long mode in CPU ===
    ; Enable long mode (set LME in EFER MSR)
    mov ecx, 0xC0000080   ; IA32_EFER MSR
    rdmsr
    or eax, 0x100         ; Set LME (Long Mode Enable) bit 8
    wrmsr
    
    ; Enable paging to activate long mode
    mov eax, cr0
    or eax, 0x80000000    ; Set PG bit
    mov cr0, eax
    
    ; Far jump to 64-bit code segment
    jmp CODE64_SEL:long_mode

; ============================================================================
; 32-bit Functions (must be before [BITS 64])
; ============================================================================
[BITS 32]
setup_paging:
    push eax
    push ebx
    push ecx
    push edi
    
    ; Clear page table area (16KB)
    mov edi, 0x1000
    mov ecx, 0x1000       ; 4096 dwords (16KB)
    xor eax, eax
    cld
    rep stosd
    
    ; Set up PML4 (Page Map Level 4) at 0x1000
    mov dword [0x1000], 0x2003    ; Point to PDPT, present + writable
    
    ; Set up PDPT (Page Directory Pointer Table) at 0x2000
    mov dword [0x2000], 0x3003    ; Point to PD, present + writable
    
    ; Set up PD (Page Directory) at 0x3000
    ; Identity map first 2MB using 2MB pages
    mov dword [0x3000], 0x000083  ; 2MB page, present + writable + large page
    
    pop edi
    pop ecx
    pop ebx
    pop eax
    ret

[BITS 64]
long_mode:
    ; Set up 64-bit segment registers
    mov ax, DATA64_SEL
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    ; Display success message
    mov rsi, success_msg
    call print_string_64
    
    ; System is now in long mode - halt
    cli
    hlt
    jmp $

; ============================================================================
; 16-bit Functions
; ============================================================================
[BITS 16]
print_string:
    push ax
    push bx
    push cx
    push dx
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
    pop dx
    pop cx
    pop bx
    pop ax
    ret

enable_a20:
    push ax
    push bx
    ; Try BIOS method first
    mov ax, 0x2401
    int 0x15
    jnc .done
    
    ; Fast A20 method
    in al, 0x92
    or al, 0x02
    out 0x92, al
.done:
    pop bx
    pop ax
    ret

; ============================================================================
; 64-bit Functions
; ============================================================================
[BITS 64]
print_string_64:
    push rax
    push rbx
    push rcx
    push rdx
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
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

; ============================================================================
; Data Section
; ============================================================================
boot_msg     db 'IKOS: Real->Protected->Long Mode', 13, 10, 0
success_msg  db 'SUCCESS: Long Mode Enabled! 64-bit Ready', 0

; GDT for long mode transition
align 8
gdt_start:
    ; Null descriptor
    dq 0x0000000000000000
    
    ; 32-bit code segment
    dq 0x00CF9A000000FFFF
    
    ; 32-bit data segment  
    dq 0x00CF92000000FFFF
    
    ; 64-bit code segment
    dq 0x00209A0000000000
    
    ; 64-bit data segment
    dq 0x0000920000000000
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; Segment selectors
CODE_SEL    equ 0x08      ; 32-bit code
DATA_SEL    equ 0x10      ; 32-bit data
CODE64_SEL  equ 0x18      ; 64-bit code
DATA64_SEL  equ 0x20      ; 64-bit data

; Boot sector signature
times 510-($-$$) db 0
dw 0xAA55
