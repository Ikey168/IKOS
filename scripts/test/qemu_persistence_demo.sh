#!/usr/bin/env bash
#
# Orthogonal-persistence QEMU demo (#130): the real "yank power, it remembers".
#
# Unlike scripts/test/persistence_demo.sh (which proves the persistence modules
# headless over a file-backed disk), this boots the actual IKOS kernel in QEMU
# with a NON-VOLATILE disk image as the checkpoint store, runs the persistent
# counter, simulates a power cut by hard-resetting QEMU, reboots from the same
# image, and shows the counter resuming.
#
# Requirements (not present in every CI environment):
#   - qemu-system-x86_64
#   - a bootable IKOS image with persistence wired to an IDE store region
#     (checkpoint_ide_bind over the QEMU -drive), built by the top-level Makefile
#
# If QEMU is unavailable this script explains how to run it and exits 0 so it
# does not break CI on machines without an emulator.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
QEMU="${QEMU:-qemu-system-x86_64}"
DISK="${DISK:-$ROOT/build/persistence.img}"
KERNEL_IMG="${KERNEL_IMG:-$ROOT/build/ikos.img}"
RUN_SECS="${RUN_SECS:-6}"

if ! command -v "$QEMU" >/dev/null 2>&1; then
    cat <<EOF
[skip] $QEMU not found - cannot run the in-QEMU persistence demo here.

To run it on a machine with QEMU:
  1. Build a bootable IKOS image with persistence wired to an IDE store
     region (checkpoint_ide_bind over a QEMU -drive). See #130.
  2. Create a persistent disk image for the checkpoint store:
       qemu-img create -f raw "$DISK" 16M
  3. Boot, let the counter advance, then HARD RESET (the "power cut"):
       $QEMU -drive file=$KERNEL_IMG,format=raw,if=ide,index=0 \\
             -drive file=$DISK,format=raw,if=ide,index=1 -serial stdio
     Kill the VM (Ctrl-C) after the counter passes a few thousand.
  4. Reboot from the SAME images:
       $QEMU -drive file=$KERNEL_IMG,format=raw,if=ide,index=0 \\
             -drive file=$DISK,format=raw,if=ide,index=1 -serial stdio
     The counter resumes near where it stopped instead of restarting at 0.

To capture the README GIF:
  asciinema rec -c "$QEMU ... -serial stdio" persistence.cast
  agg persistence.cast docs/persistence-demo.gif
EOF
    exit 0
fi

if [ ! -f "$KERNEL_IMG" ]; then
    echo "[skip] kernel image $KERNEL_IMG not built; run 'make' first."
    exit 0
fi

# --- Real run (on a machine that has QEMU + a bootable image) ---
echo "==> creating checkpoint-store disk image"
command -v qemu-img >/dev/null 2>&1 && qemu-img create -f raw "$DISK" 16M >/dev/null

run_qemu() {
    timeout "${RUN_SECS}s" "$QEMU" \
        -drive file="$KERNEL_IMG",format=raw,if=ide,index=0 \
        -drive file="$DISK",format=raw,if=ide,index=1 \
        -serial stdio -display none 2>/dev/null || true
}

echo "==> boot 1: run the counter, then 'cut power'"
OUT1="$(run_qemu)"
echo "$OUT1" | tail -3

echo "==> boot 2: reboot from the same disk image"
OUT2="$(run_qemu)"
echo "$OUT2" | tail -3

LAST1="$(echo "$OUT1" | sed -n 's/.*count = \([0-9]\+\).*/\1/p' | tail -1)"
FIRST2="$(echo "$OUT2" | sed -n 's/.*count = \([0-9]\+\).*/\1/p' | head -1)"
if [ -n "${LAST1:-}" ] && [ -n "${FIRST2:-}" ] && [ "$FIRST2" -ge 1 ]; then
    echo "PASSED: boot 1 reached $LAST1; boot 2 resumed at $FIRST2 (not 0)"
    exit 0
fi
echo "FAILED: counter did not resume across the simulated power cut"
exit 1
