/* IKOS USB Driver Framework Header
 *
 * Comprehensive USB driver framework for IKOS operating system.
 * This implementation provides:
 * - USB host controller drivers (UHCI, OHCI, EHCI, XHCI)
 * - USB device enumeration and management
 * - USB device class drivers (HID, Mass Storage, etc.)
 * - Hot-plug support and dynamic device discovery
 * - Power management for USB devices
 */

#ifndef USB_H
#define USB_H

#include <stdint.h>
#include <stdbool.h>

/* USB Constants */
#define USB_MAX_DEVICES         127     /* Maximum USB devices */
#define USB_MAX_ADDRESS         127     /* Maximum USB address */
#define USB_MAX_BUSES           8       /* Maximum USB buses */
#define USB_MAX_DRIVERS         32      /* Maximum USB drivers */
#define USB_MAX_TRANSFERS       64      /* Maximum active transfers */
#define USB_MAX_ENDPOINTS       32      /* Maximum endpoints per device */
#define USB_MAX_INTERFACES      32      /* Maximum interfaces per device */
#define USB_MAX_CONFIGURATIONS  8       /* Maximum configurations per device */
#define USB_MAX_STRING_LEN      255     /* Maximum string descriptor length */

/* USB Speeds */
#define USB_SPEED_LOW           0       /* 1.5 Mbps */
#define USB_SPEED_FULL          1       /* 12 Mbps */
#define USB_SPEED_HIGH          2       /* 480 Mbps */
#define USB_SPEED_SUPER         3       /* 5 Gbps */
#define USB_SPEED_SUPER_PLUS    4       /* 10 Gbps */
#define USB_SPEED_UNKNOWN       0xFF    /* Unknown speed */

/* USB Bus States */
#define USB_BUS_STATE_ACTIVE    1       /* Bus is active */

/* USB Descriptor Types */
#define USB_DESC_DEVICE         1       /* Device descriptor */
#define USB_DESC_CONFIG         2       /* Configuration descriptor */
#define USB_DESC_STRING         3       /* String descriptor */
#define USB_DESC_INTERFACE      4       /* Interface descriptor */
#define USB_DESC_ENDPOINT       5       /* Endpoint descriptor */
#define USB_DESC_DEVICE_QUALIFIER 6     /* Device qualifier descriptor */
#define USB_DESC_OTHER_SPEED    7       /* Other speed configuration */
#define USB_DESC_INTERFACE_POWER 8      /* Interface power descriptor */
#define USB_DESC_HID            0x21    /* HID descriptor */
#define USB_DESC_REPORT         0x22    /* HID report descriptor */

/* USB Request Types */
#define USB_REQ_TYPE_STANDARD   0x00    /* Standard request */
#define USB_REQ_TYPE_CLASS      0x20    /* Class-specific request */
#define USB_REQ_TYPE_VENDOR     0x40    /* Vendor-specific request */
#define USB_REQ_TYPE_RESERVED   0x60    /* Reserved */

/* USB Request Recipients */
#define USB_REQ_DEVICE          0x00    /* Device recipient */
#define USB_REQ_INTERFACE       0x01    /* Interface recipient */
#define USB_REQ_ENDPOINT        0x02    /* Endpoint recipient */
#define USB_REQ_OTHER           0x03    /* Other recipient */

/* USB Standard Requests */
#define USB_REQ_GET_STATUS      0x00    /* Get status */
#define USB_REQ_CLEAR_FEATURE   0x01    /* Clear feature */
#define USB_REQ_SET_FEATURE     0x03    /* Set feature */
#define USB_REQ_SET_ADDRESS     0x05    /* Set address */
#define USB_REQ_GET_DESCRIPTOR  0x06    /* Get descriptor */
#define USB_REQ_SET_DESCRIPTOR  0x07    /* Set descriptor */
#define USB_REQ_GET_CONFIGURATION 0x08  /* Get configuration */
#define USB_REQ_SET_CONFIGURATION 0x09  /* Set configuration */
#define USB_REQ_GET_INTERFACE   0x0A    /* Get interface */
#define USB_REQ_SET_INTERFACE   0x0B    /* Set interface */
#define USB_REQ_SYNCH_FRAME     0x0C    /* Synch frame */

/* USB Device Classes */
#define USB_CLASS_PER_INTERFACE 0x00    /* Class specified at interface level */
#define USB_CLASS_AUDIO         0x01    /* Audio device */
#define USB_CLASS_CDC           0x02    /* Communications device */
#define USB_CLASS_HID           0x03    /* Human interface device */
#define USB_CLASS_PHYSICAL      0x05    /* Physical device */
#define USB_CLASS_IMAGE         0x06    /* Image device */
#define USB_CLASS_PRINTER       0x07    /* Printer device */
#define USB_CLASS_MASS_STORAGE  0x08    /* Mass storage device */
#define USB_CLASS_HUB           0x09    /* Hub device */
#define USB_CLASS_CDC_DATA      0x0A    /* CDC data device */
#define USB_CLASS_SMART_CARD    0x0B    /* Smart card device */
#define USB_CLASS_SECURITY      0x0D    /* Content security */
#define USB_CLASS_VIDEO         0x0E    /* Video device */
#define USB_CLASS_WIRELESS      0xE0    /* Wireless controller */
#define USB_CLASS_VENDOR        0xFF    /* Vendor-specific */

/* USB Endpoint Types */
#define USB_ENDPOINT_CONTROL    0x00    /* Control endpoint */
#define USB_ENDPOINT_ISOCHRONOUS 0x01   /* Isochronous endpoint */
#define USB_ENDPOINT_BULK       0x02    /* Bulk endpoint */
#define USB_ENDPOINT_INTERRUPT  0x03    /* Interrupt endpoint */

/* USB Transfer Directions */
#define USB_DIR_OUT             0x00    /* Host to device */
#define USB_DIR_IN              0x80    /* Device to host */

/* USB Result Codes */
#define USB_SUCCESS             0       /* Operation successful */
#define USB_ERROR_INVALID_PARAM -1      /* Invalid parameter */
#define USB_ERROR_NO_MEMORY     -2      /* Out of memory */
#define USB_ERROR_NO_RESOURCES  -3      /* No resources available */
#define USB_ERROR_NOT_SUPPORTED -4      /* Operation not supported */
#define USB_ERROR_BUSY          -5      /* Device busy */
#define USB_ERROR_TIMEOUT       -6      /* Operation timeout */
#define USB_ERROR_NO_CONFIG     -7      /* No configuration */
#define USB_ERROR_NO_DEVICE     -8      /* Device not found */
#define USB_ERROR_PROTOCOL      -9      /* Protocol error */
#define USB_ERROR_BUFFER_TOO_SMALL -10   /* Buffer too small */
#define USB_ERROR_TRANSFER_FAILED -11   /* Transfer failed */
#define USB_ERROR_DEVICE_NOT_FOUND -12  /* Device not found */
#define USB_ERROR_ACCESS_DENIED    -13  /* Access denied */
#define USB_ERROR_NO_DRIVER       -14  /* No driver available */

/* USB Feature Selectors */
#define USB_FEATURE_ENDPOINT_HALT 0x00

/* USB Request Type components */
#define USB_TYPE_STANDARD (0x00 << 5)
#define USB_TYPE_CLASS    (0x01 << 5)
#define USB_TYPE_VENDOR   (0x02 << 5)

#define USB_RECIP_DEVICE    0x00
#define USB_RECIP_INTERFACE 0x01
#define USB_RECIP_ENDPOINT  0x02
#define USB_RECIP_OTHER     0x03

/* USB Transfer Types */
#define USB_TRANSFER_TYPE_CONTROL       0
#define USB_TRANSFER_TYPE_ISOCHRONOUS   1
#define USB_TRANSFER_TYPE_BULK          2
#define USB_TRANSFER_TYPE_INTERRUPT     3

/* USB Transfer Status */
#define USB_STATUS_SUCCESS      0       /* Transfer completed successfully */
#define USB_STATUS_PENDING      1       /* Transfer is pending */
#define USB_STATUS_ERROR        2       /* Transfer failed */
#define USB_STATUS_TIMEOUT      3       /* Transfer timed out */
#define USB_STATUS_STALL        4       /* Endpoint stalled */
#define USB_STATUS_NAK          5       /* NAK received */
#define USB_STATUS_BABBLE       6       /* Babble detected */
#define USB_STATUS_CRC          7       /* CRC error */

/* USB Device States */
#define USB_DEVICE_STATE_ATTACHED       0
#define USB_DEVICE_STATE_POWERED        1
#define USB_DEVICE_STATE_DEFAULT        2
#define USB_DEVICE_STATE_ADDRESS        3
#define USB_DEVICE_STATE_CONFIGURED     4
#define USB_DEVICE_STATE_SUSPENDED      5
#define USB_DEVICE_STATE_DISCONNECTED   6

/* USB Transfer States */
#define USB_TRANSFER_STATE_IDLE         0
#define USB_TRANSFER_STATE_ACTIVE       1
#define USB_TRANSFER_STATE_COMPLETE     2
#define USB_TRANSFER_STATE_ERROR        3

/* USB Transfer Status Codes */
#define USB_TRANSFER_STATUS_PENDING     0
#define USB_TRANSFER_STATUS_COMPLETE    1
#define USB_TRANSFER_STATUS_SUCCESS     1
#define USB_TRANSFER_STATUS_ERROR       2
#define USB_TRANSFER_STATUS_CANCELLED   3

/* USB Device Classes */
#define USB_CLASS_CONTENT_SECURITY      0x0D
#define USB_CLASS_PERSONAL_HEALTHCARE   0x0F
#define USB_CLASS_AUDIO_VIDEO          0x10
#define USB_CLASS_BILLBOARD            0x11
#define USB_CLASS_DIAGNOSTIC           0xDC
#define USB_CLASS_MISCELLANEOUS        0xEF
#define USB_CLASS_APPLICATION          0xFE
#define USB_CLASS_VENDOR_SPECIFIC      0xFF

/* USB Standard Descriptors */
typedef struct usb_device_descriptor {
    uint8_t  bLength;               /* Size of this descriptor */
    uint8_t  bDescriptorType;       /* DEVICE descriptor type */
    uint16_t bcdUSB;                /* USB specification version */
    uint8_t  bDeviceClass;          /* Device class code */
    uint8_t  bDeviceSubClass;       /* Device subclass code */
    uint8_t  bDeviceProtocol;       /* Device protocol code */
    uint8_t  bMaxPacketSize0;       /* Maximum packet size for endpoint 0 */
    uint16_t idVendor;              /* Vendor ID */
    uint16_t idProduct;             /* Product ID */
    uint16_t bcdDevice;             /* Device release number */
    uint8_t  iManufacturer;         /* Manufacturer string index */
    uint8_t  iProduct;              /* Product string index */
    uint8_t  iSerialNumber;         /* Serial number string index */
    uint8_t  bNumConfigurations;    /* Number of possible configurations */
} __attribute__((packed)) usb_device_descriptor_t;

typedef struct usb_configuration_descriptor {
    uint8_t  bLength;               /* Size of this descriptor */
    uint8_t  bDescriptorType;       /* CONFIGURATION descriptor type */
    uint16_t wTotalLength;          /* Total length of configuration */
    uint8_t  bNumInterfaces;        /* Number of interfaces */
    uint8_t  bConfigurationValue;   /* Configuration value */
    uint8_t  iConfiguration;        /* Configuration string index */
    uint8_t  bmAttributes;          /* Configuration attributes */
    uint8_t  bMaxPower;             /* Maximum power consumption */
} __attribute__((packed)) usb_configuration_descriptor_t;

typedef struct usb_interface_descriptor {
    uint8_t  bLength;               /* Size of this descriptor */
    uint8_t  bDescriptorType;       /* INTERFACE descriptor type */
    uint8_t  bInterfaceNumber;      /* Interface number */
    uint8_t  bAlternateSetting;     /* Alternate setting */
    uint8_t  bNumEndpoints;         /* Number of endpoints */
    uint8_t  bInterfaceClass;       /* Interface class code */
    uint8_t  bInterfaceSubClass;    /* Interface subclass code */
    uint8_t  bInterfaceProtocol;    /* Interface protocol code */
    uint8_t  iInterface;            /* Interface string index */
} __attribute__((packed)) usb_interface_descriptor_t;

typedef struct usb_endpoint_descriptor {
    uint8_t  bLength;               /* Size of this descriptor */
    uint8_t  bDescriptorType;       /* ENDPOINT descriptor type */
    uint8_t  bEndpointAddress;      /* Endpoint address */
    uint8_t  bmAttributes;          /* Endpoint attributes */
    uint16_t wMaxPacketSize;        /* Maximum packet size */
    uint8_t  bInterval;             /* Polling interval */
} __attribute__((packed)) usb_endpoint_descriptor_t;

typedef struct usb_string_descriptor {
    uint8_t  bLength;               /* Size of this descriptor */
    uint8_t  bDescriptorType;       /* STRING descriptor type */
    uint16_t wString[];             /* Unicode string */
} __attribute__((packed)) usb_string_descriptor_t;

/* USB Setup Packet */
typedef struct usb_setup_packet {
    uint8_t  bmRequestType;         /* Request type */
    uint8_t  bRequest;              /* Request */
    uint16_t wValue;                /* Value */
    uint16_t wIndex;                /* Index */
    uint16_t wLength;               /* Length */
} __attribute__((packed)) usb_setup_packet_t;

/* USB Transfer Request */
typedef struct usb_transfer {
    uint32_t transfer_id;           /* Transfer ID */
    uint8_t  device_address;        /* Device address */
    uint8_t  endpoint;              /* Endpoint number */
    uint8_t  direction;             /* Transfer direction */
    uint8_t  type;                  /* Transfer type */
    uint8_t  state;                 /* Transfer state */
    uint16_t max_packet_size;       /* Maximum packet size */
    void*    buffer;                /* Data buffer */
    uint32_t length;                /* Transfer length */
    uint32_t actual_length;         /* Actual transferred length */
    uint32_t status;                /* Transfer status */
    uint32_t timeout;               /* Transfer timeout (ms) */
    uint32_t interval;              /* Polling interval for interrupt transfers */
    void*    context;               /* User context */
    struct usb_device* device;      /* Device reference */
    void     (*callback)(struct usb_transfer* transfer); /* Completion callback */
} usb_transfer_t;

/* USB Device ID Structure for Driver Matching */
typedef struct usb_device_id {
    uint16_t vendor_id;             /* Vendor ID */
    uint16_t product_id;            /* Product ID */
    uint8_t  device_class;          /* Device class */
    uint8_t  device_subclass;       /* Device subclass */
    uint8_t  device_protocol;       /* Device protocol */
} usb_device_id_t;

/* USB Bus Structure */
typedef struct usb_bus {
    uint8_t  bus_id;                /* Bus ID */
    uint8_t  bus_num;
    uint8_t  state;
    uint8_t  speed;                 /* Bus speed */
    uint8_t  max_speed;
    uint8_t  num_ports;
    void* private_data;
    struct usb_hci* hci;
    struct usb_bus* next;
    const char* name;
    struct usb_device* root_hub;
} usb_bus_t;

/* USB Device Information */
typedef struct usb_device {
    uint8_t  device_id;             /* Device ID */
    uint8_t  address;               /* Device address */
    uint8_t  speed;                 /* Device speed */
    uint8_t  port;                  /* Port number */
    uint8_t  hub_address;           /* Hub address (0 for root hub) */
    uint8_t  configuration;         /* Current configuration */
    uint8_t  current_config;        /* Current configuration index */
    uint8_t  state;                 /* Device state */
    uint16_t vendor_id;             /* Vendor ID */
    uint16_t product_id;            /* Product ID */
    uint16_t device_version;        /* Device version */
    uint8_t  device_class;          /* Device class */
    uint8_t  device_subclass;       /* Device subclass */
    uint8_t  device_protocol;       /* Device protocol */
    uint8_t  max_packet_size;       /* Maximum packet size for endpoint 0 */
    
    /* Descriptors */
    usb_device_descriptor_t device_desc;
    usb_configuration_descriptor_t* config_desc;
    usb_configuration_descriptor_t* configurations[USB_MAX_CONFIGURATIONS];
    uint8_t  num_configurations;    /* Number of configurations */
    usb_interface_descriptor_t* interfaces[USB_MAX_INTERFACES];
    usb_endpoint_descriptor_t* endpoints[USB_MAX_ENDPOINTS];
    
    /* Bus reference */
    struct usb_bus* bus;            /* USB bus this device is on */
    
    /* String descriptors */
    char manufacturer[USB_MAX_STRING_LEN];
    char product[USB_MAX_STRING_LEN];
    char serial_number[USB_MAX_STRING_LEN];
    
    /* Device state */
    bool connected;                 /* Device is connected */
    bool configured;                /* Device is configured */
    bool suspended;                 /* Device is suspended */
    
    /* Driver information */
    void* driver_data;              /* Driver-specific data */
    struct usb_driver* driver;      /* Associated driver */
} usb_device_t;

/* USB Driver Structure */
typedef struct usb_driver {
    const char* name;               /* Driver name */
    
    /* Device matching */
    const usb_device_id_t* id_table; /* Device ID table for matching */
    uint16_t* vendor_ids;           /* Supported vendor IDs */
    uint16_t* product_ids;          /* Supported product IDs */
    uint8_t*  device_classes;       /* Supported device classes */
    
    /* Driver callbacks */
    int (*probe)(usb_device_t* device);
    void (*disconnect)(usb_device_t* device);
    int (*suspend)(usb_device_t* device);
    int (*resume)(usb_device_t* device);
    
    /* Transfer handling */
    void (*transfer_complete)(usb_transfer_t* transfer);
    
    /* Driver data */
    void* private_data;
    
    /* List linkage */
    struct usb_driver* next;
} usb_driver_t;

/* USB Host Controller Interface */
typedef struct usb_hci {
    const char* name;               /* Controller name */
    uint32_t type;                  /* Controller type */
    
    /* Controller operations */
    int (*init)(struct usb_hci* hci);
    void (*shutdown)(struct usb_hci* hci);
    int (*reset)(struct usb_hci* hci);
    
    /* Port operations */
    int (*port_reset)(struct usb_hci* hci, uint8_t port);
    int (*port_enable)(struct usb_hci* hci, uint8_t port);
    int (*port_disable)(struct usb_hci* hci, uint8_t port);
    uint32_t (*port_status)(struct usb_hci* hci, uint8_t port);
    
    /* Transfer operations */
    int (*submit_transfer)(struct usb_hci* hci, usb_transfer_t* transfer);
    int (*cancel_transfer)(struct usb_hci* hci, usb_transfer_t* transfer);
    
    /* Device operations */
    int (*set_address)(struct usb_hci* hci, uint8_t old_addr, uint8_t new_addr);
    int (*configure_device)(struct usb_hci* hci, uint8_t address, uint8_t config);
    
    /* Controller data */
    void* private_data;
    uint8_t num_ports;
    usb_device_t* devices[USB_MAX_DEVICES];
    
    /* IRQ handling */
    uint32_t irq;
    void (*irq_handler)(struct usb_hci* hci);
    void (*scan_ports)(struct usb_bus* bus);
    
    /* List linkage */
    struct usb_hci* next;
} usb_hci_t;

/* USB HCI Types */
#define USB_HCI_UHCI            1       /* UHCI (USB 1.1) */
#define USB_HCI_OHCI            2       /* OHCI (USB 1.1) */
#define USB_HCI_EHCI            3       /* EHCI (USB 2.0) */
#define USB_HCI_XHCI            4       /* XHCI (USB 3.0+) */

/* USB Core Functions */
int usb_init(void);
void usb_shutdown(void);

/* Host Controller Interface */
int usb_register_hci(usb_hci_t* hci);
void usb_unregister_hci(usb_hci_t* hci);
usb_hci_t* usb_find_hci(uint32_t type);

/* Device Management */
int usb_enumerate_device(usb_device_t* device);
void usb_disconnect_device(usb_device_t* device);
usb_device_t* usb_find_device(uint8_t address);
usb_device_t* usb_get_device(uint16_t vendor_id, uint16_t product_id);
void usb_enumerate_root_hub(usb_bus_t* bus);

/* Driver Management */
int usb_register_driver(usb_driver_t* driver);
void usb_unregister_driver(usb_driver_t* driver);
usb_driver_t* usb_find_driver(usb_device_t* device);
bool usb_driver_matches(usb_driver_t* driver, usb_device_t* device);

/* Control Transfer Helpers */
int usb_get_device_descriptor(usb_device_t* device, usb_device_descriptor_t* desc);
int usb_get_configuration_descriptor(usb_device_t* device, uint8_t config_index, void* buffer, uint16_t length);
int usb_set_address(usb_device_t* device, uint8_t address);
uint8_t usb_allocate_address(usb_bus_t* bus);

/* Transfer Management */
int usb_submit_transfer(usb_transfer_t* transfer);
int usb_cancel_transfer(usb_transfer_t* transfer);
usb_transfer_t* usb_alloc_transfer(usb_device_t* device, uint8_t endpoint, uint8_t type, uint16_t max_packet_size);
void usb_free_transfer(usb_transfer_t* transfer);

/* Control Transfers */
int usb_control_transfer(usb_device_t* device, uint8_t bmRequestType, uint8_t bRequest,
                         uint16_t wValue, uint16_t wIndex, uint16_t wLength, void* data);
int usb_get_descriptor(usb_device_t* device, uint8_t type, uint8_t index,
                       uint16_t lang_id, void* buf, uint16_t len);
int usb_set_configuration(usb_device_t* device, uint8_t config_index);
int usb_set_interface(usb_device_t* device, uint8_t interface_index, uint8_t alt_setting);

/* Bulk Transfers */
int usb_bulk_transfer(usb_device_t* device, uint8_t endpoint,
                     void* buffer, uint32_t length, uint32_t timeout);
/* Interrupt Transfers */
int usb_interrupt_transfer(usb_device_t* device, uint8_t endpoint,
                          void* buffer, uint32_t length, uint32_t timeout);

/* String Operations */
int usb_get_string_descriptor(usb_device_t* device, uint8_t index, uint16_t lang_id, void* buffer, uint16_t length);

/* Power Management */
int usb_suspend_device(usb_device_t* device);
int usb_resume_device(usb_device_t* device);

/* Utility Functions */
const char* usb_speed_string(uint8_t speed);
const char* usb_class_string(uint8_t class);
const char* usb_status_string(uint32_t status);

/* Debug and Monitoring */
void usb_dump_device(usb_device_t* device);
void usb_dump_descriptor(void* desc, uint32_t length);
uint32_t usb_get_device_count(void);
void usb_list_devices(void);
void usb_connect_device(usb_device_t* device);
void usb_transfer_complete(usb_transfer_t* transfer, int status, uint16_t actual_length);

/* Function prototypes for USB core functions */
int usb_register_bus(usb_bus_t* bus);
usb_device_t* usb_alloc_device(usb_bus_t* bus, uint8_t address);
void usb_free_device(usb_device_t* device);
void usb_hub_port_status_change(usb_device_t* hub, int port);
void usb_device_disconnected(usb_device_t* device);
void usb_bus_scan_ports(usb_bus_t* bus);

#endif /* USB_H */
