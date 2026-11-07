# Event ownership and ordering

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
- DAEMON_MESSAGE reads the frame through `daemon_read_message`
  (src/event_loop.c), which rejects a declared length outside 1..1 MiB
  before its `ts_alloc_unaligned` call and then validates the framing
  with `yabai_socket_frame_decode` (see doc/zig-baseline.md). At dd84572
  the declared length was allocated unchecked.

## Shutdown

- There is no graceful drain: no `event_loop_end` exists, and nothing
  ever clears `is_running`. Service stop terminates the process.
- A Zig-owned worker must therefore keep teardown semantics identical
  (no new flush-on-exit behavior) unless the service contract is
  deliberately changed and re-tested.

## Ownership across the Zig boundary

- view_create/clear/destroy and view_find_window_list are routed
  (src/view.c). view_clear/view_destroy therefore run Zig code that calls
  `window_manager_remove_managed_window(&g_window_manager, ...)`,
  `insert_feedback_destroy` and `CFRelease` (zig/view.zig:594-627): Zig
  triggers manager mutation in the daemon through those C calls, though
  it still never assigns a `g_*` global itself.
- view_find_window_list returns libc-malloc'd memory and its sole caller,
  window_manager_validate_windows_on_space (src/window_manager.c), frees
  it after its last use. The find_window_list routing decision is closed.
- view_create and view_find_window_list are now routed allocations;
  window_node_split and parse_label remain unrouted ones. Every Zig
  malloc failure aborts (`orelse abort()`).
- yabai_parse_rule (zig/rule.zig:745) takes
  `const struct yabai_rule_lookup *` (src/message.c:2603, both call sites
  at :2627 and :2641) instead of eager acting ids. The seven function
  pointers (active display/space, closest display in direction, prev/next
  display, prev/next space) are called only inside the display=/space=
  branches. Label, cursor, first/last and mission-control lookups still
  call C managers and read `g_display_manager`/`g_space_manager` directly.
  The C body survives only as `ref_parse_rule` in
  tests/src/ref_upstream_rule.c.
- DAEMON_MESSAGE: `daemon_read_message` (src/event_loop.c:1625) validates
  the length before `ts_alloc_unaligned` (:1633) and decodes at :1645;
  the handler calls `handle_message` at :1659 and returns before the
  consumer's `event_signal_flush()`/`ts_reset()` (:1694-1695). The
  early-return skip described in "Consumer ordering" is unchanged.
  A rejected frame is closed with no reply.
