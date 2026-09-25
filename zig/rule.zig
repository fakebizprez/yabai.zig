// Rule-effects boundary for the yabai migration (step 3).
//
// Pinned toolchain: Zig 0.16.0. Mirrors rule_combine_effects() in
// src/rule.c. RuleEffects mirrors struct rule_effects in src/rule.h
// (80 bytes; offsets asserted here and in tests/src/zig_rule.c).
// Scratchpad ownership (free old, malloc new) uses libc directly,
// matching string_copy()/free() in C.

pub const RULE_PROP_UD: c_int = 0;

pub const RULE_FOLLOW_SPACE: u16 = 0x01;
pub const RULE_OPACITY: u16 = 0x02;
pub const RULE_LAYER: u16 = 0x04;

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

extern "c" fn malloc(size: usize) ?[*]u8;
extern "c" fn free(ptr: ?[*]u8) void;
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
