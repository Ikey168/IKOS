; IKOS Disk Reading Functions
; Low-level disk I/O for reading kernel from disk

; ============================================================================
; FUNCTION: read_sectors
; Read sectors from disk using BIOS INT 0x13
; Input: 
;   AL = number of sectors to read
;   AH = 0x02 (read sectors function)
;   CH = cylinder number
;   CL = sector number  
;   DH = head number
;   DL = drive number (0x00 = floppy A:, 0x80 = hard disk C:)
;   ES:BX = buffer address
; Output:
;   CF = clear if successful, set if error
;   AL = number of sectors read
; ============================================================================
read_sectors:
    pusha
    mov bp, sp              ; Save stack pointer for retry counter
    
    ; Initialize retry counter
    mov byte [disk_retry_count], 3
    
.retry:
    ; Reset disk system first
    mov ah, 0x00            ; Reset disk function
    mov dl, [boot_drive]    ; Drive number
    int 0x13                ; Call BIOS
    jc .retry_check         ; If error, check retry count
    
    ; Read sectors
    mov ah, 0x02            ; Read sectors function
    mov al, [sectors_to_read] ; Number of sectors
    mov ch, [cylinder]      ; Cylinder number
    mov cl, [sector]        ; Sector number
    mov dh, [head]          ; Head number
    mov dl, [boot_drive]    ; Drive number
    mov bx, [buffer_addr]   ; Buffer address
    int 0x13                ; Call BIOS
    jnc .success            ; If no error, success
    
.retry_check:
    dec byte [disk_retry_count]
    jnz .retry              ; Retry if count > 0
    
    ; Failed after retries
    mov si, disk_error_msg
    call print_string
    stc                     ; Set carry flag to indicate error
    jmp .done
    
.success:
    mov si, disk_read_success_msg
    call print_string
    clc                     ; Clear carry flag to indicate success
    
.done:
    popa
    ret

; ============================================================================
; FUNCTION: lba_to_chs
; Convert LBA (Logical Block Address) to CHS (Cylinder, Head, Sector)
; Input: EAX = LBA sector number
; Output: Sets cylinder, head, sector variables
; Formula:
;   sector = (LBA % sectors_per_track) + 1
;   head = (LBA / sectors_per_track) % heads_per_cylinder  
;   cylinder = LBA / (sectors_per_track * heads_per_cylinder)
; ============================================================================
lba_to_chs:
    pusha
    
    ; Convert LBA to CHS
    xor edx, edx            ; Clear EDX for division
    mov ebx, SECTORS_PER_TRACK
    div ebx                 ; EAX = LBA / sectors_per_track, EDX = remainder
    inc edx                 ; Sector = remainder + 1
    mov [sector], dl        ; Store sector
    
    xor edx, edx            ; Clear EDX for division
    mov ebx, HEADS_PER_CYLINDER
    div ebx                 ; EAX = cylinder, EDX = head
    mov [head], dl          ; Store head
    mov [cylinder], al      ; Store cylinder (lower 8 bits)
    
    popa
    ret

; ============================================================================
; FUNCTION: read_kernel_from_disk
; Read kernel ELF file from disk starting at sector 2
; Input: ES:BX = destination buffer
; Output: CF = clear if successful, set if error
; ============================================================================
read_kernel_from_disk:
    pusha
    
    mov si, reading_kernel_msg
    call print_string
    
    ; Setup for reading kernel (starting at sector 2)
    mov eax, KERNEL_START_SECTOR ; Start at sector 2
    call lba_to_chs
    
    ; Setup read parameters
    mov [sectors_to_read], byte 10  ; Read 10 sectors (5KB) initially for headers
    mov [buffer_addr], bx           ; Store buffer address
    
    ; Read sectors
    call read_sectors
    jc .error
    
    ; Success
    mov si, kernel_read_success_msg
    call print_string
    clc
    jmp .done
    
.error:
    mov si, kernel_read_error_msg
    call print_string
    stc
    
.done:
    popa
    ret

; ============================================================================
; FUNCTION: print_hex_byte
; Print a single byte in hexadecimal
; Input: AL = byte to print
; ============================================================================
print_hex_byte:
    pusha
    
    mov ah, al              ; Save original value
    shr al, 4               ; Get upper 4 bits
    add al, '0'             ; Convert to ASCII
    cmp al, '9'
    jle .upper_digit
    add al, 7               ; Convert A-F
.upper_digit:
    mov bl, al              ; Save upper digit
    
    mov al, ah              ; Restore original value
    and al, 0x0F            ; Get lower 4 bits
    add al, '0'             ; Convert to ASCII
    cmp al, '9'
    jle .lower_digit
    add al, 7               ; Convert A-F
.lower_digit:
    
    ; Print upper digit
    push ax
    mov al, bl
    mov ah, 0x0E
    int 0x10
    pop ax
    
    ; Print lower digit
    mov ah, 0x0E
    int 0x10
    
    popa
    ret

; ============================================================================
; FUNCTION: print_string
; Print null-terminated string
; Input: SI = pointer to string
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

; Disk operation variables
boot_drive          db 0x00         ; Boot drive (floppy A:)
sectors_to_read     db 0            ; Number of sectors to read
cylinder            db 0            ; Cylinder number
head                db 0            ; Head number
sector              db 0            ; Sector number
buffer_addr         dw 0            ; Buffer address
disk_retry_count    db 0            ; Retry counter

; Status messages
reading_kernel_msg      db 'Reading kernel from disk...', 0x0D, 0x0A, 0
disk_read_success_msg   db 'Disk read successful', 0x0D, 0x0A, 0
disk_error_msg          db 'Disk read error!', 0x0D, 0x0A, 0
kernel_read_success_msg db 'Kernel read from disk successfully!', 0x0D, 0x0A, 0
kernel_read_error_msg   db 'Failed to read kernel from disk!', 0x0D, 0x0A, 0
