/* IKOS Protected Mode Definitions */
/* Constants and structures for protected mode transition */

#ifndef PROTECTED_MODE_H
#define PROTECTED_MODE_H

/* A20 Line Control */
#define A20_TEST_ADDR1          0x7C00      /* First test address */
#define A20_TEST_ADDR2          0x17C00     /* Second test address (wrap around) */
#define A20_TEST_VALUE          0x1234      /* Test pattern */

/* Keyboard Controller Ports for A20 */
#define KBC_STATUS_PORT         0x64        /* Keyboard controller status */
#define KBC_COMMAND_PORT        0x64        /* Keyboard controller command */
#define KBC_DATA_PORT           0x60        /* Keyboard controller data */

/* Keyboard Controller Commands */
#define KBC_READ_OUTPUT         0xD0        /* Read output port */
#define KBC_WRITE_OUTPUT        0xD1        /* Write output port */

/* Keyboard Controller Status Flags */
#define KBC_STATUS_OUTPUT_FULL  0x01        /* Output buffer full */
#define KBC_STATUS_INPUT_FULL   0x02        /* Input buffer full */

/* A20 Enable Bit in Output Port */
#define KBC_OUTPUT_A20_ENABLE   0x02        /* A20 gate enable bit */

/* System Control Port A (Fast A20) */
#define SYSTEM_CONTROL_PORT_A   0x92        /* System control port A */
#define FAST_A20_ENABLE         0x02        /* Fast A20 enable bit */

/* Protected Mode Control Register 0 */
#define CR0_PROTECTED_MODE_BIT  0x00000001  /* Protected mode enable bit */

/* Memory Addresses for Protected Mode */
#define GDT_BASE_ADDRESS        0x8000      /* Base address for GDT */
#define PROTECTED_MODE_ENTRY    0x8200      /* Entry point after mode switch */

#endif /* PROTECTED_MODE_H */