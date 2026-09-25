// Message tokenizer boundary for the yabai migration (step 3).
//
// Pinned toolchain: Zig 0.16.0. Mirrors the token helpers in
// src/message.c. Layout contract with the C structs, checked both by
// the comptime block below and by _Static_assert in tests/src/zig_message.c.

const std = @import("std");

pub const MsgToken = extern struct {
    text: ?[*]u8,
    length: c_int,
};

pub const TOKEN_TYPE_INVALID: c_int = 0;
pub const TOKEN_TYPE_UNKNOWN: c_int = 1;
pub const TOKEN_TYPE_INT: c_int = 2;
pub const TOKEN_TYPE_FLOAT: c_int = 3;
pub const TOKEN_TYPE_U32: c_int = 4;
pub const TOKEN_TYPE_STRING: c_int = 5;

pub const TokenValueUnion = extern union {
    int_value: c_int,
    float_value: f32,
    u32_value: u32,
    string_value: ?[*]u8,
};

pub const TokenValue = extern struct {
    token: MsgToken,
    type: c_int,
    u: TokenValueUnion,
};

comptime {
    if (@sizeOf(MsgToken) != 16) @compileError("MsgToken layout drift");
    if (@offsetOf(MsgToken, "length") != 8) @compileError("MsgToken.length drift");
    if (@sizeOf(TokenValue) != 32) @compileError("TokenValue layout drift");
    if (@offsetOf(TokenValue, "type") != 16) @compileError("TokenValue.type drift");
    if (@offsetOf(TokenValue, "u") != 24) @compileError("TokenValue.u drift");
}

// Mirrors get_token(): scan to NUL, then advance past it unless the next
// byte is the final NUL (double-NUL stop). Loop counters stay c_int so
// degenerate lengths behave exactly as in C.
export fn yabai_message_get_token(message: *[*:0]u8, out: *MsgToken) void {
    @setRuntimeSafety(false);
    const start = message.*;
    var p = start;
    while (p[0] != 0) {
        p += 1;
    }
    out.text = start;
    out.length = @intCast(p - start);
    // p[0] is always NUL here; C leaves the cursor at the NUL when the
    // next byte is the final NUL, and past it otherwise.
    if (p[1] != 0) {
        message.* = p + 1;
    } else {
        message.* = p;
    }
}

export fn yabai_token_prefix(token: *const MsgToken, match: [*:0]const u8) bool {
    @setRuntimeSafety(false);
    const text = token.text orelse unreachable;
    var at = match;
    var i: c_int = 0;
    while (i < token.length) {
        if (at[0] == 0) return true;
        if (text[@intCast(i)] != at[0]) return false;
        i += 1;
        at += 1;
    }
    return at[0] == 0;
}

export fn yabai_token_equals(token: *const MsgToken, match: [*:0]const u8) bool {
    @setRuntimeSafety(false);
    const text = token.text orelse unreachable;
    var at = match;
    var i: c_int = 0;
    while (i < token.length) {
        if (at[0] == 0 or text[@intCast(i)] != at[0]) return false;
        i += 1;
        at += 1;
    }
    return at[0] == 0;
}

export fn yabai_token_is_valid(token: *const MsgToken) bool {
    @setRuntimeSafety(false);
    return token.text != null and token.length > 0;
}

// Mirrors token_is_positive_integer(), including wrap-on-overflow (the C
// build runs with wrapping -O0 codegen; safety is off here as well).
export fn yabai_token_is_positive_integer(token: *const MsgToken, value: *c_int) bool {
    @setRuntimeSafety(false);
    value.* = 0;
    const text = token.text orelse unreachable;
    var i: c_int = 0;
    while (i < token.length) {
        const c: u8 = text[@intCast(i)];
        if (c < '0' or c > '9') return false;
        value.* = value.* * 10 + @as(c_int, c - '0');
        i += 1;
    }
    return true;
}

export fn yabai_token_is_hexadecimal(token: *const MsgToken, value: *u32) bool {
    @setRuntimeSafety(false);
    value.* = 0;
    if (token.length <= 2) return false;
    const text = token.text orelse unreachable;
    if (text[0] != '0') return false;
    if (text[1] != 'x' and text[1] != 'X') return false;
    var i: c_int = 2;
    while (i < token.length) {
        const c: u8 = text[@intCast(i)];
        const nibble: u32 = if (c >= '0' and c <= '9')
            c - '0'
        else if (c >= 'a' and c <= 'f')
            c - 'a' + 10
        else if (c >= 'A' and c <= 'F')
            c - 'A' + 10
        else
            return false;
        value.* = value.* * 16 + nibble;
        i += 1;
    }
    return true;
}

// Uses the same libc strtof as the C helper, so float parsing (hex
// floats, inf/nan, exponents) matches by construction. Resolves from
// libc at the final link of the test binary and daemon.
extern "c" fn strtof(nptr: [*:0]const u8, endptr: *? [*:0]u8) f32;

export fn yabai_token_is_float(token: *const MsgToken, value: *f32) bool {
    @setRuntimeSafety(false);
    const text = token.text orelse unreachable;
    var end: ?[*:0]u8 = null;
    const v = strtof(@ptrCast(text), &end);
    const e = end orelse {
        value.* = 0;
        return false;
    };
    if (e[0] != 0) {
        value.* = 0;
        return false;
    }
    value.* = v;
    return true;
}

// Byte-identical port of parse_key_value_pair(), including the in-place
// NUL write and the untouched-exclusion-flag paths.
export fn yabai_parse_key_value_pair(token: [*:0]u8, key: *?[*]u8, value: *?[*]u8, exclusion: *bool) void {
    @setRuntimeSafety(false);
    key.* = token;
    var t = token;
    while (t[0] != 0) {
        const fst = t[0];
        const snd = t[1];
        if (fst == '!' and snd == '=') break;
        if (fst == '=') break;
        t += 1;
    }
    const index: usize = if (t[0] == '!' and t[1] == '=') 2 else 1;
    const check: u8 = if (index == 2) '!' else '=';
    if (t[0] != check) {
        key.* = null;
        value.* = null;
    } else if (t[index] != 0) {
        t[0] = 0;
        value.* = t + index;
        exclusion.* = index == 2;
    } else {
        value.* = null;
    }
}

// NUL-terminated comparison against a known literal, mirroring
// string_equals() for the non-null inputs these parsers receive.
fn litEqual(text: [*:0]const u8, comptime lit: []const u8) bool {
    @setRuntimeSafety(false);
    var i: usize = 0;
    while (i < lit.len) : (i += 1) {
        if (text[i] != lit[i]) return false;
    }
    return text[i] == 0;
}

export fn yabai_parse_value_type(typ: [*:0]const u8) u8 {
    @setRuntimeSafety(false);
    if (litEqual(typ, "abs")) return 0x1;
    if (litEqual(typ, "rel")) return 0x2;
    return 0;
}

export fn yabai_parse_resize_handle(handle: [*:0]const u8) u8 {
    @setRuntimeSafety(false);
    if (litEqual(handle, "top")) return 0x01;
    if (litEqual(handle, "bottom")) return 0x02;
    if (litEqual(handle, "left")) return 0x04;
    if (litEqual(handle, "right")) return 0x08;
    if (litEqual(handle, "top_left")) return 0x01 | 0x04;
    if (litEqual(handle, "top_right")) return 0x01 | 0x08;
    if (litEqual(handle, "bottom_left")) return 0x02 | 0x04;
    if (litEqual(handle, "bottom_right")) return 0x02 | 0x08;
    if (litEqual(handle, "abs")) return 0x10;
    return 0;
}

pub const FrameField = extern struct {
    ptr: ?[*]u8,
    len: c_int,
};

pub const SOCKET_FRAME_MAX: c_int = 1024 * 1024;

comptime {
    if (@sizeOf(FrameField) != 16) @compileError("FrameField size drift");
}

// Checked wire-format decoder: validates the native length prefix and
// framing before exposing any field, so no allocation or parsing ever
// runs on unvalidated sizes. Accepts exactly the CLI-produced language
// (NUL-separated args plus final NUL); field pointers borrow the frame.
export fn yabai_socket_frame_decode(
    frame: [*]const u8,
    frame_size: usize,
    fields: [*]FrameField,
    max_fields: usize,
    out_count: *usize,
) c_int {
    @setRuntimeSafety(false);
    out_count.* = 0;
    if (frame_size < @sizeOf(c_int)) return -1;

    const declared: c_int = @as(*align(1) const c_int, @ptrCast(frame)).*;
    if (declared <= 0 or declared > SOCKET_FRAME_MAX) return -1;
    const payload_len: usize = @intCast(declared);
    if (frame_size != @sizeOf(c_int) + payload_len) return -1;
    if (frame[payload_len + 3] != 0) return -1;

    var count: usize = 0;
    var off: usize = 0;
    while (off < payload_len) {
        if (count >= max_fields) return -1;
        var end = off;
        while (end < payload_len and frame[4 + end] != 0) : (end += 1) {}
        if (end >= payload_len) return -1;
        fields[count].ptr = @constCast(frame + 4 + off);
        fields[count].len = @intCast(end - off);
        count += 1;
        off = end + 1;
    }
    if (count == 0) return -1;
    if (fields[count - 1].len != 0) return -1;
    out_count.* = count;
    return 0;
}

pub const Properties = extern struct {
    token: MsgToken,
    did_parse: bool,
    did_error: bool,
    flags: u64,
};

comptime {
    if (@sizeOf(Properties) != 32) @compileError("Properties size drift");
    if (@offsetOf(Properties, "flags") != 24) @compileError("Properties.flags drift");
}

fn strEqualZ(a: [*:0]const u8, b: [*:0]const u8) bool {
    @setRuntimeSafety(false);
    var i: usize = 0;
    while (true) {
        if (a[i] != b[i]) return false;
        if (a[i] == 0) return true;
        i += 1;
    }
}

export fn yabai_parse_property(
    properties: *Properties,
    property: [*:0]const u8,
    property_val: [*]const u64,
    property_str: [*]const [*:0]const u8,
    property_count: c_int,
) bool {
    @setRuntimeSafety(false);
    var i: c_int = 0;
    while (i < property_count) {
        const idx: usize = @intCast(i);
        if (strEqualZ(property, property_str[idx])) {
            properties.flags |= property_val[idx];
            return true;
        }
        i += 1;
    }
    return false;
}

extern "c" fn fwrite(buf: *const anyopaque, size: usize, nmemb: usize, stream: ?*anyopaque) usize;

// Byte-identical equivalent of daemon_fail() for the parse_properties
// "'%.*s' is not a valid property.\n" emission: the 0x07 failure marker,
// a quote, the segment bytes, then the suffix. Written with fwrite so no
// varargs ABI is involved; the byte sequence matches the fprintf output.
fn parsePropertiesFail(rsp: ?*anyopaque, segment: [*]const u8, seg_len: usize) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    const marker: u8 = 0x07;
    _ = fwrite(@ptrCast(&marker), 1, 1, stream);
    const quote: u8 = '\'';
    _ = fwrite(@ptrCast(&quote), 1, 1, stream);
    if (seg_len > 0) {
        _ = fwrite(@ptrCast(segment), 1, seg_len, stream);
    }
    const suffix: []const u8 = "' is not a valid property.\n";
    _ = fwrite(@ptrCast(suffix.ptr), 1, suffix.len, stream);
}

// Byte-identical port of parse_properties() in src/message.c: comma
// splitting with in-place NUL writes, flag accumulation through
// yabai_parse_property, and daemon_fail error emission. The comma branch
// emits i-cursor bytes (C passes i-cursor+1 but %.*s stops at the NUL
// just written); the final-segment branch emits the full i-cursor+1.
export fn yabai_parse_properties(
    rsp: ?*anyopaque,
    token: *const MsgToken,
    property_val: [*]const u64,
    property_str: [*]const [*:0]const u8,
    property_count: c_int,
    out: *Properties,
) void {
    @setRuntimeSafety(false);
    out.token = token.*;
    out.did_parse = false;
    out.did_error = false;
    out.flags = 0;
    if (!yabai_token_is_valid(token)) return;
    if (yabai_token_prefix(token, "--")) return;
    out.did_parse = true;
    const text = token.text orelse return;
    const n: usize = @intCast(token.length);
    var cursor: usize = 0;
    var i: usize = 0;
    while (i < n) : (i += 1) {
        if (i + 1 == n) {
            const seg: [*:0]const u8 = @ptrCast(text + cursor);
            if (!yabai_parse_property(out, seg, property_val, property_str, property_count)) {
                parsePropertiesFail(rsp, text + cursor, i - cursor + 1);
                out.did_error = true;
            }
        } else if (text[i] == ',') {
            text[i] = 0;
            const seg: [*:0]const u8 = @ptrCast(text + cursor);
            if (!yabai_parse_property(out, seg, property_val, property_str, property_count)) {
                parsePropertiesFail(rsp, text + cursor, i - cursor);
                out.did_error = true;
            }
            cursor = i + 1;
        }
    }
}

pub const LABEL_DISPLAY: c_int = 0;
pub const LABEL_SPACE: c_int = 1;
pub const LABEL_WINDOW: c_int = 2;

extern "c" fn malloc(size: usize) ?[*]u8;

// Byte-identical equivalent of daemon_fail() for the three parse_label()
// emissions: the 0x07 failure marker, a quote, the length-bounded segment
// bytes, then the suffix. Written with fwrite so no varargs ABI is
// involved; the byte sequence matches the fprintf output.
fn labelFail(rsp: ?*anyopaque, segment: [*]const u8, seg_len: usize, suffix: []const u8) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    const marker: u8 = 0x07;
    _ = fwrite(@ptrCast(&marker), 1, 1, stream);
    const quote: u8 = '\'';
    _ = fwrite(@ptrCast(&quote), 1, 1, stream);
    if (seg_len > 0) {
        _ = fwrite(@ptrCast(segment), 1, seg_len, stream);
    }
    _ = fwrite(@ptrCast(suffix.ptr), 1, suffix.len, stream);
}

const reserved_display_identifiers = [_][*:0]const u8{
    "north", "east", "south", "west",
    "prev",  "next", "first", "last",
    "recent", "mouse",
};

const reserved_space_identifiers = [_][*:0]const u8{
    "prev", "next", "first", "last",
    "recent", "mouse",
};

const reserved_window_identifiers = [_][*:0]const u8{
    "float",  "sticky", "shadow", "split",
    "zoom-parent", "zoom-fullscreen",
    "windowed-fullscreen", "native-fullscreen",
    "expose", "pip",
    "recover",
};

// Byte-identical port of parse_label() in src/message.c: invalid tokens
// yield NULL with success, non-strings and reserved keywords fail with
// daemon_fail emission, and valid labels are copied into libc malloc'd
// memory (owned by the C caller). Type dispatch and the reserved-keyword
// scans reuse yabai_token_to_value/yabai_token_equals on the same
// keyword tables as C.
export fn yabai_parse_label(
    rsp: ?*anyopaque,
    token: *const MsgToken,
    label_type: c_int,
    label_out: *?[*]u8,
) bool {
    @setRuntimeSafety(false);
    var value: TokenValue = std.mem.zeroes(TokenValue);
    yabai_token_to_value(token, &value);
    if (value.type == TOKEN_TYPE_INVALID) {
        label_out.* = null;
        return true;
    }
    const n: usize = @intCast(token.length);
    const text: [*]const u8 = @ptrCast(token.text orelse unreachable);
    if (value.type != TOKEN_TYPE_STRING) {
        labelFail(rsp, text, n, "' cannot be used as a label.\n");
        return false;
    }
    switch (label_type) {
        LABEL_DISPLAY => {
            for (reserved_display_identifiers) |keyword| {
                if (yabai_token_equals(token, keyword)) {
                    labelFail(rsp, text, n, "' is a reserved keyword and cannot be used as a label.\n");
                    return false;
                }
            }
        },
        LABEL_SPACE => {
            for (reserved_space_identifiers) |keyword| {
                if (yabai_token_equals(token, keyword)) {
                    labelFail(rsp, text, n, "' is a reserved keyword and cannot be used as a label.\n");
                    return false;
                }
            }
        },
        LABEL_WINDOW => {
            for (reserved_window_identifiers) |keyword| {
                if (yabai_token_equals(token, keyword)) {
                    labelFail(rsp, text, n, "' is a reserved keyword and cannot be used as a scratchpad.\n");
                    return false;
                }
            }
        },
        else => {},
    }
    const mem = malloc(n + 1) orelse return false;
    var i: usize = 0;
    while (i < n) : (i += 1) {
        mem[i] = text[i];
    }
    mem[n] = 0;
    label_out.* = mem;
    return true;
}

export fn yabai_token_to_value(token: *const MsgToken, out: *TokenValue) void {
    @setRuntimeSafety(false);
    out.* = std.mem.zeroes(TokenValue);
    out.token = token.*;
    out.type = TOKEN_TYPE_INVALID;
    if (!yabai_token_is_valid(token)) return;
    if (yabai_token_is_positive_integer(token, &out.u.int_value)) {
        out.type = TOKEN_TYPE_INT;
    } else if (yabai_token_is_hexadecimal(token, &out.u.u32_value)) {
        out.type = TOKEN_TYPE_U32;
    } else if (yabai_token_is_float(token, &out.u.float_value)) {
        out.type = TOKEN_TYPE_FLOAT;
    } else if (token.text != null) {
        out.u.string_value = token.text;
        out.type = TOKEN_TYPE_STRING;
    } else {
        out.type = TOKEN_TYPE_UNKNOWN;
    }
}

pub const DIR_NORTH: c_int = 360;
pub const DIR_EAST: c_int = 90;
pub const DIR_SOUTH: c_int = 180;
pub const DIR_WEST: c_int = 270;
pub const STACK: c_int = 111;

pub const Selector = extern struct {
    token: MsgToken,
    did_parse: bool,
    payload: u64,
};

comptime {
    if (@sizeOf(Selector) != 32) @compileError("Selector size drift");
    if (@offsetOf(Selector, "did_parse") != 16) @compileError("Selector.did_parse drift");
    if (@offsetOf(Selector, "payload") != 24) @compileError("Selector.payload drift");
}

// Byte-identical equivalent of daemon_fail() for the parse_insert_selector
// "value '%.*s' is not a valid option for DIR_SEL\n" emission: the 0x07
// failure marker, the "value '" prefix, the token bytes, then the suffix.
// Written with fwrite so no varargs ABI is involved.
fn insertFail(rsp: ?*anyopaque, segment: [*]const u8, seg_len: usize) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    const marker: u8 = 0x07;
    _ = fwrite(@ptrCast(&marker), 1, 1, stream);
    const prefix: []const u8 = "value '";
    _ = fwrite(@ptrCast(prefix.ptr), 1, prefix.len, stream);
    if (seg_len > 0) {
        _ = fwrite(@ptrCast(segment), 1, seg_len, stream);
    }
    const suffix: []const u8 = "' is not a valid option for DIR_SEL\n";
    _ = fwrite(@ptrCast(suffix.ptr), 1, suffix.len, stream);
}

// Byte-identical port of parse_insert_selector() in src/message.c: consumes
// one token from the message cursor via yabai_message_get_token, maps the
// five direction keywords through yabai_token_equals, and emits the
// daemon_fail bytes on any other input. The dir value is stored as the raw
// 8-byte union payload (upper 32 bits zero, matching the C union init).
export fn yabai_parse_insert_selector(rsp: ?*anyopaque, message: *[*:0]u8, out: *Selector) void {
    @setRuntimeSafety(false);
    yabai_message_get_token(message, &out.token);
    out.did_parse = true;
    out.payload = 0;
    if (yabai_token_equals(&out.token, "north")) {
        out.payload = @as(u64, @intCast(DIR_NORTH));
    } else if (yabai_token_equals(&out.token, "east")) {
        out.payload = @as(u64, @intCast(DIR_EAST));
    } else if (yabai_token_equals(&out.token, "south")) {
        out.payload = @as(u64, @intCast(DIR_SOUTH));
    } else if (yabai_token_equals(&out.token, "west")) {
        out.payload = @as(u64, @intCast(DIR_WEST));
    } else if (yabai_token_equals(&out.token, "stack")) {
        out.payload = @as(u64, @intCast(STACK));
    } else {
        out.did_parse = false;
        const text: [*]const u8 = @ptrCast(out.token.text orelse unreachable);
        const n: usize = @intCast(out.token.length);
        insertFail(rsp, text, n);
    }
}
