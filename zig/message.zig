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
