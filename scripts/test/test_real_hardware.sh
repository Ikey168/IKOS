#!/bin/bash
# IKOS Real Hardware Testing Suite
# Tools and instructions for testing bootloader on actual x86 hardware

set -e

echo "=== IKOS Real Hardware Testing Suite ==="
echo "Preparing bootloader for testing on actual x86 hardware"
echo ""

# Configuration
REAL_HW_DIR="real_hardware_test"
USB_DEVICE=""
BOOTLOADER_IMAGE="build/ikos_longmode.img"

# Create testing directory
mkdir -p "$REAL_HW_DIR"

# Helper functions
log_info() {
    echo "[INFO] $1"
}

log_warn() {
    echo "[WARN] $1"
}

log_error() {
    echo "[ERROR] $1"
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        log_error "This operation requires root privileges. Please run with sudo."
        return 1
    fi
    return 0
}

# Function to create bootable USB
create_usb_boot() {
    local device=$1
    local image=$2
    
    if [ ! -b "$device" ]; then
        log_error "Device $device is not a block device"
        return 1
    fi
    
    if [ ! -f "$image" ]; then
        log_error "Bootloader image $image not found"
        return 1
    fi
    
    log_warn "WARNING: This will completely overwrite $device"
    log_warn "All data on the device will be lost!"
    echo -n "Are you sure you want to continue? (yes/no): "
    read -r confirmation
    
    if [ "$confirmation" != "yes" ]; then
        log_info "Operation cancelled"
        return 1
    fi
    
    log_info "Writing bootloader to $device..."
    
    # Unmount any mounted partitions
    umount "${device}"* 2>/dev/null || true
    
    # Write the bootloader image to the device
    dd if="$image" of="$device" bs=512 count=2880 status=progress
    
    # Ensure data is written
    sync
    
    log_info "Bootloader written to $device successfully"
    log_info "You can now boot from this USB device"
    
    return 0
}

# Function to create PXE boot files
create_pxe_boot() {
    local output_dir="$REAL_HW_DIR/pxe"
    
    mkdir -p "$output_dir"
    
    log_info "Creating PXE boot configuration..."
    
    # Convert IMG to binary for PXE
    cp "$BOOTLOADER_IMAGE" "$output_dir/ikos_boot.bin"
    
    # Create PXE configuration
    cat > "$output_dir/pxelinux.cfg" << 'EOF'
# IKOS PXE Boot Configuration
DEFAULT ikos

LABEL ikos
    KERNEL ikos_boot.bin
    APPEND 
EOF

    # Create TFTP server instructions
    cat > "$output_dir/README_PXE.md" << 'EOF'
# IKOS PXE Boot Setup

## Prerequisites
- TFTP server (tftpd-hpa or similar)
- DHCP server with PXE support
- Network-bootable target machine

## Setup Instructions

1. Copy files to TFTP root:
   ```bash
   sudo cp ikos_boot.bin /var/lib/tftpboot/
   sudo cp pxelinux.cfg /var/lib/tftpboot/
   ```

2. Configure DHCP server (example for ISC DHCP):
   ```
   option space pxelinux;
   option pxelinux.magic code 208 = string;
   option pxelinux.configfile code 209 = text;
   option pxelinux.pathprefix code 210 = text;
   option pxelinux.reboottime code 211 = unsigned integer 32;
   
   subnet 192.168.1.0 netmask 255.255.255.0 {
       range 192.168.1.100 192.168.1.200;
       option routers 192.168.1.1;
       option domain-name-servers 8.8.8.8;
       
       class "pxeclients" {
           match if substring (option vendor-class-identifier, 0, 9) = "PXEClient";
           next-server 192.168.1.1;  # TFTP server IP
           filename "ikos_boot.bin";
       }
   }
   ```

3. Start services:
   ```bash
   sudo systemctl start tftpd-hpa
   sudo systemctl start isc-dhcp-server
   ```

4. Boot target machine with PXE enabled in BIOS/UEFI
EOF

    log_info "PXE boot files created in $output_dir"
}

# Function to create hardware compatibility test
create_hw_compatibility_test() {
    local output_file="$REAL_HW_DIR/hardware_test_report.md"
    
    cat > "$output_file" << 'EOF'
# IKOS Hardware Compatibility Test Report

## Test Information
- Date: $(date)
- Bootloader Version: IKOS Long Mode Bootloader
- Test Conducted By: 

## Hardware Information
Please fill in the following information about the test system:

### System Details
- Manufacturer: 
- Model: 
- CPU: 
- RAM: 
- Storage: 
- BIOS/UEFI Version: 
- BIOS/UEFI Date: 

### Boot Test Results

#### USB Boot Test
- [ ] USB device recognized by BIOS/UEFI
- [ ] Boot menu shows USB option
- [ ] Bootloader starts execution
- [ ] Display output visible
- [ ] A20 line enabled successfully
- [ ] Protected mode entered
- [ ] Long mode activated
- [ ] No system crashes or hangs

#### PXE Boot Test (if applicable)
- [ ] Network boot option available
- [ ] PXE client connects to server
- [ ] Bootloader downloaded successfully
- [ ] Bootloader execution starts
- [ ] All boot stages complete

#### BIOS Compatibility
- [ ] Legacy BIOS boot successful
- [ ] CSM (Compatibility Support Module) works
- [ ] INT 10h video services functional
- [ ] INT 15h memory services functional
- [ ] Keyboard input responsive

#### UEFI Compatibility
- [ ] UEFI legacy boot mode works
- [ ] Secure Boot disabled test
- [ ] Boot from removable media enabled

### Performance Observations
- Boot time from power on: _____ seconds
- Time to bootloader start: _____ seconds
- Any noticeable delays: 

### Visual Output Test
- [ ] Text appears correctly on screen
- [ ] Colors display properly (if using debug version)
- [ ] No screen corruption or artifacts
- [ ] Cursor positioning correct

### Error Conditions
Document any errors, unusual behavior, or compatibility issues:

```
[Error logs and observations]
```

### Debug Output (if using debug version)
Serial output captured (connect to COM1 port if available):

```
[Serial debug output]
```

### Recommendations
Based on testing results:
- [ ] Fully compatible
- [ ] Compatible with minor issues
- [ ] Requires modifications
- [ ] Not compatible

### Additional Notes

EOF

    log_info "Hardware test report template created: $output_file"
}

# Function to create hardware preparation scripts
create_hw_scripts() {
    # USB preparation script
    cat > "$REAL_HW_DIR/prepare_usb.sh" << 'EOF'
#!/bin/bash
# USB Preparation Script for IKOS Hardware Testing

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOOTLOADER_IMAGE="../build/ikos_longmode.img"

echo "=== IKOS USB Boot Preparation ==="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root (use sudo)"
    exit 1
fi

# List available USB devices
echo "Available storage devices:"
lsblk -d -o NAME,SIZE,MODEL | grep -E "(sd[a-z]|nvme[0-9])"
echo ""

echo "WARNING: This will completely erase the selected device!"
echo -n "Enter the device path (e.g., /dev/sdb): "
read -r DEVICE

if [ ! -b "$DEVICE" ]; then
    echo "Error: $DEVICE is not a valid block device"
    exit 1
fi

if [ ! -f "$BOOTLOADER_IMAGE" ]; then
    echo "Error: Bootloader image not found: $BOOTLOADER_IMAGE"
    echo "Please run 'make' in the project root first"
    exit 1
fi

echo ""
echo "Device: $DEVICE"
echo "Bootloader: $BOOTLOADER_IMAGE"
echo ""
echo "This will completely overwrite $DEVICE with the IKOS bootloader"
echo -n "Type 'CONFIRM' to proceed: "
read -r CONFIRMATION

if [ "$CONFIRMATION" != "CONFIRM" ]; then
    echo "Operation cancelled"
    exit 1
fi

echo "Preparing USB device..."

# Unmount any mounted partitions
umount "${DEVICE}"* 2>/dev/null || true

# Write bootloader to device
echo "Writing bootloader..."
dd if="$BOOTLOADER_IMAGE" of="$DEVICE" bs=512 status=progress

# Ensure data is written
sync

echo ""
echo "✓ USB device prepared successfully!"
echo "✓ You can now boot from $DEVICE on target hardware"
echo ""
echo "Boot Instructions:"
echo "1. Insert USB device into target machine"
echo "2. Enter BIOS/UEFI setup (usually F2, F12, or DEL during boot)"
echo "3. Enable 'Boot from USB' or 'Boot from Removable Media'"
echo "4. Set USB device as first boot priority"
echo "5. Save settings and restart"
echo "6. The IKOS bootloader should start automatically"
EOF

    chmod +x "$REAL_HW_DIR/prepare_usb.sh"

    # Hardware diagnostics script
    cat > "$REAL_HW_DIR/diagnose_hardware.sh" << 'EOF'
#!/bin/bash
# Hardware Diagnostics for IKOS Compatibility

echo "=== IKOS Hardware Compatibility Diagnostics ==="
echo ""

echo "System Information:"
echo "==================="
echo "Date: $(date)"
echo "Hostname: $(hostname)"
echo "Kernel: $(uname -r)"
echo "Architecture: $(uname -m)"
echo ""

echo "CPU Information:"
echo "================"
lscpu | grep -E "(Architecture|CPU\(s\)|Model name|CPU family|CPU max MHz)"
echo ""

echo "Memory Information:"
echo "=================="
free -h
echo ""
echo "Memory map from /proc/meminfo:"
grep -E "(MemTotal|MemFree|MemAvailable)" /proc/meminfo
echo ""

echo "Storage Information:"
echo "==================="
lsblk -o NAME,SIZE,TYPE,MOUNTPOINT
echo ""

echo "USB Devices:"
echo "============"
lsusb
echo ""

echo "PCI Devices:"
echo "============"
lspci | grep -E "(VGA|Network|USB|SATA|IDE)"
echo ""

echo "BIOS/UEFI Information:"
echo "======================"
if [ -d "/sys/firmware/efi" ]; then
    echo "System uses UEFI firmware"
    echo "EFI variables:"
    ls /sys/firmware/efi/efivars/ 2>/dev/null | head -5
else
    echo "System uses legacy BIOS"
fi
echo ""

echo "DMI/SMBIOS Information:"
echo "======================"
if command -v dmidecode >/dev/null 2>&1; then
    echo "System Manufacturer: $(sudo dmidecode -s system-manufacturer 2>/dev/null || echo 'Unknown')"
    echo "System Product: $(sudo dmidecode -s system-product-name 2>/dev/null || echo 'Unknown')"
    echo "BIOS Vendor: $(sudo dmidecode -s bios-vendor 2>/dev/null || echo 'Unknown')"
    echo "BIOS Version: $(sudo dmidecode -s bios-version 2>/dev/null || echo 'Unknown')"
    echo "BIOS Date: $(sudo dmidecode -s bios-release-date 2>/dev/null || echo 'Unknown')"
else
    echo "dmidecode not available - install with: sudo apt install dmidecode"
fi
echo ""

echo "Boot Capability Assessment:"
echo "==========================="
echo "USB Boot Support: Check BIOS/UEFI settings"
echo "Legacy Boot Support: Check BIOS/UEFI settings"
echo "Network Boot Support: Check BIOS/UEFI settings"
echo ""

echo "Recommendations:"
echo "================"
echo "1. Ensure legacy boot support is enabled in BIOS/UEFI"
echo "2. Disable Secure Boot if using UEFI"
echo "3. Enable 'Boot from USB' or similar option"
echo "4. Set USB device as first boot priority"
echo "5. For PXE boot, enable network boot and configure DHCP"
echo ""

echo "Compatibility Notes:"
echo "===================="
echo "- IKOS bootloader requires x86/x86_64 architecture"
echo "- Minimum 32MB RAM recommended"
echo "- VGA-compatible display required for visual output"
echo "- Serial port (COM1) useful for debug output"
echo ""
EOF

    chmod +x "$REAL_HW_DIR/diagnose_hardware.sh"

    log_info "Hardware preparation scripts created in $REAL_HW_DIR"
}

# Main menu
show_menu() {
    echo ""
    echo "=== IKOS Real Hardware Testing Options ==="
    echo "1. Create USB bootable device"
    echo "2. Create PXE boot configuration"
    echo "3. Generate hardware test report template"
    echo "4. Create hardware preparation scripts"
    echo "5. Run hardware diagnostics"
    echo "6. Display testing instructions"
    echo "7. Exit"
    echo ""
}

# Main execution
if [ ! -f "$BOOTLOADER_IMAGE" ]; then
    log_error "Bootloader image not found: $BOOTLOADER_IMAGE"
    log_info "Please run 'make' to build the bootloader first"
    exit 1
fi

log_info "Bootloader image found: $BOOTLOADER_IMAGE"
log_info "Setting up real hardware testing environment..."

# Always create the basic files
create_pxe_boot
create_hw_compatibility_test
create_hw_scripts

# Show interactive menu
while true; do
    show_menu
    echo -n "Select an option (1-7): "
    read -r choice
    
    case $choice in
        1)
            echo ""
            if check_root; then
                echo "Available storage devices:"
                lsblk -d -o NAME,SIZE,MODEL | grep -E "(sd[a-z]|nvme[0-9])" || echo "No suitable devices found"
                echo ""
                echo -n "Enter USB device path (e.g., /dev/sdb): "
                read -r usb_device
                create_usb_boot "$usb_device" "$BOOTLOADER_IMAGE"
            else
                log_info "Please run this script with sudo to create USB boot device"
            fi
            ;;
        2)
            create_pxe_boot
            log_info "PXE configuration created"
            ;;
        3)
            create_hw_compatibility_test
            log_info "Hardware test report template created"
            ;;
        4)
            create_hw_scripts
            log_info "Hardware preparation scripts created"
            ;;
        5)
            bash "$REAL_HW_DIR/diagnose_hardware.sh"
            ;;
        6)
            cat << 'EOF'

=== IKOS Real Hardware Testing Instructions ===

1. USB Boot Testing:
   - Run: sudo ./real_hardware_test/prepare_usb.sh
   - Insert USB into target machine
   - Boot from USB device
   - Observe bootloader execution

2. PXE Boot Testing:
   - Set up TFTP server with files from real_hardware_test/pxe/
   - Configure DHCP for PXE boot
   - Boot target machine with network boot enabled

3. Compatibility Testing:
   - Fill out hardware test report: real_hardware_test/hardware_test_report.md
   - Test various BIOS/UEFI configurations
   - Document any issues or compatibility problems

4. Debug Output:
   - Connect serial cable to COM1 if available
   - Use debug-enabled bootloader for detailed output
   - Capture serial output for analysis

5. Multiple Hardware Test:
   - Test on different CPU architectures (32-bit, 64-bit)
   - Test on various manufacturers (Dell, HP, Lenovo, etc.)
   - Test on different BIOS versions and dates

For detailed instructions, see the generated documentation files.
EOF
            ;;
        7)
            log_info "Exiting real hardware testing suite"
            break
            ;;
        *)
            log_warn "Invalid option. Please select 1-7."
            ;;
    esac
done

echo ""
echo "=== Real Hardware Testing Setup Complete ==="
echo ""
echo "Generated files:"
echo "- USB preparation script: $REAL_HW_DIR/prepare_usb.sh"
echo "- PXE boot configuration: $REAL_HW_DIR/pxe/"
echo "- Hardware test report template: $REAL_HW_DIR/hardware_test_report.md"
echo "- Hardware diagnostics: $REAL_HW_DIR/diagnose_hardware.sh"
echo ""
echo "Next steps:"
echo "1. Review the generated documentation"
echo "2. Prepare bootable media using provided scripts"
echo "3. Test on target hardware"
echo "4. Fill out compatibility report"
echo ""
