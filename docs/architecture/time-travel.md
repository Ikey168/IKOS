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

Every source that can make two runs diverge must be either recorded and replayed,
virtualized, or gated to a deterministic point.

| Source | How it leaks in | Strategy |
|--------|-----------------|----------|
| Interrupt and preemption timing | The scheduler preempts at hardware-timer edges that vary run to run | Record the logical point of each context switch; replay switches at the same points |
| Scheduler tick | Drives periodic checkpoints and time slicing | Fold into the recorded preemption sequence |
| Time and cycle reads (RDTSC, timers) | User or kernel code branches on wall-clock or cycle values | Record the value on the live run; return the recorded value on replay |
| Device input (keyboard, IDE completion IRQs, network) | External events arrive at arbitrary times | Record each event with its epoch and logical clock in the input journal |
| RNG and entropy | Seeds differ per boot | Capture seeds into the checkpoint so sequences replay identically |

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
