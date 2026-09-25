/* Rule-effects parity tests (doc/zig-refactor-plan.md step 3).
 *
 * The Zig export in zig/rule.zig must match rule_combine_effects() in
 * src/rule.c exactly, including scratchpad ownership (old copy freed,
 * new copy allocated) and the untouched-field paths.
 */

void yabai_rule_combine_effects(struct rule_effects *effects, struct rule_effects *result);

_Static_assert(sizeof(struct rule_effects) == 80, "rule_effects size drift");
_Static_assert(offsetof(struct rule_effects, sid) == 8, "rule_effects.sid drift");
_Static_assert(offsetof(struct rule_effects, grid) == 40, "rule_effects.grid drift");
_Static_assert(offsetof(struct rule_effects, scratchpad) == 64, "rule_effects.scratchpad drift");
_Static_assert(offsetof(struct rule_effects, flags) == 72, "rule_effects.flags drift");

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
