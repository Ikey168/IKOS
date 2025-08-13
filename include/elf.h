/* IKOS ELF Format Definitions */
/* Defines ELF file format structures and constants for kernel loading */

#ifndef ELF_H
#define ELF_H

#include <stddef.h>
#include <stdint.h>

/* ELF File Header Constants */
#define ELF_MAGIC           0x464C457F      /* ELF magic number (0x7F + "ELF") */
#define ELF_CLASS_32        1               /* 32-bit architecture */
#define ELF_CLASS_64        2               /* 64-bit architecture */
#define ELF_DATA_LSB        1               /* Little endian */
#define ELF_DATA_MSB        2               /* Big endian */
#define ELF_VERSION_CURRENT 1               /* Current ELF version */

/* ELF File Types */
#define ELF_TYPE_NONE       0               /* No file type */
#define ELF_TYPE_REL        1               /* Relocatable file */
#define ELF_TYPE_EXEC       2               /* Executable file */
#define ELF_TYPE_DYN        3               /* Shared object file */
#define ELF_TYPE_CORE       4               /* Core file */

/* ELF Machine Types */
#define ELF_MACHINE_NONE    0               /* No machine */
#define ELF_MACHINE_386     3               /* Intel 80386 */
#define ELF_MACHINE_X86_64  62              /* AMD x86-64 */

/* Program Header Types */
#define PT_NULL             0               /* Unused entry */
#define PT_LOAD             1               /* Loadable segment */
#define PT_DYNAMIC          2               /* Dynamic linking info */
#define PT_INTERP           3               /* Interpreter info */
#define PT_NOTE             4               /* Auxiliary info */
#define PT_SHLIB            5               /* Reserved */
#define PT_PHDR             6               /* Program header table */

/* Program Header Flags */
#define PF_X                0x1             /* Execute permission */
#define PF_W                0x2             /* Write permission */
#define PF_R                0x4             /* Read permission */

/* ELF Header Structure (32-bit) */
struct elf32_header {
    unsigned char e_ident[16];              /* ELF identification */
    unsigned short e_type;                  /* Object file type */
    unsigned short e_machine;               /* Architecture */
    unsigned int e_version;                 /* Object file version */
    unsigned int e_entry;                   /* Entry point virtual address */
    unsigned int e_phoff;                   /* Program header table offset */
    unsigned int e_shoff;                   /* Section header table offset */
    unsigned int e_flags;                   /* Processor-specific flags */
    unsigned short e_ehsize;                /* ELF header size */
    unsigned short e_phentsize;             /* Program header entry size */
    unsigned short e_phnum;                 /* Program header entry count */
    unsigned short e_shentsize;             /* Section header entry size */
    unsigned short e_shnum;                 /* Section header entry count */
    unsigned short e_shstrndx;              /* Section header string table index */
} __attribute__((packed));

/* Program Header Structure (32-bit) */
struct elf32_program_header {
    unsigned int p_type;                    /* Segment type */
    unsigned int p_offset;                  /* Segment file offset */
    unsigned int p_vaddr;                   /* Segment virtual address */
    unsigned int p_paddr;                   /* Segment physical address */
    unsigned int p_filesz;                  /* Segment size in file */
    unsigned int p_memsz;                   /* Segment size in memory */
    unsigned int p_flags;                   /* Segment flags */
    unsigned int p_align;                   /* Segment alignment */
} __attribute__((packed));

/* ELF Header Structure (64-bit) */
struct elf64_header {
    unsigned char e_ident[16];              /* ELF identification */
    unsigned short e_type;                  /* Object file type */
    unsigned short e_machine;               /* Architecture */
    unsigned int e_version;                 /* Object file version */
    unsigned long e_entry;                  /* Entry point virtual address */
    unsigned long e_phoff;                  /* Program header table offset */
    unsigned long e_shoff;                  /* Section header table offset */
    unsigned int e_flags;                   /* Processor-specific flags */
    unsigned short e_ehsize;                /* ELF header size */
    unsigned short e_phentsize;             /* Program header entry size */
    unsigned short e_phnum;                 /* Program header entry count */
    unsigned short e_shentsize;             /* Section header entry size */
    unsigned short e_shnum;                 /* Section header entry count */
    unsigned short e_shstrndx;              /* Section header string table index */
} __attribute__((packed));

/* Program Header Structure (64-bit) */
struct elf64_program_header {
    unsigned int p_type;                    /* Segment type */
    unsigned int p_flags;                   /* Segment flags */
    unsigned long p_offset;                 /* Segment file offset */
    unsigned long p_vaddr;                  /* Segment virtual address */
    unsigned long p_paddr;                  /* Segment physical address */
    unsigned long p_filesz;                 /* Segment size in file */
    unsigned long p_memsz;                  /* Segment size in memory */
    unsigned long p_align;                  /* Segment alignment */
} __attribute__((packed));

/* Type definitions for convenience */
typedef struct elf32_header elf32_header_t;
typedef struct elf32_program_header elf32_program_header_t;
typedef struct elf64_header elf64_header_t;
typedef struct elf64_program_header elf64_program_header_t;

/* ELF Validation Macros */
#define ELF_IS_VALID(hdr) \
    ((hdr)->e_ident[0] == 0x7F && \
     (hdr)->e_ident[1] == 'E' && \
     (hdr)->e_ident[2] == 'L' && \
     (hdr)->e_ident[3] == 'F')

#define ELF_IS_32BIT(hdr) ((hdr)->e_ident[4] == ELF_CLASS_32)
#define ELF_IS_64BIT(hdr) ((hdr)->e_ident[4] == ELF_CLASS_64)
#define ELF_IS_LITTLE_ENDIAN(hdr) ((hdr)->e_ident[5] == ELF_DATA_LSB)
#define ELF_IS_EXECUTABLE(hdr) ((hdr)->e_type == ELF_TYPE_EXEC)

/* ELF loading functions */
int elf_validate(const void* elf_data);
int elf_load_process(const void* elf_data, size_t size, uint64_t* entry_point);
int elf64_parse_headers(const elf64_header_t* header, elf64_program_header_t** phdrs);
int elf64_load_segment(const void* elf_data, const elf64_program_header_t* phdr, uint64_t base_addr);

/* Kernel Loading Constants */
#define KERNEL_LOAD_ADDRESS     0x100000    /* 1MB - Standard kernel load address */
#define KERNEL_MAX_SIZE         0x400000    /* 4MB - Maximum kernel size */
#define SECTORS_PER_TRACK       18          /* Standard floppy sectors per track */
#define HEADS_PER_CYLINDER      2           /* Standard floppy heads */
#define KERNEL_START_SECTOR     2           /* Kernel starts at sector 2 (after boot) */

#endif /* ELF_H */
