#!/usr/bin/env bash
#
# Orthogonal-persistence end-to-end demo / test (#119).
#
# Proves "yank power, it remembers" against the REAL checkpoint engine and
# on-disk snapshot store, using a file-backed disk image. Each invocation of the
# harness is a fresh process: the only thing that survives between them is the
# disk file, so re-running it models a power cycle. The script:
#   1. boots fresh, counts to 5, "cuts power"
#   2. reboots, asserts the counter resumed at 5
#   3. counts 5 more, "cuts power"
#   4. reboots, asserts the counter resumed at 10
#   5. cuts power MID-checkpoint (kill -9) and asserts the image is still a
#      valid checkpoint (crash consistency), counter unchanged-or-advanced.
#
# Exits non-zero on any failure, so it doubles as a CI gate.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CC="${CC:-gcc}"
WORK="$(mktemp -d)"
BIN="$WORK/e2e"
DISK="$WORK/disk.img"
trap 'rm -rf "$WORK"' EXIT

echo "==> building end-to-end harness"
"$CC" -I"$ROOT/include" -O2 -Wall -o "$BIN" \
    "$ROOT/tests/test_persistence_e2e.c" \
    "$ROOT/kernel/checkpoint.c" \
    "$ROOT/kernel/snapshot_store.c"

echo
echo "==> power cycle 1: boot fresh, count to 5"
"$BIN" "$DISK" run 5

echo "==> reboot: counter must have survived at 5"
"$BIN" "$DISK" verify 5

echo
echo "==> power cycle 2: count 5 more (resumes from 5)"
"$BIN" "$DISK" run 5

echo "==> reboot: counter must have survived at 10"
"$BIN" "$DISK" verify 10

echo
echo "==> crash test: cut power MID-RUN (kill -9), then reboot"
# Large run; kill it abruptly partway so a checkpoint is interrupted.
"$BIN" "$DISK" run 100000 >/dev/null 2>&1 &
RUN_PID=$!
sleep 0.2
kill -9 "$RUN_PID" 2>/dev/null || true
wait "$RUN_PID" 2>/dev/null || true

# The image must still be a valid checkpoint (not corrupt) and the counter must
# be at least 10 (never went backwards). 'show' loads the last committed
# checkpoint and prints it, returning 0 on a successful load.
AFTER="$("$BIN" "$DISK" show 2>&1 | sed -n 's/.*counter = \([0-9]\+\).*/\1/p' | head -1)"
if [ -z "$AFTER" ]; then
    echo "FAIL: image unreadable after mid-run power cut"
    exit 1
fi
if [ "$AFTER" -lt 10 ]; then
    echo "FAIL: counter went backwards after crash ($AFTER < 10)"
    exit 1
fi
echo "  [crash] reloaded a valid checkpoint; counter = $AFTER (>= 10, monotonic)"

echo
echo "PASSED: the counter remembered itself across every power cycle"
