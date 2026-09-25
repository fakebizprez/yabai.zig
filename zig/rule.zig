// Rule boundary for the yabai migration (step 3).
//
// Pinned toolchain: Zig 0.16.0. Mirrors rule_combine_effects(),
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
