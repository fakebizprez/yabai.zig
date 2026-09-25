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
