# IKOS Operating System

## About
IKOS is a **custom microkernel-based operating system** designed for **x86 and x86_64 consumer devices** (desktops, laptops). It features a **custom bootloader with real mode initialization**, a **virtual memory manager (VMM)**, and **message-passing-based inter-process communication (IPC)**. The OS is developed with a **modular and parallel development approach**, allowing independent system components to evolve without a strict module API.

## Current Status - Issue #1: Real Mode Initialization ✅ COMPLETED

**All Required Tasks Successfully Implemented:**
- [x] Configure segment registers (DS, ES, FS, GS, SS set to 0 for flat memory model)
- [x] Set up the stack pointer (SP configured at 0x7C00 with downward growth)
- [x] Prepare memory map for the bootloader (BIOS INT 0x15, AX=0xE820 implementation)

**Testing Results:**
- [x] Basic bootloader: Successfully boots and completes initialization
- [x] Compact bootloader: Successfully boots with memory detection (639KB conv + 64MB ext)
- [x] All segment registers properly configured to 0x0000
- [x] Stack pointer set to 0x7C00 with proper downward growth
- [x] Memory mapping functional using BIOS services

**Additional Features Implemented:**
- [x] Video mode initialization (80x25 color text mode)
- [x] Comprehensive memory detection (conventional and extended memory)
- [x] System information display
- [x] Error handling and status reporting
- [x] Clean bootloader architecture with modular functions

## Bootloader Features
The IKOS bootloader provides two implementations:

### Basic Bootloader (`boot/boot.asm`)
- Essential real mode initialization
- Segment register configuration
- Stack setup
- Basic memory mapping
- Minimal system output

### Enhanced Bootloader (`boot/boot_enhanced.asm`)
- Complete real mode initialization sequence
- Comprehensive memory detection and mapping
- Video mode initialization
- Detailed system information display
- Enhanced error handling
- Modular function architecture

## Features
- **Custom Bootloader**: Initializes CPU, memory, and loads the kernel.
- **Microkernel Architecture**: Only core functions in the kernel; drivers and services in user space.
- **Virtual Memory Management (VMM)**: Paging-based memory isolation.
- **Message-Passing IPC**: Inter-process communication via queues.
- **Parallel Feature Development**: No strict module API for flexibility.
- **Basic Device Support**: Keyboard, display, disk, networking.
- **Preemptive Multi-tasking**: Round-robin or priority scheduling.
- **Filesystem Support**: Virtual file system (VFS), with initial FAT support.
- **Graphical UI (Planned)**: Framebuffer-based window system.
- **Networking Stack (Planned)**: Basic TCP/IP for internet access.

## Architecture Overview
```
+---------------------------------------------------+
| User Applications (GUI, Shell, Web, Filesystem)  |
+---------------------------------------------------+
| System Services (FS, Network, Audio, etc.)       |
+---------------------------------------------------+
| Device Drivers (GPU, Storage, Input, etc.)       |
+---------------------------------------------------+
| Microkernel (IPC, Scheduler, VMM, Interrupts)    |
+---------------------------------------------------+
| Hardware (CPU, RAM, Disk, GPU, Network, etc.)    |
+---------------------------------------------------+
```

## Getting Started

### Prerequisites
Install the required development tools:
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install -y nasm qemu-system-x86 build-essential

# Or use the Makefile target
make install-deps
```

### Building the Bootloader
1. Clone and navigate to the repository:
   ```bash
   git clone <repository-url>
   cd IKOS
   ```

2. Build both bootloader versions:
   ```bash
   make all
   ```
   This creates:
   - `build/ikos.img` - Basic bootloader disk image
   - `build/ikos_enhanced.img` - Enhanced bootloader disk image

3. Build individual versions:
   ```bash
   make build/ikos.img          # Basic bootloader only
   make build/ikos_enhanced.img # Enhanced bootloader only
   ```

### Quick Start

To quickly test the real mode initialization:

```bash
# Build and test the compact bootloader (recommended)
make test-compact

# Build and test the basic bootloader  
make test

# Build both versions
make all
```

**Expected Output (Compact Bootloader):**
```
IKOS Bootloader - Real Mode Init
Conv: 027FKB
Ext: FC00KB  
Init complete!
```

### Testing the Bootloader

#### Run the Enhanced Bootloader (Recommended)
```bash
make test-enhanced
```

#### Run the Basic Bootloader
```bash
make test
```

#### Debug Mode
```bash
make debug-enhanced    # Enhanced bootloader with GDB debugging
make debug            # Basic bootloader with GDB debugging
```

### What You'll See
When running the enhanced bootloader, you'll see:
1. **Boot Banner** - IKOS bootloader identification
2. **Memory Detection** - Conventional and extended memory sizes
3. **Memory Map Creation** - BIOS memory map enumeration
4. **System Information** - Segment registers and stack pointer values
5. **Completion Status** - Confirmation of all initialization tasks

## Project Structure
```
IKOS/
├── boot/
│   ├── boot.asm           # Basic bootloader implementation
│   ├── boot_enhanced.asm  # Enhanced bootloader with full features
│   └── boot.ld           # Linker script for bootloader
├── include/
│   ├── boot.h            # C header definitions
│   ├── boot.inc          # Assembly include file
│   └── memory.h          # Memory layout definitions
├── src/                  # Source code (future kernel implementation)
├── build/                # Build output directory (created by make)
├── Makefile             # Build system
└── README.md           # This file
```
   make run
   ```

### Testing
- Use `make test` to run unit tests on kernel components.
- Debugging can be done using `GDB` with QEMU:
   ```sh
   make debug
   ```

## Roadmap
- [x] Bootloader Development
- [x] Microkernel Core (IPC, Scheduling, VMM)
- [ ] User-space Services (Filesystem, Networking, Device Drivers)
- [ ] GUI & Shell Development
- [ ] Networking Stack & Advanced Features

## Contributing
Contributions are welcome! Please follow these steps:
1. Fork the repository.
2. Create a new branch (`feature-name`).
3. Commit changes (`git commit -m "Added feature X"`).
4. Push to the branch (`git push origin feature-name`).
5. Open a pull request.

## License
This project is licensed under the MIT License. See `LICENSE` for details.


