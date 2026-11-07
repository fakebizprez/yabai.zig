# Zig migration baseline (yabai 7.1.25, commit dd84572)

Recorded from a clean `make` build with the pinned toolchain below. Any
Zig-led replacement must reproduce everything in this file byte for byte,
except for the changes listed under "Deliberate output changes" and
"Renamed to yabai-zig". Parity tests compare routed exports against the
frozen dd84572 copies in tests/src/ref_upstream_*.c.

## Toolchain

- `zig version`: 0.17.0, pinned in zig/ZIG_VERSION and enforced by
  zig/build.sh (the migration started on 0.16.0)
- `xcrun clang` from system Xcode; macOS minimum 11.0 (`-mmacosx-version-min=11.0`)

## Reference binary

- `./bin/yabai-zig --version` prints `yabai-zig-v7.1.25`, exit 0.
- `./bin/yabai-zig --help` prints the option list in src/yabai-zig.c
  (`--load-sa`, `--uninstall-sa`, `--install-service`,
  `--uninstall-service`, `--start-service`, `--restart-service`,
  `--stop-service`, `--message/-m`, `--config/-c`, `--verbose/-V`, `--version/-v`,
  `--help/-h`) plus the `man yabai-zig` pointer.
- `lipo -info ./bin/yabai-zig`: fat file with `x86_64 arm64` slices.
- Suite: `cd tests && make` runs the 106 entries in `TEST_LIST`
  (tests/src/tests.m) as `tests/bin/tests`, then the `tests/bin/osax27`
  Dock payload helper checks, then the same suite built for x86_64
  (`tests/bin/tests-x86_64`) under Rosetta, or prints a skip line when
  Rosetta is not installed.

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
