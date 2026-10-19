// First Zig/C boundary for the yabai migration (doc/zig-refactor-plan.md step 2).
//
// Pinned toolchain: Zig 0.17.0. This object mirrors area_make_pair() in
// src/view.c exactly: same inputs, same outputs, same C float-to-int
// rounding (truncation toward zero via (int) casts, round-half-up via
// +0.5f before the cast on the offset). The split codes match
// enum window_node_split in src/view.h (SPLIT_Y = 1, SPLIT_X = 2).

const std = @import("std");

pub const Area = extern struct {
    x: f32,
    y: f32,
    w: f32,
    h: f32,
};

pub const SPLIT_Y: c_int = 1;

export fn yabai_area_make_pair(
    split: c_int,
    gap: c_int,
    ratio: f32,
    parent: *const Area,
    left: *Area,
    right: *Area,
) void {
    // No Zig runtime required: disables safety checks so the object links
    // into the existing C build without panic/compiler-rt dependencies.
    // Float operations stay strict IEEE, matching clang -O0 codegen.
    @setRuntimeSafety(false);

    left.* = parent.*;
    right.* = parent.*;

    const gap_f: f32 = @floatFromInt(gap);

    if (split == SPLIT_Y) {
        const left_width: f32 = (parent.w - gap_f) * ratio;
        const right_width: f32 = (parent.w - gap_f) * (1.0 - ratio);

        left.w = @floatFromInt(@as(i32, @intFromFloat(left_width)));
        right.w = @floatFromInt(@as(i32, @intFromFloat(right_width)));
        right.x += @floatFromInt(@as(i32, @intFromFloat(left_width + 0.5)) + gap);
    } else {
        const left_height: f32 = (parent.h - gap_f) * ratio;
        const right_height: f32 = (parent.h - gap_f) * (1.0 - ratio);

        left.h = @floatFromInt(@as(i32, @intFromFloat(left_height)));
        right.h = @floatFromInt(@as(i32, @intFromFloat(right_height)));
        right.y += @floatFromInt(@as(i32, @intFromFloat(left_height + 0.5)) + gap);
    }
}

pub const Point = extern struct {
    x: f64,
    y: f64,
};

// Direction codes match src/misc/macros.h.
pub const DIR_NORTH: c_int = 360;
pub const DIR_EAST: c_int = 90;
pub const DIR_SOUTH: c_int = 180;
pub const DIR_WEST: c_int = 270;

// Point is layout-identical to CGPoint on 64-bit macOS (CGFloat is double).
// Area fields widen to f64 for comparison exactly as C promotes float to
// double; widening is exact so ordering is preserved.
export fn yabai_area_is_in_direction(
    r1: *const Area,
    r1_max: *const Point,
    r2: *const Area,
    r2_max: *const Point,
    direction: c_int,
) bool {
    @setRuntimeSafety(false);

    if (direction == DIR_NORTH and r1_max.y <= @as(f64, r2.y)) return false;
    if (direction == DIR_EAST and r2_max.x <= @as(f64, r1.x)) return false;
    if (direction == DIR_SOUTH and r2_max.y <= @as(f64, r1.y)) return false;
    if (direction == DIR_WEST and r1_max.x <= @as(f64, r2.x)) return false;

    if (direction == DIR_NORTH or direction == DIR_SOUTH) {
        const r1x: f64 = r1.x;
        const r1_max_x: f64 = r1_max.x;
        const r2x: f64 = r2.x;
        const r2_max_x: f64 = r2_max.x;
        return (r2_max_x > r1x and r2_max_x <= r1_max_x) or
            (r2x < r1x and r2_max_x > r1_max_x) or
            (r2x >= r1x and r2x < r1_max_x);
    }

    if (direction == DIR_EAST or direction == DIR_WEST) {
        const r1y: f64 = r1.y;
        const r1_max_y: f64 = r1_max.y;
        const r2y: f64 = r2.y;
        const r2_max_y: f64 = r2_max.y;
        return (r2_max_y > r1y and r2_max_y <= r1_max_y) or
            (r2y < r1y and r2_max_y > r1_max_y) or
            (r2y >= r1y and r2y < r1_max_y);
    }

    return false;
}

// Mirrors area_distance_in_direction(): double subtraction, then C-style
// truncation toward zero on conversion to int.
export fn yabai_area_distance_in_direction(
    r1: *const Area,
    r1_max: *const Point,
    r2: *const Area,
    r2_max: *const Point,
    direction: c_int,
) c_int {
    @setRuntimeSafety(false);

    const r1x: f64 = r1.x;
    const r1y: f64 = r1.y;
    const r1_max_x: f64 = r1_max.x;
    const r1_max_y: f64 = r1_max.y;
    const r2x: f64 = r2.x;
    const r2y: f64 = r2.y;
    const r2_max_x: f64 = r2_max.x;
    const r2_max_y: f64 = r2_max.y;

    if (direction == DIR_NORTH) {
        if (r2_max_y > r1y) return @intFromFloat(r2_max_y - r1y);
        return @intFromFloat(r1y - r2_max_y);
    }
    if (direction == DIR_EAST) {
        if (r2x < r1_max_x) return @intFromFloat(r1_max_x - r2x);
        return @intFromFloat(r2x - r1_max_x);
    }
    if (direction == DIR_SOUTH) {
        if (r2y < r1_max_y) return @intFromFloat(r1_max_y - r2y);
        return @intFromFloat(r2y - r1_max_y);
    }
    if (direction == DIR_WEST) {
        if (r2_max_x > r1x) return @intFromFloat(r2_max_x - r1x);
        return @intFromFloat(r1x - r2_max_x);
    }

    return std.math.maxInt(c_int);
}
