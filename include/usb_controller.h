/* IKOS USB Controller Driver
 * Issue #15 Enhancement - USB Support for Device Driver Framework
 * 
 * Provides USB controller support including UHCI, OHCI, EHCI, and xHCI.
 * Enables USB device detection and enumeration through the device framework.
 */

#ifndef USB_CONTROLLER_H
#define USB_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "device_manager.h"

/* ================================
 * USB Controller Types
 * ================================ */

typedef enum {
    USB_CONTROLLER_UHCI = 0x00,    /* Universal Host Controller Interface (USB 1.1) */
    USB_CONTROLLER_OHCI = 0x10,    /* Open Host Controller Interface (USB 1.1) */
    USB_CONTROLLER_EHCI = 0x20,    /* Enhanced Host Controller Interface (USB 2.0) */
    USB_CONTROLLER_XHCI = 0x30,    /* eXtensible Host Controller Interface (USB 3.0+) */
    USB_CONTROLLER_UNKNOWN = 0xFF
} usb_controller_type_t;

typedef enum {
    USB_SPEED_LOW    = 0,  /* 1.5 Mbps (USB 1.0) */
    USB_SPEED_FULL   = 1,  /* 12 Mbps (USB 1.1) */
    USB_SPEED_HIGH   = 2,  /* 480 Mbps (USB 2.0) */
    USB_SPEED_SUPER  = 3,  /* 5 Gbps (USB 3.0) */
    USB_SPEED_SUPER_PLUS = 4 /* 10 Gbps (USB 3.1+) */
} usb_speed_t;

/* ================================
 * USB Device Descriptors
 * ================================ */

#pragma pack(push, 1)

typedef struct {
    uint8_t bLength;            /* Size of this descriptor */
    uint8_t bDescriptorType;    /* Device descriptor type (0x01) */
    uint16_t bcdUSB;            /* USB specification version */
    uint8_t bDeviceClass;       /* Device class code */
    uint8_t bDeviceSubClass;    /* Device subclass code */
    uint8_t bDeviceProtocol;    /* Device protocol code */
    uint8_t bMaxPacketSize0;    /* Maximum packet size for endpoint 0 */
    uint16_t idVendor;          /* Vendor ID */
    uint16_t idProduct;         /* Product ID */
    uint16_t bcdDevice;         /* Device release number */
    uint8_t iManufacturer;      /* Manufacturer string index */
    uint8_t iProduct;           /* Product string index */
    uint8_t iSerialNumber;      /* Serial number string index */
    uint8_t bNumConfigurations; /* Number of configurations */
} usb_device_descriptor_t;

typedef struct {
    uint8_t bLength;            /* Size of this descriptor */
    uint8_t bDescriptorType;    /* Configuration descriptor type (0x02) */
    uint16_t wTotalLength;      /* Total length of data for this configuration */
    uint8_t bNumInterfaces;     /* Number of interfaces */
    uint8_t bConfigurationValue; /* Configuration value */
    uint8_t iConfiguration;     /* Configuration string index */
    uint8_t bmAttributes;       /* Configuration characteristics */
    uint8_t bMaxPower;          /* Maximum power consumption (2mA units) */
} usb_config_descriptor_t;

typedef struct {
    uint8_t bLength;            /* Size of this descriptor */
    uint8_t bDescriptorType;    /* Interface descriptor type (0x04) */
    uint8_t bInterfaceNumber;   /* Interface number */
    uint8_t bAlternateSetting;  /* Alternate setting number */
    uint8_t bNumEndpoints;      /* Number of endpoints */
    uint8_t bInterfaceClass;    /* Interface class code */
    uint8_t bInterfaceSubClass; /* Interface subclass code */
    uint8_t bInterfaceProtocol; /* Interface protocol code */
    uint8_t iInterface;         /* Interface string index */
} usb_interface_descriptor_t;

typedef struct {
    uint8_t bLength;            /* Size of this descriptor */
    uint8_t bDescriptorType;    /* Endpoint descriptor type (0x05) */
    uint8_t bEndpointAddress;   /* Endpoint address */
    uint8_t bmAttributes;       /* Endpoint attributes */
    uint16_t wMaxPacketSize;    /* Maximum packet size */
    uint8_t bInterval;          /* Polling interval */
} usb_endpoint_descriptor_t;

#pragma pack(pop)

/* ================================
 * USB Controller Structure
 * ================================ */

typedef struct usb_controller {
    /* Basic controller information */
    device_t* device;           /* Associated device structure */
    usb_controller_type_t type; /* Controller type */
    uint32_t base_address;      /* Base I/O or memory address */
    uint32_t irq;               /* Interrupt request line */
    
    /* Controller capabilities */
    uint8_t num_ports;          /* Number of USB ports */
    usb_speed_t max_speed;      /* Maximum supported USB speed */
    bool supports_64bit;        /* 64-bit addressing support */
    bool supports_power_mgmt;   /* Power management support */
    
    /* Runtime state */
    bool initialized;           /* Controller initialization state */
    bool enabled;               /* Controller enabled state */
    uint32_t frame_number;      /* Current frame number */
    
    /* Connected devices */
    struct usb_device* devices[16]; /* Connected USB devices (max 16 per controller) */
    uint8_t device_count;       /* Number of connected devices */
    
    /* Statistics */
    uint64_t frames_processed;  /* Total frames processed */
    uint64_t transfers_completed; /* Total transfers completed */
    uint64_t errors_detected;   /* Total errors detected */
    
    /* Controller-specific data */
    void* controller_data;      /* Controller-specific private data */
    
    /* List management */
    struct usb_controller* next; /* Next controller in list */
} usb_controller_t;

/* ================================
 * USB Device Structure
 * ================================ */

typedef struct usb_device {
    /* Device identification */
    uint8_t address;            /* USB device address (1-127) */
    uint8_t port;               /* Port number on hub/controller */
    usb_speed_t speed;          /* Device speed */
    
    /* Device descriptors */
    usb_device_descriptor_t device_desc;
    usb_config_descriptor_t* config_desc;
    
    /* Device state */
    bool configured;            /* Device configuration state */
    uint8_t configuration;      /* Current configuration number */
    
    /* Associated controller and IKOS device */
    usb_controller_t* controller; /* Parent USB controller */
    device_t* ikos_device;      /* Associated IKOS device structure */
    
    /* Hub information (if this is a hub) */
    bool is_hub;                /* Is this device a USB hub */
    uint8_t hub_ports;          /* Number of hub ports (if hub) */
    struct usb_device* hub_devices[8]; /* Connected devices (if hub) */
    
    /* List management */
    struct usb_device* next;    /* Next device in list */
} usb_device_t;

/* ================================
 * USB Controller API
 * ================================ */

/* Controller management */
int usb_controller_init(void);
int usb_controller_shutdown(void);
int usb_register_controller(device_t* device);
int usb_unregister_controller(usb_controller_t* controller);

/* Controller operations */
int usb_controller_start(usb_controller_t* controller);
int usb_controller_stop(usb_controller_t* controller);
int usb_controller_reset(usb_controller_t* controller);
int usb_controller_suspend(usb_controller_t* controller);
int usb_controller_resume(usb_controller_t* controller);

/* Device enumeration and management */
int usb_enumerate_devices(usb_controller_t* controller);
int usb_connect_device(usb_controller_t* controller, uint8_t port);
int usb_disconnect_device(usb_device_t* device);
int usb_reset_device(usb_device_t* device);
usb_controller_t* usb_get_controllers(void);
usb_device_t* usb_get_devices(void);

/* Device configuration */
int usb_get_device_descriptor(usb_device_t* device);
int usb_get_config_descriptor(usb_device_t* device, uint8_t config_index);
int usb_set_configuration(usb_device_t* device, uint8_t configuration);
int usb_set_address(usb_device_t* device, uint8_t address);

/* Transfer operations */
int usb_control_transfer(usb_device_t* device, uint8_t request_type, 
                        uint8_t request, uint16_t value, uint16_t index,
                        void* data, uint16_t length);
int usb_bulk_transfer(usb_device_t* device, uint8_t endpoint,
                     void* data, uint32_t length, uint32_t timeout);
int usb_interrupt_transfer(usb_device_t* device, uint8_t endpoint,
                          void* data, uint32_t length, uint32_t timeout);

/* Hub operations */
int usb_hub_get_status(usb_device_t* hub, uint8_t port);
int usb_hub_set_port_feature(usb_device_t* hub, uint8_t port, uint16_t feature);
int usb_hub_clear_port_feature(usb_device_t* hub, uint8_t port, uint16_t feature);

/* ================================
 * USB Device Class Drivers
 * ================================ */

/* HID (Human Interface Device) support */
int usb_hid_init(void);
int usb_hid_register_device(usb_device_t* device);
int usb_hid_get_report(usb_device_t* device, uint8_t* report, uint16_t length);

/* Mass Storage support */
int usb_storage_init(void);
int usb_storage_register_device(usb_device_t* device);
int usb_storage_read(usb_device_t* device, uint64_t lba, void* buffer, uint32_t count);
int usb_storage_write(usb_device_t* device, uint64_t lba, const void* buffer, uint32_t count);

/* ================================
 * USB Standard Requests
 * ================================ */

#define USB_REQ_GET_STATUS          0x00
#define USB_REQ_CLEAR_FEATURE       0x01
#define USB_REQ_SET_FEATURE         0x03
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_DESCRIPTOR      0x07
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_SET_CONFIGURATION   0x09
#define USB_REQ_GET_INTERFACE       0x0A
#define USB_REQ_SET_INTERFACE       0x0B
#define USB_REQ_SYNCH_FRAME         0x0C

/* Request types */
#define USB_TYPE_STANDARD           (0x00 << 5)
#define USB_TYPE_CLASS              (0x01 << 5)
#define USB_TYPE_VENDOR             (0x02 << 5)

#define USB_RECIP_DEVICE            0x00
#define USB_RECIP_INTERFACE         0x01
#define USB_RECIP_ENDPOINT          0x02
#define USB_RECIP_OTHER             0x03

#define USB_DIR_OUT                 0x00
#define USB_DIR_IN                  0x80

/* ================================
 * USB Device Classes
 * ================================ */

#define USB_CLASS_AUDIO             0x01
#define USB_CLASS_COMM              0x02
#define USB_CLASS_HID               0x03
#define USB_CLASS_PHYSICAL          0x05
#define USB_CLASS_IMAGE             0x06
#define USB_CLASS_PRINTER           0x07
#define USB_CLASS_MASS_STORAGE      0x08
#define USB_CLASS_HUB               0x09
#define USB_CLASS_CDC_DATA          0x0A
#define USB_CLASS_SMART_CARD        0x0B
#define USB_CLASS_SECURITY          0x0D
#define USB_CLASS_VIDEO             0x0E
#define USB_CLASS_WIRELESS          0xE0
#define USB_CLASS_VENDOR_SPECIFIC   0xFF

/* ================================
 * Error Codes
 * ================================ */

#define USB_SUCCESS                 0
#define USB_ERROR_INVALID_PARAM     -1
#define USB_ERROR_NO_MEMORY         -2
#define USB_ERROR_NOT_FOUND         -3
#define USB_ERROR_TIMEOUT           -4
#define USB_ERROR_IO                -5
#define USB_ERROR_PROTOCOL          -6
#define USB_ERROR_NO_DEVICE         -7
#define USB_ERROR_BUSY              -8
#define USB_ERROR_NOT_SUPPORTED     -9

/* ================================
 * Statistics and Debugging
 * ================================ */

typedef struct {
    uint32_t controllers_found;
    uint32_t devices_connected;
    uint32_t transfers_completed;
    uint32_t errors_detected;
    uint32_t hubs_detected;
    uint32_t storage_devices;
    uint32_t hid_devices;
} usb_stats_t;

void usb_get_stats(usb_stats_t* stats);
void usb_reset_stats(void);

/* ================================
 * UHCI/OHCI/EHCI/xHCI Specific
 * ================================ */

/* UHCI (USB 1.1) specific functions */
int uhci_init_controller(usb_controller_t* controller);
int uhci_start_controller(usb_controller_t* controller);

/* OHCI (USB 1.1) specific functions */
int ohci_init_controller(usb_controller_t* controller);
int ohci_start_controller(usb_controller_t* controller);

/* EHCI (USB 2.0) specific functions */
int ehci_init_controller(usb_controller_t* controller);
int ehci_start_controller(usb_controller_t* controller);

/* xHCI (USB 3.0+) specific functions */
int xhci_init_controller(usb_controller_t* controller);
int xhci_start_controller(usb_controller_t* controller);

#endif /* USB_CONTROLLER_H */
