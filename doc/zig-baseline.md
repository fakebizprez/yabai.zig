# Zig migration baseline (yabai 7.1.25, commit dd84572)

Recorded from a clean `make` build with the pinned toolchain below. Any
Zig-led replacement must reproduce everything in this file byte for byte
unless doc/zig-refactor-plan.md explicitly allows a change.

## Toolchain

- `zig version`: 0.16.0 (pinned for the migration)
- `xcrun clang` from system Xcode; macOS minimum 11.0 (`-mmacosx-version-min=11.0`)

## Reference binary

- `./bin/yabai --version` prints `yabai-v7.1.25`, exit 0.
- `./bin/yabai --help` prints the option list in src/yabai.c
  (`--load-sa`, `--uninstall-sa`, `--install-service`,
  `--uninstall-service`, `--start-service`, `--restart-service`,
  `--message/-m`, `--config/-c`, `--verbose/-V`, `--version/-v`,
  `--help/-h`) plus the `man yabai` pointer.
- `lipo -info ./bin/yabai`: fat file with `x86_64 arm64` slices.
- Existing suite: `cd tests && make` runs 38 tests, all passing
  (2 display geometry + 8 socket wire-format/dispatcher characterization
  in tests/src/socket_frame.c + 3 geometry parity in
  tests/src/zig_geometry.c + 8 tokenizer/command-decoding parity in
  tests/src/zig_message.c + 6 view-tree parity in tests/src/zig_view.c
  + 6 pure-helper parity in tests/src/zig_util.c + 3 rule-effects
  parity in tests/src/zig_rule.c + 2 window-math parity in
  tests/src/zig_window.c).

## Step 2 boundary evidence

- `zig/geometry.zig` (pinned Zig 0.16.0, enforced by tests/makefile)
  exports `_yabai_area_make_pair` with a C ABI; the tests/makefile
  builds it with `zig build-obj` and links it into `tests/bin/tests`.
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
  Zig returns caller-frees memory — routing would leak).
- Rule call-site routing: both handle_domain_rule parse sites call
  Zig with eager acting ids; C parse_rule stays as the live oracle.
  71/71 tests; leaks 0; fat daemon, 101 exports.
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

- `zig/build.sh` (pin: zig/ZIG_VERSION = 0.16.0) compiles
  zig/geometry.zig and zig/message.zig with `-O ReleaseSafe` for
  x86_64-macos.11.0 and aarch64-macos.11.0 (minos 11.0 on both slices)
  and lipos them into zig/zig-geometry.o and zig/zig-message.o.
- The root makefile links both objects into `bin/yabai` (still a fat
  x86_64+arm64 binary, `--version` unchanged); all 11 `yabai_*`
  exports are present in the daemon. The objects need no Zig runtime
  (geometry: zero external symbols; message: libc `strlen`/`strtof`
  plus stack guard only).
- tests/makefile links these same universal objects, so the 17-test
  parity suite proves the shipped ReleaseSafe objects directly.

## CLI/daemon wire format (frozen)

- Socket path: `/tmp/yabai_$USER.socket` (SOCKET_PATH_FMT in src/yabai.c),
  created with mode `0600` (message_loop_begin in src/message.c).
- Accept thread (message_loop_run) hands each fd to the event worker;
  the DAEMON_MESSAGE handler in src/event_loop.c reads a native `int`
  byte length, then loops until exactly that many payload bytes arrive.
- Encoder (client_send_message in src/yabai.c):
  `length = argc + sum(strlen(argv[i]))`; frame = native `int` length,
  then each argument bytes plus `'\0'`, then one final `'\0'`.
  Example: `yabai query --windows` encodes a 17-byte payload
  `query\0--windows\0\0`.
- Failure marker: responses starting with byte `0x07` (FAILURE_MESSAGE in
  src/misc/macros.h) go to stderr and force a non-zero CLI exit.
- The seven command domains (src/message.c): `config`, `display`,
  `space`, `window`, `query`, `rule`, `signal`.
- Checked-decode contract for the Zig replacement: reject truncated
  length prefixes, short or over-long payloads, non-positive or absurd
  (>1 MiB) declared lengths, and payloads whose last byte is not NUL,
  all without allocating. Covered by the `socket_frame_malformed` test.

## Known current weakness (must not be carried over)

The DAEMON_MESSAGE handler allocates `bytes_to_read` bytes via
`ts_alloc_unaligned` with no validation of the declared length. The Zig
decoder must add the checked lengths above while preserving every valid
message byte for byte.
