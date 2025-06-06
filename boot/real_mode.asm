; Real Mode initialization code
; Sets up the CPU in 16-bit real mode, initializes segment registers,
; and retrieves the BIOS memory map.

BITS 16
ORG 0x7C00

; Buffer location for BIOS memory map entries
MEM_MAP_BUFFER  equ 0x0500
ENTRY_SIZE      equ 24
STACK_TOP       equ 0x7C00

start:
    cli                 ; Disable interrupts during setup

    ; Zero all segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov sp, STACK_TOP    ; Set up stack pointer

    sti                 ; Re-enable interrupts

    ; Retrieve BIOS memory map using INT 0x15, E820h
    mov di, MEM_MAP_BUFFER ; Destination buffer in low memory
    xor ebx, ebx         ; Continuation value set to zero on first call
    mov eax, 0xE820
    mov edx, 0x534D4150  ; 'SMAP'
    mov ecx, ENTRY_SIZE  ; Buffer size
next_e820:
    int 0x15
    jc done_e820
    add di, ENTRY_SIZE
    cmp ebx, 0
    jne next_e820

done_e820:
    cli
    hlt                 ; Halt the CPU after collecting the memory map

    times 510-($-$$) db 0
    dw 0xAA55            ; Boot signature
