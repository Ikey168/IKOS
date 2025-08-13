; IKOS Boot Debugging Functions
; Provides serial output and framebuffer debugging capabilities

%include "boot.inc"

; ============================================================================
; Debug Constants
; ============================================================================
SERIAL_COM1         equ 0x3F8       ; COM1 base port
SERIAL_DATA         equ 0x00        ; Data register offset
SERIAL_STATUS       equ 0x05        ; Line status register offset
SERIAL_READY        equ 0x20        ; Transmitter ready bit

VGA_BUFFER          equ 0xB8000     ; VGA text buffer
VGA_WIDTH           equ 80          ; Screen width
VGA_HEIGHT          equ 25          ; Screen height

; Color attributes
ATTR_NORMAL         equ 0x07        ; White on black
ATTR_DEBUG          equ 0x0A        ; Light green on black
ATTR_ERROR          equ 0x0C        ; Light red on black
ATTR_SUCCESS        equ 0x0B        ; Light cyan on black
ATTR_INFO           equ 0x0E        ; Yellow on black

; ============================================================================
; Serial Debugging Functions
; ============================================================================

; Initialize serial port for debugging
; Input: None
; Output: None
; Modifies: AX, DX
init_serial:
    push ax
    push dx
    
    ; Set baud rate divisor (38400 baud)
    mov dx, SERIAL_COM1 + 3     ; Line control register
    mov al, 0x80                ; Set DLAB bit
    out dx, al
    
    mov dx, SERIAL_COM1 + 0     ; Divisor low byte
    mov al, 0x03                ; 38400 baud
    out dx, al
    
    mov dx, SERIAL_COM1 + 1     ; Divisor high byte
    mov al, 0x00
    out dx, al
    
    ; Configure line control (8N1)
    mov dx, SERIAL_COM1 + 3     ; Line control register
    mov al, 0x03                ; 8 bits, no parity, 1 stop bit
    out dx, al
    
    ; Enable FIFO
    mov dx, SERIAL_COM1 + 2     ; FIFO control register
    mov al, 0xC7                ; Enable FIFO, clear buffers
    out dx, al
    
    ; Enable interrupts
    mov dx, SERIAL_COM1 + 4     ; Modem control register
    mov al, 0x0B                ; IRQs enabled, RTS/DSR set
    out dx, al
    
    pop dx
    pop ax
    ret

; Send character via serial port
; Input: AL = character to send
; Output: None
; Modifies: AX, DX
serial_putchar:
    push ax
    push dx
    
    ; Wait for transmitter to be ready
.wait:
    mov dx, SERIAL_COM1 + SERIAL_STATUS
    in al, dx
    test al, SERIAL_READY
    jz .wait
    
    ; Send character
    pop dx
    push dx
    mov dx, SERIAL_COM1 + SERIAL_DATA
    pop ax
    push ax
    out dx, al
    
    pop dx
    pop ax
    ret

; Send string via serial port
; Input: SI = pointer to null-terminated string
; Output: None
; Modifies: AX, SI
serial_print:
    push ax
    push si
    
.loop:
    lodsb                       ; Load byte from DS:SI
    test al, al                 ; Check for null terminator
    jz .done
    call serial_putchar
    jmp .loop
    
.done:
    pop si
    pop ax
    ret

; Send hex byte via serial
; Input: AL = byte to send as hex
; Output: None
; Modifies: AX, BX
serial_print_hex_byte:
    push ax
    push bx
    
    ; Print high nibble
    mov bl, al
    shr al, 4
    and al, 0x0F
    cmp al, 10
    jl .high_digit
    add al, 'A' - 10
    jmp .high_print
.high_digit:
    add al, '0'
.high_print:
    call serial_putchar
    
    ; Print low nibble
    mov al, bl
    and al, 0x0F
    cmp al, 10
    jl .low_digit
    add al, 'A' - 10
    jmp .low_print
.low_digit:
    add al, '0'
.low_print:
    call serial_putchar
    
    pop bx
    pop ax
    ret

; ============================================================================
; Framebuffer Debugging Functions
; ============================================================================

; Global cursor position
debug_cursor_x      dw 0
debug_cursor_y      dw 0

; Clear debug area of screen (bottom 5 lines)
; Input: None
; Output: None
; Modifies: AX, CX, DI, ES
clear_debug_area:
    push ax
    push cx
    push di
    push es
    
    mov ax, 0xB800              ; VGA segment
    mov es, ax
    
    ; Clear bottom 5 lines (lines 20-24)
    mov di, (VGA_WIDTH * 20) * 2 ; Start of line 20
    mov cx, (VGA_WIDTH * 5)     ; 5 lines worth
    mov ax, (ATTR_DEBUG << 8) | ' ' ; Green background, space
    
.clear_loop:
    stosw
    loop .clear_loop
    
    ; Reset debug cursor to debug area
    mov word [debug_cursor_x], 0
    mov word [debug_cursor_y], 20
    
    pop es
    pop di
    pop cx
    pop ax
    ret

; Print character to framebuffer at debug cursor position
; Input: AL = character, AH = attribute
; Output: None
; Modifies: BX, CX, DI, ES
fb_putchar:
    push bx
    push cx
    push di
    push es
    
    mov bx, 0xB800              ; VGA segment
    mov es, bx
    
    ; Calculate position: (y * 80 + x) * 2
    mov bx, [debug_cursor_y]
    mov cx, VGA_WIDTH
    mul cx                      ; AX = y * 80 (AX was preserved)
    add ax, [debug_cursor_x]
    shl ax, 1                   ; Multiply by 2 (char + attr)
    mov di, ax
    
    ; Store character and attribute
    pop ax
    push ax
    stosw
    
    ; Advance cursor
    inc word [debug_cursor_x]
    cmp word [debug_cursor_x], VGA_WIDTH
    jl .done
    
    ; Wrap to next line
    mov word [debug_cursor_x], 0
    inc word [debug_cursor_y]
    
    ; Check if we need to scroll
    cmp word [debug_cursor_y], VGA_HEIGHT
    jl .done
    mov word [debug_cursor_y], VGA_HEIGHT - 1
    
.done:
    pop es
    pop di
    pop cx
    pop bx
    ret

; Print string to framebuffer
; Input: SI = string pointer, BL = attribute
; Output: None
; Modifies: AX, SI
fb_print:
    push ax
    push si
    
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, bl                  ; Set attribute
    call fb_putchar
    jmp .loop
    
.done:
    pop si
    pop ax
    ret

; Print debug message with timestamp
; Input: SI = message string
; Output: None
; Modifies: AX, BX, SI
debug_print:
    push ax
    push bx
    push si
    
    ; Print to serial
    mov si, debug_prefix
    call serial_print
    pop si
    push si
    call serial_print
    mov si, debug_newline
    call serial_print
    
    ; Print to framebuffer
    mov bl, ATTR_DEBUG
    mov si, debug_prefix_short
    call fb_print
    pop si
    push si
    mov bl, ATTR_NORMAL
    call fb_print
    mov al, 10                  ; Newline
    mov ah, ATTR_NORMAL
    call fb_putchar
    
    pop si
    pop bx
    pop ax
    ret

; Print error message
; Input: SI = error message string
; Output: None
; Modifies: AX, BX, SI
debug_error:
    push ax
    push bx
    push si
    
    ; Print to serial
    mov si, error_prefix
    call serial_print
    pop si
    push si
    call serial_print
    mov si, debug_newline
    call serial_print
    
    ; Print to framebuffer
    mov bl, ATTR_ERROR
    mov si, error_prefix_short
    call fb_print
    pop si
    push si
    mov bl, ATTR_ERROR
    call fb_print
    mov al, 10                  ; Newline
    mov ah, ATTR_ERROR
    call fb_putchar
    
    pop si
    pop bx
    pop ax
    ret

; Print success message
; Input: SI = success message string
; Output: None
; Modifies: AX, BX, SI
debug_success:
    push ax
    push bx
    push si
    
    ; Print to serial
    mov si, success_prefix
    call serial_print
    pop si
    push si
    call serial_print
    mov si, debug_newline
    call serial_print
    
    ; Print to framebuffer
    mov bl, ATTR_SUCCESS
    mov si, success_prefix_short
    call fb_print
    pop si
    push si
    mov bl, ATTR_SUCCESS
    call fb_print
    mov al, 10                  ; Newline
    mov ah, ATTR_SUCCESS
    call fb_putchar
    
    pop si
    pop bx
    pop ax
    ret

; ============================================================================
; Data Section
; ============================================================================
debug_prefix        db '[DEBUG] ', 0
debug_prefix_short  db '[DBG] ', 0
error_prefix        db '[ERROR] ', 0
error_prefix_short  db '[ERR] ', 0
success_prefix      db '[SUCCESS] ', 0
success_prefix_short db '[OK] ', 0
debug_newline       db 13, 10, 0

; Debug messages for boot stages
msg_init            db 'Bootloader initialization started', 0
msg_a20             db 'A20 line enabled', 0
msg_gdt             db 'GDT loaded', 0
msg_protected       db 'Entered protected mode', 0
msg_pae             db 'PAE enabled', 0
msg_paging          db 'Paging structures set up', 0
msg_longmode        db 'Long mode enabled', 0
msg_complete        db 'Boot process completed successfully', 0

; Error messages
err_a20             db 'Failed to enable A20 line', 0
err_protected       db 'Failed to enter protected mode', 0
err_paging          db 'Failed to set up paging', 0
err_longmode        db 'Failed to enter long mode', 0
