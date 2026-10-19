/* FROZEN REFERENCE: verbatim copies of the upstream C bodies at commit
 * dd84572 (git show dd84572:src/view.c, :src/window.c,
 * :src/window_manager.c, :src/mouse_handler.c), renamed with a ref_
 * prefix and made static.
 *
 * Why this file exists: tests.m includes src/manifest.m, and the routed
 * C functions in src/ now just call the Zig exports, so comparing
 * c_fn(x) against yabai_fn(x) would run Zig on both sides and prove
 * nothing. Every parity helper in zig_view.c, zig_window.c, zig_mouse.c
 * and zig_geometry.c compares against these ref_ copies instead. Calls
 * between routed functions go to the ref_ copies too, so the whole
 * reference chain is original C. Header macros/inlines (view_check_flag,
 * in_range_ii, max, triangle_contains_point, CGRectContainsPoint) are
 * unchanged since dd84572 and are used as-is.
 *
 * NEVER edit these bodies to match Zig. If a parity test fails, the Zig
 * port is wrong (or the divergence is a deliberate, documented choice
 * that belongs in the test, not here).
 *
 * Seams (the only departures from verbatim, each forced by SLS calls
 * that abort headless with g_connection == 0):
 *  - ref_view_find_window_node_in_direction takes the space_window_list
 *    result as window_list/window_count parameters (the NULL early-out is
 *    kept).
 *  - ref_view_create takes the SLSSpaceCopyName uuid and space_is_user
 *    result as parameters and skips the trailing view_update.
 *  - ref_view_clear skips the trailing view_update.
 *  - ref_move_frame_math / ref_resize_frame_math are the pure frame-math
 *    statements of window_manager_move_window_relative and
 *    window_manager_resize_window_relative_internal, with the AX/animate
 *    side effects replaced by out-params.
 */

/* ---- src/view.c @ dd84572 ---- */

static CGPoint ref_area_max_point(struct area area)
{
    return (CGPoint) { area.x + area.w - 1, area.y + area.h - 1 };
}

static enum window_node_split ref_window_node_get_split(struct view *view, struct window_node *node)
{
    if (node->split != SPLIT_NONE) return node->split;

    if (view->split_type != SPLIT_NONE) {
        if (view->split_type != SPLIT_AUTO) {
            return view->split_type;
        }
    } else if (g_space_manager.split_type != SPLIT_AUTO) {
        return g_space_manager.split_type;
    }

    return node->area.w >= node->area.h ? SPLIT_Y : SPLIT_X;
}

static float ref_window_node_get_ratio(struct window_node *node)
{
    return in_range_ii(node->ratio, 0.1f, 0.9f) ? node->ratio : g_space_manager.split_ratio;
}

static int ref_window_node_get_gap(struct view *view)
{
    return view_check_flag(view, VIEW_ENABLE_GAP) ? view->window_gap : 0;
}

static void ref_area_make_pair(enum window_node_split split, int gap, float ratio, struct area *parent_area, struct area *left_area, struct area *right_area)
{
    if (split == SPLIT_Y) {
        *left_area  = *parent_area;
        *right_area = *parent_area;

        float left_width  = (parent_area->w - gap) * ratio;
        float right_width = (parent_area->w - gap) * (1 - ratio);

        left_area->w   = (int)left_width;
        right_area->w  = (int)right_width;
        right_area->x += (int)(left_width + 0.5f) + gap;
    } else {
        *left_area  = *parent_area;
        *right_area = *parent_area;

        float left_width  = (parent_area->h - gap) * ratio;
        float right_width = (parent_area->h - gap) * (1 - ratio);

        left_area->h   = (int)left_width;
        right_area->h  = (int)right_width;
        right_area->y += (int)(left_width + 0.5f) + gap;
    }
}

static bool ref_window_node_is_leaf(struct window_node *node)
{
    return node->left == NULL && node->right == NULL;
}

static bool ref_window_node_is_left_child(struct window_node *node)
{
    return node->parent && node->parent->left == node;
}

static bool ref_window_node_is_right_child(struct window_node *node)
{
    return node->parent && node->parent->right == node;
}

static void ref_window_node_destroy(struct window_node *node)
{
    if (node->left)  ref_window_node_destroy(node->left);
    if (node->right) ref_window_node_destroy(node->right);

    for (int i = 0; i < node->window_count; ++i) {
        window_manager_remove_managed_window(&g_window_manager, node->window_list[i]);
    }

    insert_feedback_destroy(node);
    free(node);
}

static bool ref_window_node_contains_window(struct window_node *node, uint32_t window_id)
{
    for (int i = 0; i < node->window_count; ++i) {
        if (node->window_list[i] == window_id) return true;
    }

    return false;
}

static int ref_window_node_index_of_window(struct window_node *node, uint32_t window_id)
{
    for (int i = 0; i < node->window_count; ++i) {
        if (node->window_list[i] == window_id) return i;
    }

    return 0;
}

static void ref_window_node_swap_window_list(struct window_node *a_node, struct window_node *b_node)
{
    uint32_t tmp_window_list[NODE_MAX_WINDOW_COUNT];
    uint32_t tmp_window_order[NODE_MAX_WINDOW_COUNT];
    uint32_t tmp_window_count;

    memcpy(tmp_window_list, a_node->window_list, sizeof(uint32_t) * a_node->window_count);
    memcpy(tmp_window_order, a_node->window_order, sizeof(uint32_t) * a_node->window_count);
    tmp_window_count = a_node->window_count;

    memcpy(a_node->window_list, b_node->window_list, sizeof(uint32_t) * b_node->window_count);
    memcpy(a_node->window_order, b_node->window_order, sizeof(uint32_t) * b_node->window_count);
    a_node->window_count = b_node->window_count;

    memcpy(b_node->window_list, tmp_window_list, sizeof(uint32_t) * tmp_window_count);
    memcpy(b_node->window_order, tmp_window_order, sizeof(uint32_t) * tmp_window_count);
    b_node->window_count = tmp_window_count;

    a_node->zoom = NULL;
    b_node->zoom = NULL;
}

static struct window_node *ref_window_node_find_first_leaf(struct window_node *root)
{
    struct window_node *node = root;
    while (!ref_window_node_is_leaf(node)) {
        node = node->left;
    }
    return node;
}

static struct window_node *ref_window_node_find_last_leaf(struct window_node *root)
{
    struct window_node *node = root;
    while (!ref_window_node_is_leaf(node)) {
        node = node->right;
    }
    return node;
}

static struct window_node *ref_window_node_find_prev_leaf(struct window_node *node)
{
    if (!node->parent) return NULL;

    if (ref_window_node_is_left_child(node)) {
        return ref_window_node_find_prev_leaf(node->parent);
    }

    if (ref_window_node_is_leaf(node->parent->left)) {
        return node->parent->left;
    }

    return ref_window_node_find_last_leaf(node->parent->left->right);
}

static struct window_node *ref_window_node_find_next_leaf(struct window_node *node)
{
    if (!node->parent) return NULL;

    if (ref_window_node_is_right_child(node)) {
        return ref_window_node_find_next_leaf(node->parent);
    }

    if (ref_window_node_is_leaf(node->parent->right)) {
        return node->parent->right;
    }

    return ref_window_node_find_first_leaf(node->parent->right->left);
}

static void ref_window_node_rotate(struct window_node *node, int degrees)
{
    if ((degrees ==  90 && node->split == SPLIT_Y) ||
        (degrees == 270 && node->split == SPLIT_X) ||
        (degrees == 180)) {
        struct window_node *temp = node->left;
        node->left  = node->right;
        node->right = temp;
        node->ratio = 1 - node->ratio;
    }

    if (degrees != 180) {
        if (node->split == SPLIT_X) {
            node->split = SPLIT_Y;
        } else if (node->split == SPLIT_Y) {
            node->split = SPLIT_X;
        }
    }

    if (!ref_window_node_is_leaf(node)) {
        ref_window_node_rotate(node->left, degrees);
        ref_window_node_rotate(node->right, degrees);
    }
}

static struct window_node *ref_window_node_mirror(struct window_node *node, enum window_node_split axis)
{
    if (!ref_window_node_is_leaf(node)) {
        struct window_node *left = ref_window_node_mirror(node->left, axis);
        struct window_node *right = ref_window_node_mirror(node->right, axis);

        if (node->split == axis) {
            node->left = right;
            node->right = left;
        }
    }

    return node;
}

static struct window_node *ref_window_node_fence(struct window_node *node, int dir)
{
    if (!node) return NULL;

    for (struct window_node *parent = node->parent; parent; parent = parent->parent) {
        if ((dir == DIR_NORTH && parent->split == SPLIT_X && parent->area.y < node->area.y) ||
            (dir == DIR_WEST  && parent->split == SPLIT_Y && parent->area.x < node->area.x) ||
            (dir == DIR_SOUTH && parent->split == SPLIT_X && (parent->area.y + parent->area.h) > (node->area.y + node->area.h)) ||
            (dir == DIR_EAST  && parent->split == SPLIT_Y && (parent->area.x + parent->area.w) > (node->area.x + node->area.w))) {
                return parent;
        }
    }

    return NULL;
}

static bool ref_area_is_in_direction(struct area *r1, CGPoint r1_max, struct area *r2, CGPoint r2_max, int direction)
{
    if (direction == DIR_NORTH && r1_max.y <= r2->y) return false;
    if (direction == DIR_EAST  && r2_max.x <= r1->x) return false;
    if (direction == DIR_SOUTH && r2_max.y <= r1->y) return false;
    if (direction == DIR_WEST  && r1_max.x <= r2->x) return false;

    if (direction == DIR_NORTH || direction == DIR_SOUTH) {
        return ((r2_max.x >  r1->x && r2_max.x <= r1_max.x) ||
                (r2->x    <  r1->x && r2_max.x >  r1_max.x) ||
                (r2->x    >= r1->x && r2->x    <  r1_max.x));
    }

    if (direction == DIR_EAST || direction == DIR_WEST) {
        return ((r2_max.y >  r1->y && r2_max.y <= r1_max.y) ||
                (r2->y    <  r1->y && r2_max.y >  r1_max.y) ||
                (r2->y    >= r1->y && r2->y    <  r1_max.y));
    }

    return false;
}

static int ref_area_distance_in_direction(struct area *r1, CGPoint r1_max, struct area *r2, CGPoint r2_max, int direction)
{
    switch (direction) {
    case DIR_NORTH: {
        return r2_max.y > r1->y ? r2_max.y - r1->y : r1->y - r2_max.y;
    } break;
    case DIR_EAST: {
        return r2->x < r1_max.x ? r1_max.x - r2->x : r2->x - r1_max.x;
    } break;
    case DIR_SOUTH: {
        return r2->y < r1_max.y ? r1_max.y - r2->y : r2->y - r1_max.y;
    } break;
    case DIR_WEST: {
        return r2_max.x > r1->x ? r2_max.x - r1->x : r1->x - r2_max.x;
    } break;
    }

    return INT_MAX;
}

/* ---- src/window_manager.c @ dd84572 ---- */

static int ref_window_manager_find_rank_of_window_in_list(uint32_t wid, uint32_t *window_list, int window_count)
{
    for (int i = 0, rank = 0; i < window_count; ++i) {
        if (window_list[i] == wid) {
            return rank;
        } else {
            ++rank;
        }
    }

    return INT_MAX;
}

/* window_manager_move_window_relative: the TYPE_REL adjust and the
 * capture fields it animates to (.x = dx, .y = dy, .w/.h = frame size). */
static void ref_move_frame_math(CGRect frame, int type, float dx, float dy, float *x, float *y, float *w, float *h)
{
    if (type == TYPE_REL) {
        dx += frame.origin.x;
        dy += frame.origin.y;
    }

    *x = dx;
    *y = dy;
    *w = frame.size.width;
    *h = frame.size.height;
}

/* window_manager_resize_window_relative_internal: the frame math before
 * the animate/AX branch. */
static void ref_resize_frame_math(CGRect frame, int direction, float dx, float dy, float *out_fx, float *out_fy, float *out_fw, float *out_fh)
{
    int x_mod = (direction & HANDLE_LEFT) ? -1 : (direction & HANDLE_RIGHT)  ? 1 : 0;
    int y_mod = (direction & HANDLE_TOP)  ? -1 : (direction & HANDLE_BOTTOM) ? 1 : 0;

    float fw = max(1, frame.size.width  + dx * x_mod);
    float fh = max(1, frame.size.height + dy * y_mod);
    float fx = (direction & HANDLE_LEFT) ? frame.origin.x + frame.size.width  - fw : frame.origin.x;
    float fy = (direction & HANDLE_TOP)  ? frame.origin.y + frame.size.height - fh : frame.origin.y;

    *out_fx = fx;
    *out_fy = fy;
    *out_fw = fw;
    *out_fh = fh;
}

/* ---- src/view.c @ dd84572 (continued) ---- */

/* Seam: space_window_list(view->sid, &window_count, false) arrives as
 * window_list/window_count. */
static struct window_node *ref_view_find_window_node_in_direction(struct view *view, struct window_node *source, int direction, uint32_t *window_list, int window_count)
{
    if (!window_list) return NULL;

    int best_distance = INT_MAX;
    int best_rank = INT_MAX;
    struct window_node *best_node = NULL;
    CGPoint source_area_max = ref_area_max_point(source->area);

    for (struct window_node *target = ref_window_node_find_first_leaf(view->root); target; target = ref_window_node_find_next_leaf(target)) {
        if (source == target) continue;

        CGPoint target_area_max = ref_area_max_point(target->area);
        if (ref_area_is_in_direction(&source->area, source_area_max, &target->area, target_area_max, direction)) {
            int distance = ref_area_distance_in_direction(&source->area, source_area_max, &target->area, target_area_max, direction);
            int rank = ref_window_manager_find_rank_of_window_in_list(target->window_order[0], window_list, window_count);
            if ((distance < best_distance) || (distance == best_distance && rank < best_rank)) {
                best_node = target;
                best_distance = distance;
                best_rank = rank;
            }
        }
    }

    return best_node;
}

static struct window_node *ref_view_find_window_node(struct view *view, uint32_t window_id)
{
    for (struct window_node *node = ref_window_node_find_first_leaf(view->root); node; node = ref_window_node_find_next_leaf(node)) {
        if (ref_window_node_contains_window(node, window_id)) return node;
    }

    return NULL;
}

static void ref_view_stack_window_node(struct window_node *node, struct window *window)
{
    int insert_index = node->window_count;

    for (int i = 0; i < node->window_count; ++i) {
        if (node->window_list[i] == node->window_order[0]) {
            insert_index = i+1;
            break;
        }
    }

    if (insert_index < node->window_count) {
        memmove(node->window_list + insert_index + 1, node->window_list + insert_index, sizeof(uint32_t) * (node->window_count - insert_index));
    }

    node->window_list[insert_index] = window->id;
    memmove(node->window_order + 1, node->window_order, sizeof(uint32_t) * node->window_count);
    node->window_order[0] = window->id;
    ++node->window_count;
}

/* Returns temp-pool memory (ts_alloc_list), exactly like upstream: the
 * caller must NOT free it. */
static uint32_t *ref_view_find_window_list(struct view *view, int *window_count)
{
    *window_count = 0;

    int capacity = 13;
    uint32_t *window_list = ts_alloc_list(uint32_t, capacity);

    for (struct window_node *node = ref_window_node_find_first_leaf(view->root); node; node = ref_window_node_find_next_leaf(node)) {
        if (*window_count + node->window_count >= capacity) {
            ts_expand(window_list, sizeof(uint32_t) * capacity, sizeof(uint32_t) * capacity);
            capacity *= 2;
        }

        for (int i = 0; i < node->window_count; ++i) {
            window_list[(*window_count)++] = node->window_list[i];
        }
    }

    return window_list;
}

static bool ref_view_is_invalid(struct view *view)
{
    return !view_check_flag(view, VIEW_IS_VALID);
}

static bool ref_view_is_dirty(struct view *view)
{
    return view_check_flag(view, VIEW_IS_DIRTY);
}

/* Verbatim. The INDEX, DISPLAY, WINDOWS, IS_VISIBLE and IS_FULLSCREEN
 * branches call SLS and abort headless; callers only pass SLS-free flags. */
static void ref_view_serialize(FILE *rsp, struct view *view, uint64_t flags)
{
    TIME_FUNCTION;

    if (flags == 0x0) flags |= ~flags;

    bool did_output = false;
    fprintf(rsp, "{\n");

    if (flags & SPACE_PROPERTY_ID) {
        fprintf(rsp, "\t\"id\":%lld", view->sid);
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_UUID) {
        if (did_output) fprintf(rsp, ",\n");

        char *uuid = ts_cfstring_copy(view->uuid);
        fprintf(rsp, "\t\"uuid\":\"%s\"", uuid ? uuid : "<unknown>");
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_INDEX) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"index\":%d", space_manager_mission_control_index(view->sid));
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_LABEL) {
        if (did_output) fprintf(rsp, ",\n");

        struct space_label *space_label = space_manager_get_label_for_space(&g_space_manager, view->sid);
        fprintf(rsp, "\t\"label\":\"%s\"", space_label ? space_label->label : "");
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_TYPE) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"type\":\"%s\"", view_type_str[view->layout]);
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_DISPLAY) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"display\":%d", display_manager_display_id_arrangement(space_display_id(view->sid)));
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_WINDOWS) {
        if (did_output) fprintf(rsp, ",\n");

        int window_count = 0;
        uint32_t *window_list = space_window_list(view->sid, &window_count, true);

        fprintf(rsp, "\t\"windows\":[");
        for (int i = 0; i < window_count; ++i) {
            if (i < window_count - 1) {
                fprintf(rsp, "%d, ", window_list[i]);
            } else {
                fprintf(rsp, "%d", window_list[i]);
            }
        }
        fprintf(rsp, "]");
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_FIRST_WINDOW) {
        if (did_output) fprintf(rsp, ",\n");

        struct window_node *first_leaf = ref_window_node_find_first_leaf(view->root);
        fprintf(rsp, "\t\"first-window\":%d", first_leaf ? first_leaf->window_order[0] : 0);
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_LAST_WINDOW) {
        if (did_output) fprintf(rsp, ",\n");

        struct window_node *last_leaf = ref_window_node_find_last_leaf(view->root);
        fprintf(rsp, "\t\"last-window\":%d", last_leaf ? last_leaf->window_order[0] : 0);
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_HAS_FOCUS) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"has-focus\":%s", json_bool(view->sid == g_space_manager.current_space_id));
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_IS_VISIBLE) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"is-visible\":%s", json_bool(space_is_visible(view->sid)));
        did_output = true;
    }

    if (flags & SPACE_PROPERTY_IS_FULLSCREEN) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"is-native-fullscreen\":%s", json_bool(space_is_fullscreen(view->sid)));
    }

    fprintf(rsp, "\n}");
}

/* Seam: uuid = SLSSpaceCopyName(g_connection, sid) and
 * is_user = space_is_user(view->sid) arrive as parameters; the
 * user-branch view_update (SLS display bounds) is skipped. */
static struct view *ref_view_create(uint64_t sid, CFStringRef uuid, bool is_user)
{
    struct view *view = malloc(sizeof(struct view));
    memset(view, 0, sizeof(struct view));

    view->root = malloc(sizeof(struct window_node));
    memset(view->root, 0, sizeof(struct window_node));

    view->sid = sid;
    view->uuid = uuid;

    view_set_flag(view, VIEW_ENABLE_PADDING);
    view_set_flag(view, VIEW_ENABLE_GAP);

    if (is_user) {
        if (!view_check_flag(view, VIEW_LAYOUT))         view->layout         = g_space_manager.layout;
        if (!view_check_flag(view, VIEW_TOP_PADDING))    view->top_padding    = g_space_manager.top_padding;
        if (!view_check_flag(view, VIEW_BOTTOM_PADDING)) view->bottom_padding = g_space_manager.bottom_padding;
        if (!view_check_flag(view, VIEW_LEFT_PADDING))   view->left_padding   = g_space_manager.left_padding;
        if (!view_check_flag(view, VIEW_RIGHT_PADDING))  view->right_padding  = g_space_manager.right_padding;
        if (!view_check_flag(view, VIEW_WINDOW_GAP))     view->window_gap     = g_space_manager.window_gap;
        if (!view_check_flag(view, VIEW_AUTO_BALANCE))   view->auto_balance   = g_space_manager.auto_balance;
        if (!view_check_flag(view, VIEW_SPLIT_TYPE))     view->split_type     = g_space_manager.split_type;
    } else {
        view->layout = VIEW_FLOAT;
    }

    return view;
}

/* Seam: the trailing view_update (SLS display bounds) is skipped. */
static void ref_view_clear(struct view *view)
{
    if (view->root) {
        if (view->root->left)  ref_window_node_destroy(view->root->left);
        if (view->root->right) ref_window_node_destroy(view->root->right);

        for (int i = 0; i < view->root->window_count; ++i) {
            window_manager_remove_managed_window(&g_window_manager, view->root->window_list[i]);
        }

        insert_feedback_destroy(view->root);
        memset(view->root, 0, sizeof(struct window_node));
    }
}

static void ref_view_destroy(struct view *view)
{
    if (view->root) {
        ref_window_node_destroy(view->root);
    }

    if (view->uuid) {
        CFRelease(view->uuid);
    }
}

/* ---- src/window.c @ dd84572 ---- */

static bool ref_window_can_move(struct window *window)
{
    return window_check_flag(window, WINDOW_MOVABLE);
}

static bool ref_window_can_resize(struct window *window)
{
    return window_check_flag(window, WINDOW_RESIZABLE);
}

static bool ref_window_is_undersized(struct window *window)
{
    if (window->frame.size.width  <= 500.0f) return true;
    if (window->frame.size.height <= 500.0f) return true;
    return false;
}

/* ---- src/mouse_handler.c @ dd84572 ---- */

static enum mouse_drop_action ref_mouse_determine_drop_action(struct mouse_state *ms, struct window_node *src_node, struct window *dst_window, CGPoint point)
{
    CGRect  f    = dst_window->frame;
    CGPoint wp   = { point.x - f.origin.x, point.y - f.origin.y };
    CGRect  c    = {{ 0.25f * f.size.width, 0.25f * f.size.height }, { 0.50f * f.size.width, 0.50f * f.size.height }};
    CGPoint t[3] = {{ 0.0f, 0.0f}, { 0.5f * f.size.width, 0.5f * f.size.height }, { f.size.width, 0.0f }};
    CGPoint r[3] = {{ f.size.width, 0.0f }, { 0.5f * f.size.width, 0.5f * f.size.height }, { f.size.width, f.size.height }};
    CGPoint b[3] = {{ f.size.width, f.size.height }, { 0.5f * f.size.width, 0.5f * f.size.height }, { 0.0f, f.size.height }};
    CGPoint l[3] = {{ 0.0f, f.size.height }, { 0.5f * f.size.width, 0.5f * f.size.height }, { 0.0f, 0.0f }};

    if ((CGRectContainsPoint(c, wp)) && (src_node->window_count == 1)) {
        return ms->drop_action == MOUSE_MODE_STACK ? MOUSE_DROP_ACTION_STACK : MOUSE_DROP_ACTION_SWAP;
    } else if (triangle_contains_point(t, wp)) {
        return MOUSE_DROP_ACTION_WARP_TOP;
    } else if (triangle_contains_point(r, wp)) {
        return MOUSE_DROP_ACTION_WARP_RIGHT;
    } else if (triangle_contains_point(b, wp)) {
        return MOUSE_DROP_ACTION_WARP_BOTTOM;
    } else if (triangle_contains_point(l, wp)) {
        return MOUSE_DROP_ACTION_WARP_LEFT;
    }

    return MOUSE_DROP_ACTION_NONE;
}
