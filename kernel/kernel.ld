/* IKOS Kernel Linker Script
 * Defines memory layout for kernel with scheduler
 */

ENTRY(_start)

SECTIONS
{
    /* Kernel loads at 1MB */
    . = 0x100000;
    
    /* Code section */
    .text : ALIGN(4K)
    {
        *(.text)
        *(.text.*)
    }
    
    /* Read-only data */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }
    
    /* Initialized data */
    .data : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }
    
    /* Uninitialized data */
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
    
    /* Stack space for kernel */
    .stack : ALIGN(4K)
    {
        . += 0x4000; /* 16KB stack */
    }
    
    /* End of kernel */
    _kernel_end = .;
}
