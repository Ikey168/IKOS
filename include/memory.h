/* IKOS Memory Layout Definitions */
/* Defines memory regions and addresses for real mode and protected mode operation */

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* Basic memory functions */
void* kmalloc(size_t size);
void kfree(void* ptr);
void* memset(void* dest, int value, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
int memcmp(const void* ptr1, const void* ptr2, size_t count);

/* String functions */
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t count);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t count);

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

/* Protected Mode Memory Layout */
#define PMODE_STACK_BASE    0x9000    /* Protected mode stack base */
#define PMODE_STACK_SIZE    0x1000    /* 4KB protected mode stack */
#define PMODE_CODE_BASE     0x100000  /* 1MB - Protected mode code base */
#define PMODE_DATA_BASE     0x200000  /* 2MB - Protected mode data base */
#define PMODE_HEAP_BASE     0x300000  /* 3MB - Protected mode heap base */

/* GDT Memory Layout */
#define GDT_BASE            0x8000    /* GDT location in memory */
#define GDT_SIZE            0x800     /* 2KB for GDT (256 entries max) */
#define GDT_ENTRIES         8         /* Number of GDT entries */

/* IDT Memory Layout (for future interrupt handling) */
#define IDT_BASE            0x8800    /* IDT location in memory */
#define IDT_SIZE            0x800     /* 2KB for IDT (256 entries max) */

/* Paging Structures Memory Layout */
#define PML4_BASE           0x1000    /* Page Map Level 4 base address */
#define PDPT_BASE           0x2000    /* Page Directory Pointer Table base address */
#define PD_BASE             0x3000    /* Page Directory base address */
#define PT_BASE             0x4000    /* Page Table base address */
#define PAGING_AREA_SIZE    0x5000    /* 20KB total for paging structures */

/* Virtual Memory Layout */
#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000ULL  /* -2GB virtual address */
#define KERNEL_PHYSICAL_BASE 0x100000             /* 1MB physical address */
#define IDENTITY_MAP_SIZE   0x400000              /* 4MB identity mapped */

/* Kernel Loading Memory Layout */
#define KERNEL_BUFFER       0x10000   /* 64KB - Temporary kernel buffer */
#define DISK_BUFFER         0x20000   /* 128KB - Disk read buffer */
#define KERNEL_LOAD_BASE    0x100000  /* 1MB - Final kernel load address */
#define KERNEL_MAX_SIZE     0x400000  /* 4MB - Maximum kernel size */
#define ELF_HEADER_BUFFER   0x30000   /* 192KB - ELF header buffer */

#endif /* MEMORY_H */
