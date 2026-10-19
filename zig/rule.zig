// Rule boundary for the yabai migration (step 3).
//
// Pinned toolchain: Zig 0.17.0. Mirrors rule_combine_effects(),
// rule_serialize() and rule_destroy() in src/rule.c plus the rule flag
// helpers in src/rule.h and ts_string_escape() in
// src/misc/helpers.h. RuleEffects mirrors struct rule_effects in src/rule.h
// (80 bytes); Rule mirrors struct rule (256 bytes, regex_t fields as
// opaque 32-byte arrays released via libc regfree under the same
// VALID-flag guards as C). Offsets asserted here and in
// tests/src/zig_rule.c. Heap ownership uses libc directly,
// matching string_copy()/free() in C.

pub const RULE_PROP_UD: c_int = 0;

pub const RULE_FOLLOW_SPACE: u16 = 0x01;
pub const RULE_OPACITY: u16 = 0x02;
pub const RULE_LAYER: u16 = 0x04;

pub const RULE_APP_VALID: u16 = 0x001;
pub const RULE_TITLE_VALID: u16 = 0x002;
pub const RULE_ROLE_VALID: u16 = 0x004;
pub const RULE_SUBROLE_VALID: u16 = 0x008;
pub const RULE_APP_EXCLUDE: u16 = 0x010;
pub const RULE_TITLE_EXCLUDE: u16 = 0x020;
pub const RULE_ROLE_EXCLUDE: u16 = 0x040;
pub const RULE_SUBROLE_EXCLUDE: u16 = 0x080;
pub const RULE_ONE_SHOT: u16 = 0x100;
pub const RULE_ONE_SHOT_REMOVE: u16 = 0x200;

pub const RuleEffects = extern struct {
    did: u32,
    sid: u64,
    opacity: f32,
    manage: c_int,
    sticky: c_int,
    mff: c_int,
    layer: c_int,
    fullscreen: c_int,
    grid: [6]c_uint,
    scratchpad: ?[*]u8,
    flags: u16,
};

comptime {
    if (@sizeOf(RuleEffects) != 80) @compileError("RuleEffects size drift");
    if (@offsetOf(RuleEffects, "sid") != 8) @compileError("RuleEffects.sid drift");
    if (@offsetOf(RuleEffects, "grid") != 40) @compileError("RuleEffects.grid drift");
    if (@offsetOf(RuleEffects, "scratchpad") != 64) @compileError("RuleEffects.scratchpad drift");
    if (@offsetOf(RuleEffects, "flags") != 72) @compileError("RuleEffects.flags drift");
}

// Opaque regex_t mirror: 32 bytes on 64-bit macOS. Never touched;
// rule_destroy() intentionally skips regfree() exactly as specified.
pub const RegexT = [32]u8;

pub const Rule = extern struct {
    label: ?[*]u8,
    app: ?[*]u8,
    title: ?[*]u8,
    role: ?[*]u8,
    subrole: ?[*]u8,
    app_regex: RegexT,
    title_regex: RegexT,
    role_regex: RegexT,
    subrole_regex: RegexT,
    effects: RuleEffects,
    flags: u16,
};

comptime {
    if (@sizeOf(RegexT) != 32) @compileError("RegexT size drift");
    if (@sizeOf(Rule) != 256) @compileError("Rule size drift");
    if (@offsetOf(Rule, "label") != 0) @compileError("Rule.label drift");
    if (@offsetOf(Rule, "app") != 8) @compileError("Rule.app drift");
    if (@offsetOf(Rule, "title") != 16) @compileError("Rule.title drift");
    if (@offsetOf(Rule, "role") != 24) @compileError("Rule.role drift");
    if (@offsetOf(Rule, "subrole") != 32) @compileError("Rule.subrole drift");
    if (@offsetOf(Rule, "app_regex") != 40) @compileError("Rule.app_regex drift");
    if (@offsetOf(Rule, "title_regex") != 72) @compileError("Rule.title_regex drift");
    if (@offsetOf(Rule, "role_regex") != 104) @compileError("Rule.role_regex drift");
    if (@offsetOf(Rule, "subrole_regex") != 136) @compileError("Rule.subrole_regex drift");
    if (@offsetOf(Rule, "effects") != 168) @compileError("Rule.effects drift");
    if (@offsetOf(Rule, "flags") != 248) @compileError("Rule.flags drift");
    if (RULE_APP_VALID != 0x001) @compileError("RULE_APP_VALID drift");
    if (RULE_TITLE_VALID != 0x002) @compileError("RULE_TITLE_VALID drift");
    if (RULE_ROLE_VALID != 0x004) @compileError("RULE_ROLE_VALID drift");
    if (RULE_SUBROLE_VALID != 0x008) @compileError("RULE_SUBROLE_VALID drift");
    if (RULE_APP_EXCLUDE != 0x010) @compileError("RULE_APP_EXCLUDE drift");
    if (RULE_TITLE_EXCLUDE != 0x020) @compileError("RULE_TITLE_EXCLUDE drift");
    if (RULE_ROLE_EXCLUDE != 0x040) @compileError("RULE_ROLE_EXCLUDE drift");
    if (RULE_SUBROLE_EXCLUDE != 0x080) @compileError("RULE_SUBROLE_EXCLUDE drift");
    if (RULE_ONE_SHOT != 0x100) @compileError("RULE_ONE_SHOT drift");
    if (RULE_ONE_SHOT_REMOVE != 0x200) @compileError("RULE_ONE_SHOT_REMOVE drift");
}

extern "c" fn malloc(size: usize) ?[*]u8;
extern "c" fn free(ptr: ?[*]u8) void;
extern "c" fn regfree(regex: *anyopaque) void;
extern fn yabai_in_range_ii_f(a: f32, b: f32, c: f32) bool;

fn stringCopy(s: [*:0]const u8) ?[*:0]u8 {
    @setRuntimeSafety(false);
    var len: usize = 0;
    while (s[len] != 0) : (len += 1) {}
    const mem = malloc(len + 1) orelse return null;
    const out: [*:0]u8 = @ptrCast(mem);
    var i: usize = 0;
    while (i < len) : (i += 1) {
        out[i] = s[i];
    }
    out[len] = 0;
    return out;
}

fn hasFlag(e: *const RuleEffects, flag: u16) bool {
    return (e.flags & flag) != 0;
}

fn setFlag(e: *RuleEffects, flag: u16) void {
    e.flags |= flag;
}

fn clearFlag(e: *RuleEffects, flag: u16) void {
    e.flags &= ~flag;
}

export fn yabai_rule_combine_effects(effects: *const RuleEffects, result: *RuleEffects) void {
    @setRuntimeSafety(false);

    if (effects.did != 0) {
        result.did = effects.did;
        if (hasFlag(effects, RULE_FOLLOW_SPACE)) {
            setFlag(result, RULE_FOLLOW_SPACE);
        } else {
            clearFlag(result, RULE_FOLLOW_SPACE);
        }
    }

    if (effects.sid != 0) {
        result.sid = effects.sid;
        if (hasFlag(effects, RULE_FOLLOW_SPACE)) {
            setFlag(result, RULE_FOLLOW_SPACE);
        } else {
            clearFlag(result, RULE_FOLLOW_SPACE);
        }
    }

    if (hasFlag(effects, RULE_OPACITY) and yabai_in_range_ii_f(effects.opacity, 0.0, 1.0)) {
        result.opacity = effects.opacity;
        setFlag(result, RULE_OPACITY);
    }

    if (hasFlag(effects, RULE_LAYER)) {
        result.layer = effects.layer;
        setFlag(result, RULE_LAYER);
    }

    if (effects.scratchpad) |pad| {
        if (result.scratchpad) |old| {
            free(old);
        }
        // Scratchpad strings are always NUL-terminated in C.
        result.scratchpad = stringCopy(@ptrCast(pad));
    }

    if (effects.manage != RULE_PROP_UD) result.manage = effects.manage;
    if (effects.sticky != RULE_PROP_UD) result.sticky = effects.sticky;
    if (effects.mff != RULE_PROP_UD) result.mff = effects.mff;
    if (effects.fullscreen != RULE_PROP_UD) result.fullscreen = effects.fullscreen;

    if (effects.grid[0] != 0 and effects.grid[1] != 0) {
        var i: usize = 0;
        while (i < 6) : (i += 1) {
            result.grid[i] = effects.grid[i];
        }
    }
}

// Mirrors rule_check_flag() in src/rule.h.
export fn yabai_rule_check_flag(r: *const Rule, flag: u16) bool {
    @setRuntimeSafety(false);
    return (r.flags & flag) != 0;
}

// Mirrors rule_set_flag() in src/rule.h.
export fn yabai_rule_set_flag(r: *Rule, flag: u16) void {
    @setRuntimeSafety(false);
    r.flags |= flag;
}

// Mirrors rule_clear_flag() in src/rule.h.
export fn yabai_rule_clear_flag(r: *Rule, flag: u16) void {
    @setRuntimeSafety(false);
    r.flags &= ~flag;
}

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

extern fn yabai_json_bool(value: bool) [*:0]const u8;
extern fn yabai_json_optional_bool(value: c_int) [*:0]const u8;
extern "c" fn snprintf(buf: [*]u8, n: usize, fmt: [*:0]const u8, ...) c_int;
extern "c" fn fwrite(buf: *const anyopaque, size: usize, nmemb: usize, stream: ?*anyopaque) usize;

fn strLenZ(s: [*:0]const u8) usize {
    @setRuntimeSafety(false);
    var len: usize = 0;
    while (s[len] != 0) : (len += 1) {}
    return len;
}

fn writeBytes(rsp: ?*anyopaque, s: []const u8) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    if (s.len == 0) return;
    _ = fwrite(@ptrCast(s.ptr), 1, s.len, stream);
}

fn writeZ(rsp: ?*anyopaque, s: [*:0]const u8) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    const len = strLenZ(s);
    if (len == 0) return;
    _ = fwrite(@ptrCast(s), 1, len, stream);
}

fn writeInt(rsp: ?*anyopaque, v: c_int) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    var tmp: [32]u8 = undefined;
    const n = snprintf(@ptrCast(&tmp), tmp.len, "%d", v);
    if (n <= 0) return;
    _ = fwrite(@ptrCast(&tmp), 1, @intCast(n), stream);
}

fn writeOpacity(rsp: ?*anyopaque, v: f32) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    var tmp: [512]u8 = undefined;
    const d: f64 = v;
    const n = snprintf(@ptrCast(&tmp), tmp.len, "%.4f", d);
    if (n <= 0) return;
    _ = fwrite(@ptrCast(&tmp), 1, @intCast(n), stream);
}

fn writeFlagsHex(rsp: ?*anyopaque, v: u32) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    var tmp: [32]u8 = undefined;
    const u: c_uint = @intCast(v);
    const n = snprintf(@ptrCast(&tmp), tmp.len, "0x%08x", u);
    if (n <= 0) return;
    _ = fwrite(@ptrCast(&tmp), 1, @intCast(n), stream);
}

fn writeGrid(rsp: ?*anyopaque, grid: [6]c_uint) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    var tmp: [256]u8 = undefined;
    const g0: c_int = @intCast(grid[0]);
    const g1: c_int = @intCast(grid[1]);
    const g2: c_int = @intCast(grid[2]);
    const g3: c_int = @intCast(grid[3]);
    const g4: c_int = @intCast(grid[4]);
    const g5: c_int = @intCast(grid[5]);
    const n = snprintf(@ptrCast(&tmp), tmp.len, "%d:%d:%d:%d:%d:%d", g0, g1, g2, g3, g4, g5);
    if (n <= 0) return;
    _ = fwrite(@ptrCast(&tmp), 1, @intCast(n), stream);
}

fn layerStr(layer: c_int) [*:0]const u8 {
    @setRuntimeSafety(false);
    if (layer == LAYER_AUTO) return "auto";
    if (layer == LAYER_BELOW) return "below";
    if (layer == LAYER_NORMAL) return "normal";
    if (layer == LAYER_ABOVE) return "above";
    return "";
}

// Byte-identical port of ts_string_escape() in src/misc/helpers.h.
// ts_alloc_unaligned() is a temp-storage bump allocator, not plain
// malloc; the Zig port uses libc malloc for equivalent ownership here
// (the serialize port frees these buffers, keeping the suite leak-clean)
// and returns NULL when no replacements are needed, exactly like C.
fn tsEscape(s: [*:0]const u8) ?[*:0]u8 {
    @setRuntimeSafety(false);
    var len: usize = 0;
    var num_replacements: usize = 0;
    while (s[len] != 0) {
        const c = s[len];
        if (c == '"' or c == '\\' or c == 0x08 or c == 0x0c or c == '\n' or c == '\r' or c == '\t') {
            num_replacements += 1;
        } else if (c <= 0x1f) {
            num_replacements += 5;
        }
        len += 1;
    }
    if (num_replacements == 0) return null;
    const total = len + num_replacements;
    const mem = malloc(total + 1) orelse return null;
    const out: [*]u8 = @ptrCast(mem);
    var dst: usize = 0;
    var i: usize = 0;
    while (i < len) : (i += 1) {
        const c = s[i];
        if (c == '"') {
            out[dst] = '\\';
            dst += 1;
            out[dst] = '"';
            dst += 1;
        } else if (c == '\\') {
            out[dst] = '\\';
            dst += 1;
            out[dst] = '\\';
            dst += 1;
        } else if (c == 0x08) {
            out[dst] = '\\';
            dst += 1;
            out[dst] = 'b';
            dst += 1;
        } else if (c == 0x0c) {
            out[dst] = '\\';
            dst += 1;
            out[dst] = 'f';
            dst += 1;
        } else if (c == '\n') {
            out[dst] = '\\';
            dst += 1;
            out[dst] = 'n';
            dst += 1;
        } else if (c == '\r') {
            out[dst] = '\\';
            dst += 1;
            out[dst] = 'r';
            dst += 1;
        } else if (c == '\t') {
            out[dst] = '\\';
            dst += 1;
            out[dst] = 't';
            dst += 1;
        } else if (c <= 0x1f) {
            const hex = "0123456789abcdef";
            out[dst] = '\\';
            dst += 1;
            out[dst] = 'u';
            dst += 1;
            out[dst] = '0';
            dst += 1;
            out[dst] = '0';
            dst += 1;
            out[dst] = hex[(c >> 4) & 0x0f];
            dst += 1;
            out[dst] = hex[c & 0x0f];
            dst += 1;
        } else {
            out[dst] = c;
            dst += 1;
        }
    }
    out[dst] = 0;
    return @ptrCast(mem);
}

export fn yabai_ts_string_escape(s: [*:0]const u8) ?[*:0]u8 {
    @setRuntimeSafety(false);
    return tsEscape(s);
}

// Byte-identical port of rule_serialize() in src/rule.c. The two
// manager-derived indices are lifted in as did_index/sid_index (seam
// pattern): C emits rule->effects.did ?
// display_manager_display_id_arrangement(...) : 0 and the space
// equivalent; Zig emits did_index/sid_index when did/sid are non-zero
// and 0 otherwise. Label and scratchpad are raw (never escaped), exactly
// like C. Numerics go through libc snprintf (%.4f, 0x%08x, %d) so float
// rounding and hex padding match by construction; all emission uses
// libc fwrite. json strings reuse yabai_json_bool/optional_bool.
export fn yabai_rule_serialize(rsp: ?*anyopaque, rule: *const Rule, index: c_int, did_index: c_int, sid_index: c_int) void {
    @setRuntimeSafety(false);
    const app_src: [*:0]const u8 = if (rule.app) |p| @ptrCast(p) else "";
    const title_src: [*:0]const u8 = if (rule.title) |p| @ptrCast(p) else "";
    const role_src: [*:0]const u8 = if (rule.role) |p| @ptrCast(p) else "";
    const subrole_src: [*:0]const u8 = if (rule.subrole) |p| @ptrCast(p) else "";
    const esc_app = if (rule.app) |p| tsEscape(@ptrCast(p)) else null;
    const esc_title = if (rule.title) |p| tsEscape(@ptrCast(p)) else null;
    const esc_role = if (rule.role) |p| tsEscape(@ptrCast(p)) else null;
    const esc_subrole = if (rule.subrole) |p| tsEscape(@ptrCast(p)) else null;
    const app_out: [*:0]const u8 = if (esc_app) |e| e else app_src;
    const title_out: [*:0]const u8 = if (esc_title) |e| e else title_src;
    const role_out: [*:0]const u8 = if (esc_role) |e| e else role_src;
    const subrole_out: [*:0]const u8 = if (esc_subrole) |e| e else subrole_src;
    const label_out: [*:0]const u8 = if (rule.label) |p| @ptrCast(p) else "";
    const scratch_out: [*:0]const u8 = if (rule.effects.scratchpad) |p| @ptrCast(p) else "";
    const display: c_int = if (rule.effects.did != 0) did_index else 0;
    const space: c_int = if (rule.effects.sid != 0) sid_index else 0;
    const follow_out = yabai_json_bool((rule.effects.flags & RULE_FOLLOW_SPACE) != 0);
    const manage_out = yabai_json_optional_bool(rule.effects.manage);
    const sticky_out = yabai_json_optional_bool(rule.effects.sticky);
    const mff_out = yabai_json_optional_bool(rule.effects.mff);
    const layer_out: [*:0]const u8 = if ((rule.effects.flags & RULE_LAYER) != 0) layerStr(rule.effects.layer) else "";
    const fullscreen_out = yabai_json_optional_bool(rule.effects.fullscreen);
    const oneshot_out = yabai_json_bool((rule.flags & RULE_ONE_SHOT) != 0);
    const flags_combined: u32 = (@as(u32, rule.effects.flags) << 16) | @as(u32, rule.flags);

    const stream = rsp orelse {
        if (esc_app) |e| free(@ptrCast(e));
        if (esc_title) |e| free(@ptrCast(e));
        if (esc_role) |e| free(@ptrCast(e));
        if (esc_subrole) |e| free(@ptrCast(e));
        return;
    };

    writeBytes(stream, "{\n\t\"index\":");
    writeInt(stream, index);
    writeBytes(stream, ",\n\t\"label\":\"");
    writeZ(stream, label_out);
    writeBytes(stream, "\",\n\t\"app\":\"");
    writeZ(stream, app_out);
    writeBytes(stream, "\",\n\t\"title\":\"");
    writeZ(stream, title_out);
    writeBytes(stream, "\",\n\t\"role\":\"");
    writeZ(stream, role_out);
    writeBytes(stream, "\",\n\t\"subrole\":\"");
    writeZ(stream, subrole_out);
    writeBytes(stream, "\",\n\t\"display\":");
    writeInt(stream, display);
    writeBytes(stream, ",\n\t\"space\":");
    writeInt(stream, space);
    writeBytes(stream, ",\n\t\"follow_space\":");
    writeZ(stream, follow_out);
    writeBytes(stream, ",\n\t\"opacity\":");
    writeOpacity(stream, rule.effects.opacity);
    writeBytes(stream, ",\n\t\"manage\":");
    writeZ(stream, manage_out);
    writeBytes(stream, ",\n\t\"sticky\":");
    writeZ(stream, sticky_out);
    writeBytes(stream, ",\n\t\"mouse_follows_focus\":");
    writeZ(stream, mff_out);
    writeBytes(stream, ",\n\t\"sub-layer\":\"");
    writeZ(stream, layer_out);
    writeBytes(stream, "\",\n\t\"native-fullscreen\":");
    writeZ(stream, fullscreen_out);
    writeBytes(stream, ",\n\t\"grid\":\"");
    writeGrid(stream, rule.effects.grid);
    writeBytes(stream, "\",\n\t\"scratchpad\":\"");
    writeZ(stream, scratch_out);
    writeBytes(stream, "\",\n\t\"one-shot\":");
    writeZ(stream, oneshot_out);
    writeBytes(stream, ",\n\t\"flags\":\"");
    writeFlagsHex(stream, flags_combined);
    writeBytes(stream, "\"\n}");

    if (esc_app) |e| free(@ptrCast(e));
    if (esc_title) |e| free(@ptrCast(e));
    if (esc_role) |e| free(@ptrCast(e));
    if (esc_subrole) |e| free(@ptrCast(e));
}

// yabai_parse_rule port (src/message.c parse_rule).
//
// Mirrors struct token (16 bytes) and struct token_value (32 bytes) for
// the display/space selector paths; the union payload of the 32-byte
// Selector mirror is zeroed where C leaves it implicit (same convention
// as yabai_parse_insert_selector). Tokenizer, key/value split, float
// range check, and flag helpers are reused via extern calls into the
// already-ported message/util objects; string_copy/regcomp/sscanf come
// from libc exactly like C. All daemon_fail emissions use the established
// fwrite pattern (0x07 marker + literal bytes), so no varargs ABI is
// involved. The acting display/space ids are lifted in as parameters
// (same seam as yabai_rule_serialize's did_index/sid_index): C evaluates
// display_manager_active_display_id()/space_manager_active_space()
// eagerly at the call site, which cannot run in-process under test.
pub const MsgToken = extern struct {
    text: ?[*]u8,
    length: c_int,
};

comptime {
    if (@sizeOf(MsgToken) != 16) @compileError("MsgToken size drift");
    if (@offsetOf(MsgToken, "length") != 8) @compileError("MsgToken.length drift");
}

const TOKEN_TYPE_INVALID: c_int = 0;
const TOKEN_TYPE_INT: c_int = 2;
const TOKEN_TYPE_STRING: c_int = 5;

const REG_EXTENDED: c_int = 1;

pub const RULE_PROP_ON: c_int = 1;
pub const RULE_PROP_OFF: c_int = 2;

const ParseValueUnion = extern union {
    int_value: c_int,
    float_value: f32,
    u32_value: u32,
    string_value: ?[*]u8,
};

const ParseTokenValue = extern struct {
    token: MsgToken,
    type: c_int,
    u: ParseValueUnion,
};

pub const RuleSelector = extern struct {
    token: MsgToken,
    did_parse: bool,
    payload: u64,
};

const DisplayManagerState = extern struct {
    current_display_id: u32,
    last_display_id: u32,
};

const SpaceManagerState = extern struct {
    table: [40]u8,
    current_space_id: u64,
    last_space_id: u64,
};

const DisplayLabel = extern struct {
    did: u32,
    label: ?[*]u8,
};

const SpaceLabel = extern struct {
    sid: u64,
    label: ?[*]u8,
};

comptime {
    if (@sizeOf(ParseTokenValue) != 32) @compileError("ParseTokenValue size drift");
    if (@offsetOf(ParseTokenValue, "type") != 16) @compileError("ParseTokenValue.type drift");
    if (@offsetOf(ParseTokenValue, "u") != 24) @compileError("ParseTokenValue.u drift");
    if (@sizeOf(RuleSelector) != 32) @compileError("RuleSelector size drift");
    if (@offsetOf(RuleSelector, "did_parse") != 16) @compileError("RuleSelector.did_parse drift");
    if (@offsetOf(RuleSelector, "payload") != 24) @compileError("RuleSelector.payload drift");
    if (@offsetOf(DisplayManagerState, "last_display_id") != 4) @compileError("DisplayManagerState.last_display_id drift");
    if (@offsetOf(SpaceManagerState, "current_space_id") != 40) @compileError("SpaceManagerState.current_space_id drift");
    if (@offsetOf(SpaceManagerState, "last_space_id") != 48) @compileError("SpaceManagerState.last_space_id drift");
    if (@offsetOf(DisplayLabel, "did") != 0) @compileError("DisplayLabel.did drift");
    if (@offsetOf(SpaceLabel, "sid") != 0) @compileError("SpaceLabel.sid drift");
    if (TOKEN_TYPE_INVALID != 0) @compileError("TOKEN_TYPE_INVALID drift");
    if (TOKEN_TYPE_INT != 2) @compileError("TOKEN_TYPE_INT drift");
    if (TOKEN_TYPE_STRING != 5) @compileError("TOKEN_TYPE_STRING drift");
    if (REG_EXTENDED != 1) @compileError("REG_EXTENDED drift");
    if (RULE_PROP_ON != 1) @compileError("RULE_PROP_ON drift");
    if (RULE_PROP_OFF != 2) @compileError("RULE_PROP_OFF drift");
}

extern fn yabai_message_get_token(message: *[*:0]u8, out: *MsgToken) void;
extern fn yabai_token_is_valid(token: *const MsgToken) bool;
extern fn yabai_token_equals(token: *const MsgToken, match: [*:0]const u8) bool;
extern fn yabai_token_to_value(token: *const MsgToken, out: *ParseTokenValue) void;
extern fn yabai_parse_key_value_pair(token: [*:0]u8, key: *?[*]u8, value: *?[*]u8, exclusion: *bool) void;
extern "c" fn sscanf(s: [*:0]const u8, fmt: [*:0]const u8, ...) c_int;
extern "c" fn regcomp(regex: *anyopaque, pattern: [*:0]const u8, cflags: c_int) c_int;

extern fn display_manager_arrangement_display_id(arrangement: c_int) u32;
extern fn display_manager_find_closest_display_in_direction(acting_did: u32, direction: c_int) u32;
extern fn display_manager_prev_display_id(did: u32) u32;
extern fn display_manager_next_display_id(did: u32) u32;
extern fn display_manager_first_display_id() u32;
extern fn display_manager_last_display_id() u32;
extern fn display_manager_cursor_display_id() u32;
extern fn display_manager_get_display_for_label(dm: *DisplayManagerState, label: [*:0]u8) ?*DisplayLabel;
extern fn space_manager_mission_control_space(desktop_id: c_int) u64;
extern fn space_manager_cursor_space() u64;
extern fn space_manager_prev_space(sid: u64) u64;
extern fn space_manager_next_space(sid: u64) u64;
extern fn space_manager_first_space() u64;
extern fn space_manager_last_space() u64;
extern fn space_manager_get_space_for_label(sm: *SpaceManagerState, label: [*:0]u8) ?*SpaceLabel;
extern var g_display_manager: DisplayManagerState;
extern var g_space_manager: SpaceManagerState;

const DIR_NORTH: c_int = 360;
const DIR_EAST: c_int = 90;
const DIR_SOUTH: c_int = 180;
const DIR_WEST: c_int = 270;

// Same reserved table as parse_label()'s LABEL_WINDOW path in
// src/message.c (order preserved). Contents are pinned behaviorally by
// the 11-word scratchpad loop in tests/src/zig_rule.c.
const reserved_window_identifiers = [_][*:0]const u8{
    "float", "sticky", "shadow", "split",
    "zoom-parent", "zoom-fullscreen",
    "windowed-fullscreen", "native-fullscreen",
    "expose", "pip",
    "recover",
};

fn keyEqual(key: [*:0]const u8, lit: [*:0]const u8) bool {
    @setRuntimeSafety(false);
    var i: usize = 0;
    while (true) {
        if (key[i] != lit[i]) return false;
        if (key[i] == 0) return true;
        i += 1;
    }
}

fn failMark(rsp: ?*anyopaque) ?*anyopaque {
    @setRuntimeSafety(false);
    const stream = rsp orelse return null;
    const marker: u8 = 0x07;
    _ = fwrite(@ptrCast(&marker), 1, 1, stream);
    return stream;
}

fn failInvalidPair(rsp: ?*anyopaque, text: [*:0]const u8) void {
    @setRuntimeSafety(false);
    const stream = failMark(rsp) orelse return;
    writeBytes(stream, "invalid key-value pair '");
    writeZ(stream, text);
    writeBytes(stream, "'\n");
}

fn failInvalidValue(rsp: ?*anyopaque, value: [*:0]const u8, key: [*:0]const u8) void {
    @setRuntimeSafety(false);
    const stream = failMark(rsp) orelse return;
    writeBytes(stream, "invalid value '");
    writeZ(stream, value);
    writeBytes(stream, "' for key '");
    writeZ(stream, key);
    writeBytes(stream, "'\n");
}

fn failInvalidRegex(rsp: ?*anyopaque, value: [*:0]const u8, key: [*:0]const u8) void {
    @setRuntimeSafety(false);
    const stream = failMark(rsp) orelse return;
    writeBytes(stream, "invalid regex pattern '");
    writeZ(stream, value);
    writeBytes(stream, "' for key '");
    writeZ(stream, key);
    writeBytes(stream, "'\n");
}

fn failUnknownKey(rsp: ?*anyopaque, key: [*:0]const u8) void {
    @setRuntimeSafety(false);
    const stream = failMark(rsp) orelse return;
    writeBytes(stream, "unknown key '");
    writeZ(stream, key);
    writeBytes(stream, "'\n");
}

fn failMissingFilter(rsp: ?*anyopaque) void {
    @setRuntimeSafety(false);
    const stream = failMark(rsp) orelse return;
    writeBytes(stream, "missing required key-value pair 'app[!]=..' or 'title[!]=..'\n");
}

fn failUnsupportedExclusion(rsp: ?*anyopaque, key: [*:0]const u8) void {
    @setRuntimeSafety(false);
    const stream = failMark(rsp) orelse return;
    writeBytes(stream, "unsupported token '!' (exclusion) given for key '");
    writeZ(stream, key);
    writeBytes(stream, "'\n");
}

fn failFixed(rsp: ?*anyopaque, msg: []const u8) void {
    @setRuntimeSafety(false);
    const stream = failMark(rsp) orelse return;
    writeBytes(stream, msg);
}

fn failDisplayArrangement(rsp: ?*anyopaque, v: c_int) void {
    @setRuntimeSafety(false);
    const stream = failMark(rsp) orelse return;
    writeBytes(stream, "could not locate display with arrangement index '");
    writeInt(stream, v);
    writeBytes(stream, "'.\n");
}

fn failSpaceMissionControl(rsp: ?*anyopaque, v: c_int) void {
    @setRuntimeSafety(false);
    const stream = failMark(rsp) orelse return;
    writeBytes(stream, "could not locate space with mission-control index '");
    writeInt(stream, v);
    writeBytes(stream, "'.\n");
}

fn failSelectorOption(rsp: ?*anyopaque, text: [*]const u8, len: usize, suffix: []const u8) void {
    @setRuntimeSafety(false);
    const stream = failMark(rsp) orelse return;
    writeBytes(stream, "value '");
    if (len > 0) {
        _ = fwrite(@ptrCast(text), 1, len, stream);
    }
    writeBytes(stream, suffix);
}

// Byte-identical port of parse_display_selector() in src/message.c.
// The union payload is zeroed up front (C leaves it implicit); every
// manager query is an extern call with identical argument order.
fn parseDisplaySelector(rsp: ?*anyopaque, message: *[*:0]u8, acting_did: u32, out: *RuleSelector) void {
    @setRuntimeSafety(false);
    yabai_message_get_token(message, &out.token);
    out.did_parse = true;
    out.payload = 0;
    var value: ParseTokenValue = .{ .token = undefined, .type = TOKEN_TYPE_INVALID, .u = .{ .int_value = 0 } };
    yabai_token_to_value(&out.token, &value);
    if (value.type == TOKEN_TYPE_INT) {
        const did = display_manager_arrangement_display_id(value.u.int_value);
        if (did != 0) {
            out.payload = did;
        } else {
            failDisplayArrangement(rsp, value.u.int_value);
        }
    } else if (value.type == TOKEN_TYPE_STRING) {
        if (yabai_token_equals(&out.token, "north")) {
            if (acting_did != 0) {
                const did = display_manager_find_closest_display_in_direction(acting_did, DIR_NORTH);
                if (did != 0) {
                    out.payload = did;
                } else {
                    failFixed(rsp, "could not locate a northward display.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected display.\n");
            }
        } else if (yabai_token_equals(&out.token, "east")) {
            if (acting_did != 0) {
                const did = display_manager_find_closest_display_in_direction(acting_did, DIR_EAST);
                if (did != 0) {
                    out.payload = did;
                } else {
                    failFixed(rsp, "could not locate a eastward display.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected display.\n");
            }
        } else if (yabai_token_equals(&out.token, "south")) {
            if (acting_did != 0) {
                const did = display_manager_find_closest_display_in_direction(acting_did, DIR_SOUTH);
                if (did != 0) {
                    out.payload = did;
                } else {
                    failFixed(rsp, "could not locate a southward display.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected display.\n");
            }
        } else if (yabai_token_equals(&out.token, "west")) {
            if (acting_did != 0) {
                const did = display_manager_find_closest_display_in_direction(acting_did, DIR_WEST);
                if (did != 0) {
                    out.payload = did;
                } else {
                    failFixed(rsp, "could not locate a westward display.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected display.\n");
            }
        } else if (yabai_token_equals(&out.token, "prev")) {
            if (acting_did != 0) {
                const did = display_manager_prev_display_id(acting_did);
                if (did != 0) {
                    out.payload = did;
                } else {
                    failFixed(rsp, "could not locate the previous display.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected display.\n");
            }
        } else if (yabai_token_equals(&out.token, "next")) {
            if (acting_did != 0) {
                const did = display_manager_next_display_id(acting_did);
                if (did != 0) {
                    out.payload = did;
                } else {
                    failFixed(rsp, "could not locate the next display.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected display.\n");
            }
        } else if (yabai_token_equals(&out.token, "first")) {
            const did = display_manager_first_display_id();
            if (did != 0) {
                out.payload = did;
            } else {
                failFixed(rsp, "could not locate the first display.\n");
            }
        } else if (yabai_token_equals(&out.token, "last")) {
            const did = display_manager_last_display_id();
            if (did != 0) {
                out.payload = did;
            } else {
                failFixed(rsp, "could not locate the last display.\n");
            }
        } else if (yabai_token_equals(&out.token, "recent")) {
            out.payload = g_display_manager.last_display_id;
        } else if (yabai_token_equals(&out.token, "mouse")) {
            const did = display_manager_cursor_display_id();
            if (did != 0) {
                out.payload = did;
            } else {
                failFixed(rsp, "could not locate display containing cursor.\n");
            }
        } else {
            const s = value.u.string_value orelse unreachable;
            if (display_manager_get_display_for_label(&g_display_manager, @ptrCast(s))) |lbl| {
                out.did_parse = true;
                out.payload = lbl.did;
            } else {
                out.did_parse = false;
                const text: [*]const u8 = @ptrCast(out.token.text orelse unreachable);
                failSelectorOption(rsp, text, @intCast(out.token.length), "' is not a valid option for DISPLAY_SEL\n");
            }
        }
    } else if (value.type == TOKEN_TYPE_INVALID) {
        out.did_parse = false;
        const text: [*]const u8 = @ptrCast(out.token.text orelse unreachable);
        failSelectorOption(rsp, text, @intCast(out.token.length), "' is not a valid option for DISPLAY_SEL\n");
    } else {
        out.did_parse = false;
        const text: [*]const u8 = @ptrCast(out.token.text orelse unreachable);
        failSelectorOption(rsp, text, @intCast(out.token.length), "' is not a valid option for DISPLAY_SEL\n");
    }
}

// Byte-identical port of parse_space_selector() in src/message.c.
fn parseSpaceSelector(rsp: ?*anyopaque, message: *[*:0]u8, acting_sid: u64, out: *RuleSelector) void {
    @setRuntimeSafety(false);
    yabai_message_get_token(message, &out.token);
    out.did_parse = true;
    out.payload = 0;
    var value: ParseTokenValue = .{ .token = undefined, .type = TOKEN_TYPE_INVALID, .u = .{ .int_value = 0 } };
    yabai_token_to_value(&out.token, &value);
    if (value.type == TOKEN_TYPE_INT) {
        const sid = space_manager_mission_control_space(value.u.int_value);
        if (sid != 0) {
            out.payload = sid;
        } else {
            failSpaceMissionControl(rsp, value.u.int_value);
        }
    } else if (value.type == TOKEN_TYPE_STRING) {
        if (yabai_token_equals(&out.token, "prev")) {
            if (acting_sid != 0) {
                const sid = space_manager_prev_space(acting_sid);
                if (sid != 0) {
                    out.payload = sid;
                } else {
                    failFixed(rsp, "could not locate the previous space.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected space.\n");
            }
        } else if (yabai_token_equals(&out.token, "next")) {
            if (acting_sid != 0) {
                const sid = space_manager_next_space(acting_sid);
                if (sid != 0) {
                    out.payload = sid;
                } else {
                    failFixed(rsp, "could not locate the next space.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected space.\n");
            }
        } else if (yabai_token_equals(&out.token, "first")) {
            const sid = space_manager_first_space();
            if (sid != 0) {
                out.payload = sid;
            } else {
                failFixed(rsp, "could not locate the first space.\n");
            }
        } else if (yabai_token_equals(&out.token, "last")) {
            const sid = space_manager_last_space();
            if (sid != 0) {
                out.payload = sid;
            } else {
                failFixed(rsp, "could not locate the last space.\n");
            }
        } else if (yabai_token_equals(&out.token, "recent")) {
            out.payload = g_space_manager.last_space_id;
        } else if (yabai_token_equals(&out.token, "mouse")) {
            const sid = space_manager_cursor_space();
            if (sid != 0) {
                out.payload = sid;
            } else {
                failFixed(rsp, "could not locate space containing cursor.\n");
            }
        } else {
            const s = value.u.string_value orelse unreachable;
            if (space_manager_get_space_for_label(&g_space_manager, @ptrCast(s))) |lbl| {
                out.did_parse = true;
                out.payload = lbl.sid;
            } else {
                out.did_parse = false;
                const text: [*]const u8 = @ptrCast(out.token.text orelse unreachable);
                failSelectorOption(rsp, text, @intCast(out.token.length), "' is not a valid option for SPACE_SEL\n");
            }
        }
    } else if (value.type == TOKEN_TYPE_INVALID) {
        out.did_parse = false;
        const text: [*]const u8 = @ptrCast(out.token.text orelse unreachable);
        failSelectorOption(rsp, text, @intCast(out.token.length), "' is not a valid option for SPACE_SEL\n");
    } else {
        out.did_parse = false;
        const text: [*]const u8 = @ptrCast(out.token.text orelse unreachable);
        failSelectorOption(rsp, text, @intCast(out.token.length), "' is not a valid option for SPACE_SEL\n");
    }
}

// Byte-identical port of parse_rule() in src/message.c: key dispatch over
// label/scratchpad/app/title/role/subrole plus the display/space/grid/
// opacity/manage/sticky/mff/sub-layer/fullscreen effect keys, string_copy
// ownership, REG_EXTENDED regcomp with VALID/EXCLUDE flag sets, libc
// sscanf numerics (%f opacity with in_range_ii, six-%d grid), and the
// trailing missing-filter / unsupported-exclusion checks.
export fn yabai_parse_rule(rsp: ?*anyopaque, message: *[*:0]u8, rule: *Rule, token: *const MsgToken, acting_did: u32, acting_sid: u64) bool {
    @setRuntimeSafety(false);
    var unsupported_exclusion: ?[*]u8 = null;
    var did_parse = true;
    var has_filter = false;

    var tok = token.*;
    while (yabai_token_is_valid(&tok)) : (yabai_message_get_token(message, &tok)) {
        var key: ?[*]u8 = null;
        var val: ?[*]u8 = null;
        var exclusion = false;
        yabai_parse_key_value_pair(@ptrCast(tok.text orelse unreachable), &key, &val, &exclusion);

        if (key == null or val == null) {
            failInvalidPair(rsp, @ptrCast(tok.text orelse unreachable));
            did_parse = false;
            continue;
        }
        const k: [*:0]u8 = @ptrCast(key.?);
        const v: [*:0]u8 = @ptrCast(val.?);

        if (keyEqual(k, "label")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);
            rule.label = @ptrCast(stringCopy(v));
        } else if (keyEqual(k, "scratchpad")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            var valid = true;
            for (reserved_window_identifiers) |reserved| {
                if (keyEqual(v, reserved)) {
                    valid = false;
                    break;
                }
            }

            if (valid) {
                rule.effects.scratchpad = @ptrCast(stringCopy(v));
                rule.effects.manage = RULE_PROP_OFF;
            } else {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "app")) {
            has_filter = true;
            rule.app = @ptrCast(stringCopy(v));
            if (exclusion) yabai_rule_set_flag(rule, RULE_APP_EXCLUDE);
            if (regcomp(@ptrCast(&rule.app_regex), v, REG_EXTENDED) == 0) {
                yabai_rule_set_flag(rule, RULE_APP_VALID);
            } else {
                failInvalidRegex(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "title")) {
            has_filter = true;
            rule.title = @ptrCast(stringCopy(v));
            if (exclusion) yabai_rule_set_flag(rule, RULE_TITLE_EXCLUDE);
            if (regcomp(@ptrCast(&rule.title_regex), v, REG_EXTENDED) == 0) {
                yabai_rule_set_flag(rule, RULE_TITLE_VALID);
            } else {
                failInvalidRegex(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "role")) {
            has_filter = true;
            rule.role = @ptrCast(stringCopy(v));
            if (exclusion) yabai_rule_set_flag(rule, RULE_ROLE_EXCLUDE);
            if (regcomp(@ptrCast(&rule.role_regex), v, REG_EXTENDED) == 0) {
                yabai_rule_set_flag(rule, RULE_ROLE_VALID);
            } else {
                failInvalidRegex(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "subrole")) {
            has_filter = true;
            rule.subrole = @ptrCast(stringCopy(v));
            if (exclusion) yabai_rule_set_flag(rule, RULE_SUBROLE_EXCLUDE);
            if (regcomp(@ptrCast(&rule.subrole_regex), v, REG_EXTENDED) == 0) {
                yabai_rule_set_flag(rule, RULE_SUBROLE_VALID);
            } else {
                failInvalidRegex(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "display")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            var cursor: [*:0]u8 = v;
            if (cursor[0] == '^') {
                cursor += 1;
                setFlag(&rule.effects, RULE_FOLLOW_SPACE);
            }

            var selector: RuleSelector = undefined;
            parseDisplaySelector(rsp, &cursor, acting_did, &selector);
            if (selector.did_parse and @as(u32, @truncate(selector.payload)) != 0) {
                rule.effects.did = @truncate(selector.payload);
            } else {
                did_parse = false;
            }
        } else if (keyEqual(k, "space")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            var cursor: [*:0]u8 = v;
            if (cursor[0] == '^') {
                cursor += 1;
                setFlag(&rule.effects, RULE_FOLLOW_SPACE);
            }

            var selector: RuleSelector = undefined;
            parseSpaceSelector(rsp, &cursor, acting_sid, &selector);
            if (selector.did_parse and selector.payload != 0) {
                rule.effects.sid = selector.payload;
            } else {
                did_parse = false;
            }
        } else if (keyEqual(k, "grid")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            if (sscanf(v, "%d:%d:%d:%d:%d:%d", &rule.effects.grid[0], &rule.effects.grid[1], &rule.effects.grid[2], &rule.effects.grid[3], &rule.effects.grid[4], &rule.effects.grid[5]) != 6) {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "opacity")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            if (sscanf(v, "%f", &rule.effects.opacity) == 1 and yabai_in_range_ii_f(rule.effects.opacity, 0.0, 1.0)) {
                setFlag(&rule.effects, RULE_OPACITY);
            } else {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "manage")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            if (keyEqual(v, "on")) {
                rule.effects.manage = RULE_PROP_ON;
            } else if (keyEqual(v, "off")) {
                rule.effects.manage = RULE_PROP_OFF;
            } else {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "sticky")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            if (keyEqual(v, "on")) {
                rule.effects.sticky = RULE_PROP_ON;
            } else if (keyEqual(v, "off")) {
                rule.effects.sticky = RULE_PROP_OFF;
            } else {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "mouse_follows_focus")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            if (keyEqual(v, "on")) {
                rule.effects.mff = RULE_PROP_ON;
            } else if (keyEqual(v, "off")) {
                rule.effects.mff = RULE_PROP_OFF;
            } else {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "sub-layer")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            if (keyEqual(v, "below")) {
                rule.effects.layer = LAYER_BELOW;
                setFlag(&rule.effects, RULE_LAYER);
            } else if (keyEqual(v, "normal")) {
                rule.effects.layer = LAYER_NORMAL;
                setFlag(&rule.effects, RULE_LAYER);
            } else if (keyEqual(v, "above")) {
                rule.effects.layer = LAYER_ABOVE;
                setFlag(&rule.effects, RULE_LAYER);
            } else if (keyEqual(v, "auto")) {
                rule.effects.layer = LAYER_AUTO;
                setFlag(&rule.effects, RULE_LAYER);
            } else {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "native-fullscreen")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            if (keyEqual(v, "on")) {
                rule.effects.fullscreen = RULE_PROP_ON;
            } else if (keyEqual(v, "off")) {
                rule.effects.fullscreen = RULE_PROP_OFF;
            } else {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else {
            failUnknownKey(rsp, k);
            did_parse = false;
        }
    }

    if (!has_filter) {
        failMissingFilter(rsp);
        did_parse = false;
    }

    if (unsupported_exclusion) |uk| {
        failUnsupportedExclusion(rsp, @ptrCast(uk));
        did_parse = false;
    }

    return did_parse;
}

// Mirrors rule_destroy() in src/rule.c exactly: guarded regfree() on
// the four opaque regex fields (same VALID-flag guards as C; libc
// regfree takes the storage address), then the six heap-string frees
// with identical NULL handling.
export fn yabai_rule_destroy(rule: *Rule) void {
    @setRuntimeSafety(false);
    if (yabai_rule_check_flag(rule, RULE_APP_VALID)) regfree(@ptrCast(&rule.app_regex));
    if (yabai_rule_check_flag(rule, RULE_TITLE_VALID)) regfree(@ptrCast(&rule.title_regex));
    if (yabai_rule_check_flag(rule, RULE_ROLE_VALID)) regfree(@ptrCast(&rule.role_regex));
    if (yabai_rule_check_flag(rule, RULE_SUBROLE_VALID)) regfree(@ptrCast(&rule.subrole_regex));
    if (rule.label) |p| free(p);
    if (rule.app) |p| free(p);
    if (rule.title) |p| free(p);
    if (rule.role) |p| free(p);
    if (rule.subrole) |p| free(p);
    if (rule.effects.scratchpad) |p| free(p);
}

// Mirrors REGEX_MATCH_* in src/misc/macros.h.
pub const REGEX_MATCH_UD: c_int = 0;
pub const REGEX_MATCH_YES: c_int = 1;
pub const REGEX_MATCH_NO: c_int = 2;

comptime {
    if (REGEX_MATCH_UD != 0) @compileError("REGEX_MATCH_UD drift");
    if (REGEX_MATCH_YES != 1) @compileError("REGEX_MATCH_YES drift");
    if (REGEX_MATCH_NO != 2) @compileError("REGEX_MATCH_NO drift");
}

extern "c" fn regexec(regex: *const anyopaque, string: [*:0]const u8, nmatch: usize, pmatch: ?*anyopaque, eflags: c_int) c_int;

// Byte-identical port of regex_match() in src/misc/helpers.h: without
// VALID the field is undecided (UD, string untouched); otherwise the
// libc regexec result maps 0 to YES and anything else to NO. C passes
// the string straight into regexec, which dereferences it, so NULL
// with VALID set is a segfault in C; the daemon never produces NULL
// here (the title/role/subrole accessors fall back to ""), and the
// guard below only defines that unreachable case as NO.
fn regexMatch(valid: bool, regex: *const RegexT, match: ?[*:0]const u8) c_int {
    @setRuntimeSafety(false);
    if (!valid) return REGEX_MATCH_UD;
    const m = match orelse return REGEX_MATCH_NO;
    const rc = regexec(@ptrCast(regex), m, 0, null, 0);
    return if (rc == 0) REGEX_MATCH_YES else REGEX_MATCH_NO;
}

export fn yabai_regex_match(valid: bool, regex: *const RegexT, match: ?[*:0]const u8) c_int {
    @setRuntimeSafety(false);
    return regexMatch(valid, regex, match);
}

// Byte-identical port of window_manager_rule_matches_window() in
// src/window_manager.c. The window's application name is lifted in as
// a plain string (the only window touch in C is
// window->application->name), so no window struct crosses the
// boundary. Each of the four fields short-circuits to false when its
// match result equals the EXCLUDE-flipped expectation; VALID-clear
// fields take the UD path and never filter.
export fn yabai_rule_matches_window(rule: *const Rule, app_name: ?[*:0]const u8, window_title: ?[*:0]const u8, window_role: ?[*:0]const u8, window_subrole: ?[*:0]const u8) bool {
    @setRuntimeSafety(false);
    const expect_app: c_int = if (yabai_rule_check_flag(rule, RULE_APP_EXCLUDE)) REGEX_MATCH_YES else REGEX_MATCH_NO;
    if (regexMatch(yabai_rule_check_flag(rule, RULE_APP_VALID), &rule.app_regex, app_name) == expect_app) return false;

    const expect_title: c_int = if (yabai_rule_check_flag(rule, RULE_TITLE_EXCLUDE)) REGEX_MATCH_YES else REGEX_MATCH_NO;
    if (regexMatch(yabai_rule_check_flag(rule, RULE_TITLE_VALID), &rule.title_regex, window_title) == expect_title) return false;

    const expect_role: c_int = if (yabai_rule_check_flag(rule, RULE_ROLE_EXCLUDE)) REGEX_MATCH_YES else REGEX_MATCH_NO;
    if (regexMatch(yabai_rule_check_flag(rule, RULE_ROLE_VALID), &rule.role_regex, window_role) == expect_role) return false;

    const expect_subrole: c_int = if (yabai_rule_check_flag(rule, RULE_SUBROLE_EXCLUDE)) REGEX_MATCH_YES else REGEX_MATCH_NO;
    if (regexMatch(yabai_rule_check_flag(rule, RULE_SUBROLE_VALID), &rule.subrole_regex, window_subrole) == expect_subrole) return false;

    return true;
}
