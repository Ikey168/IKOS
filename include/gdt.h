/* IKOS Global Descriptor Table Definitions */
/* Defines GDT structure and segment descriptors for protected mode */

#ifndef GDT_H
#define GDT_H

/* GDT Entry Structure */
struct gdt_entry {
    unsigned short limit_low;       /* Lower 16 bits of limit */
    unsigned short base_low;        /* Lower 16 bits of base */
    unsigned char base_middle;      /* Next 8 bits of base */
    unsigned char access;           /* Access flags */
    unsigned char granularity;      /* Granularity and upper limit */
    unsigned char base_high;        /* Upper 8 bits of base */
} __attribute__((packed));

/* GDT Pointer Structure */
struct gdt_ptr {
    unsigned short limit;           /* Size of GDT */
    unsigned int base;              /* Base address of GDT */
} __attribute__((packed));

/* Access Byte Flags */
#define GDT_ACCESS_PRESENT      0x80    /* Present bit */
#define GDT_ACCESS_PRIV_RING0   0x00    /* Ring 0 (kernel) */
#define GDT_ACCESS_PRIV_RING1   0x20    /* Ring 1 */
#define GDT_ACCESS_PRIV_RING2   0x40    /* Ring 2 */
#define GDT_ACCESS_PRIV_RING3   0x60    /* Ring 3 (user) */
#define GDT_ACCESS_EXEC         0x10    /* Executable segment */
#define GDT_ACCESS_DC           0x04    /* Direction/Conforming bit */
#define GDT_ACCESS_RW           0x02    /* Read/Write bit */
#define GDT_ACCESS_ACCESSED     0x01    /* Accessed bit */

/* Granularity Byte Flags */
#define GDT_GRAN_4K             0x80    /* 4KB granularity */
#define GDT_GRAN_32BIT          0x40    /* 32-bit operand size */
#define GDT_GRAN_16BIT          0x00    /* 16-bit operand size */

/* Standard GDT Segment Selectors */
#define GDT_NULL_SELECTOR       0x00    /* Null segment */
#define GDT_CODE_SELECTOR       0x08    /* Code segment (ring 0) */
#define GDT_DATA_SELECTOR       0x10    /* Data segment (ring 0) */

/* Standard Access Bytes */
#define GDT_CODE_ACCESS         (GDT_ACCESS_PRESENT | GDT_ACCESS_PRIV_RING0 | GDT_ACCESS_EXEC | GDT_ACCESS_RW)
#define GDT_DATA_ACCESS         (GDT_ACCESS_PRESENT | GDT_ACCESS_PRIV_RING0 | GDT_ACCESS_RW)

/* Standard Granularity Bytes */
#define GDT_CODE_GRANULARITY    (GDT_GRAN_4K | GDT_GRAN_32BIT | 0x0F)
#define GDT_DATA_GRANULARITY    (GDT_GRAN_4K | GDT_GRAN_32BIT | 0x0F)

#endif /* GDT_H */