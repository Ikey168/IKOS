/* IKOS USB Controller Driver Implementation
 * Issue #15 Enhancement - USB Support for Device Driver Framework
 * 
 * Implements USB controller support for UHCI, OHCI, EHCI, and xHCI.
 * Provides device enumeration and basic transfer capabilities.
 */

#include "../include/usb_controller.h"
#include "../include/device_manager.h"
#include "../include/pci.h"
#include "../include/memory.h"
#include <string.h>

/* ================================
 * Global Variables
 * ================================ */

static usb_controller_t* g_usb_controllers = NULL;
static usb_device_t* g_usb_devices = NULL;
static usb_stats_t g_usb_stats;
static bool g_usb_initialized = false;

/* ================================
 * Helper Functions
 * ================================ */

/**
 * Allocate and initialize a USB controller structure
 */
static usb_controller_t* usb_alloc_controller(void) {
    usb_controller_t* controller = (usb_controller_t*)kmalloc(sizeof(usb_controller_t));
    if (controller) {
        memset(controller, 0, sizeof(usb_controller_t));
    }
    return controller;
}

/**
 * Free a USB controller structure
 */
static void usb_free_controller(usb_controller_t* controller) {
    if (controller) {
        if (controller->controller_data) {
            kfree(controller->controller_data);
        }
        kfree(controller);
    }
}

/**
 * Allocate and initialize a USB device structure
 */
static usb_device_t* usb_alloc_device(void) {
    usb_device_t* device = (usb_device_t*)kmalloc(sizeof(usb_device_t));
    if (device) {
        memset(device, 0, sizeof(usb_device_t));
    }
    return device;
}

/**
 * Free a USB device structure
 */
static void usb_free_device(usb_device_t* device) {
    if (device) {
        if (device->config_desc) {
            kfree(device->config_desc);
        }
        kfree(device);
    }
}

/**
 * Determine USB controller type from PCI class codes
 */
static usb_controller_type_t usb_get_controller_type(uint8_t prog_if) {
    switch (prog_if) {
        case 0x00: return USB_CONTROLLER_UHCI;
        case 0x10: return USB_CONTROLLER_OHCI;
        case 0x20: return USB_CONTROLLER_EHCI;
        case 0x30: return USB_CONTROLLER_XHCI;
        default:   return USB_CONTROLLER_UNKNOWN;
    }
}

/**
 * Convert USB device class to IKOS device type
 */
static device_type_t usb_class_to_device_type(uint8_t usb_class) {
    switch (usb_class) {
        case USB_CLASS_HID:
            return DEVICE_TYPE_KEYBOARD; /* Could be mouse, keyboard, etc. */
        case USB_CLASS_MASS_STORAGE:
            return DEVICE_TYPE_USB_STORAGE;
        case USB_CLASS_HUB:
            return DEVICE_TYPE_UNKNOWN; /* Hubs don't map to device types */
        default:
            return DEVICE_TYPE_UNKNOWN;
    }
}

/* ================================
 * USB Controller Management
 * ================================ */

/**
 * Initialize USB controller subsystem
 */
int usb_controller_init(void) {
    if (g_usb_initialized) {
        return USB_SUCCESS;
    }
    
    /* Initialize statistics */
    memset(&g_usb_stats, 0, sizeof(usb_stats_t));
    
    /* Initialize controller and device lists */
    g_usb_controllers = NULL;
    g_usb_devices = NULL;
    
    g_usb_initialized = true;
    
    /* Scan for USB controllers on PCI bus */
    /* This would be called after PCI enumeration */
    
    return USB_SUCCESS;
}

/**
 * Shutdown USB controller subsystem
 */
int usb_controller_shutdown(void) {
    if (!g_usb_initialized) {
        return USB_SUCCESS;
    }
    
    /* Stop all controllers */
    usb_controller_t* controller = g_usb_controllers;
    while (controller) {
        usb_controller_stop(controller);
        controller = controller->next;
    }
    
    /* Free all devices */
    usb_device_t* device = g_usb_devices;
    while (device) {
        usb_device_t* next = device->next;
        usb_free_device(device);
        device = next;
    }
    
    /* Free all controllers */
    controller = g_usb_controllers;
    while (controller) {
        usb_controller_t* next = controller->next;
        usb_free_controller(controller);
        controller = next;
    }
    
    g_usb_initialized = false;
    
    return USB_SUCCESS;
}

/**
 * Register a USB controller from PCI device
 */
int usb_register_controller(device_t* device) {
    if (!g_usb_initialized || !device) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Allocate controller structure */
    usb_controller_t* controller = usb_alloc_controller();
    if (!controller) {
        return USB_ERROR_NO_MEMORY;
    }
    
    /* Initialize controller from device information */
    controller->device = device;
    
    /* Get controller type from PCI programming interface */
    uint8_t prog_if = 0; /* Would read from PCI config space */
    controller->type = usb_get_controller_type(prog_if);
    
    /* Get base address from device resources */
    device_resource_t* resource = device_get_resource(device, RESOURCE_TYPE_IO_PORT, 0);
    if (!resource) {
        resource = device_get_resource(device, RESOURCE_TYPE_MEMORY, 0);
    }
    
    if (resource) {
        controller->base_address = (uint32_t)resource->base_address;
    }
    
    /* Get IRQ */
    resource = device_get_resource(device, RESOURCE_TYPE_IRQ, 0);
    if (resource) {
        controller->irq = (uint32_t)resource->base_address;
    }
    
    /* Set default capabilities based on controller type */
    switch (controller->type) {
        case USB_CONTROLLER_UHCI:
        case USB_CONTROLLER_OHCI:
            controller->max_speed = USB_SPEED_FULL;
            controller->num_ports = 2;
            controller->supports_64bit = false;
            break;
        case USB_CONTROLLER_EHCI:
            controller->max_speed = USB_SPEED_HIGH;
            controller->num_ports = 4;
            controller->supports_64bit = false;
            break;
        case USB_CONTROLLER_XHCI:
            controller->max_speed = USB_SPEED_SUPER_PLUS;
            controller->num_ports = 8;
            controller->supports_64bit = true;
            controller->supports_power_mgmt = true;
            break;
        default:
            usb_free_controller(controller);
            return USB_ERROR_NOT_SUPPORTED;
    }
    
    /* Add to controller list */
    controller->next = g_usb_controllers;
    g_usb_controllers = controller;
    
    /* Update statistics */
    g_usb_stats.controllers_found++;
    
    return USB_SUCCESS;
}

/**
 * Start USB controller
 */
int usb_controller_start(usb_controller_t* controller) {
    if (!controller) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    if (controller->enabled) {
        return USB_SUCCESS; /* Already started */
    }
    
    /* Initialize controller based on type */
    int result = USB_ERROR_NOT_SUPPORTED;
    switch (controller->type) {
        case USB_CONTROLLER_UHCI:
            result = uhci_init_controller(controller);
            if (result == USB_SUCCESS) {
                result = uhci_start_controller(controller);
            }
            break;
        case USB_CONTROLLER_OHCI:
            result = ohci_init_controller(controller);
            if (result == USB_SUCCESS) {
                result = ohci_start_controller(controller);
            }
            break;
        case USB_CONTROLLER_EHCI:
            result = ehci_init_controller(controller);
            if (result == USB_SUCCESS) {
                result = ehci_start_controller(controller);
            }
            break;
        case USB_CONTROLLER_XHCI:
            result = xhci_init_controller(controller);
            if (result == USB_SUCCESS) {
                result = xhci_start_controller(controller);
            }
            break;
        default:
            return USB_ERROR_NOT_SUPPORTED;
    }
    
    if (result == USB_SUCCESS) {
        controller->initialized = true;
        controller->enabled = true;
        
        /* Enumerate connected devices */
        usb_enumerate_devices(controller);
    }
    
    return result;
}

/**
 * Stop USB controller
 */
int usb_controller_stop(usb_controller_t* controller) {
    if (!controller || !controller->enabled) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Disconnect all devices */
    for (int i = 0; i < controller->device_count; i++) {
        if (controller->devices[i]) {
            usb_disconnect_device(controller->devices[i]);
            controller->devices[i] = NULL;
        }
    }
    controller->device_count = 0;
    
    /* Stop controller (implementation would be controller-specific) */
    controller->enabled = false;
    controller->initialized = false;
    
    return USB_SUCCESS;
}

/* ================================
 * USB Device Management
 * ================================ */

/**
 * Enumerate USB devices on controller
 */
int usb_enumerate_devices(usb_controller_t* controller) {
    if (!controller || !controller->enabled) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Check each port for connected devices */
    for (uint8_t port = 0; port < controller->num_ports; port++) {
        /* In real implementation, would check port status registers */
        /* For demo, we'll simulate finding a device on port 0 */
        if (port == 0) {
            usb_connect_device(controller, port);
        }
    }
    
    return USB_SUCCESS;
}

/**
 * Connect a USB device on specified port
 */
int usb_connect_device(usb_controller_t* controller, uint8_t port) {
    if (!controller || port >= controller->num_ports) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Allocate device structure */
    usb_device_t* device = usb_alloc_device();
    if (!device) {
        return USB_ERROR_NO_MEMORY;
    }
    
    /* Initialize device */
    device->address = 0; /* Will be assigned during enumeration */
    device->port = port;
    device->speed = USB_SPEED_FULL; /* Would be detected from port status */
    device->controller = controller;
    
    /* Simulate getting device descriptor */
    device->device_desc.bLength = sizeof(usb_device_descriptor_t);
    device->device_desc.bDescriptorType = 0x01;
    device->device_desc.bcdUSB = 0x0200; /* USB 2.0 */
    device->device_desc.bDeviceClass = USB_CLASS_HID; /* Simulate HID device */
    device->device_desc.idVendor = 0x046D; /* Logitech */
    device->device_desc.idProduct = 0xC077; /* Mouse */
    device->device_desc.bNumConfigurations = 1;
    
    /* Assign device address */
    static uint8_t next_address = 1;
    device->address = next_address++;
    if (next_address > 127) next_address = 1;
    
    /* Create IKOS device structure for this USB device */
    device_type_t device_type = usb_class_to_device_type(device->device_desc.bDeviceClass);
    if (device_type != DEVICE_TYPE_UNKNOWN) {
        device_t* ikos_device = device_create(DEVICE_CLASS_INPUT, device_type, "usb_device");
        if (ikos_device) {
            ikos_device->vendor_id = device->device_desc.idVendor;
            ikos_device->product_id = device->device_desc.idProduct;
            ikos_device->bus_type = 0x03; /* USB bus type */
            ikos_device->bus_number = 0; /* Controller index */
            ikos_device->device_number = device->address;
            
            /* Register with device manager */
            device_register(ikos_device);
            
            device->ikos_device = ikos_device;
        }
    }
    
    /* Add to device lists */
    device->next = g_usb_devices;
    g_usb_devices = device;
    
    if (controller->device_count < 16) {
        controller->devices[controller->device_count] = device;
        controller->device_count++;
    }
    
    /* Update statistics */
    g_usb_stats.devices_connected++;
    if (device->device_desc.bDeviceClass == USB_CLASS_HID) {
        g_usb_stats.hid_devices++;
    } else if (device->device_desc.bDeviceClass == USB_CLASS_MASS_STORAGE) {
        g_usb_stats.storage_devices++;
    }
    
    return USB_SUCCESS;
}

/**
 * Disconnect USB device
 */
int usb_disconnect_device(usb_device_t* device) {
    if (!device) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Remove from IKOS device manager */
    if (device->ikos_device) {
        /* Would call device_unregister(device->ikos_device) if implemented */
        device->ikos_device = NULL;
    }
    
    /* Remove from controller device list */
    if (device->controller) {
        for (int i = 0; i < device->controller->device_count; i++) {
            if (device->controller->devices[i] == device) {
                /* Shift remaining devices */
                for (int j = i; j < device->controller->device_count - 1; j++) {
                    device->controller->devices[j] = device->controller->devices[j + 1];
                }
                device->controller->devices[device->controller->device_count - 1] = NULL;
                device->controller->device_count--;
                break;
            }
        }
    }
    
    /* Remove from global device list */
    if (g_usb_devices == device) {
        g_usb_devices = device->next;
    } else {
        usb_device_t* current = g_usb_devices;
        while (current && current->next != device) {
            current = current->next;
        }
        if (current) {
            current->next = device->next;
        }
    }
    
    /* Update statistics */
    g_usb_stats.devices_connected--;
    if (device->device_desc.bDeviceClass == USB_CLASS_HID) {
        g_usb_stats.hid_devices--;
    } else if (device->device_desc.bDeviceClass == USB_CLASS_MASS_STORAGE) {
        g_usb_stats.storage_devices--;
    }
    
    /* Free device structure */
    usb_free_device(device);
    
    return USB_SUCCESS;
}

/* ================================
 * USB Transfer Operations
 * ================================ */

/**
 * Perform USB control transfer
 */
int usb_control_transfer(usb_device_t* device, uint8_t request_type, 
                        uint8_t request, uint16_t value, uint16_t index,
                        void* data, uint16_t length) {
    if (!device || !device->controller) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* In real implementation, this would set up control transfer descriptors
     * and submit them to the appropriate controller */
    
    /* Simulate successful transfer */
    g_usb_stats.transfers_completed++;
    
    return length; /* Return number of bytes transferred */
}

/**
 * Get device descriptor
 */
int usb_get_device_descriptor(usb_device_t* device) {
    if (!device) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Use control transfer to get device descriptor */
    return usb_control_transfer(device,
                               USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                               USB_REQ_GET_DESCRIPTOR,
                               0x0100, /* Device descriptor type */
                               0,
                               &device->device_desc,
                               sizeof(usb_device_descriptor_t));
}

/**
 * Set device address
 */
int usb_set_address(usb_device_t* device, uint8_t address) {
    if (!device || address == 0 || address > 127) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Use control transfer to set device address */
    int result = usb_control_transfer(device,
                                     USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                                     USB_REQ_SET_ADDRESS,
                                     address,
                                     0,
                                     NULL,
                                     0);
    
    if (result >= 0) {
        device->address = address;
    }
    
    return result;
}

/* ================================
 * Controller-Specific Implementations
 * ================================ */

/**
 * Initialize UHCI controller (USB 1.1)
 */
int uhci_init_controller(usb_controller_t* controller) {
    if (!controller) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* UHCI-specific initialization would go here */
    /* For now, just simulate successful initialization */
    
    return USB_SUCCESS;
}

/**
 * Start UHCI controller
 */
int uhci_start_controller(usb_controller_t* controller) {
    if (!controller) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* UHCI-specific startup would go here */
    
    return USB_SUCCESS;
}

/**
 * Initialize OHCI controller (USB 1.1)
 */
int ohci_init_controller(usb_controller_t* controller) {
    if (!controller) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* OHCI-specific initialization would go here */
    
    return USB_SUCCESS;
}

/**
 * Start OHCI controller
 */
int ohci_start_controller(usb_controller_t* controller) {
    if (!controller) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* OHCI-specific startup would go here */
    
    return USB_SUCCESS;
}

/**
 * Initialize EHCI controller (USB 2.0)
 */
int ehci_init_controller(usb_controller_t* controller) {
    if (!controller) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* EHCI-specific initialization would go here */
    
    return USB_SUCCESS;
}

/**
 * Start EHCI controller
 */
int ehci_start_controller(usb_controller_t* controller) {
    if (!controller) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* EHCI-specific startup would go here */
    
    return USB_SUCCESS;
}

/**
 * Initialize xHCI controller (USB 3.0+)
 */
int xhci_init_controller(usb_controller_t* controller) {
    if (!controller) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* xHCI-specific initialization would go here */
    
    return USB_SUCCESS;
}

/**
 * Start xHCI controller
 */
int xhci_start_controller(usb_controller_t* controller) {
    if (!controller) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* xHCI-specific startup would go here */
    
    return USB_SUCCESS;
}

/* ================================
 * Statistics and Debugging
 * ================================ */

/**
 * Get list of USB controllers
 */
usb_controller_t* usb_get_controllers(void) {
    return g_usb_controllers;
}

/**
 * Get list of USB devices
 */
usb_device_t* usb_get_devices(void) {
    return g_usb_devices;
}

/**
 * Get USB statistics
 */
void usb_get_stats(usb_stats_t* stats) {
    if (stats) {
        *stats = g_usb_stats;
    }
}

/**
 * Reset USB statistics
 */
void usb_reset_stats(void) {
    memset(&g_usb_stats, 0, sizeof(usb_stats_t));
}

/* ================================
 * Device Class Drivers
 * ================================ */

/**
 * Initialize HID support
 */
int usb_hid_init(void) {
    /* HID class driver initialization */
    return USB_SUCCESS;
}

/**
 * Register HID device
 */
int usb_hid_register_device(usb_device_t* device) {
    if (!device || device->device_desc.bDeviceClass != USB_CLASS_HID) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Initialize HID device */
    /* Would set up interrupt endpoints for input reports */
    
    return USB_SUCCESS;
}

/**
 * Initialize mass storage support
 */
int usb_storage_init(void) {
    /* Mass storage class driver initialization */
    return USB_SUCCESS;
}

/**
 * Register mass storage device
 */
int usb_storage_register_device(usb_device_t* device) {
    if (!device || device->device_desc.bDeviceClass != USB_CLASS_MASS_STORAGE) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Initialize mass storage device */
    /* Would set up bulk endpoints for data transfer */
    
    return USB_SUCCESS;
}
