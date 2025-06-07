; IKOS Bootloader Include File
; Common constants and macros for assembly files

; Boot sector constants
%define BOOTLOADER_ADDR     0x7C00
%define BOOT_SIGNATURE      0xAA55
%define SECTOR_SIZE         512

; Memory layout
%define STACK_BASE          0x7C00
%define MEMORY_MAP_ADDR     0x8000

; BIOS interrupts
%define BIOS_VIDEO          0x10
%define BIOS_DISK           0x13  
%define BIOS_MEMORY         0x15
%define BIOS_KEYBOARD       0x16

; Video functions
%define VIDEO_TELETYPE      0x0E
%define VIDEO_SET_MODE      0x00

; Memory functions
%define MEMORY_MAP_FUNC     0xE820
%define MEMORY_SIZE_FUNC    0x88

; ASCII characters
%define CHAR_CR             0x0D
%define CHAR_LF             0x0A
%define CHAR_NULL           0x00

; Memory map signature
%define SMAP_SIGNATURE      0x534D4150
