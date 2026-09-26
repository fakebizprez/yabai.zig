/* View-tree traversal parity tests (doc/zig-refactor-plan.md step 3).
 *
 * The Zig exports in zig/view.zig must match the pure tree helpers in
 * src/view.c exactly. Layout contract: struct window_node is 344 bytes
 * with the offsets below (verified with clang -fdump-record-layouts);
 * both sides assert it.
 */

bool yabai_window_node_is_leaf(struct window_node *node);
bool yabai_window_node_is_left_child(struct window_node *node);
bool yabai_window_node_is_right_child(struct window_node *node);
struct window_node *yabai_window_node_find_first_leaf(struct window_node *root);
struct window_node *yabai_window_node_find_last_leaf(struct window_node *root);
struct window_node *yabai_window_node_find_prev_leaf(struct window_node *node);
struct window_node *yabai_window_node_find_next_leaf(struct window_node *node);
bool yabai_window_node_contains_window(struct window_node *node, uint32_t window_id);
int yabai_window_node_index_of_window(struct window_node *node, uint32_t window_id);
void yabai_window_node_rotate(struct window_node *node, int degrees);
struct window_node *yabai_window_node_mirror(struct window_node *node, int axis);
struct window_node *yabai_window_node_fence(struct window_node *node, int dir);
void yabai_view_stack_window(struct window_node *node, uint32_t window_id);
bool yabai_window_node_is_occupied(struct window_node *node);
bool yabai_window_node_is_intermediate(struct window_node *node);
int yabai_window_node_get_child(struct window_node *node, int window_placement);
int yabai_window_node_get_split(struct view *view, struct window_node *node, int space_split_type);
float yabai_window_node_get_ratio(struct window_node *node, float space_split_ratio);
int yabai_window_node_get_gap(struct view *view);
void yabai_area_from_cgrect(const CGRect *rect, struct area *out);
void yabai_area_max_point(const struct area *area, CGPoint *out);
void yabai_window_node_equalize(struct window_node *node, uint32_t axis_flag, float space_split_ratio);
void yabai_window_node_balance(struct window_node *node, uint32_t axis_flag, struct balance_node *out);
void yabai_area_make_pair_for_node(struct view *view, struct window_node *node, int space_split_type, float space_split_ratio);
void yabai_window_node_split(struct view *view, struct window_node *node, uint32_t window_id, bool window_zoom_persist, int window_placement, int space_split_type, float space_split_ratio);
void yabai_window_node_swap_window_list(struct window_node *a_node, struct window_node *b_node);
struct window_node *yabai_view_find_window_node_in_direction(const struct yabai_view *view, struct window_node *source, int direction, const uint32_t *window_list, int window_count);

struct yabai_view
{
    void *uuid;
    uint64_t sid;
    struct window_node *root;
    uint32_t insertion_point;
    int layout;
    int split_type;
    int top_padding;
    int bottom_padding;
    int left_padding;
    int right_padding;
    int window_gap;
    uint32_t auto_balance;
    uint64_t flags;
};

struct window_node *yabai_view_find_window_node(const struct yabai_view *view, uint32_t window_id);

_Static_assert(sizeof(struct view) == 72, "C view size drift");
_Static_assert(sizeof(struct yabai_view) == 72, "view size drift");
_Static_assert(offsetof(struct view, root) == 16, "C view.root drift");

_Static_assert(sizeof(struct window_node) == 344, "window_node size drift");
_Static_assert(sizeof(struct feedback_window) == 16, "feedback_window size drift");
_Static_assert(offsetof(struct window_node, left) == 24, "window_node.left drift");
_Static_assert(offsetof(struct window_node, window_list) == 48, "window_node.window_list drift");
_Static_assert(offsetof(struct window_node, window_count) == 304, "window_node.window_count drift");
_Static_assert(offsetof(struct window_node, feedback_window) == 328, "window_node.feedback_window drift");

static struct window_node *make_test_node(void)
{
    struct window_node *node = malloc(sizeof(struct window_node));
    memset(node, 0, sizeof(struct window_node));
    return node;
}

static void set_test_windows(struct window_node *node, uint32_t first, uint32_t second, int count)
{
    node->window_list[0] = first;
    node->window_list[1] = second;
    node->window_count = count;
}

/* Builds: R -> (A, C); A -> (D, E). D:{5,9}, C:{7}, others empty. */
static struct window_node *make_test_tree(struct window_node **r, struct window_node **a, struct window_node **c, struct window_node **d, struct window_node **e)
{
    *r = make_test_node();
    *a = make_test_node();
    *c = make_test_node();
    *d = make_test_node();
    *e = make_test_node();

    (*r)->left = *a;
    (*r)->right = *c;
    (*a)->parent = *r;
    (*c)->parent = *r;
    (*a)->left = *d;
    (*a)->right = *e;
    (*d)->parent = *a;
    (*e)->parent = *a;

    set_test_windows(*d, 5, 9, 2);
    set_test_windows(*c, 7, 0, 1);

    return *r;
}

static void free_test_tree(struct window_node *r, struct window_node *a, struct window_node *c, struct window_node *d, struct window_node *e)
{
    free(r);
    free(a);
    free(c);
    free(d);
    free(e);
}

TEST_FUNC(zig_view_leaf_walk,
{
    struct window_node *r;
    struct window_node *a;
    struct window_node *c;
    struct window_node *d;
    struct window_node *e;
    make_test_tree(&r, &a, &c, &d, &e);

    TEST_CHECK(window_node_is_leaf(r) != yabai_window_node_is_leaf(r), 0);
    TEST_CHECK(window_node_is_leaf(d) != yabai_window_node_is_leaf(d), 0);
    TEST_CHECK(window_node_is_left_child(d) != yabai_window_node_is_left_child(d), 0);
    TEST_CHECK(window_node_is_right_child(e) != yabai_window_node_is_right_child(e), 0);
    TEST_CHECK(window_node_is_left_child(e) != yabai_window_node_is_left_child(e), 0);

    TEST_CHECK((void *) window_node_find_first_leaf(r) != (void *) yabai_window_node_find_first_leaf(r), 0);
    TEST_CHECK((void *) window_node_find_first_leaf(r) != (void *) d, 0);
    TEST_CHECK((void *) yabai_window_node_find_first_leaf(r) != (void *) d, 0);
    TEST_CHECK((void *) window_node_find_last_leaf(r) != (void *) yabai_window_node_find_last_leaf(r), 0);
    TEST_CHECK((void *) window_node_find_last_leaf(r) != (void *) c, 0);
    TEST_CHECK((void *) yabai_window_node_find_last_leaf(r) != (void *) c, 0);

    TEST_CHECK((void *) window_node_find_prev_leaf(e) != (void *) yabai_window_node_find_prev_leaf(e), 0);
    TEST_CHECK((void *) window_node_find_prev_leaf(e) != (void *) d, 0);
    TEST_CHECK((void *) yabai_window_node_find_prev_leaf(e) != (void *) d, 0);
    TEST_CHECK((void *) window_node_find_prev_leaf(d) != (void *) yabai_window_node_find_prev_leaf(d), 0);
    /* Hand-pinned boundary: first leaf in walk order has no predecessor. */
    TEST_CHECK(window_node_find_prev_leaf(d), NULL);
    TEST_CHECK(yabai_window_node_find_prev_leaf(d), NULL);
    TEST_CHECK((void *) window_node_find_next_leaf(d) != (void *) yabai_window_node_find_next_leaf(d), 0);
    TEST_CHECK((void *) window_node_find_next_leaf(d) != (void *) e, 0);
    TEST_CHECK((void *) yabai_window_node_find_next_leaf(d) != (void *) e, 0);
    TEST_CHECK((void *) window_node_find_next_leaf(e) != (void *) yabai_window_node_find_next_leaf(e), 0);
    TEST_CHECK((void *) window_node_find_next_leaf(e) != (void *) c, 0);
    TEST_CHECK((void *) yabai_window_node_find_next_leaf(e) != (void *) c, 0);
    TEST_CHECK((void *) window_node_find_next_leaf(c) != (void *) yabai_window_node_find_next_leaf(c), 0);
    /* Hand-pinned boundary: last leaf in walk order has no successor. */
    TEST_CHECK(window_node_find_next_leaf(c), NULL);
    TEST_CHECK(yabai_window_node_find_next_leaf(c), NULL);

    /* Hand-pinned vector: degenerate single-node tree is its own
     * first and last leaf on both sides. */
    struct window_node *solo_leaf = make_test_node();
    TEST_CHECK((void *) window_node_find_first_leaf(solo_leaf) != (void *) solo_leaf, 0);
    TEST_CHECK((void *) yabai_window_node_find_first_leaf(solo_leaf) != (void *) solo_leaf, 0);
    TEST_CHECK((void *) window_node_find_last_leaf(solo_leaf) != (void *) solo_leaf, 0);
    TEST_CHECK((void *) yabai_window_node_find_last_leaf(solo_leaf) != (void *) solo_leaf, 0);
    free(solo_leaf);

    free_test_tree(r, a, c, d, e);
})

TEST_FUNC(zig_view_window_lookup,
{
    struct window_node *r;
    struct window_node *a;
    struct window_node *c;
    struct window_node *d;
    struct window_node *e;
    make_test_tree(&r, &a, &c, &d, &e);

    TEST_CHECK(window_node_contains_window(d, 5) != yabai_window_node_contains_window(d, 5), 0);
    TEST_CHECK(window_node_contains_window(d, 5), true);
    TEST_CHECK(window_node_contains_window(d, 6) != yabai_window_node_contains_window(d, 6), 0);
    TEST_CHECK(window_node_contains_window(d, 6), false);
    TEST_CHECK(window_node_contains_window(c, 7) != yabai_window_node_contains_window(c, 7), 0);
    TEST_CHECK(window_node_contains_window(c, 7), true);
    TEST_CHECK(yabai_window_node_contains_window(c, 7), true);
    TEST_CHECK(window_node_contains_window(e, 5) != yabai_window_node_contains_window(e, 5), 0);
    TEST_CHECK(window_node_contains_window(e, 5), false);
    TEST_CHECK(yabai_window_node_contains_window(e, 5), false);
    /* Hand-pinned: empty leaf contains nothing on either side. */
    TEST_CHECK(window_node_contains_window(e, 7), false);
    TEST_CHECK(yabai_window_node_contains_window(e, 7), false);

    TEST_CHECK(window_node_index_of_window(d, 9) != yabai_window_node_index_of_window(d, 9), 0);
    TEST_CHECK(window_node_index_of_window(d, 9), 1);
    TEST_CHECK(yabai_window_node_index_of_window(d, 9), 1);
    /* Hand-pinned: first element resolves to index 0 on both sides. */
    TEST_CHECK(window_node_index_of_window(d, 5) != yabai_window_node_index_of_window(d, 5), 0);
    TEST_CHECK(window_node_index_of_window(d, 5), 0);
    TEST_CHECK(yabai_window_node_index_of_window(d, 5), 0);
    /* C returns 0 (not -1) for a missing window; the port must match. */
    TEST_CHECK(window_node_index_of_window(d, 6) != yabai_window_node_index_of_window(d, 6), 0);
    TEST_CHECK(window_node_index_of_window(d, 6), 0);
    TEST_CHECK(yabai_window_node_index_of_window(d, 6), 0);
    /* Hand-pinned: empty leaf index falls back to 0 on both sides. */
    TEST_CHECK(window_node_index_of_window(e, 5) != yabai_window_node_index_of_window(e, 5), 0);
    TEST_CHECK(window_node_index_of_window(e, 5), 0);
    TEST_CHECK(yabai_window_node_index_of_window(e, 5), 0);

    free_test_tree(r, a, c, d, e);
})

static void init_mutate_fixture(struct window_node *r, struct window_node *a, struct window_node *c, struct window_node *d, struct window_node *e)
{
    r->split = SPLIT_X;
    r->ratio = 0.6f;
    a->split = SPLIT_Y;
    a->ratio = 0.5f;
    init_test_area(&r->area, 0, 0, 200, 300);
    init_test_area(&a->area, 0, 0, 250, 100);
    init_test_area(&c->area, 0, 100, 200, 100);
    init_test_area(&d->area, 0, 10, 100, 100);
    init_test_area(&e->area, 100, 0, 100, 100);
}

static void make_test_pair(struct window_node **cn, struct window_node **zn)
{
    struct window_node *t1;
    struct window_node *t2;
    struct window_node *t3;
    struct window_node *t4;
    struct window_node *t5;
    make_test_tree(&t1, &t2, &t3, &t4, &t5);
    cn[0] = t1;
    cn[1] = t2;
    cn[2] = t3;
    cn[3] = t4;
    cn[4] = t5;
    make_test_tree(&t1, &t2, &t3, &t4, &t5);
    zn[0] = t1;
    zn[1] = t2;
    zn[2] = t3;
    zn[3] = t4;
    zn[4] = t5;
    init_mutate_fixture(cn[0], cn[1], cn[2], cn[3], cn[4]);
    init_mutate_fixture(zn[0], zn[1], zn[2], zn[3], zn[4]);
}

static void free_test_pair(struct window_node **cn, struct window_node **zn)
{
    int i = 0;
    for (i = 0; i < 5; ++i) {
        free(cn[i]);
        free(zn[i]);
    }
}

static int node_rel(struct window_node **nodes, struct window_node *p)
{
    int i = 0;
    if (!p) return -1;
    for (i = 0; i < 5; ++i) {
        if (nodes[i] == p) return i;
    }
    return -2;
}

/* Structural comparison: pointer fields are compared by position in the
 * node array, since the C and Zig trees live at different addresses. */
static int tree_struct_equal_n(struct window_node **cn, struct window_node **zn, int n)
{
    int i = 0;
    for (i = 0; i < n; ++i) {
        if (memcmp(&cn[i]->area, &zn[i]->area, sizeof(struct area)) != 0) return 0;
        if (node_rel(cn, cn[i]->parent) != node_rel(zn, zn[i]->parent)) return 0;
        if (node_rel(cn, cn[i]->left) != node_rel(zn, zn[i]->left)) return 0;
        if (node_rel(cn, cn[i]->right) != node_rel(zn, zn[i]->right)) return 0;
        if (node_rel(cn, cn[i]->zoom) != node_rel(zn, zn[i]->zoom)) return 0;
        if (memcmp(cn[i]->window_list, zn[i]->window_list, sizeof(cn[i]->window_list)) != 0) return 0;
        if (memcmp(cn[i]->window_order, zn[i]->window_order, sizeof(cn[i]->window_order)) != 0) return 0;
        if (cn[i]->window_count != zn[i]->window_count) return 0;
        if (memcmp(&cn[i]->ratio, &zn[i]->ratio, sizeof(float)) != 0) return 0;
        if (cn[i]->split != zn[i]->split) return 0;
        if (cn[i]->child != zn[i]->child) return 0;
        if (cn[i]->insert_dir != zn[i]->insert_dir) return 0;
        if (cn[i]->feedback_window.id != zn[i]->feedback_window.id) return 0;
        if ((cn[i]->feedback_window.context == NULL) != (zn[i]->feedback_window.context == NULL)) return 0;
    }
    return 1;
}

static int tree_struct_equal(struct window_node **cn, struct window_node **zn)
{
    return tree_struct_equal_n(cn, zn, 5);
}

/* Hand-pin check for the rotate/mirror seam: literal split, ratio, and
 * child-index expectations for the R(X,0.6)->(A,C); A(Y,0.5)->(D,E)
 * fixture (nodes array order R,A,C,D,E; leaves split NONE). Unlike the
 * C-vs-Z structural comparison, these stay meaningful after the C
 * bodies delegate to the Zig exports. */
static int tree_shape_ok(struct window_node **nodes, int r_split, float r_ratio, int r_left, int r_right, int a_split, float a_ratio, int a_left, int a_right, float leaf_ratio)
{
    struct window_node *r = nodes[0];
    struct window_node *a = nodes[1];
    if (r->split != r_split) return 0;
    if (memcmp(&r->ratio, &r_ratio, sizeof(float)) != 0) return 0;
    if (node_rel(nodes, r->left) != r_left) return 0;
    if (node_rel(nodes, r->right) != r_right) return 0;
    if (a->split != a_split) return 0;
    if (memcmp(&a->ratio, &a_ratio, sizeof(float)) != 0) return 0;
    if (node_rel(nodes, a->left) != a_left) return 0;
    if (node_rel(nodes, a->right) != a_right) return 0;
    if (nodes[2]->split != SPLIT_NONE || nodes[3]->split != SPLIT_NONE || nodes[4]->split != SPLIT_NONE) return 0;
    if (memcmp(&nodes[2]->ratio, &leaf_ratio, sizeof(float)) != 0) return 0;
    if (memcmp(&nodes[3]->ratio, &leaf_ratio, sizeof(float)) != 0) return 0;
    if (memcmp(&nodes[4]->ratio, &leaf_ratio, sizeof(float)) != 0) return 0;
    return 1;
}

TEST_FUNC(zig_view_tree_mutate,
{
    struct window_node *cn[5];
    struct window_node *zn[5];

    make_test_pair(cn, zn);
    window_node_rotate(cn[0], 90);
    yabai_window_node_rotate(zn[0], 90);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    free_test_pair(cn, zn);

    make_test_pair(cn, zn);
    window_node_rotate(cn[0], 180);
    yabai_window_node_rotate(zn[0], 180);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    free_test_pair(cn, zn);

    make_test_pair(cn, zn);
    window_node_rotate(cn[0], 270);
    yabai_window_node_rotate(zn[0], 270);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    free_test_pair(cn, zn);

    make_test_pair(cn, zn);
    struct window_node *c_ret = window_node_mirror(cn[0], SPLIT_Y);
    struct window_node *z_ret = yabai_window_node_mirror(zn[0], SPLIT_Y);
    TEST_CHECK(c_ret != cn[0], 0);
    TEST_CHECK(z_ret != zn[0], 0);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    free_test_pair(cn, zn);

    make_test_pair(cn, zn);
    c_ret = window_node_mirror(cn[0], SPLIT_X);
    z_ret = yabai_window_node_mirror(zn[0], SPLIT_X);
    TEST_CHECK(c_ret != cn[0], 0);
    TEST_CHECK(z_ret != zn[0], 0);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    free_test_pair(cn, zn);

    /* Hand-pinned vectors: 90-degree rotation flips splits X->Y and
     * swaps only the Y-split node (A: D,E -> E,D, ratio 1-0.5). */
    make_test_pair(cn, zn);
    window_node_rotate(cn[0], 90);
    yabai_window_node_rotate(zn[0], 90);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    TEST_CHECK(tree_shape_ok(cn, SPLIT_Y, 0.6f, 1, 2, SPLIT_X, 0.5f, 4, 3, 0.0f), 1);
    TEST_CHECK(tree_shape_ok(zn, SPLIT_Y, 0.6f, 1, 2, SPLIT_X, 0.5f, 4, 3, 0.0f), 1);
    free_test_pair(cn, zn);

    /* Hand-pinned vectors: 180-degree rotation keeps splits, swaps
     * every node (R: A,C -> C,A ratio 1-0.6; A: D,E -> E,D) and
     * complements the zero leaf ratios to 1. */
    make_test_pair(cn, zn);
    window_node_rotate(cn[0], 180);
    yabai_window_node_rotate(zn[0], 180);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    TEST_CHECK(tree_shape_ok(cn, SPLIT_X, (1.0f - 0.6f), 2, 1, SPLIT_Y, 0.5f, 4, 3, 1.0f), 1);
    TEST_CHECK(tree_shape_ok(zn, SPLIT_X, (1.0f - 0.6f), 2, 1, SPLIT_Y, 0.5f, 4, 3, 1.0f), 1);
    free_test_pair(cn, zn);

    /* Hand-pinned vectors: 270-degree rotation swaps only the X-split
     * node (R: A,C -> C,A ratio 1-0.6) and flips both splits. */
    make_test_pair(cn, zn);
    window_node_rotate(cn[0], 270);
    yabai_window_node_rotate(zn[0], 270);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    TEST_CHECK(tree_shape_ok(cn, SPLIT_Y, (1.0f - 0.6f), 2, 1, SPLIT_X, 0.5f, 3, 4, 0.0f), 1);
    TEST_CHECK(tree_shape_ok(zn, SPLIT_Y, (1.0f - 0.6f), 2, 1, SPLIT_X, 0.5f, 3, 4, 0.0f), 1);
    free_test_pair(cn, zn);

    /* Hand-pinned vectors: mirror about Y swaps only the Y-split node
     * (A: D,E -> E,D); mirror about X swaps only R (A,C -> C,A).
     * Splits and ratios are untouched; mirror returns its argument. */
    make_test_pair(cn, zn);
    c_ret = window_node_mirror(cn[0], SPLIT_Y);
    z_ret = yabai_window_node_mirror(zn[0], SPLIT_Y);
    TEST_CHECK(c_ret != cn[0], 0);
    TEST_CHECK(z_ret != zn[0], 0);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    TEST_CHECK(tree_shape_ok(cn, SPLIT_X, 0.6f, 1, 2, SPLIT_Y, 0.5f, 4, 3, 0.0f), 1);
    TEST_CHECK(tree_shape_ok(zn, SPLIT_X, 0.6f, 1, 2, SPLIT_Y, 0.5f, 4, 3, 0.0f), 1);
    free_test_pair(cn, zn);

    make_test_pair(cn, zn);
    c_ret = window_node_mirror(cn[0], SPLIT_X);
    z_ret = yabai_window_node_mirror(zn[0], SPLIT_X);
    TEST_CHECK(c_ret != cn[0], 0);
    TEST_CHECK(z_ret != zn[0], 0);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    TEST_CHECK(tree_shape_ok(cn, SPLIT_X, 0.6f, 2, 1, SPLIT_Y, 0.5f, 3, 4, 0.0f), 1);
    TEST_CHECK(tree_shape_ok(zn, SPLIT_X, 0.6f, 2, 1, SPLIT_Y, 0.5f, 3, 4, 0.0f), 1);
    free_test_pair(cn, zn);
})

static int fence_index(struct window_node **nodes, struct window_node *p)
{
    int i = 0;
    if (!p) return -1;
    for (i = 0; i < 5; ++i) {
        if (nodes[i] == p) return i;
    }
    return -2;
}

static int fence_check(struct window_node **nodes, struct window_node *node, int dir, int expected)
{
    struct window_node *c = window_node_fence(node, dir);
    struct window_node *z = yabai_window_node_fence(node, dir);
    if (fence_index(nodes, c) != expected) return 0;
    if (fence_index(nodes, z) != expected) return 0;
    return 1;
}

TEST_FUNC(zig_view_fence,
{
    struct window_node *r;
    struct window_node *a;
    struct window_node *c;
    struct window_node *d;
    struct window_node *e;
    make_test_tree(&r, &a, &c, &d, &e);
    init_mutate_fixture(r, a, c, d, e);

    struct window_node *nodes[5];
    nodes[0] = r;
    nodes[1] = a;
    nodes[2] = c;
    nodes[3] = d;
    nodes[4] = e;

    TEST_CHECK(fence_check(nodes, e, DIR_WEST, 1), 1);
    TEST_CHECK(fence_check(nodes, e, DIR_NORTH, -1), 1);
    TEST_CHECK(fence_check(nodes, e, DIR_EAST, 1), 1);
    TEST_CHECK(fence_check(nodes, c, DIR_SOUTH, 0), 1);
    TEST_CHECK(fence_check(nodes, d, DIR_WEST, -1), 1);
    TEST_CHECK(fence_check(nodes, d, DIR_NORTH, 0), 1);
    TEST_CHECK(fence_check(nodes, c, DIR_EAST, -1), 1);
    TEST_CHECK(fence_check(nodes, NULL, DIR_NORTH, -1), 1);

    free_test_tree(r, a, c, d, e);
})

static struct window *make_test_window(uint32_t wid)
{
    struct window *window = malloc(sizeof(struct window));
    memset(window, 0, sizeof(struct window));
    window->id = wid;
    return window;
}

static void set_stack_lists(struct window_node *node, uint32_t l0, uint32_t l1, uint32_t l2, uint32_t o0, uint32_t o1, int count)
{
    node->window_list[0] = l0;
    node->window_list[1] = l1;
    node->window_list[2] = l2;
    node->window_order[0] = o0;
    node->window_order[1] = o1;
    node->window_count = count;
}

static int stack_case(uint32_t l0, uint32_t l1, uint32_t l2, uint32_t o0, uint32_t o1, int count, uint32_t wid)
{
    struct window_node *c_node = make_test_node();
    struct window_node *z_node = make_test_node();
    set_stack_lists(c_node, l0, l1, l2, o0, o1, count);
    set_stack_lists(z_node, l0, l1, l2, o0, o1, count);

    struct window *window = make_test_window(wid);
    view_stack_window_node(c_node, window);
    yabai_view_stack_window(z_node, wid);
    free(window);

    struct window_node *cn[5];
    struct window_node *zn[5];
    cn[0] = c_node;
    zn[0] = z_node;
    int i = 0;
    for (i = 1; i < 5; ++i) {
        cn[i] = NULL;
        zn[i] = NULL;
    }
    int ok = tree_struct_equal_n(cn, zn, 1);
    free(c_node);
    free(z_node);
    return ok;
}

TEST_FUNC(zig_view_stack_window,
{
    /* order[0] present in list: insert right after it. */
    TEST_CHECK(stack_case(5, 9, 7, 9, 5, 3, 42), 1);
    /* order[0] absent: append at the end. */
    TEST_CHECK(stack_case(5, 9, 7, 99, 5, 3, 42), 1);
    /* Empty node. */
    TEST_CHECK(stack_case(0, 0, 0, 0, 0, 0, 42), 1);

    /* Hand-pinned: order[0]=9 found at list[1], so 42 inserts at
     * list[2], shifting the old tail right: list [5,9,42,7],
     * order [42,9,5], count 4. */
    struct window_node *c_node = make_test_node();
    struct window_node *z_node = make_test_node();
    set_stack_lists(c_node, 5, 9, 7, 9, 5, 3);
    set_stack_lists(z_node, 5, 9, 7, 9, 5, 3);
    struct window *window = make_test_window(42);
    view_stack_window_node(c_node, window);
    yabai_view_stack_window(z_node, 42);
    free(window);
    TEST_CHECK(c_node->window_list[0], 5);
    TEST_CHECK(c_node->window_list[1], 9);
    TEST_CHECK(c_node->window_list[2], 42);
    TEST_CHECK(c_node->window_list[3], 7);
    TEST_CHECK(c_node->window_order[0], 42);
    TEST_CHECK(c_node->window_order[1], 9);
    TEST_CHECK(c_node->window_order[2], 5);
    TEST_CHECK(c_node->window_count, 4);
    TEST_CHECK(z_node->window_list[0], 5);
    TEST_CHECK(z_node->window_list[1], 9);
    TEST_CHECK(z_node->window_list[2], 42);
    TEST_CHECK(z_node->window_list[3], 7);
    TEST_CHECK(z_node->window_order[0], 42);
    TEST_CHECK(z_node->window_order[1], 9);
    TEST_CHECK(z_node->window_order[2], 5);
    TEST_CHECK(z_node->window_count, 4);
    free(c_node);
    free(z_node);

    /* Hand-pinned: order[0]=99 absent from the list, so 42 appends:
     * list [5,9,7,42], order [42,99,5], count 4. */
    c_node = make_test_node();
    z_node = make_test_node();
    set_stack_lists(c_node, 5, 9, 7, 99, 5, 3);
    set_stack_lists(z_node, 5, 9, 7, 99, 5, 3);
    window = make_test_window(42);
    view_stack_window_node(c_node, window);
    yabai_view_stack_window(z_node, 42);
    free(window);
    TEST_CHECK(c_node->window_list[0], 5);
    TEST_CHECK(c_node->window_list[1], 9);
    TEST_CHECK(c_node->window_list[2], 7);
    TEST_CHECK(c_node->window_list[3], 42);
    TEST_CHECK(c_node->window_order[0], 42);
    TEST_CHECK(c_node->window_order[1], 99);
    TEST_CHECK(c_node->window_order[2], 5);
    TEST_CHECK(c_node->window_count, 4);
    TEST_CHECK(z_node->window_list[0], 5);
    TEST_CHECK(z_node->window_list[1], 9);
    TEST_CHECK(z_node->window_list[2], 7);
    TEST_CHECK(z_node->window_list[3], 42);
    TEST_CHECK(z_node->window_order[0], 42);
    TEST_CHECK(z_node->window_order[1], 99);
    TEST_CHECK(z_node->window_order[2], 5);
    TEST_CHECK(z_node->window_count, 4);
    free(c_node);
    free(z_node);

    /* Hand-pinned: empty node stacks the first window:
     * list [42], order [42], count 1. */
    c_node = make_test_node();
    z_node = make_test_node();
    set_stack_lists(c_node, 0, 0, 0, 0, 0, 0);
    set_stack_lists(z_node, 0, 0, 0, 0, 0, 0);
    window = make_test_window(42);
    view_stack_window_node(c_node, window);
    yabai_view_stack_window(z_node, 42);
    free(window);
    TEST_CHECK(c_node->window_list[0], 42);
    TEST_CHECK(c_node->window_order[0], 42);
    TEST_CHECK(c_node->window_count, 1);
    TEST_CHECK(z_node->window_list[0], 42);
    TEST_CHECK(z_node->window_order[0], 42);
    TEST_CHECK(z_node->window_count, 1);
    free(c_node);
    free(z_node);
})

TEST_FUNC(zig_view_find_window_node,
{
    struct window_node *r;
    struct window_node *a;
    struct window_node *c;
    struct window_node *d;
    struct window_node *e;
    make_test_tree(&r, &a, &c, &d, &e);

    struct view *c_view = malloc(sizeof(struct view));
    memset(c_view, 0, sizeof(struct view));
    c_view->root = r;
    c_view->sid = 3;

    struct yabai_view *z_view = malloc(sizeof(struct yabai_view));
    memset(z_view, 0, sizeof(struct yabai_view));
    z_view->root = r;
    z_view->sid = 3;

    TEST_CHECK((void *) view_find_window_node(c_view, 9) != (void *) yabai_view_find_window_node(z_view, 9), 0);
    TEST_CHECK((void *) view_find_window_node(c_view, 9) != (void *) d, 0);
    TEST_CHECK((void *) yabai_view_find_window_node(z_view, 9) != (void *) d, 0);
    /* Hand-pinned vector: second window id in the same multi-window leaf. */
    TEST_CHECK((void *) view_find_window_node(c_view, 5) != (void *) d, 0);
    TEST_CHECK((void *) yabai_view_find_window_node(z_view, 5) != (void *) d, 0);
    TEST_CHECK((void *) view_find_window_node(c_view, 7) != (void *) yabai_view_find_window_node(z_view, 7), 0);
    TEST_CHECK((void *) view_find_window_node(c_view, 7) != (void *) c, 0);
    TEST_CHECK((void *) yabai_view_find_window_node(z_view, 7) != (void *) c, 0);
    TEST_CHECK((void *) view_find_window_node(c_view, 12345) != (void *) yabai_view_find_window_node(z_view, 12345), 0);
    /* Hand-pinned vector: absent id resolves to NULL on both sides. */
    TEST_CHECK(view_find_window_node(c_view, 12345), NULL);
    TEST_CHECK(yabai_view_find_window_node(z_view, 12345), NULL);

    free(c_view);
    free(z_view);
    free_test_tree(r, a, c, d, e);
})

TEST_FUNC(zig_view_occupancy_intermediate,
{
    struct window_node *r;
    struct window_node *a;
    struct window_node *c;
    struct window_node *d;
    struct window_node *e;
    make_test_tree(&r, &a, &c, &d, &e);

    /* d:{5,9} count=2 occupied, c:{7} count=1 occupied, e empty. */
    TEST_CHECK(window_node_is_occupied(d) != yabai_window_node_is_occupied(d), 0);
    TEST_CHECK(window_node_is_occupied(d), true);
    TEST_CHECK(yabai_window_node_is_occupied(d), true);
    TEST_CHECK(window_node_is_occupied(c) != yabai_window_node_is_occupied(c), 0);
    TEST_CHECK(window_node_is_occupied(c), true);
    TEST_CHECK(window_node_is_occupied(e) != yabai_window_node_is_occupied(e), 0);
    TEST_CHECK(window_node_is_occupied(e), false);
    TEST_CHECK(yabai_window_node_is_occupied(e), false);
    TEST_CHECK(window_node_is_occupied(r) != yabai_window_node_is_occupied(r), 0);
    TEST_CHECK(window_node_is_occupied(r), false);

    /* Hand vector: bump count on a leaf. */
    e->window_count = 1;
    TEST_CHECK(window_node_is_occupied(e) != yabai_window_node_is_occupied(e), 0);
    TEST_CHECK(window_node_is_occupied(e), true);
    e->window_count = 0;

    TEST_CHECK(window_node_is_intermediate(r) != yabai_window_node_is_intermediate(r), 0);
    TEST_CHECK(window_node_is_intermediate(r), false);
    TEST_CHECK(yabai_window_node_is_intermediate(r), false);
    TEST_CHECK(window_node_is_intermediate(a) != yabai_window_node_is_intermediate(a), 0);
    TEST_CHECK(window_node_is_intermediate(a), true);
    TEST_CHECK(window_node_is_intermediate(d) != yabai_window_node_is_intermediate(d), 0);
    TEST_CHECK(window_node_is_intermediate(d), true);
    TEST_CHECK(window_node_is_intermediate(c) != yabai_window_node_is_intermediate(c), 0);

    struct window_node *solo = make_test_node();
    TEST_CHECK(window_node_is_intermediate(solo) != yabai_window_node_is_intermediate(solo), 0);
    TEST_CHECK(window_node_is_intermediate(solo), false);
    TEST_CHECK(window_node_is_occupied(solo) != yabai_window_node_is_occupied(solo), 0);
    TEST_CHECK(window_node_is_occupied(solo), false);
    free(solo);

    free_test_tree(r, a, c, d, e);
})

TEST_FUNC(zig_view_child_split_ratio_gap,
{
    int saved_placement = g_space_manager.window_placement;
    int saved_split = g_space_manager.split_type;
    float saved_ratio = g_space_manager.split_ratio;

    struct window_node *n = make_test_node();

    /* get_child: explicit child wins, NONE falls back to placement. */
    n->child = CHILD_SECOND;
    g_space_manager.window_placement = CHILD_FIRST;
    TEST_CHECK(window_node_get_child(n) != yabai_window_node_get_child(n, g_space_manager.window_placement), 0);
    TEST_CHECK(window_node_get_child(n), CHILD_SECOND);
    TEST_CHECK(yabai_window_node_get_child(n, CHILD_FIRST), CHILD_SECOND);
    n->child = CHILD_FIRST;
    TEST_CHECK(window_node_get_child(n) != yabai_window_node_get_child(n, g_space_manager.window_placement), 0);
    TEST_CHECK(yabai_window_node_get_child(n, CHILD_SECOND), CHILD_FIRST);
    n->child = CHILD_NONE;
    TEST_CHECK(window_node_get_child(n) != yabai_window_node_get_child(n, g_space_manager.window_placement), 0);
    TEST_CHECK(window_node_get_child(n), CHILD_FIRST);
    TEST_CHECK(yabai_window_node_get_child(n, CHILD_FIRST), CHILD_FIRST);
    g_space_manager.window_placement = CHILD_SECOND;
    TEST_CHECK(window_node_get_child(n) != yabai_window_node_get_child(n, g_space_manager.window_placement), 0);
    TEST_CHECK(window_node_get_child(n), CHILD_SECOND);
    TEST_CHECK(yabai_window_node_get_child(n, CHILD_SECOND), CHILD_SECOND);

    /* get_split branches. */
    struct view *v = malloc(sizeof(struct view));
    memset(v, 0, sizeof(struct view));
    struct window_node *sn = make_test_node();

    sn->split = SPLIT_Y;
    v->split_type = SPLIT_X;
    g_space_manager.split_type = SPLIT_X;
    init_test_area(&sn->area, 0, 0, 100, 200);
    TEST_CHECK(window_node_get_split(v, sn) != yabai_window_node_get_split(v, sn, g_space_manager.split_type), 0);
    TEST_CHECK(window_node_get_split(v, sn), SPLIT_Y);
    TEST_CHECK(yabai_window_node_get_split(v, sn, SPLIT_X), SPLIT_Y);

    sn->split = SPLIT_NONE;
    v->split_type = SPLIT_X;
    TEST_CHECK(window_node_get_split(v, sn) != yabai_window_node_get_split(v, sn, g_space_manager.split_type), 0);
    TEST_CHECK(window_node_get_split(v, sn), SPLIT_X);
    TEST_CHECK(yabai_window_node_get_split(v, sn, SPLIT_Y), SPLIT_X);

    sn->split = SPLIT_NONE;
    v->split_type = SPLIT_AUTO;
    init_test_area(&sn->area, 0, 0, 200, 100);
    g_space_manager.split_type = SPLIT_X;
    TEST_CHECK(window_node_get_split(v, sn) != yabai_window_node_get_split(v, sn, g_space_manager.split_type), 0);
    TEST_CHECK(window_node_get_split(v, sn), SPLIT_Y);
    TEST_CHECK(yabai_window_node_get_split(v, sn, SPLIT_X), SPLIT_Y);
    init_test_area(&sn->area, 0, 0, 100, 200);
    TEST_CHECK(window_node_get_split(v, sn) != yabai_window_node_get_split(v, sn, g_space_manager.split_type), 0);
    TEST_CHECK(window_node_get_split(v, sn), SPLIT_X);
    TEST_CHECK(yabai_window_node_get_split(v, sn, SPLIT_X), SPLIT_X);

    v->split_type = SPLIT_NONE;
    g_space_manager.split_type = SPLIT_Y;
    TEST_CHECK(window_node_get_split(v, sn) != yabai_window_node_get_split(v, sn, g_space_manager.split_type), 0);
    TEST_CHECK(window_node_get_split(v, sn), SPLIT_Y);
    TEST_CHECK(yabai_window_node_get_split(v, sn, SPLIT_Y), SPLIT_Y);
    g_space_manager.split_type = SPLIT_X;
    TEST_CHECK(window_node_get_split(v, sn) != yabai_window_node_get_split(v, sn, g_space_manager.split_type), 0);
    TEST_CHECK(window_node_get_split(v, sn), SPLIT_X);
    TEST_CHECK(yabai_window_node_get_split(v, sn, SPLIT_X), SPLIT_X);

    v->split_type = SPLIT_NONE;
    g_space_manager.split_type = SPLIT_AUTO;
    init_test_area(&sn->area, 0, 0, 200, 100);
    TEST_CHECK(window_node_get_split(v, sn) != yabai_window_node_get_split(v, sn, g_space_manager.split_type), 0);
    TEST_CHECK(window_node_get_split(v, sn), SPLIT_Y);
    TEST_CHECK(yabai_window_node_get_split(v, sn, SPLIT_AUTO), SPLIT_Y);
    init_test_area(&sn->area, 0, 0, 100, 200);
    TEST_CHECK(window_node_get_split(v, sn) != yabai_window_node_get_split(v, sn, g_space_manager.split_type), 0);
    TEST_CHECK(window_node_get_split(v, sn), SPLIT_X);
    TEST_CHECK(yabai_window_node_get_split(v, sn, SPLIT_AUTO), SPLIT_X);

    /* get_ratio: in-range passes through, out-of-range falls back. */
    g_space_manager.split_ratio = 0.5f;
    sn->ratio = 0.3f;
    TEST_CHECK(memcmp(&(float){window_node_get_ratio(sn)}, &(float){yabai_window_node_get_ratio(sn, g_space_manager.split_ratio)}, sizeof(float)), 0);
    TEST_CHECK(memcmp(&(float){0.3f}, &(float){yabai_window_node_get_ratio(sn, 0.5f)}, sizeof(float)), 0);
    sn->ratio = 0.05f;
    TEST_CHECK(memcmp(&(float){window_node_get_ratio(sn)}, &(float){yabai_window_node_get_ratio(sn, g_space_manager.split_ratio)}, sizeof(float)), 0);
    TEST_CHECK(memcmp(&(float){0.5f}, &(float){yabai_window_node_get_ratio(sn, 0.5f)}, sizeof(float)), 0);
    sn->ratio = 0.95f;
    TEST_CHECK(memcmp(&(float){window_node_get_ratio(sn)}, &(float){yabai_window_node_get_ratio(sn, g_space_manager.split_ratio)}, sizeof(float)), 0);
    TEST_CHECK(memcmp(&(float){0.5f}, &(float){yabai_window_node_get_ratio(sn, 0.5f)}, sizeof(float)), 0);
    sn->ratio = 0.1f;
    TEST_CHECK(memcmp(&(float){window_node_get_ratio(sn)}, &(float){yabai_window_node_get_ratio(sn, g_space_manager.split_ratio)}, sizeof(float)), 0);
    TEST_CHECK(memcmp(&(float){0.1f}, &(float){yabai_window_node_get_ratio(sn, 0.5f)}, sizeof(float)), 0);
    sn->ratio = 0.9f;
    TEST_CHECK(memcmp(&(float){window_node_get_ratio(sn)}, &(float){yabai_window_node_get_ratio(sn, g_space_manager.split_ratio)}, sizeof(float)), 0);
    TEST_CHECK(memcmp(&(float){0.9f}, &(float){yabai_window_node_get_ratio(sn, 0.5f)}, sizeof(float)), 0);
    sn->ratio = 0.09f;
    TEST_CHECK(memcmp(&(float){0.5f}, &(float){yabai_window_node_get_ratio(sn, 0.5f)}, sizeof(float)), 0);
    sn->ratio = 0.91f;
    TEST_CHECK(memcmp(&(float){0.5f}, &(float){yabai_window_node_get_ratio(sn, 0.5f)}, sizeof(float)), 0);

    /* get_gap: flag gates window_gap; paddings pinned to prove no influence. */
    v->top_padding = 5;
    v->bottom_padding = 6;
    v->left_padding = 7;
    v->right_padding = 8;
    v->window_gap = 10;
    v->flags = VIEW_ENABLE_GAP;
    TEST_CHECK(window_node_get_gap(v) != yabai_window_node_get_gap(v), 0);
    TEST_CHECK(window_node_get_gap(v), 10);
    TEST_CHECK(yabai_window_node_get_gap(v), 10);
    v->flags = 0;
    TEST_CHECK(window_node_get_gap(v) != yabai_window_node_get_gap(v), 0);
    TEST_CHECK(window_node_get_gap(v), 0);
    TEST_CHECK(yabai_window_node_get_gap(v), 0);
    v->window_gap = 0;
    v->flags = VIEW_ENABLE_GAP;
    TEST_CHECK(window_node_get_gap(v) != yabai_window_node_get_gap(v), 0);
    TEST_CHECK(yabai_window_node_get_gap(v), 0);

    free(v);
    free(sn);
    free(n);
    g_space_manager.window_placement = saved_placement;
    g_space_manager.split_type = saved_split;
    g_space_manager.split_ratio = saved_ratio;
})

TEST_FUNC(zig_view_area_convert,
{
    CGRect rect;
    rect.origin.x = 0;
    rect.origin.y = 0;
    rect.size.width = 1920;
    rect.size.height = 1080;
    struct area c_a = area_from_cgrect(rect);
    struct area z_a;
    yabai_area_from_cgrect(&rect, &z_a);
    TEST_CHECK(memcmp(&c_a, &z_a, sizeof(struct area)), 0);
    struct area expected;
    init_test_area(&expected, 0, 0, 1920, 1080);
    TEST_CHECK(memcmp(&z_a, &expected, sizeof(struct area)), 0);

    rect.origin.x = 10.5;
    rect.origin.y = 20.25;
    rect.size.width = 100.75;
    rect.size.height = 200.5;
    c_a = area_from_cgrect(rect);
    yabai_area_from_cgrect(&rect, &z_a);
    TEST_CHECK(memcmp(&c_a, &z_a, sizeof(struct area)), 0);
    init_test_area(&expected, 10.5f, 20.25f, 100.75f, 200.5f);
    TEST_CHECK(memcmp(&z_a, &expected, sizeof(struct area)), 0);

    rect.origin.x = -5.5;
    rect.origin.y = -10.25;
    rect.size.width = 50.5;
    rect.size.height = 60.75;
    c_a = area_from_cgrect(rect);
    yabai_area_from_cgrect(&rect, &z_a);
    TEST_CHECK(memcmp(&c_a, &z_a, sizeof(struct area)), 0);

    struct area a;
    init_test_area(&a, 0, 0, 100, 100);
    CGPoint c_p = area_max_point(a);
    CGPoint z_p;
    yabai_area_max_point(&a, &z_p);
    TEST_CHECK(memcmp(&c_p, &z_p, sizeof(CGPoint)), 0);
    TEST_CHECK(c_p.x == 99, 1);
    TEST_CHECK(c_p.y == 99, 1);
    TEST_CHECK(z_p.x == 99, 1);
    TEST_CHECK(z_p.y == 99, 1);

    init_test_area(&a, 10, 20, 30, 40);
    c_p = area_max_point(a);
    yabai_area_max_point(&a, &z_p);
    TEST_CHECK(memcmp(&c_p, &z_p, sizeof(CGPoint)), 0);
    TEST_CHECK(c_p.x == 39, 1);
    TEST_CHECK(c_p.y == 59, 1);

    init_test_area(&a, 0.5f, 1.5f, 100, 100);
    c_p = area_max_point(a);
    yabai_area_max_point(&a, &z_p);
    TEST_CHECK(memcmp(&c_p, &z_p, sizeof(CGPoint)), 0);
    TEST_CHECK(c_p.x == 99.5, 1);
    TEST_CHECK(c_p.y == 100.5, 1);

    init_test_area(&a, 0, 0, 1, 1);
    c_p = area_max_point(a);
    yabai_area_max_point(&a, &z_p);
    TEST_CHECK(memcmp(&c_p, &z_p, sizeof(CGPoint)), 0);
    TEST_CHECK(c_p.x == 0, 1);
    TEST_CHECK(c_p.y == 0, 1);
})

static void make_equalize_pair(struct window_node **cr, struct window_node **cl, struct window_node **crr, struct window_node **zr, struct window_node **zl, struct window_node **zrr)
{
    *cr = make_test_node();
    *cl = make_test_node();
    *crr = make_test_node();
    (*cr)->left = *cl;
    (*cr)->right = *crr;
    (*cl)->parent = *cr;
    (*crr)->parent = *cr;
    *zr = make_test_node();
    *zl = make_test_node();
    *zrr = make_test_node();
    (*zr)->left = *zl;
    (*zr)->right = *zrr;
    (*zl)->parent = *zr;
    (*zrr)->parent = *zr;
}

TEST_FUNC(zig_view_equalize,
{
    float saved_ratio = g_space_manager.split_ratio;
    g_space_manager.split_ratio = 0.5f;

    struct window_node *cr;
    struct window_node *cl;
    struct window_node *crr;
    struct window_node *zr;
    struct window_node *zl;
    struct window_node *zrr;
    make_equalize_pair(&cr, &cl, &crr, &zr, &zl, &zrr);
    cr->split = SPLIT_Y;
    cr->ratio = 0.3f;
    zr->split = SPLIT_Y;
    zr->ratio = 0.3f;
    window_node_equalize(cr, SPLIT_Y);
    yabai_window_node_equalize(zr, SPLIT_Y, g_space_manager.split_ratio);
    TEST_CHECK(memcmp(&cr->ratio, &zr->ratio, sizeof(float)), 0);
    TEST_CHECK(memcmp(&zr->ratio, &(float){0.5f}, sizeof(float)), 0);
    free(cr);
    free(cl);
    free(crr);
    free(zr);
    free(zl);
    free(zrr);

    make_equalize_pair(&cr, &cl, &crr, &zr, &zl, &zrr);
    cr->split = SPLIT_Y;
    cr->ratio = 0.3f;
    zr->split = SPLIT_Y;
    zr->ratio = 0.3f;
    window_node_equalize(cr, SPLIT_X);
    yabai_window_node_equalize(zr, SPLIT_X, g_space_manager.split_ratio);
    TEST_CHECK(memcmp(&cr->ratio, &zr->ratio, sizeof(float)), 0);
    TEST_CHECK(memcmp(&zr->ratio, &(float){0.3f}, sizeof(float)), 0);
    free(cr);
    free(cl);
    free(crr);
    free(zr);
    free(zl);
    free(zrr);

    /* Mixed splits: Y-only resets Y nodes, X-only resets X nodes. */
    struct window_node *cn[5];
    struct window_node *zn[5];
    make_test_pair(cn, zn);
    cn[0]->split = SPLIT_Y;
    cn[0]->ratio = 0.2f;
    cn[1]->split = SPLIT_X;
    cn[1]->ratio = 0.8f;
    zn[0]->split = SPLIT_Y;
    zn[0]->ratio = 0.2f;
    zn[1]->split = SPLIT_X;
    zn[1]->ratio = 0.8f;
    window_node_equalize(cn[0], SPLIT_Y);
    yabai_window_node_equalize(zn[0], SPLIT_Y, g_space_manager.split_ratio);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    TEST_CHECK(memcmp(&zn[0]->ratio, &(float){0.5f}, sizeof(float)), 0);
    TEST_CHECK(memcmp(&zn[1]->ratio, &(float){0.8f}, sizeof(float)), 0);
    free_test_pair(cn, zn);

    make_test_pair(cn, zn);
    cn[0]->split = SPLIT_Y;
    cn[0]->ratio = 0.2f;
    cn[1]->split = SPLIT_X;
    cn[1]->ratio = 0.8f;
    zn[0]->split = SPLIT_Y;
    zn[0]->ratio = 0.2f;
    zn[1]->split = SPLIT_X;
    zn[1]->ratio = 0.8f;
    window_node_equalize(cn[0], SPLIT_X);
    yabai_window_node_equalize(zn[0], SPLIT_X, g_space_manager.split_ratio);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    TEST_CHECK(memcmp(&zn[0]->ratio, &(float){0.2f}, sizeof(float)), 0);
    TEST_CHECK(memcmp(&zn[1]->ratio, &(float){0.5f}, sizeof(float)), 0);
    free_test_pair(cn, zn);

    g_space_manager.split_ratio = saved_ratio;
})

static void make_balance_trio(struct window_node **r, struct window_node **l, struct window_node **rr)
{
    *r = make_test_node();
    *l = make_test_node();
    *rr = make_test_node();
    (*r)->left = *l;
    (*r)->right = *rr;
    (*l)->parent = *r;
    (*rr)->parent = *r;
}

TEST_FUNC(zig_view_balance,
{
    /* Two-leaf Y tree, axis Y: ratio 1/2, total {1,0}. */
    struct window_node *cr;
    struct window_node *cl;
    struct window_node *crr;
    struct window_node *zr;
    struct window_node *zl;
    struct window_node *zrr;
    make_balance_trio(&cr, &cl, &crr);
    make_balance_trio(&zr, &zl, &zrr);
    cr->split = SPLIT_Y;
    cr->ratio = 0.9f;
    zr->split = SPLIT_Y;
    zr->ratio = 0.9f;
    struct balance_node c_out = window_node_balance(cr, SPLIT_Y);
    struct balance_node z_out;
    yabai_window_node_balance(zr, SPLIT_Y, &z_out);
    TEST_CHECK(memcmp(&c_out, &z_out, sizeof(struct balance_node)), 0);
    TEST_CHECK(memcmp(&cr->ratio, &zr->ratio, sizeof(float)), 0);
    TEST_CHECK(memcmp(&zr->ratio, &(float){0.5f}, sizeof(float)), 0);
    TEST_CHECK(z_out.y_count, 1);
    TEST_CHECK(z_out.x_count, 0);
    free(cr);
    free(cl);
    free(crr);
    free(zr);
    free(zl);
    free(zrr);

    /* Five-node R(Y) -> (A(Y) -> (D,E), C), axis Y: A=1/2, R=2/3. */
    struct window_node *cn[5];
    struct window_node *zn[5];
    make_test_pair(cn, zn);
    cn[0]->split = SPLIT_Y;
    cn[0]->ratio = 0.9f;
    cn[1]->split = SPLIT_Y;
    cn[1]->ratio = 0.9f;
    zn[0]->split = SPLIT_Y;
    zn[0]->ratio = 0.9f;
    zn[1]->split = SPLIT_Y;
    zn[1]->ratio = 0.9f;
    c_out = window_node_balance(cn[0], SPLIT_Y);
    yabai_window_node_balance(zn[0], SPLIT_Y, &z_out);
    TEST_CHECK(memcmp(&c_out, &z_out, sizeof(struct balance_node)), 0);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    float exp_a = 1.0f / 2.0f;
    float exp_r = 2.0f / 3.0f;
    TEST_CHECK(memcmp(&zn[1]->ratio, &exp_a, sizeof(float)), 0);
    TEST_CHECK(memcmp(&zn[0]->ratio, &exp_r, sizeof(float)), 0);
    free_test_pair(cn, zn);

    /* Mixed R(Y) -> (A(X) -> (D,E), C), axis both: A=1/2, R=1/2. */
    make_test_pair(cn, zn);
    cn[0]->split = SPLIT_Y;
    cn[0]->ratio = 0.9f;
    cn[1]->split = SPLIT_X;
    cn[1]->ratio = 0.9f;
    zn[0]->split = SPLIT_Y;
    zn[0]->ratio = 0.9f;
    zn[1]->split = SPLIT_X;
    zn[1]->ratio = 0.9f;
    c_out = window_node_balance(cn[0], SPLIT_Y | SPLIT_X);
    yabai_window_node_balance(zn[0], SPLIT_Y | SPLIT_X, &z_out);
    TEST_CHECK(memcmp(&c_out, &z_out, sizeof(struct balance_node)), 0);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);
    TEST_CHECK(memcmp(&zn[1]->ratio, &exp_a, sizeof(float)), 0);
    TEST_CHECK(memcmp(&zn[0]->ratio, &exp_a, sizeof(float)), 0);
    free_test_pair(cn, zn);
})

static void make_pair_node(struct window_node **node, struct window_node **left, struct window_node **right)
{
    *node = make_test_node();
    *left = make_test_node();
    *right = make_test_node();
    (*node)->left = *left;
    (*node)->right = *right;
    (*left)->parent = *node;
    (*right)->parent = *node;
}

static int pair_for_node_case(struct view *v, int view_split, int space_split, float space_ratio, int node_split, float node_ratio, float w, float h, int gap)
{
    struct window_node *cn;
    struct window_node *cl;
    struct window_node *cr;
    struct window_node *zn;
    struct window_node *zl;
    struct window_node *zr;
    make_pair_node(&cn, &cl, &cr);
    make_pair_node(&zn, &zl, &zr);

    init_test_area(&cn->area, 0, 0, w, h);
    init_test_area(&zn->area, 0, 0, w, h);
    cn->split = node_split;
    zn->split = node_split;
    cn->ratio = node_ratio;
    zn->ratio = node_ratio;
    v->split_type = view_split;
    v->window_gap = gap;
    v->flags = gap ? VIEW_ENABLE_GAP : 0;
    g_space_manager.split_type = space_split;
    g_space_manager.split_ratio = space_ratio;

    area_make_pair_for_node(v, cn);
    yabai_area_make_pair_for_node(v, zn, g_space_manager.split_type, g_space_manager.split_ratio);

    struct window_node *ca[3];
    struct window_node *za[3];
    ca[0] = cn;
    ca[1] = cl;
    ca[2] = cr;
    za[0] = zn;
    za[1] = zl;
    za[2] = zr;
    int ok = tree_struct_equal_n(ca, za, 3);
    free(cn);
    free(cl);
    free(cr);
    free(zn);
    free(zl);
    free(zr);
    return ok;
}

TEST_FUNC(zig_view_area_pair_for_node,
{
    int saved_split = g_space_manager.split_type;
    float saved_ratio = g_space_manager.split_ratio;

    struct view *v = malloc(sizeof(struct view));
    memset(v, 0, sizeof(struct view));

    /* Oracle parity: explicit split, view split, space split, aspect
     * fallback, in-range vs fallback ratio, gap on/off. */
    TEST_CHECK(pair_for_node_case(v, SPLIT_NONE, SPLIT_AUTO, 0.5f, SPLIT_NONE, 0.0f, 200, 100, 0), 1);
    TEST_CHECK(pair_for_node_case(v, SPLIT_NONE, SPLIT_AUTO, 0.5f, SPLIT_NONE, 0.0f, 100, 200, 0), 1);
    TEST_CHECK(pair_for_node_case(v, SPLIT_X, SPLIT_Y, 0.5f, SPLIT_NONE, 0.0f, 200, 100, 0), 1);
    TEST_CHECK(pair_for_node_case(v, SPLIT_NONE, SPLIT_X, 0.5f, SPLIT_NONE, 0.0f, 200, 100, 8), 1);
    TEST_CHECK(pair_for_node_case(v, SPLIT_NONE, SPLIT_AUTO, 0.5f, SPLIT_Y, 0.3f, 200, 100, 4), 1);
    TEST_CHECK(pair_for_node_case(v, SPLIT_NONE, SPLIT_AUTO, 0.7f, SPLIT_X, 0.05f, 100, 200, 4), 1);

    /* Hand-computed: SPLIT_Y over (0,0,200,100), ratio 0.5, gap 10. */
    struct window_node *n;
    struct window_node *l;
    struct window_node *r;
    make_pair_node(&n, &l, &r);
    init_test_area(&n->area, 0, 0, 200, 100);
    n->split = SPLIT_Y;
    n->ratio = 0.5f;
    v->split_type = SPLIT_NONE;
    v->window_gap = 10;
    v->flags = VIEW_ENABLE_GAP;
    yabai_area_make_pair_for_node(v, n, SPLIT_AUTO, 0.5f);
    struct area exp_l;
    struct area exp_r;
    init_test_area(&exp_l, 0, 0, 95, 100);
    init_test_area(&exp_r, 105, 0, 95, 100);
    TEST_CHECK(memcmp(&l->area, &exp_l, sizeof(struct area)), 0);
    TEST_CHECK(memcmp(&r->area, &exp_r, sizeof(struct area)), 0);
    TEST_CHECK(n->split, SPLIT_Y);
    TEST_CHECK(memcmp(&n->ratio, &(float){0.5f}, sizeof(float)), 0);
    free(n);
    free(l);
    free(r);

    /* Hand-computed: SPLIT_X over (0,0,100,200), ratio 0.5, gap 10. */
    make_pair_node(&n, &l, &r);
    init_test_area(&n->area, 0, 0, 100, 200);
    n->split = SPLIT_X;
    n->ratio = 0.5f;
    v->window_gap = 10;
    v->flags = VIEW_ENABLE_GAP;
    yabai_area_make_pair_for_node(v, n, SPLIT_AUTO, 0.5f);
    init_test_area(&exp_l, 0, 0, 100, 95);
    init_test_area(&exp_r, 0, 105, 100, 95);
    TEST_CHECK(memcmp(&l->area, &exp_l, sizeof(struct area)), 0);
    TEST_CHECK(memcmp(&r->area, &exp_r, sizeof(struct area)), 0);
    TEST_CHECK(n->split, SPLIT_X);
    free(n);
    free(l);
    free(r);

    free(v);
    g_space_manager.split_type = saved_split;
    g_space_manager.split_ratio = saved_ratio;
})

/* NULL-safe relative comparison of node + malloc'd children (+ parent
 * slot when present on both sides). Guards the RED phase where the Zig
 * split stub leaves children NULL. */
static int split_pair_equal(struct window_node *cn, struct window_node *cpar, struct window_node *zn, struct window_node *zpar)
{
    if (!cn->left || !cn->right || !zn->left || !zn->right) return 0;
    if ((cpar == NULL) != (zpar == NULL)) return 0;
    struct window_node *ca[4];
    struct window_node *za[4];
    ca[0] = cn;
    ca[1] = cn->left;
    ca[2] = cn->right;
    ca[3] = cpar;
    za[0] = zn;
    za[1] = zn->left;
    za[2] = zn->right;
    za[3] = zpar;
    return tree_struct_equal_n(ca, za, cpar ? 4 : 3);
}

static void set_split_leaf(struct window_node *node, uint32_t w0, uint32_t w1, int child)
{
    node->window_list[0] = w0;
    node->window_list[1] = w1;
    node->window_order[0] = w0;
    node->window_order[1] = w1;
    node->window_count = 2;
    node->child = child;
    init_test_area(&node->area, 0, 0, 200, 100);
}

/* One oracle-parity split case. Both leaves start as {5,9}; the fresh
 * window id is 42. Frees every malloc'd node and window. */
static int node_split_case(int node_child, int placement, bool persist, int with_parent)
{
    struct view *cv = malloc(sizeof(struct view));
    struct view *zv = malloc(sizeof(struct view));
    memset(cv, 0, sizeof(struct view));
    memset(zv, 0, sizeof(struct view));
    struct window_node *cn = make_test_node();
    struct window_node *zn = make_test_node();
    struct window_node *cp = NULL;
    struct window_node *zp = NULL;
    if (with_parent) {
        cp = make_test_node();
        zp = make_test_node();
        cn->parent = cp;
        zn->parent = zp;
    }
    set_split_leaf(cn, 5, 9, node_child);
    set_split_leaf(zn, 5, 9, node_child);
    cv->split_type = SPLIT_NONE;
    zv->split_type = SPLIT_NONE;
    cv->root = with_parent ? cp : cn;
    zv->root = with_parent ? zp : zn;

    g_space_manager.window_placement = placement;
    g_space_manager.window_zoom_persist = persist;
    g_space_manager.split_type = SPLIT_AUTO;
    g_space_manager.split_ratio = 0.5f;

    struct window *cw = make_test_window(42);
    window_node_split(cv, cn, cw);
    yabai_window_node_split(zv, zn, 42, g_space_manager.window_zoom_persist, g_space_manager.window_placement, g_space_manager.split_type, g_space_manager.split_ratio);
    free(cw);

    int ok = split_pair_equal(cn, cp, zn, zp);
    free(cn->left);
    free(cn->right);
    free(zn->left);
    free(zn->right);
    free(cn);
    free(zn);
    free(cp);
    free(zp);
    free(cv);
    free(zv);
    return ok;
}

TEST_FUNC(zig_view_node_split,
{
    int saved_placement = g_space_manager.window_placement;
    bool saved_persist = g_space_manager.window_zoom_persist;
    int saved_split = g_space_manager.split_type;
    float saved_ratio = g_space_manager.split_ratio;

    /* Placement matrix: explicit SECOND wins, else the lifted placement. */
    TEST_CHECK(node_split_case(CHILD_SECOND, CHILD_FIRST, false, 0), 1);
    TEST_CHECK(node_split_case(CHILD_NONE, CHILD_FIRST, false, 0), 1);
    TEST_CHECK(node_split_case(CHILD_NONE, CHILD_SECOND, false, 0), 1);
    TEST_CHECK(node_split_case(CHILD_FIRST, CHILD_SECOND, false, 0), 1);
    /* Zoom persist on with no zoom: children stay NULL-zoomed. */
    TEST_CHECK(node_split_case(CHILD_SECOND, CHILD_FIRST, true, 0), 1);
    TEST_CHECK(node_split_case(CHILD_NONE, CHILD_SECOND, true, 0), 1);
    /* Parent present (zoom still NULL on both sides). */
    TEST_CHECK(node_split_case(CHILD_SECOND, CHILD_FIRST, false, 1), 1);

    /* Zoom matrix with oracle parity on the resulting child zoom. */
    struct view *cv = malloc(sizeof(struct view));
    struct view *zv = malloc(sizeof(struct view));
    memset(cv, 0, sizeof(struct view));
    memset(zv, 0, sizeof(struct view));
    struct window_node *cn = make_test_node();
    struct window_node *zn = make_test_node();
    struct window_node *cp = make_test_node();
    struct window_node *zp = make_test_node();
    set_split_leaf(cn, 5, 9, CHILD_SECOND);
    set_split_leaf(zn, 5, 9, CHILD_SECOND);
    cn->parent = cp;
    zn->parent = zp;
    cv->root = cp;
    zv->root = zp;
    g_space_manager.window_placement = CHILD_FIRST;
    g_space_manager.split_type = SPLIT_AUTO;
    g_space_manager.split_ratio = 0.5f;

    /* persist=false with zoom set: zoom dropped on both sides. */
    cn->zoom = cp;
    zn->zoom = zp;
    g_space_manager.window_zoom_persist = false;
    struct window *cw = make_test_window(42);
    window_node_split(cv, cn, cw);
    yabai_window_node_split(zv, zn, 42, false, CHILD_FIRST, SPLIT_AUTO, 0.5f);
    free(cw);
    TEST_CHECK(split_pair_equal(cn, cp, zn, zp), 1);
    if (zn->left && cn->left) {
        TEST_CHECK(cn->left->zoom != zn->left->zoom, 0);
        TEST_CHECK(cn->left->zoom, NULL);
        TEST_CHECK(zn->left->zoom, NULL);
    } else {
        TEST_CHECK(zn->left != NULL, 1);
    }
    free(cn->left);
    free(cn->right);
    free(zn->left);
    free(zn->right);
    cn->left = NULL;
    cn->right = NULL;
    zn->left = NULL;
    zn->right = NULL;
    cn->window_count = 0;
    zn->window_count = 0;

    /* persist=true with zoom == parent: child zoom is the node itself. */
    cn->zoom = cp;
    zn->zoom = zp;
    g_space_manager.window_zoom_persist = true;
    cw = make_test_window(43);
    window_node_split(cv, cn, cw);
    yabai_window_node_split(zv, zn, 43, true, CHILD_FIRST, SPLIT_AUTO, 0.5f);
    free(cw);
    TEST_CHECK(split_pair_equal(cn, cp, zn, zp), 1);
    if (zn->left && cn->left) {
        TEST_CHECK(cn->left->zoom != cn, 0);
        TEST_CHECK(zn->left->zoom != zn, 0);
    } else {
        TEST_CHECK(zn->left != NULL, 1);
    }
    free(cn->left);
    free(cn->right);
    free(zn->left);
    free(zn->right);
    cn->left = NULL;
    cn->right = NULL;
    zn->left = NULL;
    zn->right = NULL;
    cn->window_count = 0;
    zn->window_count = 0;

    /* persist=true with foreign zoom: child zoom is the view root. */
    struct window_node *co = make_test_node();
    struct window_node *zo = make_test_node();
    cn->zoom = co;
    zn->zoom = zo;
    cw = make_test_window(44);
    window_node_split(cv, cn, cw);
    yabai_window_node_split(zv, zn, 44, true, CHILD_FIRST, SPLIT_AUTO, 0.5f);
    free(cw);
    TEST_CHECK(split_pair_equal(cn, cp, zn, zp), 1);
    if (zn->left && cn->left) {
        TEST_CHECK(cn->left->zoom != cv->root, 0);
        TEST_CHECK(zn->left->zoom != zv->root, 0);
    } else {
        TEST_CHECK(zn->left != NULL, 1);
    }
    free(cn->left);
    free(cn->right);
    free(zn->left);
    free(zn->right);
    free(cn);
    free(zn);
    free(cp);
    free(zp);
    free(co);
    free(zo);
    free(cv);
    free(zv);

    /* Hand-computed: SECOND placement over (0,0,200,100), ratio 0.5. */
    struct view *hv = malloc(sizeof(struct view));
    memset(hv, 0, sizeof(struct view));
    struct window_node *hn = make_test_node();
    set_split_leaf(hn, 5, 9, CHILD_SECOND);
    hv->root = hn;
    yabai_window_node_split(hv, hn, 42, false, CHILD_FIRST, SPLIT_AUTO, 0.5f);
    TEST_CHECK(hn->window_count, 0);
    TEST_CHECK(hn->left != NULL, 1);
    TEST_CHECK(hn->right != NULL, 1);
    if (hn->left && hn->right) {
        TEST_CHECK(hn->left->window_count, 2);
        TEST_CHECK(hn->right->window_count, 1);
        TEST_CHECK(hn->left->window_list[0], 5);
        TEST_CHECK(hn->left->window_list[1], 9);
        TEST_CHECK(hn->right->window_list[0], 42);
        TEST_CHECK(hn->right->window_order[0], 42);
        TEST_CHECK(hn->left->parent != hn, 0);
        TEST_CHECK(hn->right->parent != hn, 0);
        TEST_CHECK(hn->left->zoom, NULL);
        TEST_CHECK(hn->right->zoom, NULL);
        TEST_CHECK(hn->split, SPLIT_Y);
        TEST_CHECK(memcmp(&hn->ratio, &(float){0.5f}, sizeof(float)), 0);
        struct area exp_l;
        struct area exp_r;
        init_test_area(&exp_l, 0, 0, 100, 100);
        init_test_area(&exp_r, 100, 0, 100, 100);
        TEST_CHECK(memcmp(&hn->left->area, &exp_l, sizeof(struct area)), 0);
        TEST_CHECK(memcmp(&hn->right->area, &exp_r, sizeof(struct area)), 0);
    }
    free(hn->left);
    free(hn->right);
    free(hn);
    free(hv);

    /* Hand-computed: FIRST placement puts the fresh window left. */
    hv = malloc(sizeof(struct view));
    memset(hv, 0, sizeof(struct view));
    hn = make_test_node();
    set_split_leaf(hn, 5, 9, CHILD_NONE);
    hv->root = hn;
    yabai_window_node_split(hv, hn, 42, false, CHILD_FIRST, SPLIT_AUTO, 0.5f);
    if (hn->left && hn->right) {
        TEST_CHECK(hn->left->window_count, 1);
        TEST_CHECK(hn->left->window_list[0], 42);
        TEST_CHECK(hn->right->window_count, 2);
        TEST_CHECK(hn->right->window_list[0], 5);
        TEST_CHECK(hn->right->window_list[1], 9);
    } else {
        TEST_CHECK(hn->left != NULL, 1);
    }
    free(hn->left);
    free(hn->right);
    free(hn);
    free(hv);

    g_space_manager.window_placement = saved_placement;
    g_space_manager.window_zoom_persist = saved_persist;
    g_space_manager.split_type = saved_split;
    g_space_manager.split_ratio = saved_ratio;
})

static void set_swap_lists(struct window_node *node, uint32_t l0, uint32_t l1, uint32_t o0, uint32_t o1, int count)
{
    node->window_list[0] = l0;
    node->window_list[1] = l1;
    node->window_order[0] = o0;
    node->window_order[1] = o1;
    node->window_count = count;
}

TEST_FUNC(zig_view_swap_window_list,
{
    /* Oracle parity: uneven lists, orders, counts, zooms cleared. */
    struct window_node *ca = make_test_node();
    struct window_node *cb = make_test_node();
    struct window_node *za = make_test_node();
    struct window_node *zb = make_test_node();
    init_test_area(&ca->area, 0, 0, 100, 100);
    init_test_area(&cb->area, 0, 0, 100, 100);
    init_test_area(&za->area, 0, 0, 100, 100);
    init_test_area(&zb->area, 0, 0, 100, 100);
    set_swap_lists(ca, 1, 2, 2, 1, 2);
    set_swap_lists(cb, 3, 0, 3, 0, 1);
    set_swap_lists(za, 1, 2, 2, 1, 2);
    set_swap_lists(zb, 3, 0, 3, 0, 1);
    ca->zoom = ca;
    cb->zoom = cb;
    za->zoom = za;
    zb->zoom = zb;
    window_node_swap_window_list(ca, cb);
    yabai_window_node_swap_window_list(za, zb);
    struct window_node *cc[2];
    struct window_node *zz[2];
    cc[0] = ca;
    cc[1] = cb;
    zz[0] = za;
    zz[1] = zb;
    TEST_CHECK(tree_struct_equal_n(cc, zz, 2), 1);
    TEST_CHECK(za->window_count, 1);
    TEST_CHECK(zb->window_count, 2);
    TEST_CHECK(za->zoom, NULL);
    TEST_CHECK(zb->zoom, NULL);
    free(ca);
    free(cb);
    free(za);
    free(zb);

    /* Hand-computed vectors guard the seam after C delegates to Zig. */
    struct window_node *a = make_test_node();
    struct window_node *b = make_test_node();
    set_swap_lists(a, 1, 2, 2, 1, 2);
    set_swap_lists(b, 3, 0, 3, 0, 1);
    a->zoom = a;
    b->zoom = b;
    yabai_window_node_swap_window_list(a, b);
    TEST_CHECK(a->window_list[0], 3);
    TEST_CHECK(a->window_order[0], 3);
    TEST_CHECK(a->window_count, 1);
    TEST_CHECK(b->window_list[0], 1);
    TEST_CHECK(b->window_list[1], 2);
    TEST_CHECK(b->window_order[0], 2);
    TEST_CHECK(b->window_order[1], 1);
    TEST_CHECK(b->window_count, 2);
    TEST_CHECK(a->zoom, NULL);
    TEST_CHECK(b->zoom, NULL);
    free(a);
    free(b);

    /* Empty-vs-full edge. */
    a = make_test_node();
    b = make_test_node();
    set_swap_lists(a, 0, 0, 0, 0, 0);
    b->window_list[0] = 7;
    b->window_list[1] = 8;
    b->window_list[2] = 9;
    b->window_order[0] = 9;
    b->window_order[1] = 7;
    b->window_order[2] = 8;
    b->window_count = 3;
    yabai_window_node_swap_window_list(a, b);
    TEST_CHECK(a->window_list[0], 7);
    TEST_CHECK(a->window_list[1], 8);
    TEST_CHECK(a->window_list[2], 9);
    TEST_CHECK(a->window_order[0], 9);
    TEST_CHECK(a->window_order[2], 8);
    TEST_CHECK(a->window_count, 3);
    TEST_CHECK(b->window_count, 0);
    free(a);
    free(b);
})

/* C oracle has no header prototype (defined in src/view.c, which the
 * test binary includes via manifest.m); declare it for the parity
 * comparisons below. */
struct window_node *view_find_min_depth_leaf_node(struct window_node *node);

struct window_node *yabai_view_find_min_depth_leaf_node(struct window_node *node);
bool yabai_view_is_invalid(const struct yabai_view *view);
bool yabai_view_is_dirty(const struct yabai_view *view);
void yabai_window_node_clear_zoom(struct window_node *node);

/* Pin the mirrored flag values (zig/view.zig VIEW_IS_VALID/DIRTY). */
_Static_assert(VIEW_IS_VALID == 0x200, "VIEW_IS_VALID drift");
_Static_assert(VIEW_IS_DIRTY == 0x400, "VIEW_IS_DIRTY drift");
_Static_assert(VIEW_ENABLE_PADDING == 0x080, "VIEW_ENABLE_PADDING drift");
_Static_assert(VIEW_FLOAT == 3, "VIEW_FLOAT drift");

/* Builds an asymmetric fork: root -> (deep, shallow-leaf) when
 * deep_left is nonzero, root -> (shallow-leaf, deep) otherwise. The
 * deep side is a two-leaf parent at depth 1, so BFS must return the
 * depth-1 leaf on the shallow side first. */
static struct window_node *make_asymmetric_tree(struct window_node **deep, struct window_node **shallow, struct window_node **d1, struct window_node **d2, int deep_left)
{
    struct window_node *r = make_test_node();
    *deep = make_test_node();
    *shallow = make_test_node();
    *d1 = make_test_node();
    *d2 = make_test_node();
    if (deep_left) {
        r->left = *deep;
        r->right = *shallow;
    } else {
        r->left = *shallow;
        r->right = *deep;
    }
    (*deep)->parent = r;
    (*shallow)->parent = r;
    (*deep)->left = *d1;
    (*deep)->right = *d2;
    (*d1)->parent = *deep;
    (*d2)->parent = *deep;
    return r;
}

TEST_FUNC(zig_view_min_depth_leaf,
{
    /* Single leaf returns itself on both sides. */
    struct window_node *solo = make_test_node();
    TEST_CHECK(view_find_min_depth_leaf_node(solo) != yabai_view_find_min_depth_leaf_node(solo), 0);
    TEST_CHECK(view_find_min_depth_leaf_node(solo) != solo, 0);
    TEST_CHECK(yabai_view_find_min_depth_leaf_node(solo) != solo, 0);
    free(solo);

    /* R -> (A, C); A -> (D, E): BFS order R,A,C hits C (depth 1)
     * before D,E (depth 2). */
    struct window_node *r;
    struct window_node *a;
    struct window_node *c;
    struct window_node *d;
    struct window_node *e;
    make_test_tree(&r, &a, &c, &d, &e);
    TEST_CHECK(view_find_min_depth_leaf_node(r) != yabai_view_find_min_depth_leaf_node(r), 0);
    TEST_CHECK(view_find_min_depth_leaf_node(r) != c, 0);
    TEST_CHECK(yabai_view_find_min_depth_leaf_node(r) != c, 0);
    free_test_tree(r, a, c, d, e);

    /* Deep-left spine: DFS would return d1, BFS must return shallow. */
    struct window_node *deep;
    struct window_node *shallow;
    struct window_node *d1;
    struct window_node *d2;
    r = make_asymmetric_tree(&deep, &shallow, &d1, &d2, 1);
    TEST_CHECK(view_find_min_depth_leaf_node(r) != yabai_view_find_min_depth_leaf_node(r), 0);
    TEST_CHECK(view_find_min_depth_leaf_node(r) != shallow, 0);
    TEST_CHECK(yabai_view_find_min_depth_leaf_node(r) != shallow, 0);
    free(r);
    free(deep);
    free(shallow);
    free(d1);
    free(d2);

    /* Deep-right mirror: BFS must return the left shallow leaf. */
    r = make_asymmetric_tree(&deep, &shallow, &d1, &d2, 0);
    TEST_CHECK(view_find_min_depth_leaf_node(r) != yabai_view_find_min_depth_leaf_node(r), 0);
    TEST_CHECK(view_find_min_depth_leaf_node(r) != shallow, 0);
    TEST_CHECK(yabai_view_find_min_depth_leaf_node(r) != shallow, 0);
    free(r);
    free(deep);
    free(shallow);
    free(d1);
    free(d2);

    /* Full depth-2 tree R -> (A, B), A -> (D, E), B -> (F, G):
     * queue runs R,A,B,D so the first leaf is D. */
    struct window_node *t1;
    struct window_node *t2;
    struct window_node *t3;
    struct window_node *t4;
    struct window_node *t5;
    make_test_tree(&t1, &t2, &t3, &t4, &t5);
    struct window_node *f = make_test_node();
    struct window_node *g = make_test_node();
    t3->left = f;
    t3->right = g;
    f->parent = t3;
    g->parent = t3;
    TEST_CHECK(view_find_min_depth_leaf_node(t1) != yabai_view_find_min_depth_leaf_node(t1), 0);
    TEST_CHECK(view_find_min_depth_leaf_node(t1) != t4, 0);
    TEST_CHECK(yabai_view_find_min_depth_leaf_node(t1) != t4, 0);
    free(t1);
    free(t2);
    free(t3);
    free(t4);
    free(t5);
    free(f);
    free(g);
})

TEST_FUNC(zig_view_invalid_dirty,
{
    struct view *v = malloc(sizeof(struct view));
    memset(v, 0, sizeof(struct view));

    /* flags == 0: invalid (VALID clear), clean (DIRTY clear). */
    v->flags = 0;
    TEST_CHECK(view_is_invalid(v) != yabai_view_is_invalid((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_invalid(v), true);
    TEST_CHECK(yabai_view_is_invalid((const struct yabai_view *) v), true);
    TEST_CHECK(view_is_dirty(v) != yabai_view_is_dirty((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_dirty(v), false);
    TEST_CHECK(yabai_view_is_dirty((const struct yabai_view *) v), false);

    /* Only VALID: valid, clean. */
    v->flags = VIEW_IS_VALID;
    TEST_CHECK(view_is_invalid(v) != yabai_view_is_invalid((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_invalid(v), false);
    TEST_CHECK(yabai_view_is_invalid((const struct yabai_view *) v), false);
    TEST_CHECK(view_is_dirty(v) != yabai_view_is_dirty((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_dirty(v), false);

    /* Only DIRTY: invalid, dirty. */
    v->flags = VIEW_IS_DIRTY;
    TEST_CHECK(view_is_invalid(v) != yabai_view_is_invalid((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_invalid(v), true);
    TEST_CHECK(view_is_dirty(v) != yabai_view_is_dirty((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_dirty(v), true);
    TEST_CHECK(yabai_view_is_dirty((const struct yabai_view *) v), true);

    /* VALID|DIRTY: valid, dirty. */
    v->flags = VIEW_IS_VALID | VIEW_IS_DIRTY;
    TEST_CHECK(view_is_invalid(v) != yabai_view_is_invalid((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_invalid(v), false);
    TEST_CHECK(view_is_dirty(v) != yabai_view_is_dirty((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_dirty(v), true);

    /* Neighboring flag bits must not alias either bit. */
    v->flags = VIEW_LAYOUT | VIEW_TOP_PADDING | VIEW_WINDOW_GAP | VIEW_ENABLE_GAP | VIEW_ENABLE_PADDING | VIEW_SPLIT_TYPE;
    TEST_CHECK(view_is_invalid(v) != yabai_view_is_invalid((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_invalid(v), true);
    TEST_CHECK(yabai_view_is_invalid((const struct yabai_view *) v), true);
    TEST_CHECK(view_is_dirty(v) != yabai_view_is_dirty((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_dirty(v), false);
    TEST_CHECK(yabai_view_is_dirty((const struct yabai_view *) v), false);

    /* Complement shapes: every bit except the pair set. */
    v->flags = ~(VIEW_IS_VALID | VIEW_IS_DIRTY);
    TEST_CHECK(view_is_invalid(v) != yabai_view_is_invalid((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_invalid(v), true);
    TEST_CHECK(view_is_dirty(v) != yabai_view_is_dirty((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_dirty(v), false);
    v->flags = ~((uint64_t) 0);
    TEST_CHECK(view_is_invalid(v) != yabai_view_is_invalid((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_invalid(v), false);
    TEST_CHECK(view_is_dirty(v) != yabai_view_is_dirty((const struct yabai_view *) v), 0);
    TEST_CHECK(view_is_dirty(v), true);

    free(v);
})

TEST_FUNC(zig_view_clear_zoom,
{
    /* Oracle parity: zooms planted at every depth on mirrored trees. */
    struct window_node *cn[5];
    struct window_node *zn[5];
    make_test_pair(cn, zn);
    cn[0]->zoom = cn[0];
    zn[0]->zoom = zn[0];
    cn[1]->zoom = cn[3];
    zn[1]->zoom = zn[3];
    cn[2]->zoom = cn[2];
    zn[2]->zoom = zn[2];
    cn[3]->zoom = cn[0];
    zn[3]->zoom = zn[0];
    cn[4]->zoom = NULL;
    zn[4]->zoom = NULL;
    window_node_clear_zoom(cn[0]);
    yabai_window_node_clear_zoom(zn[0]);
    TEST_CHECK(tree_struct_equal(cn, zn), 1);

    /* Hand vectors: zoom is NULL at every depth on the Zig side. */
    TEST_CHECK(zn[0]->zoom, NULL);
    TEST_CHECK(zn[1]->zoom, NULL);
    TEST_CHECK(zn[2]->zoom, NULL);
    TEST_CHECK(zn[3]->zoom, NULL);
    TEST_CHECK(zn[4]->zoom, NULL);

    /* Non-zoom state survives: oracle fixture windows still stacked. */
    TEST_CHECK(zn[3]->window_count, 2);
    TEST_CHECK(zn[3]->window_list[0], 5);
    TEST_CHECK(zn[2]->window_count, 1);
    TEST_CHECK(zn[2]->window_list[0], 7);
    free_test_pair(cn, zn);

    /* Single leaf with a self zoom clears. */
    struct window_node *s = make_test_node();
    s->zoom = s;
    yabai_window_node_clear_zoom(s);
    TEST_CHECK(s->zoom, NULL);
    free(s);

    /* Idempotent on an already-clear tree. */
    struct window_node *r;
    struct window_node *a;
    struct window_node *c;
    struct window_node *d;
    struct window_node *e;
    make_test_tree(&r, &a, &c, &d, &e);
    yabai_window_node_clear_zoom(r);
    TEST_CHECK(r->zoom, NULL);
    TEST_CHECK(a->zoom, NULL);
    TEST_CHECK(c->zoom, NULL);
    TEST_CHECK(d->zoom, NULL);
    TEST_CHECK(e->zoom, NULL);
    free_test_tree(r, a, c, d, e);
})

uint32_t *yabai_view_find_window_list(const struct yabai_view *view, int *window_count);

/* Temp-pool init (mirrors zig_rule.c): both list sides now use libc
 * malloc and never touch the pool; the reset keeps pool state sane. */
static void ensure_ts_for_window_list(void)
{
    static bool ts_ready = false;
    if (!ts_ready) {
        ts_init(MEGABYTES(8));
        ts_ready = true;
    }
    ts_reset();
}

/* Fills a leaf with `count` sequential ids starting at `base`. */
static void set_leaf_windows(struct window_node *node, uint32_t base, int count)
{
    int i = 0;
    for (i = 0; i < count; ++i) {
        node->window_list[i] = base + (uint32_t) i;
    }
    node->window_count = count;
}

/* Oracle parity: the C delegate list vs the Zig malloc'd list (both
 * caller-frees malloc since routing; the hand-pinned vectors below
 * guard the now-tautological memcmp). Compares the count and the out
 * bytes with memcmp, then frees both sides. */
static int window_list_case(struct window_node *root, int expected_count)
{
    struct view *c_view = malloc(sizeof(struct view));
    memset(c_view, 0, sizeof(struct view));
    c_view->root = root;
    struct yabai_view *z_view = malloc(sizeof(struct yabai_view));
    memset(z_view, 0, sizeof(struct yabai_view));
    z_view->root = root;

    int c_count = -1;
    int z_count = -2;
    uint32_t *c_list = view_find_window_list(c_view, &c_count);
    uint32_t *z_list = yabai_view_find_window_list(z_view, &z_count);

    int ok = 1;
    if (c_count != expected_count) ok = 0;
    if (z_count != expected_count) ok = 0;
    if (c_count != z_count) ok = 0;
    if (ok && c_count > 0) {
        if (!c_list || !z_list) ok = 0;
        else if (memcmp(c_list, z_list, (size_t) c_count * sizeof(uint32_t)) != 0) ok = 0;
    }

    if (c_list) free(c_list);
    if (z_list) free(z_list);
    free(c_view);
    free(z_view);
    return ok;
}

TEST_FUNC(zig_view_find_window_list,
{
    ensure_ts_for_window_list();

    /* Empty single leaf. */
    struct window_node *solo = make_test_node();
    TEST_CHECK(window_list_case(solo, 0), 1);
    free(solo);

    /* Single leaf with a few windows. */
    solo = make_test_node();
    set_leaf_windows(solo, 101, 3);
    TEST_CHECK(window_list_case(solo, 3), 1);
    /* Hand-pinned: sequential ids [101,102,103], count 3, both sides. */
    {
        struct view *cv2 = malloc(sizeof(struct view));
        memset(cv2, 0, sizeof(struct view));
        cv2->root = solo;
        struct yabai_view *zv2 = malloc(sizeof(struct yabai_view));
        memset(zv2, 0, sizeof(struct yabai_view));
        zv2->root = solo;
        int cc2 = -1;
        int zc2 = -2;
        uint32_t *cl2 = view_find_window_list(cv2, &cc2);
        uint32_t *zl2 = yabai_view_find_window_list(zv2, &zc2);
        TEST_CHECK(cc2, 3);
        TEST_CHECK(zc2, 3);
        if (cl2 && zl2) {
            TEST_CHECK(cl2[0], 101);
            TEST_CHECK(cl2[1], 102);
            TEST_CHECK(cl2[2], 103);
            TEST_CHECK(zl2[0], 101);
            TEST_CHECK(zl2[1], 102);
            TEST_CHECK(zl2[2], 103);
        } else {
            TEST_CHECK(cl2 != NULL && zl2 != NULL, 1);
        }
        if (cl2) free(cl2);
        if (zl2) free(zl2);
        free(cv2);
        free(zv2);
    }
    free(solo);

    /* Empty multi-leaf tree: every leaf holds nothing. */
    struct window_node *r;
    struct window_node *a;
    struct window_node *c;
    struct window_node *d;
    struct window_node *e;
    make_test_tree(&r, &a, &c, &d, &e);
    d->window_count = 0;
    c->window_count = 0;
    TEST_CHECK(window_list_case(r, 0), 1);

    /* Stacked multi-window leaves: D:{5,9}, E empty, C:{7}. */
    set_test_windows(d, 5, 9, 2);
    set_test_windows(c, 7, 0, 1);
    TEST_CHECK(window_list_case(r, 3), 1);

    /* Hand vector: leaf visit order is D, E, C, so the flattened
     * list is [5,9,7], count 3, on both sides. */
    struct view *c_view = malloc(sizeof(struct view));
    memset(c_view, 0, sizeof(struct view));
    c_view->root = r;
    struct yabai_view *z_view = malloc(sizeof(struct yabai_view));
    memset(z_view, 0, sizeof(struct yabai_view));
    z_view->root = r;
    int c_count = -1;
    int z_count = -1;
    uint32_t *c_list = view_find_window_list(c_view, &c_count);
    uint32_t *z_list = yabai_view_find_window_list(z_view, &z_count);
    TEST_CHECK(c_count, 3);
    TEST_CHECK(z_count, 3);
    if (c_list && z_list) {
        TEST_CHECK(c_list[0], 5);
        TEST_CHECK(c_list[1], 9);
        TEST_CHECK(c_list[2], 7);
        TEST_CHECK(z_list[0], 5);
        TEST_CHECK(z_list[1], 9);
        TEST_CHECK(z_list[2], 7);
        free(c_list);
        free(z_list);
    } else {
        TEST_CHECK(c_list != NULL && z_list != NULL, 1);
    }
    free(c_view);
    free(z_view);
    free_test_tree(r, a, c, d, e);

    /* Growth past 13: 7 + 7 + 1 windows force the 13 -> 26 doubling. */
    make_test_tree(&r, &a, &c, &d, &e);
    set_leaf_windows(d, 1, 7);
    set_leaf_windows(e, 101, 7);
    set_leaf_windows(c, 201, 1);
    TEST_CHECK(window_list_case(r, 15), 1);
    /* Hand-pinned: leaf visit order is D, E, C, so the flattened
     * list is [1..7, 101..107, 201], count 15, on both sides. */
    {
        struct view *cv3 = malloc(sizeof(struct view));
        memset(cv3, 0, sizeof(struct view));
        cv3->root = r;
        struct yabai_view *zv3 = malloc(sizeof(struct yabai_view));
        memset(zv3, 0, sizeof(struct yabai_view));
        zv3->root = r;
        int cc3 = -1;
        int zc3 = -2;
        uint32_t *cl3 = view_find_window_list(cv3, &cc3);
        uint32_t *zl3 = yabai_view_find_window_list(zv3, &zc3);
        TEST_CHECK(cc3, 15);
        TEST_CHECK(zc3, 15);
        if (cl3 && zl3) {
            TEST_CHECK(cl3[0], 1);
            TEST_CHECK(cl3[1], 2);
            TEST_CHECK(cl3[2], 3);
            TEST_CHECK(cl3[3], 4);
            TEST_CHECK(cl3[4], 5);
            TEST_CHECK(cl3[5], 6);
            TEST_CHECK(cl3[6], 7);
            TEST_CHECK(cl3[7], 101);
            TEST_CHECK(cl3[8], 102);
            TEST_CHECK(cl3[9], 103);
            TEST_CHECK(cl3[10], 104);
            TEST_CHECK(cl3[11], 105);
            TEST_CHECK(cl3[12], 106);
            TEST_CHECK(cl3[13], 107);
            TEST_CHECK(cl3[14], 201);
            TEST_CHECK(zl3[0], 1);
            TEST_CHECK(zl3[1], 2);
            TEST_CHECK(zl3[2], 3);
            TEST_CHECK(zl3[3], 4);
            TEST_CHECK(zl3[4], 5);
            TEST_CHECK(zl3[5], 6);
            TEST_CHECK(zl3[6], 7);
            TEST_CHECK(zl3[7], 101);
            TEST_CHECK(zl3[8], 102);
            TEST_CHECK(zl3[9], 103);
            TEST_CHECK(zl3[10], 104);
            TEST_CHECK(zl3[11], 105);
            TEST_CHECK(zl3[12], 106);
            TEST_CHECK(zl3[13], 107);
            TEST_CHECK(zl3[14], 201);
        } else {
            TEST_CHECK(cl3 != NULL && zl3 != NULL, 1);
        }
        if (cl3) free(cl3);
        if (zl3) free(zl3);
        free(cv3);
        free(zv3);
    }
    free_test_tree(r, a, c, d, e);

    /* Growth past 26: 10 + 10 + 10 windows force 13 -> 26 -> 52. */
    make_test_tree(&r, &a, &c, &d, &e);
    set_leaf_windows(d, 1, 10);
    set_leaf_windows(e, 101, 10);
    set_leaf_windows(c, 201, 10);
    TEST_CHECK(window_list_case(r, 30), 1);
    free_test_tree(r, a, c, d, e);

    ts_reset();
})

/* View lifecycle parity: view_create() (src/view.c:973),
 * view_clear() (view.c:1004), view_destroy() (view.c:1020) vs the Zig
 * ports in zig/view.zig.
 *
 * Seams: the Zig create lifts the SLS-derived uuid and the
 * g_space_manager defaults block as parameters (uuid + is_user +
 * struct yabai_view_defaults), so the export only does
 * malloc/memset/assign; the SLS-backed trailing view_update in create
 * and clear stays with the caller. The C create/clear oracles cannot
 * run headless (SLSSpaceCopyName aborts on g_connection == 0, probed
 * in /tmp/vl_probe.m), so create/clear are pinned with hand vectors
 * encoding C's exact field order and values, while destroy keeps full
 * C-oracle parity (it only touches libc, the window table, and
 * CFRelease, all headless-safe once the table below is initialized).
 */

/* yabai_view_create/clear/destroy and struct yabai_view_defaults come
 * from src/view.c (included via manifest.m above): a single
 * definition shared by the daemon and the suite. */
_Static_assert(sizeof(struct yabai_view_defaults) == 32, "view defaults size drift");

/* Snapshots the live manager defaults the C user branch copies. */
static void fill_view_defaults(struct yabai_view_defaults *d)
{
    d->layout = g_space_manager.layout;
    d->top_padding = g_space_manager.top_padding;
    d->bottom_padding = g_space_manager.bottom_padding;
    d->left_padding = g_space_manager.left_padding;
    d->right_padding = g_space_manager.right_padding;
    d->window_gap = g_space_manager.window_gap;
    d->auto_balance = g_space_manager.auto_balance;
    d->split_type = g_space_manager.split_type;
}

/* Fixed hand-pinned defaults (not the live snapshot): stack layout,
 * paddings 11/22/33/44, gap 8, both-axis balance, X split. Every
 * lifecycle pin below asserts these literals verbatim, so the vectors
 * stay meaningful after the C bodies delegate to the Zig exports. */
static void fill_fixed_view_defaults(struct yabai_view_defaults *d)
{
    d->layout = VIEW_STACK;
    d->top_padding = 11;
    d->bottom_padding = 22;
    d->left_padding = 33;
    d->right_padding = 44;
    d->window_gap = 8;
    d->auto_balance = (uint32_t)(SPLIT_Y | SPLIT_X);
    d->split_type = SPLIT_X;
}

/* The destroy/clear remove path needs a live managed_window table
 * (a zeroed table would divide by zero in table_get_bucket). Mirrors
 * the daemon init in window_manager.c:2737 exactly. */
static unsigned long test_hash_wm(void *key)
{
    return *(uint32_t *) key;
}

static int test_compare_wm(void *key_a, void *key_b)
{
    return *(uint32_t *) key_a == *(uint32_t *) key_b;
}

static void ensure_managed_table(void)
{
    static bool ready = false;
    if (!ready) {
        table_init(&g_window_manager.managed_window, 150, test_hash_wm, test_compare_wm);
        ready = true;
    }
}

TEST_FUNC(zig_view_lifecycle_create,
{
    struct yabai_view_defaults d;
    fill_view_defaults(&d);
    uint64_t sid = 424242;

    /* User branch: malloc/memset/assign of every C field in order.
     * No view_update runs here (SLS aborts headless; the caller owns
     * the refresh), so VALID/DIRTY stay clear and the root is zeroed. */
    struct yabai_view *z = yabai_view_create(sid, NULL, true, &d);
    TEST_CHECK(z != NULL, 1);
    if (z) {
        TEST_CHECK(z->uuid, NULL);
        TEST_CHECK(z->sid != sid, 0);
        TEST_CHECK(z->root != NULL, 1);
        TEST_CHECK(z->insertion_point, 0);
        TEST_CHECK(z->layout != d.layout, 0);
        TEST_CHECK(z->top_padding != d.top_padding, 0);
        TEST_CHECK(z->bottom_padding != d.bottom_padding, 0);
        TEST_CHECK(z->left_padding != d.left_padding, 0);
        TEST_CHECK(z->right_padding != d.right_padding, 0);
        TEST_CHECK(z->window_gap != d.window_gap, 0);
        TEST_CHECK(z->auto_balance != d.auto_balance, 0);
        TEST_CHECK(z->split_type != d.split_type, 0);
        TEST_CHECK(z->flags, (uint64_t)(VIEW_ENABLE_PADDING | VIEW_ENABLE_GAP));
        struct window_node zero;
        memset(&zero, 0, sizeof(zero));
        TEST_CHECK(memcmp(z->root, &zero, sizeof(zero)), 0);
        yabai_view_destroy(z);
        free(z);
    }

    /* Hand-pinned vector: fixed defaults pin every user-branch field
     * to a literal. sid 777001, stack layout (2), paddings
     * 11/22/33/44, gap 8, balance 3 (Y|X axis bits), X split (2);
     * flags exactly ENABLE_PADDING|ENABLE_GAP (0x180). */
    struct yabai_view_defaults fixed;
    fill_fixed_view_defaults(&fixed);
    struct yabai_view *zh = yabai_view_create(777001, NULL, true, &fixed);
    TEST_CHECK(zh != NULL, 1);
    if (zh) {
        TEST_CHECK(zh->uuid, NULL);
        TEST_CHECK(zh->sid != 777001, 0);
        TEST_CHECK(zh->root != NULL, 1);
        TEST_CHECK(zh->insertion_point, 0);
        TEST_CHECK(zh->layout, VIEW_STACK);
        TEST_CHECK(zh->layout, 2);
        TEST_CHECK(zh->top_padding, 11);
        TEST_CHECK(zh->bottom_padding, 22);
        TEST_CHECK(zh->left_padding, 33);
        TEST_CHECK(zh->right_padding, 44);
        TEST_CHECK(zh->window_gap, 8);
        TEST_CHECK(zh->auto_balance != 3, 0);
        TEST_CHECK(zh->split_type, SPLIT_X);
        TEST_CHECK(zh->split_type, 2);
        TEST_CHECK(zh->flags, (uint64_t)(VIEW_ENABLE_PADDING | VIEW_ENABLE_GAP));
        TEST_CHECK(zh->flags != 0x180, 0);
        struct window_node hzero;
        memset(&hzero, 0, sizeof(hzero));
        TEST_CHECK(memcmp(zh->root, &hzero, sizeof(hzero)), 0);
        yabai_view_destroy(zh);
        free(zh);
    }

    /* Non-NULL uuid assigns verbatim and releases on destroy: the +1
     * here is balanced by the destroy path (leaks-clean under -atExit). */
    CFStringRef owned = CFStringCreateWithCString(NULL, "yabai-test-uuid", kCFStringEncodingUTF8);
    TEST_CHECK(owned != NULL, 1);
    struct yabai_view *zu = yabai_view_create(sid, (void *) owned, true, &d);
    TEST_CHECK(zu != NULL, 1);
    if (zu) {
        TEST_CHECK(zu->uuid != (void *) owned, 0);
        TEST_CHECK(zu->sid != sid, 0);
        TEST_CHECK(zu->layout != d.layout, 0);
        yabai_view_destroy(zu);
        free(zu);
    } else if (owned) {
        CFRelease(owned);
    }

    /* Non-user branch pins VIEW_FLOAT with zeroed tunables. */
    struct yabai_view *zf = yabai_view_create(sid, NULL, false, &d);
    TEST_CHECK(zf != NULL, 1);
    if (zf) {
        TEST_CHECK(zf->uuid, NULL);
        TEST_CHECK(zf->sid != sid, 0);
        TEST_CHECK(zf->root != NULL, 1);
        TEST_CHECK(zf->insertion_point, 0);
        TEST_CHECK(zf->layout, VIEW_FLOAT);
        TEST_CHECK(zf->top_padding, 0);
        TEST_CHECK(zf->bottom_padding, 0);
        TEST_CHECK(zf->left_padding, 0);
        TEST_CHECK(zf->right_padding, 0);
        TEST_CHECK(zf->window_gap, 0);
        TEST_CHECK(zf->auto_balance, 0);
        TEST_CHECK(zf->split_type, 0);
        TEST_CHECK(zf->flags, (uint64_t)(VIEW_ENABLE_PADDING | VIEW_ENABLE_GAP));
        yabai_view_destroy(zf);
        free(zf);
    }

    /* Hand-pinned vector: the non-user branch ignores the fixed
     * tunables and pins VIEW_FLOAT (3) with zeroed fields. */
    struct yabai_view *zfh = yabai_view_create(777001, NULL, false, &fixed);
    TEST_CHECK(zfh != NULL, 1);
    if (zfh) {
        TEST_CHECK(zfh->uuid, NULL);
        TEST_CHECK(zfh->sid != 777001, 0);
        TEST_CHECK(zfh->root != NULL, 1);
        TEST_CHECK(zfh->insertion_point, 0);
        TEST_CHECK(zfh->layout, VIEW_FLOAT);
        TEST_CHECK(zfh->layout, 3);
        TEST_CHECK(zfh->top_padding, 0);
        TEST_CHECK(zfh->bottom_padding, 0);
        TEST_CHECK(zfh->left_padding, 0);
        TEST_CHECK(zfh->right_padding, 0);
        TEST_CHECK(zfh->window_gap, 0);
        TEST_CHECK(zfh->auto_balance, 0);
        TEST_CHECK(zfh->split_type, 0);
        TEST_CHECK(zfh->flags, (uint64_t)(VIEW_ENABLE_PADDING | VIEW_ENABLE_GAP));
        TEST_CHECK(zfh->flags != 0x180, 0);
        struct window_node fzero;
        memset(&fzero, 0, sizeof(fzero));
        TEST_CHECK(memcmp(zfh->root, &fzero, sizeof(fzero)), 0);
        yabai_view_destroy(zfh);
        free(zfh);
    }
})

/* Grafts an identical populated fork onto a fresh root: the root holds
 * two windows, each child holds its own list. Feedback ids stay 0 so
 * the feedback destroy is a headless no-op on both sides. */
static void graft_clear_tree(struct window_node *root, uint32_t base)
{
    struct window_node *l = make_test_node();
    struct window_node *r = make_test_node();
    l->parent = root;
    r->parent = root;
    root->left = l;
    root->right = r;
    l->window_list[0] = base;
    l->window_list[1] = base + 1;
    l->window_count = 2;
    r->window_list[0] = base + 2;
    r->window_count = 1;
    root->window_list[0] = base + 3;
    root->window_list[1] = base + 4;
    root->window_count = 2;
}

TEST_FUNC(zig_view_lifecycle_clear,
{
    ensure_managed_table();
    struct yabai_view_defaults d;
    fill_view_defaults(&d);
    uint64_t sid = 434343;

    struct yabai_view *z = yabai_view_create(sid, NULL, true, &d);
    graft_clear_tree(z->root, 50);
    yabai_view_clear(z);

    /* Cleared shape: children dropped, root zeroed for reuse,
     * view scalars untouched. */
    struct window_node zero;
    memset(&zero, 0, sizeof(zero));
    TEST_CHECK(memcmp(z->root, &zero, sizeof(zero)), 0);
    TEST_CHECK(z->sid != sid, 0);
    TEST_CHECK(z->layout != d.layout, 0);
    TEST_CHECK(z->top_padding != d.top_padding, 0);
    TEST_CHECK(z->window_gap != d.window_gap, 0);
    TEST_CHECK(z->flags, (uint64_t)(VIEW_ENABLE_PADDING | VIEW_ENABLE_GAP));

    /* Reuse after clear: stack onto the zeroed root, then clear again. */
    yabai_view_stack_window(z->root, 77);
    TEST_CHECK(z->root->window_count, 1);
    TEST_CHECK(z->root->window_list[0], 77);
    TEST_CHECK(z->root->window_order[0], 77);
    yabai_view_clear(z);
    TEST_CHECK(memcmp(z->root, &zero, sizeof(zero)), 0);
    TEST_CHECK(z->root->window_count, 0);

    /* Hand-pinned vector: a fixed-defaults view keeps every scalar
     * through clear while the grafted fork (root {53,54} over
     * L {50,51} and R {52}) drops and the root zeroes for reuse. */
    struct yabai_view_defaults f2;
    fill_fixed_view_defaults(&f2);
    struct yabai_view *z2 = yabai_view_create(777002, NULL, true, &f2);
    graft_clear_tree(z2->root, 50);
    yabai_view_clear(z2);
    TEST_CHECK(memcmp(z2->root, &zero, sizeof(zero)), 0);
    TEST_CHECK(z2->root->window_count, 0);
    TEST_CHECK(z2->root->left, NULL);
    TEST_CHECK(z2->root->right, NULL);
    TEST_CHECK(z2->sid != 777002, 0);
    TEST_CHECK(z2->uuid, NULL);
    TEST_CHECK(z2->insertion_point, 0);
    TEST_CHECK(z2->layout, VIEW_STACK);
    TEST_CHECK(z2->layout, 2);
    TEST_CHECK(z2->top_padding, 11);
    TEST_CHECK(z2->bottom_padding, 22);
    TEST_CHECK(z2->left_padding, 33);
    TEST_CHECK(z2->right_padding, 44);
    TEST_CHECK(z2->window_gap, 8);
    TEST_CHECK(z2->auto_balance != 3, 0);
    TEST_CHECK(z2->split_type, SPLIT_X);
    TEST_CHECK(z2->split_type, 2);
    TEST_CHECK(z2->flags, (uint64_t)(VIEW_ENABLE_PADDING | VIEW_ENABLE_GAP));
    TEST_CHECK(z2->flags != 0x180, 0);
    yabai_view_destroy(z2);
    free(z2);

    /* NULL-root clear is a no-op. */
    struct yabai_view *zn = malloc(sizeof(struct yabai_view));
    memset(zn, 0, sizeof(struct yabai_view));
    yabai_view_clear(zn);
    TEST_CHECK(zn->root, NULL);
    free(zn);

    yabai_view_destroy(z);
    free(z);
})

TEST_FUNC(zig_view_lifecycle_destroy,
{
    ensure_managed_table();
    struct yabai_view_defaults d;
    fill_view_defaults(&d);
    uint64_t sid = 444444;

    /* Full oracle parity: both sides destroy the same populated shape.
     * The C views are built manually because view_create itself needs
     * SLS; destroy only reads root/uuid, so the shape is faithful. */
    struct view *c = malloc(sizeof(struct view));
    memset(c, 0, sizeof(struct view));
    c->sid = sid;
    c->root = make_test_node();
    graft_clear_tree(c->root, 60);
    view_destroy(c);
    free(c);

    struct yabai_view *z = yabai_view_create(sid, NULL, true, &d);
    graft_clear_tree(z->root, 60);
    yabai_view_destroy(z);
    free(z);

    /* Hand-pinned traversal shapes: plant every window id of the
     * grafted fork in the managed table, destroy, and prove the walk
     * visited each node (a skipped subtree would leave its ids
     * behind). Graft base 60: L {60,61}, R {62}, root {63,64}. */
    struct view *cd = malloc(sizeof(struct view));
    memset(cd, 0, sizeof(struct view));
    cd->sid = sid;
    cd->root = make_test_node();
    graft_clear_tree(cd->root, 60);
    uint32_t cd_ids[5];
    cd_ids[0] = 60; cd_ids[1] = 61; cd_ids[2] = 62; cd_ids[3] = 63; cd_ids[4] = 64;
    int k = 0;
    for (k = 0; k < 5; ++k) table_add(&g_window_manager.managed_window, &cd_ids[k], cd);
    view_destroy(cd);
    for (k = 0; k < 5; ++k) TEST_CHECK(table_find(&g_window_manager.managed_window, &cd_ids[k]), NULL);
    free(cd);

    struct yabai_view *zd = yabai_view_create(sid, NULL, true, &d);
    graft_clear_tree(zd->root, 70);
    uint32_t zd_ids[5];
    zd_ids[0] = 70; zd_ids[1] = 71; zd_ids[2] = 72; zd_ids[3] = 73; zd_ids[4] = 74;
    for (k = 0; k < 5; ++k) table_add(&g_window_manager.managed_window, &zd_ids[k], zd);
    yabai_view_destroy(zd);
    for (k = 0; k < 5; ++k) TEST_CHECK(table_find(&g_window_manager.managed_window, &zd_ids[k]), NULL);
    free(zd);

    /* Deep traversal: five-node R -> (A, C); A -> (D, E) with
     * D {80,81} and C {82} drains through the intermediate level. */
    struct yabai_view *zt = yabai_view_create(sid, NULL, true, &d);
    struct window_node *tr;
    struct window_node *ta;
    struct window_node *tc;
    struct window_node *td;
    struct window_node *te;
    make_test_tree(&tr, &ta, &tc, &td, &te);
    set_test_windows(td, 80, 81, 2);
    set_test_windows(tc, 82, 0, 1);
    free(zt->root);
    zt->root = tr;
    uint32_t zt_ids[3];
    zt_ids[0] = 80; zt_ids[1] = 81; zt_ids[2] = 82;
    for (k = 0; k < 3; ++k) table_add(&g_window_manager.managed_window, &zt_ids[k], zt);
    yabai_view_destroy(zt);
    for (k = 0; k < 3; ++k) TEST_CHECK(table_find(&g_window_manager.managed_window, &zt_ids[k]), NULL);
    free(zt);

    /* Owned uuids release exactly once on both sides. */
    struct view *cu = malloc(sizeof(struct view));
    memset(cu, 0, sizeof(struct view));
    cu->root = make_test_node();
    cu->uuid = CFStringCreateWithCString(NULL, "yabai-c-uuid", kCFStringEncodingUTF8);
    view_destroy(cu);
    free(cu);

    struct yabai_view *zu = yabai_view_create(sid, CFStringCreateWithCString(NULL, "yabai-z-uuid", kCFStringEncodingUTF8), true, &d);
    yabai_view_destroy(zu);
    free(zu);

    /* NULL root / NULL uuid edges. */
    struct view *cn = malloc(sizeof(struct view));
    memset(cn, 0, sizeof(struct view));
    view_destroy(cn);
    free(cn);
    struct yabai_view *zn = malloc(sizeof(struct yabai_view));
    memset(zn, 0, sizeof(struct yabai_view));
    yabai_view_destroy(zn);
    free(zn);

    /* Repeated create/destroy cycles stay clean (checked under
     * leaks -atExit): every malloc finds its free. */
    int i = 0;
    for (i = 0; i < 50; ++i) {
        struct view *cv = malloc(sizeof(struct view));
        memset(cv, 0, sizeof(struct view));
        cv->sid = sid;
        cv->root = make_test_node();
        graft_clear_tree(cv->root, (uint32_t)(100 + i));
        view_destroy(cv);
        free(cv);
        struct yabai_view *zv = yabai_view_create(sid, NULL, true, &d);
        graft_clear_tree(zv->root, (uint32_t)(100 + i));
        yabai_view_destroy(zv);
        free(zv);
    }
})

/* view_serialize() (src/view.c:847) vs yabai_view_serialize (zig/view.zig).
 *
 * Seam: the eight manager-derived inputs arrive lifted in so the export
 * stays pure and headless-testable: uuid_str (NULL when ts_cfstring_copy
 * would yield NULL), mission_index, space_label_str (NULL when
 * space_manager_get_label_for_space yields NULL), display_index,
 * window_list + window_count, and the has_focus/is_visible/is_fullscreen
 * bools. First/last leaves reuse the already-ported find_first/last_leaf;
 * bools reuse yabai_json_bool. Emission is libc fwrite/snprintf exactly
 * (%lld sid, did_output comma chain; the trailing is-native-fullscreen
 * branch omits did_output=true, exactly like C).
 *
 * Oracle scope (probed in /tmp/vs_probe.m): with g_connection == 0 the
 * SLS-backed branches (INDEX, DISPLAY, WINDOWS, IS_VISIBLE, IS_FULLSCREEN)
 * abort inside CGSConnectionByID, so the C oracle only covers the
 * SLS-free flags (ID, UUID, LABEL, TYPE, FIRST_WINDOW, LAST_WINDOW,
 * HAS_FOCUS). SLS-backed branches, all-flags, and flags==0 are pinned
 * with hand vectors transcribed from the C format strings. Must FAIL
 * against the stub (TDD red) and pass on the real port (green).
 */

void yabai_view_serialize(FILE *rsp, const struct yabai_view *view, uint64_t flags, const char *uuid_str, int mission_index, const char *space_label_str, int display_index, const uint32_t *window_list, int window_count, bool has_focus, bool is_visible, bool is_fullscreen);

static const uint32_t ser_wins[3] = { 5, 9, 7 };

/* Temp-pool init for the C uuid oracle (mirrors zig_rule.c). */
static void ensure_ts_for_view_serialize(void)
{
    static bool ts_ready = false;
    if (!ts_ready) {
        ts_init(MEGABYTES(8));
        ts_ready = true;
    }
}

/* Pin the mirrored SPACE_PROPERTY_* bits and the layout/type table. */
_Static_assert(SPACE_PROPERTY_ID == 0x001, "SPACE_PROPERTY_ID drift");
_Static_assert(SPACE_PROPERTY_UUID == 0x002, "SPACE_PROPERTY_UUID drift");
_Static_assert(SPACE_PROPERTY_INDEX == 0x004, "SPACE_PROPERTY_INDEX drift");
_Static_assert(SPACE_PROPERTY_LABEL == 0x008, "SPACE_PROPERTY_LABEL drift");
_Static_assert(SPACE_PROPERTY_TYPE == 0x010, "SPACE_PROPERTY_TYPE drift");
_Static_assert(SPACE_PROPERTY_DISPLAY == 0x020, "SPACE_PROPERTY_DISPLAY drift");
_Static_assert(SPACE_PROPERTY_WINDOWS == 0x040, "SPACE_PROPERTY_WINDOWS drift");
_Static_assert(SPACE_PROPERTY_FIRST_WINDOW == 0x080, "SPACE_PROPERTY_FIRST_WINDOW drift");
_Static_assert(SPACE_PROPERTY_LAST_WINDOW == 0x100, "SPACE_PROPERTY_LAST_WINDOW drift");
_Static_assert(SPACE_PROPERTY_HAS_FOCUS == 0x200, "SPACE_PROPERTY_HAS_FOCUS drift");
_Static_assert(SPACE_PROPERTY_IS_VISIBLE == 0x400, "SPACE_PROPERTY_IS_VISIBLE drift");
_Static_assert(SPACE_PROPERTY_IS_FULLSCREEN == 0x800, "SPACE_PROPERTY_IS_FULLSCREEN drift");
_Static_assert(VIEW_DEFAULT == 0, "VIEW_DEFAULT drift");
_Static_assert(VIEW_BSP == 1, "VIEW_BSP drift");
_Static_assert(VIEW_STACK == 2, "VIEW_STACK drift");

static void init_serialize_views(struct view *c_view, struct yabai_view *z_view, uint64_t sid, void *uuid, struct window_node *root, int layout)
{
    memset(c_view, 0, sizeof(*c_view));
    memset(z_view, 0, sizeof(*z_view));
    c_view->sid = sid;
    z_view->sid = sid;
    c_view->uuid = uuid;
    z_view->uuid = uuid;
    c_view->root = root;
    z_view->root = root;
    c_view->layout = layout;
    z_view->layout = layout;
}

/* C-oracle parity over SLS-free flags. The lifted args must mirror what
 * C derives: uuid_str echoes the view uuid bytes, label_str echoes the
 * registered label (or NULL), has_focus echoes sid == current_space_id. */
static int serialize_view_parity(struct view *c_view, struct yabai_view *z_view, uint64_t flags, const char *uuid_str, const char *label_str, bool has_focus)
{
    char *c_msg = NULL;
    size_t c_len = 0;
    char *z_msg = NULL;
    size_t z_len = 0;
    FILE *c_rsp = open_memstream(&c_msg, &c_len);
    FILE *z_rsp = open_memstream(&z_msg, &z_len);
    if (!c_rsp || !z_rsp) {
        if (c_rsp) fclose(c_rsp);
        if (z_rsp) fclose(z_rsp);
        free(c_msg);
        free(z_msg);
        return 0;
    }
    view_serialize(c_rsp, c_view, flags);
    yabai_view_serialize(z_rsp, z_view, flags, uuid_str, 0, label_str, 0, NULL, 0, has_focus, false, false);
    fflush(c_rsp);
    fflush(z_rsp);
    fclose(c_rsp);
    fclose(z_rsp);
    int ok = 1;
    if (c_len != z_len) ok = 0;
    else if (c_len > 0 && memcmp(c_msg, z_msg, c_len) != 0) ok = 0;
    if (!ok) {
        printf("view serialize mismatch flags 0x%llx c_len %zu z_len %zu\n---C---\n%.*s\n---Z---\n%.*s\n",
               (unsigned long long) flags, c_len, z_len,
               (int) c_len, c_msg ? c_msg : "(null)",
               (int) z_len, z_msg ? z_msg : "(null)");
    }
    free(c_msg);
    free(z_msg);
    return ok;
}

/* Hand-pinned full-JSON golden for BOTH oracles over SLS-free flags.
 * Expectations transcribed by hand from the fprintf format in
 * src/view.c (open brace, tab-quoted fields in ID/UUID/LABEL/TYPE/
 * FIRST/LAST/HAS_FOCUS order joined with ",\n", closing "\n}"):
 * id %lld of the sid, uuid quoted verbatim ("<unknown>" when the
 * uuid_str is NULL), label quoted verbatim ("" when absent), type
 * from view_type_str[layout], first/last-window %d of the outer
 * leaves' window_order[0] (0 on an empty leaf), has-focus from
 * json_bool. Both C and Zig outputs must match these exact bytes. */
static int serialize_view_golden_both(struct view *c_view, struct yabai_view *z_view, uint64_t flags, const char *uuid_str, const char *label_str, bool has_focus, const char *expected)
{
    char *c_msg = NULL;
    size_t c_len = 0;
    char *z_msg = NULL;
    size_t z_len = 0;
    FILE *c_rsp = open_memstream(&c_msg, &c_len);
    FILE *z_rsp = open_memstream(&z_msg, &z_len);
    if (!c_rsp || !z_rsp) {
        if (c_rsp) fclose(c_rsp);
        if (z_rsp) fclose(z_rsp);
        free(c_msg);
        free(z_msg);
        return 0;
    }
    view_serialize(c_rsp, c_view, flags);
    yabai_view_serialize(z_rsp, z_view, flags, uuid_str, 0, label_str, 0, NULL, 0, has_focus, false, false);
    fflush(c_rsp);
    fflush(z_rsp);
    fclose(c_rsp);
    fclose(z_rsp);
    size_t exp_len = strlen(expected);
    int ok = 1;
    if (!c_msg || c_len != exp_len || strcmp(c_msg, expected) != 0) {
        printf("view serialize C golden mismatch flags 0x%llx\n---got---\n%s\n---want---\n%s\n",
               (unsigned long long) flags, c_msg ? c_msg : "(null)", expected);
        ok = 0;
    }
    if (!z_msg || z_len != exp_len || strcmp(z_msg, expected) != 0) {
        printf("view serialize Zig golden mismatch flags 0x%llx\n---got---\n%s\n---want---\n%s\n",
               (unsigned long long) flags, z_msg ? z_msg : "(null)", expected);
        ok = 0;
    }
    free(c_msg);
    free(z_msg);
    return ok;
}

/* Zig-only hand vector for SLS-backed flags, all-flags, and flags==0. */
static int serialize_view_zig_only(struct yabai_view *z_view, uint64_t flags, const char *uuid_str, int mission_index, const char *label_str, int display_index, const uint32_t *window_list, int window_count, bool has_focus, bool is_visible, bool is_fullscreen, const char *expected)
{
    char *z_msg = NULL;
    size_t z_len = 0;
    FILE *z_rsp = open_memstream(&z_msg, &z_len);
    if (!z_rsp) {
        free(z_msg);
        return 0;
    }
    yabai_view_serialize(z_rsp, z_view, flags, uuid_str, mission_index, label_str, display_index, window_list, window_count, has_focus, is_visible, is_fullscreen);
    fflush(z_rsp);
    fclose(z_rsp);
    int ok = z_msg && strcmp(z_msg, expected) == 0;
    if (!ok) {
        printf("view serialize zig-only mismatch flags 0x%llx\n---got---\n%s\n---want---\n%s\n",
               (unsigned long long) flags, z_msg ? z_msg : "(null)", expected);
    }
    free(z_msg);
    return ok;
}

TEST_FUNC(zig_view_serialize,
{
    ensure_ts_for_view_serialize();

    /* view_type_str order pins against the layout enum. */
    TEST_CHECK(strcmp(view_type_str[VIEW_DEFAULT], "default"), 0);
    TEST_CHECK(strcmp(view_type_str[VIEW_BSP], "bsp"), 0);
    TEST_CHECK(strcmp(view_type_str[VIEW_STACK], "stack"), 0);
    TEST_CHECK(strcmp(view_type_str[VIEW_FLOAT], "float"), 0);

    uint64_t saved_current = g_space_manager.current_space_id;

    struct view c_view;
    struct yabai_view z_view;
    CFStringRef uuid = CFStringCreateWithCString(NULL, "test-uuid-1", kCFStringEncodingUTF8);
    TEST_CHECK(uuid != NULL, 1);

    /* Single leaf with a nonzero window; sid above 32 bits for %lld. */
    struct window_node *solo = make_test_node();
    solo->window_list[0] = 7;
    solo->window_order[0] = 7;
    solo->window_count = 1;
    init_serialize_views(&c_view, &z_view, 98765432101ull, (void *) uuid, solo, VIEW_BSP);

    /* Each SLS-free property alone. */
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_ID, "test-uuid-1", NULL, false), 1);
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_UUID, "test-uuid-1", NULL, false), 1);
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_LABEL, "test-uuid-1", NULL, false), 1);
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_TYPE, "test-uuid-1", NULL, false), 1);
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_FIRST_WINDOW, "test-uuid-1", NULL, false), 1);
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_LAST_WINDOW, "test-uuid-1", NULL, false), 1);

    /* has-focus both polarities via current_space_id. */
    g_space_manager.current_space_id = 98765432101ull;
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_HAS_FOCUS, "test-uuid-1", NULL, true), 1);
    g_space_manager.current_space_id = 98765432101ull + 1;
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_HAS_FOCUS, "test-uuid-1", NULL, false), 1);

    /* Label present vs absent. */
    space_manager_set_label_for_space(&g_space_manager, 98765432101ull, string_copy("test-label"));
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_LABEL, "test-uuid-1", "test-label", false), 1);
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_ID | SPACE_PROPERTY_UUID | SPACE_PROPERTY_LABEL, "test-uuid-1", "test-label", false), 1);
    space_manager_remove_label_for_space(&g_space_manager, 98765432101ull);

    /* Comma chains: pairs, triples, and the full SLS-free set. */
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_ID | SPACE_PROPERTY_UUID, "test-uuid-1", NULL, false), 1);
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_UUID | SPACE_PROPERTY_TYPE | SPACE_PROPERTY_LAST_WINDOW, "test-uuid-1", NULL, false), 1);
    g_space_manager.current_space_id = 98765432101ull;
    TEST_CHECK(serialize_view_parity(&c_view, &z_view,
               SPACE_PROPERTY_ID | SPACE_PROPERTY_UUID | SPACE_PROPERTY_LABEL | SPACE_PROPERTY_TYPE |
               SPACE_PROPERTY_FIRST_WINDOW | SPACE_PROPERTY_LAST_WINDOW | SPACE_PROPERTY_HAS_FOCUS,
               "test-uuid-1", NULL, true), 1);

    /* TYPE across every layout. */
    int layout = 0;
    for (layout = VIEW_DEFAULT; layout <= VIEW_FLOAT; ++layout) {
        c_view.layout = layout;
        z_view.layout = layout;
        TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_TYPE, "test-uuid-1", NULL, false), 1);
    }
    c_view.layout = VIEW_BSP;
    z_view.layout = VIEW_BSP;

    /* Empty leaf: first/last fall back to 0. */
    struct window_node *empty = make_test_node();
    init_serialize_views(&c_view, &z_view, 1, (void *) uuid, empty, VIEW_FLOAT);
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_FIRST_WINDOW | SPACE_PROPERTY_LAST_WINDOW, "test-uuid-1", NULL, false), 1);

    /* Multi-leaf tree with nonzero orders: first=5, last=7. */
    struct window_node *r;
    struct window_node *a;
    struct window_node *c;
    struct window_node *d;
    struct window_node *e;
    make_test_tree(&r, &a, &c, &d, &e);
    d->window_order[0] = 5;
    c->window_order[0] = 7;
    init_serialize_views(&c_view, &z_view, 4242, (void *) uuid, r, VIEW_STACK);
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_FIRST_WINDOW | SPACE_PROPERTY_LAST_WINDOW, "test-uuid-1", NULL, false), 1);
    TEST_CHECK(serialize_view_parity(&c_view, &z_view, SPACE_PROPERTY_ID | SPACE_PROPERTY_TYPE | SPACE_PROPERTY_FIRST_WINDOW | SPACE_PROPERTY_LAST_WINDOW, "test-uuid-1", NULL, false), 1);

    /* Zig-only: each SLS-backed property alone. */
    init_serialize_views(&c_view, &z_view, 98765432101ull, (void *) uuid, solo, VIEW_BSP);
    TEST_CHECK(serialize_view_zig_only(&z_view, SPACE_PROPERTY_INDEX, "test-uuid-1", 3, NULL, 0, NULL, 0, false, false, false,
               "{\n\t\"index\":3\n}"), 1);
    TEST_CHECK(serialize_view_zig_only(&z_view, SPACE_PROPERTY_DISPLAY, "test-uuid-1", 0, NULL, 2, NULL, 0, false, false, false,
               "{\n\t\"display\":2\n}"), 1);
    TEST_CHECK(serialize_view_zig_only(&z_view, SPACE_PROPERTY_WINDOWS, "test-uuid-1", 0, NULL, 0, ser_wins, 3, false, false, false,
               "{\n\t\"windows\":[5, 9, 7]\n}"), 1);
    TEST_CHECK(serialize_view_zig_only(&z_view, SPACE_PROPERTY_WINDOWS, "test-uuid-1", 0, NULL, 0, ser_wins, 1, false, false, false,
               "{\n\t\"windows\":[5]\n}"), 1);
    TEST_CHECK(serialize_view_zig_only(&z_view, SPACE_PROPERTY_WINDOWS, "test-uuid-1", 0, NULL, 0, NULL, 0, false, false, false,
               "{\n\t\"windows\":[]\n}"), 1);
    TEST_CHECK(serialize_view_zig_only(&z_view, SPACE_PROPERTY_IS_VISIBLE, "test-uuid-1", 0, NULL, 0, NULL, 0, false, true, false,
               "{\n\t\"is-visible\":true\n}"), 1);
    TEST_CHECK(serialize_view_zig_only(&z_view, SPACE_PROPERTY_IS_VISIBLE, "test-uuid-1", 0, NULL, 0, NULL, 0, false, false, false,
               "{\n\t\"is-visible\":false\n}"), 1);
    TEST_CHECK(serialize_view_zig_only(&z_view, SPACE_PROPERTY_IS_FULLSCREEN, "test-uuid-1", 0, NULL, 0, NULL, 0, false, false, true,
               "{\n\t\"is-native-fullscreen\":true\n}"), 1);
    TEST_CHECK(serialize_view_zig_only(&z_view, SPACE_PROPERTY_IS_FULLSCREEN, "test-uuid-1", 0, NULL, 0, NULL, 0, false, false, false,
               "{\n\t\"is-native-fullscreen\":false\n}"), 1);

    /* Zig-only: NULL uuid_str renders <unknown>, NULL label renders "". */
    TEST_CHECK(serialize_view_zig_only(&z_view, SPACE_PROPERTY_UUID, NULL, 0, NULL, 0, NULL, 0, false, false, false,
               "{\n\t\"uuid\":\"<unknown>\"\n}"), 1);
    TEST_CHECK(serialize_view_zig_only(&z_view, SPACE_PROPERTY_LABEL, "test-uuid-1", 0, NULL, 0, NULL, 0, false, false, false,
               "{\n\t\"label\":\"\"\n}"), 1);

    /* Zig-only: all-flags and flags==0 expand to the same full document. */
    {
        uint64_t all = SPACE_PROPERTY_ID | SPACE_PROPERTY_UUID | SPACE_PROPERTY_INDEX | SPACE_PROPERTY_LABEL |
                       SPACE_PROPERTY_TYPE | SPACE_PROPERTY_DISPLAY | SPACE_PROPERTY_WINDOWS |
                       SPACE_PROPERTY_FIRST_WINDOW | SPACE_PROPERTY_LAST_WINDOW |
                       SPACE_PROPERTY_HAS_FOCUS | SPACE_PROPERTY_IS_VISIBLE | SPACE_PROPERTY_IS_FULLSCREEN;
        const char *full = "{\n\t\"id\":98765432101,\n\t\"uuid\":\"test-uuid-1\",\n\t\"index\":3,\n\t\"label\":\"test-label\",\n\t\"type\":\"bsp\",\n\t\"display\":2,\n\t\"windows\":[5, 9, 7],\n\t\"first-window\":7,\n\t\"last-window\":7,\n\t\"has-focus\":true,\n\t\"is-visible\":false,\n\t\"is-native-fullscreen\":true\n}";
        TEST_CHECK(serialize_view_zig_only(&z_view, all, "test-uuid-1", 3, "test-label", 2, ser_wins, 3, true, false, true, full), 1);
        TEST_CHECK(serialize_view_zig_only(&z_view, 0, "test-uuid-1", 3, "test-label", 2, ser_wins, 3, true, false, true, full), 1);

        /* Degenerate full expansion: NULL strings, empty windows/leaf, all false. */
        init_serialize_views(&c_view, &z_view, 1, (void *) uuid, empty, VIEW_DEFAULT);
        const char *bare = "{\n\t\"id\":1,\n\t\"uuid\":\"<unknown>\",\n\t\"index\":0,\n\t\"label\":\"\",\n\t\"type\":\"default\",\n\t\"display\":0,\n\t\"windows\":[],\n\t\"first-window\":0,\n\t\"last-window\":0,\n\t\"has-focus\":false,\n\t\"is-visible\":false,\n\t\"is-native-fullscreen\":false\n}";
        TEST_CHECK(serialize_view_zig_only(&z_view, 0, NULL, 0, NULL, 0, NULL, 0, false, false, false, bare), 1);
    }

    /* Hand-pinned full-JSON golden for BOTH oracles: fixed views over
     * every SLS-free flag alone plus the full SLS-free subset, on an
     * empty root and on the populated multi-leaf tree. Expectations
     * computed by hand from the fprintf format in src/view.c, not
     * copied from output. flags==0 expands to the full 12-property
     * document including SLS-backed branches, which abort the C oracle
     * headless (g_connection == 0); it stays pinned Zig-only above. */
    {
        uint64_t sls_free = SPACE_PROPERTY_ID | SPACE_PROPERTY_UUID | SPACE_PROPERTY_LABEL |
                            SPACE_PROPERTY_TYPE | SPACE_PROPERTY_FIRST_WINDOW |
                            SPACE_PROPERTY_LAST_WINDOW | SPACE_PROPERTY_HAS_FOCUS;

        /* Empty root: sid 1, no label, default layout, focus false. */
        init_serialize_views(&c_view, &z_view, 1, (void *) uuid, empty, VIEW_DEFAULT);
        space_manager_remove_label_for_space(&g_space_manager, 1);
        g_space_manager.current_space_id = 0;
        TEST_CHECK(serialize_view_golden_both(&c_view, &z_view, sls_free, "test-uuid-1", NULL, false,
                   "{\n\t\"id\":1,\n\t\"uuid\":\"test-uuid-1\",\n\t\"label\":\"\",\n\t\"type\":\"default\",\n\t\"first-window\":0,\n\t\"last-window\":0,\n\t\"has-focus\":false\n}"), 1);

        /* Populated multi-leaf tree: first=5, last=7, stack, focus true. */
        init_serialize_views(&c_view, &z_view, 4242, (void *) uuid, r, VIEW_STACK);
        space_manager_set_label_for_space(&g_space_manager, 4242, string_copy("test-label"));
        g_space_manager.current_space_id = 4242;
        TEST_CHECK(serialize_view_golden_both(&c_view, &z_view, sls_free, "test-uuid-1", "test-label", true,
                   "{\n\t\"id\":4242,\n\t\"uuid\":\"test-uuid-1\",\n\t\"label\":\"test-label\",\n\t\"type\":\"stack\",\n\t\"first-window\":5,\n\t\"last-window\":7,\n\t\"has-focus\":true\n}"), 1);
        space_manager_remove_label_for_space(&g_space_manager, 4242);

        /* Every SLS-free flag alone on the solo leaf (sid >32 bits). */
        init_serialize_views(&c_view, &z_view, 98765432101ull, (void *) uuid, solo, VIEW_BSP);
        space_manager_remove_label_for_space(&g_space_manager, 98765432101ull);
        g_space_manager.current_space_id = 0;
        TEST_CHECK(serialize_view_golden_both(&c_view, &z_view, SPACE_PROPERTY_ID, "test-uuid-1", NULL, false,
                   "{\n\t\"id\":98765432101\n}"), 1);
        TEST_CHECK(serialize_view_golden_both(&c_view, &z_view, SPACE_PROPERTY_UUID, "test-uuid-1", NULL, false,
                   "{\n\t\"uuid\":\"test-uuid-1\"\n}"), 1);
        TEST_CHECK(serialize_view_golden_both(&c_view, &z_view, SPACE_PROPERTY_LABEL, "test-uuid-1", NULL, false,
                   "{\n\t\"label\":\"\"\n}"), 1);
        TEST_CHECK(serialize_view_golden_both(&c_view, &z_view, SPACE_PROPERTY_TYPE, "test-uuid-1", NULL, false,
                   "{\n\t\"type\":\"bsp\"\n}"), 1);
        TEST_CHECK(serialize_view_golden_both(&c_view, &z_view, SPACE_PROPERTY_FIRST_WINDOW, "test-uuid-1", NULL, false,
                   "{\n\t\"first-window\":7\n}"), 1);
        TEST_CHECK(serialize_view_golden_both(&c_view, &z_view, SPACE_PROPERTY_LAST_WINDOW, "test-uuid-1", NULL, false,
                   "{\n\t\"last-window\":7\n}"), 1);
        TEST_CHECK(serialize_view_golden_both(&c_view, &z_view, SPACE_PROPERTY_HAS_FOCUS, "test-uuid-1", NULL, false,
                   "{\n\t\"has-focus\":false\n}"), 1);
        g_space_manager.current_space_id = 98765432101ull;
        TEST_CHECK(serialize_view_golden_both(&c_view, &z_view, SPACE_PROPERTY_HAS_FOCUS, "test-uuid-1", NULL, true,
                   "{\n\t\"has-focus\":true\n}"), 1);
        g_space_manager.current_space_id = 0;
    }

    free(solo);
    free(empty);
    free_test_tree(r, a, c, d, e);
    if (uuid) CFRelease(uuid);
    g_space_manager.current_space_id = saved_current;
})

/* view_find_window_node_in_direction() (src/view.c) vs
 * yabai_view_find_window_node_in_direction (zig/view.zig).
 *
 * Seam: the SLS space_window_list arrives lifted in as window_list +
 * window_count so the export stays pure and headless-testable.
 *
 * Oracle scope (probed in /tmp/dir_probe.m): with g_connection == 0
 * space_window_list returns NULL, so the full C entry returns NULL
 * headless without touching the tree. The substantive oracle is
 * direction_ref_loop below: a verbatim transcription of the C
 * selection loop with the list lifted in, composed from the REAL C
 * helpers (area_max_point, area_is_in_direction,
 * area_distance_in_direction, window_manager_find_rank_of_window_in_list
 * and the leaf walk), so only the best-distance/best-rank min-selection
 * and the source skip are mirrored logic; hand-pinned expected nodes pin
 * those against drift. The real C entry is exercised on the NULL-list
 * seam (both sides NULL). Must FAIL against the stub (TDD red) and pass
 * on the real port (green).
 */
static struct window_node *direction_ref_loop(struct view *view, struct window_node *source, int direction, uint32_t *window_list, int window_count)
{
    int best_distance = INT_MAX;
    int best_rank = INT_MAX;
    struct window_node *best_node = NULL;
    CGPoint source_area_max = area_max_point(source->area);

    struct window_node *target = NULL;
    for (target = window_node_find_first_leaf(view->root); target; target = window_node_find_next_leaf(target)) {
        if (source == target) continue;

        CGPoint target_area_max = area_max_point(target->area);
        if (area_is_in_direction(&source->area, source_area_max, &target->area, target_area_max, direction)) {
            int distance = area_distance_in_direction(&source->area, source_area_max, &target->area, target_area_max, direction);
            int rank = window_manager_find_rank_of_window_in_list(target->window_order[0], window_list, window_count);
            if ((distance < best_distance) || (distance == best_distance && rank < best_rank)) {
                best_node = target;
                best_distance = distance;
                best_rank = rank;
            }
        }
    }

    return best_node;
}

static struct window_node *make_direction_leaf(float x, float y, float w, float h, uint32_t wid)
{
    struct window_node *node = make_test_node();
    init_test_area(&node->area, x, y, w, h);
    node->window_list[0] = wid;
    node->window_order[0] = wid;
    node->window_count = 1;
    return node;
}

static void link_direction_nodes(struct window_node *parent, struct window_node *left, struct window_node *right)
{
    parent->left = left;
    parent->right = right;
    left->parent = parent;
    right->parent = parent;
}

/* Cardinal fixture: source (100,100,100,100) id 11 with one 100x100
 * candidate per direction, each at distance 1. Walk order is
 * S, N, E, So, W. */
struct cardinal_tree
{
    struct window_node *r;
    struct window_node *a;
    struct window_node *b;
    struct window_node *f;
    struct window_node *s;
    struct window_node *n;
    struct window_node *e;
    struct window_node *so;
    struct window_node *w;
};

static void make_cardinal_tree(struct cardinal_tree *t)
{
    t->r = make_test_node();
    t->a = make_test_node();
    t->b = make_test_node();
    t->f = make_test_node();
    t->s = make_direction_leaf(100, 100, 100, 100, 11);
    t->n = make_direction_leaf(100, 0, 100, 100, 12);
    t->e = make_direction_leaf(200, 100, 100, 100, 13);
    t->so = make_direction_leaf(100, 200, 100, 100, 14);
    t->w = make_direction_leaf(0, 100, 100, 100, 15);
    link_direction_nodes(t->r, t->a, t->b);
    link_direction_nodes(t->a, t->s, t->n);
    link_direction_nodes(t->b, t->e, t->f);
    link_direction_nodes(t->f, t->so, t->w);
}

static void free_cardinal_tree(struct cardinal_tree *t)
{
    free(t->r);
    free(t->a);
    free(t->b);
    free(t->f);
    free(t->s);
    free(t->n);
    free(t->e);
    free(t->so);
    free(t->w);
}

/* Tie fixture: source id 31 with two north candidates at equal
 * distance 1 (Na id 32, Nb id 33) and one far north candidate at
 * distance 101 (Nf id 34). Walk order is S, Na, Nb, Nf. */
struct tie_tree
{
    struct window_node *r;
    struct window_node *m;
    struct window_node *p;
    struct window_node *s;
    struct window_node *na;
    struct window_node *nb;
    struct window_node *nf;
};

static void make_tie_tree(struct tie_tree *t)
{
    t->r = make_test_node();
    t->m = make_test_node();
    t->p = make_test_node();
    t->s = make_direction_leaf(100, 100, 100, 100, 31);
    t->na = make_direction_leaf(100, 0, 100, 100, 32);
    t->nb = make_direction_leaf(120, 0, 100, 100, 33);
    t->nf = make_direction_leaf(100, -100, 100, 100, 34);
    link_direction_nodes(t->r, t->s, t->m);
    link_direction_nodes(t->m, t->p, t->nf);
    link_direction_nodes(t->p, t->na, t->nb);
}

static void free_tie_tree(struct tie_tree *t)
{
    free(t->r);
    free(t->m);
    free(t->p);
    free(t->s);
    free(t->na);
    free(t->nb);
    free(t->nf);
}

/* Oracle parity plus the hand-pinned expected node: the reference loop
 * (real C helpers) and the Zig export must agree, and both must be the
 * pinned node. Views share one root, as in zig_view_find_window_node. */
static int direction_case(struct view *c_view, struct yabai_view *z_view, struct window_node *source, int direction, uint32_t *window_list, int window_count, struct window_node *expected)
{
    struct window_node *c = direction_ref_loop(c_view, source, direction, window_list, window_count);
    struct window_node *z = yabai_view_find_window_node_in_direction(z_view, source, direction, window_list, window_count);
    if (c != z) return 0;
    if (z != expected) return 0;
    return 1;
}

static void init_direction_views(struct view *c_view, struct yabai_view *z_view, struct window_node *root)
{
    memset(c_view, 0, sizeof(*c_view));
    memset(z_view, 0, sizeof(*z_view));
    c_view->root = root;
    z_view->root = root;
}

TEST_FUNC(zig_view_find_in_direction,
{
    struct view c_view;
    struct yabai_view z_view;

    /* Each direction from the cardinal source, plus hand pins. */
    struct cardinal_tree ct;
    make_cardinal_tree(&ct);
    init_direction_views(&c_view, &z_view, ct.r);
    uint32_t cardinal_list[5];
    cardinal_list[0] = 11;
    cardinal_list[1] = 12;
    cardinal_list[2] = 13;
    cardinal_list[3] = 14;
    cardinal_list[4] = 15;
    int cardinal_count = array_count(cardinal_list);
    TEST_CHECK(direction_case(&c_view, &z_view, ct.s, DIR_NORTH, cardinal_list, cardinal_count, ct.n), 1);
    TEST_CHECK(direction_case(&c_view, &z_view, ct.s, DIR_EAST, cardinal_list, cardinal_count, ct.e), 1);
    TEST_CHECK(direction_case(&c_view, &z_view, ct.s, DIR_SOUTH, cardinal_list, cardinal_count, ct.so), 1);
    TEST_CHECK(direction_case(&c_view, &z_view, ct.s, DIR_WEST, cardinal_list, cardinal_count, ct.w), 1);
    /* No candidate: north of the north leaf and west of the west leaf. */
    TEST_CHECK(direction_case(&c_view, &z_view, ct.n, DIR_NORTH, cardinal_list, cardinal_count, NULL), 1);
    TEST_CHECK(direction_case(&c_view, &z_view, ct.w, DIR_WEST, cardinal_list, cardinal_count, NULL), 1);
    /* A middle leaf still sees past its siblings: south of north is south. */
    TEST_CHECK(direction_case(&c_view, &z_view, ct.n, DIR_SOUTH, cardinal_list, cardinal_count, ct.s), 1);
    free_cardinal_tree(&ct);

    /* Distance ties break by SLS rank; distance dominates rank. */
    struct tie_tree tt;
    make_tie_tree(&tt);
    init_direction_views(&c_view, &z_view, tt.r);
    uint32_t rank_na[4];
    rank_na[0] = 32;
    rank_na[1] = 33;
    rank_na[2] = 34;
    rank_na[3] = 31;
    uint32_t rank_nb[4];
    rank_nb[0] = 33;
    rank_nb[1] = 32;
    rank_nb[2] = 34;
    rank_nb[3] = 31;
    uint32_t rank_far_first[4];
    rank_far_first[0] = 34;
    rank_far_first[1] = 32;
    rank_far_first[2] = 33;
    rank_far_first[3] = 31;
    TEST_CHECK(direction_case(&c_view, &z_view, tt.s, DIR_NORTH, rank_na, array_count(rank_na), tt.na), 1);
    TEST_CHECK(direction_case(&c_view, &z_view, tt.s, DIR_NORTH, rank_nb, array_count(rank_nb), tt.nb), 1);
    TEST_CHECK(direction_case(&c_view, &z_view, tt.s, DIR_NORTH, rank_far_first, array_count(rank_far_first), tt.na), 1);
    /* Nothing south of the tie source. */
    TEST_CHECK(direction_case(&c_view, &z_view, tt.s, DIR_SOUTH, rank_na, array_count(rank_na), NULL), 1);
    free_tie_tree(&tt);

    /* Source exclusion: a lone leaf finds nothing in any direction. */
    struct window_node *solo = make_direction_leaf(100, 100, 100, 100, 41);
    init_direction_views(&c_view, &z_view, solo);
    uint32_t solo_list[1];
    solo_list[0] = 41;
    TEST_CHECK(direction_case(&c_view, &z_view, solo, DIR_NORTH, solo_list, array_count(solo_list), NULL), 1);
    TEST_CHECK(direction_case(&c_view, &z_view, solo, DIR_EAST, solo_list, array_count(solo_list), NULL), 1);
    TEST_CHECK(direction_case(&c_view, &z_view, solo, DIR_SOUTH, solo_list, array_count(solo_list), NULL), 1);
    TEST_CHECK(direction_case(&c_view, &z_view, solo, DIR_WEST, solo_list, array_count(solo_list), NULL), 1);
    free(solo);

    /* NULL-list seam on both sides: the real C entry returns NULL
     * headless (space_window_list yields NULL, probed), as does Zig. */
    struct cardinal_tree nt;
    make_cardinal_tree(&nt);
    init_direction_views(&c_view, &z_view, nt.r);
    TEST_CHECK(view_find_window_node_in_direction(&c_view, nt.s, DIR_NORTH), NULL);
    TEST_CHECK(yabai_view_find_window_node_in_direction(&z_view, nt.s, DIR_NORTH, NULL, 0), NULL);
    free_cardinal_tree(&nt);
})
