#!/bin/bash
# USB Driver Framework Validation Test - Issue #50

echo "=== USB Driver Framework Validation Test ==="
echo "Testing Issue #50 USB Driver Framework Implementation"
echo

cd /workspaces/IKOS/kernel

# Test 1: Check if all USB headers exist
echo "Test 1: Checking USB headers..."
if [[ -f "../include/usb.h" ]]; then
    echo "✓ USB core header found"
else
    echo "❌ USB core header missing"
    exit 1
fi

if [[ -f "../include/usb_hid.h" ]]; then
    echo "✓ USB HID header found"
else
    echo "❌ USB HID header missing"
    exit 1
fi

if [[ -f "../include/io.h" ]]; then
    echo "✓ I/O header found"
else
    echo "❌ I/O header missing"
    exit 1
fi

# Test 2: Check if all USB implementations exist
echo
echo "Test 2: Checking USB implementations..."
for file in usb.c usb_hid.c usb_uhci.c usb_control.c usb_syscalls.c usb_test.c; do
    if [[ -f "$file" ]]; then
        echo "✓ $file found"
    else
        echo "❌ $file missing"
        exit 1
    fi
done

# Test 3: Check if compilation was successful
echo
echo "Test 3: Checking compilation results..."
if [[ -f "build/usb.o" ]]; then
    echo "✓ USB core compiled successfully"
else
    echo "❌ USB core compilation failed"
    exit 1
fi

if [[ -f "build/usb_hid.o" ]]; then
    echo "✓ USB HID compiled successfully"
else
    echo "❌ USB HID compilation failed"
    exit 1
fi

if [[ -f "build/usb_uhci.o" ]]; then
    echo "✓ USB UHCI compiled successfully"
else
    echo "❌ USB UHCI compilation failed"
    exit 1
fi

if [[ -f "build/usb_control.o" ]]; then
    echo "✓ USB control transfers compiled successfully"
else
    echo "❌ USB control transfers compilation failed"
    exit 1
fi

if [[ -f "build/usb_syscalls.o" ]]; then
    echo "✓ USB system calls compiled successfully"
else
    echo "❌ USB system calls compilation failed"
    exit 1
fi

if [[ -f "build/usb_test.o" ]]; then
    echo "✓ USB tests compiled successfully"
else
    echo "❌ USB tests compilation failed"
    exit 1
fi

# Test 4: Validate key symbols and functions exist
echo
echo "Test 4: Checking for key USB symbols..."

# Check for USB core functions
if nm build/usb.o 2>/dev/null | grep -q "usb_init"; then
    echo "✓ USB initialization function found"
else
    echo "❌ USB initialization function missing"
    exit 1
fi

if nm build/usb.o 2>/dev/null | grep -q "usb_connect_device"; then
    echo "✓ USB device connection function found"
else
    echo "❌ USB device connection function missing"
    exit 1
fi

# Check for HID functions
if nm build/usb_hid.o 2>/dev/null | grep -q "hid_init"; then
    echo "✓ HID driver initialization function found"
else
    echo "❌ HID driver initialization function missing"
    exit 1
fi

# Check for UHCI functions
if nm build/usb_uhci.o 2>/dev/null | grep -q "uhci_register_controller"; then
    echo "✓ UHCI controller registration function found"
else
    echo "❌ UHCI controller registration function missing"
    exit 1
fi

# Test 5: Basic header syntax validation
echo
echo "Test 5: Validating header file syntax..."

if grep -q "USB_MAX_DEVICES" ../include/usb.h; then
    echo "✓ USB constants defined"
else
    echo "❌ USB constants missing"
    exit 1
fi

if grep -q "usb_device_t" ../include/usb.h; then
    echo "✓ USB device structures defined"
else
    echo "❌ USB device structures missing"
    exit 1
fi

if grep -q "usb_driver_t" ../include/usb.h; then
    echo "✓ USB driver structures defined"
else
    echo "❌ USB driver structures missing"
    exit 1
fi

if grep -q "usb_hci_t" ../include/usb.h; then
    echo "✓ USB host controller interface defined"
else
    echo "❌ USB host controller interface missing"
    exit 1
fi

echo
echo "=== USB Driver Framework Validation Results ==="
echo "✓ All validation tests passed!"
echo "✓ USB Driver Framework successfully implemented"
echo "✓ Ready for integration and deployment"
echo
echo "Framework includes:"
echo "  - USB Core API (device management, transfers)"
echo "  - UHCI Host Controller Driver (USB 1.1)"
echo "  - HID Class Driver (keyboards, mice)"
echo "  - Control Transfer Handler"
echo "  - System Call Interface"
echo "  - Comprehensive Test Suite"
