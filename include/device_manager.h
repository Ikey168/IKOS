/* IKOS Device Driver Framework - Core Device Manager
 * Issue #15 - Comprehensive Device Driver Framework
 * 
 * This module provides centralized device management, driver registration,
 * and hardware abstraction for the IKOS operating system.
 */

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ================================
 * Device Types and Classes
 * ================================ */

typedef enum {
    DEVICE_CLASS_UNKNOWN    = 0x00,
    DEVICE_CLASS_STORAGE    = 0x01,    /* Storage controllers and devices */
    DEVICE_CLASS_NETWORK    = 0x02,    /* Network adapters */
    DEVICE_CLASS_DISPLAY    = 0x03,    /* Graphics and display devices */
    DEVICE_CLASS_INPUT      = 0x04,    /* Keyboard, mouse, etc. */
    DEVICE_CLASS_AUDIO      = 0x05,    /* Audio controllers */
    DEVICE_CLASS_BRIDGE     = 0x06,    /* PCI bridges and chipset */
    DEVICE_CLASS_COMM      = 0x07,    /* Serial, parallel ports */
    DEVICE_CLASS_SYSTEM     = 0x08,    /* System devices (timers, PICs) */
    DEVICE_CLASS_PROCESSOR  = 0x0B,    /* CPU, co-processors */
    DEVICE_CLASS_SERIAL_BUS = 0x0C,    /* USB, FireWire, etc. */
    DEVICE_CLASS_MAX        = 0xFF
} device_class_t;

typedef enum {
    DEVICE_TYPE_UNKNOWN     = 0x00,
    
    /* Storage device types */
    DEVICE_TYPE_IDE         = 0x0101,
    DEVICE_TYPE_SATA        = 0x0102,
    DEVICE_TYPE_SCSI        = 0x0103,
    DEVICE_TYPE_USB_STORAGE = 0x0104,
    DEVICE_TYPE_NVME        = 0x0105,
    
    /* Input device types */
    DEVICE_TYPE_KEYBOARD    = 0x0401,
    DEVICE_TYPE_MOUSE       = 0x0402,
    DEVICE_TYPE_TOUCHPAD    = 0x0403,
    
    /* Network device types */
    DEVICE_TYPE_ETHERNET    = 0x0201,
    DEVICE_TYPE_WIFI        = 0x0202,
    
    /* Display device types */
    DEVICE_TYPE_VGA         = 0x0301,
    DEVICE_TYPE_FRAMEBUFFER = 0x0302
} device_type_t;

/* ================================
 * Device State and Status
 * ================================ */

typedef enum {
    DEVICE_STATE_UNKNOWN    = 0,
    DEVICE_STATE_DETECTED   = 1,    /* Device detected but not initialized */
    DEVICE_STATE_INITIALIZING = 2,  /* Driver loading/initializing */
    DEVICE_STATE_READY      = 3,    /* Device ready for use */
    DEVICE_STATE_ACTIVE     = 4,    /* Device in active use */
    DEVICE_STATE_SUSPENDED  = 5,    /* Device suspended/sleeping */
    DEVICE_STATE_ERROR      = 6,    /* Device in error state */
    DEVICE_STATE_REMOVED    = 7     /* Device removed/disconnected */
} device_state_t;

typedef enum {
    DEVICE_FLAG_REMOVABLE   = 0x01,  /* Device can be hot-removed */
    DEVICE_FLAG_HOT_PLUG    = 0x02,  /* Supports hot-plug */
    DEVICE_FLAG_POWER_MGMT  = 0x04,  /* Supports power management */
    DEVICE_FLAG_DMA_CAPABLE = 0x08,  /* Device supports DMA */
    DEVICE_FLAG_SHARED      = 0x10,  /* Device can be shared */
    DEVICE_FLAG_EXCLUSIVE   = 0x20   /* Device requires exclusive access */
} device_flags_t;

/* ================================
 * Device Structure
 * ================================ */

/* Forward declarations */
struct device;
struct device_driver;
struct device_operations;

/* Device resource descriptor */
typedef struct {
    uint64_t base_address;      /* Base address (I/O or memory) */
    uint64_t size;              /* Resource size */
    uint32_t type;              /* Resource type (I/O, memory, IRQ) */
    uint32_t flags;             /* Resource flags */
} device_resource_t;

/* Maximum resources per device */
#define MAX_DEVICE_RESOURCES    8
#define MAX_DEVICE_NAME_LEN     64
#define MAX_DRIVER_NAME_LEN     32

/* Core device structure */
typedef struct device {
    /* Device identification */
    uint32_t device_id;         /* Unique device ID */
    char name[MAX_DEVICE_NAME_LEN]; /* Device name */
    device_class_t class;       /* Device class */
    device_type_t type;         /* Device type */
    
    /* Hardware information */
    uint16_t vendor_id;         /* Vendor ID (PCI, USB, etc.) */
    uint16_t product_id;        /* Product/Device ID */
    uint8_t revision;           /* Device revision */
    uint8_t bus_type;           /* Bus type (PCI, USB, ISA, etc.) */
    
    /* Device location */
    uint8_t bus_number;         /* Bus number */
    uint8_t device_number;      /* Device number on bus */
    uint8_t function_number;    /* Function number (for multi-function) */
    
    /* Resources */
    device_resource_t resources[MAX_DEVICE_RESOURCES];
    uint8_t resource_count;
    
    /* State and configuration */
    device_state_t state;       /* Current device state */
    uint32_t flags;             /* Device flags */
    uint32_t config_space_size; /* Configuration space size */
    void* config_space;         /* Device configuration data */
    
    /* Driver association */
    struct device_driver* driver; /* Associated driver */
    void* driver_data;          /* Driver-specific data */
    
    /* Device hierarchy */
    struct device* parent;      /* Parent device (bus controller) */
    struct device* children;    /* Child devices */
    struct device* sibling;     /* Next sibling device */
    
    /* List management */
    struct device* next;        /* Next device in global list */
    struct device* prev;        /* Previous device in global list */
    
    /* Statistics and monitoring */
    uint64_t power_on_time;     /* Time since power on */
    uint64_t error_count;       /* Error counter */
    uint64_t last_access_time;  /* Last access timestamp */
} device_t;

/* ================================
 * Device Driver Structure
 * ================================ */

/* Driver operations */
typedef struct device_operations {
    /* Driver lifecycle */
    int (*probe)(device_t* device);        /* Probe if driver supports device */
    int (*attach)(device_t* device);       /* Attach driver to device */
    int (*detach)(device_t* device);       /* Detach driver from device */
    int (*remove)(device_t* device);       /* Remove device */
    
    /* Power management */
    int (*suspend)(device_t* device);      /* Suspend device */
    int (*resume)(device_t* device);       /* Resume device */
    int (*power_off)(device_t* device);    /* Power off device */
    
    /* I/O operations */
    int (*read)(device_t* device, uint64_t offset, void* buffer, size_t size);
    int (*write)(device_t* device, uint64_t offset, const void* buffer, size_t size);
    int (*ioctl)(device_t* device, uint32_t cmd, void* arg);
    
    /* Interrupt handling */
    int (*irq_handler)(device_t* device, uint32_t irq);
} device_operations_t;

/* Device driver structure */
typedef struct device_driver {
    /* Driver identification */
    char name[MAX_DRIVER_NAME_LEN]; /* Driver name */
    uint32_t driver_id;         /* Unique driver ID */
    uint32_t version;           /* Driver version */
    
    /* Supported devices */
    device_class_t supported_class; /* Supported device class */
    uint16_t* supported_vendors; /* Supported vendor IDs (NULL-terminated) */
    uint16_t* supported_devices; /* Supported device IDs (NULL-terminated) */
    
    /* Driver operations */
    device_operations_t* ops;   /* Driver operations */
    
    /* Driver state */
    bool loaded;                /* Driver loaded flag */
    uint32_t device_count;      /* Number of devices using this driver */
    
    /* List management */
    struct device_driver* next; /* Next driver in list */
} device_driver_t;

/* ================================
 * Device Manager API
 * ================================ */

/* Device Manager initialization */
int device_manager_init(void);
void device_manager_shutdown(void);

/* Device registration and management */
device_t* device_create(device_class_t class, device_type_t type, const char* name);
int device_register(device_t* device);
int device_unregister(device_t* device);
void device_destroy(device_t* device);

/* Device discovery */
device_t* device_find_by_id(uint32_t device_id);
device_t* device_find_by_name(const char* name);
device_t* device_find_by_type(device_type_t type);
device_t* device_find_by_class(device_class_t class);

/* Device enumeration */
int device_enumerate_all(device_t** devices, uint32_t max_devices);
int device_enumerate_by_class(device_class_t class, device_t** devices, uint32_t max_devices);
uint32_t device_get_count(void);
uint32_t device_get_count_by_class(device_class_t class);

/* Driver management */
int driver_register(device_driver_t* driver);
int driver_unregister(device_driver_t* driver);
device_driver_t* driver_find_by_name(const char* name);
device_driver_t* driver_find_for_device(device_t* device);

/* Device-driver association */
int device_attach_driver(device_t* device, device_driver_t* driver);
int device_detach_driver(device_t* device);

/* Device state management */
int device_set_state(device_t* device, device_state_t state);
device_state_t device_get_state(device_t* device);
int device_power_on(device_t* device);
int device_power_off(device_t* device);
int device_reset(device_t* device);

/* Resource management */
int device_add_resource(device_t* device, uint64_t base, uint64_t size, uint32_t type);
device_resource_t* device_get_resource(device_t* device, uint32_t type, uint32_t index);
int device_request_resource(device_t* device, uint32_t type);
int device_release_resource(device_t* device, uint32_t type);

/* Hardware detection and enumeration */
int device_scan_pci_bus(void);
int device_scan_isa_devices(void);
int device_scan_usb_devices(void);
int device_rescan_all_buses(void);

/* Device hierarchy management */
int device_add_child(device_t* parent, device_t* child);
int device_remove_child(device_t* parent, device_t* child);
device_t* device_get_parent(device_t* device);
device_t* device_get_children(device_t* device);

/* Statistics and monitoring */
typedef struct {
    uint32_t total_devices;     /* Total devices registered */
    uint32_t active_devices;    /* Devices in active state */
    uint32_t failed_devices;    /* Devices in error state */
    uint32_t total_drivers;     /* Total drivers registered */
    uint32_t loaded_drivers;    /* Currently loaded drivers */
    uint64_t total_memory_used; /* Memory used by device manager */
} device_manager_stats_t;

void device_manager_get_stats(device_manager_stats_t* stats);
void device_print_info(device_t* device);
void device_print_all_devices(void);

/* ================================
 * Error Codes
 * ================================ */

#define DEVICE_SUCCESS              0
#define DEVICE_ERROR_INVALID_PARAM  -1
#define DEVICE_ERROR_NO_MEMORY      -2
#define DEVICE_ERROR_NOT_FOUND      -3
#define DEVICE_ERROR_ALREADY_EXISTS -4
#define DEVICE_ERROR_NOT_SUPPORTED  -5
#define DEVICE_ERROR_BUSY           -6
#define DEVICE_ERROR_TIMEOUT        -7
#define DEVICE_ERROR_IO_ERROR       -8
#define DEVICE_ERROR_NOT_READY      -9
#define DEVICE_ERROR_PERMISSION     -10

/* ================================
 * Resource Types
 * ================================ */

#define RESOURCE_TYPE_IO_PORT       0x01
#define RESOURCE_TYPE_MEMORY        0x02
#define RESOURCE_TYPE_IRQ           0x03
#define RESOURCE_TYPE_DMA           0x04

#endif /* DEVICE_MANAGER_H */
