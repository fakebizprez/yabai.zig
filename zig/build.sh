#!/bin/sh
# Build the Zig boundary objects as universal (x86_64 + arm64) Mach-Os.
# Pinned toolchain: zig/ZIG_VERSION. Fails if `zig version` does not match.
# ReleaseSafe keeps strict IEEE float semantics (verified by the parity
# suite, which links these same objects); the exports disable runtime
# safety internally so no Zig runtime is required at link time.
# Usage: ./zig/build.sh (from repo root) or ../zig/build.sh (from tests/).
set -eu

case "$0" in
  */*) SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd);;
  *) SCRIPT_DIR=$(pwd);;
esac
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
ZIG_DIR="$ROOT_DIR/zig"
BIN_DIR="$ZIG_DIR/bin"

pinned=$(cat "$ZIG_DIR/ZIG_VERSION")
actual=$(zig version)
if [ "$actual" != "$pinned" ]; then
  echo "zig version mismatch: have $actual, pinned $pinned" >&2
  exit 1
fi

mkdir -p "$BIN_DIR"
for mod in geometry message view util rule window display; do
  zig build-obj "$ZIG_DIR/$mod.zig" -target x86_64-macos.11.0 -O ReleaseSafe --name "$mod" -femit-bin="$BIN_DIR/$mod-x86_64.o"
  zig build-obj "$ZIG_DIR/$mod.zig" -target aarch64-macos.11.0 -O ReleaseSafe --name "$mod" -femit-bin="$BIN_DIR/$mod-arm64.o"
  lipo -create "$BIN_DIR/$mod-x86_64.o" "$BIN_DIR/$mod-arm64.o" -output "$ZIG_DIR/zig-$mod.o"
done
lipo -info "$ZIG_DIR"/zig-*.o
