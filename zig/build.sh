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
if [ ! -f "$SCRIPT_DIR/ZIG_VERSION" ]; then
  echo "usage: ./zig/build.sh (from the repo root) or ../zig/build.sh (from tests/)" >&2
  exit 1
fi
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
ZIG_DIR="$ROOT_DIR/zig"
BIN_DIR="$ZIG_DIR/bin"

pinned=$(cat "$ZIG_DIR/ZIG_VERSION")
actual=$(zig version)
if [ "$actual" != "$pinned" ]; then
  echo "zig version mismatch: have $actual, pinned $pinned" >&2
  exit 1
fi

ABI_ZIG="$ZIG_DIR/abi.zig"
ABI_H="$ROOT_DIR/src/zig_abi.h"
abi_fail=0
drift() { echo "abi drift: $*" >&2; abi_fail=1; }

# Value pins. In src/zig_abi.h, every _Static_assert whose message is a
# bare name is a value pin and must read exactly
#     _Static_assert(C_NAME == literal, "ZIG_NAME");
# In zig/abi.zig, every `pub const` that is not a struct/union/enum/opaque
# type must read exactly
#     pub const ZIG_NAME[: T] = literal;
# Each name appears once per side, and both sides name the same set with
# equal literals. Every `case NAME:` in src/zig_abi.h needs a value pin.
num='(0x[0-9A-Fa-f]+|[0-9]+)'
c_pin_re="_Static_assert\([A-Za-z_][A-Za-z_0-9]* == $num, \"[A-Za-z_][A-Za-z_0-9]*\"\);\$"
z_const_re="pub const [A-Za-z_][A-Za-z_0-9]*(: [^=]+)? = $num;\$"
z_type_re='pub const [A-Za-z_][A-Za-z_0-9]* = (extern |packed )?(struct|union|enum|opaque)[ ({]'
c_pins=$(sed -nE "s/^_Static_assert\([A-Za-z_][A-Za-z_0-9]* == $num, \"([A-Za-z_][A-Za-z_0-9]*)\"\);\$/\2 \1/p" "$ABI_H")
z_consts=$(sed -nE "s/^pub const ([A-Za-z_][A-Za-z_0-9]*)(: [^=]+)? = $num;\$/\1 \3/p" "$ABI_ZIG")
[ -n "$c_pins" ] || drift "no value pins found in src/zig_abi.h"
[ -n "$z_consts" ] || drift "no value constants found in zig/abi.zig"
c_bad=$(grep -nE '^[[:space:]]*_Static_assert\(.*, "[A-Za-z_][A-Za-z_0-9]*"\);' "$ABI_H" | grep -vE "^[0-9]+:$c_pin_re" || true)
z_bad=$(grep -nE '^[[:space:]]*pub const ' "$ABI_ZIG" | grep -vE "^[0-9]+:($z_const_re|$z_type_re)" || true)
[ -z "$c_bad" ] || drift "value pins not in the one-line form (src/zig_abi.h line:text):
$c_bad"
[ -z "$z_bad" ] || drift "pub consts that are neither a type nor a pinnable literal (zig/abi.zig line:text):
$z_bad"
for d in $(printf '%s\n' "$c_pins" | cut -d' ' -f1 | sort | uniq -d); do drift "$d is pinned more than once in src/zig_abi.h"; done
for d in $(printf '%s\n' "$z_consts" | cut -d' ' -f1 | sort | uniq -d); do drift "$d is defined more than once in zig/abi.zig"; done
for c in $(sed -nE 's/^[[:space:]]*case ([A-Za-z_][A-Za-z_0-9]*):.*/\1/p' "$ABI_H"); do
  printf '%s\n' "$c_pins" | grep -q "^$c " || drift "case $c in src/zig_abi.h has no value pin"
done
for name in $(printf '%s\n%s\n' "$c_pins" "$z_consts" | cut -d' ' -f1 | sort -u); do
  c_val=$(printf '%s\n' "$c_pins" | awk -v n="$name" '$1 == n { print $2; exit }')
  z_val=$(printf '%s\n' "$z_consts" | awk -v n="$name" '$1 == n { print $2; exit }')
  if [ -z "$c_val" ]; then
    drift "$name in zig/abi.zig has no value pin in src/zig_abi.h"
  elif [ -z "$z_val" ]; then
    drift "$name pinned in src/zig_abi.h is missing from zig/abi.zig"
  elif [ $((c_val)) -ne $((z_val)) ]; then
    drift "$name is $c_val in src/zig_abi.h but $z_val in zig/abi.zig"
  fi
done

# Prototypes. Every yabai_* prototype in src/zig_abi.h needs an
# `export fn` of the same name in zig/*.zig and the reverse, with the same
# parameter count and the same width class for each parameter and the
# return type: ptr, bool, f32, f64, or w8/w16/w32/w64 for integers.
# Signedness and pointee types are not compared.
proto=$(awk -v h="$ABI_H" '
function trim(s) { gsub(/^[ \t]+|[ \t,]+$/, "", s); return s }
function ccls(t, named) {
  t = trim(t)
  if (t ~ /\*/) return "ptr"
  if (named) sub(/[ \t]+[A-Za-z_][A-Za-z_0-9]*$/, "", t)
  gsub(/(^| )(const|volatile|struct|enum) /, " ", t); t = trim(t)
  if (t ~ /Ref$/) return "ptr"
  if (t == "void" || t == "bool") return t
  if (t == "float") return "f32"
  if (t == "double") return "f64"
  if (t ~ /^(u?int8_t|(unsigned |signed )?char)$/) return "w8"
  if (t ~ /^(u?int16_t|(unsigned )?short)$/) return "w16"
  if (t ~ /^(u?int32_t|int|unsigned|unsigned int)$/) return "w32"
  if (t ~ /^(u?int64_t|s?size_t|u?intptr_t|(unsigned )?long( long)?)$/) return "w64"
  return "?" t
}
function zcls(t) {
  t = trim(t)
  if (t ~ /^\??(\*|\[)/) return "ptr"
  if (t == "void" || t == "bool" || t == "f32" || t == "f64") return t
  if (t ~ /^([ui]8|c_char)$/) return "w8"
  if (t ~ /^([ui]16|c_u?short)$/) return "w16"
  if (t ~ /^([ui]32|c_u?int)$/) return "w32"
  if (t ~ /^([ui]64|[ui]size|c_u?long|c_u?longlong)$/) return "w64"
  return "?" t
}
# sig(text, zig): "ret(p1,p2,...)" for one declaration on a single line.
function sig(s, zig,   o, cl, i, n, p, out, ret, params) {
  o = index(s, "(")
  for (cl = length(s); cl > o && substr(s, cl, 1) != ")"; cl--) ;
  params = substr(s, o + 1, cl - o - 1)
  if (zig) { ret = substr(s, cl + 1); sub(/\{.*$/, "", ret); ret = zcls(ret) }
  else { ret = substr(s, 1, o - 1); sub(/yabai_[a-z0-9_]+[ \t]*$/, "", ret); ret = ccls(ret, 0) }
  out = ""; n = split(params, p, ",")
  for (i = 1; i <= n; i++) {
    if (trim(p[i]) == "" || (!zig && trim(p[i]) == "void")) continue
    if (zig) { sub(/^[^:]*:/, "", p[i]); out = out (out == "" ? "" : ",") zcls(p[i]) }
    else out = out (out == "" ? "" : ",") ccls(p[i], 1)
  }
  return ret "(" out ")"
}
FILENAME == h {
  if ($0 ~ /^[A-Za-z][^(]*[ *]yabai_[a-z0-9_]+\(.*\);$/) {
    match($0, /yabai_[a-z0-9_]+\(/); name = substr($0, RSTART, RLENGTH - 1)
    if (name in c) print name ": declared twice in src/zig_abi.h"
    c[name] = sig($0, 0); nc++
  }
  next
}
/^(pub )?export fn yabai_/ { buf = ""; inexp = 1 }
inexp {
  buf = buf " " $0
  if ($0 ~ /\{[ \t]*$/) {
    inexp = 0; match(buf, /yabai_[a-z0-9_]+/); name = substr(buf, RSTART, RLENGTH)
    if (name in z) print name ": exported twice in zig/"
    z[name] = sig(buf, 1); nz++
  }
}
END {
  if (nc == 0) print "no yabai_* prototypes found in src/zig_abi.h"
  if (nz == 0) print "no export fn yabai_* found in zig/"
  for (name in c) {
    if (!(name in z)) print name ": prototype in src/zig_abi.h has no Zig export"
    else if (c[name] != z[name]) print name ": C " c[name] " vs Zig " z[name]
    else if (c[name] ~ /\?/) print name ": unclassified type in " c[name]
  }
  for (name in z) if (!(name in c)) print name ": Zig export has no prototype in src/zig_abi.h"
}' "$ABI_H" "$ZIG_DIR"/*.zig)
if [ -n "$proto" ]; then
  printf '%s\n' "$proto" | sort | while IFS= read -r line; do echo "abi drift: $line"; done >&2
  abi_fail=1
fi
[ "$abi_fail" -eq 0 ] || exit 1

mkdir -p "$BIN_DIR"
for mod in geometry message view util rule window display mouse; do
  zig build-obj "$ZIG_DIR/$mod.zig" -target x86_64-macos.11.0 -O ReleaseSafe --name "$mod" -femit-bin="$BIN_DIR/$mod-x86_64.o"
  zig build-obj "$ZIG_DIR/$mod.zig" -target aarch64-macos.11.0 -O ReleaseSafe --name "$mod" -femit-bin="$BIN_DIR/$mod-arm64.o"
  lipo -create "$BIN_DIR/$mod-x86_64.o" "$BIN_DIR/$mod-arm64.o" -output "$ZIG_DIR/zig-$mod.o"
done
lipo -info "$ZIG_DIR"/zig-*.o
