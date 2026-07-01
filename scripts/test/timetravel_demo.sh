#!/usr/bin/env bash
#
# Time-travel end-to-end demo / test (#167, epic #159).
#
# Proves the deterministic-replay property against the REAL replay stack,
# headless (no emulator), the way persistence_demo.sh proves durability. The
# harness:
#   1. records a session whose result depends on nondeterministic inputs
#      (time reads #163, entropy draws #164), capturing those inputs;
#   2. persists the captured inputs to the input journal (#161);
#   3. replays the session via the replay engine (#165), reloading the inputs
#      from the journal;
#   4. asserts the replayed final state is BYTE-IDENTICAL to the recording, and
#      the divergence detector (#166) confirms no nondeterminism leak;
#   5. checks that a fresh live run with different inputs differs, so the match
#      is meaningful.
#
# Exits non-zero on any failure, so it doubles as a CI gate.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CC="${CC:-gcc}"
WORK="$(mktemp -d)"
BIN="$WORK/timetravel_e2e"
trap 'rm -rf "$WORK"' EXIT

echo "==> building time-travel end-to-end harness"
"$CC" -I"$ROOT/include" -O2 -Wall -o "$BIN" \
    "$ROOT/tests/timetravel_e2e.c" \
    "$ROOT/kernel/time_record.c" \
    "$ROOT/kernel/entropy_record.c" \
    "$ROOT/kernel/replay_engine.c" \
    "$ROOT/kernel/divergence.c" \
    "$ROOT/kernel/checkpoint_journal.c"

echo
echo "==> record a session, persist inputs, replay from the journal, assert identical"
"$BIN"

echo
echo "PASSED: the session replayed byte-identically from the journal"
