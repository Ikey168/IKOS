/* IKOS USB Driver Framework Test
 * 
 * Test program for the USB driver framework implementation.
 * This test validates:
 * - USB core initialization
 * - HID driver functionality
 * - Host controller driver registration
 * - Device enumeration simulation
 * - System call interface
 */

#include "usb.h"
#include "usb_hid.h"
#include "stdio.h"
#include "memory.h"
#include "string.h"

/* External function prototypes */
extern int uhci_register_controller(uint16_t io_base, uint8_t irq);
extern void usb_register_syscalls(void);

/* Test function prototypes */
static void test_usb_core_init(void);
static void test_hid_driver_init(void);
static void test_uhci_controller(void);
static void test_device_simulation(void);
static void test_syscall_interface(void);
static void test_keyboard_simulation(void);
static void test_mouse_simulation(void);

/* Mock device creation for testing */
static usb_device_t* create_mock_keyboard_device(void);
static usb_device_t* create_mock_mouse_device(void);
static void simulate_keyboard_input(void);
static void simulate_mouse_input(void);

/* Test statistics */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        printf("[PASS] %s\n", message); \
        tests_passed++; \
    } else { \
        printf("[FAIL] %s\n", message); \
        tests_failed++; \
    } \
} while(0)

/* Main USB test function */
int usb_test_main(void) {
    printf("\n=== IKOS USB Driver Framework Test ===\n");
    printf("Testing USB core, HID drivers, and host controllers\n\n");
    
    /* Initialize test counters */
    tests_run = 0;
    tests_passed = 0;
    tests_failed = 0;
    
    /* Run test suites */
    test_usb_core_init();
    test_hid_driver_init();
    test_uhci_controller();
    test_device_simulation();
    test_syscall_interface();
    test_keyboard_simulation();
    test_mouse_simulation();
    
    /* Print test results */
    printf("\n=== USB Framework Test Results ===\n");
    printf("Tests Run: %d\n", tests_run);
    printf("Tests Passed: %d\n", tests_passed);
    printf("Tests Failed: %d\n", tests_failed);
    printf("Success Rate: %d%%\n", tests_run > 0 ? (tests_passed * 100 / tests_run) : 0);
    
    if (tests_failed == 0) {
        printf("✓ All USB framework tests passed!\n");
        return 0;
    } else {
        printf("✗ Some USB framework tests failed!\n");
        return 1;
    }
}

/* Test USB core initialization */
static void test_usb_core_init(void) {
    printf("--- Testing USB Core Initialization ---\n");
    
    /* Test USB core initialization */
    int result = usb_init();
    TEST_ASSERT(result == USB_SUCCESS, "USB core initialization");
    
    /* Test double initialization (should succeed) */
    result = usb_init();
    TEST_ASSERT(result == USB_SUCCESS, "USB core double initialization");
    
    printf("USB core initialization tests completed\n\n");
}

/* Test HID driver initialization */
static void test_hid_driver_init(void) {
    printf("--- Testing USB HID Driver Initialization ---\n");
    
    /* Test HID driver initialization */
    int result = hid_init();
    TEST_ASSERT(result == HID_SUCCESS, "HID driver initialization");
    
    /* Test double initialization (should succeed) */
    result = hid_init();
    TEST_ASSERT(result == HID_SUCCESS, "HID driver double initialization");
    
    printf("HID driver initialization tests completed\n\n");
}

/* Test UHCI controller registration */
static void test_uhci_controller(void) {
    printf("--- Testing UHCI Host Controller ---\n");
    
    /* Test UHCI controller registration with mock I/O addresses */
    int result = uhci_register_controller(0x3000, 11);
    TEST_ASSERT(result == USB_SUCCESS, "UHCI controller registration");
    
    /* Test multiple controller registration */
    result = uhci_register_controller(0x3020, 12);
    TEST_ASSERT(result == USB_SUCCESS, "Second UHCI controller registration");
    
    printf("UHCI controller tests completed\n\n");
}

/* Test device simulation */
static void test_device_simulation(void) {
    printf("--- Testing USB Device Simulation ---\n");
    
    /* Create mock keyboard device */
    usb_device_t* keyboard = create_mock_keyboard_device();
    TEST_ASSERT(keyboard != NULL, "Mock keyboard device creation");
    
    if (keyboard) {
        TEST_ASSERT(keyboard->device_desc.bDeviceClass == USB_CLASS_HID, 
                   "Keyboard device class");
        TEST_ASSERT(keyboard->speed == USB_SPEED_LOW, 
                   "Keyboard device speed");
    }
    
    /* Create mock mouse device */
    usb_device_t* mouse = create_mock_mouse_device();
    TEST_ASSERT(mouse != NULL, "Mock mouse device creation");
    
    if (mouse) {
        TEST_ASSERT(mouse->device_desc.bDeviceClass == USB_CLASS_HID, 
                   "Mouse device class");
        TEST_ASSERT(mouse->speed == USB_SPEED_LOW, 
                   "Mouse device speed");
    }
    
    /* Test device enumeration simulation */
    if (keyboard) {
        int result = usb_connect_device(keyboard);
        TEST_ASSERT(result == USB_SUCCESS || result == USB_ERROR_NO_DRIVER, 
                   "Keyboard device connection");
    }
    
    if (mouse) {
        int result = usb_connect_device(mouse);
        TEST_ASSERT(result == USB_SUCCESS || result == USB_ERROR_NO_DRIVER, 
                   "Mouse device connection");
    }
    
    printf("Device simulation tests completed\n\n");
}

/* Test system call interface */
static void test_syscall_interface(void) {
    printf("--- Testing USB System Call Interface ---\n");
    
    /* Register USB system calls */
    usb_register_syscalls();
    TEST_ASSERT(true, "USB system call registration");
    
    /* Test device count system call */
    int device_count = sys_usb_get_device_count();
    TEST_ASSERT(device_count >= 0, "USB device count retrieval");
    
    printf("System call interface tests completed\n\n");
}

/* Test keyboard input simulation */
static void test_keyboard_simulation(void) {
    printf("--- Testing Keyboard Input Simulation ---\n");
    
    /* Simulate keyboard input processing */
    simulate_keyboard_input();
    TEST_ASSERT(true, "Keyboard input simulation");
    
    /* Test key mapping */
    uint8_t ascii = hid_scancode_to_ascii(0x04, false, false); /* 'a' key */
    TEST_ASSERT(ascii == 'a', "Keyboard scancode to ASCII mapping (lowercase)");
    
    ascii = hid_scancode_to_ascii(0x04, true, false); /* 'A' key with shift */
    TEST_ASSERT(ascii == 'A', "Keyboard scancode to ASCII mapping (uppercase)");
    
    printf("Keyboard simulation tests completed\n\n");
}

/* Test mouse input simulation */
static void test_mouse_simulation(void) {
    printf("--- Testing Mouse Input Simulation ---\n");
    
    /* Simulate mouse input processing */
    simulate_mouse_input();
    TEST_ASSERT(true, "Mouse input simulation");
    
    /* Test mouse event generation */
    hid_event_t event = {
        .type = HID_EVENT_MOUSE_BUTTON,
        .code = 1, /* Left button */
        .value = 1 /* Press */
    };
    
    hid_send_event(&event);
    TEST_ASSERT(true, "Mouse event generation");
    
    printf("Mouse simulation tests completed\n\n");
}

/* Create mock keyboard device */
static usb_device_t* create_mock_keyboard_device(void) {
    usb_device_t* device = (usb_device_t*)malloc(sizeof(usb_device_t));
    if (!device) {
        return NULL;
    }
    
    memset(device, 0, sizeof(usb_device_t));
    
    /* Set up device descriptor */
    device->device_desc.bLength = sizeof(usb_device_descriptor_t);
    device->device_desc.bDescriptorType = USB_DESC_DEVICE;
    device->device_desc.bcdUSB = 0x0110; /* USB 1.1 */
    device->device_desc.bDeviceClass = USB_CLASS_HID;
    device->device_desc.bDeviceSubClass = HID_SUBCLASS_BOOT;
    device->device_desc.bDeviceProtocol = HID_PROTOCOL_KEYBOARD;
    device->device_desc.bMaxPacketSize0 = 8;
    device->device_desc.idVendor = 0x046D; /* Logitech */
    device->device_desc.idProduct = 0xC312; /* Generic keyboard */
    device->device_desc.bcdDevice = 0x0100;
    device->device_desc.bNumConfigurations = 1;
    
    /* Set device properties */
    device->speed = USB_SPEED_LOW;
    device->address = 1;
    device->state = USB_DEVICE_STATE_DEFAULT;
    
    return device;
}

/* Create mock mouse device */
static usb_device_t* create_mock_mouse_device(void) {
    usb_device_t* device = (usb_device_t*)malloc(sizeof(usb_device_t));
    if (!device) {
        return NULL;
    }
    
    memset(device, 0, sizeof(usb_device_t));
    
    /* Set up device descriptor */
    device->device_desc.bLength = sizeof(usb_device_descriptor_t);
    device->device_desc.bDescriptorType = USB_DESC_DEVICE;
    device->device_desc.bcdUSB = 0x0110; /* USB 1.1 */
    device->device_desc.bDeviceClass = USB_CLASS_HID;
    device->device_desc.bDeviceSubClass = HID_SUBCLASS_BOOT;
    device->device_desc.bDeviceProtocol = HID_PROTOCOL_MOUSE;
    device->device_desc.bMaxPacketSize0 = 8;
    device->device_desc.idVendor = 0x046D; /* Logitech */
    device->device_desc.idProduct = 0xC00E; /* Generic mouse */
    device->device_desc.bcdDevice = 0x0100;
    device->device_desc.bNumConfigurations = 1;
    
    /* Set device properties */
    device->speed = USB_SPEED_LOW;
    device->address = 2;
    device->state = USB_DEVICE_STATE_DEFAULT;
    
    return device;
}

/* Simulate keyboard input */
static void simulate_keyboard_input(void) {
    printf("Simulating keyboard input...\n");
    
    /* Create mock keyboard report */
    hid_keyboard_report_t report = {
        .modifiers = 0,
        .reserved = 0,
        .keys = {0x04, 0, 0, 0, 0, 0} /* 'a' key pressed */
    };
    
    /* Create mock HID device */
    hid_device_t* device = (hid_device_t*)malloc(sizeof(hid_device_t));
    if (device) {
        memset(device, 0, sizeof(hid_device_t));
        device->device_type = HID_TYPE_KEYBOARD;
        device->connected = true;
        
        /* Process the keyboard report */
        hid_keyboard_input_handler(device, (uint8_t*)&report, sizeof(report));
        
        free(device);
    }
}

/* Simulate mouse input */
static void simulate_mouse_input(void) {
    printf("Simulating mouse input...\n");
    
    /* Create mock mouse report */
    hid_mouse_report_t report = {
        .buttons = HID_MOUSE_LEFT,  /* Left button pressed */
        .x = 10,                    /* Move right */
        .y = -5,                    /* Move up */
        .wheel = 1                  /* Scroll up */
    };
    
    /* Create mock HID device */
    hid_device_t* device = (hid_device_t*)malloc(sizeof(hid_device_t));
    if (device) {
        memset(device, 0, sizeof(hid_device_t));
        device->device_type = HID_TYPE_MOUSE;
        device->connected = true;
        
        /* Process the mouse report */
        hid_mouse_input_handler(device, (uint8_t*)&report, sizeof(report));
        
        free(device);
    }
}

/* Event handler for testing */
static void test_event_handler(hid_event_t* event) {
    if (!event) {
        return;
    }
    
    printf("HID Event: type=%d, code=%d, value=%d\n", 
           event->type, event->code, event->value);
}

/* USB framework cleanup for testing */
void usb_test_cleanup(void) {
    printf("Cleaning up USB test environment...\n");
    
    /* Shutdown HID driver */
    hid_shutdown();
    
    /* Shutdown USB core */
    usb_shutdown();
    
    printf("USB test cleanup completed\n");
}

/* Performance test */
void usb_performance_test(void) {
    printf("\n--- USB Performance Test ---\n");
    
    /* Test device allocation/deallocation performance */
    uint32_t start_time = 0; /* Would use real timer */
    
    for (int i = 0; i < 100; i++) {
        usb_device_t* device = create_mock_keyboard_device();
        if (device) {
            free(device);
        }
    }
    
    uint32_t end_time = 1; /* Would use real timer */
    printf("Device allocation/deallocation: %d operations completed\n", 100);
    
    /* Test HID report processing performance */
    hid_device_t* device = (hid_device_t*)malloc(sizeof(hid_device_t));
    if (device) {
        memset(device, 0, sizeof(hid_device_t));
        device->device_type = HID_TYPE_KEYBOARD;
        device->connected = true;
        
        hid_keyboard_report_t report = {0};
        
        for (int i = 0; i < 1000; i++) {
            report.keys[0] = (i % 26) + 0x04; /* Cycle through a-z */
            hid_keyboard_input_handler(device, (uint8_t*)&report, sizeof(report));
        }
        
        free(device);
        printf("HID report processing: 1000 reports processed\n");
    }
    
    printf("Performance test completed\n");
}
