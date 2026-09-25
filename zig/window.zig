// Window-manager math boundary for the yabai migration (step 3).
//
// Pinned toolchain: Zig 0.16.0. Mirrors resize_frame_math() in
// src/window_manager.c. CgRect mirrors CGRect on 64-bit macOS
// (four doubles, 32 bytes). All arithmetic stays in f64 exactly as
// the C promotion rules dictate, narrowing to f32 only on output.

pub const CgRect = extern struct {
    x: f64,
    y: f64,
    w: f64,
    h: f64,
};

comptime {
    if (@sizeOf(CgRect) != 32) @compileError("CgRect size drift");
    if (WINDOW_MOVABLE != 0x40) @compileError("WINDOW_MOVABLE drift");
    if (WINDOW_RESIZABLE != 0x80) @compileError("WINDOW_RESIZABLE drift");
}

pub const HANDLE_TOP: c_int = 0x01;
pub const HANDLE_BOTTOM: c_int = 0x02;
pub const HANDLE_LEFT: c_int = 0x04;
pub const HANDLE_RIGHT: c_int = 0x08;

pub const TYPE_REL: c_int = 0x2;

// Flag byte mirror of `enum window_flag` in src/window.h. Only the two
// capability bits are read by the ported helpers below; the rest are
// pinned here so layout drift fails the build, not the suite.
pub const WINDOW_SHADOW: u8 = 0x01;
pub const WINDOW_FULLSCREEN: u8 = 0x02;
pub const WINDOW_MINIMIZE: u8 = 0x04;
pub const WINDOW_FLOAT: u8 = 0x08;
pub const WINDOW_STICKY: u8 = 0x10;
pub const WINDOW_WINDOWED: u8 = 0x20;
pub const WINDOW_MOVABLE: u8 = 0x40;
pub const WINDOW_RESIZABLE: u8 = 0x80;

export fn yabai_move_frame_math(
    frame: *const CgRect,
    move_type: c_int,
    dx: f32,
    dy: f32,
    x: *f32,
    y: *f32,
    w: *f32,
    h: *f32,
) void {
    @setRuntimeSafety(false);
    // The C code widens dx/dy to f64 for the origin addition, then
    // narrows back to float on assignment.
    var ax: f32 = dx;
    var ay: f32 = dy;
    if (move_type == TYPE_REL) {
        ax = @floatCast(@as(f64, dx) + frame.x);
        ay = @floatCast(@as(f64, dy) + frame.y);
    }
    x.* = ax;
    y.* = ay;
    w.* = @floatCast(frame.w);
    h.* = @floatCast(frame.h);
}

export fn yabai_resize_frame_math(
    frame: *const CgRect,
    direction: c_int,
    dx: f32,
    dy: f32,
    fx: *f32,
    fy: *f32,
    fw: *f32,
    fh: *f32,
) void {
    @setRuntimeSafety(false);
    const x_mod: c_int = if (direction & HANDLE_LEFT != 0)
        -1
    else if (direction & HANDLE_RIGHT != 0)
        1
    else
        0;
    const y_mod: c_int = if (direction & HANDLE_TOP != 0)
        -1
    else if (direction & HANDLE_BOTTOM != 0)
        1
    else
        0;

    // The C code promotes the f32 products to f64 before adding the
    // frame dimensions; max() then runs in f64 with narrowing only
    // on assignment to the float outputs.
    const fw_full: f64 = frame.w + @as(f64, dx * @as(f32, @floatFromInt(x_mod)));
    const fh_full: f64 = frame.h + @as(f64, dy * @as(f32, @floatFromInt(y_mod)));
    const fw_narrow: f32 = @floatCast(if (1.0 > fw_full) 1.0 else fw_full);
    const fh_narrow: f32 = @floatCast(if (1.0 > fh_full) 1.0 else fh_full);
    fw.* = fw_narrow;
    fh.* = fh_narrow;
    fx.* = @floatCast(if (direction & HANDLE_LEFT != 0)
        frame.x + frame.w - @as(f64, fw_narrow)
    else
        frame.x);
    fy.* = @floatCast(if (direction & HANDLE_TOP != 0)
        frame.y + frame.h - @as(f64, fh_narrow)
    else
        frame.y);
}

// Mirrors window_is_undersized() in src/window.c. The C code compares
// the CGRect doubles (CGFloat) against 500.0f, which promotes to
// exactly 500.0 in f64, so the bound is spelled as 500.0 here.
export fn yabai_window_is_undersized(w: f64, h: f64) bool {
    @setRuntimeSafety(false);
    if (w <= 500.0) return true;
    if (h <= 500.0) return true;
    return false;
}

// Mirrors window_can_move() in src/window.c (flags & WINDOW_MOVABLE).
export fn yabai_window_can_move(flags: u8) bool {
    @setRuntimeSafety(false);
    return (flags & WINDOW_MOVABLE) != 0;
}

// Mirrors window_can_resize() in src/window.c (flags & WINDOW_RESIZABLE).
export fn yabai_window_can_resize(flags: u8) bool {
    @setRuntimeSafety(false);
    return (flags & WINDOW_RESIZABLE) != 0;
}

// Layer-index mirror of src/misc/macros.h (CGWindowLevelKey values).
// Pinned here so table drift fails the build, not the suite; the C side
// pins the same values with _Static_assert in tests/src/zig_window.c.
pub const LAYER_AUTO: c_int = 0;
pub const LAYER_BELOW: c_int = 3;
pub const LAYER_NORMAL: c_int = 4;
pub const LAYER_ABOVE: c_int = 5;

comptime {
    if (LAYER_AUTO != 0) @compileError("LAYER_AUTO drift");
    if (LAYER_BELOW != 3) @compileError("LAYER_BELOW drift");
    if (LAYER_NORMAL != 4) @compileError("LAYER_NORMAL drift");
    if (LAYER_ABOVE != 5) @compileError("LAYER_ABOVE drift");
}

// Mirrors window_layer() in src/window.c. The three
// g_layer_*_window_level globals (runtime CGWindowLevel values from
// CGWindowLevelForKey) are lifted in as params (established seam
// pattern). Returns the layer_str entries ("below"/"normal"/"above")
// or "unknown", as borrowed string pointers with no allocation.
export fn yabai_window_layer(level: c_int, below: c_int, normal: c_int, above: c_int) [*:0]const u8 {
    @setRuntimeSafety(false);
    if (level == below) return "below";
    if (level == normal) return "normal";
    if (level == above) return "above";
    return "unknown";
}
