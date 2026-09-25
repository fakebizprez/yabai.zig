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
