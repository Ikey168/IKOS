/* IKOS USB Controller Test - Issue #15 Enhancement
 * Tests USB controller integration with device driver framework
 */

#include "../include/usb_controller.h"
#include "../include/device_manager.h"
#include <stdio.h>
#include <assert.h>

/* Test USB controller initialization */
static void test_usb_init() {
    printf("Testing USB controller initialization...\n");
    
    int result = usb_controller_init();
    assert(result == USB_SUCCESS);
    
    printf("✓ USB controller subsystem initialized successfully\n");
}

/* Test USB controller registration */
static void test_usb_controller_registration() {
    printf("Testing USB controller registration...\n");
    
    /* Create a mock PCI device for USB controller */
    device_t* pci_device = device_create(DEVICE_CLASS_BUS, DEVICE_TYPE_PCI, "usb_controller");
    assert(pci_device != NULL);
    
    /* Add resources for USB controller */
    device_resource_t io_resource = {
        .type = RESOURCE_TYPE_IO_PORT,
        .base_address = 0xC000,
        .length = 32,
        .flags = 0
    };
    device_add_resource(pci_device, &io_resource);
    
    device_resource_t irq_resource = {
        .type = RESOURCE_TYPE_IRQ,
        .base_address = 11,
        .length = 1,
        .flags = 0
    };
    device_add_resource(pci_device, &irq_resource);
    
    /* Register USB controller */
    int result = usb_register_controller(pci_device);
    assert(result == USB_SUCCESS);
    
    printf("✓ USB controller registered successfully\n");
}

/* Test USB device enumeration */
static void test_usb_device_enumeration() {
    printf("Testing USB device enumeration...\n");
    
    /* Get the first USB controller */
    usb_controller_t* controller = usb_get_controllers();
    assert(controller != NULL);
    
    /* Start the controller */
    int result = usb_controller_start(controller);
    assert(result == USB_SUCCESS);
    
    /* Enumerate devices */
    result = usb_enumerate_devices(controller);
    assert(result == USB_SUCCESS);
    
    /* Verify a device was found */
    assert(controller->device_count > 0);
    assert(controller->devices[0] != NULL);
    
    printf("✓ USB device enumeration successful (found %d devices)\n", 
           controller->device_count);
}

/* Test USB device integration with IKOS device manager */
static void test_usb_device_integration() {
    printf("Testing USB device integration with device manager...\n");
    
    /* Get the first USB device */
    usb_device_t* usb_device = usb_get_devices();
    assert(usb_device != NULL);
    assert(usb_device->ikos_device != NULL);
    
    /* Verify the device was registered with device manager */
    device_t* device = usb_device->ikos_device;
    assert(device->vendor_id == usb_device->device_desc.idVendor);
    assert(device->product_id == usb_device->device_desc.idProduct);
    assert(device->state == DEVICE_STATE_ACTIVE);
    
    printf("✓ USB device integrated with device manager\n");
    printf("  - Vendor ID: 0x%04X\n", device->vendor_id);
    printf("  - Product ID: 0x%04X\n", device->product_id);
    printf("  - Device Class: %d\n", device->device_class);
    printf("  - Device Type: %d\n", device->device_type);
}

/* Test USB control transfers */
static void test_usb_control_transfer() {
    printf("Testing USB control transfers...\n");
    
    usb_device_t* device = usb_get_devices();
    assert(device != NULL);
    
    /* Test getting device descriptor */
    int result = usb_get_device_descriptor(device);
    assert(result > 0);
    
    /* Test setting device address */
    result = usb_set_address(device, 42);
    assert(result >= 0);
    assert(device->address == 42);
    
    printf("✓ USB control transfers working\n");
}

/* Test USB statistics */
static void test_usb_statistics() {
    printf("Testing USB statistics...\n");
    
    usb_stats_t stats;
    usb_get_stats(&stats);
    
    assert(stats.controllers_found > 0);
    assert(stats.devices_connected > 0);
    assert(stats.transfers_completed > 0);
    
    printf("✓ USB statistics collected\n");
    printf("  - Controllers found: %d\n", stats.controllers_found);
    printf("  - Devices connected: %d\n", stats.devices_connected);
    printf("  - Transfers completed: %d\n", stats.transfers_completed);
    printf("  - HID devices: %d\n", stats.hid_devices);
    printf("  - Storage devices: %d\n", stats.storage_devices);
}

/* Test USB HID device handling */
static void test_usb_hid_support() {
    printf("Testing USB HID device support...\n");
    
    /* Initialize HID support */
    int result = usb_hid_init();
    assert(result == USB_SUCCESS);
    
    /* Find a HID device */
    usb_device_t* device = usb_get_devices();
    while (device) {
        if (device->device_desc.bDeviceClass == USB_CLASS_HID) {
            result = usb_hid_register_device(device);
            assert(result == USB_SUCCESS);
            printf("✓ HID device registered successfully\n");
            break;
        }
        device = device->next;
    }
    
    if (!device) {
        printf("ℹ No HID devices found for testing\n");
    }
}

/* Test USB mass storage device handling */
static void test_usb_storage_support() {
    printf("Testing USB mass storage support...\n");
    
    /* Initialize mass storage support */
    int result = usb_storage_init();
    assert(result == USB_SUCCESS);
    
    /* Find a mass storage device */
    usb_device_t* device = usb_get_devices();
    while (device) {
        if (device->device_desc.bDeviceClass == USB_CLASS_MASS_STORAGE) {
            result = usb_storage_register_device(device);
            assert(result == USB_SUCCESS);
            printf("✓ Mass storage device registered successfully\n");
            break;
        }
        device = device->next;
    }
    
    if (!device) {
        printf("ℹ No mass storage devices found for testing\n");
    }
}

/* Test USB controller shutdown */
static void test_usb_shutdown() {
    printf("Testing USB controller shutdown...\n");
    
    int result = usb_controller_shutdown();
    assert(result == USB_SUCCESS);
    
    printf("✓ USB controller subsystem shutdown successfully\n");
}

/* Main test function */
int main(void) {
    printf("=== IKOS USB Controller Test Suite ===\n");
    printf("Issue #15 Enhancement - USB Support for Device Driver Framework\n\n");
    
    /* Initialize device manager first */
    printf("Initializing device manager...\n");
    int result = device_manager_init();
    assert(result == 0);
    printf("✓ Device manager initialized\n\n");
    
    /* Run USB tests */
    test_usb_init();
    test_usb_controller_registration();
    test_usb_device_enumeration();
    test_usb_device_integration();
    test_usb_control_transfer();
    test_usb_statistics();
    test_usb_hid_support();
    test_usb_storage_support();
    test_usb_shutdown();
    
    printf("\n=== All USB Controller Tests Passed! ===\n");
    printf("USB support successfully integrated with IKOS device driver framework\n");
    
    return 0;
}

/* Helper function to create mock USB devices for testing */
static void create_test_usb_devices() {
    /* This would be called during testing to create various USB device types */
    printf("Creating test USB devices for comprehensive testing...\n");
    
    /* In a real test, we would simulate device connection rather than
     * directly allocating device structures */
    printf("✓ Test USB device simulation ready\n");
}
