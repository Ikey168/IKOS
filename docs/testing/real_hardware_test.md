# IKOS Real Hardware Bootloader Test Guide

## USB Boot Testing

### 1. Write Bootloader Image to USB
```bash
sudo dd if=build/ikos_longmode.img of=/dev/sdX bs=512 count=2880 conv=notrunc
sync
```
Replace `/dev/sdX` with your USB device (check with `lsblk`).

### 2. Boot from USB
- Insert USB into x86 machine
- Select USB boot from BIOS/UEFI boot menu
- Observe bootloader output (serial, VGA, or debug)

## PXE Network Boot Testing

### 1. Prepare PXE Server
- Copy `build/ikos_longmode.img` to TFTP root
- Configure DHCP and PXE to serve the image

### 2. Boot PXE Client
- Network boot x86 client
- Observe bootloader output

## BIOS/UEFI Compatibility Verification

### 1. Test on Multiple Machines
- Legacy BIOS (older desktops/laptops)
- UEFI (modern systems, enable CSM/legacy mode if needed)

### 2. Troubleshooting
- If boot fails, check:
  - USB image written correctly
  - BIOS/UEFI boot order
  - Secure Boot disabled (for UEFI)
  - Serial/VGA output for debug messages

## Expected Outcome
- Bootloader works on QEMU and real x86 hardware
- Debug output visible via serial and/or VGA
- Compatible with both BIOS and UEFI implementations

## Notes
- For UEFI-only systems, consider adding a UEFI bootloader in future
- For PXE, ensure network and TFTP setup is correct
- Use serial debugging for headless or embedded systems
