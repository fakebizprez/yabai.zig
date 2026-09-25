// View-tree traversal boundary for the yabai migration (step 3).
//
// Pinned toolchain: Zig 0.16.0. Mirrors the pure tree helpers in
// src/view.c. WindowNode mirrors struct window_node in src/view.h
// byte for byte (344 bytes; offsets verified with
// clang -fdump-record-layouts and asserted below and in
// tests/src/zig_view.c).

const std = @import("std");

pub const FeedbackWindow = extern struct {
    id: u32,
    context: ?*anyopaque,
};

pub const Area = extern struct {
    x: f32,
    y: f32,
    w: f32,
    h: f32,
};

pub const CgRect = extern struct {
    x: f64,
    y: f64,
    w: f64,
    h: f64,
};

pub const CgPoint = extern struct {
    x: f64,
    y: f64,
};

pub const BalanceNode = extern struct {
    y_count: c_int,
    x_count: c_int,
};

comptime {
    if (@sizeOf(Area) != 16) @compileError("Area size drift");
    if (@sizeOf(CgRect) != 32) @compileError("CgRect size drift");
    if (@sizeOf(CgPoint) != 16) @compileError("CgPoint size drift");
    if (@sizeOf(BalanceNode) != 8) @compileError("BalanceNode size drift");
}

pub const WindowNode = extern struct {
    area: Area,
    parent: ?*WindowNode,
    left: ?*WindowNode,
    right: ?*WindowNode,
    zoom: ?*WindowNode,
    window_list: [32]u32,
    window_order: [32]u32,
    window_count: c_int,
    ratio: f32,
    split: c_int,
    child: c_int,
    insert_dir: c_int,
    feedback_window: FeedbackWindow,
};

comptime {
    if (@sizeOf(WindowNode) != 344) @compileError("WindowNode size drift");
    if (@offsetOf(WindowNode, "left") != 24) @compileError("WindowNode.left drift");
    if (@offsetOf(WindowNode, "window_list") != 48) @compileError("WindowNode.window_list drift");
    if (@offsetOf(WindowNode, "window_count") != 304) @compileError("WindowNode.window_count drift");
    if (@offsetOf(WindowNode, "feedback_window") != 328) @compileError("WindowNode.feedback_window drift");
    if (@sizeOf(FeedbackWindow) != 16) @compileError("FeedbackWindow size drift");
}

fn isLeaf(node: *const WindowNode) bool {
    return node.left == null and node.right == null;
}

fn isLeftChild(node: *const WindowNode) bool {
    const parent = node.parent orelse return false;
    return parent.left == node;
}

fn isRightChild(node: *const WindowNode) bool {
    const parent = node.parent orelse return false;
    return parent.right == node;
}

export fn yabai_window_node_is_leaf(node: *const WindowNode) bool {
    @setRuntimeSafety(false);
    return isLeaf(node);
}

export fn yabai_window_node_is_left_child(node: *const WindowNode) bool {
    @setRuntimeSafety(false);
    return isLeftChild(node);
}

export fn yabai_window_node_is_right_child(node: *const WindowNode) bool {
    @setRuntimeSafety(false);
    return isRightChild(node);
}

export fn yabai_window_node_find_first_leaf(root: *WindowNode) ?*WindowNode {
    @setRuntimeSafety(false);
    var node = root;
    while (!isLeaf(node)) {
        node = node.left orelse unreachable;
    }
    return node;
}

export fn yabai_window_node_find_last_leaf(root: *WindowNode) ?*WindowNode {
    @setRuntimeSafety(false);
    var node = root;
    while (!isLeaf(node)) {
        node = node.right orelse unreachable;
    }
    return node;
}

export fn yabai_window_node_find_prev_leaf(node: *WindowNode) ?*WindowNode {
    @setRuntimeSafety(false);
    const parent = node.parent orelse return null;
    if (isLeftChild(node)) {
        return yabai_window_node_find_prev_leaf(parent);
    }
    const left = parent.left orelse unreachable;
    if (isLeaf(left)) {
        return left;
    }
    const right = left.right orelse unreachable;
    return yabai_window_node_find_last_leaf(right);
}

export fn yabai_window_node_find_next_leaf(node: *WindowNode) ?*WindowNode {
    @setRuntimeSafety(false);
    const parent = node.parent orelse return null;
    if (isRightChild(node)) {
        return yabai_window_node_find_next_leaf(parent);
    }
    const right = parent.right orelse unreachable;
    if (isLeaf(right)) {
        return right;
    }
    const left = right.left orelse unreachable;
    return yabai_window_node_find_first_leaf(left);
}

export fn yabai_window_node_contains_window(node: *const WindowNode, window_id: u32) bool {
    @setRuntimeSafety(false);
    var i: c_int = 0;
    while (i < node.window_count) {
        if (node.window_list[@intCast(i)] == window_id) return true;
        i += 1;
    }
    return false;
}

pub const SPLIT_NONE: c_int = 0;
pub const SPLIT_Y: c_int = 1;
pub const SPLIT_X: c_int = 2;
pub const SPLIT_AUTO: c_int = 3;
pub const CHILD_NONE: c_int = 0;
pub const CHILD_SECOND: c_int = 1;
pub const CHILD_FIRST: c_int = 2;
pub const VIEW_ENABLE_PADDING: u64 = 0x080;
pub const VIEW_ENABLE_GAP: u64 = 0x100;
pub const VIEW_FLOAT: c_int = 3;
pub const VIEW_IS_VALID: u64 = 0x200;
pub const VIEW_IS_DIRTY: u64 = 0x400;
pub const DIR_NORTH: c_int = 360;
pub const DIR_EAST: c_int = 90;
pub const DIR_SOUTH: c_int = 180;
pub const DIR_WEST: c_int = 270;

export fn yabai_window_node_rotate(node: *WindowNode, degrees: c_int) void {
    @setRuntimeSafety(false);
    if ((degrees == 90 and node.split == SPLIT_Y) or
        (degrees == 270 and node.split == SPLIT_X) or
        (degrees == 180))
    {
        const temp = node.left;
        node.left = node.right;
        node.right = temp;
        node.ratio = 1 - node.ratio;
    }
    if (degrees != 180) {
        if (node.split == SPLIT_X) {
            node.split = SPLIT_Y;
        } else if (node.split == SPLIT_Y) {
            node.split = SPLIT_X;
        }
    }
    if (!isLeaf(node)) {
        const left = node.left orelse unreachable;
        const right = node.right orelse unreachable;
        yabai_window_node_rotate(left, degrees);
        yabai_window_node_rotate(right, degrees);
    }
}

export fn yabai_window_node_mirror(node: *WindowNode, axis: c_int) ?*WindowNode {
    @setRuntimeSafety(false);
    if (!isLeaf(node)) {
        const l = node.left orelse unreachable;
        const r = node.right orelse unreachable;
        const left = yabai_window_node_mirror(l, axis);
        const right = yabai_window_node_mirror(r, axis);
        if (node.split == axis) {
            node.left = right;
            node.right = left;
        }
    }
    return node;
}

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

comptime {
    if (@sizeOf(View) != 72) @compileError("View size drift");
    if (@offsetOf(View, "root") != 16) @compileError("View.root drift");
}

export fn yabai_view_find_window_node(view: *const View, window_id: u32) ?*WindowNode {
    @setRuntimeSafety(false);
    var node = yabai_window_node_find_first_leaf(view.root orelse unreachable);
    while (node) |n| {
        if (yabai_window_node_contains_window(n, window_id)) return n;
        node = yabai_window_node_find_next_leaf(n);
    }
    return null;
}

export fn yabai_view_stack_window(node: *WindowNode, window_id: u32) void {
    @setRuntimeSafety(false);
    var insert_index: c_int = node.window_count;
    var i: c_int = 0;
    while (i < node.window_count) {
        if (node.window_list[@intCast(i)] == node.window_order[0]) {
            insert_index = i + 1;
            break;
        }
        i += 1;
    }
    // Both shifts move elements right (dst > src), so they must copy
    // high-to-low like C memmove does for this overlap direction.
    if (insert_index < node.window_count) {
        const n: usize = @intCast(node.window_count - insert_index);
        const dst = node.window_list[@intCast(insert_index + 1)..][0..n];
        const src = node.window_list[@intCast(insert_index)..][0..n];
        std.mem.copyBackwards(u32, dst, src);
    }
    node.window_list[@intCast(insert_index)] = window_id;
    const m: usize = @intCast(node.window_count);
    std.mem.copyBackwards(u32, node.window_order[1..][0..m], node.window_order[0..][0..m]);
    node.window_order[0] = window_id;
    node.window_count += 1;
}

export fn yabai_window_node_fence(node: ?*WindowNode, dir: c_int) ?*WindowNode {
    @setRuntimeSafety(false);
    const n = node orelse return null;
    var parent = n.parent;
    while (parent) |p| {
        if ((dir == DIR_NORTH and p.split == SPLIT_X and p.area.y < n.area.y) or
            (dir == DIR_WEST and p.split == SPLIT_Y and p.area.x < n.area.x) or
            (dir == DIR_SOUTH and p.split == SPLIT_X and (p.area.y + p.area.h) > (n.area.y + n.area.h)) or
            (dir == DIR_EAST and p.split == SPLIT_Y and (p.area.x + p.area.w) > (n.area.x + n.area.w)))
        {
            return p;
        }
        parent = p.parent;
    }
    return null;
}

// Mirrors the C quirk of returning 0 (not -1) when the window is absent.
export fn yabai_window_node_index_of_window(node: *const WindowNode, window_id: u32) c_int {
    @setRuntimeSafety(false);
    var i: c_int = 0;
    while (i < node.window_count) {
        if (node.window_list[@intCast(i)] == window_id) return i;
        i += 1;
    }
    return 0;
}

// Mirrors window_node_is_occupied(): occupied when the node holds windows.
export fn yabai_window_node_is_occupied(node: *const WindowNode) bool {
    @setRuntimeSafety(false);
    return node.window_count != 0;
}

// Mirrors window_node_is_intermediate(): intermediate when it has a parent.
export fn yabai_window_node_is_intermediate(node: *const WindowNode) bool {
    @setRuntimeSafety(false);
    return node.parent != null;
}

// Mirrors window_node_get_child(): explicit child wins, otherwise the
// space placement is lifted in as window_placement so the export stays pure.
export fn yabai_window_node_get_child(node: *const WindowNode, window_placement: c_int) c_int {
    @setRuntimeSafety(false);
    if (node.child != CHILD_NONE) return node.child;
    return window_placement;
}

// Mirrors window_node_get_split(): node split wins, then explicit view
// split, then the space split_type (lifted in), then area aspect fallback.
export fn yabai_window_node_get_split(view: *const View, node: *const WindowNode, space_split_type: c_int) c_int {
    @setRuntimeSafety(false);
    if (node.split != SPLIT_NONE) return node.split;
    if (view.split_type != SPLIT_NONE) {
        if (view.split_type != SPLIT_AUTO) return view.split_type;
    } else if (space_split_type != SPLIT_AUTO) {
        return space_split_type;
    }
    return if (node.area.w >= node.area.h) SPLIT_Y else SPLIT_X;
}

// Mirrors window_node_get_ratio(): in-range ratio wins, otherwise the
// space split_ratio lifted in as space_split_ratio.
export fn yabai_window_node_get_ratio(node: *const WindowNode, space_split_ratio: f32) f32 {
    @setRuntimeSafety(false);
    if (node.ratio >= 0.1 and node.ratio <= 0.9) return node.ratio;
    return space_split_ratio;
}

// Mirrors window_node_get_gap(): gap gated by VIEW_ENABLE_GAP.
export fn yabai_window_node_get_gap(view: *const View) c_int {
    @setRuntimeSafety(false);
    if ((view.flags & VIEW_ENABLE_GAP) != 0) return view.window_gap;
    return 0;
}

// Mirrors area_from_cgrect(): narrows the CGRect doubles to float areas.
export fn yabai_area_from_cgrect(rect: *const CgRect, out: *Area) void {
    @setRuntimeSafety(false);
    out.x = @floatCast(rect.x);
    out.y = @floatCast(rect.y);
    out.w = @floatCast(rect.w);
    out.h = @floatCast(rect.h);
}

// Mirrors area_max_point(): float math first, then widened to double,
// exactly as the C promotion rules dictate.
export fn yabai_area_max_point(area: *const Area, out: *CgPoint) void {
    @setRuntimeSafety(false);
    const mx: f32 = area.x + area.w - 1;
    const my: f32 = area.y + area.h - 1;
    out.x = mx;
    out.y = my;
}

// Mirrors window_node_equalize(): resets matching-axis ratios to the
// space split_ratio lifted in as space_split_ratio.
export fn yabai_window_node_equalize(node: *WindowNode, axis_flag: u32, space_split_ratio: f32) void {
    @setRuntimeSafety(false);
    if (node.left) |l| yabai_window_node_equalize(l, axis_flag, space_split_ratio);
    if (node.right) |r| yabai_window_node_equalize(r, axis_flag, space_split_ratio);
    const y_mask: u32 = @intCast(SPLIT_Y);
    const x_mask: u32 = @intCast(SPLIT_X);
    if ((axis_flag & y_mask) != 0 and node.split == SPLIT_Y) {
        node.ratio = space_split_ratio;
    }
    if ((axis_flag & x_mask) != 0 and node.split == SPLIT_X) {
        node.ratio = space_split_ratio;
    }
}

extern fn yabai_balance_node_add(a: *const BalanceNode, b: *const BalanceNode, out: *BalanceNode) void;

fn balanceInner(node: *WindowNode, axis_flag: u32) BalanceNode {
    @setRuntimeSafety(false);
    if (isLeaf(node)) {
        const y: c_int = if (node.parent) |p| (if (p.split == SPLIT_Y) 1 else 0) else 0;
        const x: c_int = if (node.parent) |p| (if (p.split == SPLIT_X) 1 else 0) else 0;
        return .{ .y_count = y, .x_count = x };
    }
    const left_node = node.left orelse unreachable;
    const right_node = node.right orelse unreachable;
    const left_leafs = balanceInner(left_node, axis_flag);
    const right_leafs = balanceInner(right_node, axis_flag);
    var total: BalanceNode = undefined;
    yabai_balance_node_add(&left_leafs, &right_leafs, &total);
    const y_mask: u32 = @intCast(SPLIT_Y);
    const x_mask: u32 = @intCast(SPLIT_X);
    if ((axis_flag & y_mask) != 0 and node.split == SPLIT_Y) {
        node.ratio = @as(f32, @floatFromInt(left_leafs.y_count)) / @as(f32, @floatFromInt(total.y_count));
        total.y_count -= 1;
    }
    if ((axis_flag & x_mask) != 0 and node.split == SPLIT_X) {
        node.ratio = @as(f32, @floatFromInt(left_leafs.x_count)) / @as(f32, @floatFromInt(total.x_count));
        total.x_count -= 1;
    }
    if (node.parent) |p| {
        total.y_count += if (p.split == SPLIT_Y) 1 else 0;
        total.x_count += if (p.split == SPLIT_X) 1 else 0;
    }
    return total;
}

// Mirrors window_node_balance(): leaf counts bubble up through the
// already-ported balance_node_add, ratios split by left/total shares.
export fn yabai_window_node_balance(node: *WindowNode, axis_flag: u32, out: *BalanceNode) void {
    @setRuntimeSafety(false);
    out.* = balanceInner(node, axis_flag);
}

// Shared pair geometry in zig/geometry.zig (extern pattern, like
// yabai_balance_node_add): the split seam below reuses it instead of
// duplicating the float-to-int rounding.
extern fn yabai_area_make_pair(split: c_int, gap: c_int, ratio: f32, parent: *const Area, left: *Area, right: *Area) void;

extern "c" fn malloc(size: usize) ?[*]u8;
extern "c" fn realloc(ptr: ?*anyopaque, size: usize) ?[*]u8;

// Mirrors area_make_pair_for_node(): resolves split/ratio/gap through
// the already-ported get_split/get_ratio/get_gap helpers (the space
// split_type/split_ratio arrive lifted in), fans the area out through
// the shared yabai_area_make_pair, and writes the resolved values back.
export fn yabai_area_make_pair_for_node(view: *const View, node: *WindowNode, space_split_type: c_int, space_split_ratio: f32) void {
    @setRuntimeSafety(false);
    const split = yabai_window_node_get_split(view, node, space_split_type);
    const ratio = yabai_window_node_get_ratio(node, space_split_ratio);
    const gap = yabai_window_node_get_gap(view);
    const left = node.left orelse unreachable;
    const right = node.right orelse unreachable;
    yabai_area_make_pair(split, gap, ratio, &node.area, &left.area, &right.area);
    node.split = split;
    node.ratio = ratio;
}

// Mirrors window_node_split(): two zeroed libc mallocs for the children,
// zoom carried per window_zoom_persist (own node when the zoom was the
// parent, else the view root), the existing windows kept on the placed
// side (window_placement lifted in), the fresh window id alone on the
// other side, then the area pair resolved as above.
export fn yabai_window_node_split(view: *const View, node: *WindowNode, window_id: u32, window_zoom_persist: bool, window_placement: c_int, space_split_type: c_int, space_split_ratio: f32) void {
    @setRuntimeSafety(false);
    const left_mem = malloc(@sizeOf(WindowNode)) orelse unreachable;
    const right_mem = malloc(@sizeOf(WindowNode)) orelse unreachable;
    const left: *WindowNode = @ptrCast(@alignCast(left_mem));
    const right: *WindowNode = @ptrCast(@alignCast(right_mem));
    left.* = std.mem.zeroes(WindowNode);
    right.* = std.mem.zeroes(WindowNode);

    const zoom: ?*WindowNode = if (!window_zoom_persist) null else if (node.zoom == null) null else if (node.zoom == node.parent) node else view.root;

    const count: usize = @intCast(node.window_count);
    if (yabai_window_node_get_child(node, window_placement) == CHILD_SECOND) {
        @memcpy(left.window_list[0..count], node.window_list[0..count]);
        @memcpy(left.window_order[0..count], node.window_order[0..count]);
        left.window_count = node.window_count;
        left.zoom = zoom;

        right.window_list[0] = window_id;
        right.window_order[0] = window_id;
        right.window_count = 1;
    } else {
        @memcpy(right.window_list[0..count], node.window_list[0..count]);
        @memcpy(right.window_order[0..count], node.window_order[0..count]);
        right.window_count = node.window_count;
        right.zoom = zoom;

        left.window_list[0] = window_id;
        left.window_order[0] = window_id;
        left.window_count = 1;
    }

    left.parent = node;
    right.parent = node;

    node.window_count = 0;
    node.left = left;
    node.right = right;
    node.zoom = null;

    yabai_area_make_pair_for_node(view, node, space_split_type, space_split_ratio);
}

// Mirrors window_node_swap_window_list(): exchanges both lists, both
// orders, and the counts, then clears both zooms.
export fn yabai_window_node_swap_window_list(a_node: *WindowNode, b_node: *WindowNode) void {
    @setRuntimeSafety(false);
    var tmp_list: [32]u32 = undefined;
    var tmp_order: [32]u32 = undefined;
    const a_count: usize = @intCast(a_node.window_count);
    const b_count: usize = @intCast(b_node.window_count);
    @memcpy(tmp_list[0..a_count], a_node.window_list[0..a_count]);
    @memcpy(tmp_order[0..a_count], a_node.window_order[0..a_count]);
    const tmp_count = a_node.window_count;

    @memcpy(a_node.window_list[0..b_count], b_node.window_list[0..b_count]);
    @memcpy(a_node.window_order[0..b_count], b_node.window_order[0..b_count]);
    a_node.window_count = b_node.window_count;

    const n: usize = @intCast(tmp_count);
    @memcpy(b_node.window_list[0..n], tmp_list[0..n]);
    @memcpy(b_node.window_order[0..n], tmp_order[0..n]);
    b_node.window_count = tmp_count;

    a_node.zoom = null;
    b_node.zoom = null;
}

// Mirrors view_find_min_depth_leaf_node(): breadth-first scan over a
// 256-slot queue (nodes before children, left before right); returns
// the first leaf, NULL when the slots exhaust without finding one.
export fn yabai_view_find_min_depth_leaf_node(node: *WindowNode) ?*WindowNode {
    @setRuntimeSafety(false);
    var list: [256]?*WindowNode = [_]?*WindowNode{null} ** 256;
    list[0] = node;
    var j: usize = 0;
    var i: usize = 0;
    while (i < 256) : (i += 1) {
        const cur = list[i] orelse unreachable;
        if (isLeaf(cur)) return cur;
        j += 1;
        list[j] = cur.left;
        j += 1;
        list[j] = cur.right;
    }
    return null;
}

// Mirrors view_is_invalid(): invalid while the VALID bit is clear.
export fn yabai_view_is_invalid(view: *const View) bool {
    @setRuntimeSafety(false);
    return (view.flags & VIEW_IS_VALID) == 0;
}

// Mirrors view_is_dirty(): dirty while the DIRTY bit is set.
export fn yabai_view_is_dirty(view: *const View) bool {
    @setRuntimeSafety(false);
    return (view.flags & VIEW_IS_DIRTY) != 0;
}

// Mirrors window_node_clear_zoom(): drops zoom on the whole subtree.
export fn yabai_window_node_clear_zoom(node: *WindowNode) void {
    @setRuntimeSafety(false);
    node.zoom = null;
    if (!isLeaf(node)) {
        yabai_window_node_clear_zoom(node.left orelse unreachable);
        yabai_window_node_clear_zoom(node.right orelse unreachable);
    }
}

// Mirrors view_find_window_list(): leaf walk in first/next order through
// the already-ported yabai_window_node_find_first_leaf /
// yabai_window_node_find_next_leaf, initial capacity 13 doubling when
// count + node_count reaches capacity. ts_alloc_list never zeroes and
// ts_expand preserves in place, mirrored here with libc malloc/realloc
// which likewise leave fresh bytes uninitialized and preserve old ones.
// Ownership asymmetry: the C original returns temp-pool memory the
// caller never frees; this export returns libc malloc'd memory the C
// caller must free (the parity test frees only the Zig side).
export fn yabai_view_find_window_list(view: *const View, window_count: *c_int) ?[*]u32 {
    @setRuntimeSafety(false);
    window_count.* = 0;
    var capacity: usize = 13;
    const first_mem = malloc(capacity * @sizeOf(u32)) orelse unreachable;
    var list: [*]u32 = @ptrCast(@alignCast(first_mem));
    var count: usize = 0;
    var node = yabai_window_node_find_first_leaf(view.root orelse unreachable);
    while (node) |n| {
        const node_count: usize = @intCast(n.window_count);
        if (count + node_count >= capacity) {
            const new_capacity = capacity * 2;
            const grown_mem = realloc(@ptrCast(list), new_capacity * @sizeOf(u32)) orelse unreachable;
            list = @ptrCast(@alignCast(grown_mem));
            capacity = new_capacity;
        }
        var i: usize = 0;
        while (i < node_count) : (i += 1) {
            list[count] = n.window_list[i];
            count += 1;
        }
        node = yabai_window_node_find_next_leaf(n);
    }
    window_count.* = @intCast(count);
    return list;
}

// Caller-provided g_space_manager block for yabai_view_create (field
// order mirrors the C user-branch assignment order: layout, paddings,
// gap, auto_balance, split_type).
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

comptime {
    if (@sizeOf(ViewDefaults) != 32) @compileError("ViewDefaults size drift");
}

extern fn window_manager_remove_managed_window(wm: ?*anyopaque, wid: u32) void;
extern fn insert_feedback_destroy(node: *WindowNode) void;
extern fn CFRelease(cf: ?*anyopaque) void;
extern var g_window_manager: anyopaque;
extern "c" fn free(ptr: ?*anyopaque) void;

// Mirrors view_create() in src/view.c including its trailing view_update:
// two zeroed libc mallocs (the view, then its root), then sid/uuid
// assignment and the ENABLE_PADDING|ENABLE_GAP flags in C order. The
// SLS-derived inputs arrive lifted in: uuid is assigned verbatim,
// is_user selects the user branch, and defaults carries the
// g_space_manager block the C user branch copies; the else branch pins
// VIEW_FLOAT. C's trailing view_update is omitted as a proven no-op: it
// runs on the zeroed root (a leaf with insert_dir clear), where
// window_node_update does nothing.
export fn yabai_view_create(sid: u64, uuid: ?*anyopaque, is_user: bool, defaults: *const ViewDefaults) *View {
    @setRuntimeSafety(false);
    const mem = malloc(@sizeOf(View)) orelse unreachable;
    const view: *View = @ptrCast(@alignCast(mem));
    view.* = std.mem.zeroes(View);
    const root_mem = malloc(@sizeOf(WindowNode)) orelse unreachable;
    const root: *WindowNode = @ptrCast(@alignCast(root_mem));
    root.* = std.mem.zeroes(WindowNode);
    view.root = root;
    view.sid = sid;
    view.uuid = uuid;
    view.flags |= VIEW_ENABLE_PADDING;
    view.flags |= VIEW_ENABLE_GAP;
    if (is_user) {
        view.layout = defaults.layout;
        view.top_padding = defaults.top_padding;
        view.bottom_padding = defaults.bottom_padding;
        view.left_padding = defaults.left_padding;
        view.right_padding = defaults.right_padding;
        view.window_gap = defaults.window_gap;
        view.auto_balance = defaults.auto_balance;
        view.split_type = defaults.split_type;
    } else {
        view.layout = VIEW_FLOAT;
    }
    return view;
}

// Mirrors the static window_node_destroy() in src/view.c: recursive
// child frees, managed-window removals, feedback destroy, then the
// node free itself.
fn destroyNode(node: *WindowNode) void {
    @setRuntimeSafety(false);
    if (node.left) |l| destroyNode(l);
    if (node.right) |r| destroyNode(r);
    var i: c_int = 0;
    while (i < node.window_count) {
        window_manager_remove_managed_window(&g_window_manager, node.window_list[@intCast(i)]);
        i += 1;
    }
    insert_feedback_destroy(node);
    free(@ptrCast(node));
}

// Mirrors view_clear() including its trailing view_update: drops both
// child subtrees, removes the root's windows, destroys its feedback,
// and zeroes the root. The root allocation itself is kept for reuse.
// The trailing view_update is omitted as a proven no-op: it runs on
// the just-zeroed root (a leaf with insert_dir clear), where
// window_node_update does nothing.
export fn yabai_view_clear(view: *View) void {
    @setRuntimeSafety(false);
    const root = view.root orelse return;
    if (root.left) |l| destroyNode(l);
    if (root.right) |r| destroyNode(r);
    var i: c_int = 0;
    while (i < root.window_count) {
        window_manager_remove_managed_window(&g_window_manager, root.window_list[@intCast(i)]);
        i += 1;
    }
    insert_feedback_destroy(root);
    root.* = std.mem.zeroes(WindowNode);
}

// Mirrors view_destroy(): destroys the whole root subtree and releases
// the uuid. Like C, it does not free the view itself; the caller owns
// that (see event_loop.c SLS_SPACE_DESTROYED).
export fn yabai_view_destroy(view: *View) void {
    @setRuntimeSafety(false);
    if (view.root) |r| destroyNode(r);
    if (view.uuid) |u| CFRelease(u);
}
