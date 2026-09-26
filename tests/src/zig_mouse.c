/* mouse geometry parity tests.
 *
 * The Zig exports in zig/mouse.zig must match triangle_contains_point() in
 * src/misc/helpers.h, mouse_mod_from_cgflags() and mouse_determine_drop_action()
 * in src/mouse_handler.c exactly. The C functions are the oracles (both are
 * linked into this TU via manifest.m; no SLS access needed).
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

bool yabai_triangle_contains_point(const CGPoint *t, const CGPoint *p);
uint8_t yabai_mouse_mod_from_cgflags(uint32_t cgflags);
int yabai_mouse_determine_drop_action(int drop_mode, int src_count, double fx, double fy, double fw, double fh, double px, double py);

/* Pin the mirrored MOUSE_MOD_* bits (src/mouse_handler.h). */
_Static_assert(MOUSE_MOD_NONE == 0x01, "MOUSE_MOD_NONE drift");
_Static_assert(MOUSE_MOD_ALT == 0x02, "MOUSE_MOD_ALT drift");
_Static_assert(MOUSE_MOD_SHIFT == 0x04, "MOUSE_MOD_SHIFT drift");
_Static_assert(MOUSE_MOD_CMD == 0x08, "MOUSE_MOD_CMD drift");
_Static_assert(MOUSE_MOD_CTRL == 0x10, "MOUSE_MOD_CTRL drift");
_Static_assert(MOUSE_MOD_FN == 0x20, "MOUSE_MOD_FN drift");

/* Pin the mirrored MOUSE_DROP_ACTION_* codes (src/mouse_handler.h). */
_Static_assert(MOUSE_DROP_ACTION_NONE == 0, "MOUSE_DROP_ACTION_NONE drift");
_Static_assert(MOUSE_DROP_ACTION_STACK == 1, "MOUSE_DROP_ACTION_STACK drift");
_Static_assert(MOUSE_DROP_ACTION_SWAP == 2, "MOUSE_DROP_ACTION_SWAP drift");
_Static_assert(MOUSE_DROP_ACTION_WARP_TOP == 3, "MOUSE_DROP_ACTION_WARP_TOP drift");
_Static_assert(MOUSE_DROP_ACTION_WARP_RIGHT == 4, "MOUSE_DROP_ACTION_WARP_RIGHT drift");
_Static_assert(MOUSE_DROP_ACTION_WARP_BOTTOM == 5, "MOUSE_DROP_ACTION_WARP_BOTTOM drift");
_Static_assert(MOUSE_DROP_ACTION_WARP_LEFT == 6, "MOUSE_DROP_ACTION_WARP_LEFT drift");

/* Pin the mirrored MOUSE_MODE_* codes (src/mouse_handler.h). */
_Static_assert(MOUSE_MODE_NONE == 0, "MOUSE_MODE_NONE drift");
_Static_assert(MOUSE_MODE_MOVE == 1, "MOUSE_MODE_MOVE drift");
_Static_assert(MOUSE_MODE_RESIZE == 2, "MOUSE_MODE_RESIZE drift");
_Static_assert(MOUSE_MODE_SWAP == 3, "MOUSE_MODE_SWAP drift");
_Static_assert(MOUSE_MODE_STACK == 4, "MOUSE_MODE_STACK drift");

/* Pin the CG flag constants the Zig mod map hardcodes (CGEventTypes.h via
 * NX_*MASK in IOLLEvent.h). A system-header change fails the build here. */
_Static_assert(kCGEventFlagMaskShift == 0x00020000, "kCGEventFlagMaskShift drift");
_Static_assert(kCGEventFlagMaskControl == 0x00040000, "kCGEventFlagMaskControl drift");
_Static_assert(kCGEventFlagMaskAlternate == 0x00080000, "kCGEventFlagMaskAlternate drift");
_Static_assert(kCGEventFlagMaskCommand == 0x00100000, "kCGEventFlagMaskCommand drift");
_Static_assert(kCGEventFlagMaskSecondaryFn == 0x00800000, "kCGEventFlagMaskSecondaryFn drift");

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
    return (int) mouse_determine_drop_action(&ms, &src, &dst, pt);
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
})
