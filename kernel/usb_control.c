/* IKOS USB Control Transfer Implementation
 * 
 * USB control transfer support for IKOS operating system.
 * This implementation provides:
 * - Standard USB control requests (GET_DESCRIPTOR, SET_ADDRESS, etc.)
 * - Control transfer state machine
 * - Descriptor parsing and management
 * - Device enumeration support
 */

#include "usb.h"
#include "memory.h"
#include "string.h"
#include "stdio.h"

/* Control transfer timeouts */
#define USB_CONTROL_TIMEOUT_MS  5000    /* 5 second timeout */
#define USB_SETUP_TIMEOUT_MS    1000    /* 1 second for setup */

/* Internal function prototypes */
static int usb_control_transfer_sync(usb_device_t* device, usb_setup_packet_t* setup,
                                    void* data, uint16_t length, uint32_t timeout);
static void usb_control_callback(usb_transfer_t* transfer);

/* Control Transfer State */
typedef struct control_state {
    usb_transfer_t* setup_transfer;
    usb_transfer_t* data_transfer;
    usb_transfer_t* status_transfer;
    
    usb_setup_packet_t* setup_packet;
    void* data_buffer;
    uint16_t data_length;
    
    volatile bool completed;
    volatile int result;
    volatile uint16_t actual_length;
} control_state_t;

/* Standard USB Control Requests */
int usb_get_descriptor(usb_device_t* device, uint8_t desc_type, uint8_t desc_index,
                       uint16_t lang_id, void* buffer, uint16_t length) {
    if (!device || !buffer) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    printf("[USB] Getting descriptor (type=%d, index=%d, length=%d)\n",
           desc_type, desc_index, length);
    
    usb_setup_packet_t setup = {
        .bmRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (desc_type << 8) | desc_index,
        .wIndex = lang_id,
        .wLength = length
    };
    
    int result = usb_control_transfer_sync(device, &setup, buffer, length, USB_CONTROL_TIMEOUT_MS);
    if (result < 0) {
        printf("[USB] Get descriptor failed: %d\n", result);
        return result;
    }
    
    printf("[USB] Get descriptor successful (%d bytes)\n", result);
    return result;
}

int usb_get_device_descriptor(usb_device_t* device, usb_device_descriptor_t* desc) {
    if (!device || !desc) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    int result = usb_get_descriptor(device, USB_DESC_DEVICE, 0, 0,
                                   desc, sizeof(usb_device_descriptor_t));
    
    if (result == sizeof(usb_device_descriptor_t)) {
        printf("[USB] Device descriptor: VID=0x%04X, PID=0x%04X, Class=0x%02X\n",
               desc->idVendor, desc->idProduct, desc->bDeviceClass);
        return USB_SUCCESS;
    }
    
    return (result < 0) ? result : USB_ERROR_PROTOCOL;
}

int usb_get_configuration_descriptor(usb_device_t* device, uint8_t config_index,
                                    void* buffer, uint16_t length) {
    if (!device || !buffer) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* First get just the configuration descriptor header */
    usb_config_descriptor_t config_header;
    int result = usb_get_descriptor(device, USB_DESC_CONFIG, config_index, 0,
                                   &config_header, sizeof(config_header));
    
    if (result != sizeof(config_header)) {
        printf("[USB] Failed to get config descriptor header: %d\n", result);
        return (result < 0) ? result : USB_ERROR_PROTOCOL;
    }
    
    /* Check if buffer is large enough for full descriptor */
    uint16_t total_length = config_header.wTotalLength;
    if (length < total_length) {
        printf("[USB] Buffer too small for config descriptor (%d < %d)\n", 
               length, total_length);
        return USB_ERROR_BUFFER_TOO_SMALL;
    }
    
    /* Get the full configuration descriptor */
    result = usb_get_descriptor(device, USB_DESC_CONFIG, config_index, 0,
                               buffer, total_length);
    
    if (result == total_length) {
        printf("[USB] Configuration descriptor: %d interfaces, %d bytes\n",
               config_header.bNumInterfaces, total_length);
        return USB_SUCCESS;
    }
    
    return (result < 0) ? result : USB_ERROR_PROTOCOL;
}

int usb_get_string_descriptor(usb_device_t* device, uint8_t string_index,
                             uint16_t lang_id, void* buffer, uint16_t length) {
    if (!device || !buffer || string_index == 0) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    int result = usb_get_descriptor(device, USB_DESC_STRING, string_index, lang_id,
                                   buffer, length);
    
    if (result >= 2) {
        usb_string_descriptor_t* string_desc = (usb_string_descriptor_t*)buffer;
        printf("[USB] String descriptor %d: length=%d\n", 
               string_index, string_desc->bLength);
        return USB_SUCCESS;
    }
    
    return (result < 0) ? result : USB_ERROR_PROTOCOL;
}

int usb_set_address(usb_device_t* device, uint8_t address) {
    if (!device || address > USB_MAX_ADDRESS) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    printf("[USB] Setting device address to %d\n", address);
    
    usb_setup_packet_t setup = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest = USB_REQ_SET_ADDRESS,
        .wValue = address,
        .wIndex = 0,
        .wLength = 0
    };
    
    int result = usb_control_transfer_sync(device, &setup, NULL, 0, USB_SETUP_TIMEOUT_MS);
    if (result < 0) {
        printf("[USB] Set address failed: %d\n", result);
        return result;
    }
    
    /* Wait for device to process the address change */
    for (volatile int i = 0; i < 10000; i++);
    
    printf("[USB] Set address successful\n");
    return USB_SUCCESS;
}

int usb_set_configuration(usb_device_t* device, uint8_t config_value) {
    if (!device) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    printf("[USB] Setting configuration to %d\n", config_value);
    
    usb_setup_packet_t setup = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest = USB_REQ_SET_CONFIGURATION,
        .wValue = config_value,
        .wIndex = 0,
        .wLength = 0
    };
    
    int result = usb_control_transfer_sync(device, &setup, NULL, 0, USB_CONTROL_TIMEOUT_MS);
    if (result < 0) {
        printf("[USB] Set configuration failed: %d\n", result);
        return result;
    }
    
    printf("[USB] Set configuration successful\n");
    return USB_SUCCESS;
}

int usb_get_configuration(usb_device_t* device, uint8_t* config_value) {
    if (!device || !config_value) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    usb_setup_packet_t setup = {
        .bmRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest = USB_REQ_GET_CONFIGURATION,
        .wValue = 0,
        .wIndex = 0,
        .wLength = 1
    };
    
    int result = usb_control_transfer_sync(device, &setup, config_value, 1, USB_CONTROL_TIMEOUT_MS);
    if (result == 1) {
        printf("[USB] Current configuration: %d\n", *config_value);
        return USB_SUCCESS;
    }
    
    return (result < 0) ? result : USB_ERROR_PROTOCOL;
}

int usb_set_interface(usb_device_t* device, uint8_t interface_num, uint8_t alt_setting) {
    if (!device) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    printf("[USB] Setting interface %d to alternate setting %d\n", 
           interface_num, alt_setting);
    
    usb_setup_packet_t setup = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_INTERFACE,
        .bRequest = USB_REQ_SET_INTERFACE,
        .wValue = alt_setting,
        .wIndex = interface_num,
        .wLength = 0
    };
    
    int result = usb_control_transfer_sync(device, &setup, NULL, 0, USB_CONTROL_TIMEOUT_MS);
    if (result < 0) {
        printf("[USB] Set interface failed: %d\n", result);
        return result;
    }
    
    printf("[USB] Set interface successful\n");
    return USB_SUCCESS;
}

int usb_get_interface(usb_device_t* device, uint8_t interface_num, uint8_t* alt_setting) {
    if (!device || !alt_setting) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    usb_setup_packet_t setup = {
        .bmRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE,
        .bRequest = USB_REQ_GET_INTERFACE,
        .wValue = 0,
        .wIndex = interface_num,
        .wLength = 1
    };
    
    int result = usb_control_transfer_sync(device, &setup, alt_setting, 1, USB_CONTROL_TIMEOUT_MS);
    if (result == 1) {
        printf("[USB] Interface %d alternate setting: %d\n", 
               interface_num, *alt_setting);
        return USB_SUCCESS;
    }
    
    return (result < 0) ? result : USB_ERROR_PROTOCOL;
}

int usb_clear_halt(usb_device_t* device, uint8_t endpoint) {
    if (!device) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    printf("[USB] Clearing halt on endpoint 0x%02X\n", endpoint);
    
    usb_setup_packet_t setup = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT,
        .bRequest = USB_REQ_CLEAR_FEATURE,
        .wValue = USB_FEATURE_ENDPOINT_HALT,
        .wIndex = endpoint,
        .wLength = 0
    };
    
    int result = usb_control_transfer_sync(device, &setup, NULL, 0, USB_CONTROL_TIMEOUT_MS);
    if (result < 0) {
        printf("[USB] Clear halt failed: %d\n", result);
        return result;
    }
    
    printf("[USB] Clear halt successful\n");
    return USB_SUCCESS;
}

int usb_get_status(usb_device_t* device, uint8_t recipient, uint8_t index, uint16_t* status) {
    if (!device || !status) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    uint8_t request_type = USB_DIR_IN | USB_TYPE_STANDARD;
    switch (recipient) {
        case USB_RECIP_DEVICE:
            request_type |= USB_RECIP_DEVICE;
            break;
        case USB_RECIP_INTERFACE:
            request_type |= USB_RECIP_INTERFACE;
            break;
        case USB_RECIP_ENDPOINT:
            request_type |= USB_RECIP_ENDPOINT;
            break;
        default:
            return USB_ERROR_INVALID_PARAM;
    }
    
    usb_setup_packet_t setup = {
        .bmRequestType = request_type,
        .bRequest = USB_REQ_GET_STATUS,
        .wValue = 0,
        .wIndex = index,
        .wLength = 2
    };
    
    int result = usb_control_transfer_sync(device, &setup, status, 2, USB_CONTROL_TIMEOUT_MS);
    if (result == 2) {
        printf("[USB] Status: 0x%04X\n", *status);
        return USB_SUCCESS;
    }
    
    return (result < 0) ? result : USB_ERROR_PROTOCOL;
}

/* Synchronous Control Transfer Implementation */
static int usb_control_transfer_sync(usb_device_t* device, usb_setup_packet_t* setup,
                                    void* data, uint16_t length, uint32_t timeout) {
    if (!device || !setup) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Allocate control state */
    control_state_t state;
    memset(&state, 0, sizeof(state));
    state.setup_packet = setup;
    state.data_buffer = data;
    state.data_length = length;
    state.completed = false;
    state.result = USB_ERROR_TIMEOUT;
    
    /* Allocate setup transfer */
    state.setup_transfer = usb_alloc_transfer(device, 0, USB_TRANSFER_TYPE_CONTROL, 64);
    if (!state.setup_transfer) {
        return USB_ERROR_NO_MEMORY;
    }
    
    /* Setup the setup transfer */
    state.setup_transfer->buffer = (uint8_t*)setup;
    state.setup_transfer->length = sizeof(usb_setup_packet_t);
    state.setup_transfer->callback = usb_control_callback;
    state.setup_transfer->context = &state;
    
    /* Submit setup transfer */
    int result = usb_submit_transfer(state.setup_transfer);
    if (result != USB_SUCCESS) {
        usb_free_transfer(state.setup_transfer);
        return result;
    }
    
    /* Wait for completion with timeout */
    uint32_t start_time = 0; /* Would use real timer */
    uint32_t elapsed = 0;
    
    while (!state.completed && elapsed < timeout) {
        /* Simple busy wait - in real implementation would use proper scheduling */
        for (volatile int i = 0; i < 1000; i++);
        elapsed++; /* Simplified time tracking */
    }
    
    /* Clean up transfers */
    if (state.setup_transfer) {
        if (!state.completed) {
            usb_cancel_transfer(state.setup_transfer);
        }
        usb_free_transfer(state.setup_transfer);
    }
    
    if (state.data_transfer) {
        usb_free_transfer(state.data_transfer);
    }
    
    if (state.status_transfer) {
        usb_free_transfer(state.status_transfer);
    }
    
    if (!state.completed) {
        printf("[USB] Control transfer timeout\n");
        return USB_ERROR_TIMEOUT;
    }
    
    return state.result >= 0 ? state.actual_length : state.result;
}

/* Control Transfer Callback */
static void usb_control_callback(usb_transfer_t* transfer) {
    if (!transfer || !transfer->context) {
        return;
    }
    
    control_state_t* state = (control_state_t*)transfer->context;
    
    if (transfer->status != USB_TRANSFER_STATUS_SUCCESS) {
        printf("[USB] Control transfer failed: %d\n", transfer->status);
        state->result = USB_ERROR_TRANSFER_FAILED;
        state->completed = true;
        return;
    }
    
    if (transfer == state->setup_transfer) {
        /* Setup phase completed */
        printf("[USB] Setup phase completed\n");
        
        if (state->data_length > 0) {
            /* Data phase needed */
            state->data_transfer = usb_alloc_transfer(transfer->device, 0, 
                                                     USB_TRANSFER_TYPE_CONTROL, 64);
            if (!state->data_transfer) {
                state->result = USB_ERROR_NO_MEMORY;
                state->completed = true;
                return;
            }
            
            /* Setup data transfer */
            state->data_transfer->buffer = (uint8_t*)state->data_buffer;
            state->data_transfer->length = state->data_length;
            state->data_transfer->callback = usb_control_callback;
            state->data_transfer->context = state;
            
            /* Submit data transfer */
            int result = usb_submit_transfer(state->data_transfer);
            if (result != USB_SUCCESS) {
                state->result = result;
                state->completed = true;
                return;
            }
        } else {
            /* No data phase, go to status phase */
            /* For simplicity, we'll skip the status phase in this implementation */
            state->result = USB_SUCCESS;
            state->actual_length = 0;
            state->completed = true;
        }
    } else if (transfer == state->data_transfer) {
        /* Data phase completed */
        printf("[USB] Data phase completed (%d bytes)\n", transfer->actual_length);
        
        state->actual_length = transfer->actual_length;
        
        /* Status phase would go here in a complete implementation */
        /* For now, consider the transfer complete */
        state->result = USB_SUCCESS;
        state->completed = true;
    }
}

/* Descriptor Utilities */
void usb_dump_device_descriptor(usb_device_descriptor_t* desc) {
    if (!desc) {
        return;
    }
    
    printf("USB Device Descriptor:\n");
    printf("  bLength: %d\n", desc->bLength);
    printf("  bDescriptorType: %d\n", desc->bDescriptorType);
    printf("  bcdUSB: 0x%04X\n", desc->bcdUSB);
    printf("  bDeviceClass: 0x%02X (%s)\n", desc->bDeviceClass, 
           usb_class_string(desc->bDeviceClass));
    printf("  bDeviceSubClass: 0x%02X\n", desc->bDeviceSubClass);
    printf("  bDeviceProtocol: 0x%02X\n", desc->bDeviceProtocol);
    printf("  bMaxPacketSize0: %d\n", desc->bMaxPacketSize0);
    printf("  idVendor: 0x%04X\n", desc->idVendor);
    printf("  idProduct: 0x%04X\n", desc->idProduct);
    printf("  bcdDevice: 0x%04X\n", desc->bcdDevice);
    printf("  iManufacturer: %d\n", desc->iManufacturer);
    printf("  iProduct: %d\n", desc->iProduct);
    printf("  iSerialNumber: %d\n", desc->iSerialNumber);
    printf("  bNumConfigurations: %d\n", desc->bNumConfigurations);
}

void usb_dump_config_descriptor(usb_config_descriptor_t* desc) {
    if (!desc) {
        return;
    }
    
    printf("USB Configuration Descriptor:\n");
    printf("  bLength: %d\n", desc->bLength);
    printf("  bDescriptorType: %d\n", desc->bDescriptorType);
    printf("  wTotalLength: %d\n", desc->wTotalLength);
    printf("  bNumInterfaces: %d\n", desc->bNumInterfaces);
    printf("  bConfigurationValue: %d\n", desc->bConfigurationValue);
    printf("  iConfiguration: %d\n", desc->iConfiguration);
    printf("  bmAttributes: 0x%02X\n", desc->bmAttributes);
    printf("  bMaxPower: %d mA\n", desc->bMaxPower * 2);
}
