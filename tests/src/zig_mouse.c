/* mouse geometry parity tests.
 *
 * The Zig exports in zig/mouse.zig must match triangle_contains_point() in
 * src/misc/helpers.h, mouse_mod_from_cgflags() and mouse_determine_drop_action()
 * in src/mouse_handler.c exactly. The oracles are the unrouted C helpers
 * and the frozen dd84572 drop-action body (ref_mouse_determine_drop_action
 * in tests/src/ref_upstream_view.c); no SLS access needed.
 *
 * Seam: structs arrive lifted in. The triangle export takes a pointer to
 * three MousePoints (layout-identical to CGPoint: { f64, f64 } on 64-bit
 * macOS); the drop-action export takes (drop_mode, src_count, frame x/y/w/h,
 * point x/y) as scalars, replacing struct mouse_state / struct window_node /
 * struct window / CGPoint. The center-rect test mirrors CGRectContainsPoint
 * (min edges inclusive, max edges exclusive, empty rect contains nothing).
 *
 * Must FAIL against the stub (TDD red) and pass on the real port (green).
 */

struct mod_case
{
    uint32_t cgflags;
    uint8_t expected;
};

static struct mod_case mod_cases[] =
{
    { 0, 0 },
    { kCGEventFlagMaskAlternate, MOUSE_MOD_ALT },
    { kCGEventFlagMaskShift, MOUSE_MOD_SHIFT },
    { kCGEventFlagMaskCommand, MOUSE_MOD_CMD },
    { kCGEventFlagMaskControl, MOUSE_MOD_CTRL },
    { kCGEventFlagMaskSecondaryFn, MOUSE_MOD_FN },
    { kCGEventFlagMaskAlternate | kCGEventFlagMaskShift, MOUSE_MOD_ALT | MOUSE_MOD_SHIFT },
    { kCGEventFlagMaskCommand | kCGEventFlagMaskControl | kCGEventFlagMaskSecondaryFn, MOUSE_MOD_CMD | MOUSE_MOD_CTRL | MOUSE_MOD_FN },
    { kCGEventFlagMaskAlternate | kCGEventFlagMaskShift | kCGEventFlagMaskCommand | kCGEventFlagMaskControl | kCGEventFlagMaskSecondaryFn,
      MOUSE_MOD_ALT | MOUSE_MOD_SHIFT | MOUSE_MOD_CMD | MOUSE_MOD_CTRL | MOUSE_MOD_FN },
    /* Unrelated flags (alpha-shift, numeric-pad) map to nothing. */
    { 0x00010000, 0 },
    { 0x00010000 | kCGEventFlagMaskAlternate, MOUSE_MOD_ALT },
};

TEST_FUNC(zig_mouse_mod_parity,
{
    int count = array_count(mod_cases);
    for (int i = 0; i < count; ++i) {
        uint8_t c = mouse_mod_from_cgflags(mod_cases[i].cgflags);
        uint8_t z = yabai_mouse_mod_from_cgflags(mod_cases[i].cgflags);
        TEST_CHECK(c, mod_cases[i].expected);
        TEST_CHECK(z, mod_cases[i].expected);
        TEST_CHECK(z, c);
    }
})

struct tri_case
{
    double px;
    double py;
    bool expected;
};

/* Right triangle (both windings for the sweep below). */
static CGPoint tri_fwd[3] = { { 0, 0 }, { 10, 0 }, { 0, 10 } };
static CGPoint tri_rev[3] = { { 0, 10 }, { 10, 0 }, { 0, 0 } };

static struct tri_case tri_cases[] =
{
    { 1, 1, true },
    { 2, 3, true },
    { 10, 10, false },
    { -1, -1, false },
    { 5, 5, false },   /* hypotenuse midpoint: edge excluded (strict) */
    { 5, 0, false },   /* leg midpoint: edge excluded */
    { 0, 0, false },   /* vertex excluded */
    { 10, 0, false },  /* vertex excluded */
};

TEST_FUNC(zig_mouse_triangle_parity,
{
    int count = array_count(tri_cases);
    for (int i = 0; i < count; ++i) {
        CGPoint p;
        p.x = tri_cases[i].px;
        p.y = tri_cases[i].py;
        bool c = triangle_contains_point(tri_fwd, p);
        bool z = yabai_triangle_contains_point(tri_fwd, &p);
        TEST_CHECK(c, tri_cases[i].expected);
        TEST_CHECK(z, tri_cases[i].expected);
        TEST_CHECK(z, c);
    }

    /* Sweep parity over in/out/edge-adjacent points, both windings. */
    for (double gx = -2.0; gx <= 12.0; gx += 0.5) {
        for (double gy = -2.0; gy <= 12.0; gy += 0.5) {
            CGPoint p;
            p.x = gx;
            p.y = gy;
            TEST_CHECK(yabai_triangle_contains_point(tri_fwd, &p), triangle_contains_point(tri_fwd, p));
            TEST_CHECK(yabai_triangle_contains_point(tri_rev, &p), triangle_contains_point(tri_rev, p));
        }
    }
})

/* Non-representable vertices with points on edge t0-t2: the products in
 * a*b - c*d are inexact, so the edge term's sign depends on whether clang
 * fuses it into fma(a, b, -(c*d)). It does on arm64 (even at -O0); the
 * x86_64 default CPU has no FMA. The oracle is compiled for the running
 * arch, so the Zig object must follow the same per-arch rule. */
static CGPoint tri_inexact[3] = { { 0.1, 0.2 }, { 0.9, 0.05 }, { 0.7, 0.9 } };

TEST_FUNC(zig_mouse_triangle_inexact_parity,
{
    int c_inside = 0;
    for (int k = 1; k < 1000; ++k) {
        double s = k / 1000.0;
        CGPoint p;
        p.x = tri_inexact[0].x + s * (tri_inexact[2].x - tri_inexact[0].x);
        p.y = tri_inexact[0].y + s * (tri_inexact[2].y - tri_inexact[0].y);
        bool c = triangle_contains_point(tri_inexact, p);
        bool z = yabai_triangle_contains_point(tri_inexact, &p);
        if (c != z) printf("triangle edge k=%d c=%d z=%d\n", k, c, z);
        TEST_CHECK(z, c);
        c_inside += c;
    }
#if defined(__aarch64__)
    /* Guard against a vacuous sweep: fused rounding puts some edge points inside. */
    TEST_CHECK(c_inside > 0, 1);
#endif
})

static int c_drop_action(int drop_mode, int src_count, CGRect frame, CGPoint pt)
{
    struct mouse_state ms;
    struct window_node src;
    struct window dst;
    memset(&ms, 0, sizeof(ms));
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));
    ms.drop_action = drop_mode;
    src.window_count = src_count;
    dst.frame = frame;
    return (int) ref_mouse_determine_drop_action(&ms, &src, &dst, pt);
}

static int z_drop_action(int drop_mode, int src_count, CGRect frame, CGPoint pt)
{
    return yabai_mouse_determine_drop_action(drop_mode, src_count, frame.origin.x, frame.origin.y,
                                             frame.size.width, frame.size.height, pt.x, pt.y);
}

struct drop_case
{
    int drop_mode;
    int src_count;
    double px;
    double py;
    int expected;
};

static CGRect drop_frame;
static CGRect drop_degenerate;

static struct drop_case drop_cases[] =
{
    /* Center (relative 400,300): stack-vs-swap mode branch. */
    { MOUSE_MODE_STACK, 1, 500, 400, MOUSE_DROP_ACTION_STACK },
    { MOUSE_MODE_SWAP, 1, 500, 400, MOUSE_DROP_ACTION_SWAP },
    { MOUSE_MODE_MOVE, 1, 500, 400, MOUSE_DROP_ACTION_SWAP },
    /* Center-rect min edge (relative 200,300): CGRectContainsPoint includes
     * min edges. Max edges are excluded: relative (600,300) falls through
     * to WARP_RIGHT, relative (400,450) to WARP_BOTTOM. */
    { MOUSE_MODE_STACK, 1, 300, 400, MOUSE_DROP_ACTION_STACK },
    { MOUSE_MODE_STACK, 1, 700, 400, MOUSE_DROP_ACTION_WARP_RIGHT },
    { MOUSE_MODE_STACK, 1, 500, 550, MOUSE_DROP_ACTION_WARP_BOTTOM },
    /* Center with stacked src falls through (centroid hits no triangle). */
    { MOUSE_MODE_STACK, 2, 500, 400, MOUSE_DROP_ACTION_NONE },
    /* One warp case per direction. */
    { MOUSE_MODE_STACK, 1, 500, 200, MOUSE_DROP_ACTION_WARP_TOP },
    { MOUSE_MODE_STACK, 1, 800, 400, MOUSE_DROP_ACTION_WARP_RIGHT },
    { MOUSE_MODE_STACK, 1, 500, 600, MOUSE_DROP_ACTION_WARP_BOTTOM },
    { MOUSE_MODE_STACK, 1, 200, 400, MOUSE_DROP_ACTION_WARP_LEFT },
    /* Far outside: NONE. */
    { MOUSE_MODE_STACK, 1, 10000, 10000, MOUSE_DROP_ACTION_NONE },
    { MOUSE_MODE_SWAP, 3, -500, -500, MOUSE_DROP_ACTION_NONE },
};

static int drop_modes[] = { MOUSE_MODE_STACK, MOUSE_MODE_SWAP };
static int drop_counts[] = { 1, 3 };

static void init_drop_frames(void)
{
    drop_frame.origin.x = 100;
    drop_frame.origin.y = 100;
    drop_frame.size.width = 800;
    drop_frame.size.height = 600;
    drop_degenerate.origin.x = 100;
    drop_degenerate.origin.y = 100;
    drop_degenerate.size.width = 0;
    drop_degenerate.size.height = 600;
}

TEST_FUNC(zig_mouse_drop_action_parity,
{
    init_drop_frames();
    int count = array_count(drop_cases);
    for (int i = 0; i < count; ++i) {
        CGPoint pt;
        pt.x = drop_cases[i].px;
        pt.y = drop_cases[i].py;
        int c = c_drop_action(drop_cases[i].drop_mode, drop_cases[i].src_count, drop_frame, pt);
        int z = z_drop_action(drop_cases[i].drop_mode, drop_cases[i].src_count, drop_frame, pt);
        TEST_CHECK(c, drop_cases[i].expected);
        TEST_CHECK(z, drop_cases[i].expected);
        TEST_CHECK(z, c);
    }

    /* Sweep parity: frame interior plus margin, both modes, single and
     * stacked src, plus a degenerate zero-size frame (empty center rect
     * contains nothing under CG semantics). */
    for (int mi = 0; mi < 2; ++mi) {
        for (int si = 0; si < 2; ++si) {
            for (double gx = 50.0; gx <= 950.0; gx += 25.0) {
                for (double gy = 50.0; gy <= 750.0; gy += 25.0) {
                    CGPoint pt;
                    pt.x = gx;
                    pt.y = gy;
                    TEST_CHECK(z_drop_action(drop_modes[mi], drop_counts[si], drop_frame, pt),
                               c_drop_action(drop_modes[mi], drop_counts[si], drop_frame, pt));
                }
            }
        }
    }

    CGPoint dpt;
    dpt.x = 100;
    dpt.y = 400;
    TEST_CHECK(z_drop_action(MOUSE_MODE_STACK, 1, drop_degenerate, dpt),
               c_drop_action(MOUSE_MODE_STACK, 1, drop_degenerate, dpt));

    /* Odd-sized frame, points on the top/left diagonal toward the center
     * (0.5 * 801 = 400.5): the triangle products are inexact here, so this
     * covers the arm64 contraction through the routed drop-action path. */
    CGRect odd_frame = CGRectMake(0, 0, 801, 601);
    int mismatches = 0;
    for (int k = 1; k < 10000; ++k) {
        double s = k / 10000.0;
        CGPoint pt;
        pt.x = s * 400.5;
        pt.y = s * 300.5;
        int c = c_drop_action(MOUSE_MODE_STACK, 2, odd_frame, pt);
        int z = z_drop_action(MOUSE_MODE_STACK, 2, odd_frame, pt);
        mismatches += c != z;
    }
    TEST_CHECK(mismatches, 0);
})
