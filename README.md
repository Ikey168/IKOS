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

## The machine

The system being recorded is IKOS's own microkernel for x86/x86_64 consumer
devices. Two of its properties are what make the debugger work:

- **Copy-on-write virtual memory.** A keyframe is a COW marking pass: mark every
  writable page read-only, copy nothing, and capture a page lazily on its first
  write. That makes whole-system checkpoints cheap enough to take on a timer,
  which is what makes "a keyframe every interval" affordable.
- **A small, message-passing microkernel.** The kernel surface that must be
  recorded and reconstructed (scheduler, process table, IPC) is small enough to
  checksum every epoch and reason about deterministically. Replay is
  deterministic on a single CPU today; SMP-safe replay is future work.

Around that core the project carries the scaffolding of a general-purpose OS, in
varying states of maturity (see the table above): a custom multi-stage
bootloader (real mode to long mode), a VFS with FAT support, a framebuffer and
GUI stack, a TCP/IP stack with a sockets API, audio scaffolding, and an
authentication framework.

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

The recorder lives in the microkernel layer: the checkpoint engine, the input
journal, and the record/replay wrappers sit beside the scheduler and the VMM.
That placement is why the unit of replay is the whole machine, kernel and
schedule included, rather than a single process.

## Getting started

### Prerequisites

A Unix-like environment with `nasm`, `gcc`, `make`, `git`, and (to boot the OS)
`qemu-system-x86`:

```bash
# Ubuntu/Debian
sudo apt-get install -y nasm gcc make qemu-system-x86 build-essential git
# Fedora/CentOS/RHEL
sudo dnf install -y nasm gcc make qemu-system-x86 gcc-c++ git
# macOS (Homebrew)
brew install nasm gcc make qemu git
```

### Run the debugger first (no OS build required)

The entire debugging stack is host-testable: every demo compiles the real
kernel modules with the host `gcc` and drives them headlessly, so you can watch
record / rewind / reverse work before ever booting the OS:

```bash
git clone https://github.com/Ikey168/IKOS.git && cd IKOS

./scripts/test/timetravel_live_demo.sh   # boot, record, reverse-step, verify
./scripts/test/mcp_heisenbug_demo.sh     # an agent hunts a heisenbug by rewinding
./scripts/test/persistence_demo.sh       # yank power, it remembers
```

### Build and boot the OS

```bash
make all       # bootloader + kernel image
make test      # comprehensive test suite
make debug     # boot in QEMU with a gdb server attached
```

### Debug it, forward and backward

Forward debugging uses QEMU's gdb server as usual:

```bash
make debug
gdb kernel/kernel.elf
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

Backward debugging talks to IKOS's own stub instead, over the serial port
(COM1); the stub owns the machine's recorded history, which QEMU's gdb server
knows nothing about. The MCP server listens on COM2 so the two front ends never
collide. See [Using the debugger](#using-the-debugger) above and
[`docs/testing/reverse-debugging.md`](docs/testing/reverse-debugging.md).

### Build targets

| Target | Description |
|--------|-------------|
| `make all` | Build complete IKOS system |
| `make bootloader` | Build all bootloader variants |
| `make kernel` | Build kernel components |
| `make test` | Run comprehensive test suite |
| `make debug` | Boot in QEMU with a gdb server |
| `make clean` | Clean all build artifacts |

## Testing

CI runs the debugger's whole verification pyramid on every push
(`.github/workflows/persistence.yml`): host-side unit tests for every module,
freestanding compile checks proving the same sources build into the kernel, and
the headless end-to-end demos as gates:

```bash
make test                                 # complete system test

./scripts/test/timetravel_live_demo.sh    # boot, record, reverse-step, no leak
./scripts/test/mcp_heisenbug_demo.sh      # agent rewinds to find a planted bug
./scripts/test/scrub_demo.sh              # scrub backwards, match every moment
./scripts/test/timetravel_demo.sh         # byte-identical replay
./scripts/test/persistence_demo.sh        # power-cycle resume (file-backed disk)
./scripts/test/qemu_persistence_demo.sh   # power-cut resume on the IDE store
```

Component test scripts for the broader OS (audio, GUI, input, USB, memory,
paging, real hardware) live under `scripts/test/`.

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
    checkpoint*.c        Checkpoint engine, snapshot store, restore (keyframes)
    *_record*.c          Record/replay wrappers: preemption, time, entropy
    journal_capture*.c   Per-epoch input journal, written beside each checkpoint
    keyframe_*.c         Keyframe retention ring + N-deep on-disk store
    replay_*.c           Replay engine + driver (land at any epoch/offset)
    rewind*.c reverse*.c revbreak*.c   The time-travel verbs
    divergence*.c        Byte-exact replay verification
    gdbstub*.c gdb_serial*.c   gdb reverse-execution front end (serial)
    mcp*.c               MCP JSON-RPC front end for AI agents
    scheduler.c          Process scheduler (the deterministic-preemption seam)
    vmm.c                Virtual memory manager (COW: what makes keyframes cheap)
    interrupts.c ipc.c gui.c framebuffer.c network_driver.c ...   The OS proper
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
- Deepen the journal: a multi-epoch journal ring (the live journal retains the
  latest epoch today)
- The live in-QEMU run: drive `reverse-stepi` from a real gdb against a booted image
- More divergence components (user pages, file table, IPC) to tighten the
  byte-exact guarantee
- SMP-safe deterministic replay
- Device drivers, documentation, and testing for the broader OS

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
