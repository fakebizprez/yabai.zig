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

# Value pins: every `_Static_assert(C_NAME == N, "ZIG_NAME");` line in
# src/zig_abi.h must match `pub const ZIG_NAME: T = N;` in zig/abi.zig,
# and every such abi.zig constant must be pinned there. Layouts are
# pinned by literal asserts on both sides instead.
ABI_ZIG="$ZIG_DIR/abi.zig"
ABI_H="$ROOT_DIR/src/zig_abi.h"
c_pins=$(sed -nE 's/^_Static_assert\([A-Za-z_][A-Za-z_0-9]* == ([^,]+), "([A-Z_][A-Z_0-9]*)"\);$/\2 \1/p' "$ABI_H")
z_consts=$(sed -nE 's/^pub const ([A-Z_][A-Z_0-9]*): [a-z_0-9]+ = ([^;]+);$/\1 \2/p' "$ABI_ZIG")
abi_fail=0
for name in $(printf '%s\n%s\n' "$c_pins" "$z_consts" | cut -d' ' -f1 | sort -u); do
  c_val=$(printf '%s\n' "$c_pins" | awk -v n="$name" '$1 == n { $1 = ""; print; exit }')
  z_val=$(printf '%s\n' "$z_consts" | awk -v n="$name" '$1 == n { $1 = ""; print; exit }')
  if [ -z "$c_val" ]; then
    echo "abi drift: $name in zig/abi.zig has no value pin in src/zig_abi.h" >&2; abi_fail=1
  elif [ -z "$z_val" ]; then
    echo "abi drift: $name pinned in src/zig_abi.h is missing from zig/abi.zig" >&2; abi_fail=1
  elif [ $((c_val)) -ne $((z_val)) ]; then
    echo "abi drift: $name is$c_val in src/zig_abi.h but$z_val in zig/abi.zig" >&2; abi_fail=1
  fi
done
[ "$abi_fail" -eq 0 ] || exit 1

mkdir -p "$BIN_DIR"
for mod in geometry message view util rule window display mouse; do
  zig build-obj "$ZIG_DIR/$mod.zig" -target x86_64-macos.11.0 -O ReleaseSafe --name "$mod" -femit-bin="$BIN_DIR/$mod-x86_64.o"
  zig build-obj "$ZIG_DIR/$mod.zig" -target aarch64-macos.11.0 -O ReleaseSafe --name "$mod" -femit-bin="$BIN_DIR/$mod-arm64.o"
  lipo -create "$BIN_DIR/$mod-x86_64.o" "$BIN_DIR/$mod-arm64.o" -output "$ZIG_DIR/zig-$mod.o"
done
lipo -info "$ZIG_DIR"/zig-*.o
