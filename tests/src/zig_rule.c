/* Rule-effects parity tests (doc/zig-refactor-plan.md step 3).
 *
 * The Zig export in zig/rule.zig must match rule_combine_effects() in
 * src/rule.c exactly, including scratchpad ownership (old copy freed,
 * new copy allocated) and the untouched-field paths.
 */

void yabai_rule_combine_effects(struct rule_effects *effects, struct rule_effects *result);
bool yabai_rule_check_flag(const struct rule *r, uint16_t flag);
void yabai_rule_set_flag(struct rule *r, uint16_t flag);
void yabai_rule_clear_flag(struct rule *r, uint16_t flag);
void yabai_rule_destroy(struct rule *rule);

_Static_assert(sizeof(struct rule_effects) == 80, "rule_effects size drift");
_Static_assert(offsetof(struct rule_effects, sid) == 8, "rule_effects.sid drift");
_Static_assert(offsetof(struct rule_effects, grid) == 40, "rule_effects.grid drift");
_Static_assert(offsetof(struct rule_effects, scratchpad) == 64, "rule_effects.scratchpad drift");
_Static_assert(offsetof(struct rule_effects, flags) == 72, "rule_effects.flags drift");

/* struct rule layout pins (src/rule.h). regex_t is 32 bytes on 64-bit
 * macOS; the Zig Rule mirror treats the four regex fields as opaque
 * byte arrays and never calls regcomp/regfree. */
_Static_assert(sizeof(regex_t) == 32, "regex_t size drift");
_Static_assert(sizeof(struct rule) == 256, "rule size drift");
_Static_assert(offsetof(struct rule, label) == 0, "rule.label drift");
_Static_assert(offsetof(struct rule, app) == 8, "rule.app drift");
_Static_assert(offsetof(struct rule, title) == 16, "rule.title drift");
_Static_assert(offsetof(struct rule, role) == 24, "rule.role drift");
_Static_assert(offsetof(struct rule, subrole) == 32, "rule.subrole drift");
_Static_assert(offsetof(struct rule, app_regex) == 40, "rule.app_regex drift");
_Static_assert(offsetof(struct rule, title_regex) == 72, "rule.title_regex drift");
_Static_assert(offsetof(struct rule, role_regex) == 104, "rule.role_regex drift");
_Static_assert(offsetof(struct rule, subrole_regex) == 136, "rule.subrole_regex drift");
_Static_assert(offsetof(struct rule, effects) == 168, "rule.effects drift");
_Static_assert(offsetof(struct rule, flags) == 248, "rule.flags drift");

_Static_assert(RULE_APP_VALID == 0x001, "RULE_APP_VALID drift");
_Static_assert(RULE_TITLE_VALID == 0x002, "RULE_TITLE_VALID drift");
_Static_assert(RULE_ROLE_VALID == 0x004, "RULE_ROLE_VALID drift");
_Static_assert(RULE_SUBROLE_VALID == 0x008, "RULE_SUBROLE_VALID drift");
_Static_assert(RULE_APP_EXCLUDE == 0x010, "RULE_APP_EXCLUDE drift");
_Static_assert(RULE_TITLE_EXCLUDE == 0x020, "RULE_TITLE_EXCLUDE drift");
_Static_assert(RULE_ROLE_EXCLUDE == 0x040, "RULE_ROLE_EXCLUDE drift");
_Static_assert(RULE_SUBROLE_EXCLUDE == 0x080, "RULE_SUBROLE_EXCLUDE drift");
_Static_assert(RULE_ONE_SHOT == 0x100, "RULE_ONE_SHOT drift");
_Static_assert(RULE_ONE_SHOT_REMOVE == 0x200, "RULE_ONE_SHOT_REMOVE drift");

static struct rule_effects *make_effects(void)
{
    struct rule_effects *e = malloc(sizeof(struct rule_effects));
    memset(e, 0, sizeof(struct rule_effects));
    e->manage = RULE_PROP_UD;
    e->sticky = RULE_PROP_UD;
    e->mff = RULE_PROP_UD;
    e->fullscreen = RULE_PROP_UD;
    return e;
}

static void free_effects(struct rule_effects *e)
{
    if (e->scratchpad) free(e->scratchpad);
    free(e);
}

static int effects_match(struct rule_effects *a, struct rule_effects *b)
{
    if (a->did != b->did) return 0;
    if (a->sid != b->sid) return 0;
    if (memcmp(&a->opacity, &b->opacity, sizeof(float)) != 0) return 0;
    if (a->manage != b->manage) return 0;
    if (a->sticky != b->sticky) return 0;
    if (a->mff != b->mff) return 0;
    if (a->layer != b->layer) return 0;
    if (a->fullscreen != b->fullscreen) return 0;
    if (memcmp(a->grid, b->grid, sizeof(a->grid)) != 0) return 0;
    if ((a->scratchpad == NULL) != (b->scratchpad == NULL)) return 0;
    if (a->scratchpad && strcmp(a->scratchpad, b->scratchpad) != 0) return 0;
    if (a->flags != b->flags) return 0;
    return 1;
}

TEST_FUNC(zig_rule_combine_display_space,
{
    struct rule_effects *fx = make_effects();
    struct rule_effects *c_res = make_effects();
    struct rule_effects *z_res = make_effects();

    fx->did = 5;
    rule_effects_set_flag(fx, RULE_FOLLOW_SPACE);
    fx->sid = 0x1234;
    fx->opacity = 0.5f;
    rule_effects_set_flag(fx, RULE_OPACITY);

    rule_combine_effects(fx, c_res);
    yabai_rule_combine_effects(fx, z_res);

    TEST_CHECK(effects_match(c_res, z_res), 1);
    TEST_CHECK(c_res->did, 5);
    TEST_CHECK(rule_effects_check_flag(c_res, RULE_FOLLOW_SPACE), true);
    TEST_CHECK(c_res->sid, 0x1234);
    TEST_CHECK(c_res->opacity == 0.5f, 1);

    free_effects(fx);
    free_effects(c_res);
    free_effects(z_res);
})

TEST_FUNC(zig_rule_combine_flags_cleared,
{
    struct rule_effects *fx = make_effects();
    struct rule_effects *c_res = make_effects();
    struct rule_effects *z_res = make_effects();

    /* sid without FOLLOW_SPACE clears the flag on a result that had it. */
    fx->sid = 0x99;
    rule_effects_set_flag(c_res, RULE_FOLLOW_SPACE);
    rule_effects_set_flag(z_res, RULE_FOLLOW_SPACE);
    /* Out-of-range opacity is ignored even with the flag set. */
    fx->opacity = 2.0f;
    rule_effects_set_flag(fx, RULE_OPACITY);
    fx->layer = 3;
    rule_effects_set_flag(fx, RULE_LAYER);

    rule_combine_effects(fx, c_res);
    yabai_rule_combine_effects(fx, z_res);

    TEST_CHECK(effects_match(c_res, z_res), 1);
    TEST_CHECK(rule_effects_check_flag(c_res, RULE_FOLLOW_SPACE), false);
    TEST_CHECK(rule_effects_check_flag(c_res, RULE_OPACITY), false);
    TEST_CHECK(c_res->opacity == 0.0f, 1);
    TEST_CHECK(c_res->layer, 3);
    TEST_CHECK(rule_effects_check_flag(c_res, RULE_LAYER), true);

    free_effects(fx);
    free_effects(c_res);
    free_effects(z_res);
})

TEST_FUNC(zig_rule_combine_scratchpad_props_grid,
{
    struct rule_effects *fx = make_effects();
    struct rule_effects *c_res = make_effects();
    struct rule_effects *z_res = make_effects();

    fx->scratchpad = string_copy("newpad");
    c_res->scratchpad = string_copy("oldpad");
    z_res->scratchpad = string_copy("oldpad");
    fx->manage = RULE_PROP_ON;
    fx->sticky = RULE_PROP_OFF;
    fx->mff = RULE_PROP_ON;
    fx->fullscreen = RULE_PROP_UD;
    fx->grid[0] = 1;
    fx->grid[1] = 2;
    fx->grid[2] = 3;
    fx->grid[3] = 4;
    fx->grid[4] = 5;
    fx->grid[5] = 6;

    rule_combine_effects(fx, c_res);
    yabai_rule_combine_effects(fx, z_res);

    TEST_CHECK(effects_match(c_res, z_res), 1);
    TEST_CHECK(strcmp(c_res->scratchpad, "newpad"), 0);
    int same_ptr = (c_res->scratchpad == fx->scratchpad);
    TEST_CHECK(same_ptr, 0);
    TEST_CHECK(c_res->manage, RULE_PROP_ON);
    TEST_CHECK(c_res->sticky, RULE_PROP_OFF);
    TEST_CHECK(c_res->mff, RULE_PROP_ON);
    TEST_CHECK(c_res->fullscreen, RULE_PROP_UD);
    TEST_CHECK(c_res->grid[5], 6);

    /* Zero grid pair leaves the result grid untouched. */
    struct rule_effects *fx2 = make_effects();
    fx2->grid[2] = 9;
    c_res->grid[0] = 7;
    z_res->grid[0] = 7;
    rule_combine_effects(fx2, c_res);
    yabai_rule_combine_effects(fx2, z_res);
    TEST_CHECK(effects_match(c_res, z_res), 1);
    TEST_CHECK(c_res->grid[0], 7);
    TEST_CHECK(c_res->grid[2], 3);

    /* NULL scratchpad leaves the result scratchpad untouched. */
    TEST_CHECK(strcmp(c_res->scratchpad, "newpad"), 0);

    free_effects(fx);
    free_effects(fx2);
    free_effects(c_res);
    free_effects(z_res);
})

/* rule_set_flag / rule_clear_flag / rule_check_flag in src/rule.h: pure
 * uint16 flag reads/writes on a real struct rule. Exhaustive over the
 * 10-bit flag space (0x000..0x3FF) against the C oracle plus hand-pinned
 * vectors for each defined bit. */

static const uint16_t rule_flag_bits[] =
{
    RULE_APP_VALID, RULE_TITLE_VALID, RULE_ROLE_VALID, RULE_SUBROLE_VALID,
    RULE_APP_EXCLUDE, RULE_TITLE_EXCLUDE, RULE_ROLE_EXCLUDE, RULE_SUBROLE_EXCLUDE,
    RULE_ONE_SHOT, RULE_ONE_SHOT_REMOVE
};

TEST_FUNC(zig_rule_flag_parity,
{
    struct rule r;
    memset(&r, 0, sizeof(r));
    TEST_CHECK(rule_check_flag(&r, RULE_APP_VALID), false);
    TEST_CHECK(yabai_rule_check_flag(&r, RULE_APP_VALID), false);
    TEST_CHECK(rule_check_flag(&r, RULE_ONE_SHOT_REMOVE), false);
    TEST_CHECK(yabai_rule_check_flag(&r, RULE_ONE_SHOT_REMOVE), false);

    /* Hand-pinned: single bit set reads back exactly that bit. */
    int b = 0;
    for (b = 0; b < 10; ++b) {
        struct rule c_rule;
        struct rule z_rule;
        memset(&c_rule, 0, sizeof(c_rule));
        memset(&z_rule, 0, sizeof(z_rule));
        c_rule.flags = rule_flag_bits[b];
        z_rule.flags = rule_flag_bits[b];
        int k = 0;
        for (k = 0; k < 10; ++k) {
            bool c = rule_check_flag(&c_rule, rule_flag_bits[k]);
            bool z = yabai_rule_check_flag(&z_rule, rule_flag_bits[k]);
            TEST_CHECK(c != z, 0);
            TEST_CHECK(c, k == b);
        }
    }

    /* Hand-pinned set/clear results. */
    {
        struct rule c_rule;
        struct rule z_rule;
        memset(&c_rule, 0, sizeof(c_rule));
        memset(&z_rule, 0, sizeof(z_rule));
        rule_set_flag(&c_rule, RULE_APP_VALID);
        yabai_rule_set_flag(&z_rule, RULE_APP_VALID);
        TEST_CHECK(c_rule.flags, 0x001);
        TEST_CHECK(z_rule.flags, 0x001);
        rule_set_flag(&c_rule, RULE_ONE_SHOT_REMOVE);
        yabai_rule_set_flag(&z_rule, RULE_ONE_SHOT_REMOVE);
        TEST_CHECK(c_rule.flags, 0x201);
        TEST_CHECK(z_rule.flags, 0x201);
        rule_clear_flag(&c_rule, RULE_APP_VALID);
        yabai_rule_clear_flag(&z_rule, RULE_APP_VALID);
        TEST_CHECK(c_rule.flags, 0x200);
        TEST_CHECK(z_rule.flags, 0x200);
        /* Clearing an unset bit is a no-op. */
        rule_clear_flag(&c_rule, RULE_TITLE_VALID);
        yabai_rule_clear_flag(&z_rule, RULE_TITLE_VALID);
        TEST_CHECK(c_rule.flags, 0x200);
        TEST_CHECK(z_rule.flags, 0x200);
    }

    /* Exhaustive check/set/clear parity over 0x000..0x3FF. */
    unsigned f = 0;
    for (f = 0; f < 0x400; ++f) {
        int j = 0;
        for (j = 0; j < 10; ++j) {
            struct rule c_rule;
            struct rule z_rule;
            memset(&c_rule, 0, sizeof(c_rule));
            memset(&z_rule, 0, sizeof(z_rule));
            c_rule.flags = (uint16_t) f;
            z_rule.flags = (uint16_t) f;

            bool c = rule_check_flag(&c_rule, rule_flag_bits[j]);
            bool z = yabai_rule_check_flag(&z_rule, rule_flag_bits[j]);
            TEST_CHECK(c != z, 0);

            struct rule c_set;
            struct rule z_set;
            memset(&c_set, 0, sizeof(c_set));
            memset(&z_set, 0, sizeof(z_set));
            c_set.flags = (uint16_t) f;
            z_set.flags = (uint16_t) f;
            rule_set_flag(&c_set, rule_flag_bits[j]);
            yabai_rule_set_flag(&z_set, rule_flag_bits[j]);
            TEST_CHECK(c_set.flags != z_set.flags, 0);
            TEST_CHECK(c_set.flags, (uint16_t) (f | rule_flag_bits[j]));

            struct rule c_clr;
            struct rule z_clr;
            memset(&c_clr, 0, sizeof(c_clr));
            memset(&z_clr, 0, sizeof(z_clr));
            c_clr.flags = (uint16_t) f;
            z_clr.flags = (uint16_t) f;
            rule_clear_flag(&c_clr, rule_flag_bits[j]);
            yabai_rule_clear_flag(&z_clr, rule_flag_bits[j]);
            TEST_CHECK(c_clr.flags != z_clr.flags, 0);
            TEST_CHECK(c_clr.flags, (uint16_t) (f & ~rule_flag_bits[j]));
        }
    }

    /* Upper bits pass through untouched. */
    {
        struct rule c_rule;
        struct rule z_rule;
        memset(&c_rule, 0, sizeof(c_rule));
        memset(&z_rule, 0, sizeof(z_rule));
        c_rule.flags = 0xFC00;
        z_rule.flags = 0xFC00;
        TEST_CHECK(rule_check_flag(&c_rule, RULE_APP_VALID), false);
        TEST_CHECK(yabai_rule_check_flag(&z_rule, RULE_APP_VALID), false);
        rule_set_flag(&c_rule, RULE_APP_VALID);
        yabai_rule_set_flag(&z_rule, RULE_APP_VALID);
        TEST_CHECK(c_rule.flags, 0xFC01);
        TEST_CHECK(z_rule.flags, 0xFC01);
    }
})

/* rule_destroy() in src/rule.c: regfree on compiled regexes under
 * VALID-flag guards, then the five heap string fields plus
 * effects.scratchpad when non-NULL. Parity is observable heap
 * behavior: identical NULL/non-NULL shapes destroy without crashing
 * on both sides, compiled-regex rules exercise the regfree path on
 * both sides, and repeated create/destroy cycles stay clean. */

static struct rule *make_heap_rule(int mask, int with_scratchpad)
{
    struct rule *r = malloc(sizeof(struct rule));
    memset(r, 0, sizeof(struct rule));
    if (mask & 0x01) r->label = string_copy("label");
    if (mask & 0x02) r->app = string_copy("app");
    if (mask & 0x04) r->title = string_copy("title");
    if (mask & 0x08) r->role = string_copy("role");
    if (mask & 0x10) r->subrole = string_copy("subrole");
    if (with_scratchpad) r->effects.scratchpad = string_copy("pad");
    return r;
}

TEST_FUNC(zig_rule_destroy_parity,
{
    /* All-NULL rule: both sides are no-ops that must not crash. */
    {
        struct rule *c_rule = make_heap_rule(0x00, 0);
        struct rule *z_rule = make_heap_rule(0x00, 0);
        rule_destroy(c_rule);
        yabai_rule_destroy(z_rule);
        free(c_rule);
        free(z_rule);
    }

    /* Fully populated heap rule (VALID flags clear, regex zeroed). */
    {
        struct rule *c_rule = make_heap_rule(0x1F, 1);
        struct rule *z_rule = make_heap_rule(0x1F, 1);
        rule_destroy(c_rule);
        yabai_rule_destroy(z_rule);
        free(c_rule);
        free(z_rule);
    }

    /* Partial shapes: scratchpad-only, strings-only, mixed. */
    {
        struct rule *c_rule = make_heap_rule(0x00, 1);
        struct rule *z_rule = make_heap_rule(0x00, 1);
        rule_destroy(c_rule);
        yabai_rule_destroy(z_rule);
        free(c_rule);
        free(z_rule);
    }
    {
        struct rule *c_rule = make_heap_rule(0x1F, 0);
        struct rule *z_rule = make_heap_rule(0x1F, 0);
        rule_destroy(c_rule);
        yabai_rule_destroy(z_rule);
        free(c_rule);
        free(z_rule);
    }
    {
        struct rule *c_rule = make_heap_rule(0x09, 0);
        struct rule *z_rule = make_heap_rule(0x09, 0);
        rule_destroy(c_rule);
        yabai_rule_destroy(z_rule);
        free(c_rule);
        free(z_rule);
    }
    {
        struct rule *c_rule = make_heap_rule(0x16, 1);
        struct rule *z_rule = make_heap_rule(0x16, 1);
        rule_destroy(c_rule);
        yabai_rule_destroy(z_rule);
        free(c_rule);
        free(z_rule);
    }

    /* Compiled-regex rules: both sides regfree under VALID guards.
     * A wrong pointer cast on the Zig side crashes here; a skipped
     * regfree leaks (caught by the leaks gate, not this check). */
    {
        struct rule *c_rule = make_heap_rule(0x1F, 1);
        struct rule *z_rule = make_heap_rule(0x1F, 1);
        TEST_CHECK(regcomp(&c_rule->app_regex, "foo.*", REG_EXTENDED), 0);
        TEST_CHECK(regcomp(&z_rule->app_regex, "foo.*", REG_EXTENDED), 0);
        TEST_CHECK(regcomp(&c_rule->title_regex, "^bar$", REG_EXTENDED), 0);
        TEST_CHECK(regcomp(&z_rule->title_regex, "^bar$", REG_EXTENDED), 0);
        rule_set_flag(c_rule, RULE_APP_VALID);
        yabai_rule_set_flag(z_rule, RULE_APP_VALID);
        rule_set_flag(c_rule, RULE_TITLE_VALID);
        yabai_rule_set_flag(z_rule, RULE_TITLE_VALID);
        rule_destroy(c_rule);
        yabai_rule_destroy(z_rule);
        free(c_rule);
        free(z_rule);
    }

    /* Repeated create/destroy cycles: no leaks/crashes on either side. */
    {
        int i = 0;
        for (i = 0; i < 500; ++i) {
            struct rule *c_rule = make_heap_rule(0x1F, 1);
            rule_destroy(c_rule);
            free(c_rule);
        }
        for (i = 0; i < 500; ++i) {
            struct rule *z_rule = make_heap_rule(0x1F, 1);
            yabai_rule_destroy(z_rule);
            free(z_rule);
        }
        /* Interleaved NULL-shape cycles. */
        for (i = 0; i < 200; ++i) {
            struct rule *c_rule = make_heap_rule(i & 0x1F, i & 0x01);
            struct rule *z_rule = make_heap_rule(i & 0x1F, (i >> 1) & 0x01);
            rule_destroy(c_rule);
            yabai_rule_destroy(z_rule);
            free(c_rule);
            free(z_rule);
        }
    }
})
