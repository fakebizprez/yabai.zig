// View-tree traversal boundary for the yabai migration (step 3).
//
// Pinned toolchain: Zig 0.17.0. Mirrors the pure tree helpers in
// src/view.c. WindowNode mirrors struct window_node in src/view.h
// byte for byte (344 bytes); the mirror lives in zig/abi.zig, pinned
// on the C side by src/zig_abi.h.

const std = @import("std");

const abi = @import("abi.zig");
const Area = abi.Area;
const CgRect = abi.CgRect;
const CgPoint = abi.CgPoint;
const BalanceNode = abi.BalanceNode;
const WindowNode = abi.WindowNode;
const View = abi.View;
const ViewDefaults = abi.ViewDefaults;

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

export fn yabai_window_node_rotate(node: *WindowNode, degrees: c_int) void {
    @setRuntimeSafety(false);
    if ((degrees == 90 and node.split == abi.SPLIT_Y) or
        (degrees == 270 and node.split == abi.SPLIT_X) or
        (degrees == 180))
    {
        const temp = node.left;
        node.left = node.right;
        node.right = temp;
        node.ratio = 1 - node.ratio;
    }
    if (degrees != 180) {
        if (node.split == abi.SPLIT_X) {
            node.split = abi.SPLIT_Y;
        } else if (node.split == abi.SPLIT_Y) {
            node.split = abi.SPLIT_X;
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

export fn yabai_view_find_window_node(view: *const View, window_id: u32) ?*WindowNode {
    @setRuntimeSafety(false);
    var node = yabai_window_node_find_first_leaf(view.root orelse unreachable);
    while (node) |n| {
        if (yabai_window_node_contains_window(n, window_id)) return n;
        node = yabai_window_node_find_next_leaf(n);
    }
    return null;
}

// Mirrors view_find_window_node_in_direction() in src/view.c: leaf walk
// in first/next order with the source skipped, keeping the best
// (distance, rank) pair under strict-less distance or equal-distance
// strict-less rank. The SLS space_window_list arrives lifted in as
// window_list/window_count (the C delegate evaluates it at the site
// exactly as the C body did) so the export stays pure. Leaf walk and
// area_max_point reuse the same-file ports directly.
// Rank-miss note: C window_manager_find_rank_of_window_in_list and
// yabai_find_rank_in_list both return INT_MAX on a miss, so a leaf whose
// window_order[0] is absent from the SLS list ranks last on both sides.
// A NULL list returns NULL, matching the C early-out.
export fn yabai_view_find_window_node_in_direction(view: *const View, source: *WindowNode, direction: c_int, window_list: ?[*]const u32, window_count: c_int) ?*WindowNode {
    @setRuntimeSafety(false);
    const list = window_list orelse return null;
    var best_distance: c_int = std.math.maxInt(c_int);
    var best_rank: c_int = std.math.maxInt(c_int);
    var best_node: ?*WindowNode = null;
    var source_max: CgPoint = undefined;
    yabai_area_max_point(&source.area, &source_max);
    var target = yabai_window_node_find_first_leaf(view.root orelse unreachable);
    while (target) |t| {
        if (t != source) {
            var target_max: CgPoint = undefined;
            yabai_area_max_point(&t.area, &target_max);
            if (abi.yabai_area_is_in_direction(&source.area, &source_max, &t.area, &target_max, direction)) {
                const distance = abi.yabai_area_distance_in_direction(&source.area, &source_max, &t.area, &target_max, direction);
                const rank = abi.yabai_find_rank_in_list(t.window_order[0], list, window_count);
                if (distance < best_distance or (distance == best_distance and rank < best_rank)) {
                    best_node = t;
                    best_distance = distance;
                    best_rank = rank;
                }
            }
        }
        target = yabai_window_node_find_next_leaf(t);
    }
    return best_node;
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
        if ((dir == abi.DIR_NORTH and p.split == abi.SPLIT_X and p.area.y < n.area.y) or
            (dir == abi.DIR_WEST and p.split == abi.SPLIT_Y and p.area.x < n.area.x) or
            (dir == abi.DIR_SOUTH and p.split == abi.SPLIT_X and (p.area.y + p.area.h) > (n.area.y + n.area.h)) or
            (dir == abi.DIR_EAST and p.split == abi.SPLIT_Y and (p.area.x + p.area.w) > (n.area.x + n.area.w)))
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
    if (node.child != abi.CHILD_NONE) return node.child;
    return window_placement;
}

// Mirrors window_node_get_split(): node split wins, then explicit view
// split, then the space split_type (lifted in), then area aspect fallback.
export fn yabai_window_node_get_split(view: *const View, node: *const WindowNode, space_split_type: c_int) c_int {
    @setRuntimeSafety(false);
    if (node.split != abi.SPLIT_NONE) return node.split;
    if (view.split_type != abi.SPLIT_NONE) {
        if (view.split_type != abi.SPLIT_AUTO) return view.split_type;
    } else if (space_split_type != abi.SPLIT_AUTO) {
        return space_split_type;
    }
    return if (node.area.w >= node.area.h) abi.SPLIT_Y else abi.SPLIT_X;
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
    if ((view.flags & abi.VIEW_ENABLE_GAP) != 0) return view.window_gap;
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
    const y_mask: u32 = @intCast(abi.SPLIT_Y);
    const x_mask: u32 = @intCast(abi.SPLIT_X);
    if ((axis_flag & y_mask) != 0 and node.split == abi.SPLIT_Y) {
        node.ratio = space_split_ratio;
    }
    if ((axis_flag & x_mask) != 0 and node.split == abi.SPLIT_X) {
        node.ratio = space_split_ratio;
    }
}

fn balanceInner(node: *WindowNode, axis_flag: u32) BalanceNode {
    @setRuntimeSafety(false);
    if (isLeaf(node)) {
        const y: c_int = if (node.parent) |p| (if (p.split == abi.SPLIT_Y) 1 else 0) else 0;
        const x: c_int = if (node.parent) |p| (if (p.split == abi.SPLIT_X) 1 else 0) else 0;
        return .{ .y_count = y, .x_count = x };
    }
    const left_node = node.left orelse unreachable;
    const right_node = node.right orelse unreachable;
    const left_leafs = balanceInner(left_node, axis_flag);
    const right_leafs = balanceInner(right_node, axis_flag);
    var total: BalanceNode = undefined;
    abi.yabai_balance_node_add(&left_leafs, &right_leafs, &total);
    const y_mask: u32 = @intCast(abi.SPLIT_Y);
    const x_mask: u32 = @intCast(abi.SPLIT_X);
    if ((axis_flag & y_mask) != 0 and node.split == abi.SPLIT_Y) {
        node.ratio = @as(f32, @floatFromInt(left_leafs.y_count)) / @as(f32, @floatFromInt(total.y_count));
        total.y_count -= 1;
    }
    if ((axis_flag & x_mask) != 0 and node.split == abi.SPLIT_X) {
        node.ratio = @as(f32, @floatFromInt(left_leafs.x_count)) / @as(f32, @floatFromInt(total.x_count));
        total.x_count -= 1;
    }
    if (node.parent) |p| {
        total.y_count += if (p.split == abi.SPLIT_Y) 1 else 0;
        total.x_count += if (p.split == abi.SPLIT_X) 1 else 0;
    }
    return total;
}

// Mirrors window_node_balance(): leaf counts bubble up through the
// already-ported balance_node_add, ratios split by left/total shares.
export fn yabai_window_node_balance(node: *WindowNode, axis_flag: u32, out: *BalanceNode) void {
    @setRuntimeSafety(false);
    out.* = balanceInner(node, axis_flag);
}

extern "c" fn malloc(size: usize) ?[*]u8;
extern "c" fn realloc(ptr: ?*anyopaque, size: usize) ?[*]u8;
// Out-of-memory crashes here the way C did; @panic would need a Zig runtime.
extern "c" fn abort() noreturn;

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
    abi.yabai_area_make_pair(split, gap, ratio, &node.area, &left.area, &right.area);
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
    const left_mem = malloc(@sizeOf(WindowNode)) orelse abort();
    const right_mem = malloc(@sizeOf(WindowNode)) orelse abort();
    const left: *WindowNode = @ptrCast(@alignCast(left_mem));
    const right: *WindowNode = @ptrCast(@alignCast(right_mem));
    left.* = std.mem.zeroes(WindowNode);
    right.* = std.mem.zeroes(WindowNode);

    const zoom: ?*WindowNode = if (!window_zoom_persist) null else if (node.zoom == null) null else if (node.zoom == node.parent) node else view.root;

    const count: usize = @intCast(node.window_count);
    if (yabai_window_node_get_child(node, window_placement) == abi.CHILD_SECOND) {
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
    var list: [256]?*WindowNode = @splat(null);
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
    return (view.flags & abi.VIEW_IS_VALID) == 0;
}

// Mirrors view_is_dirty(): dirty while the DIRTY bit is set.
export fn yabai_view_is_dirty(view: *const View) bool {
    @setRuntimeSafety(false);
    return (view.flags & abi.VIEW_IS_DIRTY) != 0;
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
// yabai_window_node_find_next_leaf, initial capacity 13 doubling until
// count + node_count fits, then one realloc. (Doubling only once per
// leaf, as C does, overflows when a leaf holds more windows than the
// doubled capacity, e.g. a 31-window stack leaf against 26 slots.)
// Fresh bytes stay uninitialized and realloc preserves old ones.
// Ownership asymmetry: the C original returns temp-pool memory the
// caller never frees; this export returns libc malloc'd memory the C
// caller must free (the parity test frees only the Zig side).
export fn yabai_view_find_window_list(view: *const View, window_count: *c_int) ?[*]u32 {
    @setRuntimeSafety(false);
    window_count.* = 0;
    var capacity: usize = 13;
    const first_mem = malloc(capacity * @sizeOf(u32)) orelse abort();
    var list: [*]u32 = @ptrCast(@alignCast(first_mem));
    var count: usize = 0;
    var node = yabai_window_node_find_first_leaf(view.root orelse unreachable);
    while (node) |n| {
        const node_count: usize = @intCast(n.window_count);
        if (count + node_count >= capacity) {
            var new_capacity = capacity * 2;
            while (count + node_count >= new_capacity) new_capacity *= 2;
            const grown_mem = realloc(@ptrCast(list), new_capacity * @sizeOf(u32)) orelse abort();
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
    const mem = malloc(@sizeOf(View)) orelse abort();
    const view: *View = @ptrCast(@alignCast(mem));
    view.* = std.mem.zeroes(View);
    const root_mem = malloc(@sizeOf(WindowNode)) orelse abort();
    const root: *WindowNode = @ptrCast(@alignCast(root_mem));
    root.* = std.mem.zeroes(WindowNode);
    view.root = root;
    view.sid = sid;
    view.uuid = uuid;
    view.flags |= abi.VIEW_ENABLE_PADDING;
    view.flags |= abi.VIEW_ENABLE_GAP;
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
        view.layout = abi.VIEW_FLOAT;
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

// Order mirrors view_type_str in src/view.h; indexed by view.layout
// exactly like C.
const view_type_str = [_][*:0]const u8{ "default", "bsp", "stack", "float" };

extern "c" fn snprintf(buf: [*]u8, n: usize, fmt: [*:0]const u8, ...) c_int;
extern "c" fn fwrite(buf: *const anyopaque, size: usize, nmemb: usize, stream: ?*anyopaque) usize;

fn viewStrLenZ(s: [*:0]const u8) usize {
    @setRuntimeSafety(false);
    var len: usize = 0;
    while (s[len] != 0) : (len += 1) {}
    return len;
}

fn viewWriteBytes(rsp: ?*anyopaque, s: []const u8) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    if (s.len == 0) return;
    _ = fwrite(@ptrCast(s.ptr), 1, s.len, stream);
}

fn viewWriteZ(rsp: ?*anyopaque, s: [*:0]const u8) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    const len = viewStrLenZ(s);
    if (len == 0) return;
    _ = fwrite(@ptrCast(s), 1, len, stream);
}

fn viewWriteInt(rsp: ?*anyopaque, v: c_int) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    var tmp: [32]u8 = undefined;
    const n = snprintf(@ptrCast(&tmp), tmp.len, "%d", v);
    if (n <= 0) return;
    _ = fwrite(@ptrCast(&tmp), 1, @intCast(n), stream);
}

fn viewWriteSid(rsp: ?*anyopaque, v: u64) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    var tmp: [32]u8 = undefined;
    const n = snprintf(@ptrCast(&tmp), tmp.len, "%lld", @as(c_longlong, @bitCast(v)));
    if (n <= 0) return;
    _ = fwrite(@ptrCast(&tmp), 1, @intCast(n), stream);
}

// Mirrors view_serialize() in src/view.c. The eight manager-derived
// inputs arrive lifted in (established seam pattern): uuid_str is the
// ts_cfstring_copy result (NULL when the copy would yield NULL),
// mission_index replaces space_manager_mission_control_index,
// space_label_str replaces space_manager_get_label_for_space (NULL when
// absent), display_index replaces
// display_manager_display_id_arrangement(space_display_id(...)),
// window_list/window_count replace space_window_list, and the three
// bools replace the focus/visible/fullscreen derivations. First/last
// leaves reuse the already-ported find_first/last_leaf, bools reuse
// yabai_json_bool, and every byte goes out via libc fwrite/snprintf.
// The trailing is-native-fullscreen branch omits did_output=true,
// exactly like C.
export fn yabai_view_serialize(rsp: ?*anyopaque, view: *const View, flags: u64, uuid_str: ?[*:0]const u8, mission_index: c_int, space_label_str: ?[*:0]const u8, display_index: c_int, window_list: ?[*]const u32, window_count: c_int, has_focus: bool, is_visible: bool, is_fullscreen: bool) void {
    @setRuntimeSafety(false);
    const stream = rsp orelse return;
    var f = flags;
    if (f == 0x0) f |= ~f;

    var did_output = false;
    viewWriteBytes(stream, "{\n");

    if ((f & abi.SPACE_PROPERTY_ID) != 0) {
        viewWriteBytes(stream, "\t\"id\":");
        viewWriteSid(stream, view.sid);
        did_output = true;
    }

    if ((f & abi.SPACE_PROPERTY_UUID) != 0) {
        if (did_output) viewWriteBytes(stream, ",\n");
        viewWriteBytes(stream, "\t\"uuid\":\"");
        viewWriteZ(stream, if (uuid_str) |s| s else "<unknown>");
        viewWriteBytes(stream, "\"");
        did_output = true;
    }

    if ((f & abi.SPACE_PROPERTY_INDEX) != 0) {
        if (did_output) viewWriteBytes(stream, ",\n");
        viewWriteBytes(stream, "\t\"index\":");
        viewWriteInt(stream, mission_index);
        did_output = true;
    }

    if ((f & abi.SPACE_PROPERTY_LABEL) != 0) {
        if (did_output) viewWriteBytes(stream, ",\n");
        viewWriteBytes(stream, "\t\"label\":\"");
        viewWriteZ(stream, if (space_label_str) |s| s else "");
        viewWriteBytes(stream, "\"");
        did_output = true;
    }

    if ((f & abi.SPACE_PROPERTY_TYPE) != 0) {
        if (did_output) viewWriteBytes(stream, ",\n");
        viewWriteBytes(stream, "\t\"type\":\"");
        viewWriteZ(stream, view_type_str[@intCast(view.layout)]);
        viewWriteBytes(stream, "\"");
        did_output = true;
    }

    if ((f & abi.SPACE_PROPERTY_DISPLAY) != 0) {
        if (did_output) viewWriteBytes(stream, ",\n");
        viewWriteBytes(stream, "\t\"display\":");
        viewWriteInt(stream, display_index);
        did_output = true;
    }

    if ((f & abi.SPACE_PROPERTY_WINDOWS) != 0) {
        if (did_output) viewWriteBytes(stream, ",\n");
        viewWriteBytes(stream, "\t\"windows\":[");
        if (window_count > 0) {
            const list = window_list orelse unreachable;
            var i: c_int = 0;
            while (i < window_count) : (i += 1) {
                const wid: c_int = @bitCast(list[@intCast(i)]);
                if (i < window_count - 1) {
                    viewWriteInt(stream, wid);
                    viewWriteBytes(stream, ", ");
                } else {
                    viewWriteInt(stream, wid);
                }
            }
        }
        viewWriteBytes(stream, "]");
        did_output = true;
    }

    if ((f & abi.SPACE_PROPERTY_FIRST_WINDOW) != 0) {
        if (did_output) viewWriteBytes(stream, ",\n");
        const first_leaf = yabai_window_node_find_first_leaf(view.root orelse unreachable);
        const first: c_int = @bitCast(if (first_leaf) |l| l.window_order[0] else 0);
        viewWriteBytes(stream, "\t\"first-window\":");
        viewWriteInt(stream, first);
        did_output = true;
    }

    if ((f & abi.SPACE_PROPERTY_LAST_WINDOW) != 0) {
        if (did_output) viewWriteBytes(stream, ",\n");
        const last_leaf = yabai_window_node_find_last_leaf(view.root orelse unreachable);
        const last: c_int = @bitCast(if (last_leaf) |l| l.window_order[0] else 0);
        viewWriteBytes(stream, "\t\"last-window\":");
        viewWriteInt(stream, last);
        did_output = true;
    }

    if ((f & abi.SPACE_PROPERTY_HAS_FOCUS) != 0) {
        if (did_output) viewWriteBytes(stream, ",\n");
        viewWriteBytes(stream, "\t\"has-focus\":");
        viewWriteZ(stream, abi.yabai_json_bool(has_focus));
        did_output = true;
    }

    if ((f & abi.SPACE_PROPERTY_IS_VISIBLE) != 0) {
        if (did_output) viewWriteBytes(stream, ",\n");
        viewWriteBytes(stream, "\t\"is-visible\":");
        viewWriteZ(stream, abi.yabai_json_bool(is_visible));
        did_output = true;
    }

    if ((f & abi.SPACE_PROPERTY_IS_FULLSCREEN) != 0) {
        if (did_output) viewWriteBytes(stream, ",\n");
        viewWriteBytes(stream, "\t\"is-native-fullscreen\":");
        viewWriteZ(stream, abi.yabai_json_bool(is_fullscreen));
    }

    viewWriteBytes(stream, "\n}");
}
