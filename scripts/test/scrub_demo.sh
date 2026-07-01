#!/usr/bin/env bash
#
# "Scrub the machine backwards" demo / test (#173, epic #159).
#
# Records a session, then scrubs it backward through the whole time-travel stack
# and shows the reconstructed state at each past moment matching what it was at
# that time:
#   1. record a session that folds nondeterministic time (#163) and entropy
#      (#164) inputs into its state, taking a keyframe per epoch (#168);
#   2. rewind (#169) to several past (epoch, offset) points via the replay
#      engine (#165) and verify the reconstructed state;
#   3. reverse-step (#170) backward and show the state at each earlier moment,
#      including across keyframe boundaries.
#
# Headless (no emulator). Exits non-zero on any mismatch, so it gates CI. To
# capture the README GIF, screen-record this script's output.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CC="${CC:-gcc}"
WORK="$(mktemp -d)"
BIN="$WORK/scrub"
trap 'rm -rf "$WORK"' EXIT

echo "==> building scrub-backwards demo"
"$CC" -I"$ROOT/include" -O2 -Wall -o "$BIN" \
    "$ROOT/tests/scrub_e2e.c" \
    "$ROOT/kernel/time_record.c" \
    "$ROOT/kernel/entropy_record.c" \
    "$ROOT/kernel/replay_engine.c" \
    "$ROOT/kernel/keyframe_ring.c" \
    "$ROOT/kernel/rewind.c" \
    "$ROOT/kernel/reverse.c"

echo
"$BIN"

echo
echo "PASSED: the machine scrubbed backward to every recorded moment"
