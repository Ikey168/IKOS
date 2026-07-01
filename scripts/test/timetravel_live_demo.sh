#!/usr/bin/env bash
#
# In-QEMU-style time-travel end-to-end demo (#200): boot, record, reverse-step.
#
# Two layers, mirroring the other time-travel demos:
#
#   1. Headless (always runs): builds and runs tests/timetravel_live_e2e.c, which
#      wires the REAL merged modules - keyframe retention store (#195), input
#      journal (#194), divergence detector (#197), and the MCP front end (#199) -
#      to boot, record a session, drive rewind/reverse over the MCP JSON-RPC
#      loop, and confirm every reconstructed moment matches with NO divergence
#      leak. Exits non-zero on any mismatch, so it is a CI gate.
#
#   2. In-QEMU (optional): if qemu-system-x86_64 and a bootable IKOS image are
#      present, boots the real kernel (which arms the same stack at boot) and
#      captures the serial transcript. Skips gracefully otherwise so CI passes on
#      machines without an emulator.
#
# The headless transcript is what the README time-travel recording captures; see
# docs/media/timetravel-live.cast.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

BIN="$(mktemp -d)/timetravel_live_e2e"

echo "==> building the headless live end-to-end harness"
gcc -Iinclude -Wall -o "$BIN" \
    tests/timetravel_live_e2e.c \
    kernel/keyframe_store.c kernel/snapshot_store.c kernel/keyframe_ring.c \
    kernel/journal_capture.c kernel/checkpoint_journal.c \
    kernel/divergence.c kernel/divergence_scan.c \
    kernel/mcp.c kernel/mcp_server.c

echo "==> running: boot, record, reverse-step, verify no divergence"
"$BIN"
RC=$?
if [ $RC -ne 0 ]; then
    echo "FAILED: headless live end-to-end run reported a mismatch"
    exit $RC
fi

# --- Optional in-QEMU layer ---
QEMU="${QEMU:-qemu-system-x86_64}"
KERNEL_IMG="${KERNEL_IMG:-$ROOT/build/ikos.img}"
if ! command -v "$QEMU" >/dev/null 2>&1; then
    cat <<EOF

[skip] $QEMU not found - the headless run above already proved the stack.

To capture the in-QEMU run on a machine with QEMU + a bootable image:
  make                                   # build build/ikos.img
  $QEMU -drive file=$KERNEL_IMG,format=raw -serial stdio -display none
  # IKOS arms the keyframe store, journal, divergence detector, and the gdb
  # (COM1) + MCP (COM2) front ends at boot. Drive reverse-stepi from gdb or
  # rewind_to from an MCP client, then read back the divergence "no leak" line.

To record the README asciicast:
  asciinema rec -c "$BIN" docs/media/timetravel-live.cast
EOF
    exit 0
fi

if [ ! -f "$KERNEL_IMG" ]; then
    echo "[skip] kernel image $KERNEL_IMG not built; run 'make' first."
    exit 0
fi

echo "==> booting IKOS in QEMU (arms the same stack at boot)"
timeout "${RUN_SECS:-8}s" "$QEMU" \
    -drive file="$KERNEL_IMG",format=raw -serial stdio -display none 2>/dev/null | tail -20 || true
echo "PASSED: headless live end-to-end run is green (QEMU boot captured above)"
exit 0
