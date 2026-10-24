# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

This is a fork of [asmvik/yabai](https://github.com/asmvik/yabai) (7.1.25, commit `dd84572`) being migrated to Zig piece by piece, plus macOS 27 backports from [Christian-SC26/yabai](https://github.com/Christian-SC26/yabai). The C/Objective-C daemon is still the program; pure logic is moved into Zig objects that the C code calls through a C ABI.

## Commands

- `make`: debug universal (`x86_64` + `arm64`) build to `bin/yabai-zig`. Always deletes `bin/` first. Reruns `zig/build.sh` once (stamp file `zig/bin/build.stamp`, safe under `make -j`) when any `zig/` source, `zig/build.sh`, `src/zig_abi.h`, or `zig/ZIG_VERSION` is newer.
- `make install`: optimized build only (`-O3 -DNDEBUG`). It does not install anything on the system.
- `make asan` / `make tsan`: sanitizer builds.
- `./zig/build.sh`: builds only the Zig objects (`zig/zig-<mod>.o`, universal via `lipo`). Fails if `zig version` differs from `zig/ZIG_VERSION` (currently 0.17.0) or if the C and Zig ABI checks disagree (constants, prototypes, enum cases).
- `cd tests && make`: clean, rebuild `bin/yabai-zig` if stale, build `tests/bin/tests`, an `x86_64` copy `tests/bin/tests-x86_64`, and `tests/bin/osax27` (Dock payload helper checks, type-checked for `x86_64`), then run `tests`, `osax27`, and `tests-x86_64` under Rosetta (skipped with a message when Rosetta is not installed). Tests run headless.
- `make -C tests build run`: same without the clean. `make -C tests osax27`: payload helper checks only.
- Single test: there is no filter; `tests/bin/tests` ignores its arguments and runs every entry in `TEST_LIST` (`tests/src/tests.m`). Trim that list locally to run one.
- `make clean`: also removes the generated Dock helper blobs (`src/osax/{payload,loader}_bin.c`) and Zig objects.
- `make sign` needs a `yabai-zig-cert` signing identity; `make man` needs `asciidoctor`. There is no CI.

To check a change against a real daemon, use the `verify` skill (`.claude/skills/verify/SKILL.md`): it runs a throwaway daemon under a different `USER`, so it gets its own socket and leaves the live one alone.

The development machine runs a live yabai-zig. Do not run `--install-service`, `--start-service`, `--restart-service`, `--load-sa`, or `-m` commands against the running daemon without asking.

## Architecture

**One executable, one translation unit.** `src/yabai-zig.c` is both the CLI client and the daemon entry point. `src/manifest.m` `#include`s every `.c`/`.m` file into a single compilation unit; the makefile compiles only `manifest.m` plus the Zig objects and the embedded Dock helper blobs. A new C source file must be included from `manifest.m`; do not compile the included files separately (they rely on shared statics and header definitions).

**Daemon flow.** `yabai-zig -m ...` sends a framed message to `/tmp/yabai-zig_$USER.socket` (native `int` length, NUL-separated args, final NUL; responses starting with byte `0x07` mean failure). The accept thread posts a `DAEMON_MESSAGE` event. OS callbacks (AX, CoreGraphics, SkyLight, Cocoa/KVO in `workspace.m`, the mouse event tap) also post events to a lock-free multi-producer queue in `src/event_loop.c`. A single consumer handles each event, then flushes signals (`event_signal.c`) and resets per-event temporary storage (`misc/ts.h`). `src/message.c` parses the seven command domains (`config`, `display`, `space`, `window`, `query`, `rule`, `signal`). Global managers (`g_window_manager`, `g_space_manager`, `g_display_manager`, ...) hold state; `src/view.c` holds each space's BSP/stack/float tree. Private SkyLight APIs are declared in `src/misc/extern.h`. Ordering and ownership rules a port must keep are in `doc/zig-event-ownership.md`.

**Dock scripting addition.** `src/sa.m` talks to an optional payload injected into Dock.app over `/tmp/yabai-zig-sa_$USER.socket` (protocol in `src/osax/common.h`). `src/osax/loader.m` and `payload.m` are built for `x86_64` + `arm64e` and embedded into the daemon as byte arrays via `xxd` (generated, gitignored). `payload.m` matches Dock code by per-macOS, per-architecture byte signatures.

**macOS 27 additions.** Version detection, modern window notifications, and Space-switching gestures that work with SIP enabled. `src/event_serialize.c` writes Apple's private CGEvent data format so a raw IOHID payload can be attached to synthetic Dock-swipe events. Tested scope and open items are in `doc/zig-release.md`.

**Zig boundary.** `zig/*.zig` are mostly pure-logic modules (geometry, message tokenizing/frame decoding, view tree, util/easing, rule parse/match/serialize, window math, display serialize, mouse drop action); `rule.zig` and `view.zig` also call back into C manager functions and globals. Each is built `ReleaseSafe` for both architectures and needs no Zig runtime (exports call `@setRuntimeSafety(false)`). The C functions keep their names and bodies become thin calls to `yabai_*` exports; `doc/zig-baseline.md` has the routing map of what the daemon actually runs in Zig and what is deliberately left in C.

- `src/zig_abi.h` has every `yabai_*` prototype and literal layout pins for each shared struct: `ZIG_ABI_STRUCT` (size and member count), `ZIG_ABI_FIELD` / `ZIG_ABI_FIELD_FP` (offset, size, float-ness per member), `ZIG_ABI_SIZE` for partly mirrored or opaque types. `zig/abi.zig` mirrors the structs and repeats the same literals with `pin(...)`; nothing compares the two lists, so change both by hand. Layout drift fails whichever side's build. `zig/build.sh` also checks each prototype against its export (name, parameter count, width class).
- Integer constants: `pub const NAME: T = N;` in `zig/abi.zig` must have a matching `_Static_assert(C_NAME == N, "NAME");` line in `src/zig_abi.h`; `zig/build.sh` checks the pairs both ways by regex, so keep that one-line format.
- `zig_abi.h` is included twice from `manifest.m`; the second pass (`ZIG_ABI_PRIVATE_PINS`) covers types private to `src/message.c`.
- A new Zig module must be added to the module loop in `zig/build.sh`, `ZIG_SRC`/`ZIG_OBJ` in `makefile`, and `ZIG_OBJ` plus the `build.stamp` dependency line in `tests/makefile`.
- Ports must match C bit for bit, including float rounding. Apple clang fuses multiply-adds even at `-O0`, so the easing port uses explicit `@mulAdd` at those sites.

## Tests

- `tests/src/tests.m` includes `src/manifest.m` (built with `-DTESTS`), then `#include`s each test file; every test is a `TEST_FUNC` registered in `TEST_LIST`. It stubs the embedded helper blobs and redirects `CGEventPost`/`CGEventCreate` to capture functions.
- Parity tests compare each routed Zig export against a frozen upstream C copy in `tests/src/ref_upstream_view.c` / `ref_upstream_rule.c` (`ref_` prefix, verbatim from `dd84572`); unrouted helpers are compared against the live C. Because the routed C functions now call Zig, comparing against them would test Zig against itself. Never edit the `ref_` bodies to make a test pass; a mismatch means the port is wrong.
- Headless tests cannot call SkyLight (the connection is 0 and calls abort), so those calls are split out as parameters (seams listed at the top of each `ref_upstream_*.c`; the rule copies route manager queries through `struct yabai_rule_lookup`, the same table `yabai_parse_rule` takes).
- `tests/src/zig_cli.c` runs the built `bin/yabai-zig` against goldens in `tests/data/`; after a version bump, regenerate `help.txt` (and `version.txt`, `m_no_args.txt` if they changed) using the procedure in that file's header.
- The CLI/daemon wire format, help text, and query JSON are frozen to the upstream baseline (`doc/zig-baseline.md`); any change has to be deliberate and documented there.
