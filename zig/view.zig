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

pub const WindowNode = extern struct {
    area: extern struct {
        x: f32,
        y: f32,
        w: f32,
        h: f32,
    },
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

pub const SPLIT_Y: c_int = 1;
pub const SPLIT_X: c_int = 2;
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
