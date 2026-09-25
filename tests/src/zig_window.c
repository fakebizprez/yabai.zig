/* Resize-math parity tests (doc/zig-refactor-plan.md step 3).
 *
 * The Zig export in zig/window.zig must match resize_frame_math() in
 * src/window_manager.c exactly. The C helper was extracted verbatim
 * from window_manager_resize_window_relative_internal() (pure frame
 * math; the AX/animate effects stay in the caller), so the helper is
 * both the oracle and the seam the Zig port targets. CGRect fields
 * are doubles; the narrowing to float outputs is part of the pinned
 * behavior.
 */

void yabai_resize_frame_math(const CGRect *frame, int direction, float dx, float dy, float *fx, float *fy, float *fw, float *fh);
void yabai_move_frame_math(const CGRect *frame, int type, float dx, float dy, float *x, float *y, float *w, float *h);
bool yabai_window_is_undersized(double w, double h);
bool yabai_window_can_move(uint8_t flags);
bool yabai_window_can_resize(uint8_t flags);

_Static_assert(WINDOW_MOVABLE == 0x40, "WINDOW_MOVABLE drift");
_Static_assert(WINDOW_RESIZABLE == 0x80, "WINDOW_RESIZABLE drift");

_Static_assert(sizeof(CGRect) == 32, "CGRect size drift");

struct resize_case
{
    double x;
    double y;
    double w;
    double h;
    int dir;
    float dx;
    float dy;
    float ex;
    float ey;
    float ew;
    float eh;
};

/* dir codes: HANDLE_RIGHT 0x08, LEFT 0x04, TOP 0x01, BOTTOM 0x02. */
static struct resize_case resize_cases[] =
{
    { 100, 100, 800, 600, 0x08, 50, 0, 100, 100, 850, 600 },
    { 100, 100, 800, 600, 0x04, 50, 0, 150, 100, 750, 600 },
    { 100, 100, 100, 600, 0x04, 500, 0, 199, 100, 1, 600 },
    { 100, 100, 800, 600, 0x01, 0, 30, 100, 130, 800, 570 },
    { 100, 100, 800, 600, 0x02, 0, -700, 100, 100, 800, 1 },
    { 100, 100, 800, 600, 0x05, 10, 20, 110, 120, 790, 580 },
    { 100, 100, 800, 600, 0x08, 0, 0, 100, 100, 800, 600 },
};

TEST_FUNC(zig_window_resize_math,
{
    int count = array_count(resize_cases);
    int i = 0;
    for (i = 0; i < count; ++i) {
        CGRect frame;
        frame.origin.x = resize_cases[i].x;
        frame.origin.y = resize_cases[i].y;
        frame.size.width = resize_cases[i].w;
        frame.size.height = resize_cases[i].h;

        float c_fx;
        float c_fy;
        float c_fw;
        float c_fh;
        float z_fx;
        float z_fy;
        float z_fw;
        float z_fh;
        resize_frame_math(frame, resize_cases[i].dir, resize_cases[i].dx, resize_cases[i].dy, &c_fx, &c_fy, &c_fw, &c_fh);
        yabai_resize_frame_math(&frame, resize_cases[i].dir, resize_cases[i].dx, resize_cases[i].dy, &z_fx, &z_fy, &z_fw, &z_fh);

        TEST_CHECK(memcmp(&c_fx, &z_fx, sizeof(float)), 0);
        TEST_CHECK(memcmp(&c_fy, &z_fy, sizeof(float)), 0);
        TEST_CHECK(memcmp(&c_fw, &z_fw, sizeof(float)), 0);
        TEST_CHECK(memcmp(&c_fh, &z_fh, sizeof(float)), 0);

        TEST_CHECK(c_fx == resize_cases[i].ex, 1);
        TEST_CHECK(c_fy == resize_cases[i].ey, 1);
        TEST_CHECK(c_fw == resize_cases[i].ew, 1);
        TEST_CHECK(c_fh == resize_cases[i].eh, 1);
    }

    float nan_v = strtof("nan", NULL);
    CGRect frame;
    frame.origin.x = 100;
    frame.origin.y = 100;
    frame.size.width = 800;
    frame.size.height = 600;
    float c_fw;
    float z_fw;
    float c_dummy;
    float z_dummy;
    resize_frame_math(frame, 0x08, nan_v, 0, &c_dummy, &c_dummy, &c_fw, &c_dummy);
    yabai_resize_frame_math(&frame, 0x08, nan_v, 0, &z_dummy, &z_dummy, &z_fw, &z_dummy);
    TEST_CHECK(memcmp(&c_fw, &z_fw, sizeof(float)), 0);
})

struct move_case
{
    double x;
    double y;
    double w;
    double h;
    int type;
    float dx;
    float dy;
    float ex;
    float ey;
    float ew;
    float eh;
};

/* type codes: TYPE_ABS 0x1, TYPE_REL 0x2. */
static struct move_case move_cases[] =
{
    { 100, 200, 800, 600, 0x2, 10, -20, 110, 180, 800, 600 },
    { 100, 200, 800, 600, 0x1, 10, -20, 10, -20, 800, 600 },
    { 100, 200, 800, 600, 0x0, 10, -20, 10, -20, 800, 600 },
    { 0.5, 1.5, 100, 100, 0x2, 0, 0, 0.5f, 1.5f, 100, 100 },
};

TEST_FUNC(zig_window_move_math,
{
    int count = array_count(move_cases);
    int i = 0;
    for (i = 0; i < count; ++i) {
        CGRect frame;
        frame.origin.x = move_cases[i].x;
        frame.origin.y = move_cases[i].y;
        frame.size.width = move_cases[i].w;
        frame.size.height = move_cases[i].h;

        float c_x;
        float c_y;
        float c_w;
        float c_h;
        float z_x;
        float z_y;
        float z_w;
        float z_h;
        move_frame_math(frame, move_cases[i].type, move_cases[i].dx, move_cases[i].dy, &c_x, &c_y, &c_w, &c_h);
        yabai_move_frame_math(&frame, move_cases[i].type, move_cases[i].dx, move_cases[i].dy, &z_x, &z_y, &z_w, &z_h);

        TEST_CHECK(memcmp(&c_x, &z_x, sizeof(float)), 0);
        TEST_CHECK(memcmp(&c_y, &z_y, sizeof(float)), 0);
        TEST_CHECK(memcmp(&c_w, &z_w, sizeof(float)), 0);
        TEST_CHECK(memcmp(&c_h, &z_h, sizeof(float)), 0);

        TEST_CHECK(c_x == move_cases[i].ex, 1);
        TEST_CHECK(c_y == move_cases[i].ey, 1);
        TEST_CHECK(c_w == move_cases[i].ew, 1);
        TEST_CHECK(c_h == move_cases[i].eh, 1);
    }
})

/* window_is_undersized() in src/window.c: pure frame-size check.
 * The Zig export takes the two doubles directly (CGRect uses doubles;
 * the 500.0f bound promotes to exactly 500.0). The C function on a
 * real struct window is the oracle; hand-computed pins guard the seam. */

struct undersized_case
{
    double w;
    double h;
    bool expected;
};

static struct undersized_case undersized_cases[] =
{
    { 800, 600, false },
    { 100, 100, true },
    { 500, 600, true },
    { 500, 500, true },
    { 501, 501, false },
    { 501, 600, false },
    { 800, 500, true },
    { 0, 0, true },
    { -10, 600, true },
    { 1e9, 1e9, false },
};

TEST_FUNC(zig_window_undersized_parity,
{
    int count = array_count(undersized_cases);
    int i = 0;
    for (i = 0; i < count; ++i) {
        struct window window;
        memset(&window, 0, sizeof(window));
        window.frame.size.width = undersized_cases[i].w;
        window.frame.size.height = undersized_cases[i].h;

        bool c = window_is_undersized(&window);
        bool z = yabai_window_is_undersized(undersized_cases[i].w, undersized_cases[i].h);

        TEST_CHECK(c != z, 0);
        TEST_CHECK(c != undersized_cases[i].expected, 0);
    }

    /* NaN width: NaN <= 500.0f is false, so with a large height the
     * window is not undersized in either implementation. */
    float nan_v = strtof("nan", NULL);
    struct window nan_window;
    memset(&nan_window, 0, sizeof(nan_window));
    nan_window.frame.size.width = nan_v;
    nan_window.frame.size.height = 600;
    bool c_nan = window_is_undersized(&nan_window);
    bool z_nan = yabai_window_is_undersized(nan_v, 600);
    TEST_CHECK(c_nan != z_nan, 0);
    TEST_CHECK(c_nan, false);
})

/* window_can_move() / window_can_resize() in src/window.c: pure flag
 * reads (flags & WINDOW_MOVABLE / WINDOW_RESIZABLE). Exhaustive over
 * the full uint8 flag byte against the C oracle on a real struct. */

TEST_FUNC(zig_window_flag_parity,
{
    unsigned f = 0;
    for (f = 0; f < 0x100; ++f) {
        struct window window;
        memset(&window, 0, sizeof(window));
        window.flags = (uint8_t) f;

        bool c_move = window_can_move(&window);
        bool z_move = yabai_window_can_move((uint8_t) f);
        TEST_CHECK(c_move != z_move, 0);

        bool c_resize = window_can_resize(&window);
        bool z_resize = yabai_window_can_resize((uint8_t) f);
        TEST_CHECK(c_resize != z_resize, 0);
    }

    TEST_CHECK(yabai_window_can_move(0x00), false);
    TEST_CHECK(yabai_window_can_resize(0x00), false);
    TEST_CHECK(yabai_window_can_move(0x40), true);
    TEST_CHECK(yabai_window_can_resize(0x40), false);
    TEST_CHECK(yabai_window_can_move(0x80), false);
    TEST_CHECK(yabai_window_can_resize(0x80), true);
    TEST_CHECK(yabai_window_can_move(0xC0), true);
    TEST_CHECK(yabai_window_can_resize(0xC0), true);
    TEST_CHECK(yabai_window_can_move(0xFF), true);
    TEST_CHECK(yabai_window_can_resize(0xFF), true);
})
