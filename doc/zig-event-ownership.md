# Event ownership and ordering record (Zig migration step 4 prerequisite)

Verified against src/event_loop.c, src/event_loop.h, src/misc/ts.h,
src/workspace.m, src/display.c. A Zig-owned event worker must preserve
every behavior below.

## Event types (41, src/event_loop.h)

Application ×5, window ×9 (incl. SLS_WINDOW_ORDERED/DESTROYED),
SLS space ×2, SPACE_CHANGED, display ×5, mouse ×4, Mission Control ×6,
DOCK_DID_RESTART, DOCK_DID_CHANGE_PREF, menu ×2, menu-bar ×1,
SYSTEM_WOKE, DAEMON_MESSAGE.

## Queue mechanics

- Lock-free multi-producer single-consumer: producers CAS onto
  `tail->next`, consumer CAS-steals from `head`; event nodes come from
  a 512 KiB pool; a named semaphore wakes the consumer.
- Producers: Cocoa/KVO (workspace.m), AX/event-tap (application.c,
  mouse_handler.c), CoreGraphics (display.c), SkyLight callbacks, and
  the socket accept thread (message.c).
- Payload discipline today: integer ids passed by value through
  `context` (e.g. display id as intptr_t), heap objects with transfer
  noted at the post site (e.g. `process *`), accepted socket fds as
  `param1`. Before moving the worker, audit every post site for
  retained/copied/borrowed and encode it in the Zig boundary types.

## Consumer ordering (event_loop_run)

Per dequeued event: dispatch handler, then `event_signal_flush()`,
then `ts_reset()`, inside one NSAutoreleasePool batch; empty queue
parks on the semaphore.

- Signals flush after every event, so rule/signal side effects are
  serialized behind the mutation that caused them.
- EXCEPTION: DAEMON_MESSAGE's success path returns early and skips
  both the signal flush and the temp-storage reset. A faithful port
  must preserve that skip (or prove it unobservable).

## Temporary storage lifetime

- `ts_*` is a bump allocator (`used` counter, atomic add). `ts_reset`
  only zeroes the counter after each consumer event.
- Consequence: `ts_*` memory is consumer scratch valid for one event
  only (shorter on the DAEMON_MESSAGE early-return path, where the
  payload survives until the next reset). Callback producers must
  never park data in `ts_*`; everything crossing the queue must be
  heap-owned or passed by value with ownership noted.
- DAEMON_MESSAGE allocates the frame with `ts_alloc_unaligned`
  WITHOUT validating the declared length first (see doc/zig-baseline.md:
  the Zig decoder must add checked lengths).

## Shutdown

- There is no graceful drain: no `event_loop_end` exists, and nothing
  ever clears `is_running`. Service stop terminates the process.
- A Zig-owned worker must therefore keep teardown semantics identical
  (no new flush-on-exit behavior) unless the service contract is
  deliberately changed and re-tested.
