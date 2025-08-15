/* IKOS PCI Bus Driver Implementation
 * Issue #15 - Device Driver Framework
 * 
 * Implementation of PCI bus enumeration, device detection,
 * and hardware configuration management.
 */

#include "pci.h"
#include "device_manager.h"
#include "memory.h"
#include <string.h>

/* ================================
 * Global State
 * ================================ */

static bool g_pci_initialized = false;
static pci_stats_t g_pci_stats = {0};

/* ================================
 * Low-level I/O Functions
 * ================================ */

static inline void outl(uint16_t port, uint32_t data) {
    __asm__ volatile("outl %0, %1" : : "a"(data), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t data;
    __asm__ volatile("inl %1, %0" : "=a"(data) : "Nd"(port));
    return data;
}

static inline void outw(uint16_t port, uint16_t data) {
    __asm__ volatile("outw %0, %1" : : "a"(data), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t data;
    __asm__ volatile("inw %1, %0" : "=a"(data) : "Nd"(port));
    return data;
}

static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t data;
    __asm__ volatile("inb %1, %0" : "=a"(data) : "Nd"(port));
    return data;
}

static void debug_print(const char* format, ...) {
    // Placeholder for debug printing
    (void)format;
}

/* ================================
 * PCI Configuration Access
 * ================================ */

/**
 * Create PCI configuration address
 */
static uint32_t pci_make_config_address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    return (1U << 31) |               /* Enable bit */
           ((uint32_t)bus << 16) |    /* Bus number */
           ((uint32_t)device << 11) | /* Device number */
           ((uint32_t)function << 8) |/* Function number */
           (offset & 0xFC);           /* Register offset (aligned to 4 bytes) */
}

/**
 * Read 32-bit value from PCI configuration space
 */
uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = pci_make_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

/**
 * Read 16-bit value from PCI configuration space
 */
uint16_t pci_config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = pci_make_config_address(bus, device, function, offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return (inl(PCI_CONFIG_DATA) >> ((offset & 3) * 8)) & 0xFFFF;
}

/**
 * Read 8-bit value from PCI configuration space
 */
uint8_t pci_config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = pci_make_config_address(bus, device, function, offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return (inl(PCI_CONFIG_DATA) >> ((offset & 3) * 8)) & 0xFF;
}

/**
 * Write 32-bit value to PCI configuration space
 */
void pci_config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = pci_make_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

/**
 * Write 16-bit value to PCI configuration space
 */
void pci_config_write_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t address = pci_make_config_address(bus, device, function, offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    
    uint32_t data = inl(PCI_CONFIG_DATA);
    uint32_t shift = (offset & 3) * 8;
    data = (data & ~(0xFFFF << shift)) | ((uint32_t)value << shift);
    outl(PCI_CONFIG_DATA, data);
}

/**
 * Write 8-bit value to PCI configuration space
 */
void pci_config_write_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value) {
    uint32_t address = pci_make_config_address(bus, device, function, offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    
    uint32_t data = inl(PCI_CONFIG_DATA);
    uint32_t shift = (offset & 3) * 8;
    data = (data & ~(0xFF << shift)) | ((uint32_t)value << shift);
    outl(PCI_CONFIG_DATA, data);
}

/* ================================
 * Device Detection
 * ================================ */

/**
 * Check if PCI device exists
 */
bool pci_device_exists(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t vendor_device = pci_config_read_dword(bus, device, function, PCI_CONFIG_VENDOR_ID);
    return (vendor_device != 0xFFFFFFFF) && ((vendor_device & 0xFFFF) != 0xFFFF);
}

/**
 * Get comprehensive device information
 */
int pci_get_device_info(uint8_t bus, uint8_t device, uint8_t function, pci_device_info_t* info) {
    if (!info) {
        return PCI_ERROR_INVALID_PARAM;
    }
    
    if (!pci_device_exists(bus, device, function)) {
        return PCI_ERROR_DEVICE_NOT_FOUND;
    }
    
    memset(info, 0, sizeof(pci_device_info_t));
    
    /* Set address */
    info->address.bus = bus;
    info->address.device = device;
    info->address.function = function;
    
    /* Read basic device information */
    uint32_t vendor_device = pci_config_read_dword(bus, device, function, PCI_CONFIG_VENDOR_ID);
    info->vendor_id = vendor_device & 0xFFFF;
    info->device_id = (vendor_device >> 16) & 0xFFFF;
    
    uint32_t class_rev = pci_config_read_dword(bus, device, function, PCI_CONFIG_REVISION_ID);
    info->revision = class_rev & 0xFF;
    info->prog_if = (class_rev >> 8) & 0xFF;
    info->subclass = (class_rev >> 16) & 0xFF;
    info->class_code = (class_rev >> 24) & 0xFF;
    
    info->header_type = pci_config_read_byte(bus, device, function, PCI_CONFIG_HEADER_TYPE);
    
    /* Read subsystem information (for type 0 headers) */
    if ((info->header_type & 0x7F) == PCI_HEADER_TYPE_DEVICE) {
        uint32_t subsystem = pci_config_read_dword(bus, device, function, PCI_CONFIG_SUBSYSTEM_VENDOR);
        info->subsystem_vendor = subsystem & 0xFFFF;
        info->subsystem_id = (subsystem >> 16) & 0xFFFF;
        
        info->interrupt_line = pci_config_read_byte(bus, device, function, PCI_CONFIG_INTERRUPT_LINE);
        info->interrupt_pin = pci_config_read_byte(bus, device, function, PCI_CONFIG_INTERRUPT_PIN);
    }
    
    /* Read Base Address Registers */
    pci_read_bars(info);
    
    return PCI_SUCCESS;
}

/**
 * Read and decode Base Address Registers
 */
int pci_read_bars(pci_device_info_t* info) {
    if (!info) {
        return PCI_ERROR_INVALID_PARAM;
    }
    
    for (int i = 0; i < 6; i++) {
        uint8_t offset = PCI_CONFIG_BAR0 + (i * 4);
        
        /* Read current BAR value */
        info->bar[i] = pci_config_read_dword(info->address.bus, info->address.device, 
                                           info->address.function, offset);
        
        if (info->bar[i] == 0) {
            continue;  /* BAR not implemented */
        }
        
        /* Check if I/O or memory BAR */
        info->bar_is_io[i] = (info->bar[i] & 1) ? true : false;
        
        if (info->bar_is_io[i]) {
            /* I/O BAR */
            info->bar_address[i] = info->bar[i] & 0xFFFFFFFC;
            
            /* Write all 1s to determine size */
            pci_config_write_dword(info->address.bus, info->address.device, 
                                 info->address.function, offset, 0xFFFFFFFF);
            uint32_t size_mask = pci_config_read_dword(info->address.bus, info->address.device, 
                                                     info->address.function, offset);
            
            /* Restore original value */
            pci_config_write_dword(info->address.bus, info->address.device, 
                                 info->address.function, offset, info->bar[i]);
            
            /* Calculate size */
            size_mask &= 0xFFFFFFFC;
            info->bar_size[i] = (~size_mask) + 1;
        } else {
            /* Memory BAR */
            uint8_t mem_type = (info->bar[i] >> 1) & 3;
            
            if (mem_type == 0) {
                /* 32-bit memory BAR */
                info->bar_address[i] = info->bar[i] & 0xFFFFFFF0;
                
                /* Write all 1s to determine size */
                pci_config_write_dword(info->address.bus, info->address.device, 
                                     info->address.function, offset, 0xFFFFFFFF);
                uint32_t size_mask = pci_config_read_dword(info->address.bus, info->address.device, 
                                                         info->address.function, offset);
                
                /* Restore original value */
                pci_config_write_dword(info->address.bus, info->address.device, 
                                     info->address.function, offset, info->bar[i]);
                
                /* Calculate size */
                size_mask &= 0xFFFFFFF0;
                info->bar_size[i] = (~size_mask) + 1;
            } else if (mem_type == 2) {
                /* 64-bit memory BAR */
                if (i < 5) {
                    uint32_t bar_high = pci_config_read_dword(info->address.bus, info->address.device, 
                                                            info->address.function, offset + 4);
                    info->bar_address[i] = (info->bar[i] & 0xFFFFFFF0) | ((uint64_t)bar_high << 32);
                    
                    /* Size calculation for 64-bit BAR is more complex */
                    info->bar_size[i] = 0;  /* TODO: Implement 64-bit BAR sizing */
                    i++;  /* Skip next BAR as it's part of this 64-bit BAR */
                }
            }
        }
    }
    
    return PCI_SUCCESS;
}

/* ================================
 * Device Enumeration
 * ================================ */

/**
 * Scan a single PCI function
 */
static int pci_scan_function(uint8_t bus, uint8_t device, uint8_t function) {
    pci_device_info_t info;
    
    if (pci_get_device_info(bus, device, function, &info) != PCI_SUCCESS) {
        return PCI_ERROR_DEVICE_NOT_FOUND;
    }
    
    g_pci_stats.total_functions++;
    
    /* Create device manager device */
    device_class_t dev_class = pci_class_to_device_class(info.class_code);
    device_type_t dev_type = pci_subclass_to_device_type(info.class_code, info.subclass);
    
    char device_name[MAX_DEVICE_NAME_LEN];
    // Simple name formatting without snprintf
    device_name[0] = 'p'; device_name[1] = 'c'; device_name[2] = 'i'; device_name[3] = ':';
    device_name[4] = (bus >> 4) < 10 ? '0' + (bus >> 4) : 'A' + (bus >> 4) - 10;
    device_name[5] = (bus & 0xF) < 10 ? '0' + (bus & 0xF) : 'A' + (bus & 0xF) - 10;
    device_name[6] = ':';
    device_name[7] = (device >> 4) < 10 ? '0' + (device >> 4) : 'A' + (device >> 4) - 10;
    device_name[8] = (device & 0xF) < 10 ? '0' + (device & 0xF) : 'A' + (device & 0xF) - 10;
    device_name[9] = '.';
    device_name[10] = '0' + function;
    device_name[11] = '\0';
    
    device_t* dev = device_create(dev_class, dev_type, device_name);
    if (!dev) {
        return PCI_ERROR_NO_MEMORY;
    }
    
    /* Set device properties */
    dev->vendor_id = info.vendor_id;
    dev->product_id = info.device_id;
    dev->revision = info.revision;
    dev->bus_type = 1;  /* PCI bus type */
    dev->bus_number = bus;
    dev->device_number = device;
    dev->function_number = function;
    
    /* Add resources based on BARs */
    for (int i = 0; i < 6; i++) {
        if (info.bar_size[i] > 0) {
            uint32_t resource_type = info.bar_is_io[i] ? RESOURCE_TYPE_IO_PORT : RESOURCE_TYPE_MEMORY;
            device_add_resource(dev, info.bar_address[i], info.bar_size[i], resource_type);
        }
    }
    
    /* Add IRQ resource if present */
    if (info.interrupt_line != 0 && info.interrupt_line != 0xFF) {
        device_add_resource(dev, info.interrupt_line, 1, RESOURCE_TYPE_IRQ);
    }
    
    /* Store PCI-specific information */
    pci_device_info_t* pci_info = (pci_device_info_t*)kmalloc(sizeof(pci_device_info_t));
    if (pci_info) {
        memcpy(pci_info, &info, sizeof(pci_device_info_t));
        dev->driver_data = pci_info;
    }
    
    /* Register device */
    device_register(dev);
    
    /* Update statistics */
    g_pci_stats.total_devices++;
    if ((info.header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) {
        g_pci_stats.bridges++;
    } else {
        g_pci_stats.endpoints++;
    }
    
    switch (info.class_code) {
        case PCI_CLASS_MASS_STORAGE:
            g_pci_stats.storage_devices++;
            break;
        case PCI_CLASS_NETWORK:
            g_pci_stats.network_devices++;
            break;
        case PCI_CLASS_DISPLAY:
            g_pci_stats.display_devices++;
            break;
    }
    
    debug_print("PCI: Found device %04x:%04x at %02x:%02x.%x (%s)\n",
                info.vendor_id, info.device_id, bus, device, function,
                pci_class_name(info.class_code));
    
    return PCI_SUCCESS;
}

/**
 * Scan a single PCI device (all functions)
 */
static int pci_scan_device(uint8_t bus, uint8_t device) {
    if (!pci_device_exists(bus, device, 0)) {
        return PCI_ERROR_DEVICE_NOT_FOUND;
    }
    
    /* Scan function 0 */
    pci_scan_function(bus, device, 0);
    
    /* Check if multi-function device */
    uint8_t header_type = pci_config_read_byte(bus, device, 0, PCI_CONFIG_HEADER_TYPE);
    if (header_type & PCI_HEADER_TYPE_MULTIFUNCTION) {
        /* Scan functions 1-7 */
        for (uint8_t function = 1; function < 8; function++) {
            if (pci_device_exists(bus, device, function)) {
                pci_scan_function(bus, device, function);
            }
        }
    }
    
    return PCI_SUCCESS;
}

/**
 * Scan a single PCI bus
 */
int pci_scan_bus(uint8_t bus) {
    debug_print("PCI: Scanning bus %d\n", bus);
    
    for (uint8_t device = 0; device < 32; device++) {
        pci_scan_device(bus, device);
    }
    
    g_pci_stats.buses_scanned++;
    return PCI_SUCCESS;
}

/**
 * Scan all PCI buses
 */
int pci_scan_all_buses(void) {
    debug_print("PCI: Starting full bus enumeration\n");
    
    /* Reset statistics */
    memset(&g_pci_stats, 0, sizeof(g_pci_stats));
    
    /* Scan all possible buses (0-255) */
    for (uint16_t bus = 0; bus < 256; bus++) {
        /* Check if bus has any devices */
        bool bus_has_devices = false;
        for (uint8_t device = 0; device < 32 && !bus_has_devices; device++) {
            if (pci_device_exists(bus, device, 0)) {
                bus_has_devices = true;
            }
        }
        
        if (bus_has_devices) {
            pci_scan_bus(bus);
        }
    }
    
    debug_print("PCI: Enumeration complete - found %u devices on %u buses\n",
                g_pci_stats.total_devices, g_pci_stats.buses_scanned);
    
    return PCI_SUCCESS;
}

/* ================================
 * Device Management
 * ================================ */

/**
 * Enable PCI device
 */
int pci_enable_device(const pci_device_info_t* info) {
    if (!info) {
        return PCI_ERROR_INVALID_PARAM;
    }
    
    /* Read current command register */
    uint16_t command = pci_config_read_word(info->address.bus, info->address.device, 
                                          info->address.function, PCI_CONFIG_COMMAND);
    
    /* Enable I/O and memory access */
    command |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEMORY_ENABLE;
    
    /* Write back command register */
    pci_config_write_word(info->address.bus, info->address.device, 
                         info->address.function, PCI_CONFIG_COMMAND, command);
    
    return PCI_SUCCESS;
}

/**
 * Set bus master enable
 */
int pci_set_bus_master(const pci_device_info_t* info, bool enable) {
    if (!info) {
        return PCI_ERROR_INVALID_PARAM;
    }
    
    uint16_t command = pci_config_read_word(info->address.bus, info->address.device, 
                                          info->address.function, PCI_CONFIG_COMMAND);
    
    if (enable) {
        command |= PCI_COMMAND_BUS_MASTER;
    } else {
        command &= ~PCI_COMMAND_BUS_MASTER;
    }
    
    pci_config_write_word(info->address.bus, info->address.device, 
                         info->address.function, PCI_CONFIG_COMMAND, command);
    
    return PCI_SUCCESS;
}

/* ================================
 * Utility Functions
 * ================================ */

/**
 * Convert PCI class to device manager class
 */
device_class_t pci_class_to_device_class(uint8_t pci_class) {
    switch (pci_class) {
        case PCI_CLASS_MASS_STORAGE:    return DEVICE_CLASS_STORAGE;
        case PCI_CLASS_NETWORK:         return DEVICE_CLASS_NETWORK;
        case PCI_CLASS_DISPLAY:         return DEVICE_CLASS_DISPLAY;
        case PCI_CLASS_INPUT:           return DEVICE_CLASS_INPUT;
        case PCI_CLASS_MULTIMEDIA:      return DEVICE_CLASS_AUDIO;
        case PCI_CLASS_BRIDGE:          return DEVICE_CLASS_BRIDGE;
        case PCI_CLASS_COMMUNICATION:   return DEVICE_CLASS_COMM;
        case PCI_CLASS_SYSTEM:          return DEVICE_CLASS_SYSTEM;
        case PCI_CLASS_PROCESSOR:       return DEVICE_CLASS_PROCESSOR;
        case PCI_CLASS_SERIAL_BUS:      return DEVICE_CLASS_SERIAL_BUS;
        default:                        return DEVICE_CLASS_UNKNOWN;
    }
}

/**
 * Convert PCI subclass to device type
 */
device_type_t pci_subclass_to_device_type(uint8_t pci_class, uint8_t subclass) {
    switch (pci_class) {
        case PCI_CLASS_MASS_STORAGE:
            switch (subclass) {
                case PCI_SUBCLASS_IDE:      return DEVICE_TYPE_IDE;
                case PCI_SUBCLASS_SATA:     return DEVICE_TYPE_SATA;
                case PCI_SUBCLASS_SCSI:     return DEVICE_TYPE_SCSI;
                case PCI_SUBCLASS_NVME:     return DEVICE_TYPE_NVME;
                default:                    return DEVICE_TYPE_UNKNOWN;
            }
        case PCI_CLASS_NETWORK:
            switch (subclass) {
                case PCI_SUBCLASS_ETHERNET: return DEVICE_TYPE_ETHERNET;
                case PCI_SUBCLASS_WIFI:     return DEVICE_TYPE_WIFI;
                default:                    return DEVICE_TYPE_UNKNOWN;
            }
        case PCI_CLASS_DISPLAY:
            return DEVICE_TYPE_VGA;
        default:
            return DEVICE_TYPE_UNKNOWN;
    }
}

/**
 * Get PCI class name
 */
const char* pci_class_name(uint8_t class_code) {
    switch (class_code) {
        case PCI_CLASS_UNCLASSIFIED:    return "Unclassified";
        case PCI_CLASS_MASS_STORAGE:    return "Mass Storage";
        case PCI_CLASS_NETWORK:         return "Network";
        case PCI_CLASS_DISPLAY:         return "Display";
        case PCI_CLASS_MULTIMEDIA:      return "Multimedia";
        case PCI_CLASS_MEMORY:          return "Memory";
        case PCI_CLASS_BRIDGE:          return "Bridge";
        case PCI_CLASS_COMMUNICATION:   return "Communication";
        case PCI_CLASS_SYSTEM:          return "System";
        case PCI_CLASS_INPUT:           return "Input";
        case PCI_CLASS_PROCESSOR:       return "Processor";
        case PCI_CLASS_SERIAL_BUS:      return "Serial Bus";
        case PCI_CLASS_WIRELESS:        return "Wireless";
        default:                        return "Unknown";
    }
}

/* ================================
 * PCI Initialization
 * ================================ */

/**
 * Initialize PCI bus driver
 */
int pci_init(void) {
    if (g_pci_initialized) {
        return PCI_SUCCESS;
    }
    
    debug_print("PCI: Initializing PCI bus driver\n");
    
    /* Reset statistics */
    memset(&g_pci_stats, 0, sizeof(g_pci_stats));
    
    /* Test PCI configuration mechanism */
    uint32_t test_address = pci_make_config_address(0, 0, 0, 0);
    outl(PCI_CONFIG_ADDRESS, test_address);
    uint32_t read_back = inl(PCI_CONFIG_ADDRESS);
    
    if (read_back != test_address) {
        debug_print("PCI: Configuration mechanism not available\n");
        return PCI_ERROR_ACCESS_DENIED;
    }
    
    g_pci_initialized = true;
    
    /* Perform initial bus scan */
    pci_scan_all_buses();
    
    debug_print("PCI: Driver initialized successfully\n");
    return PCI_SUCCESS;
}

/**
 * Get PCI statistics
 */
void pci_get_stats(pci_stats_t* stats) {
    if (stats) {
        memcpy(stats, &g_pci_stats, sizeof(pci_stats_t));
    }
}

/**
 * Print all PCI devices
 */
void pci_print_all_devices(void) {
    debug_print("=== PCI Device Enumeration ===\n");
    debug_print("Total devices: %u\n", g_pci_stats.total_devices);
    debug_print("Total functions: %u\n", g_pci_stats.total_functions);
    debug_print("Buses scanned: %u\n", g_pci_stats.buses_scanned);
    debug_print("Storage devices: %u\n", g_pci_stats.storage_devices);
    debug_print("Network devices: %u\n", g_pci_stats.network_devices);
    debug_print("Display devices: %u\n", g_pci_stats.display_devices);
}
