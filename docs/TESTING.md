# IKOS Bootloader Testing Suite

This document provides comprehensive information about testing the IKOS bootloader on both QEMU (emulation) and real hardware platforms.

## Overview

The IKOS testing suite provides three main testing environments:
1. **QEMU Testing** - Virtual machine testing with various configurations
2. **Real Hardware Testing** - Tools for testing on actual x86 hardware
3. **BIOS/UEFI Compatibility Testing** - Firmware compatibility validation

## Quick Start

### Prerequisites

Install required packages:
```bash
# Install QEMU and OVMF (UEFI firmware)
sudo apt update
sudo apt install nasm qemu-system-x86 ovmf

# Or use the make target
make install-deps
```

### Build and Test

```bash
# Build the bootloader
make

# Run comprehensive QEMU tests
make test-qemu

# Set up real hardware testing
make setup-real-hardware

# Set up compatibility testing
make setup-compat-tests

# Run all automated tests
make test-all
```

## Testing Components

### 1. QEMU Testing Suite (`test_qemu.sh`)

Comprehensive virtual machine testing with 8 test suites:

- **CPU Compatibility Tests**: x86, x86_64, different CPU models
- **Memory Configuration Tests**: Various RAM sizes and configurations  
- **Boot Device Tests**: Floppy, hard disk, CD-ROM, USB simulation
- **BIOS/UEFI Firmware Tests**: Legacy BIOS and UEFI with CSM
- **Network Simulation**: PXE boot testing
- **Debug and Monitoring**: Serial output, GDB integration
- **Performance Testing**: Boot time measurements
- **Stress Testing**: Resource limits and edge cases

#### Usage:
```bash
# Run all QEMU tests
./test_qemu.sh

# Or use make target
make test-qemu
```

#### Example Output:
```
=== IKOS QEMU Testing Suite ===
Running comprehensive bootloader tests

✓ CPU Compatibility: x86_64 basic test
✓ Memory Configuration: 128MB test
✓ Boot Device: Floppy disk test
✓ BIOS Firmware: SeaBIOS test
...
Results saved to: qemu_test_results/test_20240101_120000.log
```

### 2. Real Hardware Testing (`test_real_hardware.sh`)

Tools and utilities for testing on actual x86 hardware:

- **USB Boot Creation**: Automated USB bootable device creation
- **PXE Boot Setup**: Network boot configuration files
- **Hardware Compatibility Report**: Structured testing checklist
- **Hardware Diagnostics**: System information gathering

#### Usage:
```bash
# Interactive setup
./test_real_hardware.sh

# Create USB bootable device
sudo ./test_real_hardware.sh
# Select option 1, follow prompts

# Or use make target
make create-usb
```

#### Generated Files:
- `real_hardware_test/prepare_usb.sh` - USB creation script
- `real_hardware_test/pxe/` - PXE boot files
- `real_hardware_test/hardware_test_report.md` - Test report template
- `real_hardware_test/diagnose_hardware.sh` - Hardware diagnostics

### 3. BIOS/UEFI Compatibility Testing (`create_bios_uefi_compat.sh`)

Firmware compatibility validation suite:

- **Legacy BIOS Tests**: SeaBIOS, interrupt handlers, memory detection
- **UEFI Tests**: CSM support, legacy boot, removable media
- **Automated Test Suite**: Comprehensive compatibility validation
- **Compatibility Matrix**: Results tracking and documentation

#### Usage:
```bash
# Set up compatibility tests
./create_bios_uefi_compat.sh

# Run automated compatibility tests
make test-compat

# Or manually run specific tests
cd bios_uefi_compat/bios_tests
./test_legacy_bios.sh

cd ../uefi_tests  
./test_uefi.sh
```

## Detailed Testing Procedures

### QEMU Virtual Machine Testing

#### Basic Testing:
```bash
# Test with default configuration
make test-longmode

# Test with debug output
make debug-serial

# Test with GDB debugging
make debug-gdb
# In another terminal: gdb -ex 'target remote localhost:1234'
```

#### Advanced QEMU Testing:
```bash
# Test specific memory sizes
qemu-system-x86_64 -m 32M -drive format=raw,file=build/ikos_longmode.img
qemu-system-x86_64 -m 512M -drive format=raw,file=build/ikos_longmode.img

# Test different CPU models
qemu-system-x86_64 -cpu qemu64 -drive format=raw,file=build/ikos_longmode.img
qemu-system-x86_64 -cpu Nehalem -drive format=raw,file=build/ikos_longmode.img

# Test UEFI firmware
qemu-system-x86_64 \
    -drive if=pflash,format=raw,readonly,file=/usr/share/ovmf/OVMF.fd \
    -drive if=pflash,format=raw,file=/usr/share/ovmf/OVMF_VARS.fd \
    -drive format=raw,file=build/ikos_longmode.img
```

### Real Hardware Testing

#### USB Boot Testing:

1. **Prepare USB Device:**
   ```bash
   # Check available devices
   lsblk
   
   # Create bootable USB (replace /dev/sdX with your USB device)
   sudo ./real_hardware_test/prepare_usb.sh
   ```

2. **Boot from USB:**
   - Insert USB into target machine
   - Enter BIOS/UEFI setup (usually F2, F12, or DEL)
   - Enable "Boot from USB" or "Boot from Removable Media"
   - Set USB device as first boot priority
   - Save and restart

3. **Document Results:**
   - Fill out `real_hardware_test/hardware_test_report.md`
   - Note any compatibility issues
   - Record performance observations

#### PXE Network Boot Testing:

1. **Set up TFTP Server:**
   ```bash
   # Install TFTP server
   sudo apt install tftpd-hpa
   
   # Copy PXE files
   sudo cp real_hardware_test/pxe/* /var/lib/tftpboot/
   ```

2. **Configure DHCP Server:**
   - See `real_hardware_test/pxe/README_PXE.md` for configuration examples

3. **Test Network Boot:**
   - Enable PXE boot in target machine BIOS/UEFI
   - Boot from network device

### BIOS/UEFI Compatibility Testing

#### Legacy BIOS Testing:
```bash
# Run all BIOS tests
cd bios_uefi_compat/bios_tests
./test_legacy_bios.sh

# Check results
cat bios_test_results.log
```

#### UEFI Testing:
```bash
# Install OVMF if not already installed
sudo apt install ovmf

# Run UEFI tests
cd bios_uefi_compat/uefi_tests
./test_uefi.sh

# Check results
cat uefi_test_results.log
```

#### Automated Compatibility Testing:
```bash
# Run comprehensive automated tests
cd bios_uefi_compat
./automated_compat_test.sh

# Check results
cat test_results/compat_test_*.log
```

## Debug and Troubleshooting

### Debug Output

The bootloader supports multiple debug output methods:

1. **Serial Output (COM1):**
   ```bash
   # Test with serial debugging
   qemu-system-x86_64 \
       -drive format=raw,file=build/ikos_longmode_debug.img \
       -serial stdio
   ```

2. **VGA Framebuffer Output:**
   - Debug messages appear on screen with color coding
   - Green: Successful operations
   - Yellow: Warnings
   - Red: Errors

3. **GDB Debugging:**
   ```bash
   # Start QEMU with GDB server
   make debug-gdb
   
   # In another terminal, connect with GDB
   gdb
   (gdb) target remote localhost:1234
   (gdb) break *0x7c00  # Break at boot sector start
   (gdb) continue
   ```

### Common Issues and Solutions

#### Issue: Bootloader doesn't start
**Symptoms:** System hangs at boot, no output visible
**Solutions:**
- Verify BIOS boot order (USB/floppy first)
- Check if Secure Boot is disabled (UEFI systems)
- Ensure USB device is properly created
- Test with different USB ports/devices

#### Issue: A20 line not enabled
**Symptoms:** Memory access fails, system crashes in protected mode
**Solutions:**
- Try different A20 enabling methods
- Test on different hardware
- Check BIOS settings for A20 gate

#### Issue: UEFI system won't boot
**Symptoms:** UEFI firmware doesn't recognize bootloader
**Solutions:**
- Enable CSM (Compatibility Support Module)
- Enable legacy boot support
- Disable Secure Boot
- Try different boot device types

#### Issue: No debug output
**Symptoms:** Expected serial/screen output not visible
**Solutions:**
- Verify serial cable connection (COM1)
- Check terminal settings (38400 baud, 8N1)
- Build debug-enabled version: `make debug-builds`
- Test debug functions individually

### Hardware Compatibility

#### Verified Compatible Hardware:
- Most x86/x86_64 desktop systems
- Legacy BIOS systems
- UEFI systems with CSM enabled
- Systems with standard VGA compatibility

#### Known Limitations:
- UEFI-only systems (no CSM) require modifications
- Very old systems (pre-Pentium) may need adjustments
- Some embedded systems may have non-standard BIOS
- Secure Boot enabled systems (by design)

#### Hardware Requirements:
- x86 or x86_64 processor
- Minimum 32MB RAM (recommended 128MB+)
- VGA-compatible display adapter
- Standard PC BIOS or UEFI with CSM
- USB port or floppy drive for boot media

## Test Results and Reporting

### Test Result Locations:
- **QEMU Tests:** `qemu_test_results/`
- **Compatibility Tests:** `bios_uefi_compat/test_results/`
- **Hardware Reports:** `real_hardware_test/hardware_test_report.md`

### Interpreting Results:

#### QEMU Test Results:
```
✓ PASS: Test passed successfully
✗ FAIL: Test failed - check logs for details
⚠ WARN: Test completed with warnings
ℹ INFO: Informational message
```

#### Compatibility Test Results:
- **Pass Rate:** Percentage of successful tests
- **Boot Time:** Time from start to bootloader execution
- **Error Count:** Number of failed operations

#### Hardware Test Results:
- Fill out structured report template
- Document specific hardware details
- Note any compatibility issues
- Record performance measurements

### Performance Benchmarks:

#### Expected Performance:
- **QEMU Boot Time:** 2-5 seconds
- **Real Hardware Boot Time:** 5-15 seconds
- **Memory Usage:** <1MB for bootloader
- **Storage Requirements:** 1.44MB floppy image

## Continuous Integration

For automated testing in CI/CD pipelines:

```bash
# Install dependencies
make install-deps

# Build all variants
make

# Run automated tests (no interactive prompts)
make test-qemu
make test-compat

# Check exit codes
echo "Exit code: $?"
```

## Contributing Test Results

When testing on new hardware:

1. Run hardware diagnostics:
   ```bash
   make check-hardware
   ```

2. Fill out compatibility report:
   ```bash
   # Edit the generated report
   nano real_hardware_test/hardware_test_report.md
   ```

3. Submit test results:
   - Include hardware specifications
   - Note any compatibility issues
   - Document workarounds if found
   - Attach relevant log files

## Advanced Testing

### Custom Test Configurations:

Create custom test scripts for specific scenarios:
```bash
#!/bin/bash
# Custom test for specific hardware model

qemu-system-x86_64 \
    -cpu host \
    -m 1024M \
    -drive format=raw,file=build/ikos_longmode.img \
    -netdev user,id=net0 \
    -device rtl8139,netdev=net0 \
    -boot menu=on \
    -monitor stdio
```

### Performance Profiling:

Monitor bootloader performance:
```bash
# Time the boot process
time make test-longmode

# Profile memory usage
qemu-system-x86_64 \
    -drive format=raw,file=build/ikos_longmode.img \
    -monitor stdio \
    -d trace:memory
```

### Stress Testing:

Test under resource constraints:
```bash
# Minimal memory
qemu-system-x86_64 -m 16M -drive format=raw,file=build/ikos_longmode.img

# Maximum memory
qemu-system-x86_64 -m 4096M -drive format=raw,file=build/ikos_longmode.img

# Multiple virtual CPUs
qemu-system-x86_64 -smp 4 -drive format=raw,file=build/ikos_longmode.img
```

## Support and Troubleshooting

For additional support:
1. Check the generated log files for detailed error messages
2. Review the compatibility matrix for known issues
3. Run hardware diagnostics to identify system-specific problems
4. Consult the debug output for bootloader execution details

## Summary

The IKOS testing suite provides comprehensive validation across multiple environments:
- ✅ Virtual machine testing with QEMU
- ✅ Real hardware compatibility validation  
- ✅ BIOS/UEFI firmware compatibility testing
- ✅ Debug and troubleshooting tools
- ✅ Performance benchmarking
- ✅ Automated test execution

This ensures the IKOS bootloader works reliably across a wide range of x86 hardware platforms and firmware configurations.
