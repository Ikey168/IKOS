/* IKOS Device Manager Implementation
 * Issue #15 - Comprehensive Device Driver Framework
 * 
 * Core device management functionality including device registration,
 * driver binding, resource management, and hardware enumeration.
 */

#include "device_manager.h"
#include "memory.h"
#include <string.h>

/* ================================
 * Global State
 * ================================ */

/* Device lists */
static device_t* g_device_list = NULL;
static device_t* g_device_list_tail = NULL;
static uint32_t g_device_count = 0;
static uint32_t g_next_device_id = 1;

/* Driver lists */
static device_driver_t* g_driver_list = NULL;
static uint32_t g_driver_count = 0;
static uint32_t g_next_driver_id = 1;

/* Manager state */
static bool g_device_manager_initialized = false;
static device_manager_stats_t g_stats = {0};

/* ================================
 * Helper Functions
 * ================================ */

static void debug_print(const char* format, ...) {
    // Placeholder for debug printing
    (void)format;
}

static bool driver_supports_device(device_driver_t* driver, device_t* device);

static void update_stats(void) {
    g_stats.total_devices = g_device_count;
    g_stats.total_drivers = g_driver_count;
    
    g_stats.active_devices = 0;
    g_stats.failed_devices = 0;
    g_stats.loaded_drivers = 0;
    
    device_t* device = g_device_list;
    while (device) {
        if (device->state == DEVICE_STATE_ACTIVE) {
            g_stats.active_devices++;
        } else if (device->state == DEVICE_STATE_ERROR) {
            g_stats.failed_devices++;
        }
        device = device->next;
    }
    
    device_driver_t* driver = g_driver_list;
    while (driver) {
        if (driver->loaded) {
            g_stats.loaded_drivers++;
        }
        driver = driver->next;
    }
}

/* ================================
 * Device Manager Core
 * ================================ */

/**
 * Initialize the device manager
 */
int device_manager_init(void) {
    if (g_device_manager_initialized) {
        return DEVICE_SUCCESS;
    }
    
    debug_print("DEVICE: Initializing device manager...\n");
    
    /* Initialize global state */
    g_device_list = NULL;
    g_device_list_tail = NULL;
    g_device_count = 0;
    g_next_device_id = 1;
    
    g_driver_list = NULL;
    g_driver_count = 0;
    g_next_driver_id = 1;
    
    memset(&g_stats, 0, sizeof(g_stats));
    
    g_device_manager_initialized = true;
    
    debug_print("DEVICE: Device manager initialized successfully\n");
    return DEVICE_SUCCESS;
}

/**
 * Shutdown the device manager
 */
void device_manager_shutdown(void) {
    if (!g_device_manager_initialized) {
        return;
    }
    
    debug_print("DEVICE: Shutting down device manager...\n");
    
    /* Detach all drivers and cleanup devices */
    device_t* device = g_device_list;
    while (device) {
        device_t* next = device->next;
        if (device->driver) {
            device_detach_driver(device);
        }
        device_destroy(device);
        device = next;
    }
    
    /* Cleanup drivers */
    device_driver_t* driver = g_driver_list;
    while (driver) {
        device_driver_t* next = driver->next;
        driver_unregister(driver);
        driver = next;
    }
    
    g_device_manager_initialized = false;
    debug_print("DEVICE: Device manager shutdown complete\n");
}

/* ================================
 * Device Management
 * ================================ */

/**
 * Create a new device
 */
device_t* device_create(device_class_t class, device_type_t type, const char* name) {
    if (!g_device_manager_initialized || !name) {
        return NULL;
    }
    
    device_t* device = (device_t*)kmalloc(sizeof(device_t));
    if (!device) {
        return NULL;
    }
    
    /* Initialize device structure */
    memset(device, 0, sizeof(device_t));
    
    device->device_id = g_next_device_id++;
    device->class = class;
    device->type = type;
    device->state = DEVICE_STATE_UNKNOWN;
    
    /* Copy device name */
    strncpy(device->name, name, MAX_DEVICE_NAME_LEN - 1);
    device->name[MAX_DEVICE_NAME_LEN - 1] = '\0';
    
    debug_print("DEVICE: Created device '%s' (ID: %u, Class: 0x%02x, Type: 0x%04x)\n",
                device->name, device->device_id, device->class, device->type);
    
    return device;
}

/**
 * Register a device with the device manager
 */
int device_register(device_t* device) {
    if (!g_device_manager_initialized || !device) {
        return DEVICE_ERROR_INVALID_PARAM;
    }
    
    /* Check if device already registered */
    if (device_find_by_id(device->device_id)) {
        return DEVICE_ERROR_ALREADY_EXISTS;
    }
    
    /* Add to device list */
    device->next = NULL;
    device->prev = g_device_list_tail;
    
    if (g_device_list_tail) {
        g_device_list_tail->next = device;
    } else {
        g_device_list = device;
    }
    g_device_list_tail = device;
    
    g_device_count++;
    device->state = DEVICE_STATE_DETECTED;
    
    debug_print("DEVICE: Registered device '%s' (ID: %u)\n", 
                device->name, device->device_id);
    
    /* Try to find and attach a suitable driver */
    device_driver_t* driver = driver_find_for_device(device);
    if (driver) {
        device_attach_driver(device, driver);
    }
    
    update_stats();
    return DEVICE_SUCCESS;
}

/**
 * Unregister a device
 */
int device_unregister(device_t* device) {
    if (!g_device_manager_initialized || !device) {
        return DEVICE_ERROR_INVALID_PARAM;
    }
    
    /* Detach driver if attached */
    if (device->driver) {
        device_detach_driver(device);
    }
    
    /* Remove from device list */
    if (device->prev) {
        device->prev->next = device->next;
    } else {
        g_device_list = device->next;
    }
    
    if (device->next) {
        device->next->prev = device->prev;
    } else {
        g_device_list_tail = device->prev;
    }
    
    g_device_count--;
    device->state = DEVICE_STATE_REMOVED;
    
    debug_print("DEVICE: Unregistered device '%s' (ID: %u)\n", 
                device->name, device->device_id);
    
    update_stats();
    return DEVICE_SUCCESS;
}

/**
 * Destroy a device structure
 */
void device_destroy(device_t* device) {
    if (!device) {
        return;
    }
    
    /* Free configuration space if allocated */
    if (device->config_space) {
        kfree(device->config_space);
    }
    
    /* Free device structure */
    kfree(device);
}

/* ================================
 * Device Discovery
 * ================================ */

/**
 * Find device by ID
 */
device_t* device_find_by_id(uint32_t device_id) {
    if (!g_device_manager_initialized) {
        return NULL;
    }
    
    device_t* device = g_device_list;
    while (device) {
        if (device->device_id == device_id) {
            return device;
        }
        device = device->next;
    }
    
    return NULL;
}

/**
 * Find device by name
 */
device_t* device_find_by_name(const char* name) {
    if (!g_device_manager_initialized || !name) {
        return NULL;
    }
    
    device_t* device = g_device_list;
    while (device) {
        if (strcmp(device->name, name) == 0) {
            return device;
        }
        device = device->next;
    }
    
    return NULL;
}

/**
 * Find first device by type
 */
device_t* device_find_by_type(device_type_t type) {
    if (!g_device_manager_initialized) {
        return NULL;
    }
    
    device_t* device = g_device_list;
    while (device) {
        if (device->type == type) {
            return device;
        }
        device = device->next;
    }
    
    return NULL;
}

/**
 * Find first device by class
 */
device_t* device_find_by_class(device_class_t class) {
    if (!g_device_manager_initialized) {
        return NULL;
    }
    
    device_t* device = g_device_list;
    while (device) {
        if (device->class == class) {
            return device;
        }
        device = device->next;
    }
    
    return NULL;
}

/* ================================
 * Device Enumeration
 * ================================ */

/**
 * Enumerate all devices
 */
int device_enumerate_all(device_t** devices, uint32_t max_devices) {
    if (!g_device_manager_initialized || !devices || max_devices == 0) {
        return DEVICE_ERROR_INVALID_PARAM;
    }
    
    uint32_t count = 0;
    device_t* device = g_device_list;
    
    while (device && count < max_devices) {
        devices[count++] = device;
        device = device->next;
    }
    
    return count;
}

/**
 * Enumerate devices by class
 */
int device_enumerate_by_class(device_class_t class, device_t** devices, uint32_t max_devices) {
    if (!g_device_manager_initialized || !devices || max_devices == 0) {
        return DEVICE_ERROR_INVALID_PARAM;
    }
    
    uint32_t count = 0;
    device_t* device = g_device_list;
    
    while (device && count < max_devices) {
        if (device->class == class) {
            devices[count++] = device;
        }
        device = device->next;
    }
    
    return count;
}

/**
 * Get total device count
 */
uint32_t device_get_count(void) {
    return g_device_count;
}

/**
 * Get device count by class
 */
uint32_t device_get_count_by_class(device_class_t class) {
    if (!g_device_manager_initialized) {
        return 0;
    }
    
    uint32_t count = 0;
    device_t* device = g_device_list;
    
    while (device) {
        if (device->class == class) {
            count++;
        }
        device = device->next;
    }
    
    return count;
}

/* ================================
 * Driver Management
 * ================================ */

/**
 * Register a device driver
 */
int driver_register(device_driver_t* driver) {
    if (!g_device_manager_initialized || !driver || !driver->name[0]) {
        return DEVICE_ERROR_INVALID_PARAM;
    }
    
    /* Check if driver already registered */
    if (driver_find_by_name(driver->name)) {
        return DEVICE_ERROR_ALREADY_EXISTS;
    }
    
    /* Assign driver ID */
    driver->driver_id = g_next_driver_id++;
    
    /* Add to driver list */
    driver->next = g_driver_list;
    g_driver_list = driver;
    g_driver_count++;
    
    driver->loaded = true;
    driver->device_count = 0;
    
    debug_print("DEVICE: Registered driver '%s' (ID: %u)\n", 
                driver->name, driver->driver_id);
    
    /* Try to attach to compatible devices */
    device_t* device = g_device_list;
    while (device) {
        if (!device->driver && driver_supports_device(driver, device)) {
            device_attach_driver(device, driver);
        }
        device = device->next;
    }
    
    update_stats();
    return DEVICE_SUCCESS;
}

/**
 * Unregister a device driver
 */
int driver_unregister(device_driver_t* driver) {
    if (!g_device_manager_initialized || !driver) {
        return DEVICE_ERROR_INVALID_PARAM;
    }
    
    /* Detach from all devices */
    device_t* device = g_device_list;
    while (device) {
        if (device->driver == driver) {
            device_detach_driver(device);
        }
        device = device->next;
    }
    
    /* Remove from driver list */
    device_driver_t** current = &g_driver_list;
    while (*current) {
        if (*current == driver) {
            *current = driver->next;
            g_driver_count--;
            break;
        }
        current = &(*current)->next;
    }
    
    driver->loaded = false;
    
    debug_print("DEVICE: Unregistered driver '%s' (ID: %u)\n", 
                driver->name, driver->driver_id);
    
    update_stats();
    return DEVICE_SUCCESS;
}

/**
 * Find driver by name
 */
device_driver_t* driver_find_by_name(const char* name) {
    if (!g_device_manager_initialized || !name) {
        return NULL;
    }
    
    device_driver_t* driver = g_driver_list;
    while (driver) {
        if (strcmp(driver->name, name) == 0) {
            return driver;
        }
        driver = driver->next;
    }
    
    return NULL;
}

/**
 * Check if driver supports a specific device
 */
static bool driver_supports_device(device_driver_t* driver, device_t* device) {
    if (!driver || !device) {
        return false;
    }
    
    /* Check device class */
    if (driver->supported_class != DEVICE_CLASS_UNKNOWN && 
        driver->supported_class != device->class) {
        return false;
    }
    
    /* Check vendor ID if specified */
    if (driver->supported_vendors) {
        bool vendor_match = false;
        for (int i = 0; driver->supported_vendors[i] != 0; i++) {
            if (driver->supported_vendors[i] == device->vendor_id) {
                vendor_match = true;
                break;
            }
        }
        if (!vendor_match) {
            return false;
        }
    }
    
    /* Check device ID if specified */
    if (driver->supported_devices) {
        bool device_match = false;
        for (int i = 0; driver->supported_devices[i] != 0; i++) {
            if (driver->supported_devices[i] == device->product_id) {
                device_match = true;
                break;
            }
        }
        if (!device_match) {
            return false;
        }
    }
    
    /* Call driver's probe function if available */
    if (driver->ops && driver->ops->probe) {
        return (driver->ops->probe(device) == DEVICE_SUCCESS);
    }
    
    return true;
}

/**
 * Find suitable driver for device
 */
device_driver_t* driver_find_for_device(device_t* device) {
    if (!g_device_manager_initialized || !device) {
        return NULL;
    }
    
    device_driver_t* driver = g_driver_list;
    while (driver) {
        if (driver->loaded && driver_supports_device(driver, device)) {
            return driver;
        }
        driver = driver->next;
    }
    
    return NULL;
}

/* ================================
 * Device-Driver Association
 * ================================ */

/**
 * Attach driver to device
 */
int device_attach_driver(device_t* device, device_driver_t* driver) {
    if (!g_device_manager_initialized || !device || !driver) {
        return DEVICE_ERROR_INVALID_PARAM;
    }
    
    if (device->driver) {
        return DEVICE_ERROR_BUSY;
    }
    
    /* Call driver's attach function */
    if (driver->ops && driver->ops->attach) {
        int result = driver->ops->attach(device);
        if (result != DEVICE_SUCCESS) {
            return result;
        }
    }
    
    /* Associate device and driver */
    device->driver = driver;
    driver->device_count++;
    device->state = DEVICE_STATE_READY;
    
    debug_print("DEVICE: Attached driver '%s' to device '%s'\n", 
                driver->name, device->name);
    
    return DEVICE_SUCCESS;
}

/**
 * Detach driver from device
 */
int device_detach_driver(device_t* device) {
    if (!g_device_manager_initialized || !device || !device->driver) {
        return DEVICE_ERROR_INVALID_PARAM;
    }
    
    device_driver_t* driver = device->driver;
    
    /* Call driver's detach function */
    if (driver->ops && driver->ops->detach) {
        driver->ops->detach(device);
    }
    
    /* Disassociate device and driver */
    device->driver = NULL;
    device->driver_data = NULL;
    driver->device_count--;
    device->state = DEVICE_STATE_DETECTED;
    
    debug_print("DEVICE: Detached driver '%s' from device '%s'\n", 
                driver->name, device->name);
    
    return DEVICE_SUCCESS;
}

/* ================================
 * Statistics and Information
 * ================================ */

/**
 * Get device manager statistics
 */
void device_manager_get_stats(device_manager_stats_t* stats) {
    if (!stats) {
        return;
    }
    
    update_stats();
    memcpy(stats, &g_stats, sizeof(device_manager_stats_t));
}

/**
 * Print device information
 */
void device_print_info(device_t* device) {
    if (!device) {
        return;
    }
    
    debug_print("Device: %s (ID: %u)\n", device->name, device->device_id);
    debug_print("  Class: 0x%02x, Type: 0x%04x\n", device->class, device->type);
    debug_print("  Vendor: 0x%04x, Product: 0x%04x\n", device->vendor_id, device->product_id);
    debug_print("  State: %d, Flags: 0x%08x\n", device->state, device->flags);
    
    if (device->driver) {
        debug_print("  Driver: %s\n", device->driver->name);
    } else {
        debug_print("  Driver: None\n");
    }
    
    debug_print("  Resources: %d\n", device->resource_count);
    for (int i = 0; i < device->resource_count; i++) {
        debug_print("    [%d] Type: 0x%02x, Base: 0x%016llx, Size: 0x%016llx\n",
                    i, device->resources[i].type, 
                    device->resources[i].base_address, device->resources[i].size);
    }
}

/**
 * Print all registered devices
 */
void device_print_all_devices(void) {
    if (!g_device_manager_initialized) {
        debug_print("Device manager not initialized\n");
        return;
    }
    
    debug_print("=== Registered Devices ===\n");
    debug_print("Total devices: %u\n", g_device_count);
    debug_print("Total drivers: %u\n", g_driver_count);
    
    device_t* device = g_device_list;
    while (device) {
        device_print_info(device);
        debug_print("\n");
        device = device->next;
    }
}
