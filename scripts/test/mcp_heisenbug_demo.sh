#!/usr/bin/env bash
#
# MCP heisenbug demo / test (#159 Milestone E).
#
# A scripted agent debugs a planted heisenbug using the MCP time-travel tools.
# The tool calls are real JSON-RPC requests handled by the MCP dispatch core,
# backed by the real rewind (#169) / reverse (#170) / reverse-watchpoint (#171)
# stack. A watched value is good throughout the run except for one bad write; the
# agent finds it by asking watch_last_write and rewinding, without knowing where
# the bug is.
#
# Headless (no emulator). Exits non-zero if the agent fails to localize the bug,
# so it gates CI.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CC="${CC:-gcc}"
WORK="$(mktemp -d)"
BIN="$WORK/mcp_heisenbug"
trap 'rm -rf "$WORK"' EXIT

echo "==> building MCP heisenbug demo"
"$CC" -I"$ROOT/include" -O2 -Wall -o "$BIN" \
    "$ROOT/tests/mcp_heisenbug_e2e.c" \
    "$ROOT/kernel/mcp.c" \
    "$ROOT/kernel/revbreak.c" \
    "$ROOT/kernel/reverse.c" \
    "$ROOT/kernel/rewind.c" \
    "$ROOT/kernel/keyframe_ring.c" \
    "$ROOT/kernel/replay_engine.c"

echo
"$BIN"

echo
echo "PASSED: the agent used the MCP tools to rewind and localize the bug"
