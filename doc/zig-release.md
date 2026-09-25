# Release verification record (Zig migration step 5)

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
