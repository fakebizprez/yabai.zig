/* Pure-helper parity tests (doc/zig-refactor-plan.md step 3).
 *
 * The Zig exports in zig/util.zig must match the helpers in
 * src/misc/helpers.h, src/misc/macros.h and src/view.c exactly.
 * in_range_* are C macros used with both float and int operands, so
 * both variants are ported for the actually used combinations.
 */

struct yabai_balance_node
{
    int y_count;
    int x_count;
};

struct yabai_rgba_color
{
    uint32_t p;
    float r;
    float g;
    float b;
    float a;
};

void yabai_balance_node_add(const struct yabai_balance_node *a, const struct yabai_balance_node *b, struct yabai_balance_node *out);
float yabai_clampf_range(float value, float min, float max);
const char *yabai_json_bool(bool value);
const char *yabai_json_optional_bool(int value);
bool yabai_in_range_ii_f(float a, float b, float c);
bool yabai_in_range_ii_i(int a, int b, int c);
bool yabai_in_range_ei_f(float a, float b, float c);
bool yabai_in_range_ie_i(int a, int b, int c);
void yabai_rgba_color_from_hex(uint32_t color, struct yabai_rgba_color *out);

_Static_assert(sizeof(struct balance_node) == 8, "C balance_node size drift");
_Static_assert(sizeof(struct yabai_balance_node) == 8, "balance_node size drift");
_Static_assert(sizeof(struct rgba_color) == 20, "C rgba_color size drift");
_Static_assert(sizeof(struct yabai_rgba_color) == 20, "rgba_color size drift");

TEST_FUNC(zig_util_balance_clamp,
{
    struct balance_node c_add = balance_node_add((struct balance_node) { 1, 2 }, (struct balance_node) { 3, 4 });
    struct yabai_balance_node za;
    struct yabai_balance_node zb;
    struct yabai_balance_node zout;
    za.y_count = 1;
    za.x_count = 2;
    zb.y_count = 3;
    zb.x_count = 4;
    yabai_balance_node_add(&za, &zb, &zout);
    TEST_CHECK(c_add.y_count != zout.y_count, 0);
    TEST_CHECK(c_add.x_count != zout.x_count, 0);
    TEST_CHECK(zout.y_count, 4);
    TEST_CHECK(zout.x_count, 6);

    TEST_CHECK(clampf_range(5.0f, 0.0f, 10.0f) != yabai_clampf_range(5.0f, 0.0f, 10.0f), 0);
    TEST_CHECK(yabai_clampf_range(5.0f, 0.0f, 10.0f), 5.0f);
    TEST_CHECK(clampf_range(-1.0f, 0.0f, 10.0f) != yabai_clampf_range(-1.0f, 0.0f, 10.0f), 0);
    TEST_CHECK(yabai_clampf_range(11.0f, 0.0f, 10.0f), 10.0f);

    float nan_v = strtof("nan", NULL);
    float c_nan = clampf_range(nan_v, 0.0f, 10.0f);
    float z_nan = yabai_clampf_range(nan_v, 0.0f, 10.0f);
    TEST_CHECK(memcmp(&c_nan, &z_nan, sizeof(float)), 0);
})

TEST_FUNC(zig_util_json_bool,
{
    TEST_CHECK(strcmp(json_bool(true), yabai_json_bool(true)), 0);
    TEST_CHECK(strcmp(json_bool(false), yabai_json_bool(false)), 0);
    TEST_CHECK(strcmp(yabai_json_bool(true), "true"), 0);
    TEST_CHECK(strcmp(json_optional_bool(0), yabai_json_optional_bool(0)), 0);
    TEST_CHECK(strcmp(json_optional_bool(1), yabai_json_optional_bool(1)), 0);
    TEST_CHECK(strcmp(json_optional_bool(2), yabai_json_optional_bool(2)), 0);
    TEST_CHECK(strcmp(json_optional_bool(-1), yabai_json_optional_bool(-1)), 0);
    TEST_CHECK(strcmp(yabai_json_optional_bool(0), "null"), 0);
    TEST_CHECK(strcmp(yabai_json_optional_bool(1), "true"), 0);
    TEST_CHECK(strcmp(yabai_json_optional_bool(7), "false"), 0);
})

struct range_f_case
{
    float a;
    float b;
    float c;
    bool ii;
    bool ei;
};

static struct range_f_case range_f_cases[] =
{
    { 0.5f, 0.0f, 1.0f, true, true },
    { 0.0f, 0.0f, 1.0f, true, false },
    { 1.0f, 0.0f, 1.0f, true, true },
    { -0.5f, 0.0f, 1.0f, false, false },
    { 1.5f, 0.0f, 1.0f, false, false },
    { 0.05f, 0.1f, 0.9f, false, false },
};

struct range_i_case
{
    int a;
    int b;
    int c;
    bool ii;
    bool ie;
};

static struct range_i_case range_i_cases[] =
{
    { 1, 0, 3, true, true },
    { 0, 0, 3, true, true },
    { 3, 0, 3, true, false },
    { -1, 0, 3, false, false },
    { 4, 0, 3, false, false },
};

TEST_FUNC(zig_util_in_range,
{
    int f_count = array_count(range_f_cases);
    int i = 0;
    for (i = 0; i < f_count; ++i) {
        float a = range_f_cases[i].a;
        float b = range_f_cases[i].b;
        float c = range_f_cases[i].c;
        bool c_ii = in_range_ii(a, b, c);
        bool z_ii = yabai_in_range_ii_f(a, b, c);
        TEST_CHECK(c_ii != z_ii, 0);
        TEST_CHECK(c_ii != range_f_cases[i].ii, 0);
        bool c_ei = in_range_ei(a, b, c);
        bool z_ei = yabai_in_range_ei_f(a, b, c);
        TEST_CHECK(c_ei != z_ei, 0);
        TEST_CHECK(c_ei != range_f_cases[i].ei, 0);
    }

    int i_count = array_count(range_i_cases);
    for (i = 0; i < i_count; ++i) {
        int a = range_i_cases[i].a;
        int b = range_i_cases[i].b;
        int c = range_i_cases[i].c;
        bool c_ii = in_range_ii(a, b, c);
        bool z_ii = yabai_in_range_ii_i(a, b, c);
        TEST_CHECK(c_ii != z_ii, 0);
        TEST_CHECK(c_ii != range_i_cases[i].ii, 0);
        bool c_ie = in_range_ie(a, b, c);
        bool z_ie = yabai_in_range_ie_i(a, b, c);
        TEST_CHECK(c_ie != z_ie, 0);
        TEST_CHECK(c_ie != range_i_cases[i].ie, 0);
    }
})

TEST_FUNC(zig_util_rgba,
{
    struct rgba_color c_red = rgba_color_from_hex(0xFF0000);
    struct yabai_rgba_color z_red;
    yabai_rgba_color_from_hex(0xFF0000, &z_red);
    TEST_CHECK(memcmp(&c_red, &z_red, sizeof(struct rgba_color)), 0);
    TEST_CHECK(z_red.p, 0xFF0000);
    TEST_CHECK(z_red.r == 1.0f, 1);
    TEST_CHECK(z_red.g == 0.0f, 1);
    TEST_CHECK(z_red.b == 0.0f, 1);
    /* Alpha byte of 0xFF0000 is 0x00. */
    TEST_CHECK(z_red.a == 0.0f, 1);

    struct rgba_color c_mix = rgba_color_from_hex(0x80402010);
    struct yabai_rgba_color z_mix;
    yabai_rgba_color_from_hex(0x80402010, &z_mix);
    TEST_CHECK(memcmp(&c_mix, &z_mix, sizeof(struct rgba_color)), 0);
})
