/* IKOS Device Driver Framework Test Header
 * Issue #15 - Device Driver Framework Test Suite
 */

#ifndef DEVICE_DRIVER_TEST_H
#define DEVICE_DRIVER_TEST_H

#include <stdint.h>

/**
 * Run comprehensive device driver framework tests
 * 
 * This function tests all components of the device driver framework:
 * - Device Manager (device creation, registration, enumeration)
 * - PCI Bus Driver (device detection, enumeration)
 * - IDE Driver (controller initialization, drive detection)
 * - Integration (driver binding, resource management)
 */
void test_device_driver_framework(void);

#endif /* DEVICE_DRIVER_TEST_H */
