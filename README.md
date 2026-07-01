# IKOS

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Orthogonal Persistence](https://github.com/Ikey168/IKOS/actions/workflows/persistence.yml/badge.svg)](https://github.com/Ikey168/IKOS/actions/workflows/persistence.yml)
[![Architecture](https://img.shields.io/badge/Architecture-x86__64-blue.svg)]()
[![Kernel](https://img.shields.io/badge/Kernel-Microkernel-orange.svg)]()

> **IKOS is a time-traveling debugger built as an operating system.** Record a
> session, then scrub the whole machine backward: step back from the crash,
> run backward to a breakpoint, or ask where a value was last written and land
> there.

Heisenbugs die on the operating table: rerun the program and the schedule
shifts, the timer reads change, the entropy differs, and the crash is gone.
IKOS attacks that at the root. The OS records every nondeterministic input as
it runs, so any past moment of the whole machine can be reconstructed exactly,
as many times as you need. The bug cannot escape into a different interleaving,
because the interleaving is part of the recording.

Two front ends drive it:

- **gdb**: `reverse-stepi` / `reverse-continue` against the running OS over the
  serial port, using gdb's native reverse-execution protocol.
- **MCP**: `list_checkpoints`, `rewind_to`, `reverse_step`, and
  `watch_last_write` exposed as JSON-RPC tools, so an AI agent can drive the
  machine backward. Agents are bad at exactly what this is good at: reproducing
  a flaky bug and keeping the world stable between attempts. IKOS turns an
  agent into a time-traveling debugger.

## See it

The live end-to-end run: boot the stack, record a session, then drive
rewind/reverse over the MCP loop and verify every reconstructed moment,
byte-exact:

```
=== In-QEMU-style time-travel end-to-end (#200) ===
[record] retained window: epochs 3..6 (last 4 of 6)
[agent] driving rewind/reverse over the MCP JSON-RPC loop
        rewound to epoch=5 offset=0  ->  stepped back to epoch=4  ->  epoch=3
[verify] epoch 3..6: state=match divergence=clean
  divergence detector reports NO leak over the live run
PASSED: booted, recorded, reverse-stepped, no leak
```

(Recording: `asciinema play docs/media/timetravel-live.cast`.)

All the demos run headless against the real modules and gate CI:

```bash
# Boot, record, reverse-step: keyframe store + journal + divergence detector +
# MCP front end, with every reconstructed moment verified leak-free:
./scripts/test/timetravel_live_demo.sh

# An AI agent debugs a planted heisenbug by rewinding the machine:
./scripts/test/mcp_heisenbug_demo.sh

# Record a session, scrub it backward, and match every past moment:
./scripts/test/scrub_demo.sh

# A recorded session replays byte-identically:
./scripts/test/timetravel_demo.sh
```

## Using the debugger

From gdb, over the serial port (the stub advertises `ReverseStep+` /
`ReverseContinue+`, so gdb's stock reverse commands just work):

```
(gdb) target remote <serial>
(gdb) reverse-stepi      # one step back: restore the prior keyframe, replay to
                         # just before the current point
(gdb) reverse-continue   # run backward to the previous stop / oldest keyframe
```

From an MCP client, as newline-delimited JSON-RPC (one response line per
request):

```
{"jsonrpc":"2.0","id":1,"method":"tools/list"}
{"id":2,"method":"tools/call","params":{"name":"rewind_to","arguments":{"epoch":35,"offset":2}}}
{"id":3,"method":"tools/call","params":{"name":"reverse_step","arguments":{}}}
{"id":4,"method":"tools/call","params":{"name":"watch_last_write","arguments":{}}}
```

How-tos: [gdb reverse debugging](docs/testing/reverse-debugging.md) and
[driving time-travel from MCP](docs/testing/mcp-server.md).

## Why the debugger is an operating system

Record/replay debuggers exist: rr records single Linux processes from the
outside, and emulator-level record/replay (QEMU, Simics) records a virtual
machine from underneath. IKOS moves the recorder inside: time travel is an OS
service. The kernel owns its keyframes, its input journal, its replay engine,
and a divergence detector that checksums system state every epoch on both runs
to prove each reconstruction is byte-exact. Nothing is instrumented and no
emulator sits in the loop; the unit of replay is the whole machine, kernel and
schedule included.

It works like film: a **keyframe** (a whole-system checkpoint) every interval,
and between keyframes a CRC-protected **journal** of every nondeterministic
input, each entry stamped with a logical clock:

- preemption points: the exact logical moment of every context switch
- timer and cycle reads (RDTSC virtualized on replay)
- entropy draws

Restoring the nearest keyframe and re-driving execution with the journaled
inputs reconstructs any `(epoch, offset)` in the recorded window. Reverse-step
is "restore the prior keyframe, replay to just before here"; a reverse
watchpoint scans backward for the last write. The retention ring keeps the last
N keyframes on disk (the rewind horizon), and its index survives a reboot.

Full design and the module map:
[`docs/architecture/time-travel.md`](docs/architecture/time-travel.md).

## The substrate: orthogonal persistence

The recorder falls out of a stranger property: IKOS is a persistent OS. There
is no save, no load, and no shutdown; the running system is the durable state.
Pull the power cord, plug it back in, and your work is where you left it. The
periodic whole-system checkpoints that make that true are exactly the keyframes
time travel needs, which is why the debugger could be built as an OS at all.
(The lineage is real: KeyKOS, EROS, Phantom OS, IBM i's single-level store. No
other hobby-tier microkernel implements it.)

A program does **nothing special** to be durable. The counter below has no
`save()`, no `load()`, no serialization:

```c
/* user/persistent_counter.c - the entire persistence "logic" */
uint64_t counter = 0;
for (;;) { counter++; print_u64(counter); yield(); }
```

The end-to-end demo (`scripts/test/persistence_demo.sh`) proves it against the
real checkpoint engine and on-disk store, modeling a power cut as a process
restart:

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

How: a checkpoint marks every writable page read-only and copy-on-write (cheap;
it copies nothing). The first write to a marked page faults, and the kernel
preserves the pre-checkpoint contents before letting the write proceed. A
background pass streams those pages into the **inactive** one of two on-disk
slots; a single superblock-sector write flips the active slot and is the only
commit point, so a crash mid-checkpoint always leaves the previous one intact
(CRC32 guards every slot). On boot, a valid checkpoint is reloaded instead of
cold-starting.

```bash
# Headless proof against the real checkpoint engine (no emulator needed):
./scripts/test/persistence_demo.sh

# Yank power, boot, resume - on the durable IDE store. Runs a headless resume
# gate over the same IDE binding the kernel wires at boot (always), then the
# booted-system QEMU run when an emulator + image are present:
./scripts/test/qemu_persistence_demo.sh
```

The checkpoint store lives on a real IDE disk when one is present (true
power-cut durability; the kernel binds it at boot and falls back to the
volatile RAM disk otherwise). Recording of the yank-power/boot/resume cycle:
`asciinema play docs/media/qemu-resume.cast`. Full design:
[`docs/architecture/orthogonal-persistence.md`](docs/architecture/orthogonal-persistence.md).

## What actually works vs. roadmap

Being honest about maturity. The debugging stack:

| Layer | Status |
|-------|--------|
| Deterministic replay core: input journal, deterministic preemption, virtualized time, deterministic entropy | Implemented, unit-tested |
| Live capture: every checkpoint commit journals its epoch's inputs and divergence checksums | Implemented, unit-tested |
| Keyframe retention store: last N checkpoints across on-disk regions, index survives reboot | Implemented, unit-tested |
| Replay driver: land the booted system at an arbitrary (epoch, offset) | Implemented, unit-tested; the live journal retains the latest epoch (a deeper journal ring is future work) |
| Divergence detector fed by real component checksums (process table, scheduler) | Implemented, unit-tested |
| Rewind-to, reverse-step/continue, reverse breakpoints and watchpoints | Implemented, unit-tested |
| gdb front end: RSP stub (bs/bc) + serial transport loop | Implemented, unit-tested |
| MCP front end: JSON-RPC tools + server loop | Implemented, unit-tested |
| End-to-end: byte-identical replay, scrub-backwards, agent heisenbug hunt, live boot/record/reverse-step | Passing headlessly in CI; the in-QEMU layer runs when an emulator + image are present |

And the persistence substrate it rides on:

| Layer | Status |
|-------|--------|
| Snapshot store: double-buffered slots + CRC, single superblock-flip commit | Implemented, unit-tested |
| Checkpoint engine: stop-the-world COW marking, page-fault capture, background writeback | Implemented, unit-tested |
| Restore + boot decision; context and process-table reconstruction | Implemented, unit-tested |
| Periodic trigger + external-state policy (sockets/DMA/devices severed on restore) | Implemented, unit-tested |
| IDE-backed durable store wired into the boot path (falls back to the RAM disk) | Implemented; durability + counter resume across a power cut gated headlessly in CI, with a scripted QEMU booted-system demo |
| Process register/scheduler-state resume actually executing | Table/context restore done; scheduler bridge + QEMU boot pending |
| Kernel-internal and driver state | v2 design (v1 cold-inits the kernel/drivers and restores user spaces on top) |

The broader subsystems below describe the project's overall scope; several are
partial or aspirational. The debugging and persistence stacks are the parts
with end-to-end tests and CI today.

## About

The machine under the microscope is **IKOS** itself: a **microkernel-based operating
system** for **x86/x86_64 consumer devices** (desktops, laptops, embedded systems),
organized around the persistence-and-replay thesis above. Alongside it the project
includes:

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

The time-travel debugging stack and the persistence substrate it rides on are the
mature parts of IKOS, with unit tests and CI demos (see the sections above). The items
below are the broader OS scaffolding, in rough priority order.

Recently landed: the IDE-backed durable store wired into the boot path, the in-QEMU
persistence-resume demo, and the full live time-travel stack (see the maturity table
and the time-travel section above).

Short term:
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

Full index: [`docs/README.md`](docs/README.md).

### Headline features
- [Orthogonal persistence (design)](docs/architecture/orthogonal-persistence.md) and
  [persistence v2](docs/architecture/orthogonal-persistence-v2.md)
- [Time-travel debugging (design + module map)](docs/architecture/time-travel.md)
- [Reverse debugging with gdb](docs/testing/reverse-debugging.md)
- [Driving time-travel from an MCP client](docs/testing/mcp-server.md)

### Architecture and subsystems
- [Virtual Memory Manager](docs/architecture/VMM_README.md)
- [Device Driver Framework](docs/architecture/DEVICE_DRIVER_FRAMEWORK.md)
- [Network Stack](docs/architecture/NETWORK_STACK.md) and [TCP/IP](docs/architecture/TCPIP.md)
- Per-subsystem reference notes in [`docs/implementation/`](docs/implementation/)

### Testing and debugging
- [Bootloader and system testing guide](docs/TESTING.md)
- [QEMU and real-hardware testing](docs/testing/QEMU_REAL_HARDWARE_TEST.md)
- [Runtime kernel debugger](docs/testing/RUNTIME_KERNEL_DEBUGGER.md)

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
