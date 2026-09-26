# Refactoring yabai in Zig

This plan describes a feature-compatible Zig migration of yabai at commit `dd84572` (yabai 7.1.25). The recommended result is a Zig daemon and CLI with a small native macOS bridge and the existing Dock scripting addition. A literal all-Zig rewrite is a separate, higher-risk project whose `arm64e` toolchain path must first be proved.

The local Zig compiler is 0.16.0. Pin the version used for the migration and validate build instructions against that version: Zig's [0.16.0 language reference](https://ziglang.org/documentation/0.16.0/) documents C imports, C ABI exports, and mixed Zig/C objects; its [release notes](https://ziglang.org/download/0.16.0/release-notes.html) describe the current toolchain and its known limitations. None of those facilities translates Objective-C source or removes yabai's macOS private API dependencies.

## What exists today

`makefile` compiles [`src/manifest.m`](../src/manifest.m), which textually includes the C and Objective-C implementation files into one translation unit. [`src/yabai.c`](../src/yabai.c) provides both the CLI and daemon entry point. Startup checks Accessibility and Spaces settings, initializes global managers, starts OS observers and an event worker, opens a Unix socket, executes the config file, then runs the Cocoa application loop.

```text
CLI -m -> /tmp/yabai_$USER.socket -> accept thread -> event queue
       -> command parser -> window/space/display managers -> macOS APIs
OS callbacks -----------------------> event queue -> layout and signals
daemon -> /tmp/yabai-sa_$USER.socket -> Dock scripting addition (optional)
```

The daemon's state is spread across globals in [`src/yabai.c`](../src/yabai.c), the window and space managers, and the BSP/stack/float view tree in [`src/view.c`](../src/view.c). The event loop serializes mutations and flushes signals after each event ([`src/event_loop.c`](../src/event_loop.c)). Command parsing is coupled to live manager state ([`src/message.c`](../src/message.c)); the view file mixes geometry with window updates and feedback drawing. Neither whole file is a pure logic module that can simply be translated in isolation.

| Area | Current code | Zig migration requirement |
| --- | --- | --- |
| CLI and daemon protocol | [`src/yabai.c`](../src/yabai.c), [`src/message.c`](../src/message.c), [`src/event_loop.c`](../src/event_loop.c) | Preserve one executable's flags, socket path, framing, command order, responses, errors, and query JSON. |
| Layout and state | [`src/view.c`](../src/view.c), [`src/space_manager.c`](../src/space_manager.c), [`src/window_manager.c`](../src/window_manager.c) | Port geometry first, then the tree and manager state; keep OS effects outside the geometry boundary. |
| Events and rules | [`src/event_loop.c`](../src/event_loop.c), [`src/rule.c`](../src/rule.c), [`src/event_signal.c`](../src/event_signal.c) | Preserve callback ordering, regex/rule effects, signal execution, and per-event temporary lifetime. |
| macOS bridge | [`src/misc/extern.h`](../src/misc/extern.h), [`src/application.c`](../src/application.c), [`src/workspace.m`](../src/workspace.m), [`src/mouse_handler.c`](../src/mouse_handler.c) | Bind C-compatible APIs; retain a narrow Objective-C shim for Cocoa/KVO, blocks, exceptions, and application lifecycle. |
| Dock scripting addition | [`src/sa.m`](../src/sa.m), [`src/osax/`](../src/osax/) | Keep its loader and payload initially; retain their bundle, handshake, and binary socket protocol. |
| Build and release | [`makefile`](../makefile), [`scripts/install.sh`](../scripts/install.sh) | Reproduce architecture slices, embedded binaries/plist, signing, service behavior, archive layout, and installer hash workflow. |

## Compatibility that must be recorded before replacement

1. **CLI and daemon wire format.** `-m` sends a native `int` byte length followed by NUL-separated arguments and a final NUL ([`src/yabai.c`](../src/yabai.c)); the daemon reads that payload on its event worker ([`src/event_loop.c`](../src/event_loop.c)). Keep `/tmp/yabai_$USER.socket`, mode `0600`, the failure marker byte `0x07`, output/exit behavior, all current option names, and the seven command domains. When rewriting the decoder, validate lengths before allocating or parsing while preserving valid messages byte for byte.
2. **Observable layout behavior.** Capture representative BSP, stack, and float layouts; split ratios and rounding; gaps and padding; window insertion/removal; focus; display/space changes; rules; and JSON query output. Use the current binary as the reference for these fixtures.
3. **Scripting addition protocol.** The daemon uses `/tmp/yabai-sa_$USER.socket`, an `OSAX_VERSION` handshake, feature bits, and binary opcodes from [`src/osax/common.h`](../src/osax/common.h). Preserve the exact native field sizes, byte order, framing, and failure behavior while the existing payload remains in use. Add checked lengths to any new Zig decoder.
4. **Event ownership and ordering.** AX, CoreGraphics, Carbon, SkyLight, and Cocoa callbacks enqueue work; the single consumer applies mutations, flushes signals, and resets temporary storage ([`src/event_loop.c`](../src/event_loop.c)). Record which callback data is retained, copied, or borrowed before moving it across the Zig/C boundary.

The current automated suite has only two display-geometry tests ([`tests/src/tests.m`](../tests/src/tests.m), [`tests/src/area.c`](../tests/src/area.c)); there is no CI workflow. A port needs a small set of characterization checks for protocol, geometry, parser/query output, and event ordering before the original implementations are removed.

## Native macOS work that Zig does not eliminate

- **Private APIs:** [`src/misc/extern.h`](../src/misc/extern.h) declares SkyLight and related symbols, including dynamically resolved ones. Zig can call C ABI symbols, but each declaration, struct layout, weak/dynamic lookup, and callback signature must match the current binary. Apple OS updates can still break those APIs.
- **Cocoa and callbacks:** [`src/workspace.m`](../src/workspace.m) uses `NSRunningApplication`, KVO, `NSWorkspace`, `NSScreen`, Objective-C exceptions, and the `NSApp` run loop. [`src/application.c`](../src/application.c) and [`src/mouse_handler.c`](../src/mouse_handler.c) install AX/CFRunLoop and event-tap callbacks. Keep Objective-C entry points and ownership rules in a narrow native bridge initially; expose plain C ABI functions/events to Zig. [`src/workspace.h`](../src/workspace.h) contains an Objective-C class declaration, so a Zig-facing C header must separate that declaration from its C functions. Some `.c` code also uses Objective-C blocks, so it cannot be split out of the current Objective-C compilation unit unchanged.
- **Dock injection:** [`src/osax/loader.m`](../src/osax/loader.m) uses Mach task/thread APIs, raw architecture-specific code, and ARM pointer authentication. [`src/osax/payload.m`](../src/osax/payload.m) scans and patches Dock internals using OS/architecture signatures. The daemon can speak to this payload from Zig without translating it. An all-Zig requirement would additionally need to recreate and validate this entire mechanism on every supported architecture and macOS release.
- **Permissions:** Accessibility, optional Screen Recording, optional partial SIP disablement for the scripting addition, and stable code signing remain deployment requirements ([`README.md`](../README.md)). A language change does not grant these permissions.

## Build requirements

The first hybrid build should compile Zig into an object with exported C ABI functions and link it with the existing `manifest.m` build. Keep `xcrun clang` for Objective-C and the scripting addition until a replacement has passed runtime tests. Do not compile the included `.c` files as separate units without first fixing their single-translation-unit assumptions, header definitions, and implicit shared state.

Preserve these artifact properties from [`makefile`](../makefile):

- Main executable: macOS 11 minimum, `x86_64` and `arm64` slices, Carbon/Cocoa/CoreServices/CoreVideo/private SkyLight linkage, and the embedded `__TEXT,__info_plist`.
- Embedded Dock loader and payload: `x86_64` and **`arm64e`** slices. The current build converts each helper to bytes and links those bytes into the main executable; [`src/sa.m`](../src/sa.m) writes the bundles at install time and signs the extracted helpers.
- Release commands: debug and optimized builds, sanitizer targets where supported, `yabai-cert` signing, manpage, icon, archive contents, and the version/hash updates used by [`scripts/install.sh`](../scripts/install.sh).
- Service identity: the existing launchd label, plist location, logs, executable path, and `--install-service`/start/stop behavior in [`src/misc/service.h`](../src/misc/service.h). Re-signing also changes the SHA-256 value users put in the `--load-sa` sudoers rule.

Use the existing Makefile to drive the first Zig object build if that is the smallest reproducible change. Introduce `build.zig` only when it can reproduce the full macOS artifact and release steps; do not switch build systems merely to begin porting code.

## Migration sequence and exit checks

1. **Freeze the baseline.** Build and sign the current binary. Record `--help`, `--version`, CLI responses, representative queries, geometry results, launchd behavior, and scripting-addition handshake/feature availability on the machines used for testing. Add focused checks for valid and malformed socket frames. *Exit:* a changed implementation can be compared against the current one.
2. **Prove the Zig/C boundary.** Pin Zig 0.16.0, link one Zig-exported geometry function into the existing daemon/test build, and call it through a small C ABI declaration. [`area_make_pair`](../src/view.c) is a useful first candidate because its inputs and outputs are plain geometry values. Match C float-to-int rounding and both architecture ABIs. *Exit:* the original tests and the new geometry checks pass with both slices.
3. **Move the core in vertical pieces.** Move geometry, then view-tree operations, then command decoding/query formatting and manager logic. For each piece, keep the existing event and macOS-effect calls at the boundary until the Zig replacement is behaviorally equivalent. Selector parsing and view flushing are coupled to live managers, so split those dependencies before replacing entire files. *Exit:* CLI fixtures, layout cases, rules, signals, and query output match the baseline.
4. **Move event and daemon ownership.** Replace global managers and the event worker with Zig-owned state only after callback payload ownership, queue ordering, temporary allocations, and shutdown behavior are explicit. Let the native Cocoa/AX/SkyLight shim post events and apply platform operations through a narrow C ABI. *Exit:* launch/relaunch, window create/destroy/focus/move, display changes, sleep/wake, Dock restart, and service lifecycle pass on real macOS sessions.
5. **Reproduce the release.** Verify each Mach-O slice and minimum OS, embedded plist/helper bytes, code signatures, installation paths, launchd service, manpage, archive structure, installer hash, and sudoers hash guidance. Test the optional scripting addition and every opcode on Intel and Apple Silicon where those features are supported. *Exit:* the Zig-led build can replace the existing release without changing user-facing behavior.

For a **literal all-Zig** outcome, first prove the compiler/linker can produce the required `arm64e` payload and reproduce its pointer-authenticated behavior. Then add a separate phase for Cocoa runtime/KVO/blocks, the Mach injector, Dock pattern matching, and per-macOS-version testing. That phase is not required to obtain a Zig daemon and is the largest maintenance risk.

## Definition of done

The refactor is complete when the Zig-led executable preserves the documented CLI/config/query behavior; tiles and reacts to OS events correctly on supported Intel and Apple Silicon systems; retains the existing optional scripting-addition features; and ships with the same installation, signing, and service contract. Until those checks pass, keep the original implementation available as the behavioral reference.
