/* IKOS UHC#include "usb_uhci.h"
#include "usb.h"
#include "interrupts.h"
#include "memory.h"
#include "string.h"
#include "stdio.h"
#include <stdlib.h>
#include <stdint.h>t Controller Driver
 * 
 * Universal Host Controller Interface (UHCI) driver for USB 1.1
 * This implementation provides:
 * - UHCI host controller initialization and management
 * - USB 1.1 Low Speed and Full Speed device support
 * - Transfer scheduling and completion handling
 * - Port management and device detection
 */

#include "usb.h"
#include "io.h"
#include "interrupts.h"
#include "memory.h"
#include "stdio.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/* UHCI Register Offsets */
#define UHCI_REG_USBCMD         0x00    /* USB Command */
#define UHCI_REG_USBSTS         0x02    /* USB Status */
#define UHCI_REG_USBINTR        0x04    /* USB Interrupt Enable */
#define UHCI_REG_FRNUM          0x06    /* Frame Number */
#define UHCI_REG_FLBASEADD      0x08    /* Frame List Base Address */
#define UHCI_REG_SOFMOD         0x0C    /* Start of Frame Modify */
#define UHCI_REG_PORTSC1        0x10    /* Port 1 Status/Control */
#define UHCI_REG_PORTSC2        0x12    /* Port 2 Status/Control */

/* UHCI Command Register Bits */
#define UHCI_CMD_RS             0x0001  /* Run/Stop */
#define UHCI_CMD_HCRESET        0x0002  /* Host Controller Reset */
#define UHCI_CMD_GRESET         0x0004  /* Global Reset */
#define UHCI_CMD_EGSM           0x0008  /* Enter Global Suspend Mode */
#define UHCI_CMD_FGR            0x0010  /* Force Global Resume */
#define UHCI_CMD_SWDBG          0x0020  /* Software Debug */
#define UHCI_CMD_CF             0x0040  /* Configure Flag */
#define UHCI_CMD_MAXP           0x0080  /* Max Packet */

/* UHCI Status Register Bits */
#define UHCI_STS_USBINT         0x0001  /* USB Interrupt */
#define UHCI_STS_ERROR          0x0002  /* USB Error Interrupt */
#define UHCI_STS_RD             0x0004  /* Resume Detect */
#define UHCI_STS_HSE            0x0008  /* Host System Error */
#define UHCI_STS_HCPE           0x0010  /* Host Controller Process Error */
#define UHCI_STS_HCH            0x0020  /* Host Controller Halted */

/* UHCI Interrupt Enable Register Bits */
#define UHCI_INTR_TIMEOUT       0x0001  /* Timeout/CRC Interrupt Enable */
#define UHCI_INTR_RESUME        0x0002  /* Resume Interrupt Enable */
#define UHCI_INTR_IOC           0x0004  /* Interrupt On Complete Enable */
#define UHCI_INTR_SP            0x0008  /* Short Packet Interrupt Enable */

/* UHCI Port Status/Control Register Bits */
#define UHCI_PORT_CCS           0x0001  /* Current Connect Status */
#define UHCI_PORT_CSC           0x0002  /* Connect Status Change */
#define UHCI_PORT_PE            0x0004  /* Port Enable */
#define UHCI_PORT_PEC           0x0008  /* Port Enable Change */
#define UHCI_PORT_LS            0x0030  /* Line Status */
#define UHCI_PORT_RD            0x0040  /* Resume Detect */
#define UHCI_PORT_LSDA          0x0100  /* Low Speed Device Attached */
#define UHCI_PORT_PR            0x0200  /* Port Reset */
#define UHCI_PORT_SUSP          0x1000  /* Suspend */

/* UHCI Transfer Descriptor (TD) */
typedef struct uhci_td {
    uint32_t link;              /* Link to next TD */
    uint32_t cs;                /* Control and Status */
    uint32_t token;             /* Token */
    uint32_t buffer;            /* Buffer pointer */
    
    /* Software fields */
    struct uhci_td* next;       /* Next TD in chain */
    usb_transfer_t* transfer;   /* Associated transfer */
    bool active;                /* TD is active */
} __attribute__((packed)) uhci_td_t;

/* UHCI Queue Head (QH) */
typedef struct uhci_qh {
    uint32_t link;              /* Horizontal link */
    uint32_t element;           /* Element link (to TD) */
    
    /* Software fields */
    struct uhci_qh* next;       /* Next QH */
    uhci_td_t* first_td;        /* First TD in queue */
    uint8_t endpoint;           /* Endpoint address */
    uint8_t device_addr;        /* Device address */
} __attribute__((packed)) uhci_qh_t;

/* UHCI Controller State */
typedef struct uhci_controller {
    uint16_t io_base;           /* I/O base address */
    uint8_t irq;                /* IRQ number */
    
    /* Frame list */
    uint32_t* frame_list;       /* 1024 frame pointers */
    uint32_t frame_list_phys;   /* Physical address */
    
    /* Queue heads */
    uhci_qh_t* control_qh;      /* Control transfers */
    uhci_qh_t* bulk_qh;         /* Bulk transfers */
    uhci_qh_t* interrupt_qh[8]; /* Interrupt transfers (different intervals) */
    
    /* Transfer descriptors pool */
    uhci_td_t* td_pool;         /* TD pool */
    bool* td_used;              /* TD usage bitmap */
    uint16_t num_tds;           /* Number of TDs */
    
    /* Port status */
    uint16_t port_status[2];    /* Port status cache */
    
    /* State */
    bool running;               /* Controller is running */
    uint16_t frame_number;      /* Current frame number */
} uhci_controller_t;

/* Global UHCI state */
static uhci_controller_t uhci_controllers[4];
static uint8_t num_uhci_controllers = 0;

/* TD Control/Status bits */
#define UHCI_TD_ACTLEN_MASK     0x000007FF  /* Actual length */
#define UHCI_TD_BITSTUFF        0x00020000  /* Bitstuff error */
#define UHCI_TD_CRC_TIMEOUT     0x00040000  /* CRC/Timeout error */
#define UHCI_TD_NAK             0x00080000  /* NAK received */
#define UHCI_TD_BABBLE          0x00100000  /* Babble detected */
#define UHCI_TD_DATABUFFER      0x00200000  /* Data buffer error */
#define UHCI_TD_STALLED         0x00400000  /* Stalled */
#define UHCI_TD_ACTIVE          0x00800000  /* Active */
#define UHCI_TD_IOC             0x01000000  /* Interrupt on Complete */
#define UHCI_TD_IOS             0x02000000  /* Isochronous Select */
#define UHCI_TD_LS              0x04000000  /* Low Speed Device */
#define UHCI_TD_C_ERR_MASK      0x18000000  /* Error counter */
#define UHCI_TD_SPD             0x20000000  /* Short Packet Detect */

/* TD Token bits */
#define UHCI_TD_PID_MASK        0x000000FF  /* Packet ID */
#define UHCI_TD_DEVADDR_MASK    0x00007F00  /* Device address */
#define UHCI_TD_ENDPOINT_MASK   0x00078000  /* Endpoint */
#define UHCI_TD_DT              0x00080000  /* Data toggle */
#define UHCI_TD_MAXLEN_MASK     0x7FF00000  /* Maximum length */

/* PID values */
#define UHCI_PID_SETUP          0x2D
#define UHCI_PID_IN             0x69
#define UHCI_PID_OUT            0xE1

/* Internal function prototypes */
static int uhci_init_controller(usb_bus_t* bus);
static void uhci_shutdown_controller(usb_bus_t* bus);
static int uhci_submit_transfer(usb_bus_t* bus, usb_transfer_t* transfer);
static int uhci_cancel_transfer(usb_bus_t* bus, usb_transfer_t* transfer);
static void uhci_scan_ports(usb_bus_t* bus);
static void uhci_irq_handler(int irq, void* context);

static uhci_td_t* uhci_alloc_td(uhci_controller_t* uhci);
static void uhci_free_td(uhci_controller_t* uhci, uhci_td_t* td);
static void uhci_setup_td(uhci_td_t* td, uint8_t pid, uint8_t device_addr,
                          uint8_t endpoint, void* buffer, uint16_t length,
                          bool low_speed, bool toggle);
static void uhci_process_completed_transfers(uhci_controller_t* uhci);

static uint16_t uhci_read_reg16(uhci_controller_t* uhci, uint16_t offset);
static void uhci_write_reg16(uhci_controller_t* uhci, uint16_t offset, uint16_t value);
static uint32_t uhci_read_reg32(uhci_controller_t* uhci, uint16_t offset);
static void uhci_write_reg32(uhci_controller_t* uhci, uint16_t offset, uint32_t value);

/* Simplified aligned memory allocation */
static void* malloc_aligned(size_t size, size_t alignment) {
    /* Simple implementation - in real OS would use proper aligned allocation */
    void* ptr = malloc(size + alignment);
    if (!ptr) return NULL;
    
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return (void*)aligned;
}

/* Simplified interrupt handler registration */
static void register_irq_handler(uint8_t irq, void (*handler)(int, void*), void* context) {
    /* In real implementation, would register IRQ handler with interrupt system */
    printf("[UHCI] Registering IRQ handler for IRQ %d\n", irq);
}

static void unregister_irq_handler(uint8_t irq) {
    /* In real implementation, would unregister IRQ handler */
    printf("[UHCI] Unregistering IRQ handler for IRQ %d\n", irq);
}

/* UHCI Host Controller Interface */
static usb_hci_t uhci_hci = {
    .name = "UHCI",
    .type = USB_HCI_UHCI,
    .init = (int (*)(struct usb_hci *))uhci_init_controller,
    .shutdown = (void (*)(struct usb_hci *))uhci_shutdown_controller,
    .submit_transfer = (int (*)(struct usb_hci *, usb_transfer_t *))uhci_submit_transfer,
    .cancel_transfer = (int (*)(struct usb_hci *, usb_transfer_t *))uhci_cancel_transfer,
    .scan_ports = (void (*)(struct usb_bus *))uhci_scan_ports,
};

/* Register I/O functions */
static uint16_t uhci_read_reg16(uhci_controller_t* uhci, uint16_t offset) {
    return inw(uhci->io_base + offset);
}

static void uhci_write_reg16(uhci_controller_t* uhci, uint16_t offset, uint16_t value) {
    outw(uhci->io_base + offset, value);
}

static uint32_t uhci_read_reg32(uhci_controller_t* uhci, uint16_t offset) {
    return inl(uhci->io_base + offset);
}

static void uhci_write_reg32(uhci_controller_t* uhci, uint16_t offset, uint32_t value) {
    outl(uhci->io_base + offset, value);
}

/* UHCI Initialization */
int uhci_register_controller(uint16_t io_base, uint8_t irq) {
    if (num_uhci_controllers >= 4) {
        return USB_ERROR_NO_RESOURCES;
    }
    
    printf("[UHCI] Registering UHCI controller at I/O 0x%X, IRQ %d\n", io_base, irq);
    
    uhci_controller_t* uhci = &uhci_controllers[num_uhci_controllers];
    memset(uhci, 0, sizeof(uhci_controller_t));
    
    uhci->io_base = io_base;
    uhci->irq = irq;
    uhci->num_tds = 256; /* Pool of 256 TDs */
    
    /* Create USB bus */
    usb_bus_t bus;
    memset(&bus, 0, sizeof(usb_bus_t));
    
    snprintf(bus.name, sizeof(bus.name), "UHCI Controller %d", num_uhci_controllers);
    bus.hci = &uhci_hci;
    bus.max_speed = USB_SPEED_FULL;
    bus.num_ports = 2; /* UHCI typically has 2 ports */
    bus.private_data = uhci;
    
    /* Register with USB core */
    int result = usb_register_bus(&bus);
    if (result != USB_SUCCESS) {
        printf("[UHCI] Failed to register USB bus: %d\n", result);
        return result;
    }
    
    num_uhci_controllers++;
    printf("[UHCI] UHCI controller registered successfully\n");
    
    return USB_SUCCESS;
}

static int uhci_init_controller(usb_bus_t* bus) {
    uhci_controller_t* uhci = (uhci_controller_t*)bus->private_data;
    
    printf("[UHCI] Initializing UHCI controller\n");
    
    /* Reset controller */
    uhci_write_reg16(uhci, UHCI_REG_USBCMD, UHCI_CMD_HCRESET);
    
    /* Wait for reset to complete */
    int timeout = 1000;
    while ((uhci_read_reg16(uhci, UHCI_REG_USBCMD) & UHCI_CMD_HCRESET) && timeout--) {
        /* Small delay */
        for (volatile int i = 0; i < 1000; i++);
    }
    
    if (timeout <= 0) {
        printf("[UHCI] Controller reset timeout\n");
        return USB_ERROR_TIMEOUT;
    }
    
    /* Allocate frame list (1024 entries, 4KB aligned) */
    uhci->frame_list = (uint32_t*)malloc_aligned(4096, 4096);
    if (!uhci->frame_list) {
        printf("[UHCI] Failed to allocate frame list\n");
        return USB_ERROR_NO_MEMORY;
    }
    
    uhci->frame_list_phys = (uint32_t)uhci->frame_list; /* Simplified */
    
    /* Initialize frame list to terminate */
    for (int i = 0; i < 1024; i++) {
        uhci->frame_list[i] = 1; /* Terminate bit */
    }
    
    /* Allocate TD pool */
    uhci->td_pool = (uhci_td_t*)malloc(sizeof(uhci_td_t) * uhci->num_tds);
    uhci->td_used = (bool*)malloc(sizeof(bool) * uhci->num_tds);
    
    if (!uhci->td_pool || !uhci->td_used) {
        printf("[UHCI] Failed to allocate TD pool\n");
        free(uhci->frame_list);
        free(uhci->td_pool);
        free(uhci->td_used);
        return USB_ERROR_NO_MEMORY;
    }
    
    memset(uhci->td_pool, 0, sizeof(uhci_td_t) * uhci->num_tds);
    memset(uhci->td_used, 0, sizeof(bool) * uhci->num_tds);
    
    /* Set frame list base address */
    uhci_write_reg32(uhci, UHCI_REG_FLBASEADD, uhci->frame_list_phys);
    
    /* Clear frame number */
    uhci_write_reg16(uhci, UHCI_REG_FRNUM, 0);
    
    /* Enable interrupts */
    uhci_write_reg16(uhci, UHCI_REG_USBINTR, 
                     UHCI_INTR_TIMEOUT | UHCI_INTR_RESUME | 
                     UHCI_INTR_IOC | UHCI_INTR_SP);
    
    /* Register IRQ handler */
    register_irq_handler(uhci->irq, uhci_irq_handler, uhci);
    
    /* Start controller */
    uhci_write_reg16(uhci, UHCI_REG_USBCMD, UHCI_CMD_RS | UHCI_CMD_CF);
    uhci->running = true;
    
    printf("[UHCI] UHCI controller initialized and running\n");
    
    return USB_SUCCESS;
}

static void uhci_shutdown_controller(usb_bus_t* bus) {
    uhci_controller_t* uhci = (uhci_controller_t*)bus->private_data;
    
    printf("[UHCI] Shutting down UHCI controller\n");
    
    /* Stop controller */
    uhci_write_reg16(uhci, UHCI_REG_USBCMD, 0);
    uhci->running = false;
    
    /* Disable interrupts */
    uhci_write_reg16(uhci, UHCI_REG_USBINTR, 0);
    
    /* Unregister IRQ handler */
    unregister_irq_handler(uhci->irq);
    
    /* Free resources */
    if (uhci->frame_list) {
        free(uhci->frame_list);
    }
    if (uhci->td_pool) {
        free(uhci->td_pool);
    }
    if (uhci->td_used) {
        free(uhci->td_used);
    }
    
    memset(uhci, 0, sizeof(uhci_controller_t));
}

/* Transfer Management */
static int uhci_submit_transfer(usb_bus_t* bus, usb_transfer_t* transfer) {
    uhci_controller_t* uhci = (uhci_controller_t*)bus->private_data;
    
    if (!uhci->running) {
        return USB_ERROR_NO_DEVICE;
    }
    
    printf("[UHCI] Submitting transfer (EP 0x%02X, length %d)\n",
           transfer->endpoint, transfer->length);
    
    /* Allocate TD for transfer */
    uhci_td_t* td = uhci_alloc_td(uhci);
    if (!td) {
        printf("[UHCI] Failed to allocate TD\n");
        return USB_ERROR_NO_MEMORY;
    }
    
    /* Determine PID based on transfer type and direction */
    uint8_t pid;
    if (transfer->type == USB_TRANSFER_TYPE_CONTROL) {
        /* Control transfers use SETUP, IN/OUT, and IN (status) */
        pid = UHCI_PID_SETUP; /* Simplified for now */
    } else if (transfer->endpoint & 0x80) {
        pid = UHCI_PID_IN;
    } else {
        pid = UHCI_PID_OUT;
    }
    
    /* Setup TD */
    bool low_speed = (transfer->device->speed == USB_SPEED_LOW);
    uhci_setup_td(td, pid, transfer->device->address,
                  transfer->endpoint & 0x0F, transfer->buffer,
                  transfer->length, low_speed, false);
    
    td->transfer = transfer;
    td->active = true;
    
    /* Add to appropriate queue based on transfer type */
    /* For now, just add to frame list directly */
    uint16_t frame = uhci_read_reg16(uhci, UHCI_REG_FRNUM) & 0x3FF;
    uint32_t td_phys = (uint32_t)td; /* Simplified */
    
    uhci->frame_list[frame] = td_phys;
    
    printf("[UHCI] Transfer submitted at frame %d\n", frame);
    
    return USB_SUCCESS;
}

static int uhci_cancel_transfer(usb_bus_t* bus, usb_transfer_t* transfer) {
    uhci_controller_t* uhci = (uhci_controller_t*)bus->private_data;
    
    printf("[UHCI] Cancelling transfer\n");
    
    /* Find and deactivate TD */
    for (int i = 0; i < uhci->num_tds; i++) {
        if (uhci->td_used[i] && uhci->td_pool[i].transfer == transfer) {
            uhci->td_pool[i].cs &= ~UHCI_TD_ACTIVE;
            uhci->td_pool[i].active = false;
            uhci_free_td(uhci, &uhci->td_pool[i]);
            break;
        }
    }
    
    return USB_SUCCESS;
}

/* TD Management */
static uhci_td_t* uhci_alloc_td(uhci_controller_t* uhci) {
    for (int i = 0; i < uhci->num_tds; i++) {
        if (!uhci->td_used[i]) {
            uhci->td_used[i] = true;
            memset(&uhci->td_pool[i], 0, sizeof(uhci_td_t));
            return &uhci->td_pool[i];
        }
    }
    return NULL;
}

static void uhci_free_td(uhci_controller_t* uhci, uhci_td_t* td) {
    if (!td) {
        return;
    }
    
    /* Find TD in pool */
    for (int i = 0; i < uhci->num_tds; i++) {
        if (&uhci->td_pool[i] == td) {
            uhci->td_used[i] = false;
            break;
        }
    }
}

static void uhci_setup_td(uhci_td_t* td, uint8_t pid, uint8_t device_addr,
                          uint8_t endpoint, void* buffer, uint16_t length,
                          bool low_speed, bool toggle) {
    /* Setup link pointer (terminate for now) */
    td->link = 1; /* Terminate */
    
    /* Setup control/status */
    td->cs = UHCI_TD_ACTIVE | UHCI_TD_IOC | (3 << 27); /* 3 error retries */
    if (low_speed) {
        td->cs |= UHCI_TD_LS;
    }
    
    /* Setup token */
    td->token = pid |
                ((device_addr & 0x7F) << 8) |
                ((endpoint & 0x0F) << 15) |
                (((length > 0 ? length - 1 : 0x7FF) & 0x7FF) << 21);
    
    if (toggle) {
        td->token |= UHCI_TD_DT;
    }
    
    /* Setup buffer pointer */
    td->buffer = (uint32_t)buffer;
}

/* Port Management */
static void uhci_scan_ports(usb_bus_t* bus) {
    uhci_controller_t* uhci = (uhci_controller_t*)bus->private_data;
    
    printf("[UHCI] Scanning ports\n");
    
    /* Check both ports */
    for (int port = 0; port < 2; port++) {
        uint16_t offset = (port == 0) ? UHCI_REG_PORTSC1 : UHCI_REG_PORTSC2;
        uint16_t status = uhci_read_reg16(uhci, offset);
        
        if (status & UHCI_PORT_CSC) {
            /* Connect status changed */
            printf("[UHCI] Port %d connect status changed\n", port + 1);
            
            if (status & UHCI_PORT_CCS) {
                /* Device connected */
                printf("[UHCI] Device connected to port %d\n", port + 1);
                
                /* Reset port */
                uhci_write_reg16(uhci, offset, status | UHCI_PORT_PR);
                
                /* Wait for reset */
                for (volatile int i = 0; i < 10000; i++);
                
                /* Clear reset */
                uhci_write_reg16(uhci, offset, status & ~UHCI_PORT_PR);
                
                /* Enable port */
                uhci_write_reg16(uhci, offset, status | UHCI_PORT_PE);
                
                /* Determine device speed */
                status = uhci_read_reg16(uhci, offset);
                uint8_t speed = (status & UHCI_PORT_LSDA) ? USB_SPEED_LOW : USB_SPEED_FULL;
                
                printf("[UHCI] Device speed: %s\n", usb_speed_string(speed));
                
                /* Allocate device and enumerate */
                usb_device_t* device = usb_alloc_device(bus, 0);
                if (device) {
                    device->speed = speed;
                    device->port = port + 1;
                    usb_connect_device(device);
                }
            } else {
                /* Device disconnected */
                printf("[UHCI] Device disconnected from port %d\n", port + 1);
                
                /* Find and disconnect device */
                /* This would search the device list */
            }
            
            /* Clear connect status change */
            uhci_write_reg16(uhci, offset, status | UHCI_PORT_CSC);
        }
        
        uhci->port_status[port] = status;
    }
}

/* IRQ Handler */
static void uhci_irq_handler(int irq, void* context) {
    uhci_controller_t* uhci = (uhci_controller_t*)context;
    
    /* Read status register */
    uint16_t status = uhci_read_reg16(uhci, UHCI_REG_USBSTS);
    
    if (status == 0) {
        return; /* Not our interrupt */
    }
    
    /* Clear interrupt status */
    uhci_write_reg16(uhci, UHCI_REG_USBSTS, status);
    
    if (status & UHCI_STS_USBINT) {
        /* USB interrupt - transfer completed */
        uhci_process_completed_transfers(uhci);
    }
    
    if (status & UHCI_STS_ERROR) {
        /* USB error interrupt */
        printf("[UHCI] USB error interrupt\n");
    }
    
    if (status & UHCI_STS_RD) {
        /* Resume detect */
        printf("[UHCI] Resume detect\n");
    }
    
    if (status & UHCI_STS_HSE) {
        /* Host system error */
        printf("[UHCI] Host system error\n");
    }
    
    if (status & UHCI_STS_HCPE) {
        /* Host controller process error */
        printf("[UHCI] Host controller process error\n");
    }
    
    if (status & UHCI_STS_HCH) {
        /* Host controller halted */
        printf("[UHCI] Host controller halted\n");
        uhci->running = false;
    }
}

/* Transfer Completion Processing */
static void uhci_process_completed_transfers(uhci_controller_t* uhci) {
    /* Scan TD pool for completed transfers */
    for (int i = 0; i < uhci->num_tds; i++) {
        if (uhci->td_used[i] && uhci->td_pool[i].active) {
            uhci_td_t* td = &uhci->td_pool[i];
            
            /* Check if TD is no longer active */
            if (!(td->cs & UHCI_TD_ACTIVE)) {
                printf("[UHCI] Transfer completed (TD %d)\n", i);
                
                usb_transfer_t* transfer = td->transfer;
                int status = USB_TRANSFER_STATUS_SUCCESS;
                uint16_t actual_length = 0;
                
                /* Check for errors */
                if (td->cs & (UHCI_TD_STALLED | UHCI_TD_DATABUFFER | 
                              UHCI_TD_BABBLE | UHCI_TD_CRC_TIMEOUT | 
                              UHCI_TD_BITSTUFF)) {
                    status = USB_TRANSFER_STATUS_ERROR;
                    printf("[UHCI] Transfer error: 0x%08X\n", td->cs);
                } else {
                    /* Calculate actual length */
                    actual_length = (td->cs & UHCI_TD_ACTLEN_MASK) + 1;
                    if (actual_length > transfer->length) {
                        actual_length = transfer->length;
                    }
                }
                
                /* Mark TD as inactive and free it */
                td->active = false;
                uhci_free_td(uhci, td);
                
                /* Complete transfer */
                if (transfer) {
                    usb_transfer_complete(transfer, status, actual_length);
                }
            }
        }
    }
}
