// Pure-helper boundary for the yabai migration (step 3).
//
// Pinned toolchain: Zig 0.16.0. Mirrors the helpers in
// src/misc/helpers.h, src/misc/macros.h and src/view.c.

pub const BalanceNode = extern struct {
    y_count: c_int,
    x_count: c_int,
};

pub const RgbaColor = extern struct {
    p: u32,
    r: f32,
    g: f32,
    b: f32,
    a: f32,
};

comptime {
    if (@sizeOf(BalanceNode) != 8) @compileError("BalanceNode size drift");
    if (@sizeOf(RgbaColor) != 20) @compileError("RgbaColor size drift");
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

export fn yabai_rgba_color_from_hex(color: u32, out: *RgbaColor) void {
    @setRuntimeSafety(false);
    out.p = color;
    out.r = @as(f32, @floatFromInt((color >> 0x10) & 0xff)) / 255.0;
    out.g = @as(f32, @floatFromInt((color >> 0x08) & 0xff)) / 255.0;
    out.b = @as(f32, @floatFromInt((color >> 0x00) & 0xff)) / 255.0;
    out.a = @as(f32, @floatFromInt((color >> 0x18) & 0xff)) / 255.0;
}
