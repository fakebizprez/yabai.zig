// Rule boundary for the yabai migration (step 3).
//
// Pinned toolchain: Zig 0.16.0. Mirrors rule_combine_effects() and
// rule_destroy() in src/rule.c plus the rule flag helpers in
// src/rule.h. RuleEffects mirrors struct rule_effects in src/rule.h
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
