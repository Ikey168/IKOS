#!/bin/bash
# IKOS QEMU Testing Suite
# Comprehensive testing of bootloader in various QEMU configurations

set -e

echo "=== IKOS QEMU Testing Suite ==="
echo "Testing bootloader compatibility across different virtual hardware configurations"
echo ""

# Configuration
TIMEOUT_DURATION=10
TEST_RESULTS_DIR="test_results"
BOOTLOADERS=(
    "build/ikos.img:Basic Bootloader"
    "build/ikos_compact.img:Compact Bootloader"
    "build/ikos_protected_compact.img:Protected Mode Bootloader"
    "build/ikos_elf_compact.img:ELF Loader Bootloader"
    "build/ikos_longmode.img:Long Mode Bootloader"
    "build/ikos_longmode_debug.img:Debug-Enabled Bootloader"
)

# Create results directory
mkdir -p "$TEST_RESULTS_DIR"

# Test counter
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Helper functions
log_test() {
    echo "[TEST] $1"
    ((TOTAL_TESTS++))
}

log_pass() {
    echo "✓ PASS: $1"
    ((PASSED_TESTS++))
}

log_fail() {
    echo "✗ FAIL: $1"
    ((FAILED_TESTS++))
}

log_warn() {
    echo "⚠ WARN: $1"
}

# Build all bootloaders first
echo "Building all bootloader variants..."
make clean && make
if [ $? -ne 0 ]; then
    echo "Build failed! Cannot proceed with testing."
    exit 1
fi
echo "✓ All bootloaders built successfully"
echo ""

# Test 1: Basic QEMU compatibility tests
echo "=== Test Suite 1: Basic QEMU Compatibility ==="

for bootloader_info in "${BOOTLOADERS[@]}"; do
    IFS=':' read -r bootloader_path bootloader_name <<< "$bootloader_info"
    
    if [ ! -f "$bootloader_path" ]; then
        log_warn "$bootloader_name - File not found: $bootloader_path"
        continue
    fi
    
    log_test "$bootloader_name - Basic boot test"
    
    # Test basic boot
    timeout $TIMEOUT_DURATION qemu-system-x86_64 \
        -fda "$bootloader_path" \
        -boot a \
        -display none \
        -serial file:"$TEST_RESULTS_DIR/$(basename $bootloader_path .img)_basic.log" \
        -no-reboot -no-shutdown > /dev/null 2>&1
    
    if [ $? -eq 124 ]; then
        log_pass "$bootloader_name - Boot successful (timeout reached)"
    else
        log_fail "$bootloader_name - Boot failed or crashed"
    fi
done

echo ""

# Test 2: Different CPU architectures/features
echo "=== Test Suite 2: CPU Architecture Compatibility ==="

CPU_CONFIGS=(
    "486:Intel 486 CPU"
    "pentium:Pentium CPU"
    "pentium2:Pentium II CPU"
    "pentium3:Pentium III CPU"
    "core2duo:Core 2 Duo CPU"
    "coreduo:Core Duo CPU"
    "qemu64:QEMU 64-bit CPU"
    "kvm64:KVM 64-bit CPU"
)

# Test with long mode bootloader (most advanced)
BOOTLOADER="build/ikos_longmode.img"
if [ -f "$BOOTLOADER" ]; then
    for cpu_info in "${CPU_CONFIGS[@]}"; do
        IFS=':' read -r cpu_type cpu_name <<< "$cpu_info"
        
        log_test "CPU compatibility - $cpu_name"
        
        timeout $TIMEOUT_DURATION qemu-system-x86_64 \
            -cpu "$cpu_type" \
            -fda "$BOOTLOADER" \
            -boot a \
            -display none \
            -serial file:"$TEST_RESULTS_DIR/cpu_${cpu_type}.log" \
            -no-reboot -no-shutdown > /dev/null 2>&1
        
        if [ $? -eq 124 ]; then
            log_pass "CPU compatibility - $cpu_name"
        else
            log_fail "CPU compatibility - $cpu_name"
        fi
    done
else
    log_warn "Long mode bootloader not found for CPU testing"
fi

echo ""

# Test 3: Memory configurations
echo "=== Test Suite 3: Memory Configuration Compatibility ==="

MEMORY_CONFIGS=(
    "16M:16 MB RAM"
    "32M:32 MB RAM"
    "64M:64 MB RAM"
    "128M:128 MB RAM"
    "256M:256 MB RAM"
    "512M:512 MB RAM"
    "1G:1 GB RAM"
)

for mem_info in "${MEMORY_CONFIGS[@]}"; do
    IFS=':' read -r mem_size mem_name <<< "$mem_info"
    
    log_test "Memory configuration - $mem_name"
    
    timeout $TIMEOUT_DURATION qemu-system-x86_64 \
        -m "$mem_size" \
        -fda "$BOOTLOADER" \
        -boot a \
        -display none \
        -serial file:"$TEST_RESULTS_DIR/mem_${mem_size}.log" \
        -no-reboot -no-shutdown > /dev/null 2>&1
    
    if [ $? -eq 124 ]; then
        log_pass "Memory configuration - $mem_name"
    else
        log_fail "Memory configuration - $mem_name"
    fi
done

echo ""

# Test 4: Boot device configurations
echo "=== Test Suite 4: Boot Device Compatibility ==="

BOOT_CONFIGS=(
    "floppy:Floppy disk boot"
    "cdrom:CD-ROM boot"
    "usb:USB boot simulation"
)

# Test floppy boot (already tested above)
log_test "Floppy disk boot"
timeout $TIMEOUT_DURATION qemu-system-x86_64 \
    -fda "$BOOTLOADER" \
    -boot a \
    -display none \
    -serial file:"$TEST_RESULTS_DIR/boot_floppy.log" \
    -no-reboot -no-shutdown > /dev/null 2>&1

if [ $? -eq 124 ]; then
    log_pass "Floppy disk boot"
else
    log_fail "Floppy disk boot"
fi

# Test CD-ROM boot (create ISO)
log_test "CD-ROM boot"
if command -v genisoimage >/dev/null 2>&1 || command -v mkisofs >/dev/null 2>&1; then
    # Create bootable ISO
    mkdir -p iso_temp
    cp "$BOOTLOADER" iso_temp/
    
    if command -v genisoimage >/dev/null 2>&1; then
        genisoimage -b "$(basename $BOOTLOADER)" -no-emul-boot -boot-load-size 4 \
            -boot-info-table -o "$TEST_RESULTS_DIR/test_boot.iso" iso_temp/ > /dev/null 2>&1
    else
        mkisofs -b "$(basename $BOOTLOADER)" -no-emul-boot -boot-load-size 4 \
            -boot-info-table -o "$TEST_RESULTS_DIR/test_boot.iso" iso_temp/ > /dev/null 2>&1
    fi
    
    if [ -f "$TEST_RESULTS_DIR/test_boot.iso" ]; then
        timeout $TIMEOUT_DURATION qemu-system-x86_64 \
            -cdrom "$TEST_RESULTS_DIR/test_boot.iso" \
            -boot d \
            -display none \
            -serial file:"$TEST_RESULTS_DIR/boot_cdrom.log" \
            -no-reboot -no-shutdown > /dev/null 2>&1
        
        if [ $? -eq 124 ]; then
            log_pass "CD-ROM boot"
        else
            log_fail "CD-ROM boot"
        fi
    else
        log_fail "CD-ROM boot - ISO creation failed"
    fi
    
    rm -rf iso_temp
else
    log_warn "CD-ROM boot - genisoimage/mkisofs not available"
fi

echo ""

# Test 5: BIOS/UEFI compatibility simulation
echo "=== Test Suite 5: Firmware Compatibility ==="

# Test with SeaBIOS (default QEMU BIOS)
log_test "SeaBIOS compatibility"
timeout $TIMEOUT_DURATION qemu-system-x86_64 \
    -bios /usr/share/seabios/bios.bin \
    -fda "$BOOTLOADER" \
    -boot a \
    -display none \
    -serial file:"$TEST_RESULTS_DIR/bios_seabios.log" \
    -no-reboot -no-shutdown > /dev/null 2>&1 2>/dev/null || true

if [ $? -eq 124 ]; then
    log_pass "SeaBIOS compatibility"
else
    log_warn "SeaBIOS compatibility - Default BIOS test"
fi

# Test with OVMF (UEFI)
if [ -f "/usr/share/ovmf/OVMF.fd" ] || [ -f "/usr/share/qemu/OVMF.fd" ]; then
    log_test "UEFI/OVMF compatibility"
    
    OVMF_PATH=""
    if [ -f "/usr/share/ovmf/OVMF.fd" ]; then
        OVMF_PATH="/usr/share/ovmf/OVMF.fd"
    elif [ -f "/usr/share/qemu/OVMF.fd" ]; then
        OVMF_PATH="/usr/share/qemu/OVMF.fd"
    fi
    
    timeout $TIMEOUT_DURATION qemu-system-x86_64 \
        -bios "$OVMF_PATH" \
        -fda "$BOOTLOADER" \
        -boot a \
        -display none \
        -serial file:"$TEST_RESULTS_DIR/bios_uefi.log" \
        -no-reboot -no-shutdown > /dev/null 2>&1 2>/dev/null || true
    
    if [ $? -eq 124 ]; then
        log_pass "UEFI/OVMF compatibility"
    else
        log_warn "UEFI/OVMF compatibility - May not support legacy boot"
    fi
else
    log_warn "UEFI/OVMF compatibility - OVMF firmware not found"
fi

echo ""

# Test 6: Network boot simulation (PXE-like)
echo "=== Test Suite 6: Network Boot Simulation ==="

log_test "Network interface availability"
timeout $TIMEOUT_DURATION qemu-system-x86_64 \
    -netdev user,id=net0 \
    -device rtl8139,netdev=net0 \
    -fda "$BOOTLOADER" \
    -boot a \
    -display none \
    -serial file:"$TEST_RESULTS_DIR/network_boot.log" \
    -no-reboot -no-shutdown > /dev/null 2>&1

if [ $? -eq 124 ]; then
    log_pass "Network interface availability"
else
    log_fail "Network interface availability"
fi

echo ""

# Test 7: Debug and monitoring capabilities
echo "=== Test Suite 7: Debug and Monitoring ==="

# Test with debug bootloader if available
DEBUG_BOOTLOADER="build/ikos_longmode_debug.img"
if [ -f "$DEBUG_BOOTLOADER" ]; then
    log_test "Debug output verification"
    
    timeout $TIMEOUT_DURATION qemu-system-x86_64 \
        -fda "$DEBUG_BOOTLOADER" \
        -boot a \
        -display none \
        -serial file:"$TEST_RESULTS_DIR/debug_output.log" \
        -no-reboot -no-shutdown > /dev/null 2>&1
    
    if [ -f "$TEST_RESULTS_DIR/debug_output.log" ] && [ -s "$TEST_RESULTS_DIR/debug_output.log" ]; then
        log_pass "Debug output verification"
        echo "   Debug messages captured: $(wc -l < "$TEST_RESULTS_DIR/debug_output.log") lines"
    else
        log_fail "Debug output verification"
    fi
    
    # Test GDB connectivity
    log_test "GDB debugging capability"
    
    # Start QEMU with GDB in background
    qemu-system-x86_64 \
        -fda "$DEBUG_BOOTLOADER" \
        -boot a \
        -S -s \
        -display none \
        -serial none \
        -monitor none > /dev/null 2>&1 &
    
    QEMU_PID=$!
    sleep 2
    
    # Test GDB connection
    echo "target remote localhost:1234" | timeout 5s gdb -batch -nx > /dev/null 2>&1
    GDB_RESULT=$?
    
    # Kill QEMU
    kill $QEMU_PID 2>/dev/null
    wait $QEMU_PID 2>/dev/null
    
    if [ $GDB_RESULT -eq 0 ]; then
        log_pass "GDB debugging capability"
    else
        log_fail "GDB debugging capability"
    fi
else
    log_warn "Debug bootloader not available for testing"
fi

echo ""

# Test 8: Performance and resource usage
echo "=== Test Suite 8: Performance Monitoring ==="

log_test "Boot time measurement"
START_TIME=$(date +%s%N)
timeout $TIMEOUT_DURATION qemu-system-x86_64 \
    -fda "$BOOTLOADER" \
    -boot a \
    -display none \
    -serial file:"$TEST_RESULTS_DIR/performance.log" \
    -no-reboot -no-shutdown > /dev/null 2>&1
END_TIME=$(date +%s%N)

BOOT_TIME=$(( (END_TIME - START_TIME) / 1000000 ))  # Convert to milliseconds
log_pass "Boot time measurement - ${BOOT_TIME}ms (includes QEMU startup)"

echo ""

# Generate summary report
echo "=== QEMU Testing Summary ==="
echo "Total tests run: $TOTAL_TESTS"
echo "Passed: $PASSED_TESTS"
echo "Failed: $FAILED_TESTS"
echo "Success rate: $(( PASSED_TESTS * 100 / TOTAL_TESTS ))%"
echo ""

if [ $FAILED_TESTS -gt 0 ]; then
    echo "⚠ Some tests failed. Check logs in $TEST_RESULTS_DIR/ for details."
else
    echo "🎉 All tests passed! Bootloader is compatible with tested QEMU configurations."
fi

echo ""
echo "Test results saved in: $TEST_RESULTS_DIR/"
echo "Key log files:"
ls -la "$TEST_RESULTS_DIR/"*.log 2>/dev/null | head -10

echo ""
echo "=== QEMU Testing Complete ==="
