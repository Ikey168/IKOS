; IKOS Bootloader - Real Mode Initialization
; This bootloader sets up the CPU in real mode, configures segment registers,
; and initializes memory mapping for the IKOS operating system.

[BITS 16]                   ; Tell assembler we're working in 16-bit real mode
[ORG 0x7C00]               ; BIOS loads bootloader at address 0x7C00

start:
    ; Clear interrupts during initialization
    cli
    
    ; Configure segment registers
    ; Set all segment registers to 0 for a flat memory model in real mode
    xor ax, ax              ; Clear AX register (set to 0)
    mov ds, ax              ; Data Segment = 0
    mov es, ax              ; Extra Segment = 0
    mov fs, ax              ; FS Segment = 0
    mov gs, ax              ; GS Segment = 0
    mov ss, ax              ; Stack Segment = 0
    
    ; Set up the stack pointer
    ; Place stack at 0x7C00 (just below bootloader) and grow downward
    mov sp, 0x7C00          ; Stack Pointer starts at bootloader address
    
    ; Re-enable interrupts after segment setup
    sti
    
    ; Display initialization message
    mov si, init_msg
    call print_string
    
    ; Initialize memory mapping
    call init_memory_map
    
    ; Display completion message
    mov si, complete_msg
    call print_string
    
    ; Halt system (placeholder for kernel loading)
    hlt

; Function: print_string
; Prints a null-terminated string to screen using BIOS interrupt
; Input: SI = pointer to string
print_string:
    pusha                   ; Save all registers
    mov ah, 0x0E           ; BIOS teletype function
.repeat:
    lodsb                  ; Load byte from [SI] into AL, increment SI
    cmp al, 0              ; Check if null terminator
    je .done               ; If null, we're done
    int 0x10               ; Call BIOS interrupt to print character
    jmp .repeat            ; Continue with next character
.done:
    popa                   ; Restore all registers
    ret

; Function: init_memory_map
; Initializes basic memory mapping for the bootloader
init_memory_map:
    pusha                  ; Save all registers
    
    ; Display memory mapping message
    mov si, memory_msg
    call print_string
    
    ; Get memory map using BIOS interrupt 0x15, function 0xE820
    ; This will help us understand available memory regions
    mov di, memory_map     ; Destination for memory map
    xor ebx, ebx           ; Clear EBX (continuation value)
    xor bp, bp             ; Clear BP (entry counter)
    mov edx, 0x534D4150    ; Magic number "SMAP"
    mov eax, 0xE820        ; Function code
    mov ecx, 24            ; Size of each entry
    int 0x15               ; Call BIOS interrupt
    
    ; Check if function is supported
    jc .error              ; If carry flag set, function not supported
    cmp eax, 0x534D4150    ; Check if magic number returned
    jne .error             ; If not equal, error occurred
    
    ; Display success message
    mov si, memory_success_msg
    call print_string
    jmp .done
    
.error:
    ; Display error message if memory mapping failed
    mov si, memory_error_msg
    call print_string
    
.done:
    popa                   ; Restore all registers
    ret

; Data section
init_msg db 'IKOS Bootloader: Initializing Real Mode...', 0x0D, 0x0A, 0
memory_msg db 'Setting up memory mapping...', 0x0D, 0x0A, 0
memory_success_msg db 'Memory mapping initialized successfully!', 0x0D, 0x0A, 0
memory_error_msg db 'Error: Memory mapping failed!', 0x0D, 0x0A, 0
complete_msg db 'Real mode initialization complete!', 0x0D, 0x0A, 0

; Memory map storage (reserve space for memory map entries)
memory_map:
    times 64 db 0          ; Reserve 64 bytes for memory map data (enough for ~2 entries)

; Boot signature (required for BIOS to recognize as bootable)
times 510-($-$$) db 0      ; Pad with zeros to byte 510
dw 0xAA55                  ; Boot signature at bytes 510-511
