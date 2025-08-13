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
; Paging Constants
; ============================================================================
PML4_BASE       equ 0x1000    ; Page Map Level 4 base address
PDPT_BASE       equ 0x2000    ; Page Directory Pointer Table base address  
PD_BASE         equ 0x3000    ; Page Directory base address
PT_BASE         equ 0x4000    ; Page Table base address

; Page flags
PAGE_PRESENT    equ 0x01      ; Page is present in memory
PAGE_WRITABLE   equ 0x02      ; Page is writable
PAGE_USER       equ 0x04      ; User mode access allowed
PAGE_LARGE      equ 0x80      ; Large page (2MB for PD entries)

; ============================================================================
; 32-bit Functions (must be before [BITS 64])
; ============================================================================
[BITS 32]
setup_paging:
    push eax
    push ebx
    push ecx
    push edi
    
    ; Clear page table area (20KB for PML4, PDPT, PD, PT, and extra space)
    mov edi, PML4_BASE
    mov ecx, 0x1400       ; 5120 dwords (20KB)
    xor eax, eax
    cld
    rep stosd
    
    ; === Set up PML4 (Page Map Level 4) at 0x1000 ===
    ; First entry points to PDPT
    mov dword [PML4_BASE], PDPT_BASE | PAGE_PRESENT | PAGE_WRITABLE
    mov dword [PML4_BASE + 4], 0x00000000  ; Clear upper 32 bits
    
    ; === Set up PDPT (Page Directory Pointer Table) at 0x2000 ===
    ; First entry points to PD
    mov dword [PDPT_BASE], PD_BASE | PAGE_PRESENT | PAGE_WRITABLE
    mov dword [PDPT_BASE + 4], 0x00000000  ; Clear upper 32 bits
    
    ; === Set up PD (Page Directory) at 0x3000 ===
    ; Map first 4MB using 2MB pages (identity mapping)
    ; Entry 0: 0x000000 - 0x1FFFFF (first 2MB)
    mov dword [PD_BASE], 0x000000 | PAGE_PRESENT | PAGE_WRITABLE | PAGE_LARGE
    mov dword [PD_BASE + 4], 0x00000000
    
    ; Entry 1: 0x200000 - 0x3FFFFF (second 2MB)  
    mov dword [PD_BASE + 8], 0x200000 | PAGE_PRESENT | PAGE_WRITABLE | PAGE_LARGE
    mov dword [PD_BASE + 12], 0x00000000
    
    ; === Optional: Set up PT for fine-grained mapping ===
    ; If you need 4KB pages instead of 2MB pages, uncomment the following:
    ; Replace first PD entry to point to PT instead of using large page
    ; mov dword [PD_BASE], PT_BASE | PAGE_PRESENT | PAGE_WRITABLE
    ; Then map individual 4KB pages in PT_BASE
    
    ; === Map kernel sections into virtual memory ===
    ; Current setup provides identity mapping for first 4MB:
    ; 0x000000-0x1FFFFF: Boot code, GDT, IDT, page tables
    ; 0x200000-0x3FFFFF: Kernel code and data sections
    
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
