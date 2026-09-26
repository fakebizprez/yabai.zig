// Mouse-geometry boundary for the yabai migration.
//
// Pinned toolchain: Zig 0.16.0. This object mirrors triangle_contains_point()
// in src/misc/helpers.h, mouse_mod_from_cgflags() and
// mouse_determine_drop_action() in src/mouse_handler.c. Struct inputs arrive
// lifted in (seam pattern): the triangle arrives as a pointer to three
// layout-identical MousePoints (CGPoint is { f64, f64 } on 64-bit macOS),
// the drop-action ms/src/window structs arrive as scalars (drop_mode,
// src_count, frame x/y/w/h, point x/y). No Zig runtime is required at link
// time; float operations stay strict IEEE, matching clang codegen.

pub const MousePoint = extern struct {
    x: f64,
    y: f64,
};

// Bit codes match enum mouse_mod in src/mouse_handler.h.
pub const MOUSE_MOD_NONE: u8 = 0x01;
pub const MOUSE_MOD_ALT: u8 = 0x02;
pub const MOUSE_MOD_SHIFT: u8 = 0x04;
pub const MOUSE_MOD_CMD: u8 = 0x08;
pub const MOUSE_MOD_CTRL: u8 = 0x10;
pub const MOUSE_MOD_FN: u8 = 0x20;

// Action codes match enum mouse_drop_action in src/mouse_handler.h.
pub const MOUSE_DROP_ACTION_NONE: c_int = 0;
pub const MOUSE_DROP_ACTION_STACK: c_int = 1;
pub const MOUSE_DROP_ACTION_SWAP: c_int = 2;
pub const MOUSE_DROP_ACTION_WARP_TOP: c_int = 3;
pub const MOUSE_DROP_ACTION_WARP_RIGHT: c_int = 4;
pub const MOUSE_DROP_ACTION_WARP_BOTTOM: c_int = 5;
pub const MOUSE_DROP_ACTION_WARP_LEFT: c_int = 6;

// Mode codes match enum mouse_mode in src/mouse_handler.h.
pub const MOUSE_MODE_NONE: c_int = 0;
pub const MOUSE_MODE_MOVE: c_int = 1;
pub const MOUSE_MODE_RESIZE: c_int = 2;
pub const MOUSE_MODE_SWAP: c_int = 3;
pub const MOUSE_MODE_STACK: c_int = 4;

// CG flag masks mirror the kCGEventFlagMask* values from the system
// CoreGraphics headers (CGEventTypes.h via NX_*MASK in IOLLEvent.h).
// Pinned here so drift fails the build; the C side pins the same values
// with _Static_assert in tests/src/zig_mouse.c.
pub const CG_FLAG_MASK_SHIFT: u32 = 0x00020000;
pub const CG_FLAG_MASK_CONTROL: u32 = 0x00040000;
pub const CG_FLAG_MASK_ALTERNATE: u32 = 0x00080000;
pub const CG_FLAG_MASK_COMMAND: u32 = 0x00100000;
pub const CG_FLAG_MASK_SECONDARY_FN: u32 = 0x00800000;

comptime {
    if (MOUSE_MOD_NONE != 0x01) @compileError("MOUSE_MOD_NONE drift");
    if (MOUSE_MOD_ALT != 0x02) @compileError("MOUSE_MOD_ALT drift");
    if (MOUSE_MOD_SHIFT != 0x04) @compileError("MOUSE_MOD_SHIFT drift");
    if (MOUSE_MOD_CMD != 0x08) @compileError("MOUSE_MOD_CMD drift");
    if (MOUSE_MOD_CTRL != 0x10) @compileError("MOUSE_MOD_CTRL drift");
    if (MOUSE_MOD_FN != 0x20) @compileError("MOUSE_MOD_FN drift");
    if (MOUSE_DROP_ACTION_NONE != 0) @compileError("MOUSE_DROP_ACTION_NONE drift");
    if (MOUSE_DROP_ACTION_STACK != 1) @compileError("MOUSE_DROP_ACTION_STACK drift");
    if (MOUSE_DROP_ACTION_SWAP != 2) @compileError("MOUSE_DROP_ACTION_SWAP drift");
    if (MOUSE_DROP_ACTION_WARP_TOP != 3) @compileError("MOUSE_DROP_ACTION_WARP_TOP drift");
    if (MOUSE_DROP_ACTION_WARP_RIGHT != 4) @compileError("MOUSE_DROP_ACTION_WARP_RIGHT drift");
    if (MOUSE_DROP_ACTION_WARP_BOTTOM != 5) @compileError("MOUSE_DROP_ACTION_WARP_BOTTOM drift");
    if (MOUSE_DROP_ACTION_WARP_LEFT != 6) @compileError("MOUSE_DROP_ACTION_WARP_LEFT drift");
    if (MOUSE_MODE_NONE != 0) @compileError("MOUSE_MODE_NONE drift");
    if (MOUSE_MODE_MOVE != 1) @compileError("MOUSE_MODE_MOVE drift");
    if (MOUSE_MODE_RESIZE != 2) @compileError("MOUSE_MODE_RESIZE drift");
    if (MOUSE_MODE_SWAP != 3) @compileError("MOUSE_MODE_SWAP drift");
    if (MOUSE_MODE_STACK != 4) @compileError("MOUSE_MODE_STACK drift");
    if (CG_FLAG_MASK_SHIFT != 0x00020000) @compileError("CG_FLAG_MASK_SHIFT drift");
    if (CG_FLAG_MASK_CONTROL != 0x00040000) @compileError("CG_FLAG_MASK_CONTROL drift");
    if (CG_FLAG_MASK_ALTERNATE != 0x00080000) @compileError("CG_FLAG_MASK_ALTERNATE drift");
    if (CG_FLAG_MASK_COMMAND != 0x00100000) @compileError("CG_FLAG_MASK_COMMAND drift");
    if (CG_FLAG_MASK_SECONDARY_FN != 0x00800000) @compileError("CG_FLAG_MASK_SECONDARY_FN drift");
}

// Mirrors triangle_contains_point() in src/misc/helpers.h. The C inputs are
// CGFloat (f64 on 64-bit macOS); the RHS evaluates in f64 and narrows to
// float temporaries, so the port computes in f64 and rounds once to f32 via
// @floatCast (same round-to-nearest as the C assignment) before the strict
// sign tests. Edge and vertex points (any l == 0) return false, like C.
fn mouseTriangleContainsPoint(t: [*]const MousePoint, p: *const MousePoint) bool {
    @setRuntimeSafety(false);
    const l1: f32 = @floatCast((p.x - t[0].x) * (t[2].y - t[0].y) - (t[2].x - t[0].x) * (p.y - t[0].y));
    const l2: f32 = @floatCast((p.x - t[1].x) * (t[0].y - t[1].y) - (t[0].x - t[1].x) * (p.y - t[1].y));
    const l3: f32 = @floatCast((p.x - t[2].x) * (t[1].y - t[2].y) - (t[1].x - t[2].x) * (p.y - t[2].y));
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
    if ((cgflags & CG_FLAG_MASK_ALTERNATE) == CG_FLAG_MASK_ALTERNATE) flags |= MOUSE_MOD_ALT;
    if ((cgflags & CG_FLAG_MASK_SHIFT) == CG_FLAG_MASK_SHIFT) flags |= MOUSE_MOD_SHIFT;
    if ((cgflags & CG_FLAG_MASK_COMMAND) == CG_FLAG_MASK_COMMAND) flags |= MOUSE_MOD_CMD;
    if ((cgflags & CG_FLAG_MASK_CONTROL) == CG_FLAG_MASK_CONTROL) flags |= MOUSE_MOD_CTRL;
    if ((cgflags & CG_FLAG_MASK_SECONDARY_FN) == CG_FLAG_MASK_SECONDARY_FN) flags |= MOUSE_MOD_FN;
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
        return if (drop_mode == MOUSE_MODE_STACK) MOUSE_DROP_ACTION_STACK else MOUSE_DROP_ACTION_SWAP;
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

    if (mouseTriangleContainsPoint(&top, &wp)) return MOUSE_DROP_ACTION_WARP_TOP;
    if (mouseTriangleContainsPoint(&right, &wp)) return MOUSE_DROP_ACTION_WARP_RIGHT;
    if (mouseTriangleContainsPoint(&bottom, &wp)) return MOUSE_DROP_ACTION_WARP_BOTTOM;
    if (mouseTriangleContainsPoint(&left, &wp)) return MOUSE_DROP_ACTION_WARP_LEFT;

    return MOUSE_DROP_ACTION_NONE;
}
