/*
 * IKOS Network Interface Driver - Ethernet Drivers
 * Issue #45 - Network Interface Driver (Ethernet & Wi-Fi)
 * 
 * Specific implementations for common Ethernet controllers:
 * - Realtek RTL8139
 * - Intel E1000 series
 */

#include "network_driver.h"
#include "memory.h"
#include "stdio.h"
#include "string.h"
#include "pci.h"
#include "interrupts.h"
#include <stddef.h>

/* ================================
 * RTL8139 Ethernet Driver
 * ================================ */

/* RTL8139 register definitions */
#define RTL8139_MAC0            0x00    /* MAC address */
#define RTL8139_MAR0            0x08    /* Multicast registers */
#define RTL8139_TXSTATUS0       0x10    /* TX status (4 32-bit regs) */
#define RTL8139_TXADDR0         0x20    /* TX address (4 32-bit regs) */
#define RTL8139_RXBUF           0x30    /* RX buffer start address */
#define RTL8139_CMD             0x37    /* Command register */
#define RTL8139_RXBUFPTR        0x38    /* Current address of packet read */
#define RTL8139_RXBUFADDR       0x3A    /* Current RX buffer address */
#define RTL8139_IMR             0x3C    /* Interrupt mask register */
#define RTL8139_ISR             0x3E    /* Interrupt status register */
#define RTL8139_TXCONFIG        0x40    /* TX config */
#define RTL8139_RXCONFIG        0x44    /* RX config */
#define RTL8139_CONFIG1         0x52    /* Configuration register 1 */

/* RTL8139 command register bits */
#define RTL8139_CMD_RESET       0x10
#define RTL8139_CMD_RX_ENABLE   0x08
#define RTL8139_CMD_TX_ENABLE   0x04

/* RTL8139 interrupt bits */
#define RTL8139_INT_ROK         0x01    /* RX OK */
#define RTL8139_INT_RER         0x02    /* RX error */
#define RTL8139_INT_TOK         0x04    /* TX OK */
#define RTL8139_INT_TER         0x08    /* TX error */

/* RTL8139 TX configuration */
#define RTL8139_TX_MXDMA_2048   0x00000700
#define RTL8139_TX_IFG96        0x03000000

/* RTL8139 RX configuration */
#define RTL8139_RX_MXDMA_UNLIMITED  0x00000700
#define RTL8139_RX_ACCEPT_BROADCAST 0x00000008
#define RTL8139_RX_ACCEPT_MULTICAST 0x00000004
#define RTL8139_RX_ACCEPT_MY_PHYS   0x00000002
#define RTL8139_RX_ACCEPT_ALL_PHYS  0x00000001

static network_driver_ops_t rtl8139_ops = {
    .init = rtl8139_init,
    .start = rtl8139_start,
    .stop = rtl8139_stop,
    .send_packet = rtl8139_send_packet,
    .set_mac_address = NULL,
    .get_link_status = NULL,
    .wifi_scan = NULL,
    .wifi_connect = NULL,
    .wifi_disconnect = NULL,
    .wifi_get_status = NULL
};

int rtl8139_init(network_interface_t* iface) {
    printf("Initializing RTL8139 Ethernet driver...\n");
    
    // Scan PCI bus for RTL8139 devices
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t device = 0; device < 32; device++) {
            for (uint32_t function = 0; function < 8; function++) {
                uint16_t vendor_id = pci_read_word(bus, device, function, 0x00);
                if (vendor_id != 0x10EC) continue;
                
                uint16_t device_id = pci_read_word(bus, device, function, 0x02);
                if (device_id != 0x8139) continue;
                
                printf("Found RTL8139 at PCI %u:%u:%u\n", bus, device, function);
                
                // Register interface if not already provided
                if (!iface) {
                    iface = network_register_interface("eth0", NETWORK_TYPE_ETHERNET, &rtl8139_ops);
                    if (!iface) {
                        printf("Failed to register RTL8139 interface\n");
                        continue;
                    }
                }
                
                // Allocate private data
                rtl8139_private_t* priv = malloc(sizeof(rtl8139_private_t));
                if (!priv) {
                    printf("Failed to allocate RTL8139 private data\n");
                    return NETWORK_ERROR_NO_MEMORY;
                }
                
                memset(priv, 0, sizeof(rtl8139_private_t));
                iface->private_data = priv;
                iface->pci_vendor_id = vendor_id;
                iface->pci_device_id = device_id;
                
                // Get I/O base address
                uint32_t bar0 = pci_read_dword(bus, device, function, 0x10);
                priv->io_base = bar0 & 0xFFFFFFFC;
                
                // Get IRQ
                uint8_t irq_line = pci_read_byte(bus, device, function, 0x3C);
                priv->irq = irq_line;
                
                printf("RTL8139 I/O base: 0x%X, IRQ: %u\n", priv->io_base, priv->irq);
                
                // Enable PCI bus mastering
                uint16_t command = pci_read_word(bus, device, function, 0x04);
                command |= 0x07; // Enable I/O space, memory space, and bus mastering
                pci_write_word(bus, device, function, 0x04, command);
                
                // Reset the card
                outb(priv->io_base + RTL8139_CMD, RTL8139_CMD_RESET);
                
                // Wait for reset to complete
                uint32_t timeout = 1000000;
                while ((inb(priv->io_base + RTL8139_CMD) & RTL8139_CMD_RESET) && timeout--) {
                    // Wait
                }
                
                if (timeout == 0) {
                    printf("RTL8139 reset timeout\n");
                    free(priv);
                    return NETWORK_ERROR_DRIVER_ERROR;
                }
                
                // Read MAC address
                for (int i = 0; i < 6; i++) {
                    iface->mac_address.addr[i] = inb(priv->io_base + RTL8139_MAC0 + i);
                }
                
                printf("RTL8139 MAC address: %s\n", 
                       network_mac_addr_to_string(&iface->mac_address));
                
                // Allocate RX buffer (8KB + 16 bytes for overflow)
                priv->rx_buffer = malloc(8192 + 16);
                if (!priv->rx_buffer) {
                    printf("Failed to allocate RTL8139 RX buffer\n");
                    free(priv);
                    return NETWORK_ERROR_NO_MEMORY;
                }
                
                // Get physical address of RX buffer
                priv->rx_buffer_phys = (uint32_t)priv->rx_buffer; // Assuming identity mapping
                
                // Allocate TX buffers (4 buffers of 2KB each)
                for (int i = 0; i < 4; i++) {
                    priv->tx_buffers[i] = malloc(2048);
                    if (!priv->tx_buffers[i]) {
                        printf("Failed to allocate RTL8139 TX buffer %d\n", i);
                        // Cleanup allocated buffers
                        for (int j = 0; j < i; j++) {
                            free(priv->tx_buffers[j]);
                        }
                        free(priv->rx_buffer);
                        free(priv);
                        return NETWORK_ERROR_NO_MEMORY;
                    }
                    priv->tx_buffers_phys[i] = (uint32_t)priv->tx_buffers[i];
                }
                
                return NETWORK_SUCCESS;
            }
        }
    }
    
    printf("No RTL8139 devices found\n");
    return NETWORK_ERROR_INTERFACE_NOT_FOUND;
}

int rtl8139_start(network_interface_t* iface) {
    if (!iface || !iface->private_data) {
        return NETWORK_ERROR_INVALID_PARAM;
    }
    
    rtl8139_private_t* priv = (rtl8139_private_t*)iface->private_data;
    
    printf("Starting RTL8139 interface...\n");
    
    // Set RX buffer
    outl(priv->io_base + RTL8139_RXBUF, priv->rx_buffer_phys);
    
    // Configure TX
    outl(priv->io_base + RTL8139_TXCONFIG, RTL8139_TX_MXDMA_2048 | RTL8139_TX_IFG96);
    
    // Configure RX
    outl(priv->io_base + RTL8139_RXCONFIG, 
         RTL8139_RX_MXDMA_UNLIMITED | 
         RTL8139_RX_ACCEPT_BROADCAST | 
         RTL8139_RX_ACCEPT_MULTICAST | 
         RTL8139_RX_ACCEPT_MY_PHYS);
    
    // Enable interrupts
    outw(priv->io_base + RTL8139_IMR, 
         RTL8139_INT_ROK | RTL8139_INT_RER | RTL8139_INT_TOK | RTL8139_INT_TER);
    
    // Register interrupt handler
    register_interrupt_handler(priv->irq + 32, rtl8139_interrupt_handler);
    
    // Enable RX and TX
    outb(priv->io_base + RTL8139_CMD, RTL8139_CMD_RX_ENABLE | RTL8139_CMD_TX_ENABLE);
    
    printf("RTL8139 interface started\n");
    return NETWORK_SUCCESS;
}

int rtl8139_stop(network_interface_t* iface) {
    if (!iface || !iface->private_data) {
        return NETWORK_ERROR_INVALID_PARAM;
    }
    
    rtl8139_private_t* priv = (rtl8139_private_t*)iface->private_data;
    
    printf("Stopping RTL8139 interface...\n");
    
    // Disable RX and TX
    outb(priv->io_base + RTL8139_CMD, 0);
    
    // Disable interrupts
    outw(priv->io_base + RTL8139_IMR, 0);
    
    // Unregister interrupt handler
    register_interrupt_handler(priv->irq + 32, NULL);
    
    printf("RTL8139 interface stopped\n");
    return NETWORK_SUCCESS;
}

int rtl8139_send_packet(network_interface_t* iface, network_packet_t* packet) {
    if (!iface || !iface->private_data || !packet) {
        return NETWORK_ERROR_INVALID_PARAM;
    }
    
    rtl8139_private_t* priv = (rtl8139_private_t*)iface->private_data;
    
    if (packet->length > 2048) {
        return NETWORK_ERROR_PACKET_TOO_LARGE;
    }
    
    // Copy packet to TX buffer
    memcpy(priv->tx_buffers[priv->tx_current], packet->data, packet->length);
    
    // Set TX descriptor
    outl(priv->io_base + RTL8139_TXADDR0 + (priv->tx_current * 4), 
         priv->tx_buffers_phys[priv->tx_current]);
    outl(priv->io_base + RTL8139_TXSTATUS0 + (priv->tx_current * 4), 
         packet->length & 0x1FFF);
    
    // Move to next TX buffer
    priv->tx_current = (priv->tx_current + 1) % 4;
    
    return NETWORK_SUCCESS;
}

void rtl8139_interrupt_handler(uint8_t irq) {
    // Find interface with this IRQ
    network_interface_t* iface = NULL;
    for (uint32_t i = 0; i < NETWORK_MAX_INTERFACES; i++) {
        network_interface_t* test_iface = network_get_interface_by_id(i + 1);
        if (test_iface && test_iface->private_data) {
            rtl8139_private_t* priv = (rtl8139_private_t*)test_iface->private_data;
            if (priv->irq + 32 == irq) {
                iface = test_iface;
                break;
            }
        }
    }
    
    if (!iface) {
        return;
    }
    
    rtl8139_private_t* priv = (rtl8139_private_t*)iface->private_data;
    
    // Read interrupt status
    uint16_t isr = inw(priv->io_base + RTL8139_ISR);
    
    // Handle RX
    if (isr & RTL8139_INT_ROK) {
        // TODO: Process received packets
        printf("RTL8139 RX interrupt\n");
    }
    
    // Handle TX
    if (isr & RTL8139_INT_TOK) {
        printf("RTL8139 TX complete\n");
    }
    
    // Handle errors
    if (isr & (RTL8139_INT_RER | RTL8139_INT_TER)) {
        printf("RTL8139 error interrupt: 0x%X\n", isr);
    }
    
    // Clear interrupts
    outw(priv->io_base + RTL8139_ISR, isr);
}

/* ================================
 * Intel E1000 Ethernet Driver
 * ================================ */

/* E1000 register definitions */
#define E1000_CTRL      0x00000  /* Device Control */
#define E1000_STATUS    0x00008  /* Device Status */
#define E1000_EEPROM    0x00014  /* EEPROM/Flash Control */
#define E1000_ICR       0x000C0  /* Interrupt Cause Read */
#define E1000_ITR       0x000C4  /* Interrupt Throttling Rate */
#define E1000_IMS       0x000D0  /* Interrupt Mask Set */
#define E1000_IMC       0x000D8  /* Interrupt Mask Clear */
#define E1000_RCTL      0x00100  /* RX Control */
#define E1000_TCTL      0x00400  /* TX Control */
#define E1000_RDBAL     0x02800  /* RX Descriptor Base Address Low */
#define E1000_RDBAH     0x02804  /* RX Descriptor Base Address High */
#define E1000_RDLEN     0x02808  /* RX Descriptor Length */
#define E1000_RDH       0x02810  /* RX Descriptor Head */
#define E1000_RDT       0x02818  /* RX Descriptor Tail */
#define E1000_TDBAL     0x03800  /* TX Descriptor Base Address Low */
#define E1000_TDBAH     0x03804  /* TX Descriptor Base Address High */
#define E1000_TDLEN     0x03808  /* TX Descriptor Length */
#define E1000_TDH       0x03810  /* TX Descriptor Head */
#define E1000_TDT       0x03818  /* TX Descriptor Tail */

/* E1000 control register bits */
#define E1000_CTRL_RST      0x04000000  /* Global reset */
#define E1000_CTRL_ASDE     0x00000020  /* Auto-speed detection enable */
#define E1000_CTRL_SLU      0x00000040  /* Set link up (Force Link) */

/* E1000 RX control register bits */
#define E1000_RCTL_EN       0x00000002  /* enable */
#define E1000_RCTL_SBP      0x00000004  /* store bad packet */
#define E1000_RCTL_UPE      0x00000008  /* unicast promiscuous enable */
#define E1000_RCTL_MPE      0x00000010  /* multicast promiscuous enab */
#define E1000_RCTL_LPE      0x00000020  /* long packet enable */
#define E1000_RCTL_BAM      0x00008000  /* broadcast accept mode */
#define E1000_RCTL_SECRC    0x04000000  /* Strip Ethernet CRC */

/* E1000 TX control register bits */
#define E1000_TCTL_EN       0x00000002  /* enable tx */
#define E1000_TCTL_PSP      0x00000008  /* pad short packets */

static network_driver_ops_t e1000_ops = {
    .init = e1000_init,
    .start = e1000_start,
    .stop = e1000_stop,
    .send_packet = e1000_send_packet,
    .set_mac_address = NULL,
    .get_link_status = NULL,
    .wifi_scan = NULL,
    .wifi_connect = NULL,
    .wifi_disconnect = NULL,
    .wifi_get_status = NULL
};

int e1000_init(network_interface_t* iface) {
    printf("Initializing Intel E1000 Ethernet driver...\n");
    
    // Scan PCI bus for E1000 devices
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t device = 0; device < 32; device++) {
            for (uint32_t function = 0; function < 8; function++) {
                uint16_t vendor_id = pci_read_word(bus, device, function, 0x00);
                if (vendor_id != 0x8086) continue;
                
                uint16_t device_id = pci_read_word(bus, device, function, 0x02);
                
                // Check for known E1000 device IDs
                bool is_e1000 = (device_id == 0x100E || device_id == 0x1004 || device_id == 0x100F);
                if (!is_e1000) continue;
                
                printf("Found Intel E1000 (0x%04X) at PCI %u:%u:%u\n", 
                       device_id, bus, device, function);
                
                // Register interface if not already provided
                if (!iface) {
                    char name[16];
                    snprintf(name, sizeof(name), "eth%u", 
                             network_get_default_interface() ? 1 : 0);
                    
                    iface = network_register_interface(name, NETWORK_TYPE_ETHERNET, &e1000_ops);
                    if (!iface) {
                        printf("Failed to register E1000 interface\n");
                        continue;
                    }
                }
                
                // Allocate private data
                e1000_private_t* priv = malloc(sizeof(e1000_private_t));
                if (!priv) {
                    printf("Failed to allocate E1000 private data\n");
                    return NETWORK_ERROR_NO_MEMORY;
                }
                
                memset(priv, 0, sizeof(e1000_private_t));
                iface->private_data = priv;
                iface->pci_vendor_id = vendor_id;
                iface->pci_device_id = device_id;
                
                // Get MMIO base address
                uint32_t bar0 = pci_read_dword(bus, device, function, 0x10);
                priv->mmio_base = bar0 & 0xFFFFFFF0;
                
                // Get IRQ
                uint8_t irq_line = pci_read_byte(bus, device, function, 0x3C);
                priv->irq = irq_line;
                
                printf("E1000 MMIO base: 0x%X, IRQ: %u\n", priv->mmio_base, priv->irq);
                
                // Enable PCI bus mastering
                uint16_t command = pci_read_word(bus, device, function, 0x04);
                command |= 0x07; // Enable I/O space, memory space, and bus mastering
                pci_write_word(bus, device, function, 0x04, command);
                
                // TODO: Complete E1000 initialization
                // This would involve setting up descriptor rings, reading MAC address, etc.
                
                return NETWORK_SUCCESS;
            }
        }
    }
    
    printf("No Intel E1000 devices found\n");
    return NETWORK_ERROR_INTERFACE_NOT_FOUND;
}

int e1000_start(network_interface_t* iface) {
    if (!iface || !iface->private_data) {
        return NETWORK_ERROR_INVALID_PARAM;
    }
    
    printf("Starting Intel E1000 interface...\n");
    
    // TODO: Implement E1000 start sequence
    // This would involve configuring the device and enabling RX/TX
    
    return NETWORK_SUCCESS;
}

int e1000_stop(network_interface_t* iface) {
    if (!iface || !iface->private_data) {
        return NETWORK_ERROR_INVALID_PARAM;
    }
    
    printf("Stopping Intel E1000 interface...\n");
    
    // TODO: Implement E1000 stop sequence
    
    return NETWORK_SUCCESS;
}

int e1000_send_packet(network_interface_t* iface, network_packet_t* packet) {
    if (!iface || !iface->private_data || !packet) {
        return NETWORK_ERROR_INVALID_PARAM;
    }
    
    // TODO: Implement E1000 packet transmission
    
    return NETWORK_SUCCESS;
}

void e1000_interrupt_handler(uint8_t irq) {
    // TODO: Implement E1000 interrupt handling
    printf("E1000 interrupt received\n");
}
