/* Regression tests for memory-safety fixes in zig/view.zig and
 * zig/rule.zig. Included after zig_view.c and zig_rule.c, so struct
 * yabai_view comes from zig_view.c; the prototypes are re-declared here
 * (identical redeclarations are legal C). */

#include <malloc/malloc.h>

struct window_node *yabai_window_node_find_first_leaf(struct window_node *root);
uint32_t *yabai_view_find_window_list(const struct yabai_view *view, int *window_count);
bool yabai_parse_rule(FILE *rsp, char **message, struct rule *rule, const struct token *token, uint32_t acting_did, uint64_t acting_sid);
void yabai_rule_serialize(FILE *rsp, struct rule *rule, int index, int did_index, int sid_index);
void yabai_rule_destroy(struct rule *rule);

static struct window_node *regress_leaf(uint32_t base, int count)
{
    struct window_node *node = calloc(1, sizeof(struct window_node));
    for (int i = 0; i < count; ++i) node->window_list[i] = base + (uint32_t) i;
    node->window_count = count;
    return node;
}

/* Runs the Zig export over root and checks count, order (ids are
 * sequential from base) and that the malloc block really holds them. */
static int regress_window_list_case(struct window_node *root, uint32_t base, int expected)
{
    struct yabai_view view;
    memset(&view, 0, sizeof(view));
    view.root = root;

    int count = -1;
    uint32_t *list = yabai_view_find_window_list(&view, &count);
    int ok = 1;
    if (!list || count != expected) {
        printf("window list count got %d want %d\n", count, expected);
        ok = 0;
    }
    if (list && malloc_size(list) < (size_t) expected * sizeof(uint32_t)) {
        printf("window list block %zu bytes, need %zu\n", malloc_size(list), (size_t) expected * sizeof(uint32_t));
        ok = 0;
    }
    for (int i = 0; ok && i < expected; ++i) {
        if (list[i] != base + (uint32_t) i) {
            printf("window list [%d] got %u want %u\n", i, list[i], base + (uint32_t) i);
            ok = 0;
        }
    }
    free(list);
    return ok;
}

TEST_FUNC(zig_regress_window_list_single_leaf_31,
{
    /* Stack-layout shape: one root leaf of 31 windows. Initial capacity
     * 13 doubled once to 26 cannot hold 31. */
    struct window_node *root = regress_leaf(1000, 31);
    int ok = regress_window_list_case(root, 1000, 31);
    TEST_CHECK(ok, 1);
    free(root);
})

TEST_FUNC(zig_regress_window_list_two_leaves_12_15,
{
    /* bsp shape: leaves of 12 then 15 windows (27 total, past 26). */
    struct window_node *root = calloc(1, sizeof(struct window_node));
    struct window_node *left = regress_leaf(2000, 12);
    struct window_node *right = regress_leaf(2012, 15);
    left->parent = root;
    right->parent = root;
    root->left = left;
    root->right = right;
    int ok = regress_window_list_case(root, 2000, 27);
    TEST_CHECK(ok, 1);
    free(left);
    free(right);
    free(root);
})

TEST_FUNC(zig_regress_rule_negative_grid,
{
    /* sscanf("%d") stores -1 into the unsigned grid slot; serialize must
     * reinterpret it, not range-cast it. */
    char msg[] = "grid=-1:1:0:0:1:1\0app=x\0";
    char *cursor = msg;
    struct token first = get_token(&cursor);
    struct rule rule;
    memset(&rule, 0, sizeof(rule));
    bool parsed = yabai_parse_rule(NULL, &cursor, &rule, &first, 0, 0);
    TEST_CHECK(parsed, true);

    char *out = NULL;
    size_t out_len = 0;
    FILE *rsp = open_memstream(&out, &out_len);
    yabai_rule_serialize(rsp, &rule, 0, 0, 0);
    fclose(rsp);
    bool has_grid = out && strstr(out, "\"grid\":\"-1:1:0:0:1:1\"") != NULL;
    if (!has_grid) printf("serialized rule: %s\n", out ? out : "(null)");
    TEST_CHECK(has_grid, true);
    free(out);
    yabai_rule_destroy(&rule);
})
