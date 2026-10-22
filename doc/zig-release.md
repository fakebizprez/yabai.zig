# Release verification record (Zig migration step 5)

## macOS 27 compatibility backport (2026-10-21)

Source: [Christian-SC26/yabai](https://github.com/Christian-SC26/yabai) at
`ce962949253f741518bde442ebfd54096d5f0984` (v7.1.30). This backport keeps
this repository's Zig delegates and ABI definitions; unrelated session,
layout, rule and service features are not imported.

- Added explicit macOS 27 detection, modern window notifications and the
  Tahoe-compatible SkyLight request ID. Fullscreen transition waits are
  bounded, and notification storage scales with the tracked window count.
- Added SIP-enabled Space-switching gestures with the source fork's raw
  IOHID payload, direction, velocity and calibrated phase/step delays.
  Gesture preparation and payload failures propagate to command callers.
- Updated Dock signatures for arm64e/arm64e.x1 and the newer display-ID
  selector; added NULL guards. The payload was version 2.1.36 in this
  backport; it is now 2.1.37 (`OSAX_VERSION`, src/osax/common.h): every
  request except the handshake gets one status byte (0 success,
  1 failure), the daemon treats no reply as failure, and the payload
  socket sets `SO_NOSIGPIPE`. The version bump makes `--load-sa` replace
  an older payload.
  On Apple Silicon macOS 27, validation requires the available capability
  mask (0x4D), and unavailable move/front-window operations report failure.
- Entire-Space moving/reordering remains unsupported on macOS 27, as in
  the source snapshot. Window-to-Space moves use a separate path.
- Verified on Apple Silicon, macOS 27.0.1 (26A434), Zig 0.17.0:
  universal daemon and payload builds, all 91 main tests, standalone OSAX
  helper tests, and x86_64 syntax checks. The new serializer's three tests
  also pass standalone AddressSanitizer/UndefinedBehaviorSanitizer checks.
- An offline scan using the actual payload scanner matched all four
  required signatures in both installed Dock slices (arm64e and
  arm64e.x1). `tests/bin/osax27` now matches each of the four against
  byte windows captured from both slices on 27.0.1. No Dock injection
  was performed.
- Live trial on this host starts successfully and tracks displays, Spaces
  and application windows. A reported Space-switching flash exposed a
  backport timing error: preparing all phases up front compressed their
  raw HID timestamps. Phases now follow the source create/augment/post/
  delay sequence; regression tests reproduce the old failure and cover
  cancellation on allocation failure. Long jumps still flash after this
  correction; adjacent switches are smooth. Do not report the timing
  fix as fixing the flash.
- An upstream C build (v7.1.29) run with the same trial config showed
  the same flash, so the symptom is not specific to the Zig changes.
  That does not identify the cause.
- Neither comparison loaded the Dock helper, so both took the gesture
  fallback: one swipe per Space crossed, 25 ms apart, each posting
  phases 1, 2 and 4 with 1 ms delays (src/space_manager.c). A sampled
  trace of a 6-to-3 jump under the C build saw Space 5 before Space 3,
  and 4 before 6 on the way back, with foreground-application changes.
  Hypothesis: the flash is intermediate Spaces' applications showing as
  the fallback crosses them. Next step: load the Dock helper built into
  this daemon and repeat the same jump, so the switch goes through
  `scripting_addition_focus_space` instead of the fallback. Do not
  shorten the calibrated gesture delays on speculation.
- Space switching now takes the gesture fallback for `space --switch`
  and `display --space` only on macOS 27; elsewhere they fail as
  upstream did. Gesture failures report the new
  `SPACE_OP_ERROR_GESTURE` / `_GESTURE_INTERRUPTED` messages (see
  doc/zig-baseline.md). One swipe event is built before anything is
  posted, so an event-construction failure posts nothing.
- Fullscreen, sleep/wake, service lifecycle, scripting-addition injection,
  Intel execution and other macOS 27 builds remain unverified. Build and
  headless test success do not establish full runtime compatibility.

## Verified on this machine (arm64, macOS 26)

- Daemon: fat `x86_64 arm64`, macOS 11.0 min, `__TEXT,__info_plist`
  present in both slices, `--version` prints `yabai-v7.1.25`.
  All `yabai_*` Zig exports present via `nm` (107 at 2026-10-22).
- Debug binary is adhoc-signed as expected; release signs
  `yabai-cert` via `make sign` (cert not present here, not attempted).
- Scripting-addition helpers rebuild to `x86_64 arm64e` from the
  pinned makefile commands (payload shared/fPIC, loader), matching
  the slices `src/sa.m` embeds and signs at install time.
- Service contract (src/misc/service.h, read-only audit): label
  `com.asmvik.yabai`, `~/Library/LaunchAgents`, logs
  `/tmp/yabai_$USER.out/.err.log`, exe path + PATH baked into the
  plist, RunAtLoad + KeepAlive(Crashed), `enable/bootstrap/kickstart`
  start flow, `bootout/disable` stop flow. A plist is installed on
  this machine; install/start/stop were NOT exercised to avoid
  touching the live service.
- Live baseline through the real wire format against the running
  release daemon: `query --spaces --display` returns labeled bsp
  spaces with windows/focus/visibility fields; `query --windows
  --space` returns full window JSON (frame floats, role/subrole,
  layer, opacity, split fields); `query --displays` returns `[]`
  here (no displays visible in this session), exit 0 throughout.
- Installer pins `VERSION="7.1.25"` and a sha256
  `EXPECTED_HASH` (scripts/install.sh); `make publish` refreshes
  both from the built binary and tarball.
- `asciidoctor` is not installed here, so `make man` was not run;
  `codesign`, `lipo`, `python3` are present.

## Remaining matrix (needs hardware runs, not done)

- Intel + Apple Silicon: launch/relaunch, window
  create/destroy/focus/move, display changes, sleep/wake, Dock
  restart, service lifecycle, every SA opcode, on both architectures
  where supported.
- Full `make archive` (man, install, sign with `yabai-cert`, icon,
  tarball) plus installer-hash refresh and sudoers SHA-256 guidance
  re-check after any re-sign.
- The Zig-led build must reproduce: slice list and min-OS per Mach-O,
  embedded plist bytes, helper bytes, signatures, plist paths/logs,
  manpage, archive layout.

## Audit 2026-09-25 (refactor/zig @ 8fac008, arm64 macOS 27.0, debug `make`)

Built from a clean worktree with Zig 0.16.0 (zig/ZIG_VERSION at the time;
it is 0.17.0 now). The OSAX and makefile findings below held at 8fac008
and no longer do; see "Since this audit" at the end of the section.
All build outputs are gitignored (`/bin`, `/zig/zig-*.o`,
`/src/osax/{payload,loader}_bin.c`); `git status` shows no tracked
modifications besides this file.

- `lipo -info bin/yabai` -> `Architectures in the fat file:
  bin/yabai are: x86_64 arm64`.
- `./bin/yabai --version` -> `yabai-v7.1.25`.
- `otool -l bin/yabai`: `LC_BUILD_VERSION`, `minos 11.0` in both
  slices; `__TEXT,__info_plist` present in both slices (size 0x286).
- `otool -L bin/yabai`: Carbon, Cocoa, CoreServices, CoreVideo, and
  SkyLight (PrivateFrameworks) linked in both architectures.
- `codesign -dv bin/yabai`: `Signature=adhoc`,
  `flags=0x20002(adhoc,linker-signed)` — expected for a debug `make`
  build; release signs `yabai-cert` via `make sign` (cert not present
  here, not attempted).
- `nm -g bin/yabai | grep -c yabai_` -> 95 Zig exports.
- OSAX flow untouched: `git diff master...HEAD` is empty for
  `src/osax/`, `src/sa.m` (bundle-writing: `payload.bundle` layout,
  loader/payload adhoc codesign at install time), and the makefile
  OSAX rule still builds payload shared/fPIC and loader with
  `-arch x86_64 -arch arm64e` (observed in this build log).
- Makefile diff on the branch is Zig-only: `ZIG_SRC`/`ZIG_OBJ`,
  linking `$(ZIG_OBJ)` into `YABAI_SRC`, one build rule, one clean
  line. `install` flags (`-arch x86_64 -arch arm64`,
  `-sectcreate __TEXT __info_plist`), `man`/`archive`/`publish`/`sign`
  targets unchanged.
- Unchanged on the branch (empty `git diff master...HEAD`):
  `scripts/install.sh` (`VERSION="7.1.25"`, `EXPECTED_HASH`,
  sudoers guidance at lines 85-89: `visudo -f
  /private/etc/sudoers.d/yabai`, `NOPASSWD: sha256:... --load-sa`),
  `src/misc/service.h` (label `com.asmvik.yabai`,
  `~/Library/LaunchAgents`, `/tmp/yabai_%s.out/err.log`, RunAtLoad +
  KeepAlive), `doc/yabai.1` / `doc/yabai.asciidoc`.
  `asciidoctor` still not installed here, so `make man` was not run.
- Since this audit: the macOS 27 backport and the Dock status-byte change
  modified `src/osax/` (payload.m, arm64_payload.m, common.h) and
  `src/sa.m`. The makefile now also has the Zig stamp rule and extra
  prerequisites (every `src/` header and source, the plist, and the OSAX
  inputs); compile and link flags are unchanged. `scripts/install.sh`,
  `src/misc/service.h` and the man page sources are still unchanged
  against `master` (checked 2026-10-22).

## Easing curves: debug and release C differ

The upstream C `ease_*` functions (src/misc/helpers.h) compile
differently at the debug `-O0` and release `-O3` levels: at `-O3` clang
replaces `powf(2.0f, x)` with `exp2f` and inlines `powf(x, 2.0f)` as a
multiply. On arm64, comparing the two builds bit for bit over 10^6
points in [0, 1] gives different results for ease_in_out_quad,
ease_in_circ, ease_out_circ and ease_in_out_circ (2026-10-22); the
other 17 match. zig/util.zig matches the `-O0` build, which is what the
parity vectors were taken from. Easing is not routed into the daemon,
so neither daemon build runs the Zig curves.

## Explicitly unverified on this machine (needs hardware runs)

- Scripting-addition opcodes end-to-end (requires SIP-disabled test
  host with SA installed; only the bytes flow and build commands were
  audited here).
- arm64e payload slice at runtime (built with `-arch arm64e` per the
  unchanged rule, but only assembled into `_bin.c` blobs here).
- `make archive` / installer tarball hash refresh and `make publish`
  (`EXPECTED_HASH` re-check after a real `yabai-cert` re-sign).
- sudoers SHA-256 guidance re-check after re-sign.
- Intel hardware: launch/relaunch, window lifecycle, display
  changes, sleep/wake, Dock restart, service lifecycle on x86_64.
