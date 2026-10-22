# Release verification record (Zig migration step 5)

## macOS 27 compatibility audit (2026-10-21)

- Verified on Apple Silicon, macOS 27.0.1 (26A434), Zig 0.17.0:
  rebuilt all Zig objects and the universal `x86_64 arm64` daemon;
  `--version` prints `yabai-v7.1.25`. All 82 tests pass, including CLI
  fixtures; the test sources also pass the x86_64 syntax check.
- Runtime support is incomplete. `src/workspace.h` lists versions only
  through 26, and `workspace_event_handler_begin` matches exact versions,
  so 27 selects legacy notification and window-management paths.
  `src/osax/payload.m:verify_os_version` rejects 27 before resolving any
  Dock functions; the scripting addition has no macOS 27 support here.
- Upstream [#2800](https://github.com/asmvik/yabai/issues/2800) reports
  that version aliases alone do not restore scripting-addition Space
  operations. [#2832](https://github.com/asmvik/yabai/issues/2832) reports
  further Dock changes in 27.2, with Space moves still unavailable in
  that contributor's patch. Those patches are not included in this fork.
- Newer compatibility reference:
  [Christian-SC26/yabai](https://github.com/Christian-SC26/yabai), inspected
  at `ce962949253f741518bde442ebfd54096d5f0984` (v7.1.30, 2026-09-29).
  It has 26 commits absent from this branch, including explicit macOS 27
  version gates, SIP-enabled gesture Space switching, revised Dock
  patterns, arm64e.x1 handling and selector/NULL-guard fixes. These changes
  have not been integrated or tested here. Despite its README's full
  compatibility claim, its ARM payload still returns NULL for macOS 27
  DPPM and move-space patterns, and `do_space_move` returns without those
  targets. Moving Spaces remains unsupported in that snapshot.
- Live window management, Dock injection, service lifecycle and Intel
  execution were not tested. Build and headless test success do not
  establish full macOS 27 compatibility.

## Verified on this machine (arm64, macOS 26)

- Daemon: fat `x86_64 arm64`, macOS 11.0 min, `__TEXT,__info_plist`
  present in both slices, `--version` prints `yabai-v7.1.25`.
  All `yabai_*` Zig exports present via `nm` (38 and counting).
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

Built from a clean worktree with Zig 0.16.0 (matches zig/ZIG_VERSION).
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
