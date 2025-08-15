/* IKOS Device Driver Framework Test
 * Issue #15 - Device Driver Framework Test Suite
 * 
 * Comprehensive test for device manager, PCI bus driver,
 * and IDE storage controller integration.
 */

#include "device_manager.h"
#include "pci.h"
#include "ide_driver.h"
#include "memory.h"
#include <string.h>

/* ================================
 * Test Framework
 * ================================ */

static uint32_t g_tests_run = 0;
static uint32_t g_tests_passed = 0;
static uint32_t g_tests_failed = 0;

static void debug_print(const char* format, ...) {
    // Placeholder for debug printing
    (void)format;
}

#define TEST_ASSERT(condition, message) \
    do { \
        g_tests_run++; \
        if (condition) { \
            g_tests_passed++; \
            debug_print("[PASS] %s\n", message); \
        } else { \
            g_tests_failed++; \
            debug_print("[FAIL] %s\n", message); \
        } \
    } while(0)

#define TEST_START(name) \
    debug_print("\n=== Test: %s ===\n", name)

#define TEST_END() \
    debug_print("Tests: %u, Passed: %u, Failed: %u\n", \
                g_tests_run, g_tests_passed, g_tests_failed)

/* ================================
 * Mock Device Drivers
 * ================================ */

/* Mock keyboard driver for testing */
static int mock_keyboard_probe(device_t* device) {
    return (device->type == DEVICE_TYPE_KEYBOARD) ? DEVICE_SUCCESS : DEVICE_ERROR_NOT_SUPPORTED;
}

static int mock_keyboard_attach(device_t* device) {
    debug_print("MOCK: Attaching keyboard driver to %s\n", device->name);
    device->driver_data = kmalloc(64); /* Mock driver data */
    return DEVICE_SUCCESS;
}

static int mock_keyboard_detach(device_t* device) {
    debug_print("MOCK: Detaching keyboard driver from %s\n", device->name);
    if (device->driver_data) {
        kfree(device->driver_data);
        device->driver_data = NULL;
    }
    return DEVICE_SUCCESS;
}

static device_operations_t mock_keyboard_ops = {
    .probe = mock_keyboard_probe,
    .attach = mock_keyboard_attach,
    .detach = mock_keyboard_detach,
    .remove = NULL,
    .suspend = NULL,
    .resume = NULL,
    .power_off = NULL,
    .read = NULL,
    .write = NULL,
    .ioctl = NULL,
    .irq_handler = NULL
};

static device_driver_t mock_keyboard_driver = {
    .name = "mock_keyboard",
    .driver_id = 0,
    .version = 0x0100,
    .supported_class = DEVICE_CLASS_INPUT,
    .supported_vendors = NULL,
    .supported_devices = NULL,
    .ops = &mock_keyboard_ops,
    .loaded = false,
    .device_count = 0,
    .next = NULL
};

/* Mock storage driver for testing */
static int mock_storage_probe(device_t* device) {
    return (device->class == DEVICE_CLASS_STORAGE) ? DEVICE_SUCCESS : DEVICE_ERROR_NOT_SUPPORTED;
}

static int mock_storage_attach(device_t* device) {
    debug_print("MOCK: Attaching storage driver to %s\n", device->name);
    return DEVICE_SUCCESS;
}

static device_operations_t mock_storage_ops = {
    .probe = mock_storage_probe,
    .attach = mock_storage_attach,
    .detach = NULL,
    .remove = NULL,
    .suspend = NULL,
    .resume = NULL,
    .power_off = NULL,
    .read = NULL,
    .write = NULL,
    .ioctl = NULL,
    .irq_handler = NULL
};

static device_driver_t mock_storage_driver = {
    .name = "mock_storage",
    .driver_id = 0,
    .version = 0x0100,
    .supported_class = DEVICE_CLASS_STORAGE,
    .supported_vendors = NULL,
    .supported_devices = NULL,
    .ops = &mock_storage_ops,
    .loaded = false,
    .device_count = 0,
    .next = NULL
};

/* ================================
 * Device Manager Tests
 * ================================ */

static void test_device_manager_init(void) {
    TEST_START("Device Manager Initialization");
    
    int result = device_manager_init();
    TEST_ASSERT(result == DEVICE_SUCCESS, "Device manager initialization should succeed");
    
    device_manager_stats_t stats;
    device_manager_get_stats(&stats);
    TEST_ASSERT(stats.total_devices == 0, "Initial device count should be zero");
    TEST_ASSERT(stats.total_drivers == 0, "Initial driver count should be zero");
}

static void test_device_creation_and_registration(void) {
    TEST_START("Device Creation and Registration");
    
    /* Create a test device */
    device_t* kbd_device = device_create(DEVICE_CLASS_INPUT, DEVICE_TYPE_KEYBOARD, "test_keyboard");
    TEST_ASSERT(kbd_device != NULL, "Device creation should succeed");
    TEST_ASSERT(kbd_device->device_id != 0, "Device should have valid ID");
    TEST_ASSERT(strcmp(kbd_device->name, "test_keyboard") == 0, "Device name should match");
    
    /* Register the device */
    int result = device_register(kbd_device);
    TEST_ASSERT(result == DEVICE_SUCCESS, "Device registration should succeed");
    TEST_ASSERT(kbd_device->state == DEVICE_STATE_DETECTED, "Device state should be DETECTED");
    
    /* Check device count */
    device_manager_stats_t stats;
    device_manager_get_stats(&stats);
    TEST_ASSERT(stats.total_devices == 1, "Device count should be 1");
    
    /* Test device lookup */
    device_t* found_device = device_find_by_name("test_keyboard");
    TEST_ASSERT(found_device == kbd_device, "Should find device by name");
    
    found_device = device_find_by_id(kbd_device->device_id);
    TEST_ASSERT(found_device == kbd_device, "Should find device by ID");
    
    found_device = device_find_by_type(DEVICE_TYPE_KEYBOARD);
    TEST_ASSERT(found_device == kbd_device, "Should find device by type");
}

static void test_driver_registration_and_binding(void) {
    TEST_START("Driver Registration and Binding");
    
    /* Register the mock keyboard driver */
    int result = driver_register(&mock_keyboard_driver);
    TEST_ASSERT(result == DEVICE_SUCCESS, "Driver registration should succeed");
    TEST_ASSERT(mock_keyboard_driver.loaded == true, "Driver should be marked as loaded");
    
    /* Check driver count */
    device_manager_stats_t stats;
    device_manager_get_stats(&stats);
    TEST_ASSERT(stats.total_drivers == 1, "Driver count should be 1");
    
    /* Check if driver was automatically bound to device */
    device_t* kbd_device = device_find_by_name("test_keyboard");
    TEST_ASSERT(kbd_device != NULL, "Keyboard device should exist");
    TEST_ASSERT(kbd_device->driver == &mock_keyboard_driver, "Driver should be automatically bound");
    TEST_ASSERT(kbd_device->state == DEVICE_STATE_READY, "Device should be in READY state");
    TEST_ASSERT(mock_keyboard_driver.device_count == 1, "Driver should have 1 device");
}

static void test_device_enumeration(void) {
    TEST_START("Device Enumeration");
    
    /* Create additional test devices */
    device_t* storage_device = device_create(DEVICE_CLASS_STORAGE, DEVICE_TYPE_IDE, "test_ide");
    TEST_ASSERT(storage_device != NULL, "Storage device creation should succeed");
    
    device_register(storage_device);
    
    device_t* network_device = device_create(DEVICE_CLASS_NETWORK, DEVICE_TYPE_ETHERNET, "test_ethernet");
    TEST_ASSERT(network_device != NULL, "Network device creation should succeed");
    
    device_register(network_device);
    
    /* Test enumeration */
    device_t* devices[10];
    int count = device_enumerate_all(devices, 10);
    TEST_ASSERT(count == 3, "Should enumerate 3 devices");
    
    /* Test enumeration by class */
    count = device_enumerate_by_class(DEVICE_CLASS_INPUT, devices, 10);
    TEST_ASSERT(count == 1, "Should find 1 input device");
    
    count = device_enumerate_by_class(DEVICE_CLASS_STORAGE, devices, 10);
    TEST_ASSERT(count == 1, "Should find 1 storage device");
    
    /* Test device count functions */
    uint32_t total_count = device_get_count();
    TEST_ASSERT(total_count == 3, "Total device count should be 3");
    
    uint32_t input_count = device_get_count_by_class(DEVICE_CLASS_INPUT);
    TEST_ASSERT(input_count == 1, "Input device count should be 1");
}

static void test_resource_management(void) {
    TEST_START("Resource Management");
    
    device_t* storage_device = device_find_by_name("test_ide");
    TEST_ASSERT(storage_device != NULL, "Storage device should exist");
    
    /* Add resources to device */
    int result = device_add_resource(storage_device, 0x1F0, 8, RESOURCE_TYPE_IO_PORT);
    TEST_ASSERT(result == DEVICE_SUCCESS, "Adding I/O resource should succeed");
    
    result = device_add_resource(storage_device, 0x3F6, 1, RESOURCE_TYPE_IO_PORT);
    TEST_ASSERT(result == DEVICE_SUCCESS, "Adding control resource should succeed");
    
    result = device_add_resource(storage_device, 14, 1, RESOURCE_TYPE_IRQ);
    TEST_ASSERT(result == DEVICE_SUCCESS, "Adding IRQ resource should succeed");
    
    /* Test resource lookup */
    device_resource_t* resource = device_get_resource(storage_device, RESOURCE_TYPE_IO_PORT, 0);
    TEST_ASSERT(resource != NULL, "Should find I/O resource");
    TEST_ASSERT(resource->base_address == 0x1F0, "I/O base should match");
    TEST_ASSERT(resource->size == 8, "I/O size should match");
    
    resource = device_get_resource(storage_device, RESOURCE_TYPE_IRQ, 0);
    TEST_ASSERT(resource != NULL, "Should find IRQ resource");
    TEST_ASSERT(resource->base_address == 14, "IRQ number should match");
}

/* ================================
 * PCI Bus Driver Tests
 * ================================ */

static void test_pci_initialization(void) {
    TEST_START("PCI Bus Driver Initialization");
    
    int result = pci_init();
    TEST_ASSERT(result == PCI_SUCCESS || result == PCI_ERROR_ACCESS_DENIED, 
                "PCI initialization should succeed or fail gracefully");
    
    if (result == PCI_SUCCESS) {
        debug_print("PCI: Configuration mechanism available\n");
        
        pci_stats_t stats;
        pci_get_stats(&stats);
        debug_print("PCI: Found %u devices on %u buses\n", 
                    stats.total_devices, stats.buses_scanned);
    } else {
        debug_print("PCI: Configuration mechanism not available (likely QEMU without PCI)\n");
    }
}

static void test_pci_device_enumeration(void) {
    TEST_START("PCI Device Enumeration");
    
    /* Test basic PCI configuration access */
    bool host_bridge_exists = pci_device_exists(0, 0, 0);
    debug_print("PCI: Host bridge at 00:00.0: %s\n", host_bridge_exists ? "Present" : "Not present");
    
    if (host_bridge_exists) {
        pci_device_info_t info;
        int result = pci_get_device_info(0, 0, 0, &info);
        TEST_ASSERT(result == PCI_SUCCESS, "Should read host bridge info");
        
        debug_print("PCI: Host bridge: %04x:%04x class %02x\n",
                    info.vendor_id, info.device_id, info.class_code);
    }
    
    /* Test bus scanning */
    int scan_result = pci_scan_bus(0);
    TEST_ASSERT(scan_result == PCI_SUCCESS, "Bus 0 scan should succeed");
}

static void test_pci_class_conversion(void) {
    TEST_START("PCI Class Conversion");
    
    device_class_t dev_class = pci_class_to_device_class(PCI_CLASS_MASS_STORAGE);
    TEST_ASSERT(dev_class == DEVICE_CLASS_STORAGE, "Mass storage class should convert correctly");
    
    dev_class = pci_class_to_device_class(PCI_CLASS_NETWORK);
    TEST_ASSERT(dev_class == DEVICE_CLASS_NETWORK, "Network class should convert correctly");
    
    device_type_t dev_type = pci_subclass_to_device_type(PCI_CLASS_MASS_STORAGE, PCI_SUBCLASS_IDE);
    TEST_ASSERT(dev_type == DEVICE_TYPE_IDE, "IDE subclass should convert correctly");
    
    dev_type = pci_subclass_to_device_type(PCI_CLASS_MASS_STORAGE, PCI_SUBCLASS_SATA);
    TEST_ASSERT(dev_type == DEVICE_TYPE_SATA, "SATA subclass should convert correctly");
}

/* ================================
 * IDE Driver Tests
 * ================================ */

static void test_ide_driver_initialization(void) {
    TEST_START("IDE Driver Initialization");
    
    int result = ide_driver_init();
    TEST_ASSERT(result == IDE_SUCCESS, "IDE driver initialization should succeed");
    
    ide_stats_t stats;
    ide_get_stats(&stats);
    TEST_ASSERT(stats.controllers_found == 0, "Initial controller count should be zero");
    TEST_ASSERT(stats.drives_found == 0, "Initial drive count should be zero");
}

static void test_ide_mock_controller(void) {
    TEST_START("IDE Mock Controller Test");
    
    /* Create a mock IDE controller device */
    ide_device_t* ide_dev = (ide_device_t*)kmalloc(sizeof(ide_device_t));
    TEST_ASSERT(ide_dev != NULL, "IDE device allocation should succeed");
    
    memset(ide_dev, 0, sizeof(ide_device_t));
    
    /* Initialize with standard primary IDE controller settings */
    int result = ide_init_controller(ide_dev, IDE_PRIMARY_BASE, IDE_PRIMARY_CTRL, 14);
    
    /* Note: This will likely fail in QEMU without proper IDE emulation, 
     * but we test the initialization path */
    if (result == IDE_SUCCESS) {
        TEST_ASSERT(ide_dev->initialized == true, "Controller should be marked as initialized");
        TEST_ASSERT(ide_dev->controller.io_base == IDE_PRIMARY_BASE, "I/O base should match");
        TEST_ASSERT(ide_dev->controller.ctrl_base == IDE_PRIMARY_CTRL, "Control base should match");
        TEST_ASSERT(ide_dev->controller.irq == 14, "IRQ should match");
        
        debug_print("IDE: Primary controller initialized successfully\n");
    } else {
        debug_print("IDE: Primary controller initialization failed (expected in QEMU)\n");
    }
    
    kfree(ide_dev);
}

/* ================================
 * Integration Tests
 * ================================ */

static void test_device_driver_integration(void) {
    TEST_START("Device-Driver Integration");
    
    /* Register storage driver */
    int result = driver_register(&mock_storage_driver);
    TEST_ASSERT(result == DEVICE_SUCCESS, "Storage driver registration should succeed");
    
    /* Check if storage driver was bound to storage device */
    device_t* storage_device = device_find_by_name("test_ide");
    TEST_ASSERT(storage_device != NULL, "Storage device should exist");
    TEST_ASSERT(storage_device->driver == &mock_storage_driver, "Storage driver should be bound");
    
    /* Test driver detaching */
    result = device_detach_driver(storage_device);
    TEST_ASSERT(result == DEVICE_SUCCESS, "Driver detaching should succeed");
    TEST_ASSERT(storage_device->driver == NULL, "Device should have no driver");
    TEST_ASSERT(storage_device->state == DEVICE_STATE_DETECTED, "Device should be in DETECTED state");
    
    /* Test manual driver attachment */
    result = device_attach_driver(storage_device, &mock_storage_driver);
    TEST_ASSERT(result == DEVICE_SUCCESS, "Manual driver attachment should succeed");
    TEST_ASSERT(storage_device->driver == &mock_storage_driver, "Driver should be attached");
    TEST_ASSERT(storage_device->state == DEVICE_STATE_READY, "Device should be READY");
}

static void test_device_hierarchy(void) {
    TEST_START("Device Hierarchy");
    
    /* Create a PCI bus device */
    device_t* pci_bus = device_create(DEVICE_CLASS_BRIDGE, DEVICE_TYPE_UNKNOWN, "pci_bus");
    TEST_ASSERT(pci_bus != NULL, "PCI bus device creation should succeed");
    device_register(pci_bus);
    
    /* Create a child device */
    device_t* pci_device = device_create(DEVICE_CLASS_STORAGE, DEVICE_TYPE_SATA, "pci_sata");
    TEST_ASSERT(pci_device != NULL, "PCI SATA device creation should succeed");
    device_register(pci_device);
    
    /* Set up hierarchy */
    int result = device_add_child(pci_bus, pci_device);
    TEST_ASSERT(result == DEVICE_SUCCESS, "Adding child device should succeed");
    TEST_ASSERT(pci_device->parent == pci_bus, "Child should have correct parent");
    TEST_ASSERT(pci_bus->children == pci_device, "Parent should have child");
    
    /* Test hierarchy navigation */
    device_t* parent = device_get_parent(pci_device);
    TEST_ASSERT(parent == pci_bus, "Should find correct parent");
    
    device_t* child = device_get_children(pci_bus);
    TEST_ASSERT(child == pci_device, "Should find correct child");
}

/* ================================
 * Main Test Function
 * ================================ */

/**
 * Run comprehensive device driver framework tests
 */
void test_device_driver_framework(void) {
    debug_print("\n");
    debug_print("========================================\n");
    debug_print("IKOS Device Driver Framework Test Suite\n");
    debug_print("Issue #15 - Comprehensive Testing\n");
    debug_print("========================================\n");
    
    /* Reset test counters */
    g_tests_run = 0;
    g_tests_passed = 0;
    g_tests_failed = 0;
    
    /* Device Manager Tests */
    test_device_manager_init();
    test_device_creation_and_registration();
    test_driver_registration_and_binding();
    test_device_enumeration();
    test_resource_management();
    
    /* PCI Bus Driver Tests */
    test_pci_initialization();
    test_pci_device_enumeration();
    test_pci_class_conversion();
    
    /* IDE Driver Tests */
    test_ide_driver_initialization();
    test_ide_mock_controller();
    
    /* Integration Tests */
    test_device_driver_integration();
    test_device_hierarchy();
    
    /* Final Results */
    debug_print("\n");
    debug_print("========================================\n");
    debug_print("Test Results Summary\n");
    debug_print("========================================\n");
    TEST_END();
    
    if (g_tests_failed == 0) {
        debug_print("\n✅ All tests passed! Device Driver Framework is working correctly.\n");
    } else {
        debug_print("\n❌ Some tests failed. Please review the implementation.\n");
    }
    
    /* Print final statistics */
    device_manager_stats_t dev_stats;
    device_manager_get_stats(&dev_stats);
    
    debug_print("\nDevice Manager Statistics:\n");
    debug_print("  Total Devices: %u\n", dev_stats.total_devices);
    debug_print("  Active Devices: %u\n", dev_stats.active_devices);
    debug_print("  Total Drivers: %u\n", dev_stats.total_drivers);
    debug_print("  Loaded Drivers: %u\n", dev_stats.loaded_drivers);
    
    pci_stats_t pci_stats;
    pci_get_stats(&pci_stats);
    
    debug_print("\nPCI Bus Statistics:\n");
    debug_print("  Total Devices: %u\n", pci_stats.total_devices);
    debug_print("  Buses Scanned: %u\n", pci_stats.buses_scanned);
    debug_print("  Storage Devices: %u\n", pci_stats.storage_devices);
    debug_print("  Network Devices: %u\n", pci_stats.network_devices);
    
    ide_stats_t ide_stats;
    ide_get_stats(&ide_stats);
    
    debug_print("\nIDE Driver Statistics:\n");
    debug_print("  Controllers Found: %u\n", ide_stats.controllers_found);
    debug_print("  Drives Found: %u\n", ide_stats.drives_found);
    debug_print("  Total Reads: %u\n", ide_stats.total_reads);
    debug_print("  Total Writes: %u\n", ide_stats.total_writes);
}
