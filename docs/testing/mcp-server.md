# Driving IKOS time-travel from an MCP client (#188, #199)

IKOS exposes its time-travel primitives as Model Context Protocol (MCP) tools,
so an AI agent can reproduce a non-deterministic bug, step the whole system
backward, and find where a value last changed, all as callable tools over
JSON-RPC.

## The tools

| Tool | What it does | Backed by |
|------|--------------|-----------|
| `list_checkpoints` | The retained keyframe window (the rewind horizon) | keyframe ring (#168) |
| `rewind_to` | Restore the nearest keyframe and replay to `(epoch, offset)` | rewind (#169) |
| `reverse_step` | Step one unit backward | reverse execution (#170) |
| `watch_last_write` | Find where a watched value last changed | reverse watchpoint (#171) |

## How it fits together

The tool surface (`kernel/mcp.c`) parses a JSON-RPC request, dispatches
`tools/list` and `tools/call` to the operations, and formats the response; it
does no I/O. `kernel/mcp_server.c` is the transport: a loop that

1. reads one newline-delimited JSON-RPC request line,
2. hands it to `mcp_handle()` over the bound kernel ops (`mcp_kernel_ops()`
   after `mcp_bind()`), and
3. writes the JSON-RPC response followed by a newline.

Blank lines between requests are skipped, and the loop ends cleanly when the
connection closes. The byte I/O is injected, so the loop is host-tested with a
scripted byte stream; the kernel adapter `kernel/mcp_server_sync.c` wires it to
the serial port (COM2, so it does not collide with the gdb reverse stub on
COM1).

At boot the kernel calls `mcp_server_init()`, which binds the ops, configures
the serial line, and registers the keyframe ring (`mcp_set_ring`, from the
keyframe store #195) and the watch probe (`mcp_set_watch_probe`). The blocking
JSON-RPC serve loop `mcp_server_run()` is entered on demand.

## Using it

An MCP client connects to the serial line and speaks newline-delimited JSON-RPC.
List the tools:

```
{"jsonrpc":"2.0","id":1,"method":"tools/list"}
```

Reproduce a moment and step backward:

```
{"id":2,"method":"tools/call","params":{"name":"rewind_to","arguments":{"epoch":35,"offset":2}}}
{"id":3,"method":"tools/call","params":{"name":"reverse_step","arguments":{}}}
{"id":4,"method":"tools/call","params":{"name":"watch_last_write","arguments":{}}}
```

Each request gets one response line. The reach of a rewind is bounded by the
keyframe retention ring (#168): `list_checkpoints` reports the oldest and newest
retained epochs, and a `rewind_to` before the oldest is rejected.

## Verifying the server

The tool dispatch and the transport loop are unit-tested headlessly:

```
cd tests
gcc -I../include -Wall -o /tmp/t_mcp test_mcp.c ../kernel/mcp.c && /tmp/t_mcp
gcc -I../include -Wall -o /tmp/t_msl test_mcp_server.c \
    ../kernel/mcp_server.c ../kernel/mcp.c && /tmp/t_msl
```

The first checks the JSON scanning and `tools/list` / `tools/call` dispatch; the
second checks that requests are read as lines, dispatched through the loop, and
answered one response line each (with blank lines skipped and a clean close at
end of stream).
