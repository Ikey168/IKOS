# IKOS Operating System

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Orthogonal Persistence](https://github.com/Ikey168/IKOS/actions/workflows/persistence.yml/badge.svg)](https://github.com/Ikey168/IKOS/actions/workflows/persistence.yml)
[![Architecture](https://img.shields.io/badge/Architecture-x86__64-blue.svg)]()
[![Kernel](https://img.shields.io/badge/Kernel-Microkernel-orange.svg)]()

> **IKOS is a microkernel with orthogonal persistence: no save, no load, no shutdown.**
> The entire running system is the durable state. Pull the power cord, plug it back
> in, and your work is exactly where you left it, mid-instruction.

This is a real, proven idea (KeyKOS, EROS, Phantom OS, and IBM i's single-level store
in production) that no other hobby-tier microkernel implements. It is IKOS's reason to
exist, and it rides directly on the kernel's copy-on-write virtual memory.

## The headline: pull the plug, keep your work

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
[cycle 1] boot fresh, count to 5
  [run] counter now = 5 (then power is cut)
[reboot] counter must have survived at 5
  [verify] counter = 5  == expected 5  : REMEMBERED
[cycle 2] count 5 more (resumes from 5)
  [run] counter now = 10 (then power is cut)
[reboot] counter must have survived at 10
  [verify] counter = 10  == expected 10  : REMEMBERED
[crash test] cut power mid-run (kill -9), then reboot
  [crash] reloaded a valid checkpoint; counter = 3903 (>= 10, monotonic)
PASSED: the counter remembered itself across every power cycle
```

### How it works

A checkpoint marks every writable page read-only and copy-on-write (cheap; it copies
nothing). The first write to a marked page faults, and the kernel preserves the page's
pre-checkpoint contents before letting the write proceed. A background pass streams
those pages into the **inactive** one of two on-disk slots. A single superblock-sector
write flips the active slot and is the only commit point, so a crash before the flip
always leaves the previous checkpoint intact (CRC32 guards every slot). On boot, if a
valid checkpoint exists, the kernel reloads it instead of cold-starting.

Full design: [`docs/architecture/orthogonal-persistence.md`](docs/architecture/orthogonal-persistence.md).

### Try it

```bash
# Headless proof against the real checkpoint engine (no emulator needed):
./scripts/test/persistence_demo.sh

# The real thing in QEMU (boots IKOS, cuts power, reboots, resumes):
./scripts/test/qemu_persistence_demo.sh   # needs qemu-system-x86_64 + a built image
```

The checkpoint store can live on the volatile RAM disk (survives a warm reboot) or, for
true power-cut durability, on a real IDE disk via `checkpoint_ide_bind`.

## What actually works vs. roadmap

Being honest about maturity:

| Area | Status |
|------|--------|
| Orthogonal persistence: snapshot store (double-buffered slots + CRC) | Implemented, unit-tested |
| Checkpoint engine: stop-the-world COW marking, page-fault capture, writeback | Implemented, unit-tested |
| Restore + boot decision (restore vs cold boot) | Implemented, unit-tested |
| Periodic checkpoint trigger (scheduler tick) | Implemented, unit-tested |
| External-state policy (sockets/DMA/devices severed on restore) | Implemented, unit-tested |
| Context persistence + process-table/scheduler reconstruction on restore | Implemented, unit-tested |
| End-to-end "yank power" proof over a file-backed disk | Passing in CI |
| Boot store wired to a block device (RAM disk, volatile) | Implemented, unit-tested |
| IDE-backed durable store (survives a real power cut) | Wired into the boot path (prefers the IDE disk, falls back to the RAM disk); durability across a power cut unit-tested |
| Process register/scheduler-state resume actually executing | Table/context restore done; scheduler bridge + QEMU boot pending |
| Live boot/record/reverse-step end-to-end (headless CI gate + recording) | Passing in CI; QEMU boot layer runs when an image is present |
| Persisting kernel-internal and driver state | v2 (v1 cold-inits the kernel/drivers and restores user spaces on top) |

The broader subsystems below describe the project's overall scope; several are partial
or aspirational. The persistence stack is the part with end-to-end tests and CI today.

## Time-travel: scrub the machine backwards

Because the checkpoint engine already snapshots the whole system periodically, IKOS goes a
step further than "resume the last moment": it records the nondeterministic inputs between
keyframes (preemptions, timer and cycle reads, entropy) into a CRC-protected journal, so
any past moment can be reconstructed by restoring the nearest keyframe and replaying
forward. That makes the running system rewindable: `rewind-to <epoch>` lands at a past
moment, and reverse-step / reverse-continue walk it backward, driven from a normal gdb
session (`reverse-stepi`).

```bash
# Boot, record, reverse-step: the whole live stack (keyframe store, journal,
# divergence detector, MCP front end) drives an agent through rewind/reverse and
# confirms every reconstructed moment matches with no divergence leak.
./scripts/test/timetravel_live_demo.sh

# Headless proof: record a session, then scrub it backward and show every
# reconstructed past moment matches the recorded timeline.
./scripts/test/scrub_demo.sh

# Headless proof that a recorded session replays byte-identically:
./scripts/test/timetravel_demo.sh
```

Recording of the live boot -> record -> reverse-step run (asciicast, playable with
`asciinema play docs/media/timetravel-live.cast`):

```
=== In-QEMU-style time-travel end-to-end (#200) ===
[record] retained window: epochs 3..6 (last 4 of 6)
[agent] driving rewind/reverse over the MCP JSON-RPC loop
        rewound to epoch=5 offset=0  ->  stepped back to epoch=4  ->  epoch=3
[verify] epoch 3..6: state=match divergence=clean
  divergence detector reports NO leak over the live run
PASSED: booted, recorded, reverse-stepped, no leak
```

Full design and the module map: [`docs/architecture/time-travel.md`](docs/architecture/time-travel.md);
driving it from gdb: [`docs/testing/reverse-debugging.md`](docs/testing/reverse-debugging.md);
from an MCP client: [`docs/testing/mcp-server.md`](docs/testing/mcp-server.md).

## About

**IKOS** is a **microkernel-based operating system** for **x86/x86_64 consumer devices**
(desktops, laptops, embedded systems), organized around the orthogonal-persistence thesis
above. Alongside it the project includes:

- **Orthogonal Persistence** with periodic system checkpoints and resume-on-boot
- **Custom Multi-Stage Bootloader** with a real mode to long mode transition
- **Advanced Virtual Memory Manager (VMM)** with copy-on-write and demand paging
- **Message-Passing IPC** for secure inter-process communication
- **Audio System** scaffolding with AC97 codec support
- **GUI Framework** with window management and graphics
- **TCP/IP Network Stack** with a socket programming interface
- **Virtual File System (VFS)** targeting multiple filesystems
- **Security Framework** with authentication and authorization

IKOS is developed with a modular approach, enabling independent evolution of system
components while maintaining clean interfaces and robust integration.

## Key Concepts

### Microkernel Architecture
- **Minimal kernel space**: only essential services (IPC, scheduling, memory management)
- **User-space drivers**: device drivers and system services run in protected user space
- **Message-passing communication**: secure and efficient inter-process communication
- **Fault isolation**: a failed component does not crash the entire system

### Advanced Memory Management
- **Virtual Memory Manager (VMM)**: x86_64 paging with 4-level page tables
- **Copy-on-Write (COW)**: efficient memory sharing, process forking, and the basis of persistence
- **Demand Paging**: load pages only when needed
- **Memory Protection**: hardware-enforced isolation between processes

### Graphics and GUI
- **Framebuffer Driver**: direct graphics memory access
- **Window Manager**: windowing system with compositing
- **GUI Framework**: widget library for application development
- **Input Handling**: keyboard and mouse support

### Networking
- **TCP/IP Stack**: IPv4 networking implementation
- **Socket API**: Berkeley sockets compatible interface
- **Network Drivers**: Ethernet and Wi-Fi device support

### File System
- **Virtual File System (VFS)**: unified interface for multiple filesystems
- **FAT32 Support**: read/write support for FAT32
- **EXT2/EXT4 Support**: Linux-compatible filesystem support

### Security and Authentication
- **User Authentication**: authentication framework with multi-factor support
- **Authorization Framework**: role-based access control
- **Process Isolation**: hardware-enforced process boundaries

## Architecture Overview

IKOS follows a microkernel architecture with clear separation between kernel space and
user space:

```
+-----------------------------------------------------------------+
|                    User Applications                            |
|   GUI Apps      Terminal      File Manager      Web Browser     |
+-----------------------------------------------------------------+
|                    System Services                              |
|   File System   Network       Audio            Graphics        |
|   Service       Service       Service          Service         |
+-----------------------------------------------------------------+
|                    Device Drivers                               |
|   Storage       Network       Audio            Input           |
|   Drivers       Drivers       Drivers          Drivers         |
+-----------------------------------------------------------------+
|                      Microkernel                                |
|   IPC           Scheduler     VMM              Interrupts      |
|   Manager       Manager       Manager          Handler         |
+-----------------------------------------------------------------+
|                      Hardware Layer                             |
|   CPU           Memory        Storage          Peripherals     |
+-----------------------------------------------------------------+
```

### Communication Flow
- **System Calls**: user applications reach the kernel via the system call interface
- **Message Passing**: inter-process communication through secure message queues
- **Shared Memory**: high-performance data sharing between authorized processes
- **Interrupts**: hardware events handled by the kernel and forwarded to drivers

## Getting Started

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

1. Clone the repository:
   ```bash
   git clone https://github.com/Ikey168/IKOS.git
   cd IKOS
   ```

2. Build the complete system:
   ```bash
   make all
   ```

3. Test the ELF-loading bootloader:
   ```bash
   make test-elf-compact
   ```

4. Run specific components:
   ```bash
   # Audio system
   cd kernel && make audio-system && make audio-test

   # GUI system
   make gui-system && ./test_gui.sh

   # Network stack
   make network-test
   ```

### Build Targets

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

## Testing and Debugging

### Running Tests
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
./scripts/test/test_paging.sh            # Paging system tests
```

### Debugging with QEMU and GDB
```bash
# Start QEMU with a GDB server
make debug

# In another terminal, connect GDB
gdb kernel/kernel.elf
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

## Project Structure

```
IKOS/
  boot/                  Multi-stage bootloader implementations
    boot.asm             Basic real mode bootloader
    boot_enhanced.asm    Enhanced bootloader with memory detection
    boot_longmode.asm    Long mode (64-bit) bootloader
    boot_elf_loader.asm  ELF kernel loading bootloader
    boot.ld              Bootloader linker script
  kernel/                Microkernel implementation
    checkpoint*.c        Orthogonal persistence: engine, store, restore
    scheduler.c          Process scheduler
    interrupts.c         Interrupt handling
    vmm.c                Virtual memory manager
    ipc.c                Inter-process communication
    gui.c                GUI framework
    framebuffer.c        Graphics framebuffer driver
    network_driver.c     Network stack implementation
    Makefile             Kernel build system
  include/               System headers and definitions
  tests/                 Component and integration test suites
  user/                  User-space applications and demos
  docs/                  Architecture, implementation, and testing docs
  scripts/               Build, test, validation, and debug scripts
  tools/                 Development tools and utilities
  examples/              Demo scripts and examples
  Makefile               Main build system
```

## Roadmap

The near-term focus is the persistence stack and the capabilities that build on it.

### Time-Travel Debugging

The checkpoint engine produces periodic whole-system keyframes; a deterministic input
journal between keyframes makes execution reproducible, which turns "remember the last
moment" into "remember every moment, and move through them" (see the
[time-travel section](#time-travel-scrub-the-machine-backwards) above). Full design and the
module map: [`docs/architecture/time-travel.md`](docs/architecture/time-travel.md).

Status:
- Deterministic replay core (nondeterminism catalog, input journal, deterministic
  preemption, virtualized time and entropy): implemented, unit-tested
- Replay engine plus a divergence detector that proves a replay is byte-exact: implemented,
  unit-tested, with a headless byte-identical end-to-end demo
- Time-travel UX (keyframe retention, rewind-to, reverse execution, reverse
  breakpoints/watchpoints): implemented, unit-tested, with a headless scrub-backwards demo
- Tooling: a GDB reverse-execution bridge (implemented, unit-tested)
- MCP interface: expose record, rewind, and reverse execution as JSON-RPC tools an AI
  agent can call (implemented, unit-tested), with a headless demo where an agent debugs a
  planted heisenbug by rewinding the machine

### Systems Roadmap

Short term:
- [x] IDE-backed durable store wired into the in-kernel boot path
- [ ] In-QEMU boot demo for the persistence resume
- [ ] UEFI boot support
- [ ] Performance optimizations

Medium term:
- [ ] SMP (symmetric multiprocessing) support
- [ ] Persisting kernel-internal and driver state (persistence v2)
- [ ] Broader networking features
- [ ] Package management

Long term:
- [ ] Container and virtualization support
- [ ] Advanced power management
- [ ] Hardware abstraction layer

## Contributing

Contributions are welcome from developers of all skill levels.

### Development Setup
1. Fork and clone the repository:
   ```bash
   git clone https://github.com/yourusername/IKOS.git
   cd IKOS
   ```

2. Set up the development environment:
   ```bash
   make install-deps    # Install development dependencies
   make all             # Build the complete system
   make test            # Run the test suite
   ```

3. Create a feature branch:
   ```bash
   git checkout -b feature/your-feature-name
   ```

### Contribution Guidelines
- **Code Style**: follow the established coding conventions
- **Testing**: add tests for new features and bug fixes
- **Documentation**: update documentation for any API changes
- **Commit Messages**: use clear, descriptive commit messages
- **Pull Requests**: provide detailed descriptions of changes

### Areas Looking for Contributors
- The persistence stack: the in-QEMU boot demo on real hardware images
- Time-travel debugging: deterministic replay and the divergence detector
- Device drivers (graphics, sound, network)
- Documentation and tutorials
- Testing and quality assurance

## Documentation

### Architecture
- [Orthogonal Persistence](docs/architecture/orthogonal-persistence.md)
- [Virtual Memory Architecture](docs/architecture/VMM_README.md)
- [Device Driver Framework](docs/architecture/DEVICE_DRIVER_FRAMEWORK.md)
- [Network Stack](docs/architecture/NETWORK_STACK.md)
- [TCP/IP Implementation](docs/architecture/TCPIP.md)

### Implementation and Testing
- [Audio System](docs/implementation/AUDIO_IMPLEMENTATION.md)
- [GUI Framework](docs/implementation/GUI_IMPLEMENTATION.md)
- [Scheduler](docs/implementation/SCHEDULER_IMPLEMENTATION.md)
- [Memory Management](docs/implementation/VMM_IMPLEMENTATION_SUMMARY.md)
- [Hardware Testing Guide](docs/testing/QEMU_REAL_HARDWARE_TEST.md)
- [Runtime Kernel Debugger](docs/testing/RUNTIME_KERNEL_DEBUGGER.md)

## License

This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for
the full text.

## Acknowledgments

- **Contributors**: thanks to everyone who has contributed to IKOS
- **Community**: thanks to the open-source OS development community
- **Tools**: built with GCC, NASM, QEMU, and Git
- **Inspiration**: the orthogonal-persistence lineage of KeyKOS, EROS, Phantom OS, and
  IBM i's single-level store

For questions, suggestions, or support, please open an issue or start a discussion.
