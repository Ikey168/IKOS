; IKOS Global Descriptor Table Include File
; Assembly constants and macros for GDT setup

; GDT Entry Size
%define GDT_ENTRY_SIZE          8

; Access Byte Flags
%define GDT_ACCESS_PRESENT      0x80    ; Present bit
%define GDT_ACCESS_PRIV_RING0   0x00    ; Ring 0 (kernel)
%define GDT_ACCESS_PRIV_RING3   0x60    ; Ring 3 (user)
%define GDT_ACCESS_EXEC         0x10    ; Executable segment
%define GDT_ACCESS_RW           0x02    ; Read/Write bit

; Granularity Byte Flags
%define GDT_GRAN_4K             0x80    ; 4KB granularity
%define GDT_GRAN_32BIT          0x40    ; 32-bit operand size
%define GDT_GRAN_LIMIT_HIGH     0x0F    ; Upper 4 bits of limit

; Standard Access Bytes
%define GDT_CODE_ACCESS         0x9A    ; Present, Ring 0, Executable, Readable
%define GDT_DATA_ACCESS         0x92    ; Present, Ring 0, Data, Writable

; Standard Granularity Bytes
%define GDT_CODE_GRANULARITY    0xCF    ; 4KB, 32-bit, limit 0xF
%define GDT_DATA_GRANULARITY    0xCF    ; 4KB, 32-bit, limit 0xF

; Segment Selectors
%define NULL_SELECTOR           0x00
%define CODE_SELECTOR           0x08
%define DATA_SELECTOR           0x10

; Protected Mode Control Register Flags
%define CR0_PROTECTED_MODE      0x01    ; Protected mode enable bit