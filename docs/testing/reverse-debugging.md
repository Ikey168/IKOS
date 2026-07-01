# Reverse Debugging with GDB (#172)

IKOS exposes its time-travel reverse execution to a normal gdb session, so you
can step and run the whole system backward through its recorded history from the
gdb prompt.

## How it fits together

Reverse execution is a property of IKOS's replay engine (#165), not of the
emulated CPU. Restoring the nearest keyframe (#168) and replaying the input
journal (#161) forward reconstructs any past moment (#169); stepping that
position backward is reverse-step / reverse-continue (#170), and scanning back to
a condition is a reverse breakpoint or watchpoint (#171).

GDB drives reverse debugging with two Remote Serial Protocol packets:

| gdb command | RSP packet | IKOS handler |
|-------------|-----------|--------------|
| `reverse-stepi` | `bs` | `kreverse_step()` |
| `reverse-continue` | `bc` | `kreverse_continue()` |

GDB only sends these after the target advertises support, so IKOS's stub answers
`qSupported` with `ReverseStep+;ReverseContinue+`. The stub lives in
`kernel/gdbstub.c` (pure RSP framing and dispatch) and `kernel/gdbstub_sync.c`
(the bs/bc wiring and the serial transport).

This is a separate connection from QEMU's own gdb server (`qemu -s`, used by
`make debug`): that server debugs the emulated CPU going forward and does not
know about IKOS's replay history. IKOS's stub runs inside the kernel and speaks
gdb RSP over the serial port, so gdb talks to IKOS directly for reverse control.

## Using it

1. Boot IKOS under QEMU with a serial line gdb can attach to, and with the
   in-kernel stub enabled (it registers the reverse ops via
   `gdbstub_bind_reverse()` and serves packets from the serial loop).

2. From gdb, connect to the serial line and confirm the reverse packets were
   negotiated:

   ```
   (gdb) target remote <serial>
   (gdb) show can-use-reverse-execution
   ```

3. Drive execution backward:

   ```
   (gdb) reverse-stepi      # one step back: restores the prior keyframe and
                            # replays to just before the current point
   (gdb) reverse-continue   # run backward to the previous stop / the oldest
                            # retained keyframe
   ```

The reach of a rewind is bounded by the keyframe retention ring (#168): reverse
execution stops at the oldest retained keyframe rather than running off the end.

## Verifying the stub

The RSP handling is unit-tested headlessly:

```
cd tests
gcc -I../include -Wall -o /tmp/t_gdb test_gdbstub.c ../kernel/gdbstub.c && /tmp/t_gdb
```

The test checks the checksum/framing, that `qSupported` advertises the reverse
packets, and that `bs` / `bc` map to reverse-step / reverse-continue. Driving a
live `reverse-stepi` from gdb against a running IKOS under `make debug` exercises
the same handlers over the serial transport.
