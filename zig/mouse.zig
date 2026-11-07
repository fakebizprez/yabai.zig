// Mouse-geometry boundary (Zig version: zig/ZIG_VERSION).
//
// This object mirrors triangle_contains_point()
// in src/misc/helpers.h, mouse_mod_from_cgflags() and
// mouse_determine_drop_action() in src/mouse_handler.c. Struct inputs arrive
// lifted in (seam pattern): the triangle arrives as a pointer to three
// layout-identical MousePoints (CGPoint is { f64, f64 } on 64-bit macOS),
// the drop-action ms/src/window structs arrive as scalars (drop_mode,
// src_count, frame x/y/w/h, point x/y). No Zig runtime is required at link
// time. Zig never contracts float ops on its own; the one site clang
// contracts is reproduced explicitly (see crossDiff).

const builtin = @import("builtin");
const abi = @import("abi.zig");
const MousePoint = abi.CgPoint;

// a*b - c*d as clang emits it: with its default -ffp-contract=on (even at
// -O0) it fuses the left product into fma(a, b, -(c*d)) on arm64 (fnmsub),
// while the x86_64 default CPU has no FMA, so there it stays unfused.
inline fn crossDiff(a: f64, b: f64, c: f64, d: f64) f64 {
    if (comptime builtin.cpu.arch == .aarch64) return @mulAdd(f64, a, b, -(c * d));
    return a * b - c * d;
}

// Mirrors triangle_contains_point() in src/misc/helpers.h. The C inputs are
// CGFloat (f64 on 64-bit macOS); the RHS evaluates in f64 (fused per
// crossDiff) and narrows to float temporaries, so the port rounds once to
// f32 via @floatCast (same round-to-nearest as the C assignment) before the
// strict sign tests. Edge and vertex points (any l == 0) return false, like C.
fn mouseTriangleContainsPoint(t: [*]const MousePoint, p: *const MousePoint) bool {
    @setRuntimeSafety(false);
    const l1: f32 = @floatCast(crossDiff(p.x - t[0].x, t[2].y - t[0].y, t[2].x - t[0].x, p.y - t[0].y));
    const l2: f32 = @floatCast(crossDiff(p.x - t[1].x, t[0].y - t[1].y, t[0].x - t[1].x, p.y - t[1].y));
    const l3: f32 = @floatCast(crossDiff(p.x - t[2].x, t[1].y - t[2].y, t[1].x - t[2].x, p.y - t[2].y));
    return (l1 > 0.0 and l2 > 0.0 and l3 > 0.0) or (l1 < 0.0 and l2 < 0.0 and l3 < 0.0);
}

export fn yabai_triangle_contains_point(t: [*]const MousePoint, p: *const MousePoint) bool {
    @setRuntimeSafety(false);
    return mouseTriangleContainsPoint(t, p);
}

// Mirrors mouse_mod_from_cgflags() in src/mouse_handler.c. Each CG flag maps
// to its MOUSE_MOD_* bit; unmapped flags are ignored.
export fn yabai_mouse_mod_from_cgflags(cgflags: u32) u8 {
    @setRuntimeSafety(false);
    var flags: u8 = 0;
    if ((cgflags & abi.CG_FLAG_MASK_ALTERNATE) == abi.CG_FLAG_MASK_ALTERNATE) flags |= abi.MOUSE_MOD_ALT;
    if ((cgflags & abi.CG_FLAG_MASK_SHIFT) == abi.CG_FLAG_MASK_SHIFT) flags |= abi.MOUSE_MOD_SHIFT;
    if ((cgflags & abi.CG_FLAG_MASK_COMMAND) == abi.CG_FLAG_MASK_COMMAND) flags |= abi.MOUSE_MOD_CMD;
    if ((cgflags & abi.CG_FLAG_MASK_CONTROL) == abi.CG_FLAG_MASK_CONTROL) flags |= abi.MOUSE_MOD_CTRL;
    if ((cgflags & abi.CG_FLAG_MASK_SECONDARY_FN) == abi.CG_FLAG_MASK_SECONDARY_FN) flags |= abi.MOUSE_MOD_FN;
    return flags;
}

// Mirrors mouse_determine_drop_action() in src/mouse_handler.c. The frame
// rect arrives as four f64s (replacing dst_window->frame), the drop point as
// two f64s, ms->drop_action as drop_mode, src_node->window_count as
// src_count. The center test mirrors CGRectContainsPoint(c, wp) with
// c = {{ 0.25*w, 0.25*h }, { 0.5*w, 0.5*h }}: min edges inclusive and max
// edges exclusive (verified against the system CGRectContainsPoint: min
// corner true, max corner false, empty rect false), and an empty rect (w or
// h <= 0) contains nothing. The max edge recomputes as min + size
// (cx0 + cw), exactly like CGRectGetMaxX, so edge points agree bit-for-bit.
// The warp triangles reuse the ported
// triangle test with vertices built in C order (0.5f products are exact in
// f64, matching the CGFloat arithmetic).
export fn yabai_mouse_determine_drop_action(drop_mode: c_int, src_count: c_int, fx: f64, fy: f64, fw: f64, fh: f64, px: f64, py: f64) c_int {
    @setRuntimeSafety(false);
    const wpx = px - fx;
    const wpy = py - fy;

    const cx0 = 0.25 * fw;
    const cy0 = 0.25 * fh;
    const cw = 0.5 * fw;
    const ch = 0.5 * fh;
    const in_center = cw > 0.0 and ch > 0.0 and
        wpx >= cx0 and wpx < cx0 + cw and
        wpy >= cy0 and wpy < cy0 + ch;
    if (in_center and src_count == 1) {
        return if (drop_mode == abi.MOUSE_MODE_STACK) abi.MOUSE_DROP_ACTION_STACK else abi.MOUSE_DROP_ACTION_SWAP;
    }

    const hx = 0.5 * fw;
    const hy = 0.5 * fh;
    var top = [_]MousePoint{
        .{ .x = 0.0, .y = 0.0 },
        .{ .x = hx, .y = hy },
        .{ .x = fw, .y = 0.0 },
    };
    var right = [_]MousePoint{
        .{ .x = fw, .y = 0.0 },
        .{ .x = hx, .y = hy },
        .{ .x = fw, .y = fh },
    };
    var bottom = [_]MousePoint{
        .{ .x = fw, .y = fh },
        .{ .x = hx, .y = hy },
        .{ .x = 0.0, .y = fh },
    };
    var left = [_]MousePoint{
        .{ .x = 0.0, .y = fh },
        .{ .x = hx, .y = hy },
        .{ .x = 0.0, .y = 0.0 },
    };
    var wp = MousePoint{ .x = wpx, .y = wpy };

    if (mouseTriangleContainsPoint(&top, &wp)) return abi.MOUSE_DROP_ACTION_WARP_TOP;
    if (mouseTriangleContainsPoint(&right, &wp)) return abi.MOUSE_DROP_ACTION_WARP_RIGHT;
    if (mouseTriangleContainsPoint(&bottom, &wp)) return abi.MOUSE_DROP_ACTION_WARP_BOTTOM;
    if (mouseTriangleContainsPoint(&left, &wp)) return abi.MOUSE_DROP_ACTION_WARP_LEFT;

    return abi.MOUSE_DROP_ACTION_NONE;
}
