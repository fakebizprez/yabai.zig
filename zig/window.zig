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
}

pub const HANDLE_TOP: c_int = 0x01;
pub const HANDLE_BOTTOM: c_int = 0x02;
pub const HANDLE_LEFT: c_int = 0x04;
pub const HANDLE_RIGHT: c_int = 0x08;

pub const TYPE_REL: c_int = 0x2;

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
