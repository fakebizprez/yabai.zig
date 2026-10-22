/* Zig/C boundary parity tests (doc/zig-refactor-plan.md step 2).
 *
 * The Zig export yabai_area_make_pair() in zig/geometry.zig must match
 * the frozen dd84572 area_make_pair() (ref_area_make_pair in
 * tests/src/ref_upstream_view.c) exactly. That copy is the oracle:
 * every case below runs both and compares all output bytes. The rounding
 * test additionally pins hand-computed values so both sides cannot drift
 * together unnoticed.
 */

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

    ref_area_make_pair(c->split, c->gap, c->ratio, &c->parent, &c_left, &c_right);
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
    list[0].area_max = ref_area_max_point(list[0].area);

    init_test_area(&list[1].area, 100, 0, 100, 100);
    list[1].area_max = ref_area_max_point(list[1].area);

    init_test_area(&list[2].area, 25, 25, 50, 50);
    list[2].area_max = ref_area_max_point(list[2].area);

    init_test_area(&list[3].area, 0, 100, 100, 100);
    list[3].area_max = ref_area_max_point(list[3].area);
}

static int area_direction_check(struct test_area *list, int count, int *fail_case)
{
    int dir_count = array_count(test_directions);
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j < count; ++j) {
            if (i == j) continue;
            for (int d = 0; d < dir_count; ++d) {
                int direction = test_directions[d];
                bool c_in = ref_area_is_in_direction(&list[i].area, list[i].area_max, &list[j].area, list[j].area_max, direction);
                bool z_in = yabai_area_is_in_direction(&list[i].area, &list[i].area_max, &list[j].area, &list[j].area_max, direction);
                if (c_in != z_in) {
                    *fail_case = i * 10000 + j * 100 + d;
                    return 0;
                }
                int c_dist = ref_area_distance_in_direction(&list[i].area, list[i].area_max, &list[j].area, list[j].area_max, direction);
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

    /* Hand-computed with gap: SPLIT_Y over (0,0,200,100), ratio 0.5,
     * gap 10. left_width = (200-10)*0.5 = 95, so left w=95 and
     * right x=(int)(95.5)=95 + gap=105, right w=95. */
    init_test_area(&parent, 0, 0, 200, 100);
    init_test_area(&expected_left, 0, 0, 95, 100);
    init_test_area(&expected_right, 105, 0, 95, 100);

    yabai_area_make_pair(SPLIT_Y, 10, 0.5f, &parent, &left, &right);
    TEST_CHECK(memcmp(&left, &expected_left, sizeof(struct area)), 0);
    TEST_CHECK(memcmp(&right, &expected_right, sizeof(struct area)), 0);

    /* Same gap rounding on the vertical axis for SPLIT_X:
     * (0,0,100,200) gives left h=95, right y=105, right h=95. */
    init_test_area(&parent, 0, 0, 100, 200);
    init_test_area(&expected_left, 0, 0, 100, 95);
    init_test_area(&expected_right, 0, 105, 100, 95);

    yabai_area_make_pair(SPLIT_X, 10, 0.5f, &parent, &left, &right);
    TEST_CHECK(memcmp(&left, &expected_left, sizeof(struct area)), 0);
    TEST_CHECK(memcmp(&right, &expected_right, sizeof(struct area)), 0);
})

/* Non-finite and out-of-range geometry against the frozen dd84572
 * reference. Float-to-int on NaN or out-of-range values is UB in both C
 * and Zig; the cases avoid the separate signed-overflow UB of
 * `(int)(left_width + 0.5f) + gap` by pairing a saturating width only
 * with gap 0 or with a sign that cannot overflow. */

static const struct area_pair_case extreme_pair_cases[] =
{
    { SPLIT_Y, 10, 0.5f,       { 0, 0, NAN, 100 } },
    { SPLIT_X, 10, 0.5f,       { 0, 0, 100, NAN } },
    { SPLIT_Y, 10, NAN,        { 0, 0, 1920, 1080 } },
    { SPLIT_X, 10, NAN,        { 0, 0, 1920, 1080 } },
    { SPLIT_Y, 2000000000, 0.5f, { 0, 0, 1920, 1080 } },
    { SPLIT_X, INT_MAX, 0.3f,  { 0, 0, 1920, 1080 } },
    { SPLIT_Y, 0, 0.5f,        { 0, 0, 1e10f, 100 } },
    { SPLIT_X, 0, 0.5f,        { 0, 0, 100, 1e10f } },
    { SPLIT_Y, 10, 0.5f,       { 0, 0, -1e10f, 100 } },
    { SPLIT_Y, 0, 0.5f,        { 0, 0, INFINITY, 100 } },
    { SPLIT_X, 10, 0.5f,       { 0, 0, 100, -INFINITY } },
    { SPLIT_Y, 0, INFINITY,    { 0, 0, 1920, 1080 } },
    { SPLIT_Y, 10, 0.5f,       { 1e10f, NAN, 1920, 1080 } },
};

static const struct area extreme_areas[] =
{
    { 0, 0, 100, 100 },
    { NAN, 0, 100, 100 },
    { 0, NAN, 100, NAN },
    { 1e10f, 1e10f, 100, 100 },
    { -1e10f, -1e10f, 100, 100 },
    { 0, 0, INFINITY, INFINITY },
    { -INFINITY, -INFINITY, 100, 100 },
    { 0, 0, 1e10f, 1e10f },
};

#if defined(__arm64__)
/* Hand pins for the UB arms, derived from arm64 fcvtzs semantics (what
 * clang emits for the C (int) casts and what Zig's @intFromFloat lowers
 * to here): out-of-range saturates to INT_MIN/INT_MAX, NaN becomes 0.
 * (float)INT_MAX rounds to 2147483648.0f. These pins apply only to arm64;
 * x86_64 returns INT_MIN for NaN/out-of-range inputs and still runs the
 * frozen-C parity comparisons below. */
#define SAT_HI 2147483648.0f
#define SAT_LO -2147483648.0f

struct extreme_pin
{
    int index;
    struct area left;
    struct area right;
};

static const struct extreme_pin extreme_pins[] =
{
    /* NaN width/height/ratio: both widths 0, offset is just the gap. */
    { 0,  { 0, 0, 0, 100 },        { 10, 0, 0, 100 } },
    { 1,  { 0, 0, 100, 0 },        { 0, 10, 100, 0 } },
    { 2,  { 0, 0, 0, 1080 },       { 10, 0, 0, 1080 } },
    { 3,  { 0, 0, 1920, 0 },       { 0, 10, 1920, 0 } },
    /* Huge gap stays in range: (1920 - 2e9) * 0.5 = -999999040 exactly,
     * offset -999999040 + 2e9 = 1000000960. */
    { 4,  { 0, 0, -999999040.0f, 1080 }, { 1000000960.0f, 0, -999999040.0f, 1080 } },
    /* 1e10 / +inf halves saturate to INT_MAX. */
    { 6,  { 0, 0, SAT_HI, 100 },   { SAT_HI, 0, SAT_HI, 100 } },
    { 7,  { 0, 0, 100, SAT_HI },   { 0, SAT_HI, 100, SAT_HI } },
    { 9,  { 0, 0, SAT_HI, 100 },   { SAT_HI, 0, SAT_HI, 100 } },
    /* -1e10 / -inf saturate to INT_MIN; INT_MIN + 10 rounds back to
     * -2147483648.0f. */
    { 8,  { 0, 0, SAT_LO, 100 },   { SAT_LO, 0, SAT_LO, 100 } },
    { 10, { 0, 0, 100, SAT_LO },   { 0, SAT_LO, 100, SAT_LO } },
    /* ratio inf: left saturates high, right (1 - inf) saturates low. */
    { 11, { 0, 0, SAT_HI, 1080 },  { SAT_HI, 0, SAT_LO, 1080 } },
};
#endif

static void print_area(const char *tag, const struct area *a)
{
    printf("  %s {%g %g %g %g}\n", tag, a->x, a->y, a->w, a->h);
}

TEST_FUNC(zig_area_extreme_values,
{
    int count = array_count(extreme_pair_cases);
    for (int i = 0; i < count; ++i) {
        const struct area_pair_case *c = &extreme_pair_cases[i];
        struct area parent = c->parent;
        struct area r_left;
        struct area r_right;
        struct area z_left;
        struct area z_right;
        ref_area_make_pair(c->split, c->gap, c->ratio, &parent, &r_left, &r_right);
        yabai_area_make_pair(c->split, c->gap, c->ratio, &parent, &z_left, &z_right);
        int ok = memcmp(&r_left, &z_left, sizeof(struct area)) == 0 && memcmp(&r_right, &z_right, sizeof(struct area)) == 0;
        if (!ok) {
            printf("extreme make_pair case %d differs\n", i);
            print_area("ref left ", &r_left);
            print_area("zig left ", &z_left);
            print_area("ref right", &r_right);
            print_area("zig right", &z_right);
        }
        TEST_CHECK(ok, 1);
    }

#if defined(__arm64__)
    int pin_count = array_count(extreme_pins);
    for (int i = 0; i < pin_count; ++i) {
        const struct extreme_pin *p = &extreme_pins[i];
        const struct area_pair_case *c = &extreme_pair_cases[p->index];
        struct area parent = c->parent;
        struct area r_left;
        struct area r_right;
        struct area z_left;
        struct area z_right;
        ref_area_make_pair(c->split, c->gap, c->ratio, &parent, &r_left, &r_right);
        yabai_area_make_pair(c->split, c->gap, c->ratio, &parent, &z_left, &z_right);
        int ok = memcmp(&r_left, &p->left, sizeof(struct area)) == 0 && memcmp(&r_right, &p->right, sizeof(struct area)) == 0 &&
                 memcmp(&z_left, &p->left, sizeof(struct area)) == 0 && memcmp(&z_right, &p->right, sizeof(struct area)) == 0;
        if (!ok) {
            printf("extreme make_pair pin %d (case %d) differs\n", i, p->index);
            print_area("ref left ", &r_left);
            print_area("zig left ", &z_left);
            print_area("pin left ", &p->left);
            print_area("ref right", &r_right);
            print_area("zig right", &z_right);
            print_area("pin right", &p->right);
        }
        TEST_CHECK(ok, 1);
    }
#endif

    int area_count = array_count(extreme_areas);
    for (int i = 0; i < area_count; ++i) {
        CGPoint r_max = ref_area_max_point(extreme_areas[i]);
        CGPoint z_max;
        yabai_area_max_point(&extreme_areas[i], &z_max);
        TEST_CHECK(memcmp(&r_max, &z_max, sizeof(CGPoint)), 0);
    }

    int dir_count = array_count(test_directions);
    for (int i = 0; i < area_count; ++i) {
        for (int j = 0; j < area_count; ++j) {
            struct area r1 = extreme_areas[i];
            struct area r2 = extreme_areas[j];
            CGPoint r1_max = ref_area_max_point(r1);
            CGPoint r2_max = ref_area_max_point(r2);
            for (int d = 0; d < dir_count; ++d) {
                int direction = test_directions[d];
                bool r_in = ref_area_is_in_direction(&r1, r1_max, &r2, r2_max, direction);
                bool z_in = yabai_area_is_in_direction(&r1, &r1_max, &r2, &r2_max, direction);
                int r_dist = ref_area_distance_in_direction(&r1, r1_max, &r2, r2_max, direction);
                int z_dist = yabai_area_distance_in_direction(&r1, &r1_max, &r2, &r2_max, direction);
                if (r_in != z_in || r_dist != z_dist) {
                    printf("extreme direction %d->%d dir %d: ref in %d dist %d, zig in %d dist %d\n",
                           i, j, direction, r_in, r_dist, z_in, z_dist);
                }
                TEST_CHECK(r_in, z_in);
                TEST_CHECK(r_dist, z_dist);
            }
        }
    }

#if defined(__arm64__)
    /* Distance pins (extreme_areas indices): 1e10 - 99 saturates to
     * INT_MAX, a NaN difference becomes 0, -inf - -inf is NaN so 0. */
    struct area a0 = extreme_areas[0];
    struct area a1 = extreme_areas[1];
    struct area a3 = extreme_areas[3];
    struct area a4 = extreme_areas[4];
    struct area a6 = extreme_areas[6];
    CGPoint m0;
    CGPoint m1;
    CGPoint m3;
    CGPoint m4;
    CGPoint m6;
    yabai_area_max_point(&a0, &m0);
    yabai_area_max_point(&a1, &m1);
    yabai_area_max_point(&a3, &m3);
    yabai_area_max_point(&a4, &m4);
    yabai_area_max_point(&a6, &m6);
    TEST_CHECK(yabai_area_distance_in_direction(&a0, &m0, &a3, &m3, DIR_EAST), INT_MAX);
    TEST_CHECK(ref_area_distance_in_direction(&a0, m0, &a3, m3, DIR_EAST), INT_MAX);
    TEST_CHECK(yabai_area_distance_in_direction(&a4, &m4, &a0, &m0, DIR_NORTH), INT_MAX);
    TEST_CHECK(ref_area_distance_in_direction(&a4, m4, &a0, m0, DIR_NORTH), INT_MAX);
    TEST_CHECK(yabai_area_distance_in_direction(&a0, &m0, &a1, &m1, DIR_EAST), 0);
    TEST_CHECK(ref_area_distance_in_direction(&a0, m0, &a1, m1, DIR_EAST), 0);
    TEST_CHECK(yabai_area_distance_in_direction(&a6, &m6, &a6, &m6, DIR_EAST), 0);
    TEST_CHECK(ref_area_distance_in_direction(&a6, m6, &a6, m6, DIR_EAST), 0);
#endif
})
