---
name: verify
description: Run a throwaway yabai-zig daemon on its own socket and drive it through the CLI and raw socket frames.
---

# Verify yabai-zig changes at the CLI/socket

1. `make` (builds `bin/yabai-zig`; reruns `zig/build.sh` when needed).
2. Check no live daemon is running: `ps aux | grep -i yabai-zig`. A stale `/tmp/yabai-zig_$USER.socket` can exist with no process.
3. Start an isolated daemon. A different `USER` gives it its own socket and lock file; an empty config keeps `layout float`, so it moves no windows:
   ```sh
   : > "$SCRATCH/empty_yabai-zigrc"; chmod +x "$SCRATCH/empty_yabai-zigrc"
   USER=yzverify ./bin/yabai-zig --verbose -c "$SCRATCH/empty_yabai-zigrc" > "$SCRATCH/daemon.log" 2>&1 &
   ```
   The socket `/tmp/yabai-zig_yzverify.socket` takes 30 to 60 seconds to appear (window discovery runs first). Poll for it.
4. Drive it: `USER=yzverify ./bin/yabai-zig -m config layout`, `query --spaces --space`, `rule --add/--list/--remove`.
5. Raw frames: Python `socket.AF_UNIX` to the socket, frame = `struct.pack("=i", len(body)) + body`, body = args each NUL-terminated plus one final NUL. Malformed frames get a silent close (empty reply).
6. Stop with `kill <pid>`; delete `/tmp/yabai-zig_yzverify.{lock,socket}`.

Do not drive `space --focus`/`--switch` (moves the real desktop) or anything needing `--load-sa` without asking.
