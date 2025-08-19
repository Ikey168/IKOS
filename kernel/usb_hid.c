/* IKOS USB HID Driver Implementation
 * 
 * USB HID driver for IKOS operating system.
 * This implementation provides:
 * - USB HID device support (keyboards, mice, etc.)
 * - HID report parsing and processing
 * - Boot protocol support for basic keyboard/mouse
 * - Input event generation and dispatch
 */

#include "usb_hid.h"
#include "usb.h"
#include "interrupts.h"
#include "memory.h"
#include "string.h"
#include "stdio.h"
#include <stddef.h>

/* Global HID state */
static hid_device_t* hid_devices[USB_MAX_DEVICES];
static uint8_t num_hid_devices = 0;
static bool hid_initialized = false;

/* Event handling */
static void (*event_handlers[16])(hid_event_t* event);
static uint8_t num_event_handlers = 0;

/* Key mapping tables */
static const uint8_t hid_to_ascii_lower[] = {
    0, 0, 0, 0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
    'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\n', '\b', '\t', ' ',
    '-', '=', '[', ']', '\\', 0, ';', '\'', '`', ',', '.', '/'
};

static const uint8_t hid_to_ascii_upper[] = {
    0, 0, 0, 0, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '\n', '\b', '\t', ' ',
    '_', '+', '{', '}', '|', 0, ':', '"', '~', '<', '>', '?'
};

/* Internal function prototypes */
static int hid_probe(usb_device_t* usb_dev);
static void hid_disconnect(usb_device_t* usb_dev);
static void hid_input_callback(usb_transfer_t* transfer);
static int hid_setup_input_transfer(hid_device_t* device);

/* USB driver structure for HID */
static usb_device_id_t hid_device_ids[] = {
    { .device_class = USB_CLASS_HID, .device_subclass = 0, .device_protocol = 0 },
    { .device_class = 0, .device_subclass = HID_SUBCLASS_BOOT, .device_protocol = HID_PROTOCOL_KEYBOARD },
    { .device_class = 0, .device_subclass = HID_SUBCLASS_BOOT, .device_protocol = HID_PROTOCOL_MOUSE },
    { 0, 0, 0, 0, 0 } /* Terminator */
};

static usb_driver_t hid_driver = {
    .name = "USB HID Driver",
    .id_table = hid_device_ids,
    .probe = hid_probe,
    .disconnect = hid_disconnect
};

/* HID Core Functions */
int hid_init(void) {
    if (hid_initialized) {
        return HID_SUCCESS;
    }
    
    printf("[HID] Initializing USB HID driver\n");
    
    /* Clear global state */
    memset(hid_devices, 0, sizeof(hid_devices));
    memset(event_handlers, 0, sizeof(event_handlers));
    num_hid_devices = 0;
    num_event_handlers = 0;
    
    /* Register USB driver */
    int result = usb_register_driver(&hid_driver);
    if (result != USB_SUCCESS) {
        printf("[HID] Failed to register USB driver: %d\n", result);
        return result;
    }
    
    hid_initialized = true;
    printf("[HID] USB HID driver initialized\n");
    
    return HID_SUCCESS;
}

void hid_shutdown(void) {
    if (!hid_initialized) {
        return;
    }
    
    printf("[HID] Shutting down USB HID driver\n");
    
    /* Disconnect all HID devices */
    for (int i = 0; i < num_hid_devices; i++) {
        if (hid_devices[i]) {
            hid_unregister_device(hid_devices[i]);
        }
    }
    
    /* Unregister USB driver */
    usb_unregister_driver(&hid_driver);
    
    hid_initialized = false;
    printf("[HID] USB HID driver shutdown complete\n");
}

/* USB Driver Interface */
static int hid_probe(usb_device_t* usb_dev) {
    printf("[HID] Probing USB device %d\n", usb_dev->device_id);
    
    /* Check if device has HID interface */
    bool has_hid = false;
    usb_interface_descriptor_t* hid_interface = NULL;
    
    if (usb_dev->device_desc.bDeviceClass == USB_CLASS_HID) {
        has_hid = true;
    } else {
        /* Check interfaces for HID class */
        /* This is simplified - would need proper configuration parsing */
        has_hid = true; /* Assume HID for now */
    }
    
    if (!has_hid) {
        return HID_ERROR_NOT_SUPPORTED;
    }
    
    /* Allocate HID device */
    hid_device_t* device = hid_alloc_device(usb_dev);
    if (!device) {
        printf("[HID] Failed to allocate HID device\n");
        return HID_ERROR_NO_MEMORY;
    }
    
    /* Register device */
    int result = hid_register_device(device);
    if (result != HID_SUCCESS) {
        printf("[HID] Failed to register HID device: %d\n", result);
        hid_free_device(device);
        return result;
    }
    
    printf("[HID] Successfully probed HID device %d\n", usb_dev->device_id);
    return HID_SUCCESS;
}

static void hid_disconnect(usb_device_t* usb_dev) {
    printf("[HID] Disconnecting USB device %d\n", usb_dev->device_id);
    
    /* Find corresponding HID device */
    for (int i = 0; i < num_hid_devices; i++) {
        if (hid_devices[i] && hid_devices[i]->usb_device == usb_dev) {
            hid_unregister_device(hid_devices[i]);
            break;
        }
    }
}

/* HID Device Management */
hid_device_t* hid_alloc_device(usb_device_t* usb_dev) {
    if (!usb_dev) {
        return NULL;
    }
    
    hid_device_t* device = malloc(sizeof(hid_device_t));
    if (!device) {
        return NULL;
    }
    
    memset(device, 0, sizeof(hid_device_t));
    device->usb_device = usb_dev;
    device->interface_num = 0; /* Default to first interface */
    device->endpoint_in = 0x81; /* Default input endpoint */
    device->max_input_size = 8; /* Default report size */
    device->poll_interval = HID_POLL_INTERVAL_MS;
    device->current_protocol = HID_PROTOCOL_REPORT;
    
    /* Detect device type */
    device->device_type = hid_detect_device_type(device);
    
    /* Set up input handler based on device type */
    switch (device->device_type) {
        case HID_TYPE_KEYBOARD:
            device->input_handler = hid_keyboard_input_handler;
            device->boot_protocol = true;
            break;
        case HID_TYPE_MOUSE:
            device->input_handler = hid_mouse_input_handler;
            device->boot_protocol = true;
            break;
        default:
            device->input_handler = NULL;
            device->boot_protocol = false;
            break;
    }
    
    printf("[HID] Allocated HID device (type: %s)\n", 
           hid_device_type_string(device->device_type));
    
    return device;
}

void hid_free_device(hid_device_t* device) {
    if (!device) {
        return;
    }
    
    /* Cancel input transfer */
    if (device->input_transfer) {
        usb_cancel_transfer(device->input_transfer);
        usb_free_transfer(device->input_transfer);
    }
    
    /* Free report descriptor */
    if (device->report_desc) {
        free(device->report_desc);
    }
    
    free(device);
}

int hid_register_device(hid_device_t* device) {
    if (!device || num_hid_devices >= USB_MAX_DEVICES) {
        return HID_ERROR_INVALID_PARAM;
    }
    
    /* Find empty slot */
    for (int i = 0; i < USB_MAX_DEVICES; i++) {
        if (!hid_devices[i]) {
            hid_devices[i] = device;
            num_hid_devices++;
            
            printf("[HID] Registered HID device %d\n", i);
            
            /* Initialize device */
            int result = HID_SUCCESS;
            
            /* Try to use boot protocol if supported */
            if (device->boot_protocol) {
                result = hid_set_protocol(device, HID_PROTOCOL_BOOT);
                if (result == HID_SUCCESS) {
                    device->current_protocol = HID_PROTOCOL_BOOT;
                    printf("[HID] Using boot protocol\n");
                }
            }
            
            /* Set up input polling */
            result = hid_setup_input_transfer(device);
            if (result != HID_SUCCESS) {
                printf("[HID] Failed to setup input transfer: %d\n", result);
                hid_devices[i] = NULL;
                num_hid_devices--;
                return result;
            }
            
            device->connected = true;
            device->configured = true;
            
            return HID_SUCCESS;
        }
    }
    
    return HID_ERROR_NO_MEMORY;
}

void hid_unregister_device(hid_device_t* device) {
    if (!device) {
        return;
    }
    
    /* Find and remove from device list */
    for (int i = 0; i < USB_MAX_DEVICES; i++) {
        if (hid_devices[i] == device) {
            printf("[HID] Unregistering HID device %d\n", i);
            
            device->connected = false;
            device->configured = false;
            
            hid_devices[i] = NULL;
            num_hid_devices--;
            
            hid_free_device(device);
            break;
        }
    }
}

/* Input Transfer Setup */
static int hid_setup_input_transfer(hid_device_t* device) {
    if (!device || !device->usb_device) {
        return HID_ERROR_INVALID_PARAM;
    }
    
    /* Allocate input transfer */
    device->input_transfer = usb_alloc_transfer(device->usb_device,
                                               device->endpoint_in,
                                               USB_TRANSFER_TYPE_INTERRUPT,
                                               device->max_input_size);
    if (!device->input_transfer) {
        return HID_ERROR_NO_MEMORY;
    }
    
    /* Set up transfer parameters */
    device->input_transfer->buffer = device->input_buffer;
    device->input_transfer->length = device->max_input_size;
    device->input_transfer->callback = hid_input_callback;
    device->input_transfer->context = device;
    device->input_transfer->interval = device->poll_interval;
    
    /* Submit transfer */
    int result = usb_submit_transfer(device->input_transfer);
    if (result != USB_SUCCESS) {
        printf("[HID] Failed to submit input transfer: %d\n", result);
        usb_free_transfer(device->input_transfer);
        device->input_transfer = NULL;
        return result;
    }
    
    printf("[HID] Input transfer setup successful\n");
    return HID_SUCCESS;
}

/* Input Callback */
static void hid_input_callback(usb_transfer_t* transfer) {
    if (!transfer || !transfer->context) {
        return;
    }
    
    hid_device_t* device = (hid_device_t*)transfer->context;
    
    if (transfer->status == USB_TRANSFER_STATUS_SUCCESS) {
        /* Process input data */
        if (device->input_handler) {
            device->input_handler(device, transfer->buffer, transfer->actual_length);
        }
        
        /* Resubmit transfer for continuous polling */
        usb_submit_transfer(transfer);
    } else if (transfer->status == USB_TRANSFER_STATUS_CANCELLED) {
        /* Transfer was cancelled, don't resubmit */
        printf("[HID] Input transfer cancelled\n");
    } else {
        /* Transfer error, try to resubmit */
        printf("[HID] Input transfer error: %d\n", transfer->status);
        usb_submit_transfer(transfer);
    }
}

/* Device Type Detection */
uint8_t hid_detect_device_type(hid_device_t* device) {
    if (!device || !device->usb_device) {
        return HID_TYPE_UNKNOWN;
    }
    
    usb_device_t* usb_dev = device->usb_device;
    
    /* Check interface protocol for boot devices */
    if (usb_dev->device_desc.bDeviceSubClass == HID_SUBCLASS_BOOT) {
        if (usb_dev->device_desc.bDeviceProtocol == HID_PROTOCOL_KEYBOARD) {
            return HID_TYPE_KEYBOARD;
        } else if (usb_dev->device_desc.bDeviceProtocol == HID_PROTOCOL_MOUSE) {
            return HID_TYPE_MOUSE;
        }
    }
    
    /* Default detection based on device class */
    if (usb_dev->device_desc.bDeviceClass == USB_CLASS_HID) {
        /* This would normally parse the report descriptor */
        /* For now, assume keyboard for demo */
        return HID_TYPE_KEYBOARD;
    }
    
    return HID_TYPE_GENERIC;
}

bool hid_is_keyboard(hid_device_t* device) {
    return device && device->device_type == HID_TYPE_KEYBOARD;
}

bool hid_is_mouse(hid_device_t* device) {
    return device && device->device_type == HID_TYPE_MOUSE;
}

bool hid_supports_boot_protocol(hid_device_t* device) {
    return device && device->boot_protocol;
}

/* Boot Protocol Input Handlers */
void hid_keyboard_input_handler(hid_device_t* device, uint8_t* data, uint16_t length) {
    if (!device || !data || length < sizeof(hid_keyboard_report_t)) {
        return;
    }
    
    hid_keyboard_report_t* report = (hid_keyboard_report_t*)data;
    
    /* Process modifier keys */
    static uint8_t prev_modifiers = 0;
    if (report->modifiers != prev_modifiers) {
        /* Modifier state changed */
        uint8_t changed = report->modifiers ^ prev_modifiers;
        
        if (changed & HID_MOD_LEFT_CTRL) {
            hid_event_t event = {
                .type = HID_EVENT_KEY,
                .code = 0x1D, /* Left Ctrl */
                .value = (report->modifiers & HID_MOD_LEFT_CTRL) ? 1 : 0
            };
            hid_send_event(&event);
        }
        
        if (changed & HID_MOD_LEFT_SHIFT) {
            hid_event_t event = {
                .type = HID_EVENT_KEY,
                .code = 0x2A, /* Left Shift */
                .value = (report->modifiers & HID_MOD_LEFT_SHIFT) ? 1 : 0
            };
            hid_send_event(&event);
        }
        
        prev_modifiers = report->modifiers;
    }
    
    /* Process regular keys */
    static uint8_t prev_keys[6] = {0};
    
    for (int i = 0; i < 6; i++) {
        uint8_t key = report->keys[i];
        
        if (key != 0 && key != prev_keys[i]) {
            /* Key pressed */
            hid_event_t event = {
                .type = HID_EVENT_KEY,
                .code = key,
                .value = 1
            };
            hid_send_event(&event);
            
            /* Generate ASCII character if possible */
            bool shift = (report->modifiers & (HID_MOD_LEFT_SHIFT | HID_MOD_RIGHT_SHIFT)) != 0;
            uint8_t ascii = hid_scancode_to_ascii(key, shift, false);
            if (ascii) {
                printf("%c", ascii);
            }
        }
    }
    
    /* Check for key releases */
    for (int i = 0; i < 6; i++) {
        uint8_t prev_key = prev_keys[i];
        if (prev_key != 0) {
            bool still_pressed = false;
            for (int j = 0; j < 6; j++) {
                if (report->keys[j] == prev_key) {
                    still_pressed = true;
                    break;
                }
            }
            
            if (!still_pressed) {
                /* Key released */
                hid_event_t event = {
                    .type = HID_EVENT_KEY,
                    .code = prev_key,
                    .value = 0
                };
                hid_send_event(&event);
            }
        }
    }
    
    memcpy(prev_keys, report->keys, 6);
}

void hid_mouse_input_handler(hid_device_t* device, uint8_t* data, uint16_t length) {
    if (!device || !data || length < sizeof(hid_mouse_report_t)) {
        return;
    }
    
    hid_mouse_report_t* report = (hid_mouse_report_t*)data;
    
    /* Process button changes */
    static uint8_t prev_buttons = 0;
    if (report->buttons != prev_buttons) {
        uint8_t changed = report->buttons ^ prev_buttons;
        
        if (changed & HID_MOUSE_LEFT) {
            hid_event_t event = {
                .type = HID_EVENT_MOUSE_BUTTON,
                .code = 1, /* Left button */
                .value = (report->buttons & HID_MOUSE_LEFT) ? 1 : 0
            };
            hid_send_event(&event);
        }
        
        if (changed & HID_MOUSE_RIGHT) {
            hid_event_t event = {
                .type = HID_EVENT_MOUSE_BUTTON,
                .code = 2, /* Right button */
                .value = (report->buttons & HID_MOUSE_RIGHT) ? 1 : 0
            };
            hid_send_event(&event);
        }
        
        if (changed & HID_MOUSE_MIDDLE) {
            hid_event_t event = {
                .type = HID_EVENT_MOUSE_BUTTON,
                .code = 3, /* Middle button */
                .value = (report->buttons & HID_MOUSE_MIDDLE) ? 1 : 0
            };
            hid_send_event(&event);
        }
        
        prev_buttons = report->buttons;
    }
    
    /* Process movement */
    if (report->x != 0 || report->y != 0) {
        if (report->x != 0) {
            hid_event_t event = {
                .type = HID_EVENT_MOUSE_MOVE,
                .code = 0, /* X axis */
                .value = report->x
            };
            hid_send_event(&event);
        }
        
        if (report->y != 0) {
            hid_event_t event = {
                .type = HID_EVENT_MOUSE_MOVE,
                .code = 1, /* Y axis */
                .value = report->y
            };
            hid_send_event(&event);
        }
    }
    
    /* Process wheel */
    if (report->wheel != 0) {
        hid_event_t event = {
            .type = HID_EVENT_MOUSE_WHEEL,
            .code = 0, /* Wheel */
            .value = report->wheel
        };
        hid_send_event(&event);
    }
}

/* Event System */
void hid_send_event(hid_event_t* event) {
    if (!event) {
        return;
    }
    
    /* Set timestamp */
    event->timestamp = 0; /* Would use real timestamp */
    
    /* Send to all registered handlers */
    for (int i = 0; i < num_event_handlers; i++) {
        if (event_handlers[i]) {
            event_handlers[i](event);
        }
    }
}

void hid_register_event_handler(void (*handler)(hid_event_t* event)) {
    if (!handler || num_event_handlers >= 16) {
        return;
    }
    
    event_handlers[num_event_handlers++] = handler;
}

void hid_unregister_event_handler(void (*handler)(hid_event_t* event)) {
    if (!handler) {
        return;
    }
    
    for (int i = 0; i < num_event_handlers; i++) {
        if (event_handlers[i] == handler) {
            /* Shift remaining handlers */
            for (int j = i; j < num_event_handlers - 1; j++) {
                event_handlers[j] = event_handlers[j + 1];
            }
            num_event_handlers--;
            break;
        }
    }
}

/* Key Mapping */
uint8_t hid_scancode_to_ascii(uint8_t scancode, bool shift, bool alt_gr) {
    if (scancode >= sizeof(hid_to_ascii_lower)) {
        return 0;
    }
    
    if (shift) {
        return hid_to_ascii_upper[scancode];
    } else {
        return hid_to_ascii_lower[scancode];
    }
}

/* HID Protocol Operations */
int hid_set_protocol(hid_device_t* device, uint8_t protocol) {
    if (!device || !device->usb_device) {
        return HID_ERROR_INVALID_PARAM;
    }
    
    /* This would send a SET_PROTOCOL control request */
    /* For now, just update the state */
    device->current_protocol = protocol;
    
    printf("[HID] Set protocol to %s\n", 
           protocol == HID_PROTOCOL_BOOT ? "boot" : "report");
    
    return HID_SUCCESS;
}

int hid_get_protocol(hid_device_t* device) {
    if (!device) {
        return HID_ERROR_INVALID_PARAM;
    }
    
    return device->current_protocol;
}

/* Utility Functions */
const char* hid_device_type_string(uint8_t type) {
    switch (type) {
        case HID_TYPE_KEYBOARD:     return "Keyboard";
        case HID_TYPE_MOUSE:        return "Mouse";
        case HID_TYPE_JOYSTICK:     return "Joystick";
        case HID_TYPE_GAMEPAD:      return "Gamepad";
        case HID_TYPE_TABLET:       return "Graphics Tablet";
        case HID_TYPE_TOUCHPAD:     return "Touchpad";
        case HID_TYPE_GENERIC:      return "Generic HID";
        default:                    return "Unknown";
    }
}
