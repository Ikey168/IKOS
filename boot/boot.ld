/* IKOS Bootloader Linker Script */
/* Defines memory layout for real mode bootloader */

ENTRY(start)

MEMORY
{
    /* Real mode memory layout */
    BOOTLOADER (rwx) : ORIGIN = 0x7C00, LENGTH = 512
    STACK (rw)       : ORIGIN = 0x7000, LENGTH = 0xC00
    FREE (rwx)       : ORIGIN = 0x8000, LENGTH = 0x78000
}

SECTIONS
{
    . = 0x7C00;
    
    .text : {
        *(.text)
        . = ALIGN(4);
    } > BOOTLOADER
    
    .data : {
        *(.data)
        . = ALIGN(4);
    } > BOOTLOADER
    
    .bss : {
        *(.bss)
        . = ALIGN(4);
    } > BOOTLOADER
    
    /* Boot signature must be at offset 510 */
    . = 0x7DFE;
    .boot_signature : {
        SHORT(0xAA55)
    } > BOOTLOADER
}
