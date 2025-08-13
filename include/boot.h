/* IKOS Bootloader Definitions */
/* Common definitions and macros for the bootloader */

#ifndef BOOT_H
#define BOOT_H

/* Boot sector signature */
#define BOOT_SIGNATURE      0xAA55

/* BIOS Interrupts */
#define BIOS_VIDEO_INT      0x10      /* Video services */
#define BIOS_DISK_INT       0x13      /* Disk services */
#define BIOS_MEMORY_INT     0x15      /* Memory services */
#define BIOS_KEYBOARD_INT   0x16      /* Keyboard services */

/* Video Functions (INT 0x10) */
#define VIDEO_TELETYPE      0x0E      /* Teletype output */
#define VIDEO_SET_MODE      0x00      /* Set video mode */
#define VIDEO_GET_CURSOR    0x03      /* Get cursor position */
#define VIDEO_SET_CURSOR    0x02      /* Set cursor position */

/* Memory Functions (INT 0x15) */
#define MEMORY_MAP_FUNC     0xE820    /* Get memory map */
#define MEMORY_SIZE_FUNC    0x88      /* Get extended memory size */

/* Disk Functions (INT 0x13) */
#define DISK_READ           0x02      /* Read sectors */
#define DISK_WRITE          0x03      /* Write sectors */
#define DISK_RESET          0x00      /* Reset disk system */

/* Serial Port Debugging */
#define SERIAL_COM1_BASE    0x3F8     /* COM1 base port */
#define SERIAL_COM2_BASE    0x2F8     /* COM2 base port */
#define SERIAL_DATA_PORT    0x00      /* Data register offset */
#define SERIAL_STATUS_PORT  0x05      /* Line status register offset */
#define SERIAL_READY_BIT    0x20      /* Transmitter ready bit */

/* Framebuffer Constants */
#define VGA_TEXT_BUFFER     0xB8000   /* VGA text mode buffer */
#define VGA_WIDTH           80        /* Screen width in characters */
#define VGA_HEIGHT          25        /* Screen height in characters */
#define VGA_ATTR_NORMAL     0x07      /* White on black */
#define VGA_ATTR_DEBUG      0x0A      /* Light green on black */
#define VGA_ATTR_ERROR      0x0C      /* Light red on black */
#define VGA_ATTR_SUCCESS    0x0B      /* Light cyan on black */

/* Character Constants */
#define CHAR_NEWLINE        0x0A      /* Line feed */
#define CHAR_RETURN         0x0D      /* Carriage return */
#define CHAR_NULL           0x00      /* Null terminator */

/* Color Attributes for Text Mode */
#define COLOR_BLACK         0x00
#define COLOR_BLUE          0x01
#define COLOR_GREEN         0x02
#define COLOR_CYAN          0x03
#define COLOR_RED           0x04
#define COLOR_MAGENTA       0x05
#define COLOR_BROWN         0x06
#define COLOR_LIGHT_GRAY    0x07
#define COLOR_DARK_GRAY     0x08
#define COLOR_LIGHT_BLUE    0x09
#define COLOR_LIGHT_GREEN   0x0A
#define COLOR_LIGHT_CYAN    0x0B
#define COLOR_LIGHT_RED     0x0C
#define COLOR_LIGHT_MAGENTA 0x0D
#define COLOR_YELLOW        0x0E
#define COLOR_WHITE         0x0F

#endif /* BOOT_H */
