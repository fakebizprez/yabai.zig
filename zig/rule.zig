// Rule boundary (Zig version: zig/ZIG_VERSION).
//
// Mirrors rule_combine_effects(),
// rule_serialize() and rule_destroy() in src/rule.c plus the rule flag
// helpers in src/rule.h and ts_string_escape() in
// src/misc/helpers.h. The RuleEffects/Rule mirrors (regex_t
// fields opaque, released via libc regfree under the same VALID-flag
// guards as C) live in zig/abi.zig, pinned by src/zig_abi.h. Heap
// ownership uses libc directly, matching string_copy()/free() in C.

const abi = @import("abi.zig");
const RuleEffects = abi.RuleEffects;
const RegexT = abi.RegexT;
const Rule = abi.Rule;
const MsgToken = abi.MsgToken;
const TokenValue = abi.TokenValue;
const Selector = abi.Selector;
const DisplayManagerState = abi.DisplayManagerState;
const SpaceManagerState = abi.SpaceManagerState;
const DisplayLabel = abi.DisplayLabel;
const SpaceLabel = abi.SpaceLabel;

extern "c" fn malloc(size: usize) ?[*]u8;
extern "c" fn free(ptr: ?[*]u8) void;
extern "c" fn regfree(regex: *anyopaque) void;
// Out-of-memory crashes here the way C did; @panic would need a Zig runtime.
extern "c" fn abort() noreturn;

fn stringCopy(s: [*:0]const u8) [*:0]u8 {
    @setRuntimeSafety(false);
    var len: usize = 0;
    while (s[len] != 0) : (len += 1) {}
    const mem = malloc(len + 1) orelse abort();
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
        if (hasFlag(effects, abi.RULE_FOLLOW_SPACE)) {
            setFlag(result, abi.RULE_FOLLOW_SPACE);
        } else {
            clearFlag(result, abi.RULE_FOLLOW_SPACE);
        }
    }

    if (effects.sid != 0) {
        result.sid = effects.sid;
        if (hasFlag(effects, abi.RULE_FOLLOW_SPACE)) {
            setFlag(result, abi.RULE_FOLLOW_SPACE);
        } else {
            clearFlag(result, abi.RULE_FOLLOW_SPACE);
        }
    }

    if (hasFlag(effects, abi.RULE_OPACITY) and abi.yabai_in_range_ii_f(effects.opacity, 0.0, 1.0)) {
        result.opacity = effects.opacity;
        setFlag(result, abi.RULE_OPACITY);
    }

    if (hasFlag(effects, abi.RULE_LAYER)) {
        result.layer = effects.layer;
        setFlag(result, abi.RULE_LAYER);
    }

    if (effects.scratchpad) |pad| {
        // Scratchpad strings are always NUL-terminated in C.
        const copy = stringCopy(@ptrCast(pad));
        if (result.scratchpad) |old| {
            free(old);
        }
        result.scratchpad = copy;
    }

    if (effects.manage != abi.RULE_PROP_UD) result.manage = effects.manage;
    if (effects.sticky != abi.RULE_PROP_UD) result.sticky = effects.sticky;
    if (effects.mff != abi.RULE_PROP_UD) result.mff = effects.mff;
    if (effects.fullscreen != abi.RULE_PROP_UD) result.fullscreen = effects.fullscreen;

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
    // sscanf("%d") can store negatives into these unsigned slots; C prints
    // them back with %d, so reinterpret the bits rather than range-cast.
    const g0: c_int = @bitCast(grid[0]);
    const g1: c_int = @bitCast(grid[1]);
    const g2: c_int = @bitCast(grid[2]);
    const g3: c_int = @bitCast(grid[3]);
    const g4: c_int = @bitCast(grid[4]);
    const g5: c_int = @bitCast(grid[5]);
    const n = snprintf(@ptrCast(&tmp), tmp.len, "%d:%d:%d:%d:%d:%d", g0, g1, g2, g3, g4, g5);
    if (n <= 0) return;
    _ = fwrite(@ptrCast(&tmp), 1, @intCast(n), stream);
}

fn layerStr(layer: c_int) [*:0]const u8 {
    @setRuntimeSafety(false);
    if (layer == abi.LAYER_AUTO) return "auto";
    if (layer == abi.LAYER_BELOW) return "below";
    if (layer == abi.LAYER_NORMAL) return "normal";
    if (layer == abi.LAYER_ABOVE) return "above";
    return "";
}

// Byte-identical port of ts_string_escape() in src/misc/helpers.h.
// ts_alloc_unaligned() is a temp-storage bump allocator, not plain
// malloc; the Zig port uses libc malloc for equivalent ownership here
// (the serialize port frees these buffers, keeping the suite leak-clean)
// and returns NULL when no replacements are needed, exactly like C. NULL
// therefore always means "already clean"; allocation failure aborts.
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
    const mem = malloc(total + 1) orelse abort();
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
    const follow_out = abi.yabai_json_bool((rule.effects.flags & abi.RULE_FOLLOW_SPACE) != 0);
    const manage_out = abi.yabai_json_optional_bool(rule.effects.manage);
    const sticky_out = abi.yabai_json_optional_bool(rule.effects.sticky);
    const mff_out = abi.yabai_json_optional_bool(rule.effects.mff);
    const layer_out: [*:0]const u8 = if ((rule.effects.flags & abi.RULE_LAYER) != 0) layerStr(rule.effects.layer) else "";
    const fullscreen_out = abi.yabai_json_optional_bool(rule.effects.fullscreen);
    const oneshot_out = abi.yabai_json_bool((rule.flags & abi.RULE_ONE_SHOT) != 0);
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
// The display/space selector paths use the abi.zig token, token_value
// and Selector mirrors; the Selector union payload is zeroed where C leaves it implicit (same convention
// as yabai_parse_insert_selector). Tokenizer, key/value split, float
// range check, and flag helpers are reused via extern calls into the
// already-ported message/util objects; string_copy/regcomp/sscanf come
// from libc exactly like C. All daemon_fail emissions use the established
// fwrite pattern (0x07 marker + literal bytes), so no varargs ABI is
// involved. Manager queries for the display=/space= keys come through
// abi.RuleLookup; the acting getters are called only inside those
// branches, at the point upstream parse_rule evaluated them, so other
// rules never touch AX/SkyLight. Tests pass counting stubs.
extern "c" fn sscanf(s: [*:0]const u8, fmt: [*:0]const u8, ...) c_int;
extern "c" fn regcomp(regex: *anyopaque, pattern: [*:0]const u8, cflags: c_int) c_int;

extern fn display_manager_arrangement_display_id(arrangement: c_int) u32;
extern fn display_manager_first_display_id() u32;
extern fn display_manager_last_display_id() u32;
extern fn display_manager_cursor_display_id() u32;
extern fn display_manager_get_display_for_label(dm: *DisplayManagerState, label: [*:0]u8) ?*DisplayLabel;
extern fn space_manager_mission_control_space(desktop_id: c_int) u64;
extern fn space_manager_cursor_space() u64;
extern fn space_manager_first_space() u64;
extern fn space_manager_last_space() u64;
extern fn space_manager_get_space_for_label(sm: *SpaceManagerState, label: [*:0]u8) ?*SpaceLabel;
extern var g_display_manager: DisplayManagerState;
extern var g_space_manager: SpaceManagerState;

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
// manager query keeps its argument order (relative lookups go through
// abi.RuleLookup, the rest are extern calls).
fn parseDisplaySelector(rsp: ?*anyopaque, message: *[*:0]u8, acting_did: u32, lookup: *const abi.RuleLookup, out: *Selector) void {
    @setRuntimeSafety(false);
    abi.yabai_message_get_token(message, &out.token);
    out.did_parse = true;
    out.payload = 0;
    var value: TokenValue = .{ .token = undefined, .type = abi.TOKEN_TYPE_INVALID, .u = .{ .int_value = 0 } };
    abi.yabai_token_to_value(&out.token, &value);
    if (value.type == abi.TOKEN_TYPE_INT) {
        const did = display_manager_arrangement_display_id(value.u.int_value);
        if (did != 0) {
            out.payload = did;
        } else {
            failDisplayArrangement(rsp, value.u.int_value);
        }
    } else if (value.type == abi.TOKEN_TYPE_STRING) {
        if (abi.yabai_token_equals(&out.token, "north")) {
            if (acting_did != 0) {
                const did = lookup.display_in_direction(acting_did, abi.DIR_NORTH);
                if (did != 0) {
                    out.payload = did;
                } else {
                    failFixed(rsp, "could not locate a northward display.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected display.\n");
            }
        } else if (abi.yabai_token_equals(&out.token, "east")) {
            if (acting_did != 0) {
                const did = lookup.display_in_direction(acting_did, abi.DIR_EAST);
                if (did != 0) {
                    out.payload = did;
                } else {
                    failFixed(rsp, "could not locate a eastward display.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected display.\n");
            }
        } else if (abi.yabai_token_equals(&out.token, "south")) {
            if (acting_did != 0) {
                const did = lookup.display_in_direction(acting_did, abi.DIR_SOUTH);
                if (did != 0) {
                    out.payload = did;
                } else {
                    failFixed(rsp, "could not locate a southward display.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected display.\n");
            }
        } else if (abi.yabai_token_equals(&out.token, "west")) {
            if (acting_did != 0) {
                const did = lookup.display_in_direction(acting_did, abi.DIR_WEST);
                if (did != 0) {
                    out.payload = did;
                } else {
                    failFixed(rsp, "could not locate a westward display.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected display.\n");
            }
        } else if (abi.yabai_token_equals(&out.token, "prev")) {
            if (acting_did != 0) {
                const did = lookup.prev_display_id(acting_did);
                if (did != 0) {
                    out.payload = did;
                } else {
                    failFixed(rsp, "could not locate the previous display.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected display.\n");
            }
        } else if (abi.yabai_token_equals(&out.token, "next")) {
            if (acting_did != 0) {
                const did = lookup.next_display_id(acting_did);
                if (did != 0) {
                    out.payload = did;
                } else {
                    failFixed(rsp, "could not locate the next display.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected display.\n");
            }
        } else if (abi.yabai_token_equals(&out.token, "first")) {
            const did = display_manager_first_display_id();
            if (did != 0) {
                out.payload = did;
            } else {
                failFixed(rsp, "could not locate the first display.\n");
            }
        } else if (abi.yabai_token_equals(&out.token, "last")) {
            const did = display_manager_last_display_id();
            if (did != 0) {
                out.payload = did;
            } else {
                failFixed(rsp, "could not locate the last display.\n");
            }
        } else if (abi.yabai_token_equals(&out.token, "recent")) {
            out.payload = g_display_manager.last_display_id;
        } else if (abi.yabai_token_equals(&out.token, "mouse")) {
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
    } else if (value.type == abi.TOKEN_TYPE_INVALID) {
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
fn parseSpaceSelector(rsp: ?*anyopaque, message: *[*:0]u8, acting_sid: u64, lookup: *const abi.RuleLookup, out: *Selector) void {
    @setRuntimeSafety(false);
    abi.yabai_message_get_token(message, &out.token);
    out.did_parse = true;
    out.payload = 0;
    var value: TokenValue = .{ .token = undefined, .type = abi.TOKEN_TYPE_INVALID, .u = .{ .int_value = 0 } };
    abi.yabai_token_to_value(&out.token, &value);
    if (value.type == abi.TOKEN_TYPE_INT) {
        const sid = space_manager_mission_control_space(value.u.int_value);
        if (sid != 0) {
            out.payload = sid;
        } else {
            failSpaceMissionControl(rsp, value.u.int_value);
        }
    } else if (value.type == abi.TOKEN_TYPE_STRING) {
        if (abi.yabai_token_equals(&out.token, "prev")) {
            if (acting_sid != 0) {
                const sid = lookup.prev_space(acting_sid);
                if (sid != 0) {
                    out.payload = sid;
                } else {
                    failFixed(rsp, "could not locate the previous space.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected space.\n");
            }
        } else if (abi.yabai_token_equals(&out.token, "next")) {
            if (acting_sid != 0) {
                const sid = lookup.next_space(acting_sid);
                if (sid != 0) {
                    out.payload = sid;
                } else {
                    failFixed(rsp, "could not locate the next space.\n");
                }
            } else {
                failFixed(rsp, "could not locate the selected space.\n");
            }
        } else if (abi.yabai_token_equals(&out.token, "first")) {
            const sid = space_manager_first_space();
            if (sid != 0) {
                out.payload = sid;
            } else {
                failFixed(rsp, "could not locate the first space.\n");
            }
        } else if (abi.yabai_token_equals(&out.token, "last")) {
            const sid = space_manager_last_space();
            if (sid != 0) {
                out.payload = sid;
            } else {
                failFixed(rsp, "could not locate the last space.\n");
            }
        } else if (abi.yabai_token_equals(&out.token, "recent")) {
            out.payload = g_space_manager.last_space_id;
        } else if (abi.yabai_token_equals(&out.token, "mouse")) {
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
    } else if (value.type == abi.TOKEN_TYPE_INVALID) {
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
export fn yabai_parse_rule(rsp: ?*anyopaque, message: *[*:0]u8, rule: *Rule, token: *const MsgToken, lookup: *const abi.RuleLookup) bool {
    @setRuntimeSafety(false);
    var unsupported_exclusion: ?[*]u8 = null;
    var did_parse = true;
    var has_filter = false;

    var tok = token.*;
    while (abi.yabai_token_is_valid(&tok)) : (abi.yabai_message_get_token(message, &tok)) {
        var key: ?[*]u8 = null;
        var val: ?[*]u8 = null;
        var exclusion = false;
        abi.yabai_parse_key_value_pair(@ptrCast(tok.text orelse unreachable), &key, &val, &exclusion);

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
                rule.effects.manage = abi.RULE_PROP_OFF;
            } else {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "app")) {
            has_filter = true;
            rule.app = @ptrCast(stringCopy(v));
            if (exclusion) yabai_rule_set_flag(rule, abi.RULE_APP_EXCLUDE);
            if (regcomp(@ptrCast(&rule.app_regex), v, abi.REG_EXTENDED) == 0) {
                yabai_rule_set_flag(rule, abi.RULE_APP_VALID);
            } else {
                failInvalidRegex(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "title")) {
            has_filter = true;
            rule.title = @ptrCast(stringCopy(v));
            if (exclusion) yabai_rule_set_flag(rule, abi.RULE_TITLE_EXCLUDE);
            if (regcomp(@ptrCast(&rule.title_regex), v, abi.REG_EXTENDED) == 0) {
                yabai_rule_set_flag(rule, abi.RULE_TITLE_VALID);
            } else {
                failInvalidRegex(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "role")) {
            has_filter = true;
            rule.role = @ptrCast(stringCopy(v));
            if (exclusion) yabai_rule_set_flag(rule, abi.RULE_ROLE_EXCLUDE);
            if (regcomp(@ptrCast(&rule.role_regex), v, abi.REG_EXTENDED) == 0) {
                yabai_rule_set_flag(rule, abi.RULE_ROLE_VALID);
            } else {
                failInvalidRegex(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "subrole")) {
            has_filter = true;
            rule.subrole = @ptrCast(stringCopy(v));
            if (exclusion) yabai_rule_set_flag(rule, abi.RULE_SUBROLE_EXCLUDE);
            if (regcomp(@ptrCast(&rule.subrole_regex), v, abi.REG_EXTENDED) == 0) {
                yabai_rule_set_flag(rule, abi.RULE_SUBROLE_VALID);
            } else {
                failInvalidRegex(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "display")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            var cursor: [*:0]u8 = v;
            if (cursor[0] == '^') {
                cursor += 1;
                setFlag(&rule.effects, abi.RULE_FOLLOW_SPACE);
            }

            var selector: Selector = undefined;
            parseDisplaySelector(rsp, &cursor, lookup.active_display_id(), lookup, &selector);
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
                setFlag(&rule.effects, abi.RULE_FOLLOW_SPACE);
            }

            var selector: Selector = undefined;
            parseSpaceSelector(rsp, &cursor, lookup.active_space(), lookup, &selector);
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

            if (sscanf(v, "%f", &rule.effects.opacity) == 1 and abi.yabai_in_range_ii_f(rule.effects.opacity, 0.0, 1.0)) {
                setFlag(&rule.effects, abi.RULE_OPACITY);
            } else {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "manage")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            if (keyEqual(v, "on")) {
                rule.effects.manage = abi.RULE_PROP_ON;
            } else if (keyEqual(v, "off")) {
                rule.effects.manage = abi.RULE_PROP_OFF;
            } else {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "sticky")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            if (keyEqual(v, "on")) {
                rule.effects.sticky = abi.RULE_PROP_ON;
            } else if (keyEqual(v, "off")) {
                rule.effects.sticky = abi.RULE_PROP_OFF;
            } else {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "mouse_follows_focus")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            if (keyEqual(v, "on")) {
                rule.effects.mff = abi.RULE_PROP_ON;
            } else if (keyEqual(v, "off")) {
                rule.effects.mff = abi.RULE_PROP_OFF;
            } else {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "sub-layer")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            if (keyEqual(v, "below")) {
                rule.effects.layer = abi.LAYER_BELOW;
                setFlag(&rule.effects, abi.RULE_LAYER);
            } else if (keyEqual(v, "normal")) {
                rule.effects.layer = abi.LAYER_NORMAL;
                setFlag(&rule.effects, abi.RULE_LAYER);
            } else if (keyEqual(v, "above")) {
                rule.effects.layer = abi.LAYER_ABOVE;
                setFlag(&rule.effects, abi.RULE_LAYER);
            } else if (keyEqual(v, "auto")) {
                rule.effects.layer = abi.LAYER_AUTO;
                setFlag(&rule.effects, abi.RULE_LAYER);
            } else {
                failInvalidValue(rsp, v, k);
                did_parse = false;
            }
        } else if (keyEqual(k, "native-fullscreen")) {
            if (exclusion) unsupported_exclusion = @ptrCast(k);

            if (keyEqual(v, "on")) {
                rule.effects.fullscreen = abi.RULE_PROP_ON;
            } else if (keyEqual(v, "off")) {
                rule.effects.fullscreen = abi.RULE_PROP_OFF;
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
    if (yabai_rule_check_flag(rule, abi.RULE_APP_VALID)) regfree(@ptrCast(&rule.app_regex));
    if (yabai_rule_check_flag(rule, abi.RULE_TITLE_VALID)) regfree(@ptrCast(&rule.title_regex));
    if (yabai_rule_check_flag(rule, abi.RULE_ROLE_VALID)) regfree(@ptrCast(&rule.role_regex));
    if (yabai_rule_check_flag(rule, abi.RULE_SUBROLE_VALID)) regfree(@ptrCast(&rule.subrole_regex));
    if (rule.label) |p| free(p);
    if (rule.app) |p| free(p);
    if (rule.title) |p| free(p);
    if (rule.role) |p| free(p);
    if (rule.subrole) |p| free(p);
    if (rule.effects.scratchpad) |p| free(p);
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
    if (!valid) return abi.REGEX_MATCH_UD;
    const m = match orelse return abi.REGEX_MATCH_NO;
    const rc = regexec(@ptrCast(regex), m, 0, null, 0);
    return if (rc == 0) abi.REGEX_MATCH_YES else abi.REGEX_MATCH_NO;
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
    const expect_app: c_int = if (yabai_rule_check_flag(rule, abi.RULE_APP_EXCLUDE)) abi.REGEX_MATCH_YES else abi.REGEX_MATCH_NO;
    if (regexMatch(yabai_rule_check_flag(rule, abi.RULE_APP_VALID), &rule.app_regex, app_name) == expect_app) return false;

    const expect_title: c_int = if (yabai_rule_check_flag(rule, abi.RULE_TITLE_EXCLUDE)) abi.REGEX_MATCH_YES else abi.REGEX_MATCH_NO;
    if (regexMatch(yabai_rule_check_flag(rule, abi.RULE_TITLE_VALID), &rule.title_regex, window_title) == expect_title) return false;

    const expect_role: c_int = if (yabai_rule_check_flag(rule, abi.RULE_ROLE_EXCLUDE)) abi.REGEX_MATCH_YES else abi.REGEX_MATCH_NO;
    if (regexMatch(yabai_rule_check_flag(rule, abi.RULE_ROLE_VALID), &rule.role_regex, window_role) == expect_role) return false;

    const expect_subrole: c_int = if (yabai_rule_check_flag(rule, abi.RULE_SUBROLE_EXCLUDE)) abi.REGEX_MATCH_YES else abi.REGEX_MATCH_NO;
    if (regexMatch(yabai_rule_check_flag(rule, abi.RULE_SUBROLE_VALID), &rule.subrole_regex, window_subrole) == expect_subrole) return false;

    return true;
}
