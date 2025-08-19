/* IKOS USB HID (Human Interface Device) Driver Header
 * 
 * USB HID driver implementation for IKOS operating system.
 * This implementation provides:
 * - USB HID device support (keyboards, mice, gamepads, etc.)
 * - HID report parsing and processing
 * - Input event generation and dispatch
 * - Boot protocol support for basic keyboard/mouse functionality
 * - Custom HID device support
 */

#ifndef USB_HID_H
#define USB_HID_H

#include "usb.h"
#include <stdint.h>
#include <stdbool.h>

/* HID Constants */
#define HID_MAX_REPORT_SIZE     64      /* Maximum HID report size */
#define HID_MAX_USAGES          32      /* Maximum usages per report */
#define HID_MAX_COLLECTIONS     16      /* Maximum collections per device */
#define HID_POLL_INTERVAL_MS    10      /* Default polling interval */

/* HID Descriptor Types */
#define HID_DESC_HID            0x21    /* HID descriptor */
#define HID_DESC_REPORT         0x22    /* Report descriptor */
#define HID_DESC_PHYSICAL       0x23    /* Physical descriptor */

/* HID Class Requests */
#define HID_REQ_GET_REPORT      0x01    /* Get report */
#define HID_REQ_GET_IDLE        0x02    /* Get idle */
#define HID_REQ_GET_PROTOCOL    0x03    /* Get protocol */
#define HID_REQ_SET_REPORT      0x09    /* Set report */
#define HID_REQ_SET_IDLE        0x0A    /* Set idle */
#define HID_REQ_SET_PROTOCOL    0x0B    /* Set protocol */

/* HID Report Types */
#define HID_REPORT_INPUT        0x01    /* Input report */
#define HID_REPORT_OUTPUT       0x02    /* Output report */
#define HID_REPORT_FEATURE      0x03    /* Feature report */

/* HID Protocols */
#define HID_PROTOCOL_BOOT       0       /* Boot protocol */
#define HID_PROTOCOL_REPORT     1       /* Report protocol */

/* HID Interface Subclasses */
#define HID_SUBCLASS_NONE       0       /* No subclass */
#define HID_SUBCLASS_BOOT       1       /* Boot interface subclass */

/* HID Interface Protocols */
#define HID_PROTOCOL_NONE       0       /* No protocol */
#define HID_PROTOCOL_KEYBOARD   1       /* Keyboard protocol */
#define HID_PROTOCOL_MOUSE      2       /* Mouse protocol */

/* HID Usage Pages */
#define HID_USAGE_PAGE_GENERIC_DESKTOP  0x01    /* Generic desktop controls */
#define HID_USAGE_PAGE_SIMULATION       0x02    /* Simulation controls */
#define HID_USAGE_PAGE_VR               0x03    /* VR controls */
#define HID_USAGE_PAGE_SPORT            0x04    /* Sport controls */
#define HID_USAGE_PAGE_GAME             0x05    /* Game controls */
#define HID_USAGE_PAGE_GENERIC_DEVICE   0x06    /* Generic device controls */
#define HID_USAGE_PAGE_KEYBOARD         0x07    /* Keyboard/keypad */
#define HID_USAGE_PAGE_LED              0x08    /* LED */
#define HID_USAGE_PAGE_BUTTON           0x09    /* Button */
#define HID_USAGE_PAGE_ORDINAL          0x0A    /* Ordinal */
#define HID_USAGE_PAGE_TELEPHONY        0x0B    /* Telephony */
#define HID_USAGE_PAGE_CONSUMER         0x0C    /* Consumer */
#define HID_USAGE_PAGE_DIGITIZER        0x0D    /* Digitizer */
#define HID_USAGE_PAGE_UNICODE          0x10    /* Unicode */
#define HID_USAGE_PAGE_ALPHANUMERIC     0x14    /* Alphanumeric display */

/* HID Generic Desktop Usages */
#define HID_USAGE_POINTER               0x01    /* Pointer */
#define HID_USAGE_MOUSE                 0x02    /* Mouse */
#define HID_USAGE_JOYSTICK              0x04    /* Joystick */
#define HID_USAGE_GAMEPAD               0x05    /* Gamepad */
#define HID_USAGE_KEYBOARD              0x06    /* Keyboard */
#define HID_USAGE_KEYPAD                0x07    /* Keypad */
#define HID_USAGE_X                     0x30    /* X axis */
#define HID_USAGE_Y                     0x31    /* Y axis */
#define HID_USAGE_Z                     0x32    /* Z axis */
#define HID_USAGE_WHEEL                 0x38    /* Wheel */

/* HID Keyboard Modifier Bits */
#define HID_MOD_LEFT_CTRL               0x01    /* Left Control */
#define HID_MOD_LEFT_SHIFT              0x02    /* Left Shift */
#define HID_MOD_LEFT_ALT                0x04    /* Left Alt */
#define HID_MOD_LEFT_GUI                0x08    /* Left GUI (Windows key) */
#define HID_MOD_RIGHT_CTRL              0x10    /* Right Control */
#define HID_MOD_RIGHT_SHIFT             0x20    /* Right Shift */
#define HID_MOD_RIGHT_ALT               0x40    /* Right Alt */
#define HID_MOD_RIGHT_GUI               0x80    /* Right GUI */

/* HID Mouse Button Bits */
#define HID_MOUSE_LEFT                  0x01    /* Left button */
#define HID_MOUSE_RIGHT                 0x02    /* Right button */
#define HID_MOUSE_MIDDLE                0x04    /* Middle button */
#define HID_MOUSE_BUTTON4               0x08    /* Button 4 */
#define HID_MOUSE_BUTTON5               0x10    /* Button 5 */

/* HID Descriptor Structures */
typedef struct hid_descriptor {
    uint8_t  bLength;               /* Size of this descriptor */
    uint8_t  bDescriptorType;       /* HID descriptor type */
    uint16_t bcdHID;                /* HID specification version */
    uint8_t  bCountryCode;          /* Country code */
    uint8_t  bNumDescriptors;       /* Number of class descriptors */
    uint8_t  bReportDescriptorType; /* Report descriptor type */
    uint16_t wReportDescriptorLength; /* Report descriptor length */
} __attribute__((packed)) hid_descriptor_t;

/* HID Boot Keyboard Report */
typedef struct hid_keyboard_report {
    uint8_t modifiers;              /* Modifier keys */
    uint8_t reserved;               /* Reserved byte */
    uint8_t keys[6];                /* Pressed keys */
} __attribute__((packed)) hid_keyboard_report_t;

/* HID Boot Mouse Report */
typedef struct hid_mouse_report {
    uint8_t buttons;                /* Mouse buttons */
    int8_t  x;                      /* X movement */
    int8_t  y;                      /* Y movement */
    int8_t  wheel;                  /* Wheel movement */
} __attribute__((packed)) hid_mouse_report_t;

/* HID Report Item */
typedef struct hid_item {
    uint8_t type;                   /* Item type */
    uint8_t tag;                    /* Item tag */
    uint8_t size;                   /* Data size */
    uint32_t data;                  /* Item data */
} hid_item_t;

/* HID Usage */
typedef struct hid_usage {
    uint16_t usage_page;            /* Usage page */
    uint16_t usage;                 /* Usage ID */
    int32_t  logical_min;           /* Logical minimum */
    int32_t  logical_max;           /* Logical maximum */
    int32_t  physical_min;          /* Physical minimum */
    int32_t  physical_max;          /* Physical maximum */
    uint8_t  report_size;           /* Report size in bits */
    uint8_t  report_count;          /* Report count */
    uint8_t  report_id;             /* Report ID */
    uint8_t  report_type;           /* Report type */
    uint32_t unit;                  /* Unit */
    uint8_t  unit_exponent;         /* Unit exponent */
    uint32_t flags;                 /* Usage flags */
} hid_usage_t;

/* HID Collection */
typedef struct hid_collection {
    uint8_t type;                   /* Collection type */
    uint16_t usage_page;            /* Usage page */
    uint16_t usage;                 /* Usage */
    uint8_t num_usages;             /* Number of usages */
    hid_usage_t usages[HID_MAX_USAGES];
} hid_collection_t;

/* HID Report Descriptor Parser State */
typedef struct hid_parser_state {
    uint16_t usage_page;            /* Current usage page */
    int32_t  logical_min;           /* Current logical minimum */
    int32_t  logical_max;           /* Current logical maximum */
    int32_t  physical_min;          /* Current physical minimum */
    int32_t  physical_max;          /* Current physical maximum */
    uint8_t  report_size;           /* Current report size */
    uint8_t  report_count;          /* Current report count */
    uint8_t  report_id;             /* Current report ID */
    uint32_t unit;                  /* Current unit */
    uint8_t  unit_exponent;         /* Current unit exponent */
    uint32_t flags;                 /* Current flags */
} hid_parser_state_t;

/* HID Device Information */
typedef struct hid_device {
    usb_device_t* usb_device;       /* Associated USB device */
    uint8_t interface_num;          /* Interface number */
    uint8_t endpoint_in;            /* Input endpoint */
    uint8_t endpoint_out;           /* Output endpoint (optional) */
    uint16_t max_input_size;        /* Maximum input report size */
    uint16_t max_output_size;       /* Maximum output report size */
    uint16_t max_feature_size;      /* Maximum feature report size */
    
    /* HID descriptors */
    hid_descriptor_t hid_desc;
    uint8_t* report_desc;
    uint16_t report_desc_size;
    
    /* Device type and capabilities */
    uint8_t device_type;            /* Device type (keyboard, mouse, etc.) */
    bool boot_protocol;             /* Supports boot protocol */
    bool report_protocol;           /* Supports report protocol */
    uint8_t current_protocol;       /* Current protocol */
    
    /* Collections and usages */
    uint8_t num_collections;
    hid_collection_t collections[HID_MAX_COLLECTIONS];
    
    /* Input handling */
    uint8_t input_buffer[HID_MAX_REPORT_SIZE];
    usb_transfer_t* input_transfer;
    void (*input_handler)(struct hid_device* dev, uint8_t* data, uint16_t length);
    
    /* Device state */
    bool connected;
    bool configured;
    uint32_t poll_interval;         /* Polling interval in ms */
    
    /* Driver data */
    void* private_data;
} hid_device_t;

/* HID Device Types */
#define HID_TYPE_UNKNOWN            0   /* Unknown device */
#define HID_TYPE_KEYBOARD           1   /* Keyboard */
#define HID_TYPE_MOUSE              2   /* Mouse */
#define HID_TYPE_JOYSTICK           3   /* Joystick */
#define HID_TYPE_GAMEPAD            4   /* Gamepad */
#define HID_TYPE_TABLET             5   /* Graphics tablet */
#define HID_TYPE_TOUCHPAD           6   /* Touchpad */
#define HID_TYPE_GENERIC            7   /* Generic HID device */

/* HID Input Event */
typedef struct hid_event {
    uint8_t type;                   /* Event type */
    uint8_t code;                   /* Event code */
    int32_t value;                  /* Event value */
    uint32_t timestamp;             /* Event timestamp */
} hid_event_t;

/* HID Event Types */
#define HID_EVENT_KEY               1   /* Key event */
#define HID_EVENT_MOUSE_BUTTON      2   /* Mouse button event */
#define HID_EVENT_MOUSE_MOVE        3   /* Mouse movement event */
#define HID_EVENT_MOUSE_WHEEL       4   /* Mouse wheel event */
#define HID_EVENT_JOYSTICK_BUTTON   5   /* Joystick button event */
#define HID_EVENT_JOYSTICK_AXIS     6   /* Joystick axis event */

/* HID Core Functions */
int hid_init(void);
void hid_shutdown(void);

/* HID Driver Interface */
int hid_register_device(hid_device_t* device);
void hid_unregister_device(hid_device_t* device);
hid_device_t* hid_alloc_device(usb_device_t* usb_dev);
void hid_free_device(hid_device_t* device);

/* HID Protocol Operations */
int hid_get_report(hid_device_t* device, uint8_t type, uint8_t id,
                   void* buffer, uint16_t length);
int hid_set_report(hid_device_t* device, uint8_t type, uint8_t id,
                   const void* buffer, uint16_t length);
int hid_get_idle(hid_device_t* device, uint8_t report_id);
int hid_set_idle(hid_device_t* device, uint8_t report_id, uint8_t duration);
int hid_get_protocol(hid_device_t* device);
int hid_set_protocol(hid_device_t* device, uint8_t protocol);

/* HID Report Descriptor Parsing */
int hid_parse_report_descriptor(hid_device_t* device);
hid_item_t* hid_parse_next_item(uint8_t** data, uint16_t* remaining);
int hid_process_item(hid_device_t* device, hid_item_t* item, hid_parser_state_t* state);

/* HID Input Processing */
void hid_process_input_report(hid_device_t* device, uint8_t* data, uint16_t length);
void hid_process_keyboard_report(hid_device_t* device, hid_keyboard_report_t* report);
void hid_process_mouse_report(hid_device_t* device, hid_mouse_report_t* report);

/* HID Event System */
void hid_send_event(hid_event_t* event);
void hid_register_event_handler(void (*handler)(hid_event_t* event));
void hid_unregister_event_handler(void (*handler)(hid_event_t* event));

/* HID Device Detection */
uint8_t hid_detect_device_type(hid_device_t* device);
bool hid_is_keyboard(hid_device_t* device);
bool hid_is_mouse(hid_device_t* device);
bool hid_supports_boot_protocol(hid_device_t* device);

/* HID Boot Protocol Support */
int hid_init_keyboard(hid_device_t* device);
int hid_init_mouse(hid_device_t* device);
void hid_keyboard_input_handler(hid_device_t* device, uint8_t* data, uint16_t length);
void hid_mouse_input_handler(hid_device_t* device, uint8_t* data, uint16_t length);

/* HID Key Mapping */
uint8_t hid_scancode_to_ascii(uint8_t scancode, bool shift, bool alt_gr);
const char* hid_key_name(uint8_t scancode);

/* HID Utilities */
void hid_dump_descriptor(uint8_t* desc, uint16_t length);
void hid_dump_device_info(hid_device_t* device);
const char* hid_device_type_string(uint8_t type);
const char* hid_usage_page_string(uint16_t usage_page);
const char* hid_usage_string(uint16_t usage_page, uint16_t usage);

/* HID Error Codes */
#define HID_SUCCESS                 0   /* Success */
#define HID_ERROR_INVALID_PARAM     -1  /* Invalid parameter */
#define HID_ERROR_NO_MEMORY         -2  /* Out of memory */
#define HID_ERROR_NOT_SUPPORTED     -3  /* Operation not supported */
#define HID_ERROR_DEVICE_NOT_FOUND  -4  /* Device not found */
#define HID_ERROR_TRANSFER_FAILED   -5  /* Transfer failed */
#define HID_ERROR_TIMEOUT           -6  /* Operation timed out */
#define HID_ERROR_PROTOCOL          -7  /* Protocol error */
#define HID_ERROR_DESCRIPTOR        -8  /* Invalid descriptor */

#endif /* USB_HID_H */
