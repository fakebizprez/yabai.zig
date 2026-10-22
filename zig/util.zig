// Pure-helper boundary for the yabai migration (step 3).
//
// Pinned toolchain: Zig 0.17.0. Mirrors the helpers in
// src/misc/helpers.h, src/misc/macros.h and src/view.c.

const abi = @import("abi.zig");
const BalanceNode = abi.BalanceNode;
const RgbaColor = abi.RgbaColor;

comptime {
    abi.matches(yabai_balance_node_add, abi.yabai_balance_node_add);
    abi.matches(yabai_find_rank_in_list, abi.yabai_find_rank_in_list);
    abi.matches(yabai_json_bool, abi.yabai_json_bool);
    abi.matches(yabai_json_optional_bool, abi.yabai_json_optional_bool);
    abi.matches(yabai_in_range_ii_f, abi.yabai_in_range_ii_f);
}

export fn yabai_balance_node_add(a: *const BalanceNode, b: *const BalanceNode, out: *BalanceNode) void {
    @setRuntimeSafety(false);
    out.y_count = a.y_count + b.y_count;
    out.x_count = a.x_count + b.x_count;
}

export fn yabai_clampf_range(value: f32, min: f32, max: f32) f32 {
    @setRuntimeSafety(false);
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

export fn yabai_json_bool(value: bool) [*:0]const u8 {
    @setRuntimeSafety(false);
    return if (value) "true" else "false";
}

export fn yabai_json_optional_bool(value: c_int) [*:0]const u8 {
    @setRuntimeSafety(false);
    if (value == 0) return "null";
    if (value == 1) return "true";
    return "false";
}

export fn yabai_in_range_ii_f(a: f32, b: f32, c: f32) bool {
    @setRuntimeSafety(false);
    return (a >= b) and (a <= c);
}

export fn yabai_in_range_ii_i(a: c_int, b: c_int, c: c_int) bool {
    @setRuntimeSafety(false);
    return (a >= b) and (a <= c);
}

export fn yabai_in_range_ei_f(a: f32, b: f32, c: f32) bool {
    @setRuntimeSafety(false);
    return (a > b) and (a <= c);
}

export fn yabai_in_range_ie_i(a: c_int, b: c_int, c: c_int) bool {
    @setRuntimeSafety(false);
    return (a >= b) and (a < c);
}

export fn yabai_find_rank_in_list(wid: u32, window_list: [*]const u32, window_count: c_int) c_int {
    @setRuntimeSafety(false);
    var rank: c_int = 0;
    var i: c_int = 0;
    while (i < window_count) {
        if (window_list[@intCast(i)] == wid) {
            return rank;
        } else {
            rank += 1;
        }
        i += 1;
    }
    return std.math.maxInt(c_int);
}

const std = @import("std");

extern "c" fn sinf(f32) f32;
extern "c" fn cosf(f32) f32;
extern "c" fn powf(f32, f32) f32;
extern "c" fn sqrtf(f32) f32;
const builtin = @import("builtin");

// a*b + c as clang emits it: with its default -ffp-contract=on (even at
// -O0) it fuses into one fma on arm64, while the x86_64 default CPU has no
// FMA, so there it stays unfused (@mulAdd would call fmaf and differ).
inline fn mulAdd(a: f32, b: f32, c: f32) f32 {
    if (comptime builtin.cpu.arch == .aarch64) return @mulAdd(f32, a, b, c);
    return a * b + c;
}

// A visible powf call gets rewritten by LLVM: powf(2, x) into exp2f(x) and
// powf(x, 2) into x * x. The C build at -O0 calls powf at every site, and
// powf(x, 2) is not always x * x (the in_out_quad and circ easings drifted),
// so every call goes through a volatile pointer load the optimizer cannot
// resolve (a const pointer gets folded back into a direct powf call).
var powf_ptr: *const fn (f32, f32) callconv(.c) f32 = &powf;
fn pow(x: f32, y: f32) f32 {
    const p: *const volatile @TypeOf(powf_ptr) = &powf_ptr;
    return p.*(x, y);
}

export fn yabai_ease_in_sine(t: f32) f32 {
    @setRuntimeSafety(false);
    return 1.0 - cosf(@floatCast(@as(f64, t) * std.math.pi / 2.0));
}

export fn yabai_ease_out_sine(t: f32) f32 {
    @setRuntimeSafety(false);
    return sinf(@floatCast(@as(f64, t) * std.math.pi / 2.0));
}

export fn yabai_ease_in_out_sine(t: f32) f32 {
    @setRuntimeSafety(false);
    return -(cosf(@floatCast(std.math.pi * @as(f64, t))) - 1.0) / 2.0;
}

export fn yabai_ease_in_quad(t: f32) f32 {
    @setRuntimeSafety(false);
    return t * t;
}

export fn yabai_ease_out_quad(t: f32) f32 {
    @setRuntimeSafety(false);
    const u: f32 = 1.0 - t;
    return mulAdd(-u, u, 1.0);
}

export fn yabai_ease_in_out_quad(t: f32) f32 {
    @setRuntimeSafety(false);
    return if (t < 0.5) 2.0 * t * t else 1.0 - pow(mulAdd(-2.0, t, 2.0), 2.0) / 2.0;
}

export fn yabai_ease_in_cubic(t: f32) f32 {
    @setRuntimeSafety(false);
    return t * t * t;
}

export fn yabai_ease_out_cubic(t: f32) f32 {
    @setRuntimeSafety(false);
    return 1.0 - pow(1.0 - t, 3.0);
}

export fn yabai_ease_in_out_cubic(t: f32) f32 {
    @setRuntimeSafety(false);
    return if (t < 0.5) 4.0 * t * t * t else 1.0 - pow(mulAdd(-2.0, t, 2.0), 3.0) / 2.0;
}

export fn yabai_ease_in_quart(t: f32) f32 {
    @setRuntimeSafety(false);
    return t * t * t * t;
}

export fn yabai_ease_out_quart(t: f32) f32 {
    @setRuntimeSafety(false);
    return 1.0 - pow(1.0 - t, 4.0);
}

export fn yabai_ease_in_out_quart(t: f32) f32 {
    @setRuntimeSafety(false);
    return if (t < 0.5) 8.0 * t * t * t * t else 1.0 - pow(mulAdd(-2.0, t, 2.0), 4.0) / 2.0;
}

export fn yabai_ease_in_quint(t: f32) f32 {
    @setRuntimeSafety(false);
    return t * t * t * t * t;
}

export fn yabai_ease_out_quint(t: f32) f32 {
    @setRuntimeSafety(false);
    return 1.0 - pow(1.0 - t, 5.0);
}

export fn yabai_ease_in_out_quint(t: f32) f32 {
    @setRuntimeSafety(false);
    return if (t < 0.5) 16.0 * t * t * t * t * t else 1.0 - pow(mulAdd(-2.0, t, 2.0), 5.0) / 2.0;
}

export fn yabai_ease_in_expo(t: f32) f32 {
    @setRuntimeSafety(false);
    return if (t == 0.0) 0.0 else pow(2.0, mulAdd(10.0, t, -10.0));
}

export fn yabai_ease_out_expo(t: f32) f32 {
    @setRuntimeSafety(false);
    return if (t == 1.0) 1.0 else 1.0 - pow(2.0, -10.0 * t);
}

export fn yabai_ease_in_out_expo(t: f32) f32 {
    @setRuntimeSafety(false);
    return if (t == 0.0) 0.0 else if (t == 1.0) 1.0 else if (t < 0.5) pow(2.0, mulAdd(20.0, t, -10.0)) / 2.0 else (2.0 - pow(2.0, mulAdd(-20.0, t, 10.0))) / 2.0;
}

export fn yabai_ease_in_circ(t: f32) f32 {
    @setRuntimeSafety(false);
    return 1.0 - sqrtf(1.0 - pow(t, 2.0));
}

export fn yabai_ease_out_circ(t: f32) f32 {
    @setRuntimeSafety(false);
    return sqrtf(1.0 - pow(t - 1.0, 2.0));
}

export fn yabai_ease_in_out_circ(t: f32) f32 {
    @setRuntimeSafety(false);
    return if (t < 0.5) (1.0 - sqrtf(1.0 - pow(2.0 * t, 2.0))) / 2.0 else (sqrtf(1.0 - pow(mulAdd(-2.0, t, 2.0), 2.0)) + 1.0) / 2.0;
}

export fn yabai_rgba_color_from_hex(color: u32, out: *RgbaColor) void {
    @setRuntimeSafety(false);
    out.p = color;
    out.r = @as(f32, @floatFromInt((color >> 0x10) & 0xff)) / 255.0;
    out.g = @as(f32, @floatFromInt((color >> 0x08) & 0xff)) / 255.0;
    out.b = @as(f32, @floatFromInt((color >> 0x00) & 0xff)) / 255.0;
    out.a = @as(f32, @floatFromInt((color >> 0x18) & 0xff)) / 255.0;
}
