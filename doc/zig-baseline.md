# Zig migration baseline (yabai 7.1.25, commit dd84572)

Recorded from a clean `make` build with the pinned toolchain below. Any
Zig-led replacement must reproduce everything in this file byte for byte
unless doc/zig-refactor-plan.md explicitly allows a change.

The "Step 2/Step 3 progress" sections are a dated log: export and test
counts there are what each step recorded at the time, and "the C
function as oracle" meant the live C body, before routing replaced it.
Parity tests now compare routed exports against the frozen dd84572 copies
in tests/src/ref_upstream_*.c. Current figures are in "Daemon linkage"
and "Routing map" below.

## Toolchain

- `zig version`: 0.17.0, pinned in zig/ZIG_VERSION and enforced by
  zig/build.sh (the migration started on 0.16.0)
- `xcrun clang` from system Xcode; macOS minimum 11.0 (`-mmacosx-version-min=11.0`)

## Reference binary

- `./bin/yabai-zig --version` prints `yabai-zig-v7.1.25`, exit 0.
- `./bin/yabai-zig --help` prints the option list in src/yabai-zig.c
  (`--load-sa`, `--uninstall-sa`, `--install-service`,
  `--uninstall-service`, `--start-service`, `--restart-service`,
  `--message/-m`, `--config/-c`, `--verbose/-V`, `--version/-v`,
  `--help/-h`) plus the `man yabai-zig` pointer.
- `lipo -info ./bin/yabai-zig`: fat file with `x86_64 arm64` slices.
- Suite: `cd tests && make` runs the 106 entries in `TEST_LIST`
  (tests/src/tests.m) as `tests/bin/tests`, then the `tests/bin/osax27`
  Dock payload helper checks, then the same suite built for x86_64
  (`tests/bin/tests-x86_64`) under Rosetta, or prints a skip line when
  Rosetta is not installed.

## Step 2 boundary evidence

Historical (Zig 0.16.0 at the time; zig/build.sh has since replaced the
`zig build-obj` rule that tests/makefile used, and the oracle is now the
frozen `ref_area_make_pair` in tests/src/ref_upstream_view.c).

- `zig/geometry.zig` exports `_yabai_area_make_pair` with a C ABI;
  tests/makefile built it with `zig build-obj` and linked it into
  `tests/bin/tests`.
- Parity: `zig_area_make_pair_parity` runs 10 split/gap/ratio cases
  through both the C `area_make_pair` (oracle) and the Zig export and
  byte-compares all outputs; `zig_area_make_pair_rounding` pins
  hand-computed truncation behavior. Both fail against a stubbed export
  and pass against the real one (TDD red/green observed).
- Both architecture ABIs: the object compiles to Mach-O arm64 (linked
  into tests) and Mach-O x86_64 (`-target x86_64-macos` check).

## Step 3 progress (geometry slice, TDD)

- `yabai_area_is_in_direction` / `yabai_area_distance_in_direction`
  added to zig/geometry.zig; `zig_area_direction_parity` drives the
  3-display fixture plus edge-touching/contained areas over all ordered
  pairs x 4 directions (72 bool + 72 int comparisons) against the C
  functions as oracle. Red against stubbed exports, green on the real
  ones; x86_64 slice verified. CGPoint crosses the boundary as an
  extern struct of two f64 (layout-identical on 64-bit macOS).
- Tokenizer slice (TDD) in zig/message.zig: `get_token`, `prefix`,
  `equals`, `is_valid`, `is_positive_integer`, `is_hexadecimal`,
  `is_float` (via libc strtof, same as C), `to_value`. Four parity
  tests in tests/src/zig_message.c (token streams incl. cursor
  positions, 35 prefix/equals pairs, int/hex/float vectors incl.
  overflow wrap and hex-float/inf/nan, to_value type dispatch).
  Red against stubs, green on the real code; the stream test caught a
  genuine cursor-position divergence that was fixed. x86_64 slice
  verified; all 8 exports present in the linked object.
- Live dispatcher checks merged from the parallel baseline worker and
  passing: unknown domain/command yield the 0x07 failure prefix,
  `rule --list` yields `[]`, `signal --list` yields a JSON list.
- Key/value + named-constant slice (TDD) in zig/message.zig:
  `parse_key_value_pair` (incl. in-place NUL write and untouched-flag
  paths), `parse_value_type`, `parse_resize_handle`. Parity tests cover
  11 separator edge cases with byte-compared mutated buffers, plus
  hand-pinned constant tables. x86_64 slice verified.
- View-tree traversal slice (TDD) in zig/view.zig: `is_leaf`,
  `is_left/right_child`, `find_first/last/prev/next_leaf`,
  `contains_window`, `index_of_window` (incl. the returns-0-on-miss
  quirk). WindowNode mirrors struct window_node byte for byte
  (344 bytes; offsets from clang -fdump-record-layouts, asserted on
  both sides). Zero external symbols in the view object.
- Tree-mutation slice (TDD): `rotate` (90/180/270), `mirror`, `fence`
  with structural tree comparison (pointer fields by position, since
  the C and Zig trees live at different addresses). The test caught a
  real overlap-direction bug (`copyForwards` vs C `memmove`), fixed to
  `copyBackwards`.
- Stack-insert slice (TDD): `view_stack_window` takes the window id
  directly (the only field the C version reads).
- Window lookup slice (TDD): `view_find_window_node` composes the
  proven leaf-walk/contains exports internally against a 72-byte View
  mirror (offset-asserted).
- Resize-math slice (TDD) in zig/window.zig: the pure frame math was
  first extracted verbatim into `resize_frame_math()` in
  src/window_manager.c (caller behavior unchanged), then ported with
  exact f64 promotion/narrowing semantics, clamp and NaN pins.
- Move-math slice (TDD): `move_frame_math()` extracted the same way
  from the move path, ported with f64 widening on the origin add.
- Easing slice (TDD): all 21 `ease_*` curves in zig/util.zig via
  libc sinf/cosf/powf/sqrtf, proven over 147 bit-exact vectors. Key
  finding: Apple clang fuses FMA even at -O0 (disassembly-proven);
  the port mirrors each fused site with explicit `@mulAdd` (wrong
  guesses — unfused and `@setFloatMode(.strict)` — both failed
  parity first). 64 exports in the fat daemon; x86_64 slice
  verified; util object needs only libm.
- Checked frame decoder (TDD) in zig/message.zig:
  `yabai_socket_frame_decode` validates the native length prefix and
  framing before exposing borrowed field pointers — the replacement
  for the unchecked DAEMON_MESSAGE allocation. Parity-tested against
  the characterization helper plus hand pins on every malformed
  class.
- Property-table matcher (TDD): `yabai_parse_property` with a
  32-byte Properties mirror (offset-asserted), accumulation
  semantics, and case-sensitivity pins. 40 exports in the fat
  daemon; x86_64 slice verified.
- Property-list driver (TDD): `yabai_parse_properties` ports the
  comma-splitting loop with in-place NUL writes, flag accumulation,
  and byte-identical daemon_fail emission via fwrite (no varargs
  ABI). Parity-tested over 13 vectors plus a NULL-stream case and
  hand-pinned 0x07 error bytes captured via open_memstream.
- Window-pure slice (TDD): `yabai_window_is_undersized` (f64 frame
  check vs 500.0), `yabai_window_can_move` /
  `yabai_window_can_resize` (exhaustive over the full u8 flag byte
  against the C oracle on a real struct window, incl. NaN width).
  68 exports in the fat daemon; x86_64 slices verified.
- Named-const hardening (TDD): the value-type / resize-handle parity
  vectors now spell expectations with the symbolic TYPE_* / HANDLE_*
  macros, pinned by 11 _Static_asserts so a silent renumber breaks
  the build instead of re-pinning the vectors.
- First daemon-side move: src/window.c's window_can_move,
  window_can_resize, and window_is_undersized are thin delegates to
  the parity-proven Zig exports (signatures unchanged). The daemon
  still links fat x86_64+arm64 with --version unchanged.
- Window-manager delegation: move_frame_math / resize_frame_math in
  src/window_manager.c delegate to the Zig exports; call sites
  unchanged.
- Label parser (TDD): `yabai_parse_label` reuses the token ports,
  mirrors the three reserved-keyword tables (macro-verified), mallocs
  the label via libc, and emits byte-identical daemon_fail bytes.
  69 exports in the fat daemon; x86_64 slices verified.
- Rule lifecycle (TDD): `yabai_rule_destroy` with a 256-byte Rule
  mirror (regex_t as opaque 32-byte storage, offset-asserted) plus
  set/clear/check flag helpers. Review caught a skipped-regfree
  divergence in the worker draft; the merge calls libc regfree under
  the same VALID-flag guards as C, proven by a regcomp/regfree parity
  case and a whole-suite `leaks -atExit` gate (0 leaks).
  73 exports in the fat daemon; x86_64 slices verified.
- View-tree slice (TDD): ten pure helpers — occupancy/intermediate,
  child/split/ratio/gap readers (manager state lifted in as params),
  area/CGRect conversion, equalize, and balance composing the shared
  balance_node_add export. 83 exports in the fat daemon; x86_64
  slices verified.
- Window-layer leaf (TDD): `yabai_window_layer` with lifted level
  globals and LAYER_* pins; borrowed strings, no allocation.
- Rule serialization (TDD): `yabai_ts_string_escape` (malloc'd
  escapes, NULL when unneeded) and `yabai_rule_serialize` with
  manager-derived indices lifted in, libc snprintf numerics, and
  all escape buffers freed on every path. Whole-suite leaks gate
  still 0 leaks. 86 exports in the fat daemon; x86_64 verified.
- Node surgery (TDD): `yabai_area_make_pair_for_node` (reuses shared
  area_make_pair), `yabai_window_node_split` (libc-malloc'd children,
  zoom/placement/ratio lifts), and `yabai_window_node_swap_window_list`
  — the swap also delegates its C body in src/view.c. Leaks gate 0.
- Insert selector (TDD): `yabai_parse_insert_selector` with a 32-byte
  Selector mirror (union as u64 payload, zeroed where C leaves garbage)
  and byte-identical DIR_SEL error bytes. 90 exports in the fat
  daemon; x86_64 slices verified.
- View leaves (TDD): min-depth BFS (exact queue order/NULL behavior),
  VALID/DIRTY flag readers with pins, and recursive clear_zoom.
  Suite at 59/59; x86_64 slice verified.
- Rule parser (TDD): `yabai_parse_rule` covering all 16 key branches
  with byte-identical emissions, reusing every ported helper plus
  libc string_copy/regcomp/sscanf. Caveats recorded honestly: the
  display/space branches call through to the live manager functions
  and globals exactly like C (no seam split yet), so zig-rule.o is no
  longer standalone-linkable, and manager-coupled paths are parity-
  proven in zero-state only; MsgToken/DIR mirrors are duplicated
  across objects pending a shared-types pass. Leaks gate 0.
  95 exports in the fat daemon; 61/61 tests; x86_64 verified.
- Window-list walk (TDD): `yabai_view_find_window_list` reusing the
  ported leaf walk, exact 13-double growth via libc realloc, with a
  documented ownership asymmetry (C returns pool memory, Zig returns
  caller-frees malloc). Release audit recorded in doc/zig-release.md:
  fat slices, minos 11.0, plist section, frameworks, adhoc signing,
  OSAX/install/service unchanged; hardware-gated items listed as
  unverified. 96 exports; 62/62 tests; leaks 0; x86_64 verified.
- View lifecycle (TDD): create/clear/destroy with a ViewDefaults seam
  for the SLS/manager inputs; trailing view_updates omitted as proven
  no-ops on zeroed roots; destroy calls live remove/feedback/CFRelease
  exactly like C. 99 exports; 69/69 tests; leaks 0; x86_64 verified.
- View serializer (TDD): `yabai_view_serialize` with eight lifted
  manager inputs, %lld sid bits, and the did_output comma logic.
- Rule-serialize routing: a hand-pinned full-JSON golden now guards
  the seam, and C rule_serialize delegates to Zig (managers resolved
  at the call site) — the query path runs Zig code in the daemon.
  100 exports; 70/70 tests; leaks 0; x86_64 verified.
- View-serialize routing: both-sides JSON golden guards the seam;
  C view_serialize delegates with flag-guarded manager lifts.
- Display serializer (TDD): new zig/display.zig object (7th),
  wired through build.sh and both makefiles, with lifted uuid,
  indices, label, frame, spaces, and focus. 101 exports; 71/71
  tests; leaks 0; x86_64 verified.
- Display-serialize routing: full-JSON golden guards the seam; the
  display query path runs Zig in the daemon.
- Transform routing: rotate/mirror delegate to self-recursive Zig
  exports after a pin audit added hand-computed tree vectors.
  71/71 tests; leaks 0; fat daemon unchanged at 101 exports.
- Walk/lookup routing: first/last/next/prev leaf and
  view_find_window_node delegate to self-contained Zig walks
  (window_manager, event_loop, space_manager call sites now run Zig).
- Warp-site routing: window_manager_warp_window's area_make_pair
  line calls the Zig exports with g_space_manager lifts; gap pins
  added to the geometry suite. 71/71 tests; leaks 0; 101 exports.
- Predicate routing: fence, contains/index lookups, and VALID/DIRTY
  readers delegate to Zig (mouse, window, event-loop call sites).
- Combine routing: rule_combine_effects delegates with fresh-copy
  ownership pins. 71/71 tests; leaks 0; fat daemon, 101 exports.
- Stack routing: view_stack_window_node delegates to Zig.
  find_window_list deliberately stays C (sole caller never frees;
  Zig returns caller-frees memory — routing would leak). Superseded
  by "List routing" below.
- Rule call-site routing: both handle_domain_rule parse sites call
  Zig with eager acting ids; C parse_rule stays as the live oracle.
  Superseded: the C body now lives only as `ref_parse_rule` in
  tests/src/ref_upstream_rule.c, and the acting ids are resolved lazily
  through `struct yabai_rule_lookup` (see the routing map).
  71/71 tests; leaks 0; fat daemon, 101 exports.
- Lifecycle routing: view_create/clear/destroy delegate to Zig
  (SLS uuid and defaults lifted in wrappers; trailing view_update
  kept; dead C destroy body removed). Space-manager call sites now
  allocate views through Zig. 71/71 tests; leaks 0; 101 exports.
- List routing: view_find_window_list delegates to Zig with
  hand-pinned id arrays; the sole caller frees (no early returns).
  Audit close-out in doc/zig-event-ownership.md: borrowed label
  lifetimes, DAEMON_MESSAGE path, x86_64 parity — all evidenced.
  71/71 tests; leaks 0; fat daemon, 101 exports.
- Rule matching (TDD): regex_match plus rule_matches_window with the
  app name lifted (no window struct); EXCLUDE inversions and UD paths.
- Mouse geometry (TDD): new zig/mouse.zig object (8th) — triangle
  (f32-narrowing mirrored), mod-bit map, drop action with CG
  min-in/max-exclusive center rect proven by fall-through vectors.
  106 exports; 76/76 tests; leaks 0; x86_64 verified.
- Match routing: window_manager_rule_matches_window delegates with
  the app name lifted; NULL-with-VALID defined as NO (C segfaults;
  daemon never produces NULL).
- Drop-action routing: mouse_determine_drop_action delegates with
  scalar lifts; event-loop call sites run Zig. 76/76 tests; leaks 0;
  fat daemon, 106 exports.
- Directional search (TDD+route): view_find_window_node_in_direction
  reuses area/rank/leaf/max-point ports with the SLS list lifted;
  rank-miss UB-vs-INT_MAX documented. 107 exports; 77/77 tests;
  leaks 0; x86_64 verified.
- Pure-helper slice (TDD) in zig/util.zig: `balance_node_add`,
  `clampf_range` (incl. NaN bit-parity), `json_bool`,
  `json_optional_bool`, float+int `in_range_ii/ei/ie` for the exact
  combinations used in-repo, `rgba_color_from_hex`,
  `find_rank_in_list` (incl. duplicate and INT_MAX-miss cases).
- Rule-effects slice (TDD) in zig/rule.zig: `rule_combine_effects`
  incl. scratchpad ownership (libc free old, malloc new) and all
  untouched-field paths; reuses the shared `in_range_ii_f` export
  across objects. 37 exports in the fat daemon; x86_64 slice
  verified; rule object needs only libc plus the shared export.

## Daemon linkage (universal objects)

- `zig/build.sh` (pin: zig/ZIG_VERSION, currently 0.17.0) compiles each
  of the eight modules (geometry, message, view, util, rule, window,
  display, mouse) with `-O ReleaseSafe` for x86_64-macos.11.0 and
  aarch64-macos.11.0 (minos 11.0 on both slices) and lipos them into
  `zig/zig-<mod>.o`. Before building it checks the C/Zig ABI pins and
  every `yabai_*` prototype against its export.
- Both makefiles depend on one stamp, `zig/bin/build.stamp`, so
  `make -j` runs build.sh once for all objects.
- The root makefile links all eight objects into `bin/yabai-zig` (still a
  fat x86_64+arm64 binary, `--version` unchanged); 107 `yabai_*`
  exports. The objects need no Zig runtime; geometry, message, util,
  window, display and mouse need only libc/libm, while rule and view
  call back into C manager functions and globals (see the routing map).
- tests/makefile links these same universal objects into both test
  binaries, so the parity suite proves the shipped ReleaseSafe objects
  directly.

## CLI/daemon wire format (frozen)

- Socket path: `/tmp/yabai-zig_$USER.socket` (SOCKET_PATH_FMT in src/yabai-zig.c),
  created with mode `0600` (message_loop_begin in src/message.c).
- Accept thread (message_loop_run) hands each fd to the event worker;
  `daemon_read_message` in src/event_loop.c reads a native `int` byte
  length, then loops until exactly that many payload bytes arrive.
- Encoder (`socket_frame_encode`, called by client_send_message in
  src/yabai-zig.c):
  `length = argc + sum(strlen(argv[i]))`; frame = native `int` length,
  then each argument bytes plus `'\0'`, then one final `'\0'`.
  Example: `yabai-zig query --windows` encodes a 17-byte payload
  `query\0--windows\0\0`.
- Failure marker: responses starting with byte `0x07` (FAILURE_MESSAGE in
  src/misc/macros.h) go to stderr and force a non-zero CLI exit.
- The seven command domains (src/message.c): `config`, `display`,
  `space`, `window`, `query`, `rule`, `signal`.
- Checked decode (deliberate change from dd84572, which allocated the
  declared length unchecked): `daemon_read_message` rejects a declared
  length outside 1..1 MiB before allocating, and a short read; the full
  frame then goes through `yabai_socket_frame_decode`, which also
  rejects a payload that does not end in an empty final field (the
  CLI's trailing `\0\0`). A rejected frame is closed with no reply;
  upstream also closed short reads without a reply. Valid CLI frames are
  handled byte for byte as before. Covered by the `socket_frame_*`
  tests in tests/src/socket_frame.c, which drive the production encoder
  and read path.

## Deliberate output changes (macOS 27 backport)

- `space --focus`, `space --switch` and `display --space` can fail with
  two new messages when the synthetic-gesture fallback runs:
  `cannot focus space: synthetic gesture failed.` (nothing was posted)
  and `cannot focus space: synthetic gesture was interrupted; the
  focused space may have changed.` (src/message.c). For
  `space --switch` and `display --space` the fallback runs only on
  macOS 27 (`space_manager_focus_space_fallback`); elsewhere they fail
  with the upstream scripting-addition message as before.

## Routing map (2026-10-22, 107 exports, 8 objects, 106 tests)

Census via `nm -gU zig/zig-*.o` plus a grep of src/ for each export
outside src/zig_abi.h. 36 exports have live C callers (the daemon runs
them); the rest are test-only parallels or Zig-to-Zig shared helpers.

Routed (daemon runs Zig): area_make_pair (warp site);
rule_combine_effects, rule_matches_window, rule_serialize; parse_rule
(both handle_domain_rule sites; takes a `const struct yabai_rule_lookup *`
of seven C manager functions, called only inside the display=/space=
branches, as upstream evaluated them; C body frozen as `ref_parse_rule`
in tests/src/ref_upstream_rule.c); socket_frame_decode
(`daemon_read_message`); window_can_move/can_resize/is_undersized;
move/resize_frame_math and get_split/get_gap/get_ratio (warp site);
view_create/clear/destroy, view_serialize, view_find_window_list
(caller frees), view_find_window_node (+in_direction),
first/last/next/prev_leaf, fence, contains/index lookups,
is_invalid/is_dirty, stack_window, swap/rotate/mirror;
display_serialize; mouse_determine_drop_action.

Zig-to-Zig shared (no C callers, reused across objects): in_range_ii_f
(rule.zig opacity checks), token is_valid/equals/to_value/get_token,
area_is_in_direction/distance_in_direction, balance_node_add, find_rank_in_list,
json_bool/optional, parse_key_value_pair.

Deliberately unrouted: 21 easing curves (C-derived vectors would go
tautological; only 5 exact-value pins exist; see doc/zig-release.md for
the debug/release difference in the C oracle); same-TU statics with
zero boundary value (message decoders, parse_label/properties/
insert_selector, view split/balance/equalize/clear_zoom/get_child/
predicates, window_layer, mouse mod/triangle); rule_destroy/flags
(same-file/header-inline callers); regex_match and ts_string_escape
(reached through the routed rule exports); view_find_min_depth_leaf
(no callers).

Never ported (coupled, needs live sessions): display/space/window
selector resolution beyond parse_rule's call-through, node
add/remove/flush/update/capture, window/space/display AX/SLS query
paths, all manager tables and event/signal flow, easing dispatch.

## Renamed to yabai-zig (deliberate)

The program names itself `yabai-zig` everywhere so it can run beside, and
never be mistaken for, upstream yabai. Wire format, command domains, and
query JSON are unchanged; only names and paths moved:

- Binary `bin/yabai-zig`; `--version` prints `yabai-zig-v7.1.25`; help,
  usage, and error prefixes say `yabai-zig` / `yabai-zig-msg`.
- Sockets and lock: `/tmp/yabai-zig_$USER.socket`,
  `/tmp/yabai-zig_$USER.lock`, `/tmp/yabai-zig-sa_$USER.socket`.
- launchd label `com.fakebizprez.yabai-zig`; logs
  `/tmp/yabai-zig_$USER.{out,err}.log`.
- Config file `yabai-zigrc` (`$XDG_CONFIG_HOME/yabai-zig/`,
  `~/.config/yabai-zig/`, or `~/.yabai-zigrc`).
- Signal environment variables `YABAI_ZIG_*` (was `YABAI_*`).
- Bundle id `com.fakebizprez.yabai-zig`; scripting addition
  `/Library/ScriptingAdditions/yabai-zig.osax`
  (`com.fakebizprez.yabai-zig-osax`, payload `com.fakebizprez.yabai-zig-sa`).
- Man page `doc/yabai-zig.1`; signing identity `yabai-zig-cert`.

Internal `yabai_*` C/Zig symbol names are unchanged.
