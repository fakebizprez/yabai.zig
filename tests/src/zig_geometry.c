/* Zig/C boundary parity tests (doc/zig-refactor-plan.md step 2).
 *
 * The Zig export yabai_area_make_pair() in zig/geometry.zig must match
 * area_make_pair() in src/view.c exactly. The C function is the oracle:
 * every case below runs both and compares all output bytes. The rounding
 * test additionally pins hand-computed values so both sides cannot drift
 * together unnoticed.
 */

void yabai_area_make_pair(int split, int gap, float ratio, const struct area *parent, struct area *left, struct area *right);
bool yabai_area_is_in_direction(const struct area *r1, const CGPoint *r1_max, const struct area *r2, const CGPoint *r2_max, int direction);
int yabai_area_distance_in_direction(const struct area *r1, const CGPoint *r1_max, const struct area *r2, const CGPoint *r2_max, int direction);

struct area_pair_case
{
    int split;
    int gap;
    float ratio;
    struct area parent;
};

static const struct area_pair_case area_pair_cases[] =
{
    { SPLIT_Y, 10, 0.5f,  { 0, 0, 1920, 1080 } },
    { SPLIT_Y, 0,  0.5f,  { 0, 0, 1919, 1080 } },
    { SPLIT_Y, 8,  0.3f,  { 100, 50, 1366, 768 } },
    { SPLIT_Y, 8,  0.7f,  { 100, 50, 1366, 768 } },
    { SPLIT_X, 10, 0.5f,  { 0, 0, 1920, 1080 } },
    { SPLIT_X, 0,  0.5f,  { 0, 0, 1920, 1079 } },
    { SPLIT_X, 4,  0.1f,  { 0, 0, 800, 600 } },
    { SPLIT_Y, 20, 0.9f,  { -1728, 0, 1728, 1117 } },
    { SPLIT_Y, 0,  0.5f,  { 0.5f, 1.5f, 100, 100 } },
    { SPLIT_Y, 6,  0.33f, { 0, 0, 2560, 1440 } },
};

static int area_pair_check(const struct area_pair_case *c)
{
    struct area c_left;
    struct area c_right;
    struct area z_left;
    struct area z_right;

    area_make_pair(c->split, c->gap, c->ratio, &c->parent, &c_left, &c_right);
    yabai_area_make_pair(c->split, c->gap, c->ratio, &c->parent, &z_left, &z_right);

    if (memcmp(&c_left, &z_left, sizeof(struct area)) != 0) return 0;
    if (memcmp(&c_right, &z_right, sizeof(struct area)) != 0) return 0;
    return 1;
}

static inline void init_test_area(struct area *a, float x, float y, float w, float h)
{
    a->x = x;
    a->y = y;
    a->w = w;
    a->h = h;
}

TEST_FUNC(zig_area_make_pair_parity,
{
    int count = array_count(area_pair_cases);
    int i = 0;
    for (i = 0; i < count; ++i) {
        int ok = area_pair_check(&area_pair_cases[i]);
        if (ok != 1) printf("case %d failed\n", i);
        TEST_CHECK(ok, 1);
    }
})

static const int test_directions[] = { DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST };

static inline void init_test_direction_list(struct test_area *list)
{
    init_test_area(&list[0].area, 0, 0, 100, 100);
    list[0].area_max = area_max_point(list[0].area);

    init_test_area(&list[1].area, 100, 0, 100, 100);
    list[1].area_max = area_max_point(list[1].area);

    init_test_area(&list[2].area, 25, 25, 50, 50);
    list[2].area_max = area_max_point(list[2].area);

    init_test_area(&list[3].area, 0, 100, 100, 100);
    list[3].area_max = area_max_point(list[3].area);
}

static int area_direction_check(struct test_area *list, int count, int *fail_case)
{
    int dir_count = array_count(test_directions);
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j < count; ++j) {
            if (i == j) continue;
            for (int d = 0; d < dir_count; ++d) {
                int direction = test_directions[d];
                bool c_in = area_is_in_direction(&list[i].area, list[i].area_max, &list[j].area, list[j].area_max, direction);
                bool z_in = yabai_area_is_in_direction(&list[i].area, &list[i].area_max, &list[j].area, &list[j].area_max, direction);
                if (c_in != z_in) {
                    *fail_case = i * 10000 + j * 100 + d;
                    return 0;
                }
                int c_dist = area_distance_in_direction(&list[i].area, list[i].area_max, &list[j].area, list[j].area_max, direction);
                int z_dist = yabai_area_distance_in_direction(&list[i].area, &list[i].area_max, &list[j].area, &list[j].area_max, direction);
                if (c_dist != z_dist) {
                    *fail_case = i * 10000 + j * 100 + d;
                    return 0;
                }
            }
        }
    }
    return 1;
}

TEST_FUNC(zig_area_direction_parity,
{
    struct test_area display_list[3];
    init_test_display_list(display_list);
    int fail_case = -1;
    int ok = area_direction_check(display_list, array_count(display_list), &fail_case);
    if (ok != 1) printf("display case %d failed\n", fail_case);
    TEST_CHECK(ok, 1);

    struct test_area edge_list[4];
    init_test_direction_list(edge_list);
    fail_case = -1;
    ok = area_direction_check(edge_list, array_count(edge_list), &fail_case);
    if (ok != 1) printf("edge case %d failed\n", fail_case);
    TEST_CHECK(ok, 1);
})

TEST_FUNC(zig_area_make_pair_rounding,
{
    /* Hand-computed: w=101 gap=0 ratio=0.5 gives left_width=50.5, so the
     * C (int) truncation yields left w=50 and right x=(int)(51.0)=51. */
    struct area parent;
    struct area left;
    struct area right;
    struct area expected_left;
    struct area expected_right;
    init_test_area(&parent, 0, 0, 101, 100);
    init_test_area(&expected_left, 0, 0, 50, 100);
    init_test_area(&expected_right, 51, 0, 50, 100);

    yabai_area_make_pair(SPLIT_Y, 0, 0.5f, &parent, &left, &right);
    TEST_CHECK(memcmp(&left, &expected_left, sizeof(struct area)), 0);
    TEST_CHECK(memcmp(&right, &expected_right, sizeof(struct area)), 0);

    /* Same rounding on the vertical axis for SPLIT_X. */
    init_test_area(&parent, 0, 0, 100, 101);
    init_test_area(&expected_left, 0, 0, 100, 50);
    init_test_area(&expected_right, 0, 51, 100, 50);

    yabai_area_make_pair(SPLIT_X, 0, 0.5f, &parent, &left, &right);
    TEST_CHECK(memcmp(&left, &expected_left, sizeof(struct area)), 0);
    TEST_CHECK(memcmp(&right, &expected_right, sizeof(struct area)), 0);
})
