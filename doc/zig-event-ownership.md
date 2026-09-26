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

## 2026-09-26 routing-ownership audit (refactor/zig @ f0fe453)

Method: every `yabai_` call site in src/*.c enumerated by grep;
each claim checked against the Zig export body and the C call site.
Anything not so checked is marked unverified. NOTE: doc/zig-refactor-plan.md,
referenced by doc/zig-baseline.md, does not exist in this worktree, so
step-4 exit criteria could not be read; the gap list in (5) is
audit-derived, not plan-checked (unverified against the plan).

Routed delegates (C now calls Zig): src/window.c `window_can_move`,
`window_can_resize`, `window_is_undersized`; src/window_manager.c
`move_frame_math`, `resize_frame_math`, warp-site `yabai_area_make_pair`
plus `window_node_get_split/get_gap/get_ratio`; src/view.c
`window_node_contains_window`, `window_node_index_of_window`,
`window_node_swap_window_list`, `window_node_find_first/last/prev/next_leaf`,
`window_node_rotate`, `window_node_mirror`, `window_node_fence`,
`view_find_window_node`, `view_stack_window_node`, `view_is_invalid`,
`view_is_dirty`, `view_serialize`; src/rule.c `rule_serialize`,
`rule_combine_effects`; src/display.c `display_serialize`; src/message.c
`yabai_parse_rule` at both handle_domain_rule sites (add at :2829,
apply-selector at :2843). Exported-but-unrouted (test-only, C still live):
view_create/clear/destroy, view_find_window_list (deliberately, see below),
window_node_split, balance/equalize/update statics, get_child, area
converters, min-depth leaf, rule_destroy/flag helpers, parse_label,
parse_properties, parse_insert_selector, socket_frame_decode, tokenizers,
easing/rgba/find_rank/layer — no C call sites found for any of these.

### (1) Heap allocations crossing the boundary, and who frees

- `yabai_parse_rule` (zig/rule.zig:921): mallocs label/app/title/role/
  subrole/scratchpad via internal stringCopy (libc malloc) and regcomp()s
  the four regexes under the same VALID/EXCLUDE flag guards as C.
  Ownership transfers to the caller's stack `struct rule`: on success
  `rule_add` shallow-copies it into `g_window_manager.rules` (strings
  become manager-owned, freed later by C `rule_destroy`); on failure the
  C call site runs C `rule_destroy` (regfree under VALID flags plus six
  frees — byte-mirrors zig/rule.zig:1139-1148 vs src/rule.c:119-133).
  Same libc allocator both sides; no cross-allocator free.
- The parse also mutates the caller-owned message buffer in place
  (NUL writes via parse_key_value_pair, cursor advance via get_token):
  no ownership transfer, caller buffer, synchronous.
- `yabai_rule_combine_effects` (zig/rule.zig:127): frees
  `result.scratchpad` with libc free and mallocs a fresh copy. Both
  live call sites (window_manager.c:198,222) pass a stack-local `effects`
  as result and wm-owned rule effects as source. Invariant this relies
  on: `result.scratchpad` is always libc-malloc'd or NULL (true today —
  only stringCopy/parse paths set it; results start zeroed).
- `yabai_rule_serialize` (zig/rule.zig:387): mallocs up to four escape
  buffers via tsEscape and frees all of them on every path, including
  the rsp==null early return (:414-420, :462-465). Inputs (rule strings,
  label, scratchpad, lifted indices) are borrowed. Allocation-source
  asymmetry (new, behavior-neutral): C `ts_string_escape` used the
  temp-pool bump allocator (no free); Zig uses libc malloc + internal
  free. Must stay internal — returning an escape across the boundary
  would leak.
- `yabai_display_serialize` / `yabai_view_serialize`: Zig allocates
  nothing (no malloc in zig/display.zig; serialize path uses only
  fwrite/snprintf). The C call sites allocate `uuid` (ts_cfstring_copy),
  `space_mci` (ts_alloc_list), `window_list` (space_window_list, itself
  ts_alloc_list/ts_resize per src/space.c:35,71), and the intermediate
  `display_space_list` (ts_alloc_list per src/display.c:209) — all
  temp-pool, freed by the per-event `ts_reset`, never by Zig. Manager
  label strings (`display_label->label`, `space_label->label`) are
  borrowed from live manager entries, read synchronously, never retained.
- View-tree walks (first/last/prev/next/find_window_node/fence) return
  borrowed live-tree pointers; swap/rotate/mirror/stack mutate nodes in
  place (mirror returns the same pointer, no allocation — verified both
  sides). No allocation crosses on any routed tree path.
- Unrouted allocations (no live boundary yet): `window_node_split`
  mallocs two children (zig/view.zig:453-454), `view_create` mallocs
  view+root (:632-635), `view_find_window_list` mallocs/reallocs the list
  (:574-582), `parse_label` mallocs the label (zig/message.zig:466).

### (2) Globals the Zig code reads, and whether it mutates any

- zig/rule.zig declares `extern var g_display_manager` /
  `g_space_manager` (:584-585). Reads only: `last_display_id` (:802),
  `last_space_id` (:885), and label lookups by address
  (`display_manager_get_display_for_label(&g_display_manager, ...)` :812,
  `space_manager_get_space_for_label(&g_space_manager, ...)` :895), plus
  live-manager calls for cursor/prev/next/first/last/mission-control
  selectors (:575-583 externs). Acting display/space ids stay lifted in
  as parameters at the message.c call sites — but the display:/space:
  label branches inside Zig still reach the globals directly, exactly
  like C (no seam split yet, as the baseline honestly records).
- zig/view.zig declares `extern var g_window_manager` (opaque, :618)
  passed to `window_manager_remove_managed_window` in the clear/destroy
  paths (:667, :687) — but those exports are UNROUTED (C view_clear/
  view_destroy still live), so no Zig-triggered manager mutation runs in
  the daemon today.
- A grep for assignments to any `g_*` in zig/ returns nothing: Zig never
  writes manager globals directly. Indirect mutation via extern C calls
  exists only in unrouted exports (remove_managed_window,
  insert_feedback_destroy, CFRelease) and routed `fwrite(rsp, ...)`.
  geometry/window/message/display/util touch no globals. Zig mutates only
  caller-passed structs: nodes, rule, effects, message cursor/buffer,
  out-params. Pure reads (out-params or return only): all getters, leaf
  walks, fence, contains/index, is_invalid/is_dirty, can_move/can_resize/
  is_undersized/layer, frame math, easing, find_rank, area converters.

### (3) Callback-data flow (live structs into Zig; retention?)

- event_loop.c passes live manager-owned views (`window_manager_find_
  managed_window`, e.g. :57-60) and live nodes into find/is_dirty/is_
  invalid; space_manager.c:401,413 passes live `view->root` into
  rotate/mirror; mouse_handler.c:138-161 passes live src/dst nodes into
  find/stack/contains/swap; window_manager.c passes live views/nodes
  into find/fence/prev/next/first/last/combine (combine sources are
  `wm->rules[i].effects`, read-only `*const` on the Zig side).
- Zig retains nothing: no file-scope mutable state exists in any zig
  object (verified by grep — only extern decls, fns, and constants).
  Borrowed pointers are used within the call; returned node pointers are
  consumed synchronously by the C caller on the event thread. Event-queue
  payload discipline is untouched (delegates run on the consumer; post
  sites unchanged).

### (4) Ownership asymmetries: documented plus new

- Documented (upheld): `view_find_window_list` — C returns temp-pool
  memory the sole caller never frees; the Zig export returns caller-
  frees malloc. Deliberately unrouted (src/view.c:9 comment; the Zig
  comment at zig/view.zig:567-569 states the same). Routing it without
  changing the caller would leak.
- New (behavior-neutral today, must be pinned): tsEscape malloc-vs-
  ts-bump (frees internal; see (1)); combine_effects freeing a caller-
  owned scratchpad with libc free (relies on the libc-only invariant);
  `rule_add` shallow-copying Zig-filled strings into manager ownership
  (a future Zig-owned rule store must adopt C rule_destroy's free
  contract verbatim, including VALID-guarded regfree).
- New (behavioral exposure, not neutral): Zig parse_rule's display:/
  space: branches call live SLS-backed manager functions and read live
  globals; per the baseline these paths are parity-proven in zero-state
  only. Daemon behavior there is unverified by tests.

### (5) What must still be proven before Zig OWNS any manager state

- Prove parse_rule's manager-coupled branches (label/cursor/recent/
  mission-control selectors) beyond zero-state against live managers.
- Pin the combine_effects scratchpad allocator invariant (libc-only) or
  move the free inside the producer.
- Resolve find_window_list routing: change the sole caller to free, or
  record C-forever with rationale. Currently decided-but-open.
- Route or explicitly defer each unrouted export, notably view_create/
  clear/destroy (extern SLS inputs + g_window_manager mutation via
  remove_managed_window + CFRelease) and rule_destroy (must stay the
  single free contract while C owns the store).
- Prove the no-retention claim under the real consumer, including the
  DAEMON_MESSAGE early-return path (skips signal flush and ts_reset):
  serialize inputs live in ts_ memory until the next reset; Zig holds
  nothing, but the argument must cite the consumer, not the export.
- Reconcile with step-4 exit criteria once doc/zig-refactor-plan.md is
  restored — this list is audit-derived and unverified against the plan.
- Unverified items: whether `space_manager_get_label_for_space` /
  `display_manager_get_label_for_display` return borrowed (assumed from
  synchronous read-only use at the call sites — lifetime not traced into
  the manager implementations); x86_64-slice ownership parity (build
  evidence cited from baseline, not re-verified here).
