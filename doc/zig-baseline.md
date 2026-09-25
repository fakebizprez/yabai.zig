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
- Existing suite: `cd tests && make` runs 19 tests, all passing
  (2 display geometry + 8 socket wire-format/dispatcher characterization
  in tests/src/socket_frame.c + 3 geometry parity in
  tests/src/zig_geometry.c + 6 tokenizer/command-decoding parity in
  tests/src/zig_message.c).

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
  hand-pinned constant tables. 14 exports in the daemon; x86_64 slice
  verified.

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
