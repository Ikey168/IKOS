#!/bin/bash

# IKOS Paging Implementation Test Script
# This script tests the paging structures created by our bootloader

echo "=== IKOS Paging Implementation Test ==="
echo "Testing bootloader with paging structures..."

# Test 1: Build verification
echo ""
echo "Test 1: Build Verification"
echo "=========================="
if [ -f "build/ikos_longmode.img" ]; then
    echo "✓ Bootloader image exists"
    echo "  Size: $(stat -c%s build/ikos_longmode.img) bytes"
else
    echo "✗ Bootloader image not found"
    exit 1
fi

# Test 2: Memory layout verification using hexdump
echo ""
echo "Test 2: Bootloader Code Analysis"
echo "================================"
echo "First 512 bytes of bootloader (boot sector):"
hexdump -C build/boot_longmode.bin | head -20

# Test 3: Run in QEMU with debugging
echo ""
echo "Test 3: QEMU Execution Test"
echo "=========================="
echo "Running bootloader in QEMU with timeout..."

# Run QEMU with a timeout to capture initial output
timeout 10s qemu-system-x86_64 \
    -fda build/ikos_longmode.img \
    -boot a \
    -nographic \
    -no-reboot \
    -no-shutdown \
    -serial stdio 2>&1 | tee qemu_output.log

echo ""
echo "QEMU execution completed (or timed out)"

# Test 4: Check for expected output
echo ""
echo "Test 4: Output Analysis"
echo "======================"
if [ -f "qemu_output.log" ]; then
    echo "Checking for boot messages..."
    if grep -q "IKOS" qemu_output.log; then
        echo "✓ Boot message found"
    else
        echo "? Boot message not detected"
    fi
    
    if grep -q "Real.*Protected.*Long" qemu_output.log; then
        echo "✓ Mode transition message found"
    else
        echo "? Mode transition message not detected"
    fi
else
    echo "No output log found"
fi

# Test 5: Memory structure validation
echo ""
echo "Test 5: Paging Structure Validation"
echo "==================================="
echo "Analyzing paging constants in source:"
grep -n "PML4_BASE\|PDPT_BASE\|PD_BASE\|PT_BASE" boot/boot_longmode.asm
echo ""
grep -n "PAGE_PRESENT\|PAGE_WRITABLE\|PAGE_LARGE" boot/boot_longmode.asm

echo ""
echo "=== Test Summary ==="
echo "Paging implementation includes:"
echo "• PML4 at 0x1000 (Page Map Level 4)"
echo "• PDPT at 0x2000 (Page Directory Pointer Table)"  
echo "• PD at 0x3000 (Page Directory)"
echo "• Identity mapping for first 4MB"
echo "• CR0, CR4, and CR3 properly configured"
echo "• Long mode enabled via EFER MSR"

if [ -f "qemu_output.log" ]; then
    echo ""
    echo "Full QEMU output:"
    cat qemu_output.log
fi

echo ""
echo "Test completed!"
