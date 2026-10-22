# Space switching flashes on macOS 27

Updated: 2026-10-21. Status: unresolved.

## Symptom and reproduction

On Apple Silicon running macOS 27.0.1 (26A434), jumping across several
Spaces briefly shows applications from other Spaces and looks jumpy.
The user reports that adjacent Space switches are smooth.

Use the user's skhd shortcuts to jump several Spaces, for example
Command–3 from Space 6, then Command–6 to return. Relevant bindings:

```text
cmd - 3 : yabai -m space --focus 3
cmd - 6 : yabai -m space --focus 6
ctrl - left  : yabai -m space --focus prev
ctrl - right : yabai -m space --focus next
```

Control–Arrow is handled by skhd, and the native macOS shortcut is disabled.
It is therefore not an independent test of native Space switching.
Trackpad switching has not been compared.

## What is running now

- Zig build: `/Users/fakebizprez/.local/share/yabai-zig-test/yabai`.
- Last verified daemon PID: `5889`; PIDs can change after a restart.
- Config: `/Users/fakebizprez/.local/share/yabai-zig-test/yabairc`.
- `/usr/local/bin/yabai` is now a symlink to that Zig executable.
- The process executable path was checked with `ps` and `lsof`, the binary
  contains the Zig delegate symbols, and a Space query succeeds through
  the normal `yabai` command.
- Code-signing identifier: `com.asmvik.yabai.zig-test` (ad hoc).
- The daemon remains a manually launched trial; login startup is not configured.
- The Dock scripting addition is neither installed nor loaded now.

The following non-Zig installs were removed at the user's request:

- Original `/usr/local/bin/yabai`, C v7.1.29. Its path now points to Zig.
- `~/.local/bin/yabai-sa-macos27`, C v7.1.25.
- Their old launch-agent files, the old installed man page, and
  `/Library/ScriptingAdditions/yabai.osax` (payload v2.1.35).

The user's `.yabairc`, `.skhdrc`, and repository sources were preserved.
Do not attempt to repeat the C comparison using `/usr/local/bin/yabai`:
that command now executes Zig.

## Investigation and evidence

### A timing defect was corrected, but the visual issue persists

The initial backport created all three swipe-phase events before posting
them. This compressed their embedded HID timestamps even though posts
were separated by delays. The code now creates, augments, posts, and
releases each phase immediately before its delay, matching the source
fork. Allocation failure after a gesture begins attempts phase-8 cancellation.

Regression tests reproduced the old timing failure and pass after the
correction. The latest implementation verification recorded 91/91 main
tests and the standalone OSAX helper tests passing.

The user still observed flashing afterward. This correction must not be
reported as fixing the flashing.

### The installed C version reproduced the same symptom

For a controlled comparison, the Zig daemon was temporarily stopped and
the original C v7.1.29 daemon was launched with the same trial config.
The user confirmed: **"Same jumping or flashing."**

This demonstrates that the symptom is not exclusive to the Zig changes.
It does not prove the underlying cause or establish that direct switching
through the Dock helper works. The C comparison is over; Zig was restored
before the other builds were uninstalled.

### Long jumps activate intermediate Spaces

A read-only probe sampled the actual current Space and foreground PID.
With the C comparison daemon, a Space 6 → 3 jump observed Space 5 before
Space 3. The return observed Space 4 before Space 6. Foreground application
changes accompanied these transitions. Not every intermediate Space was
captured by the sampling.

This matches the gesture fallback in `src/space_manager.c`: one swipe per
Space crossed, with a 25 ms gap between swipes on macOS 27. Each swipe
posts phases 1, 2, and 4, separated by 1 ms delays.

The direct path first calls `scripting_addition_focus_space`; if that fails,
it falls back to gestures. The trial config deliberately commented out
the normal config's `sudo yabai --load-sa` and Dock-restart reload hook.
No helper socket was present during either daemon's comparison.

**Current hypothesis:** the flashes are intermediate applications becoming
visible as the gesture fallback crosses several Spaces. The trace and
both-daemon reproduction support this, but the hypothesis still needs a
comparison against direct switching with the Zig build's Dock helper.

## Other observations

- `window_animation_duration` is 0; `skip_window_focus_animation` is off.
- Focus follows mouse is disabled; mouse follows focus is on.
- Automatically rearranging Spaces is off; displays have separate Spaces is on.
- A `space_changed` signal launches a Raycast menu-bar extension in the
  background. It has not been disabled for a controlled comparison.
- Repeated unresolved-window discovery messages exist for some applications;
  no causal connection to this flash has been established.
- No new crash was established as the cause of the live switching symptom.

## Next investigation

1. Verify the daemon executable path before further live comparisons.
2. Keep the current config and other hooks fixed. Install/load the helper
   embedded in the Zig build, then repeat the same multi-Space jump and
   record actual Space transitions plus the user's visual result.
3. The old helper has been removed. Loading the Zig helper requires an
   administrator operation and can restart Dock during installation.
   Previous approval to load the old existing helper did not complete that
   operation; the attempted `sudo -n` required a password.
4. If flashing persists with confirmed direct switching, isolate the
   Raycast signal next, saving and restoring its exact action. Change one
   variable at a time and correlate window visibility/bounds with Space
   and foreground changes before making another production-code change.

Do not shorten the calibrated gesture delays speculatively or treat
headless test success as proof that the visual issue is resolved.

## Evidence locations

- Trial log: `~/.local/share/yabai-zig-test/trial.log`.
- C comparison log: `~/.local/share/yabai-zig-test/reference.log`.
- Adjacent-switch trace: `/tmp/yabai-gesture-trace-current.json`.
- C long-jump trace: `/tmp/yabai-gesture-trace-reference-long.json`.
- Timing regression test results: `/tmp/yabai-swipe-timing-green.log`.
- Compatibility/build record: `doc/zig-release.md`.

Files in `/tmp` are temporary and may disappear. The compatibility record's
reference to a pending comparison with the "existing Dock helper" predates
the uninstall; use the current installation state documented here.

The macOS 27 backport uses Christian-SC26/yabai commit
`ce962949253f741518bde442ebfd54096d5f0984` (v7.1.30) as its reference.
Offline Dock signature scans passed, but live injection remains unverified.
Entire-Space moving/reordering is still unsupported on macOS 27 in this
backport, independently of the flashing issue.
