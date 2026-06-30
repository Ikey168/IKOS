# IKOS Operating System

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Orthogonal Persistence](https://github.com/Ikey168/IKOS/actions/workflows/persistence.yml/badge.svg)](https://github.com/Ikey168/IKOS/actions/workflows/persistence.yml)
[![Architecture](https://img.shields.io/badge/Architecture-x86__64-blue.svg)]()
[![Kernel](https://img.shields.io/badge/Kernel-Microkernel-orange.svg)]()

> **IKOS is a microkernel with orthogonal persistence: no save, no load, no shutdown.**
> The entire running system is the durable state. Pull the power cord, plug it back
> in, and your work is exactly where you left it, mid-instruction.

This is a real, proven idea (KeyKOS, EROS, Phantom OS; IBM i's single-level store in
production) that **no other hobby-tier microkernel implements**. It is IKOS's reason to
exist, and it rides directly on the kernel's copy-on-write virtual memory.

## ⚡ The headline: pull the plug, keep your work

A program does **nothing special** to be durable. The counter below has no `save()`,
no `load()`, no serialization. The OS checkpoints the whole system periodically and
resumes from the last checkpoint on boot:

```c
/* user/persistent_counter.c - the entire persistence "logic" */
uint64_t counter = 0;
for (;;) { counter++; print_u64(counter); yield(); }
```

The end-to-end demo (`scripts/test/persistence_demo.sh`) proves it against the real
checkpoint engine and on-disk store, modeling a power cut as a process restart:

```
==> power cycle 1: boot fresh, count to 5
  [run] counter now = 5 (then 'power is cut')
==> reboot: counter must have survived at 5
  [verify] counter = 5  == expected 5  -> REMEMBERED
==> power cycle 2: count 5 more (resumes from 5)
  [run] counter now = 10 (then 'power is cut')
==> reboot: counter must have survived at 10
  [verify] counter = 10  == expected 10  -> REMEMBERED
==> crash test: cut power MID-RUN (kill -9), then reboot
  [crash] reloaded a valid checkpoint; counter = 3903 (>= 10, monotonic)
PASSED: the counter remembered itself across every power cycle
```

### How it works

A checkpoint marks every writable page read-only and copy-on-write (cheap; it copies
nothing). The first write to a marked page faults, and the kernel preserves the page's
pre-checkpoint contents before letting the write proceed. A background pass streams
those pages into the **inactive** one of two on-disk slots; a single superblock-sector
write flips the active slot and is the only commit point, so a crash before the flip
always leaves the previous checkpoint intact (CRC32 guards every slot). On boot, if a
valid checkpoint exists, the kernel reloads it instead of cold-starting.

Full design: [`docs/architecture/orthogonal-persistence.md`](docs/architecture/orthogonal-persistence.md).

## ✅ What actually works vs. roadmap

Being honest about maturity:

| Area | Status |
|------|--------|
| Orthogonal persistence: snapshot store (double-buffered slots + CRC) | ✅ implemented, unit-tested |
| Checkpoint engine: stop-the-world COW marking, page-fault capture, writeback | ✅ implemented, unit-tested |
| Restore + boot decision (restore vs cold boot) | ✅ implemented, unit-tested |
| Periodic checkpoint trigger (scheduler tick) | ✅ implemented, unit-tested |
| External-state policy (sockets/DMA/devices severed on restore) | ✅ implemented, unit-tested |
| End-to-end "yank power" proof over a file-backed disk | ✅ passing in CI |
| Full in-kernel boot (store wired to a block device, runnable in QEMU) | 🚧 wired and compiling; not yet booted end-to-end |
| Process register/scheduler-state resume (runnable restored processes) | 🚧 page + address-space restore done; full process resume in progress |
| Persisting kernel-internal and driver state | 🔭 v2 (v1 cold-inits the kernel/drivers and restores user spaces on top) |

The broader subsystems below describe the project's overall scope; several are partial
or aspirational. The persistence stack is the part with end-to-end tests and CI today.

## About

**IKOS** is a **microkernel-based operating system** for **x86/x86_64 consumer devices**
(desktops, laptops, embedded systems), organized around the orthogonal-persistence thesis
above. Alongside it the project includes:

- ♾️ **Orthogonal Persistence** with periodic system checkpoints and resume-on-boot
- 🚀 **Custom Multi-Stage Bootloader** with real mode to long mode transition
- 🧠 **Advanced Virtual Memory Manager (VMM)** with copy-on-write and demand paging
- 💬 **Message-Passing IPC** for secure inter-process communication
- 🎵 **Comprehensive Audio System** with AC97 codec support
- 🖥️ **GUI Framework** with window management and graphics acceleration
- 🌐 **TCP/IP Network Stack** with socket programming interface
- 📁 **Virtual File System (VFS)** supporting multiple filesystems
- 🔐 **Security Framework** with authentication and authorization

IKOS is developed with a **modular and parallel development approach**, enabling independent evolution of system components while maintaining clean interfaces and robust integration.

## 🎯 Key Features

### 🏗️ **Microkernel Architecture**
- **Minimal kernel space**: Only essential services (IPC, scheduling, memory management)
- **User-space drivers**: Device drivers and system services run in protected user space
- **Message-passing communication**: Secure and efficient inter-process communication
- **Fault isolation**: System component failures don't crash the entire OS

### 💾 **Advanced Memory Management**
- **Virtual Memory Manager (VMM)**: Full x86_64 paging with 4-level page tables
- **Copy-on-Write (COW)**: Efficient memory sharing and process forking
- **Demand Paging**: Load pages only when needed for optimal memory usage
- **Memory Protection**: Hardware-enforced isolation between processes
- **NUMA Support**: Non-uniform memory access optimization

### 🎵 **Multimedia Support**
- **Audio System**: Comprehensive audio framework with AC97 codec driver
- **Sound Playback & Recording**: Multiple format support (PCM 8/16/24/32-bit)
- **Volume Control**: Per-device volume and mute functionality
- **Tone Generation**: Built-in tone generator for system sounds and testing

### 🖥️ **Graphics & GUI**
- **Framebuffer Driver**: Direct graphics memory access
- **Window Manager**: Advanced windowing system with compositing
- **GUI Framework**: Rich widget library for application development
- **Input Handling**: Comprehensive keyboard and mouse support
- **Terminal Emulator**: Feature-rich terminal with escape sequence support

### 🌐 **Networking**
- **TCP/IP Stack**: Full IPv4 networking implementation
- **Socket API**: Berkeley sockets compatible interface
- **Network Drivers**: Ethernet and Wi-Fi device support
- **DNS & TLS**: Domain name resolution and secure connections

### 📁 **File System**
- **Virtual File System (VFS)**: Unified interface for multiple filesystems
- **FAT32 Support**: Read/write support for FAT32 filesystems
- **EXT2/EXT4 Support**: Linux-compatible filesystem support
- **File Explorer**: Graphical file management application
- **POSIX Compliance**: Standard file operations and permissions

### 🔐 **Security & Authentication**
- **User Authentication**: Multi-factor authentication support
- **Authorization Framework**: Role-based access control
- **Process Isolation**: Hardware-enforced process boundaries
- **Secure Boot**: Verified boot chain from bootloader to kernel

### 🛠️ **Development Tools**
- **Runtime Debugger**: Kernel-level debugging with symbol support
- **Logging System**: Comprehensive system logging and analysis
- **Build System**: Automated build and testing infrastructure
- **Documentation**: Extensive technical documentation for all components

## 🏗️ Architecture Overview

IKOS follows a **microkernel architecture** with clear separation between kernel space and user space:

```
┌─────────────────────────────────────────────────────────────────┐
│                    User Applications                            │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌──────────── │
│  │  GUI Apps   │ │   Terminal  │ │ File Manager│ │  Web Browser │
│  └─────────────┘ └─────────────┘ └─────────────┘ └──────────── │
├─────────────────────────────────────────────────────────────────┤
│                    System Services                              │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌──────────── │
│  │ File System │ │   Network   │ │    Audio    │ │   Graphics  │
│  │   Service   │ │   Service   │ │   Service   │ │   Service   │
│  └─────────────┘ └─────────────┘ └─────────────┘ └──────────── │
├─────────────────────────────────────────────────────────────────┤
│                    Device Drivers                               │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌──────────── │
│  │   Storage   │ │   Network   │ │    Audio    │ │    Input    │
│  │   Drivers   │ │   Drivers   │ │   Drivers   │ │   Drivers   │
│  └─────────────┘ └─────────────┘ └─────────────┘ └──────────── │
├─────────────────────────────────────────────────────────────────┤
│                      Microkernel                                │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌──────────── │
│  │     IPC     │ │  Scheduler  │ │     VMM     │ │ Interrupts  │
│  │   Manager   │ │   Manager   │ │   Manager   │ │   Handler   │
│  └─────────────┘ └─────────────┘ └─────────────┘ └──────────── │
├─────────────────────────────────────────────────────────────────┤
│                      Hardware Layer                             │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌──────────── │
│  │     CPU     │ │    Memory   │ │   Storage   │ │  Peripherals│
│  └─────────────┘ └─────────────┘ └─────────────┘ └──────────── │
└─────────────────────────────────────────────────────────────────┘
```

### Communication Flow
- **System Calls**: User applications communicate with kernel via system call interface
- **Message Passing**: Inter-process communication through secure message queues
- **Shared Memory**: High-performance data sharing between authorized processes
- **Interrupts**: Hardware events handled by kernel and forwarded to appropriate drivers

## 🚀 Getting Started

### Prerequisites

IKOS requires a Unix-like development environment with the following tools:

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    nasm gcc ld make \
    qemu-system-x86 \
    build-essential \
    git

# Fedora/CentOS/RHEL
sudo dnf install -y \
    nasm gcc ld make \
    qemu-system-x86 \
    gcc-c++ \
    git

# macOS (using Homebrew)
brew install nasm gcc make qemu git
```

### Quick Start

1. **Clone the repository:**
   ```bash
   git clone https://github.com/Ikey168/IKOS.git
   cd IKOS
   ```

2. **Build the complete system:**
   ```bash
   make all
   ```

3. **Test the latest bootloader (ELF kernel loading):**
   ```bash
   make test-elf-compact
   ```

4. **Run specific components:**
   ```bash
   # Audio system
   cd kernel && make audio-system && make audio-test
   
   # GUI system
   make gui-system && ./test_gui.sh
   
   # Network stack
   make network-test
   ```

### Build Targets

IKOS provides multiple build targets for different components:

| Target | Description |
|--------|-------------|
| `make all` | Build complete IKOS system |
| `make bootloader` | Build all bootloader variants |
| `make kernel` | Build kernel components |
| `make audio-system` | Build audio framework |
| `make gui-system` | Build GUI framework |
| `make network-stack` | Build networking components |
| `make test` | Run comprehensive test suite |
| `make clean` | Clean all build artifacts |

### Testing & Debugging

#### Running Tests
```bash
# Complete system test
make test

# Orthogonal persistence: "yank power, it remembers" end-to-end demo
./scripts/test/persistence_demo.sh       # checkpoint/restore over a power cycle

# Component-specific tests
./scripts/test/test_audio_system.sh      # Audio system validation
./scripts/test/test_gui.sh               # GUI system testing
./scripts/test/test_input_system.sh      # Input handling tests
./scripts/test/test_usb_framework.sh     # USB driver tests

# Hardware testing
./scripts/test/test_real_hardware.sh     # Real hardware compatibility
./scripts/test/qemu_test.sh              # QEMU emulation testing

# Memory and process testing
./scripts/test/test_advanced_memory.sh   # Advanced memory management
./scripts/test/test_paging.sh           # Paging system tests
```

#### Validation & Verification
```bash
# System validation
./scripts/validation/validate_scheduler.sh    # Scheduler validation
./scripts/validation/validate_cli.sh          # CLI validation
./scripts/validation/verify_device_framework.sh # Device framework verification

# Debugging
./scripts/debug/debug_boot.sh                 # Boot process debugging
./scripts/validation/validate_kernel_debug.sh # Kernel debugging validation
```
```

#### Debugging with QEMU + GDB
```bash
# Start QEMU with GDB server
make debug

# In another terminal, connect GDB
gdb kernel/kernel.elf
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

#### Expected Output

**ELF Kernel Loading Bootloader:**
```
IKOS Bootloader v2.0 - Long Mode
Initializing system...
Loading ELF kernel...
IKOS: ELF->LOAD->KERNEL
Kernel loaded successfully
Switching to long mode...
IKOS Kernel v1.0 starting...
```

**Audio System Test:**
```
=== IKOS Audio System Test ===
Initializing audio library...
Found 1 audio devices
Device 0: AC97 Audio Controller
Volume control: OK
Stream operations: OK
Tone generation: OK
All tests PASSED!
## 📁 Project Structure

```
IKOS/
├── 🚀 boot/                           # Multi-stage bootloader implementations
│   ├── boot.asm                      # Basic real mode bootloader
│   ├── boot_enhanced.asm             # Enhanced bootloader with memory detection
│   ├── boot_longmode.asm             # Long mode (64-bit) bootloader
│   ├── boot_elf_loader.asm           # ELF kernel loading bootloader
│   └── boot.ld                       # Bootloader linker script
├── 🧠 kernel/                         # Microkernel implementation
│   ├── audio.c                       # Audio system framework
│   ├── scheduler.c                   # Process scheduler
│   ├── interrupts.c                  # Interrupt handling
│   ├── vmm.c                         # Virtual memory manager
│   ├── ipc.c                         # Inter-process communication
│   ├── gui.c                         # GUI framework
│   ├── framebuffer.c                 # Graphics framebuffer driver
│   ├── network_driver.c              # Network stack implementation
│   └── Makefile                      # Kernel build system
├── 📋 include/                        # System headers and definitions
│   ├── audio.h                       # Audio system API
│   ├── audio_ac97.h                  # AC97 codec driver
│   ├── audio_user.h                  # User-space audio library
│   ├── vmm.h                         # Virtual memory management
│   ├── scheduler.h                   # Process scheduling
│   ├── gui.h                         # GUI framework
│   ├── framebuffer.h                 # Graphics API
│   └── syscalls.h                    # System call interface
├── 🧪 tests/                          # Comprehensive test suites
│   ├── test_audio.c                  # Audio system tests
│   ├── test_gui.c                    # GUI system tests
│   ├── test_scheduler.c              # Scheduler tests
│   └── test_memory.c                 # Memory management tests
├── 👤 user/                           # User-space applications
│   ├── shell/                        # Command-line shell
│   ├── gui_apps/                     # Graphical applications
│   └── demos/                        # Example applications
├── 📚 docs/                           # Comprehensive documentation
│   ├── README.md                     # Documentation index
│   ├── architecture/                 # System architecture docs
│   │   ├── VMM_README.md            # Virtual memory architecture
│   │   ├── DEVICE_DRIVER_FRAMEWORK.md # Driver framework design
│   │   ├── USB_DRIVER_FRAMEWORK.md  # USB framework architecture
│   │   ├── NETWORK_STACK.md         # Network stack design
│   │   └── TCPIP.md                 # TCP/IP implementation
│   ├── implementation/               # Implementation documentation
│   │   ├── AUDIO_IMPLEMENTATION.md  # Audio system implementation
│   │   ├── GUI_IMPLEMENTATION.md    # GUI framework implementation
│   │   ├── SCHEDULER_IMPLEMENTATION.md # Scheduler implementation
│   │   ├── VMM_IMPLEMENTATION_SUMMARY.md # Memory management
│   │   └── ...                      # Other component implementations
│   └── testing/                      # Testing and debugging docs
│       ├── QEMU_REAL_HARDWARE_TEST.md # Hardware testing guide
│       ├── RUNTIME_KERNEL_DEBUGGER.md # Debugging guide
│       └── real_hardware_test.md    # Real hardware testing
├── 🔧 scripts/                        # Build, test, and utility scripts
│   ├── README.md                     # Scripts documentation
│   ├── build/                        # Build scripts
│   │   ├── build_kernel_debug.sh    # Debug kernel build
│   │   ├── build_userspace.sh       # User-space build
│   │   └── create_bios_uefi_compat.sh # Boot image creation
│   ├── test/                         # Test scripts
│   │   ├── test_audio_system.sh     # Audio system tests
│   │   ├── test_gui.sh              # GUI system tests
│   │   ├── test_advanced_memory.sh  # Memory management tests
│   │   ├── qemu_test.sh             # QEMU testing
│   │   └── ...                      # Other test scripts
│   ├── validation/                   # Validation scripts
│   │   ├── validate_scheduler.sh    # Scheduler validation
│   │   ├── validate_cli.sh          # CLI validation
│   │   └── verify_device_framework.sh # Device framework verification
│   └── debug/                        # Debug scripts
│       └── debug_boot.sh            # Boot debugging
├── 🔧 tools/                          # Development tools and utilities
│   ├── analyze_paging.py            # Paging analysis tool
│   └── test_signal_compile.c        # Signal testing utility
├── 📋 examples/                       # Demo scripts and examples
│   ├── demo_issue_17.sh             # Issue #17 demonstration
│   └── demo_issue_18.sh             # Issue #18 demonstration
├── 🏗️ build/                          # Build artifacts (generated)
├── 📦 bin/                            # Binary executables
├── 📚 lib/                            # System libraries
└── 🛠️ Makefile                        # Main build system
```

## 🎯 Current Status & Milestones

### ✅ **Completed Components**

#### 🚀 **Bootloader & Initialization** 
- ✅ Real mode initialization and memory detection
- ✅ Protected mode transition with A20 line enablement
- ✅ Long mode (64-bit) transition and setup
- ✅ ELF kernel loading and execution
- ✅ Multi-stage bootloader architecture

#### 🧠 **Microkernel Core**
- ✅ Process scheduler with preemptive multitasking
- ✅ Virtual memory manager (VMM) with paging
- ✅ Interrupt handling and system calls
- ✅ Inter-process communication (IPC)
- ✅ Memory allocation and management

#### 🎵 **Audio System**
- ✅ Comprehensive audio framework
- ✅ AC97 codec driver implementation
- ✅ Audio streaming and buffer management
- ✅ Volume control and device management
- ✅ User-space audio library and API

#### 🖥️ **Graphics & GUI**
- ✅ Framebuffer driver for direct graphics access
- ✅ GUI framework with widget system
- ✅ Window management and compositing
- ✅ Input handling (keyboard and mouse)
- ✅ Terminal emulator with advanced features

#### 🌐 **Networking**
- ✅ TCP/IP stack implementation
- ✅ Socket programming interface
- ✅ Network device drivers (Ethernet/Wi-Fi)
- ✅ DNS resolution and TLS support

#### 📁 **File System**
- ✅ Virtual File System (VFS) framework
- ✅ FAT32 filesystem support
- ✅ EXT2/EXT4 filesystem support
- ✅ File operations and permissions

#### 🔐 **Security & Authentication**
- ✅ User authentication system
- ✅ Multi-factor authentication support
- ✅ Authorization and access control
- ✅ Process isolation and protection

#### 🛠️ **Development Tools**
- ✅ Runtime kernel debugger
- ✅ Comprehensive logging system
- ✅ Build automation and testing
- ✅ Memory analysis tools

### 🚧 **In Progress**
- 🔄 Advanced memory management optimizations
- 🔄 Hardware acceleration for graphics
- 🔄 Advanced networking protocols
- 🔄 Package management system

### 📋 **Planned Features**
- 📅 UEFI boot support
- 📅 SMP (multi-processor) support
- 📅 Advanced power management
- 📅 Container/virtualization support
- 📅 Package repository and installer

## 🧪 Testing & Quality Assurance

IKOS includes comprehensive testing infrastructure:

### Test Categories
- **Unit Tests**: Individual component testing
- **Integration Tests**: Component interaction testing  
- **System Tests**: End-to-end functionality testing
- **Performance Tests**: Benchmarking and optimization
- **Hardware Tests**: Real hardware compatibility testing

### Automated Testing
```bash
# Run all tests
make test-all

# Component-specific testing
make test-audio      # Audio system tests
make test-gui        # GUI framework tests
make test-memory     # Memory management tests
make test-network    # Network stack tests
make test-fs         # Filesystem tests
```

### Continuous Integration
- Automated builds on multiple architectures
- Regression testing for all components
- Performance benchmarking
- Code quality analysis
- Documentation validation

## 🤝 Contributing

We welcome contributions from developers of all skill levels! Here's how to get started:

### Development Setup
1. **Fork and clone the repository:**
   ```bash
   git clone https://github.com/yourusername/IKOS.git
   cd IKOS
   ```

2. **Set up development environment:**
   ```bash
   make install-deps    # Install development dependencies
   make all            # Build the complete system
   make test           # Run test suite
   ```

3. **Create a feature branch:**
   ```bash
   git checkout -b feature/your-feature-name
   ```

### Contribution Guidelines
- **Code Style**: Follow the established coding conventions
- **Testing**: Add tests for new features and bug fixes
- **Documentation**: Update documentation for any API changes
- **Commit Messages**: Use clear, descriptive commit messages
- **Pull Requests**: Provide detailed descriptions of changes

### Areas Looking for Contributors
- 🔧 Device drivers (graphics, sound, network)
- 🎮 Game and multimedia applications
- 🌐 Web browser and internet applications
- 📚 Documentation and tutorials
- 🧪 Testing and quality assurance
- 🎨 UI/UX design and themes

## 📚 Documentation

### Technical Documentation
- **[Audio System](AUDIO_IMPLEMENTATION.md)** - Complete audio framework documentation
- **[GUI Framework](GUI_IMPLEMENTATION.md)** - Graphics and windowing system
- **[Scheduler](SCHEDULER_IMPLEMENTATION.md)** - Process scheduling and multitasking
- **[Memory Management](ADVANCED_MEMORY_MANAGEMENT.md)** - VMM and memory allocation
- **[Network Stack](NETWORK_STACK.md)** - TCP/IP implementation
- **[File Systems](VFS_IMPLEMENTATION.md)** - Virtual file system and storage

### User Documentation
- **[Getting Started](docs/getting-started.md)** - Installation and basic usage
- **[User Guide](docs/user-guide.md)** - Complete user manual
- **[Application Development](docs/app-development.md)** - Writing applications for IKOS
- **[System Administration](docs/system-admin.md)** - System configuration and management

### Developer Documentation
- **[Kernel API](docs/kernel-api.md)** - Kernel programming interface
- **[Driver Development](docs/driver-development.md)** - Writing device drivers
- **[Debugging Guide](docs/debugging.md)** - Debugging tools and techniques
- **[Testing Guide](docs/testing.md)** - Testing frameworks and procedures

## 🚀 Roadmap

### Short Term (Q1 2026)
- [ ] UEFI boot support implementation
- [ ] Advanced graphics acceleration
- [ ] Enhanced audio codec support
- [ ] Improved memory management
- [ ] Performance optimizations

### Medium Term (Q2-Q3 2026)
- [ ] SMP (symmetric multiprocessing) support
- [ ] Advanced networking features
- [ ] Package management system
- [ ] Container support
- [ ] Advanced security features

### Long Term (Q4 2026 and beyond)
- [ ] Virtualization support
- [ ] Advanced power management
- [ ] Hardware abstraction layer
- [ ] Mobile device support
- [ ] Cloud integration features

## 📄 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

```
MIT License

Copyright (c) 2025 IKOS Operating System Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
```

## 🙏 Acknowledgments

- **Contributors**: Thanks to all the developers who have contributed to IKOS
- **Community**: Special thanks to the open-source OS development community
- **Tools**: Built with amazing open-source tools like GCC, NASM, QEMU, and Git
- **Inspiration**: Inspired by classic operating systems and modern design principles

---

**IKOS Operating System** - Building the future of computing, one commit at a time! 🚀

*For questions, suggestions, or support, please open an issue or join our community discussions.*


