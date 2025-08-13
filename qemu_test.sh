#!/bin/bash
# IKOS QEMU Bootloader Test Script
# Boots and debugs the bootloader in QEMU

IMG="build/ikos_longmode.img"
DEBUG_IMG="build/ikos_longmode_debug.img"

if [ ! -f "$IMG" ]; then
    echo "Error: $IMG not found. Run 'make' first."
    exit 1
fi

if [ ! -f "$DEBUG_IMG" ]; then
    echo "Warning: $DEBUG_IMG not found. Debugging will be unavailable."
fi

# Standard QEMU boot
echo "=== QEMU: Standard Boot Test ==="
qemu-system-x86_64 -fda "$IMG" -boot a -nographic -serial stdio

# Debug-enabled QEMU boot
if [ -f "$DEBUG_IMG" ]; then
    echo "\n=== QEMU: Debug Boot Test ==="
    qemu-system-x86_64 -fda "$DEBUG_IMG" -boot a -nographic -chardev stdio,id=char0 -serial chardev:char0
fi

# GDB debugging session
if [ -f "$DEBUG_IMG" ]; then
    echo "\n=== QEMU: GDB Debugging Session ==="
    echo "Run the following in another terminal:"
    echo "  gdb -ex 'target remote localhost:1234'"
    qemu-system-x86_64 -fda "$DEBUG_IMG" -boot a -S -s -nographic -serial file:qemu_gdb_serial.log
fi
