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
    TEST_CHECK((void *) window_node_find_last_leaf(r) != (void *) yabai_window_node_find_last_leaf(r), 0);
    TEST_CHECK((void *) window_node_find_last_leaf(r) != (void *) c, 0);

    TEST_CHECK((void *) window_node_find_prev_leaf(e) != (void *) yabai_window_node_find_prev_leaf(e), 0);
    TEST_CHECK((void *) window_node_find_prev_leaf(e) != (void *) d, 0);
    TEST_CHECK((void *) window_node_find_prev_leaf(d) != (void *) yabai_window_node_find_prev_leaf(d), 0);
    TEST_CHECK(window_node_find_prev_leaf(d) != yabai_window_node_find_prev_leaf(d), 0);
    TEST_CHECK((void *) window_node_find_next_leaf(d) != (void *) yabai_window_node_find_next_leaf(d), 0);
    TEST_CHECK((void *) window_node_find_next_leaf(d) != (void *) e, 0);
    TEST_CHECK((void *) window_node_find_next_leaf(e) != (void *) yabai_window_node_find_next_leaf(e), 0);
    TEST_CHECK((void *) window_node_find_next_leaf(e) != (void *) c, 0);
    TEST_CHECK((void *) window_node_find_next_leaf(c) != (void *) yabai_window_node_find_next_leaf(c), 0);
    TEST_CHECK(window_node_find_next_leaf(c) != yabai_window_node_find_next_leaf(c), 0);

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
    TEST_CHECK(window_node_contains_window(e, 5) != yabai_window_node_contains_window(e, 5), 0);

    TEST_CHECK(window_node_index_of_window(d, 9) != yabai_window_node_index_of_window(d, 9), 0);
    TEST_CHECK(window_node_index_of_window(d, 9), 1);
    /* C returns 0 (not -1) for a missing window; the port must match. */
    TEST_CHECK(window_node_index_of_window(d, 6) != yabai_window_node_index_of_window(d, 6), 0);
    TEST_CHECK(window_node_index_of_window(d, 6), 0);

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

    struct window_node *c_node = make_test_node();
    struct window_node *z_node = make_test_node();
    set_stack_lists(c_node, 5, 9, 7, 9, 5, 3);
    set_stack_lists(z_node, 5, 9, 7, 9, 5, 3);
    struct window *window = make_test_window(42);
    view_stack_window_node(c_node, window);
    yabai_view_stack_window(z_node, 42);
    free(window);
    TEST_CHECK(c_node->window_list[2], 42);
    TEST_CHECK(z_node->window_list[2], 42);
    TEST_CHECK(c_node->window_order[0], 42);
    TEST_CHECK(z_node->window_order[0], 42);
    TEST_CHECK(c_node->window_count, 4);
    TEST_CHECK(z_node->window_count, 4);
    free(c_node);
    free(z_node);
})
