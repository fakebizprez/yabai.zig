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
