# Time-Travel Debugging: Deterministic Replay Core and MCP Interface

This document is the design home for turning IKOS from a system that remembers the last
moment into one that remembers every moment and lets you move through them. It covers two
plans:

1. The **Deterministic Replay Core**: make execution between checkpoints reproducible.
2. The **MCP Interface**: expose record, rewind, and reverse execution as tools an AI
   agent can call.

Both build on the existing checkpoint engine (`kernel/checkpoint.c`,
`kernel/checkpoint_barrier.c`, the scheduler-tick trigger, and the restore and
reconstruct path). See [orthogonal-persistence.md](orthogonal-persistence.md) for that
foundation. Work is tracked in the epic and its sub-issues on the issue tracker.

The deterministic replay core and the time-travel UX are implemented; the
["Implemented architecture"](#implemented-architecture) section below maps the design to
the modules that ship it, and `scripts/test/scrub_demo.sh` runs the whole pipeline
headlessly.

## Implemented architecture

The pipeline, from recording inputs to scrubbing the machine backward, and the module that
ships each piece:

| Stage | What it does | Modules |
|-------|--------------|---------|
| Input journal | Records the nondeterministic inputs between two keyframes (keystrokes, disk completions, timer/cycle reads, entropy), each tagged with epoch and a logical clock, in a CRC-protected double-buffered store | `kernel/checkpoint_journal.c` |
| Deterministic preemption | Records the logical point of every context switch on a live run and forces switches at the same points on replay | `kernel/sched_record.c` + the `scheduler_tick` seam |
| Virtualized time | Records RDTSC/timer reads and returns the recorded values on replay | `kernel/time_record.c`, `kernel/time_record_sync.c` |
| Deterministic entropy | Records entropy draws and returns the recorded bytes on replay | `kernel/entropy_record.c`, `kernel/entropy_record_sync.c` |
| Replay engine | Restores the nearest keyframe and re-drives forward to a target epoch plus offset | `kernel/replay_engine.c`, `kernel/replay_engine_sync.c` |
| Divergence detector | Checksums system state per epoch on record and replay, flagging any nondeterminism leak with the epoch and component | `kernel/divergence.c`, `kernel/divergence_sync.c` |
| Keyframe retention ring | Keeps the last N keyframes so rewind is not limited to the latest | `kernel/keyframe_ring.c` |
| Rewind-to | The core verb: nearest keyframe at or before the target, then replay to the target | `kernel/rewind.c`, `kernel/rewind_sync.c` |
| Reverse execution | reverse-step / reverse-continue as restore-prior-keyframe-and-replay | `kernel/reverse.c`, `kernel/reverse_sync.c` |
| Reverse breakpoints/watchpoints | Scan backward to the last hit or the last write to a value, bounded by the ring | `kernel/revbreak.c`, `kernel/revbreak_sync.c` |
| GDB bridge | Maps gdb `reverse-stepi` / `reverse-continue` (RSP bs/bc) onto the reverse engine | `kernel/gdbstub.c`, `kernel/gdbstub_sync.c` |

Each stage has a host-side unit test under `tests/` and a freestanding compile check in CI.
Two end-to-end demos tie them together, both headless: `scripts/test/timetravel_demo.sh`
records a session, persists it to the journal, replays it, and asserts the final state is
byte-identical; `scripts/test/scrub_demo.sh` records a session and then scrubs it backward
(rewind and reverse-step), showing the reconstructed state at each past moment matches the
recorded timeline. See also [reverse-debugging.md](../testing/reverse-debugging.md) for
driving it from gdb.

## Why this is tractable here

The checkpoint engine already produces periodic whole-system keyframes. A keyframe is a
consistent snapshot of every user address space plus the process table and scheduler
state. What it does not capture is the path between two keyframes: the exact sequence of
inputs and preemptions that led from one to the next.

If that path is made reproducible, then any past instant can be reconstructed by taking
the nearest keyframe at or before it and replaying forward. Reverse execution then falls
out for free: to step backward, restore the prior keyframe and replay to just before the
current point. A small purpose-built kernel is far easier to make fully deterministic
than a general-purpose OS, which is the core advantage IKOS has here.

## Plan 1: Deterministic Replay Core

Goal: two runs from the same keyframe, given the same recorded inputs, reach a
byte-identical state at every epoch boundary.

### Sources of nondeterminism

This section is the spike deliverable for the catalog. Every source that can make two runs
diverge must be either recorded and replayed, virtualized, or gated to a deterministic
point. Each entry below is grounded in the current code.

#### Spike findings (current code)

A read of the kernel established the facts that shape the plan:

- Preemption is driven by the PIT on IRQ0. `kernel/interrupts.c` programs the PIT
  (`setup_timer_interrupt`, ports 0x43 and 0x40) and routes IRQ0 to INT 32
  (`timer_interrupt_entry`). `scheduler_tick` in `kernel/scheduler.c` is the handler and
  does round-robin preemption when a task's `time_slice` reaches zero. Where each tick
  lands in the instruction stream is the dominant source of divergence.
- The checkpoint cadence is already tick-counted, not wall-clock. `checkpoint_tick`
  (`kernel/checkpoint.c:668`) increments `g_timer_ticks` and fires every
  `g_timer_interval` ticks. Keyframe epochs are therefore already deterministic once the
  tick sequence itself is deterministic; checkpoint timing needs no separate work.
- RDTSC is not yet a live source. `get_rdtsc` in `kernel/numa_allocator.c` is a stub that
  returns 0. It must be recorded or virtualized before any real RDTSC read is added, but
  it does not cause divergence today.
- The random MAC generator `eth_addr_random` in `kernel/net/ethernet.c` is actually
  hardcoded, so it is deterministic despite its name. Networking state is also severed on
  restore by the external-state policy, so net-path randomness does not affect persistence
  correctness.
- Real external entropy enters through `auth_generate_random` in `kernel/auth_core.c`,
  which reads `/dev/urandom`. This is a genuine nondeterministic source.
- The IDE driver polls rather than taking completion interrupts (`ide_wait_ready` and
  `ide_wait_drq` in `kernel/ide_driver.c` spin on the status register). Disk data content
  is deterministic; only the number of poll iterations varies, which perturbs timing.

#### Catalog and strategies

| Source | Where it enters | Strategy | Files touched |
|--------|-----------------|----------|---------------|
| Interrupt and preemption timing | PIT IRQ0 lands at a hardware-timed point in the instruction stream; `scheduler_tick` may preempt | Record the logical point of each context switch (an instruction or event count since the epoch); on replay, deliver the tick and preempt at the same point | `kernel/interrupt_stubs.asm`, `kernel/interrupts.c`, `kernel/scheduler.c` |
| Checkpoint cadence | `checkpoint_tick` fires on a tick count | Already deterministic given a deterministic tick sequence; no change beyond the preemption work | `kernel/checkpoint.c` (no change expected) |
| Keyboard input | Scancodes arrive via IRQ1 and are read and translated in `kernel/input_keyboard.c` | Record each scancode with its epoch and logical clock in the input journal; feed from the journal on replay | `kernel/input_keyboard.c`, `kernel/input_manager.c`, journal |
| External entropy (/dev/urandom) | `auth_generate_random` reads `/dev/urandom` | Record the returned bytes on the live run and return them on replay, or seed a recorded PRNG captured in the checkpoint | `kernel/auth_core.c`, journal |
| RDTSC and cycle reads (latent) | `get_rdtsc` stub today; any future real read branches on cycles | Record on the live run and return recorded values on replay; gate before a real RDTSC is introduced | `kernel/numa_allocator.c`, journal |
| IDE completion timing | Poll-loop iteration counts vary in `ide_wait_ready` and `ide_wait_drq` | Content is deterministic; keep polling from perturbing the logical clock, or drain completions at a fixed logical point | `kernel/ide_driver.c` |
| MAC and TLS randomness (not active) | `eth_addr_random` is hardcoded; net state is severed on restore | No action for persistence; if net replay is wanted later, record like other entropy | `kernel/net/*` (future) |

The input journal referenced above is the deliverable of the next issue and reuses the
double-buffered slot and CRC conventions in `kernel/checkpoint_disk.c`.

#### Outcome

This catalog fixed the scope of the deterministic replay core: the primary work was
deterministic preemption, the input journal for keyboard input and entropy, and a gate for
RDTSC before it goes live; the RDTSC stub, the hardcoded MAC, and severed network
randomness needed no work. Those pieces are now implemented, per the
["Implemented architecture"](#implemented-architecture) table above.

### Components

**Input journal.** A ring log written alongside each checkpoint slot that captures every
nondeterministic input, tagged with the epoch and a logical clock. It reuses the
double-buffered slot and CRC conventions in `kernel/checkpoint_disk.c`, and must survive a
crash as cleanly as the checkpoint slots: a crash before commit leaves the last good
journal intact. The journal is the delta that turns discrete keyframes into a continuous,
replayable timeline.

**Deterministic preemption.** Record the tick or instruction-count sequence at which
context switches occur, so replay switches contexts at the identical points a live run
did. This touches `kernel/interrupt_stubs.asm`, the timer and scheduler path, and the
existing tick-based checkpoint trigger. Preemption timing is the single largest divergence
risk; getting it right removes most replay drift.

**Time and entropy capture.** Trap or wrap RDTSC, timer counters, and RNG seeding so that
on replay they return recorded values rather than live hardware.

### Replay engine and verification

**Replay mode.** Boot from the nearest keyframe and apply the input journal forward to
reach an arbitrary target epoch. This reuses the existing restore plus process-table,
scheduler, and context reconstruction path. A target epoch is a checkpoint epoch plus an
offset into the journal.

**Divergence detector.** During replay, checksum system state at each epoch boundary and
compare against what was recorded live. Any mismatch is a nondeterminism leak the catalog
missed. Build this early and keep it on in debug builds; it is the harness that keeps the
whole feature honest.

**CI test.** A `scripts/test/timetravel_demo.sh` in the spirit of
`scripts/test/persistence_demo.sh`: run a session, replay it headless, and assert the
final state is byte-identical. This gates the milestone and guards against regressions.

### Time-travel UX

- **Keyframe retention ring.** Keep the last N checkpoints on disk, not just the
  double-buffered latest, so rewind targets are not limited to the most recent snapshot.
- **rewind-to.** Restore the nearest keyframe at or before a target epoch and replay to
  the exact target point.
- **Reverse execution.** `reverse-step` and `reverse-continue`, implemented as restore
  the prior keyframe and replay forward to just before the current point.
- **Reverse breakpoints and watchpoints.** Replay forward from the nearest keyframe until
  a condition is hit, to answer "where did this value last change?"

## Plan 2: MCP Interface

Goal: expose record, rewind, and reverse execution as Model Context Protocol tools so an
AI agent can drive a time-traveling debugger over a well-defined interface.

The interface changes who the customer is. Instead of a human who must adopt a new OS, the
consumer is an agent that calls tools. Agents are poor at exactly what this engine is good
at: reproducing a non-deterministic bug, stepping backward, and keeping the world stable
between runs. Running a program inside IKOS gives the agent a fully reproducible, rewindable
world it cannot get on a general-purpose OS.

### Tool surface

The MCP server is a thin wrapper over the replay engine. Proposed tools:

- `record_session(program)`: run the program under the input journal and return a session
  handle
- `list_checkpoints()`: list available keyframes and their epochs
- `diff_epochs(a, b)`: report what changed between two moments
- `rewind_to(epoch)`: restore the nearest keyframe and replay to the target
- `reverse_step()` and `reverse_continue()`: step or run backward
- `watch_last_write(addr_or_symbol)`: find the last write to an address or symbol
- `replay(from, to)`: deterministically replay a range

### Agent debugging flow

1. The agent runs suspect code with `record_session`.
2. On a failure, it calls `watch_last_write` or `diff_epochs` to localize the cause.
3. It uses `rewind_to` and `reverse_step` to inspect the exact moment the state went bad.
4. It proposes a fix, re-records, and confirms the failure no longer reproduces.

### Positioning and limits

- MCP is the interface, not the value. It exposes the replay engine; it does not create
  it. The Deterministic Replay Core must land first, or the tools return garbage.
- MCP is not a moat. Anyone can wrap an existing record-replay tool in MCP. The defensible
  asset is the deterministic-sandbox property that off-the-shelf engines cannot match.
- Scope it to code the agent runs inside IKOS: greenfield code, reproducible verification,
  embedded targets, and teaching. It is not a way to rewind an existing production
  deployment.

## Milestones

- **A. Deterministic replay core**: catalog nondeterminism, input journal, deterministic
  preemption, virtualized time and entropy.
- **B. Replay engine and verification**: replay mode plus the divergence detector, gated
  by a byte-identical CI test.
- **C. Time-travel UX**: keyframe retention, rewind-to, reverse execution, reverse
  breakpoints.
- **D. Tooling**: a GDB reverse-execution bridge.
- **E. MCP interface**: the tool surface above, plus a demo in which an agent debugs a
  planted heisenbug by rewinding the machine.

Suggested order: land the A chain first, since it proves the whole thesis before any UX,
GDB, or MCP work. Milestone E depends on C.
