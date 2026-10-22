// C ABI mirror shared by every Zig boundary module (Zig version: zig/ZIG_VERSION).
//
// Types, constants and extern fn declarations only: no exports, so
// importing this file from each separately built object adds no
// symbols. What is enforced, on both architectures:
//  - Layout: pin() below fixes each mirror's size, field count, and per
//    field offset, size and float-ness. src/zig_abi.h pins the C structs
//    with the same literals; nothing compares the two lists, so a change
//    to one must be made to the other by hand.
//  - Values: every `pub const` here that is not a type must read exactly
//    `pub const NAME[: T] = literal;`, and zig/build.sh requires a C value
//    pin with the same name and literal in src/zig_abi.h (and the reverse).
//  - Exports: zig/build.sh checks every `export fn yabai_*` in zig/
//    against its src/zig_abi.h prototype (see that file's header).

const std = @import("std");
const builtin = @import("builtin");

// Selector stores the C selector union as one u64 payload and reads the
// int/uint32_t members back from its low bits, which is only correct on
// little-endian targets (both shipped slices, x86_64 and arm64, are).
comptime {
    std.debug.assert(builtin.cpu.arch.endian() == .little);
}

// ---------------------------------------------------------------------
// Geometry (src/view.h, CoreGraphics)
// ---------------------------------------------------------------------

pub const Area = extern struct {
    x: f32,
    y: f32,
    w: f32,
    h: f32,
};

// CGPoint/CGRect on 64-bit macOS (CGFloat is double).
pub const CgPoint = extern struct {
    x: f64,
    y: f64,
};

pub const CgRect = extern struct {
    x: f64,
    y: f64,
    w: f64,
    h: f64,
};

// ---------------------------------------------------------------------
// View tree (src/view.h)
// ---------------------------------------------------------------------

pub const NODE_MAX_WINDOW_COUNT: usize = 32;

pub const FeedbackWindow = extern struct {
    id: u32,
    context: ?*anyopaque,
};

pub const WindowNode = extern struct {
    area: Area,
    parent: ?*WindowNode,
    left: ?*WindowNode,
    right: ?*WindowNode,
    zoom: ?*WindowNode,
    window_list: [NODE_MAX_WINDOW_COUNT]u32,
    window_order: [NODE_MAX_WINDOW_COUNT]u32,
    window_count: c_int,
    ratio: f32,
    split: c_int,
    child: c_int,
    insert_dir: c_int,
    feedback_window: FeedbackWindow,
};

pub const View = extern struct {
    uuid: ?*anyopaque,
    sid: u64,
    root: ?*WindowNode,
    insertion_point: u32,
    layout: c_int,
    split_type: c_int,
    top_padding: c_int,
    bottom_padding: c_int,
    left_padding: c_int,
    right_padding: c_int,
    window_gap: c_int,
    auto_balance: u32,
    flags: u64,
};

// struct yabai_view_defaults in src/zig_abi.h: the g_space_manager block
// yabai_view_create copies in the user branch, in C assignment order.
pub const ViewDefaults = extern struct {
    layout: c_int,
    top_padding: c_int,
    bottom_padding: c_int,
    left_padding: c_int,
    right_padding: c_int,
    window_gap: c_int,
    auto_balance: u32,
    split_type: c_int,
};

// struct yabai_rule_lookup in src/zig_abi.h: manager queries for the
// display=/space= rule keys, called lazily from yabai_parse_rule.
pub const RuleLookup = extern struct {
    active_display_id: *const fn () callconv(.c) u32,
    active_space: *const fn () callconv(.c) u64,
    display_in_direction: *const fn (did: u32, direction: c_int) callconv(.c) u32,
    prev_display_id: *const fn (did: u32) callconv(.c) u32,
    next_display_id: *const fn (did: u32) callconv(.c) u32,
    prev_space: *const fn (sid: u64) callconv(.c) u64,
    next_space: *const fn (sid: u64) callconv(.c) u64,
};

pub const BalanceNode = extern struct {
    y_count: c_int,
    x_count: c_int,
};

pub const SPLIT_NONE: c_int = 0;
pub const SPLIT_Y: c_int = 1;
pub const SPLIT_X: c_int = 2;
pub const SPLIT_AUTO: c_int = 3;

pub const CHILD_NONE: c_int = 0;
pub const CHILD_SECOND: c_int = 1;
pub const CHILD_FIRST: c_int = 2;

pub const VIEW_DEFAULT: c_int = 0;
pub const VIEW_BSP: c_int = 1;
pub const VIEW_STACK: c_int = 2;
pub const VIEW_FLOAT: c_int = 3;

pub const VIEW_ENABLE_PADDING: u64 = 0x080;
pub const VIEW_ENABLE_GAP: u64 = 0x100;
pub const VIEW_IS_VALID: u64 = 0x200;
pub const VIEW_IS_DIRTY: u64 = 0x400;

pub const SPACE_PROPERTY_ID: u64 = 0x001;
pub const SPACE_PROPERTY_UUID: u64 = 0x002;
pub const SPACE_PROPERTY_INDEX: u64 = 0x004;
pub const SPACE_PROPERTY_LABEL: u64 = 0x008;
pub const SPACE_PROPERTY_TYPE: u64 = 0x010;
pub const SPACE_PROPERTY_DISPLAY: u64 = 0x020;
pub const SPACE_PROPERTY_WINDOWS: u64 = 0x040;
pub const SPACE_PROPERTY_FIRST_WINDOW: u64 = 0x080;
pub const SPACE_PROPERTY_LAST_WINDOW: u64 = 0x100;
pub const SPACE_PROPERTY_HAS_FOCUS: u64 = 0x200;
pub const SPACE_PROPERTY_IS_VISIBLE: u64 = 0x400;
pub const SPACE_PROPERTY_IS_FULLSCREEN: u64 = 0x800;

// ---------------------------------------------------------------------
// src/misc/macros.h, src/misc/helpers.h
// ---------------------------------------------------------------------

pub const DIR_NORTH: c_int = 360;
pub const DIR_EAST: c_int = 90;
pub const DIR_SOUTH: c_int = 180;
pub const DIR_WEST: c_int = 270;
pub const STACK: c_int = 111;

pub const TYPE_ABS: c_int = 0x1;
pub const TYPE_REL: c_int = 0x2;

pub const HANDLE_TOP: c_int = 0x01;
pub const HANDLE_BOTTOM: c_int = 0x02;
pub const HANDLE_LEFT: c_int = 0x04;
pub const HANDLE_RIGHT: c_int = 0x08;
pub const HANDLE_ABS: c_int = 0x10;

// CGWindowLevelKey values behind LAYER_BELOW/NORMAL/ABOVE.
pub const LAYER_AUTO: c_int = 0;
pub const LAYER_BELOW: c_int = 3;
pub const LAYER_NORMAL: c_int = 4;
pub const LAYER_ABOVE: c_int = 5;

pub const REGEX_MATCH_UD: c_int = 0;
pub const REGEX_MATCH_YES: c_int = 1;
pub const REGEX_MATCH_NO: c_int = 2;

pub const RgbaColor = extern struct {
    p: u32,
    r: f32,
    g: f32,
    b: f32,
    a: f32,
};

// ---------------------------------------------------------------------
// Window and display (src/window.h, src/display.h, src/display_manager.h)
// ---------------------------------------------------------------------

pub const WINDOW_SHADOW: u8 = 0x01;
pub const WINDOW_FULLSCREEN: u8 = 0x02;
pub const WINDOW_MINIMIZE: u8 = 0x04;
pub const WINDOW_FLOAT: u8 = 0x08;
pub const WINDOW_STICKY: u8 = 0x10;
pub const WINDOW_WINDOWED: u8 = 0x20;
pub const WINDOW_MOVABLE: u8 = 0x40;
pub const WINDOW_RESIZABLE: u8 = 0x80;

pub const DISPLAY_PROPERTY_ID: u64 = 0x01;
pub const DISPLAY_PROPERTY_UUID: u64 = 0x02;
pub const DISPLAY_PROPERTY_INDEX: u64 = 0x04;
pub const DISPLAY_PROPERTY_LABEL: u64 = 0x08;
pub const DISPLAY_PROPERTY_FRAME: u64 = 0x10;
pub const DISPLAY_PROPERTY_SPACES: u64 = 0x20;
pub const DISPLAY_PROPERTY_HAS_FOCUS: u64 = 0x40;

// Prefix of struct display_manager: only the two leading ids are read.
pub const DisplayManagerState = extern struct {
    current_display_id: u32,
    last_display_id: u32,
};

// Prefix of struct space_manager: the leading struct table (40 bytes,
// pinned by sizeof(struct table) in src/zig_abi.h) is opaque here.
pub const SpaceManagerState = extern struct {
    table: [40]u8,
    current_space_id: u64,
    last_space_id: u64,
};

pub const DisplayLabel = extern struct {
    did: u32,
    label: ?[*]u8,
};

pub const SpaceLabel = extern struct {
    sid: u64,
    label: ?[*]u8,
};

// ---------------------------------------------------------------------
// Mouse (src/mouse_handler.h, CGEventTypes.h)
// ---------------------------------------------------------------------

pub const MOUSE_MOD_NONE: u8 = 0x01;
pub const MOUSE_MOD_ALT: u8 = 0x02;
pub const MOUSE_MOD_SHIFT: u8 = 0x04;
pub const MOUSE_MOD_CMD: u8 = 0x08;
pub const MOUSE_MOD_CTRL: u8 = 0x10;
pub const MOUSE_MOD_FN: u8 = 0x20;

pub const MOUSE_DROP_ACTION_NONE: c_int = 0;
pub const MOUSE_DROP_ACTION_STACK: c_int = 1;
pub const MOUSE_DROP_ACTION_SWAP: c_int = 2;
pub const MOUSE_DROP_ACTION_WARP_TOP: c_int = 3;
pub const MOUSE_DROP_ACTION_WARP_RIGHT: c_int = 4;
pub const MOUSE_DROP_ACTION_WARP_BOTTOM: c_int = 5;
pub const MOUSE_DROP_ACTION_WARP_LEFT: c_int = 6;

pub const MOUSE_MODE_NONE: c_int = 0;
pub const MOUSE_MODE_MOVE: c_int = 1;
pub const MOUSE_MODE_RESIZE: c_int = 2;
pub const MOUSE_MODE_SWAP: c_int = 3;
pub const MOUSE_MODE_STACK: c_int = 4;

// kCGEventFlagMask* (NX_*MASK in IOLLEvent.h).
pub const CG_FLAG_MASK_SHIFT: u32 = 0x00020000;
pub const CG_FLAG_MASK_CONTROL: u32 = 0x00040000;
pub const CG_FLAG_MASK_ALTERNATE: u32 = 0x00080000;
pub const CG_FLAG_MASK_COMMAND: u32 = 0x00100000;
pub const CG_FLAG_MASK_SECONDARY_FN: u32 = 0x00800000;

// ---------------------------------------------------------------------
// Message tokens and selectors (types private to src/message.c)
// ---------------------------------------------------------------------

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

pub const Properties = extern struct {
    token: MsgToken,
    did_parse: bool,
    did_error: bool,
    flags: u64,
};

// struct selector's anonymous union { int dir; uint32_t did; uint64_t
// sid; struct window *window; } as one raw payload (see the endianness
// assert at the top).
pub const Selector = extern struct {
    token: MsgToken,
    did_parse: bool,
    payload: u64,
};

// struct frame_field in src/zig_abi.h.
pub const FrameField = extern struct {
    ptr: ?[*]u8,
    len: c_int,
};

pub const LABEL_DISPLAY: c_int = 0;
pub const LABEL_SPACE: c_int = 1;
pub const LABEL_WINDOW: c_int = 2;

// ---------------------------------------------------------------------
// Rules (src/rule.h, <regex.h>)
// ---------------------------------------------------------------------

pub const RULE_PROP_UD: c_int = 0;
pub const RULE_PROP_ON: c_int = 1;
pub const RULE_PROP_OFF: c_int = 2;

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

pub const REG_EXTENDED: c_int = 1;

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

// Opaque regex_t (32 bytes, 8-byte aligned on 64-bit macOS): filled by
// libc regcomp, read by regexec, released by regfree.
pub const RegexT = extern struct {
    _: [4]usize,
};

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

// ---------------------------------------------------------------------
// Layout pins. Same literals as src/zig_abi.h.
// ---------------------------------------------------------------------

/// Each entry is .{ name, offset, size } or .{ name, offset, size, .float };
/// a field is floating point exactly when its entry says .float, and the
/// entries must cover every field (a union's offsets are all 0).
fn pin(comptime T: type, comptime size: usize, comptime fields: anytype) void {
    if (@sizeOf(T) != size)
        @compileError(std.fmt.comptimePrint("{s}: size {d}, pinned {d}", .{ @typeName(T), @sizeOf(T), size }));
    const names = switch (@typeInfo(T)) {
        .@"struct" => |s| s.field_names,
        .@"union" => |u| u.field_names,
        else => @compileError(@typeName(T) ++ ": not a struct or union"),
    };
    if (names.len != fields.len)
        @compileError(@typeName(T) ++ ": field count changed; pin every field");
    inline for (fields) |f| {
        const F = @FieldType(T, f[0]);
        const off = if (@typeInfo(T) == .@"union") 0 else @offsetOf(T, f[0]);
        if (off != f[1])
            @compileError(std.fmt.comptimePrint("{s}.{s}: offset {d}, pinned {d}", .{ @typeName(T), f[0], off, f[1] }));
        if (@sizeOf(F) != f[2])
            @compileError(std.fmt.comptimePrint("{s}.{s}: size {d}, pinned {d}", .{ @typeName(T), f[0], @sizeOf(F), f[2] }));
        if ((@typeInfo(F) == .float) != (f.len > 3))
            @compileError(@typeName(T) ++ "." ++ f[0] ++ ": float-ness differs from its pin");
    }
}

comptime {
    pin(Area, 16, .{ .{ "x", 0, 4, .float }, .{ "y", 4, 4, .float }, .{ "w", 8, 4, .float }, .{ "h", 12, 4, .float } });
    pin(CgPoint, 16, .{ .{ "x", 0, 8, .float }, .{ "y", 8, 8, .float } });
    pin(CgRect, 32, .{ .{ "x", 0, 8, .float }, .{ "y", 8, 8, .float }, .{ "w", 16, 8, .float }, .{ "h", 24, 8, .float } });
    pin(FeedbackWindow, 16, .{ .{ "id", 0, 4 }, .{ "context", 8, 8 } });
    pin(WindowNode, 344, .{
        .{ "area", 0, 16 },              .{ "parent", 16, 8 },        .{ "left", 24, 8 },
        .{ "right", 32, 8 },             .{ "zoom", 40, 8 },          .{ "window_list", 48, 128 },
        .{ "window_order", 176, 128 },   .{ "window_count", 304, 4 }, .{ "ratio", 308, 4, .float },
        .{ "split", 312, 4 },            .{ "child", 316, 4 },        .{ "insert_dir", 320, 4 },
        .{ "feedback_window", 328, 16 },
    });
    pin(View, 72, .{
        .{ "uuid", 0, 8 },             .{ "sid", 8, 8 },             .{ "root", 16, 8 },
        .{ "insertion_point", 24, 4 }, .{ "layout", 28, 4 },         .{ "split_type", 32, 4 },
        .{ "top_padding", 36, 4 },     .{ "bottom_padding", 40, 4 }, .{ "left_padding", 44, 4 },
        .{ "right_padding", 48, 4 },   .{ "window_gap", 52, 4 },     .{ "auto_balance", 56, 4 },
        .{ "flags", 64, 8 },
    });
    pin(ViewDefaults, 32, .{
        .{ "layout", 0, 4 },        .{ "top_padding", 4, 4 },    .{ "bottom_padding", 8, 4 },
        .{ "left_padding", 12, 4 }, .{ "right_padding", 16, 4 }, .{ "window_gap", 20, 4 },
        .{ "auto_balance", 24, 4 }, .{ "split_type", 28, 4 },
    });
    pin(RuleLookup, 56, .{
        .{ "active_display_id", 0, 8 }, .{ "active_space", 8, 8 },     .{ "display_in_direction", 16, 8 },
        .{ "prev_display_id", 24, 8 },  .{ "next_display_id", 32, 8 }, .{ "prev_space", 40, 8 },
        .{ "next_space", 48, 8 },
    });
    pin(BalanceNode, 8, .{ .{ "y_count", 0, 4 }, .{ "x_count", 4, 4 } });
    pin(RgbaColor, 20, .{ .{ "p", 0, 4 }, .{ "r", 4, 4, .float }, .{ "g", 8, 4, .float }, .{ "b", 12, 4, .float }, .{ "a", 16, 4, .float } });
    pin(DisplayManagerState, 8, .{ .{ "current_display_id", 0, 4 }, .{ "last_display_id", 4, 4 } });
    pin(SpaceManagerState, 56, .{ .{ "table", 0, 40 }, .{ "current_space_id", 40, 8 }, .{ "last_space_id", 48, 8 } });
    pin(DisplayLabel, 16, .{ .{ "did", 0, 4 }, .{ "label", 8, 8 } });
    pin(SpaceLabel, 16, .{ .{ "sid", 0, 8 }, .{ "label", 8, 8 } });
    pin(MsgToken, 16, .{ .{ "text", 0, 8 }, .{ "length", 8, 4 } });
    pin(TokenValueUnion, 8, .{ .{ "int_value", 0, 4 }, .{ "float_value", 0, 4, .float }, .{ "u32_value", 0, 4 }, .{ "string_value", 0, 8 } });
    pin(TokenValue, 32, .{ .{ "token", 0, 16 }, .{ "type", 16, 4 }, .{ "u", 24, 8 } });
    pin(Properties, 32, .{ .{ "token", 0, 16 }, .{ "did_parse", 16, 1 }, .{ "did_error", 17, 1 }, .{ "flags", 24, 8 } });
    pin(Selector, 32, .{ .{ "token", 0, 16 }, .{ "did_parse", 16, 1 }, .{ "payload", 24, 8 } });
    pin(FrameField, 16, .{ .{ "ptr", 0, 8 }, .{ "len", 8, 4 } });
    pin(RuleEffects, 80, .{
        .{ "did", 0, 4 },         .{ "sid", 8, 8 },         .{ "opacity", 16, 4, .float },
        .{ "manage", 20, 4 },     .{ "sticky", 24, 4 },     .{ "mff", 28, 4 },
        .{ "layer", 32, 4 },      .{ "fullscreen", 36, 4 }, .{ "grid", 40, 24 },
        .{ "scratchpad", 64, 8 }, .{ "flags", 72, 2 },
    });
    pin(RegexT, 32, .{.{ "_", 0, 32 }});
    pin(Rule, 256, .{
        .{ "label", 0, 8 },         .{ "app", 8, 8 },           .{ "title", 16, 8 },
        .{ "role", 24, 8 },         .{ "subrole", 32, 8 },      .{ "app_regex", 40, 32 },
        .{ "title_regex", 72, 32 }, .{ "role_regex", 104, 32 }, .{ "subrole_regex", 136, 32 },
        .{ "effects", 168, 80 },    .{ "flags", 248, 2 },
    });
    if (@alignOf(RegexT) != 8) @compileError("RegexT: alignment drift (regex_t is 8-aligned)");
}

// ---------------------------------------------------------------------
// Exports of sibling Zig objects, called across object boundaries.
// Each exporting module checks its definition against these signatures.
// ---------------------------------------------------------------------

// zig/geometry.zig
pub extern fn yabai_area_make_pair(split: c_int, gap: c_int, ratio: f32, parent: *const Area, left: *Area, right: *Area) void;
pub extern fn yabai_area_is_in_direction(r1: *const Area, r1_max: *const CgPoint, r2: *const Area, r2_max: *const CgPoint, direction: c_int) bool;
pub extern fn yabai_area_distance_in_direction(r1: *const Area, r1_max: *const CgPoint, r2: *const Area, r2_max: *const CgPoint, direction: c_int) c_int;

// zig/util.zig
pub extern fn yabai_balance_node_add(a: *const BalanceNode, b: *const BalanceNode, out: *BalanceNode) void;
pub extern fn yabai_find_rank_in_list(wid: u32, window_list: [*]const u32, window_count: c_int) c_int;
pub extern fn yabai_json_bool(value: bool) [*:0]const u8;
pub extern fn yabai_json_optional_bool(value: c_int) [*:0]const u8;
pub extern fn yabai_in_range_ii_f(a: f32, b: f32, c: f32) bool;

// zig/message.zig
pub extern fn yabai_message_get_token(message: *[*:0]u8, out: *MsgToken) void;
pub extern fn yabai_token_is_valid(token: *const MsgToken) bool;
pub extern fn yabai_token_equals(token: *const MsgToken, match: [*:0]const u8) bool;
pub extern fn yabai_token_to_value(token: *const MsgToken, out: *TokenValue) void;
pub extern fn yabai_parse_key_value_pair(token: [*:0]u8, key: *?[*]u8, value: *?[*]u8, exclusion: *bool) void;

/// Compile error unless the exported definition `def` has exactly the
/// signature declared here as `decl`.
pub fn matches(comptime def: anytype, comptime decl: anytype) void {
    if (@TypeOf(def) != @TypeOf(decl))
        @compileError("export/extern signature mismatch: " ++ @typeName(@TypeOf(def)) ++ " vs " ++ @typeName(@TypeOf(decl)));
}
