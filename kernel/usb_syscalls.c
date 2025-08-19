/* IKOS USB System Call Interface
 * 
 * USB system calls for user-space applications in IKOS.
 * This implementation provides:
 * - USB device enumeration from user-space
 * - USB transfer submission and completion
 * - USB device information queries
 * - USB event notification system
 */

#include "usb.h"
#include "usb_hid.h"
#include "syscalls.h"
#include "memory.h"
#include "string.h"
#include "stdio.h"
#include <stdint.h>
#include <stddef.h>

/* USB System Call Numbers */
#define SYS_USB_GET_DEVICE_COUNT    200
#define SYS_USB_GET_DEVICE_INFO     201
#define SYS_USB_GET_DEVICE_DESC     202
#define SYS_USB_OPEN_DEVICE         203
#define SYS_USB_CLOSE_DEVICE        204
#define SYS_USB_CONTROL_TRANSFER    205
#define SYS_USB_BULK_TRANSFER       206
#define SYS_USB_INTERRUPT_TRANSFER  207
#define SYS_USB_HID_GET_REPORT      208
#define SYS_USB_HID_SET_REPORT      209
#define SYS_USB_REGISTER_EVENTS     210
#define SYS_USB_UNREGISTER_EVENTS   211

/* User-space USB device handle */
typedef struct usb_user_handle {
    uint8_t device_id;          /* Device ID */
    usb_device_t* device;       /* Kernel device pointer */
    uint32_t permissions;       /* Access permissions */
    uint32_t pid;               /* Process ID */
    bool valid;                 /* Handle is valid */
} usb_user_handle_t;

/* User-space transfer request */
typedef struct usb_user_transfer {
    uint8_t handle;             /* Device handle */
    uint8_t endpoint;           /* Endpoint address */
    uint8_t type;               /* Transfer type */
    uint16_t length;            /* Transfer length */
    uint32_t buffer;            /* User buffer address */
    uint32_t timeout;           /* Timeout in ms */
    
    /* Response fields */
    int32_t result;             /* Transfer result */
    uint16_t actual_length;     /* Actual bytes transferred */
} usb_user_transfer_t;

/* User-space device info */
typedef struct usb_user_device_info {
    uint8_t device_id;          /* Device ID */
    uint8_t bus_id;             /* Bus ID */
    uint8_t address;            /* Device address */
    uint8_t speed;              /* Device speed */
    uint8_t state;              /* Device state */
    uint8_t device_class;       /* Device class */
    uint8_t device_subclass;    /* Device subclass */
    uint8_t device_protocol;    /* Device protocol */
    uint16_t vendor_id;         /* Vendor ID */
    uint16_t product_id;        /* Product ID */
    uint16_t device_version;    /* Device version */
    uint8_t num_configurations; /* Number of configurations */
    char device_name[64];       /* Device name string */
} usb_user_device_info_t;

/* Global state */
static usb_user_handle_t user_handles[USB_MAX_DEVICES];
static uint8_t num_user_handles = 0;

/* Event notification */
typedef struct usb_event_listener {
    uint32_t pid;               /* Process ID */
    uint32_t event_mask;        /* Event mask */
    void* user_callback;        /* User callback function */
    bool active;                /* Listener is active */
} usb_event_listener_t;

static usb_event_listener_t event_listeners[16];
static uint8_t num_event_listeners = 0;

/* USB event types for user-space */
#define USB_USER_EVENT_DEVICE_CONNECTED     0x01
#define USB_USER_EVENT_DEVICE_DISCONNECTED  0x02
#define USB_USER_EVENT_TRANSFER_COMPLETE    0x04
#define USB_USER_EVENT_HID_INPUT            0x08

/* Simplified stub functions for compilation */
static uint32_t get_current_pid(void) {
    /* In real implementation, would get current process ID */
    return 1; /* Return dummy PID */
}

static bool is_user_address_valid(uint32_t addr, size_t size) {
    /* In real implementation, would validate user space address */
    return addr != 0 && size > 0; /* Simplified check */
}

static int copy_to_user(void* dest, const void* src, size_t size) {
    /* In real implementation, would safely copy to user space */
    if (!dest || !src || size == 0) return -1;
    memcpy(dest, src, size);
    return 0;
}

static int copy_from_user(void* dest, const void* src, size_t size) {
    /* In real implementation, would safely copy from user space */
    if (!dest || !src || size == 0) return -1;
    memcpy(dest, src, size);
    return 0;
}

static void register_syscall(int syscall_number, void* handler) {
    /* In real implementation, would register system call handler */
    printf("[USB SYS] Registering syscall %d\n", syscall_number);
}
static usb_user_handle_t* usb_alloc_user_handle(uint32_t pid);
static void usb_free_user_handle(usb_user_handle_t* handle);
static usb_user_handle_t* usb_get_user_handle(uint8_t handle_id, uint32_t pid);
static int usb_check_permissions(usb_user_handle_t* handle, uint32_t required_perms);
static void usb_notify_user_event(uint32_t event_type, void* event_data);

/* Permission flags */
#define USB_PERM_READ           0x01
#define USB_PERM_WRITE          0x02
#define USB_PERM_CONTROL        0x04
#define USB_PERM_ADMIN          0x08

/* System Call Implementations */

/* Get number of USB devices */
int sys_usb_get_device_count(void) {
    /* Count connected USB devices */
    int count = 0;
    
    /* This would iterate through the global device list */
    /* For now, return a simple count */
    for (int i = 0; i < USB_MAX_DEVICES; i++) {
        /* Check if device slot is used and device is connected */
        /* This is simplified - would need proper device enumeration */
        count++; /* Placeholder */
    }
    
    printf("[USB SYS] Device count requested: %d devices\n", count);
    return count;
}

/* Get device information */
int sys_usb_get_device_info(uint8_t device_id, usb_user_device_info_t* user_info) {
    if (!user_info || device_id >= USB_MAX_DEVICES) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Validate user pointer */
    if (!is_user_address_valid((uint32_t)user_info, sizeof(usb_user_device_info_t))) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Get device from kernel */
    usb_device_t* device = NULL; /* Would get from global device list */
    if (!device) {
        return USB_ERROR_DEVICE_NOT_FOUND;
    }
    
    /* Fill user info structure */
    usb_user_device_info_t info;
    memset(&info, 0, sizeof(info));
    
    info.device_id = device_id;
    info.bus_id = device->bus ? device->bus->bus_id : 0;
    info.address = device->address;
    info.speed = device->speed;
    info.state = device->state;
    info.device_class = device->device_desc.bDeviceClass;
    info.device_subclass = device->device_desc.bDeviceSubClass;
    info.device_protocol = device->device_desc.bDeviceProtocol;
    info.vendor_id = device->device_desc.idVendor;
    info.product_id = device->device_desc.idProduct;
    info.device_version = device->device_desc.bcdDevice;
    info.num_configurations = device->num_configurations;
    
    /* Get device name (simplified) */
    snprintf(info.device_name, sizeof(info.device_name), 
             "USB Device %04X:%04X", info.vendor_id, info.product_id);
    
    /* Copy to user space */
    if (copy_to_user(user_info, &info, sizeof(info)) != 0) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    printf("[USB SYS] Device info for device %d provided\n", device_id);
    return USB_SUCCESS;
}

/* Get device descriptor */
int sys_usb_get_device_desc(uint8_t device_id, usb_device_descriptor_t* user_desc) {
    if (!user_desc || device_id >= USB_MAX_DEVICES) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Validate user pointer */
    if (!is_user_address_valid((uint32_t)user_desc, sizeof(usb_device_descriptor_t))) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Get device from kernel */
    usb_device_t* device = NULL; /* Would get from global device list */
    if (!device) {
        return USB_ERROR_DEVICE_NOT_FOUND;
    }
    
    /* Copy descriptor to user space */
    if (copy_to_user(user_desc, &device->device_desc, sizeof(device->device_desc)) != 0) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    printf("[USB SYS] Device descriptor for device %d provided\n", device_id);
    return USB_SUCCESS;
}

/* Open USB device */
int sys_usb_open_device(uint8_t device_id, uint32_t permissions) {
    if (device_id >= USB_MAX_DEVICES) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    uint32_t pid = get_current_pid(); /* Get current process ID */
    
    /* Get device from kernel */
    usb_device_t* device = NULL; /* Would get from global device list */
    if (!device) {
        return USB_ERROR_DEVICE_NOT_FOUND;
    }
    
    /* Check if device is already open by this process */
    for (int i = 0; i < num_user_handles; i++) {
        if (user_handles[i].valid && 
            user_handles[i].device_id == device_id &&
            user_handles[i].pid == pid) {
            printf("[USB SYS] Device %d already open by process %d\n", device_id, pid);
            return i; /* Return existing handle */
        }
    }
    
    /* Allocate new handle */
    usb_user_handle_t* handle = usb_alloc_user_handle(pid);
    if (!handle) {
        return USB_ERROR_NO_MEMORY;
    }
    
    handle->device_id = device_id;
    handle->device = device;
    handle->permissions = permissions & (USB_PERM_READ | USB_PERM_WRITE | USB_PERM_CONTROL);
    handle->valid = true;
    
    uint8_t handle_id = handle - user_handles;
    
    printf("[USB SYS] Device %d opened by process %d (handle %d)\n", 
           device_id, pid, handle_id);
    
    return handle_id;
}

/* Close USB device */
int sys_usb_close_device(uint8_t handle_id) {
    uint32_t pid = get_current_pid();
    
    usb_user_handle_t* handle = usb_get_user_handle(handle_id, pid);
    if (!handle) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    printf("[USB SYS] Closing device %d for process %d\n", 
           handle->device_id, pid);
    
    usb_free_user_handle(handle);
    return USB_SUCCESS;
}

/* Control transfer */
int sys_usb_control_transfer(usb_user_transfer_t* user_transfer) {
    if (!user_transfer) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Validate user pointer */
    if (!is_user_address_valid((uint32_t)user_transfer, sizeof(usb_user_transfer_t))) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    uint32_t pid = get_current_pid();
    
    /* Copy transfer request from user space */
    usb_user_transfer_t transfer;
    if (copy_from_user(&transfer, user_transfer, sizeof(transfer)) != 0) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Get and validate handle */
    usb_user_handle_t* handle = usb_get_user_handle(transfer.handle, pid);
    if (!handle) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Check permissions */
    if (usb_check_permissions(handle, USB_PERM_CONTROL) != USB_SUCCESS) {
        return USB_ERROR_ACCESS_DENIED;
    }
    
    /* Validate user buffer */
    if (transfer.length > 0) {
        if (!is_user_address_valid(transfer.buffer, transfer.length)) {
            return USB_ERROR_INVALID_PARAM;
        }
    }
    
    /* Allocate kernel buffer */
    void* kernel_buffer = NULL;
    if (transfer.length > 0) {
        kernel_buffer = malloc(transfer.length);
        if (!kernel_buffer) {
            return USB_ERROR_NO_MEMORY;
        }
        
        /* Copy data from user space for OUT transfers */
        if (!(transfer.endpoint & 0x80)) { /* OUT direction */
            if (copy_from_user(kernel_buffer, (void*)transfer.buffer, transfer.length) != 0) {
                free(kernel_buffer);
                return USB_ERROR_INVALID_PARAM;
            }
        }
    }
    
    /* This would perform the actual control transfer */
    /* For now, simulate success */
    int result = USB_SUCCESS;
    uint16_t actual_length = transfer.length;
    
    /* Copy data back to user space for IN transfers */
    if (result >= 0 && transfer.length > 0 && (transfer.endpoint & 0x80)) { /* IN direction */
        if (copy_to_user((void*)transfer.buffer, kernel_buffer, actual_length) != 0) {
            result = USB_ERROR_INVALID_PARAM;
        }
    }
    
    /* Free kernel buffer */
    if (kernel_buffer) {
        free(kernel_buffer);
    }
    
    /* Update transfer result */
    transfer.result = result;
    transfer.actual_length = actual_length;
    
    /* Copy result back to user space */
    if (copy_to_user(user_transfer, &transfer, sizeof(transfer)) != 0) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    printf("[USB SYS] Control transfer completed (result: %d, length: %d)\n", 
           result, actual_length);
    
    return USB_SUCCESS;
}

/* HID get report */
int sys_usb_hid_get_report(uint8_t handle_id, uint8_t report_type, uint8_t report_id,
                          void* user_buffer, uint16_t length) {
    if (!user_buffer || length == 0) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    uint32_t pid = get_current_pid();
    
    usb_user_handle_t* handle = usb_get_user_handle(handle_id, pid);
    if (!handle) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Check permissions */
    if (usb_check_permissions(handle, USB_PERM_READ) != USB_SUCCESS) {
        return USB_ERROR_ACCESS_DENIED;
    }
    
    /* Validate user buffer */
    if (!is_user_address_valid((uint32_t)user_buffer, length)) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    /* Find HID device */
    /* This would search for the HID device corresponding to the USB device */
    /* For now, simulate success */
    
    printf("[USB SYS] HID get report (type=%d, id=%d, length=%d)\n", 
           report_type, report_id, length);
    
    return length; /* Return number of bytes read */
}

/* Register for USB events */
int sys_usb_register_events(uint32_t event_mask, void* user_callback) {
    if (!user_callback) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    uint32_t pid = get_current_pid();
    
    /* Check if already registered */
    for (int i = 0; i < num_event_listeners; i++) {
        if (event_listeners[i].active && event_listeners[i].pid == pid) {
            /* Update existing registration */
            event_listeners[i].event_mask = event_mask;
            event_listeners[i].user_callback = user_callback;
            
            printf("[USB SYS] Updated event registration for process %d\n", pid);
            return USB_SUCCESS;
        }
    }
    
    /* Find empty slot */
    for (int i = 0; i < 16; i++) {
        if (!event_listeners[i].active) {
            event_listeners[i].pid = pid;
            event_listeners[i].event_mask = event_mask;
            event_listeners[i].user_callback = user_callback;
            event_listeners[i].active = true;
            num_event_listeners++;
            
            printf("[USB SYS] Registered event listener for process %d\n", pid);
            return USB_SUCCESS;
        }
    }
    
    return USB_ERROR_NO_RESOURCES;
}

/* Handle Management */
static usb_user_handle_t* usb_alloc_user_handle(uint32_t pid) {
    for (int i = 0; i < USB_MAX_DEVICES; i++) {
        if (!user_handles[i].valid) {
            memset(&user_handles[i], 0, sizeof(usb_user_handle_t));
            user_handles[i].pid = pid;
            num_user_handles++;
            return &user_handles[i];
        }
    }
    return NULL;
}

static void usb_free_user_handle(usb_user_handle_t* handle) {
    if (handle && handle->valid) {
        handle->valid = false;
        num_user_handles--;
    }
}

static usb_user_handle_t* usb_get_user_handle(uint8_t handle_id, uint32_t pid) {
    if (handle_id >= USB_MAX_DEVICES) {
        return NULL;
    }
    
    usb_user_handle_t* handle = &user_handles[handle_id];
    if (handle->valid && handle->pid == pid) {
        return handle;
    }
    
    return NULL;
}

static int usb_check_permissions(usb_user_handle_t* handle, uint32_t required_perms) {
    if (!handle || !handle->valid) {
        return USB_ERROR_INVALID_PARAM;
    }
    
    if ((handle->permissions & required_perms) != required_perms) {
        return USB_ERROR_ACCESS_DENIED;
    }
    
    return USB_SUCCESS;
}

/* Event Notification */
static void usb_notify_user_event(uint32_t event_type, void* event_data) {
    for (int i = 0; i < num_event_listeners; i++) {
        if (event_listeners[i].active && 
            (event_listeners[i].event_mask & event_type)) {
            
            /* This would send an event to the user process */
            /* Implementation would depend on IPC mechanism */
            printf("[USB SYS] Notifying process %d of event 0x%X\n", 
                   event_listeners[i].pid, event_type);
        }
    }
}

/* System Call Registration */
void usb_register_syscalls(void) {
    printf("[USB SYS] Registering USB system calls\n");
    
    /* Register system call handlers */
    register_syscall(SYS_USB_GET_DEVICE_COUNT, (void*)sys_usb_get_device_count);
    register_syscall(SYS_USB_GET_DEVICE_INFO, (void*)sys_usb_get_device_info);
    register_syscall(SYS_USB_GET_DEVICE_DESC, (void*)sys_usb_get_device_desc);
    register_syscall(SYS_USB_OPEN_DEVICE, (void*)sys_usb_open_device);
    register_syscall(SYS_USB_CLOSE_DEVICE, (void*)sys_usb_close_device);
    register_syscall(SYS_USB_CONTROL_TRANSFER, (void*)sys_usb_control_transfer);
    register_syscall(SYS_USB_HID_GET_REPORT, (void*)sys_usb_hid_get_report);
    register_syscall(SYS_USB_REGISTER_EVENTS, (void*)sys_usb_register_events);
    
    /* Initialize global state */
    memset(user_handles, 0, sizeof(user_handles));
    memset(event_listeners, 0, sizeof(event_listeners));
    num_user_handles = 0;
    num_event_listeners = 0;
    
    printf("[USB SYS] USB system calls registered\n");
}

/* Cleanup */
void usb_cleanup_process_handles(uint32_t pid) {
    printf("[USB SYS] Cleaning up handles for process %d\n", pid);
    
    /* Close all handles for the process */
    for (int i = 0; i < USB_MAX_DEVICES; i++) {
        if (user_handles[i].valid && user_handles[i].pid == pid) {
            usb_free_user_handle(&user_handles[i]);
        }
    }
    
    /* Remove event listeners */
    for (int i = 0; i < 16; i++) {
        if (event_listeners[i].active && event_listeners[i].pid == pid) {
            event_listeners[i].active = false;
            num_event_listeners--;
        }
    }
}
