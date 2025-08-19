/* IKOS USB Core Framework Implementation
 * 
 * Core USB driver framework for IKOS operating system.
 * This implementation provides:
 * - USB device enumeration and management
 * - USB transfer handling and management
 * - Host controller abstraction layer
 * - USB driver registration and dispatch
 * - USB configuration and interface management
 */

#include "usb.h"
#include "interrupts.h"
#include "memory.h"
#include "string.h"
#include "stdio.h"
#include <stddef.h>

/* Global USB state */
static usb_bus_t usb_buses[USB_MAX_BUSES];
static usb_device_t* usb_devices[USB_MAX_DEVICES];
static usb_driver_t* usb_drivers[USB_MAX_DRIVERS];
static uint8_t num_buses = 0;
static uint8_t num_devices = 0;
static uint8_t num_drivers = 0;

/* Transfer management */
static usb_transfer_t* active_transfers[USB_MAX_TRANSFERS];
static uint8_t num_active_transfers = 0;

/* Synchronization */
static bool usb_initialized = false;

/* Internal function prototypes */
static int usb_enumerate_device(usb_device_t* device);
static int usb_configure_device(usb_device_t* device);
static void usb_handle_transfer_complete(usb_transfer_t* transfer);
static usb_driver_t* usb_find_driver(usb_device_t* device);
static int usb_parse_configuration(usb_device_t* device, uint8_t config_index);

/* USB Core Initialization */
int usb_init(void) {
    if (usb_initialized) {
        return USB_SUCCESS;
    }
    
    printf("[USB] Initializing USB core framework\n");
    
    /* Clear global state */
    memset(usb_buses, 0, sizeof(usb_buses));
    memset(usb_devices, 0, sizeof(usb_devices));
    memset(usb_drivers, 0, sizeof(usb_drivers));
    memset(active_transfers, 0, sizeof(active_transfers));
    
    num_buses = 0;
    num_devices = 0;
    num_drivers = 0;
    num_active_transfers = 0;
    
    usb_initialized = true;
    
    printf("[USB] USB core framework initialized\n");
    return USB_SUCCESS;
}

/* USB Core Shutdown */
void usb_shutdown(void) {
    if (!usb_initialized) {
        return;
    }
    
    printf("[USB] Shutting down USB core framework\n");
    
    /* Disconnect all devices */
    for (int i = 0; i < num_devices; i++) {
        if (usb_devices[i]) {
            usb_disconnect_device(usb_devices[i]);
        }
    }
    
    /* Cancel all active transfers */
    for (int i = 0; i < num_active_transfers; i++) {
        if (active_transfers[i]) {
            usb_cancel_transfer(active_transfers[i]);
        }
    }
    
    /* Shutdown all buses */
    for (int i = 0; i < num_buses; i++) {
        if (usb_buses[i].hci && usb_buses[i].hci->shutdown) {
            usb_buses[i].hci->shutdown(&usb_buses[i]);
        }
    }
    
    usb_initialized = false;
    printf("[USB] USB core framework shutdown complete\n");
}

/* Bus Management */
int usb_register_bus(usb_bus_t* bus) {
    if (!bus || num_buses >= USB_MAX_BUSES) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Find empty slot */
    for (int i = 0; i < USB_MAX_BUSES; i++) {
        if (!usb_buses[i].hci) {
            memcpy(&usb_buses[i], bus, sizeof(usb_bus_t));
            usb_buses[i].bus_id = i;
            usb_buses[i].state = USB_BUS_STATE_ACTIVE;
            num_buses++;
            
            printf("[USB] Registered USB bus %d (%s)\n", i, bus->name);
            
            /* Initialize host controller */
            if (bus->hci && bus->hci->init) {
                int result = bus->hci->init(&usb_buses[i]);
                if (result != USB_SUCCESS) {
                    printf("[USB] Failed to initialize host controller: %d\n", result);
                    memset(&usb_buses[i], 0, sizeof(usb_bus_t));
                    num_buses--;
                    return result;
                }
            }
            
            /* Start root hub enumeration */
            usb_enumerate_root_hub(&usb_buses[i]);
            
            return USB_SUCCESS;
        }
    }
    
    return USB_ERROR_NO_RESOURCES;
}

void usb_unregister_bus(usb_bus_t* bus) {
    if (!bus) {
        return;
    }
    
    printf("[USB] Unregistering USB bus %d\n", bus->bus_id);
    
    /* Disconnect all devices on this bus */
    for (int i = 0; i < num_devices; i++) {
        if (usb_devices[i] && usb_devices[i]->bus == bus) {
            usb_disconnect_device(usb_devices[i]);
        }
    }
    
    /* Shutdown host controller */
    if (bus->hci && bus->hci->shutdown) {
        bus->hci->shutdown(bus);
    }
    
    memset(bus, 0, sizeof(usb_bus_t));
    num_buses--;
}

/* Device Management */
usb_device_t* usb_alloc_device(usb_bus_t* bus, uint8_t address) {
    if (!bus || address > USB_MAX_ADDRESS) {
        return NULL;
    }
    
    /* Find empty device slot */
    for (int i = 0; i < USB_MAX_DEVICES; i++) {
        if (!usb_devices[i]) {
            usb_device_t* device = (usb_device_t*)malloc(sizeof(usb_device_t));
            if (!device) {
                return NULL;
            }
            
            memset(device, 0, sizeof(usb_device_t));
            device->device_id = i;
            device->address = address;
            device->bus = bus;
            device->state = USB_DEVICE_STATE_DEFAULT;
            device->speed = USB_SPEED_UNKNOWN;
            
            usb_devices[i] = device;
            num_devices++;
            
            printf("[USB] Allocated device slot %d (address %d)\n", i, address);
            return device;
        }
    }
    
    return NULL;
}

void usb_free_device(usb_device_t* device) {
    if (!device) {
        return;
    }
    
    printf("[USB] Freeing device %d\n", device->device_id);
    
    /* Cancel any active transfers */
    for (int i = 0; i < num_active_transfers; i++) {
        if (active_transfers[i] && active_transfers[i]->device == device) {
            usb_cancel_transfer(active_transfers[i]);
        }
    }
    
    /* Free configuration descriptors */
    for (int i = 0; i < device->num_configurations; i++) {
        if (device->configurations[i]) {
            free(device->configurations[i]);
        }
    }
    
    /* Remove from device list */
    usb_devices[device->device_id] = NULL;
    num_devices--;
    
    free(device);
}

int usb_connect_device(usb_device_t* device) {
    if (!device) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    printf("[USB] Connecting device %d\n", device->device_id);
    
    /* Enumerate device */
    int result = usb_enumerate_device(device);
    if (result != USB_SUCCESS) {
        printf("[USB] Device enumeration failed: %d\n", result);
        return result;
    }
    
    /* Configure device */
    result = usb_configure_device(device);
    if (result != USB_SUCCESS) {
        printf("[USB] Device configuration failed: %d\n", result);
        return result;
    }
    
    /* Find and bind driver */
    usb_driver_t* driver = usb_find_driver(device);
    if (driver) {
        printf("[USB] Binding driver '%s' to device %d\n", driver->name, device->device_id);
        
        device->driver = driver;
        if (driver->probe) {
            result = driver->probe(device);
            if (result != USB_SUCCESS) {
                printf("[USB] Driver probe failed: %d\n", result);
                device->driver = NULL;
                return result;
            }
        }
    } else {
        printf("[USB] No driver found for device %d\n", device->device_id);
    }
    
    device->state = USB_DEVICE_STATE_CONFIGURED;
    printf("[USB] Device %d connected successfully\n", device->device_id);
    
    return USB_SUCCESS;
}

void usb_disconnect_device(usb_device_t* device) {
    if (!device) {
        return;
    }
    
    printf("[USB] Disconnecting device %d\n", device->device_id);
    
    /* Notify driver */
    if (device->driver && device->driver->disconnect) {
        device->driver->disconnect(device);
    }
    
    device->state = USB_DEVICE_STATE_DISCONNECTED;
    device->driver = NULL;
    
    /* Free device resources */
    usb_free_device(device);
}

/* Device Enumeration */
static int usb_enumerate_device(usb_device_t* device) {
    printf("[USB] Enumerating device %d\n", device->device_id);
    
    /* Get device descriptor */
    int result = usb_get_device_descriptor(device, &device->device_desc);
    if (result != USB_SUCCESS) {
        printf("[USB] Failed to get device descriptor: %d\n", result);
        return result;
    }
    
    printf("[USB] Device descriptor: VID=%04X PID=%04X Class=%02X\n",
           device->device_desc.idVendor,
           device->device_desc.idProduct,
           device->device_desc.bDeviceClass);
    
    /* Set device address if needed */
    if (device->address == 0) {
        uint8_t new_address = usb_allocate_address(device->bus);
        if (new_address == 0) {
            printf("[USB] Failed to allocate device address\n");
            return USB_ERROR_NO_RESOURCES;
        }
        
        result = usb_set_address(device, new_address);
        if (result != USB_SUCCESS) {
            printf("[USB] Failed to set device address: %d\n", result);
            return result;
        }
        
        device->address = new_address;
        printf("[USB] Device address set to %d\n", new_address);
    }
    
    /* Get configuration descriptors */
    for (int i = 0; i < device->device_desc.bNumConfigurations; i++) {
        result = usb_parse_configuration(device, i);
        if (result != USB_SUCCESS) {
            printf("[USB] Failed to parse configuration %d: %d\n", i, result);
            return result;
        }
    }
    
    device->state = USB_DEVICE_STATE_ADDRESS;
    return USB_SUCCESS;
}

/* Configuration Parsing */
static int usb_parse_configuration(usb_device_t* device, uint8_t config_index) {
    uint8_t buffer[512];
    usb_config_descriptor_t* config_desc = (usb_config_descriptor_t*)buffer;
    
    /* Get configuration descriptor */
    int result = usb_get_configuration_descriptor(device, config_index, buffer, sizeof(buffer));
    if (result != USB_SUCCESS) {
        return result;
    }
    
    /* Allocate and store configuration */
    uint16_t total_length = config_desc->wTotalLength;
    device->configurations[config_index] = malloc(total_length);
    if (!device->configurations[config_index]) {
        return USB_ERROR_NO_MEMORY;
    }
    
    /* Get full configuration descriptor */
    result = usb_get_configuration_descriptor(device, config_index,
                                             device->configurations[config_index],
                                             total_length);
    if (result != USB_SUCCESS) {
        free(device->configurations[config_index]);
        device->configurations[config_index] = NULL;
        return result;
    }
    
    device->num_configurations = config_index + 1;
    
    printf("[USB] Configuration %d: %d interfaces, %d endpoints\n",
           config_index, config_desc->bNumInterfaces, 
           /* Count endpoints from interfaces */0);
    
    return USB_SUCCESS;
}

/* Device Configuration */
static int usb_configure_device(usb_device_t* device) {
    if (device->num_configurations == 0) {
        return USB_ERROR_NO_CONFIG;
    }
    
    /* Use first configuration by default */
    usb_config_descriptor_t* config = (usb_config_descriptor_t*)device->configurations[0];
    
    int result = usb_set_configuration(device, config->bConfigurationValue);
    if (result != USB_SUCCESS) {
        printf("[USB] Failed to set configuration: %d\n", result);
        return result;
    }
    
    device->current_config = 0;
    device->state = USB_DEVICE_STATE_CONFIGURED;
    
    printf("[USB] Device configured with configuration %d\n", config->bConfigurationValue);
    return USB_SUCCESS;
}

/* Transfer Management */
usb_transfer_t* usb_alloc_transfer(usb_device_t* device, uint8_t endpoint,
                                   uint8_t type, uint16_t max_packet_size) {
    if (!device || num_active_transfers >= USB_MAX_TRANSFERS) {
        return NULL;
    }
    
    usb_transfer_t* transfer = malloc(sizeof(usb_transfer_t));
    if (!transfer) {
        return NULL;
    }
    
    memset(transfer, 0, sizeof(usb_transfer_t));
    transfer->device = device;
    transfer->endpoint = endpoint;
    transfer->type = type;
    transfer->max_packet_size = max_packet_size;
    transfer->state = USB_TRANSFER_STATE_IDLE;
    
    /* Add to active transfers */
    for (int i = 0; i < USB_MAX_TRANSFERS; i++) {
        if (!active_transfers[i]) {
            active_transfers[i] = transfer;
            transfer->transfer_id = i;
            num_active_transfers++;
            break;
        }
    }
    
    return transfer;
}

void usb_free_transfer(usb_transfer_t* transfer) {
    if (!transfer) {
        return;
    }
    
    /* Cancel if active */
    if (transfer->state == USB_TRANSFER_STATE_ACTIVE) {
        usb_cancel_transfer(transfer);
    }
    
    /* Remove from active transfers */
    if (transfer->transfer_id < USB_MAX_TRANSFERS) {
        active_transfers[transfer->transfer_id] = NULL;
        num_active_transfers--;
    }
    
    free(transfer);
}

int usb_submit_transfer(usb_transfer_t* transfer) {
    if (!transfer || !transfer->device || !transfer->device->bus->hci) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    if (transfer->state != USB_TRANSFER_STATE_IDLE) {
        return USB_ERROR_BUSY;
    }
    
    usb_hci_t* hci = transfer->device->bus->hci;
    if (!hci->submit_transfer) {
        return USB_ERROR_NOT_SUPPORTED;
    }
    
    transfer->state = USB_TRANSFER_STATE_ACTIVE;
    transfer->status = USB_TRANSFER_STATUS_PENDING;
    
    int result = hci->submit_transfer(transfer->device->bus, transfer);
    if (result != USB_SUCCESS) {
        transfer->state = USB_TRANSFER_STATE_IDLE;
        transfer->status = USB_TRANSFER_STATUS_ERROR;
    }
    
    return result;
}

int usb_cancel_transfer(usb_transfer_t* transfer) {
    if (!transfer || !transfer->device || !transfer->device->bus->hci) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    if (transfer->state != USB_TRANSFER_STATE_ACTIVE) {
        return USB_SUCCESS;
    }
    
    usb_hci_t* hci = transfer->device->bus->hci;
    if (hci->cancel_transfer) {
        hci->cancel_transfer(transfer->device->bus, transfer);
    }
    
    transfer->state = USB_TRANSFER_STATE_IDLE;
    transfer->status = USB_TRANSFER_STATUS_CANCELLED;
    
    return USB_SUCCESS;
}

/* Transfer Completion Handling */
static void usb_handle_transfer_complete(usb_transfer_t* transfer) {
    if (!transfer) {
        return;
    }
    
    transfer->state = USB_TRANSFER_STATE_COMPLETE;
    
    /* Call completion callback */
    if (transfer->callback) {
        transfer->callback(transfer);
    }
}

void usb_transfer_complete(usb_transfer_t* transfer, int status, uint16_t actual_length) {
    if (!transfer) {
        return;
    }
    
    transfer->status = status;
    transfer->actual_length = actual_length;
    
    usb_handle_transfer_complete(transfer);
}

/* Driver Management */
int usb_register_driver(usb_driver_t* driver) {
    if (!driver || num_drivers >= USB_MAX_DRIVERS) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Find empty slot */
    for (int i = 0; i < USB_MAX_DRIVERS; i++) {
        if (!usb_drivers[i]) {
            usb_drivers[i] = driver;
            num_drivers++;
            
            printf("[USB] Registered driver '%s'\n", driver->name);
            
            /* Check existing devices for matches */
            for (int j = 0; j < num_devices; j++) {
                if (usb_devices[j] && !usb_devices[j]->driver) {
                    if (usb_driver_matches(driver, usb_devices[j])) {
                        printf("[USB] Binding driver '%s' to existing device %d\n",
                               driver->name, j);
                        
                        usb_devices[j]->driver = driver;
                        if (driver->probe) {
                            driver->probe(usb_devices[j]);
                        }
                    }
                }
            }
            
            return USB_SUCCESS;
        }
    }
    
    return USB_ERROR_NO_RESOURCES;
}

void usb_unregister_driver(usb_driver_t* driver) {
    if (!driver) {
        return;
    }
    
    printf("[USB] Unregistering driver '%s'\n", driver->name);
    
    /* Disconnect all devices using this driver */
    for (int i = 0; i < num_devices; i++) {
        if (usb_devices[i] && usb_devices[i]->driver == driver) {
            if (driver->disconnect) {
                driver->disconnect(usb_devices[i]);
            }
            usb_devices[i]->driver = NULL;
        }
    }
    
    /* Remove from driver list */
    for (int i = 0; i < USB_MAX_DRIVERS; i++) {
        if (usb_drivers[i] == driver) {
            usb_drivers[i] = NULL;
            num_drivers--;
            break;
        }
    }
}

/* Driver Matching */
static usb_driver_t* usb_find_driver(usb_device_t* device) {
    for (int i = 0; i < USB_MAX_DRIVERS; i++) {
        if (usb_drivers[i] && usb_driver_matches(usb_drivers[i], device)) {
            return usb_drivers[i];
        }
    }
    return NULL;
}

bool usb_driver_matches(usb_driver_t* driver, usb_device_t* device) {
    if (!driver || !device) {
        return false;
    }
    
    /* Check ID table */
    if (driver->id_table) {
        usb_device_id_t* id = driver->id_table;
        while (id->vendor_id || id->product_id || id->device_class) {
            bool match = true;
            
            if (id->vendor_id && id->vendor_id != device->device_desc.idVendor) {
                match = false;
            }
            if (id->product_id && id->product_id != device->device_desc.idProduct) {
                match = false;
            }
            if (id->device_class && id->device_class != device->device_desc.bDeviceClass) {
                match = false;
            }
            if (id->device_subclass && id->device_subclass != device->device_desc.bDeviceSubClass) {
                match = false;
            }
            if (id->device_protocol && id->device_protocol != device->device_desc.bDeviceProtocol) {
                match = false;
            }
            
            if (match) {
                return true;
            }
            id++;
        }
    }
    
    return false;
}

/* Utility Functions */
uint8_t usb_allocate_address(usb_bus_t* bus) {
    if (!bus) {
        return 0;
    }
    
    /* Find unused address */
    for (uint8_t addr = 1; addr <= USB_MAX_ADDRESS; addr++) {
        bool used = false;
        for (int i = 0; i < num_devices; i++) {
            if (usb_devices[i] && usb_devices[i]->bus == bus && 
                usb_devices[i]->address == addr) {
                used = true;
                break;
            }
        }
        if (!used) {
            return addr;
        }
    }
    
    return 0; /* No free address */
}

const char* usb_speed_string(uint8_t speed) {
    switch (speed) {
        case USB_SPEED_LOW:    return "Low Speed (1.5 Mbps)";
        case USB_SPEED_FULL:   return "Full Speed (12 Mbps)";
        case USB_SPEED_HIGH:   return "High Speed (480 Mbps)";
        case USB_SPEED_SUPER:  return "Super Speed (5 Gbps)";
        default:               return "Unknown Speed";
    }
}

const char* usb_class_string(uint8_t class_code) {
    switch (class_code) {
        case USB_CLASS_AUDIO:           return "Audio";
        case USB_CLASS_CDC:             return "Communications";
        case USB_CLASS_HID:             return "Human Interface Device";
        case USB_CLASS_PHYSICAL:        return "Physical";
        case USB_CLASS_IMAGE:           return "Image";
        case USB_CLASS_PRINTER:         return "Printer";
        case USB_CLASS_MASS_STORAGE:    return "Mass Storage";
        case USB_CLASS_HUB:             return "Hub";
        case USB_CLASS_CDC_DATA:        return "CDC-Data";
        case USB_CLASS_SMART_CARD:      return "Smart Card";
        case USB_CLASS_CONTENT_SECURITY: return "Content Security";
        case USB_CLASS_VIDEO:           return "Video";
        case USB_CLASS_PERSONAL_HEALTHCARE: return "Personal Healthcare";
        case USB_CLASS_AUDIO_VIDEO:     return "Audio/Video";
        case USB_CLASS_BILLBOARD:       return "Billboard";
        case USB_CLASS_DIAGNOSTIC:      return "Diagnostic";
        case USB_CLASS_WIRELESS:        return "Wireless";
        case USB_CLASS_MISCELLANEOUS:   return "Miscellaneous";
        case USB_CLASS_APPLICATION:     return "Application Specific";
        case USB_CLASS_VENDOR_SPECIFIC: return "Vendor Specific";
        default:                        return "Unknown";
    }
}

/* Root Hub Enumeration */
void usb_enumerate_root_hub(usb_bus_t* bus) {
    if (!bus || !bus->hci) {
        return;
    }
    
    printf("[USB] Enumerating root hub on bus %d\n", bus->bus_id);
    
    /* Create root hub device */
    usb_device_t* root_hub = usb_alloc_device(bus, 1);
    if (!root_hub) {
        printf("[USB] Failed to allocate root hub device\n");
        return;
    }
    
    /* Set root hub properties */
    root_hub->speed = USB_SPEED_HIGH; /* Root hub is typically high speed */
    root_hub->state = USB_DEVICE_STATE_CONFIGURED;
    
    /* Initialize device descriptor for root hub */
    memset(&root_hub->device_desc, 0, sizeof(usb_device_descriptor_t));
    root_hub->device_desc.bLength = sizeof(usb_device_descriptor_t);
    root_hub->device_desc.bDescriptorType = USB_DESC_DEVICE;
    root_hub->device_desc.bcdUSB = 0x0200; /* USB 2.0 */
    root_hub->device_desc.bDeviceClass = USB_CLASS_HUB;
    root_hub->device_desc.bMaxPacketSize0 = 64;
    root_hub->device_desc.idVendor = 0x1D6B; /* Linux Foundation */
    root_hub->device_desc.idProduct = 0x0002; /* 2.0 root hub */
    root_hub->device_desc.bcdDevice = 0x0100;
    root_hub->device_desc.bNumConfigurations = 1;
    
    bus->root_hub = root_hub;
    
    /* Start port scanning */
    if (bus->hci->scan_ports) {
        bus->hci->scan_ports(bus);
    }
}

/* Status and Debug Functions */
void usb_dump_device_info(usb_device_t* device) {
    if (!device) {
        return;
    }
    
    printf("USB Device %d Information:\n", device->device_id);
    printf("  Address: %d\n", device->address);
    printf("  Speed: %s\n", usb_speed_string(device->speed));
    printf("  State: %d\n", device->state);
    printf("  Class: %s (0x%02X)\n", 
           usb_class_string(device->device_desc.bDeviceClass),
           device->device_desc.bDeviceClass);
    printf("  Vendor ID: 0x%04X\n", device->device_desc.idVendor);
    printf("  Product ID: 0x%04X\n", device->device_desc.idProduct);
    printf("  Configurations: %d\n", device->num_configurations);
    printf("  Driver: %s\n", device->driver ? device->driver->name : "None");
}
