# Verification record

What has been tested, what is known to be broken, and what has not been
tried. Build and headless test success do not establish full runtime
compatibility.

## Tested

On Apple Silicon, macOS 27.0.1 (26A434), Zig 0.17.0:

- `make` and `make install` produce a universal daemon (`x86_64 arm64`,
  macOS 11.0 minimum, `__TEXT,__info_plist` in both slices) that is
  ad-hoc signed. `--version` prints `yabai-zig-v7.1.25`, and every
  `yabai_*` Zig export is present in both slices (`nm`).
- The Dock scripting-addition helpers build for `x86_64 arm64e` and are
  embedded in the daemon.
- `cd tests && make` passes: the main suite, the `osax27` Dock payload
  helper checks, and the x86_64 copy of the suite under Rosetta. The CGEvent
  serializer tests also pass under AddressSanitizer and
  UndefinedBehaviorSanitizer.
- An offline scan with the payload's own scanner matched all four
  required byte signatures in both installed Dock slices (arm64e and
  arm64e.x1). No Dock injection was performed.
- A live daemon starts, tracks displays, Spaces and windows, and answers
  `query --spaces`, `query --windows` and `query --displays` over the
  real wire format.

## macOS 27 backport

Source: [Christian-SC26/yabai](https://github.com/Christian-SC26/yabai) at
`ce962949253f741518bde442ebfd54096d5f0984` (v7.1.30). Only the macOS 27
changes were taken; that fork's unrelated session, layout, rule and
service features were not.

- macOS 27 detection, modern window notifications and the
  Tahoe-compatible SkyLight request ID. Fullscreen transition waits are
  bounded, and notification storage scales with the tracked window count.
- Space-switching gestures that work with SIP enabled, using a raw IOHID
  payload attached through the CGEvent serializer
  (src/event_serialize.c). `space --switch` and `display --space` use them
  only on macOS 27; elsewhere they fail as upstream did. Gesture failures
  report `SPACE_OP_ERROR_GESTURE` / `_GESTURE_INTERRUPTED` (see
  doc/zig-baseline.md).
- Dock signatures for arm64e and arm64e.x1 and the newer display-ID
  selector. The payload is version 2.1.37 (`OSAX_VERSION`,
  src/osax/common.h): every request except the handshake gets one status
  byte (0 success, 1 failure), no reply counts as failure, and the payload
  socket sets `SO_NOSIGPIPE`. On Apple Silicon macOS 27, validation
  requires the capability mask 0x4D, and unavailable move/front-window
  operations report failure.

## Known issues

- Switching across several Spaces at once flashes the Spaces in between
  when the scripting addition is not loaded. Adjacent switches are smooth.
  Upstream C yabai v7.1.29 shows the same flash on the same machine, so it
  is not caused by the Zig port. Loading the scripting addition, which
  switches through `scripting_addition_focus_space` instead of the gesture
  fallback, has not been tried yet.
- Moving or reordering entire Spaces is unsupported on macOS 27, as in
  the source fork. Moving windows between Spaces works.
- Easing curves are not routed to Zig. Upstream's C `ease_*` functions
  give different results at `-O0` and `-O3` (clang rewrites `powf` at
  `-O3`) for ease_in_out_quad and the three circ curves. zig/util.zig
  matches the `-O0` build, so routing it would change release-build
  animation by a few ULPs.

## Not verified

- Intel hardware: launch, window lifecycle, display changes, sleep/wake,
  Dock restart and the launchd service.
- Fullscreen, sleep/wake and other macOS 27 builds on Apple Silicon.
- The launchd service lifecycle (`--install-service`, `--start-service`,
  `--stop-service`).
- Every scripting-addition opcode end to end. This needs a test machine
  with SIP partly disabled and the scripting addition loaded.
- `make archive` and `make man`; `asciidoctor` was not installed on the
  test machine.
- Signing with a `yabai-zig-cert` identity (`make sign`).
