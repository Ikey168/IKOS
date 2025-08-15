/* IKOS PCI Bus Driver
 * Issue #15 - Device Driver Framework
 * 
 * PCI bus enumeration and device detection for x86/x86_64 systems.
 * Provides hardware discovery and device identification.
 */

#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stdbool.h>
#include "device_manager.h"

/* ================================
 * PCI Configuration Space
 * ================================ */

/* PCI configuration registers */
#define PCI_CONFIG_VENDOR_ID        0x00
#define PCI_CONFIG_DEVICE_ID        0x02
#define PCI_CONFIG_COMMAND          0x04
#define PCI_CONFIG_STATUS           0x06
#define PCI_CONFIG_REVISION_ID      0x08
#define PCI_CONFIG_PROG_IF          0x09
#define PCI_CONFIG_SUBCLASS         0x0A
#define PCI_CONFIG_CLASS_CODE       0x0B
#define PCI_CONFIG_CACHE_LINE_SIZE  0x0C
#define PCI_CONFIG_LATENCY_TIMER    0x0D
#define PCI_CONFIG_HEADER_TYPE      0x0E
#define PCI_CONFIG_BIST             0x0F

/* PCI Base Address Registers */
#define PCI_CONFIG_BAR0             0x10
#define PCI_CONFIG_BAR1             0x14
#define PCI_CONFIG_BAR2             0x18
#define PCI_CONFIG_BAR3             0x1C
#define PCI_CONFIG_BAR4             0x20
#define PCI_CONFIG_BAR5             0x24

/* PCI Type 0 specific registers */
#define PCI_CONFIG_CARDBUS_CIS      0x28
#define PCI_CONFIG_SUBSYSTEM_VENDOR 0x2C
#define PCI_CONFIG_SUBSYSTEM_ID     0x2E
#define PCI_CONFIG_EXPANSION_ROM    0x30
#define PCI_CONFIG_CAPABILITIES     0x34
#define PCI_CONFIG_INTERRUPT_LINE   0x3C
#define PCI_CONFIG_INTERRUPT_PIN    0x3D
#define PCI_CONFIG_MIN_GRANT        0x3E
#define PCI_CONFIG_MAX_LATENCY      0x3F

/* PCI I/O ports */
#define PCI_CONFIG_ADDRESS          0xCF8
#define PCI_CONFIG_DATA             0xCFC

/* PCI Header Types */
#define PCI_HEADER_TYPE_DEVICE      0x00
#define PCI_HEADER_TYPE_BRIDGE      0x01
#define PCI_HEADER_TYPE_CARDBUS     0x02
#define PCI_HEADER_TYPE_MULTIFUNCTION 0x80

/* PCI Command Register bits */
#define PCI_COMMAND_IO_ENABLE       0x0001
#define PCI_COMMAND_MEMORY_ENABLE   0x0002
#define PCI_COMMAND_BUS_MASTER      0x0004
#define PCI_COMMAND_SPECIAL_CYCLES  0x0008
#define PCI_COMMAND_MWI_ENABLE      0x0010
#define PCI_COMMAND_VGA_SNOOP       0x0020
#define PCI_COMMAND_PARITY_ERROR    0x0040
#define PCI_COMMAND_STEPPING        0x0080
#define PCI_COMMAND_SERR_ENABLE     0x0100
#define PCI_COMMAND_FAST_BACK       0x0200
#define PCI_COMMAND_INT_DISABLE     0x0400

/* PCI Status Register bits */
#define PCI_STATUS_CAP_LIST         0x0010
#define PCI_STATUS_66MHZ_CAPABLE    0x0020
#define PCI_STATUS_UDF_SUPPORTED    0x0040
#define PCI_STATUS_FAST_BACK        0x0080
#define PCI_STATUS_PARITY_ERROR     0x0100
#define PCI_STATUS_DEVSEL_MASK      0x0600
#define PCI_STATUS_SIG_TARGET_ABORT 0x0800
#define PCI_STATUS_REC_TARGET_ABORT 0x1000
#define PCI_STATUS_REC_MASTER_ABORT 0x2000
#define PCI_STATUS_SIG_SYSTEM_ERROR 0x4000
#define PCI_STATUS_DETECTED_PARITY  0x8000

/* ================================
 * PCI Device Classes
 * ================================ */

#define PCI_CLASS_UNCLASSIFIED      0x00
#define PCI_CLASS_MASS_STORAGE      0x01
#define PCI_CLASS_NETWORK           0x02
#define PCI_CLASS_DISPLAY           0x03
#define PCI_CLASS_MULTIMEDIA        0x04
#define PCI_CLASS_MEMORY            0x05
#define PCI_CLASS_BRIDGE            0x06
#define PCI_CLASS_COMMUNICATION     0x07
#define PCI_CLASS_SYSTEM            0x08
#define PCI_CLASS_INPUT             0x09
#define PCI_CLASS_DOCKING           0x0A
#define PCI_CLASS_PROCESSOR         0x0B
#define PCI_CLASS_SERIAL_BUS        0x0C
#define PCI_CLASS_WIRELESS          0x0D
#define PCI_CLASS_INTELLIGENT       0x0E
#define PCI_CLASS_SATELLITE         0x0F
#define PCI_CLASS_ENCRYPTION        0x10
#define PCI_CLASS_DATA_ACQUISITION  0x11
#define PCI_CLASS_UNDEFINED         0xFF

/* Mass Storage Subclasses */
#define PCI_SUBCLASS_SCSI           0x00
#define PCI_SUBCLASS_IDE            0x01
#define PCI_SUBCLASS_FLOPPY         0x02
#define PCI_SUBCLASS_IPI            0x03
#define PCI_SUBCLASS_RAID           0x04
#define PCI_SUBCLASS_ATA            0x05
#define PCI_SUBCLASS_SATA           0x06
#define PCI_SUBCLASS_SAS            0x07
#define PCI_SUBCLASS_NVME           0x08

/* Network Subclasses */
#define PCI_SUBCLASS_ETHERNET       0x00
#define PCI_SUBCLASS_TOKEN_RING     0x01
#define PCI_SUBCLASS_FDDI           0x02
#define PCI_SUBCLASS_ATM            0x03
#define PCI_SUBCLASS_ISDN           0x04
#define PCI_SUBCLASS_WIFI           0x80

/* ================================
 * PCI Data Structures
 * ================================ */

/* PCI device location */
typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
} pci_address_t;

/* PCI device information */
typedef struct {
    pci_address_t address;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint8_t header_type;
    uint16_t subsystem_vendor;
    uint16_t subsystem_id;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    
    /* Base Address Registers */
    uint32_t bar[6];
    bool bar_is_io[6];
    uint64_t bar_address[6];
    uint64_t bar_size[6];
} pci_device_info_t;

/* ================================
 * PCI Bus Driver API
 * ================================ */

/* PCI bus initialization */
int pci_init(void);
void pci_shutdown(void);

/* Low-level PCI configuration access */
uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t pci_config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

void pci_config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
void pci_config_write_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
void pci_config_write_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);

/* Device enumeration */
int pci_scan_bus(uint8_t bus);
int pci_scan_all_buses(void);
bool pci_device_exists(uint8_t bus, uint8_t device, uint8_t function);

/* Device information */
int pci_get_device_info(uint8_t bus, uint8_t device, uint8_t function, pci_device_info_t* info);
void pci_print_device_info(const pci_device_info_t* info);

/* BAR (Base Address Register) handling */
int pci_read_bars(pci_device_info_t* info);
uint64_t pci_get_bar_address(const pci_device_info_t* info, int bar_index);
uint64_t pci_get_bar_size(const pci_device_info_t* info, int bar_index);
bool pci_is_bar_io(const pci_device_info_t* info, int bar_index);

/* Device management */
int pci_enable_device(const pci_device_info_t* info);
int pci_disable_device(const pci_device_info_t* info);
int pci_set_bus_master(const pci_device_info_t* info, bool enable);

/* Capability support */
uint8_t pci_find_capability(const pci_device_info_t* info, uint8_t cap_id);
bool pci_has_capability(const pci_device_info_t* info, uint8_t cap_id);

/* Device class/type conversion */
device_class_t pci_class_to_device_class(uint8_t pci_class);
device_type_t pci_subclass_to_device_type(uint8_t pci_class, uint8_t subclass);
const char* pci_class_name(uint8_t class_code);
const char* pci_subclass_name(uint8_t class_code, uint8_t subclass);

/* Statistics and enumeration */
typedef struct {
    uint32_t total_devices;
    uint32_t total_functions;
    uint32_t bridges;
    uint32_t endpoints;
    uint32_t buses_scanned;
    uint32_t storage_devices;
    uint32_t network_devices;
    uint32_t display_devices;
} pci_stats_t;

void pci_get_stats(pci_stats_t* stats);
void pci_print_all_devices(void);

/* ================================
 * Common PCI Vendor IDs
 * ================================ */

#define PCI_VENDOR_INTEL            0x8086
#define PCI_VENDOR_AMD              0x1022
#define PCI_VENDOR_NVIDIA           0x10DE
#define PCI_VENDOR_ATI              0x1002
#define PCI_VENDOR_REALTEK          0x10EC
#define PCI_VENDOR_BROADCOM         0x14E4
#define PCI_VENDOR_QUALCOMM         0x17CB
#define PCI_VENDOR_MARVELL          0x11AB
#define PCI_VENDOR_VMWARE           0x15AD
#define PCI_VENDOR_QEMU             0x1234
#define PCI_VENDOR_REDHAT           0x1AF4

/* ================================
 * Error Codes
 * ================================ */

#define PCI_SUCCESS                 0
#define PCI_ERROR_INVALID_PARAM     -1
#define PCI_ERROR_DEVICE_NOT_FOUND  -2
#define PCI_ERROR_ACCESS_DENIED     -3
#define PCI_ERROR_TIMEOUT           -4
#define PCI_ERROR_NO_MEMORY         -5

#endif /* PCI_H */
