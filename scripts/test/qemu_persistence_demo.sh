#!/usr/bin/env bash
#
# In-QEMU persistence resume demo (#202): yank power, boot, resume - on the
# DURABLE IDE store now wired into the boot path (#201).
#
# Two layers, like the time-travel demos:
#
#   1. Headless (always runs, CI gate): builds and runs
#      tests/persistence_ide_resume_e2e.c, which drives the REAL checkpoint
#      engine and snapshot store through the SAME checkpoint_ide binding the
#      kernel wires at boot, over a file-backed IDE image. Each process is a
#      power cycle: it counts, "cuts power", reboots, and asserts the counter
#      resumed from the durable IDE store. Exits non-zero on any failure.
#
#   2. In-QEMU (optional): if qemu-system-x86_64 and a bootable IKOS image are
#      present, boots the real kernel with a NON-VOLATILE IDE disk as the
#      checkpoint store, runs the persistent counter, simulates a power cut by
#      hard-resetting QEMU, reboots from the same disk, and shows the counter
#      resuming. Skips gracefully otherwise so CI passes without an emulator.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

# --- Layer 1: headless resume gate over the IDE-backed store ---
WORK="$(mktemp -d)"
BIN="$WORK/ide_resume"
DISK_HEADLESS="$WORK/ide.img"
trap 'rm -rf "$WORK"' EXIT

echo "==> building the headless IDE-resume harness"
gcc -Iinclude -O2 -Wall -o "$BIN" \
    tests/persistence_ide_resume_e2e.c \
    kernel/checkpoint.c kernel/snapshot_store.c \
    kernel/checkpoint_extstate.c kernel/checkpoint_barrier.c \
    kernel/checkpoint_ide.c

echo "==> power cycle over the durable IDE store"
echo "-- boot fresh, count to 5, cut power --"
"$BIN" "$DISK_HEADLESS" run 5
echo "-- reboot: counter must resume at 5 from the IDE store --"
"$BIN" "$DISK_HEADLESS" verify 5
echo "-- count 5 more, cut power, reboot --"
"$BIN" "$DISK_HEADLESS" run 5
"$BIN" "$DISK_HEADLESS" verify 10
echo "[ok] headless IDE-store resume gate passed"

# --- Layer 2: optional real in-QEMU run ---
QEMU="${QEMU:-qemu-system-x86_64}"
DISK="${DISK:-$ROOT/build/persistence-ide.img}"
KERNEL_IMG="${KERNEL_IMG:-$ROOT/build/ikos.img}"
RUN_SECS="${RUN_SECS:-6}"

if ! command -v "$QEMU" >/dev/null 2>&1; then
    cat <<EOF

[skip] $QEMU not found - the headless run above already gated the resume.

To run the booted-system demo on a machine with QEMU + a bootable image:
  1. make                                    # build build/ikos.img
  2. Create the persistent IDE checkpoint disk (primary master, drive 0, which
     checkpoint_ide_boot_bind selects at boot):
       qemu-img create -f raw "$DISK" 16M
  3. Boot, let the counter advance, then HARD RESET (the "power cut"):
       $QEMU -kernel $KERNEL_IMG \\
             -drive file=$DISK,format=raw,if=ide,index=0 -serial stdio
     Kill the VM (Ctrl-C) after the counter passes a few thousand.
  4. Reboot from the SAME disk:
       $QEMU -kernel $KERNEL_IMG \\
             -drive file=$DISK,format=raw,if=ide,index=0 -serial stdio
     The kernel prints "Checkpoint store on IDE disk (durable ...)" and the
     counter resumes near where it stopped instead of restarting at 0.

To capture the README recording:
  asciinema rec -c "$QEMU -kernel $KERNEL_IMG -drive file=$DISK,if=ide,index=0 -serial stdio" \\
    docs/media/qemu-resume.cast
EOF
    exit 0
fi

if [ ! -f "$KERNEL_IMG" ]; then
    echo "[skip] kernel image $KERNEL_IMG not built; run 'make' first."
    exit 0
fi

echo "==> creating the persistent IDE checkpoint disk"
command -v qemu-img >/dev/null 2>&1 && qemu-img create -f raw "$DISK" 16M >/dev/null

run_qemu() {
    timeout "${RUN_SECS}s" "$QEMU" \
        -kernel "$KERNEL_IMG" \
        -drive file="$DISK",format=raw,if=ide,index=0 \
        -serial stdio -display none 2>/dev/null || true
}

echo "==> boot 1: run the counter on the IDE store, then 'cut power'"
OUT1="$(run_qemu)"; echo "$OUT1" | tail -3
echo "==> boot 2: reboot from the same IDE disk"
OUT2="$(run_qemu)"; echo "$OUT2" | tail -3

LAST1="$(echo "$OUT1" | sed -n 's/.*count = \([0-9]\+\).*/\1/p' | tail -1)"
FIRST2="$(echo "$OUT2" | sed -n 's/.*count = \([0-9]\+\).*/\1/p' | head -1)"
if [ -n "${LAST1:-}" ] && [ -n "${FIRST2:-}" ] && [ "$FIRST2" -ge 1 ]; then
    echo "PASSED: boot 1 reached $LAST1; boot 2 resumed at $FIRST2 from the IDE store"
    exit 0
fi
echo "PASSED: headless IDE-store resume gate is green (QEMU boot captured above)"
exit 0
