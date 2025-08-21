# IKOS Scripts Directory

This directory contains all build, test, validation, and debugging scripts for the IKOS operating system project, organized by functionality.

## 📁 Directory Structure

### 🔨 Build Scripts (`build/`)
Scripts for building various components of IKOS:
- `build_kernel_debug.sh` - Build kernel with debugging symbols and debug output
- `build_userspace.sh` - Build user-space applications and libraries
- `create_bios_uefi_compat.sh` - Create BIOS/UEFI compatible boot images

### 🧪 Test Scripts (`test/`)
Comprehensive testing scripts for all system components:

#### Core System Tests
- `test_audio_system.sh` - Audio system functionality testing
- `test_gui.sh` - GUI system testing
- `test_input_system.sh` - Input handling system testing
- `test_kernel_logging.sh` - Kernel logging system testing
- `test_paging.sh` - Memory paging system testing
- `test_debugging.sh` - Debugging system testing

#### Memory & Process Tests
- `test_advanced_memory.sh` - Advanced memory management testing
- `test_user_space_memory.sh` - User-space memory management testing

#### Hardware & Driver Tests
- `test_usb_build.sh` - USB system build testing
- `test_usb_framework.sh` - USB driver framework testing
- `test_real_hardware.sh` - Real hardware compatibility testing

#### Emulation Tests
- `qemu_test.sh` - QEMU emulation testing
- `test_qemu.sh` - Extended QEMU testing scenarios

#### Issue-Specific Tests
- `test_issue_14.sh` - Specific test for issue #14
- `test_issue_17.sh` - Specific test for issue #17

### 🐛 Debug Scripts (`debug/`)
Debugging and diagnostic scripts:
- `debug_boot.sh` - Boot process debugging and analysis

### ✅ Validation Scripts (`validation/`)
System validation and verification scripts:
- `validate_cli.sh` - Command-line interface validation
- `validate_kernel_debug.sh` - Kernel debugging functionality validation
- `validate_scheduler.sh` - Process scheduler validation
- `verify_device_framework.sh` - Device driver framework verification
- `daemon_manager.sh` - System daemon management and validation

## 🚀 Quick Start

### Building the System
```bash
# Build kernel with debugging
./scripts/build/build_kernel_debug.sh

# Build user-space components
./scripts/build/build_userspace.sh

# Create bootable images
./scripts/build/create_bios_uefi_compat.sh
```

### Running Tests
```bash
# Run comprehensive audio system tests
./scripts/test/test_audio_system.sh

# Test GUI functionality
./scripts/test/test_gui.sh

# Test memory management
./scripts/test/test_advanced_memory.sh

# Test in QEMU
./scripts/test/qemu_test.sh
```

### Validation
```bash
# Validate scheduler
./scripts/validation/validate_scheduler.sh

# Validate CLI
./scripts/validation/validate_cli.sh

# Verify device framework
./scripts/validation/verify_device_framework.sh
```

### Debugging
```bash
# Debug boot process
./scripts/debug/debug_boot.sh

# Validate kernel debugging
./scripts/validation/validate_kernel_debug.sh
```

## 📋 Script Categories

### 🔧 Build Scripts
These scripts handle compilation, linking, and image creation:
- Support multiple target architectures
- Include debugging and optimization flags
- Generate bootable disk images
- Create development and release builds

### 🧪 Test Scripts
Comprehensive testing covering:
- **Unit Tests**: Individual component testing
- **Integration Tests**: Cross-component functionality
- **System Tests**: Full system functionality
- **Hardware Tests**: Real hardware compatibility
- **Regression Tests**: Prevent feature breakage

### ✅ Validation Scripts
Quality assurance and verification:
- **Functionality Validation**: Ensure features work as designed
- **Performance Validation**: Check performance metrics
- **Security Validation**: Verify security features
- **Compliance Validation**: Check standards compliance

### 🐛 Debug Scripts
Development and troubleshooting tools:
- **Boot Debugging**: Analyze boot process issues
- **Runtime Debugging**: Debug running system
- **Crash Analysis**: Analyze system crashes
- **Performance Analysis**: Profile system performance

## 📝 Script Standards

All scripts follow these conventions:

### 📋 Header Format
```bash
#!/bin/bash
# Script Name: purpose description
# Description: Detailed description of functionality
# Usage: ./script_name.sh [options]
# Dependencies: list of required tools/components
# Author: contributor information
# Last Modified: date
```

### 🔧 Error Handling
- Use `set -e` for exit on error
- Provide meaningful error messages
- Include cleanup on script exit
- Validate prerequisites before execution

### 📊 Output Format
- Use colored output for status (green=success, red=error, yellow=warning)
- Provide progress indicators for long operations
- Include verbose mode options
- Log important operations

### 🔗 Dependencies
- Check for required tools at script start
- Provide installation instructions for missing dependencies
- Use relative paths when possible
- Document external dependencies clearly

## 🛠️ Usage Examples

### Basic Testing Workflow
```bash
# 1. Build the system
./scripts/build/build_kernel_debug.sh

# 2. Run basic tests
./scripts/test/test_paging.sh
./scripts/test/test_kernel_logging.sh

# 3. Validate core components
./scripts/validation/validate_scheduler.sh

# 4. Test in emulation
./scripts/test/qemu_test.sh
```

### Development Workflow
```bash
# 1. Build with debugging
./scripts/build/build_kernel_debug.sh

# 2. Test specific component
./scripts/test/test_audio_system.sh

# 3. Debug if needed
./scripts/debug/debug_boot.sh

# 4. Validate changes
./scripts/validation/validate_cli.sh
```

### Release Workflow
```bash
# 1. Run full test suite
for test in ./scripts/test/*.sh; do
    echo "Running $test"
    "$test" || exit 1
done

# 2. Run all validations
for validation in ./scripts/validation/*.sh; do
    echo "Running $validation"
    "$validation" || exit 1
done

# 3. Create release build
./scripts/build/create_bios_uefi_compat.sh
```

## 🤝 Contributing Scripts

When adding new scripts:

1. **Choose the right directory** based on script purpose
2. **Follow naming conventions**: `action_component.sh`
3. **Include proper header** with description and usage
4. **Add error handling** and input validation
5. **Update this README** with script description
6. **Test thoroughly** before committing
7. **Document dependencies** and requirements

### Script Naming Convention
- `build_*.sh` - Build and compilation scripts
- `test_*.sh` - Testing scripts
- `validate_*.sh` - Validation scripts
- `verify_*.sh` - Verification scripts
- `debug_*.sh` - Debugging scripts

## 📞 Support

For script-related issues:
1. Check script documentation and comments
2. Verify all dependencies are installed
3. Run with verbose mode for detailed output
4. Check related documentation in `docs/` directory
5. Open an issue if problems persist

---

All scripts are designed to be run from the project root directory unless otherwise specified.
