// Display serialization boundary for the yabai migration (step 3).
//
// Pinned toolchain: Zig 0.17.0. Mirrors display_serialize() in
// src/display.c. All manager-derived inputs arrive lifted in (seam
// pattern): uuid_str is the ts_cfstring_copy result (NULL when the copy
// would yield NULL), arrange_index replaces
// display_manager_display_id_arrangement, label_str replaces
// display_manager_get_label_for_display (NULL when absent), the frame
// rect arrives as four f64s (replacing CGDisplayBounds), the space
// mission-index int array + count replace display_space_list plus
// space_manager_mission_control_index, and has_focus replaces the
// did == current_display_id derivation. Bools reuse yabai_json_bool,
// and every byte goes out via libc fwrite/snprintf. The trailing
// has-focus branch omits did_output=true, exactly like C.

const abi = @import("abi.zig");

extern "c" fn snprintf(buf: [*]u8, n: usize, fmt: [*:0]const u8, ...) c_int;
extern "c" fn fwrite(buf: *const anyopaque, size: usize, nmemb: usize, stream: ?*anyopaque) usize;

fn displayStrLenZ(s: [*:0]const u8) usize {
    @setRuntimeSafety(false);
    var len: usize = 0;
    while (s[len] != 0) : (len += 1) {}
    return len;
}

fn displayWriteBytes(rsp: ?*anyopaque, s: []const u8) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    if (s.len == 0) return;
    _ = fwrite(@ptrCast(s.ptr), 1, s.len, stream);
}

fn displayWriteZ(rsp: ?*anyopaque, s: [*:0]const u8) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    const len = displayStrLenZ(s);
    if (len == 0) return;
    _ = fwrite(@ptrCast(s), 1, len, stream);
}

fn displayWriteInt(rsp: ?*anyopaque, v: c_int) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    var tmp: [32]u8 = undefined;
    const n = snprintf(@ptrCast(&tmp), tmp.len, "%d", v);
    if (n <= 0) return;
    _ = fwrite(@ptrCast(&tmp), 1, @intCast(n), stream);
}

fn displayWriteFloat(rsp: ?*anyopaque, v: f64) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    var tmp: [64]u8 = undefined;
    const n = snprintf(@ptrCast(&tmp), tmp.len, "%.4f", v);
    if (n <= 0) return;
    _ = fwrite(@ptrCast(&tmp), 1, @intCast(n), stream);
}

// Mirrors display_serialize() in src/display.c. The did emits via %d
// with its bits preserved (c_int bit-cast, exactly like C passing the
// uint32_t to %d); the frame doubles emit via %.4f through libc
// snprintf so rounding matches by construction.
export fn yabai_display_serialize(rsp: ?*anyopaque, did: u32, flags: u64, uuid_str: ?[*:0]const u8, arrange_index: c_int, label_str: ?[*:0]const u8, frame_x: f64, frame_y: f64, frame_w: f64, frame_h: f64, space_list: ?[*]const c_int, space_count: c_int, has_focus: bool) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    var f = flags;
    if (f == 0x0) f |= ~f;

    var did_output = false;
    displayWriteBytes(stream, "{\n");

    if ((f & abi.DISPLAY_PROPERTY_ID) != 0) {
        displayWriteBytes(stream, "\t\"id\":");
        displayWriteInt(stream, @bitCast(did));
        did_output = true;
    }

    if ((f & abi.DISPLAY_PROPERTY_UUID) != 0) {
        if (did_output) displayWriteBytes(stream, ",\n");
        displayWriteBytes(stream, "\t\"uuid\":\"");
        displayWriteZ(stream, if (uuid_str) |s| s else "<unknown>");
        displayWriteBytes(stream, "\"");
        did_output = true;
    }

    if ((f & abi.DISPLAY_PROPERTY_INDEX) != 0) {
        if (did_output) displayWriteBytes(stream, ",\n");
        displayWriteBytes(stream, "\t\"index\":");
        displayWriteInt(stream, arrange_index);
        did_output = true;
    }

    if ((f & abi.DISPLAY_PROPERTY_LABEL) != 0) {
        if (did_output) displayWriteBytes(stream, ",\n");
        displayWriteBytes(stream, "\t\"label\":\"");
        displayWriteZ(stream, if (label_str) |s| s else "");
        displayWriteBytes(stream, "\"");
        did_output = true;
    }

    if ((f & abi.DISPLAY_PROPERTY_FRAME) != 0) {
        if (did_output) displayWriteBytes(stream, ",\n");
        displayWriteBytes(stream, "\t\"frame\":{\n\t\t\"x\":");
        displayWriteFloat(stream, frame_x);
        displayWriteBytes(stream, ",\n\t\t\"y\":");
        displayWriteFloat(stream, frame_y);
        displayWriteBytes(stream, ",\n\t\t\"w\":");
        displayWriteFloat(stream, frame_w);
        displayWriteBytes(stream, ",\n\t\t\"h\":");
        displayWriteFloat(stream, frame_h);
        displayWriteBytes(stream, "\n\t}");
        did_output = true;
    }

    if ((f & abi.DISPLAY_PROPERTY_SPACES) != 0) {
        if (did_output) displayWriteBytes(stream, ",\n");
        displayWriteBytes(stream, "\t\"spaces\":[");
        if (space_count > 0) {
            const list = space_list orelse unreachable;
            var i: c_int = 0;
            while (i < space_count) : (i += 1) {
                if (i < space_count - 1) {
                    displayWriteInt(stream, list[@intCast(i)]);
                    displayWriteBytes(stream, ", ");
                } else {
                    displayWriteInt(stream, list[@intCast(i)]);
                }
            }
        }
        displayWriteBytes(stream, "]");
        did_output = true;
    }

    if ((f & abi.DISPLAY_PROPERTY_HAS_FOCUS) != 0) {
        if (did_output) displayWriteBytes(stream, ",\n");
        displayWriteBytes(stream, "\t\"has-focus\":");
        displayWriteZ(stream, abi.yabai_json_bool(has_focus));
    }

    displayWriteBytes(stream, "\n}");
}
