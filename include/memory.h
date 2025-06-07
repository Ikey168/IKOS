/* IKOS Memory Layout Definitions */
/* Defines memory regions and addresses for real mode operation */

#ifndef MEMORY_H
#define MEMORY_H

/* Real Mode Memory Layout */
#define BOOTLOADER_ADDR     0x7C00    /* BIOS loads bootloader here */
#define BOOTLOADER_SIZE     0x200     /* 512 bytes */
#define STACK_BASE          0x7C00    /* Stack grows downward from bootloader */
#define STACK_SIZE          0xC00     /* 3KB stack space */

/* Segment Register Values for Flat Memory Model */
#define SEGMENT_BASE        0x0000    /* All segments start at 0 */

/* Memory Map Entry Structure (BIOS INT 0x15, AX=0xE820) */
#define MEMORY_MAP_ENTRY_SIZE   24    /* Size of each memory map entry */
#define MEMORY_MAP_MAGIC        0x534D4150  /* "SMAP" magic number */

/* Memory Types (from BIOS memory map) */
#define MEMORY_TYPE_AVAILABLE   1     /* Available RAM */
#define MEMORY_TYPE_RESERVED    2     /* Reserved memory */
#define MEMORY_TYPE_ACPI_DATA   3     /* ACPI reclaimable */
#define MEMORY_TYPE_ACPI_NVS    4     /* ACPI non-volatile */
#define MEMORY_TYPE_BAD         5     /* Bad memory */

/* VGA Text Mode */
#define VGA_TEXT_BUFFER         0xB8000  /* VGA text mode buffer */
#define VGA_WIDTH               80       /* Characters per row */
#define VGA_HEIGHT              25       /* Number of rows */

/* BIOS Data Area */
#define BDA_BASE                0x400    /* BIOS Data Area start */
#define BDA_EQUIPMENT_WORD      0x410    /* Equipment configuration */
#define BDA_MEMORY_SIZE         0x413    /* Base memory size in KB */

#endif /* MEMORY_H */
