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
char *yabai_ts_string_escape(const char *s);
void yabai_rule_serialize(FILE *rsp, struct rule *rule, int index, int did_index, int sid_index);

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

/* rule_serialize() (src/rule.c) + ts_string_escape() (src/misc/helpers.h)
 * parity: Zig exports in zig/rule.zig must match the C oracle byte for
 * byte via open_memstream. Must FAIL against stubs (TDD red) and pass on
 * the real port (green). */

_Static_assert(LAYER_AUTO == 0, "LAYER_AUTO drift");
_Static_assert(LAYER_BELOW == 3, "LAYER_BELOW drift");
_Static_assert(LAYER_NORMAL == 4, "LAYER_NORMAL drift");
_Static_assert(LAYER_ABOVE == 5, "LAYER_ABOVE drift");

static void ensure_ts_for_serialize(void)
{
    static bool ts_ready = false;
    if (!ts_ready) {
        ts_init(MEGABYTES(8));
        ts_ready = true;
    }
    ts_reset();
}

static int escape_case_parity(char *input)
{
    ensure_ts_for_serialize();
    char *c = ts_string_escape(input);
    char *z = yabai_ts_string_escape(input);
    int ok = 1;
    if ((c == NULL) != (z == NULL)) ok = 0;
    else if (c && z && strcmp(c, z) != 0) ok = 0;
    else if (c && z && memcmp(c, z, strlen(c) + 1) != 0) ok = 0;
    if (!ok) {
        printf("escape mismatch input len %zu c %s z %s\n", strlen(input), c ? c : "(null)", z ? z : "(null)");
    }
    if (z) free(z);
    ts_reset();
    return ok;
}

TEST_FUNC(zig_ts_string_escape_parity,
{
    ensure_ts_for_serialize();
    {
        char plain[] = "hello";
        TEST_CHECK(escape_case_parity(plain), 1);
    }
    {
        char empty[] = "";
        TEST_CHECK(escape_case_parity(empty), 1);
    }
    {
        char utf8[] = "\303\251";
        TEST_CHECK(escape_case_parity(utf8), 1);
        TEST_CHECK(ts_string_escape(utf8) == NULL, true);
        char *z = yabai_ts_string_escape(utf8);
        TEST_CHECK(z == NULL, true);
        if (z) free(z);
        ts_reset();
    }
    {
        char s1[] = "\"";
        TEST_CHECK(escape_case_parity(s1), 1);
        {
            char *z = yabai_ts_string_escape(s1);
            TEST_CHECK(z != NULL, true);
            if (z) TEST_CHECK(strcmp(z, "\\\""), 0);
            free(z);
        }
        ts_reset();
    }
    {
        char s2[] = "\\";
        TEST_CHECK(escape_case_parity(s2), 1);
    }
    {
        char bs[] = "\010";
        TEST_CHECK(escape_case_parity(bs), 1);
        char *z = yabai_ts_string_escape(bs);
        TEST_CHECK(z != NULL, true);
        if (z) TEST_CHECK(strcmp(z, "\\b"), 0);
        free(z);
        ts_reset();
    }
    {
        char ff[] = "\014";
        TEST_CHECK(escape_case_parity(ff), 1);
        char *z = yabai_ts_string_escape(ff);
        TEST_CHECK(z != NULL, true);
        if (z) TEST_CHECK(strcmp(z, "\\f"), 0);
        free(z);
        ts_reset();
    }
    {
        char nl[] = "\n";
        TEST_CHECK(escape_case_parity(nl), 1);
    }
    {
        char cr[] = "\r";
        TEST_CHECK(escape_case_parity(cr), 1);
    }
    {
        char tab[] = "\t";
        TEST_CHECK(escape_case_parity(tab), 1);
    }
    {
        char c1[] = "\001";
        TEST_CHECK(escape_case_parity(c1), 1);
        char *z = yabai_ts_string_escape(c1);
        TEST_CHECK(z != NULL, true);
        if (z) TEST_CHECK(strcmp(z, "\\u0001"), 0);
        free(z);
        ts_reset();
    }
    {
        char c2[] = "\002";
        TEST_CHECK(escape_case_parity(c2), 1);
    }
    {
        char bell[] = "\007";
        TEST_CHECK(escape_case_parity(bell), 1);
        char *z = yabai_ts_string_escape(bell);
        TEST_CHECK(z != NULL, true);
        if (z) TEST_CHECK(strcmp(z, "\\u0007"), 0);
        free(z);
        ts_reset();
    }
    {
        char us[] = "\037";
        TEST_CHECK(escape_case_parity(us), 1);
        char *z = yabai_ts_string_escape(us);
        TEST_CHECK(z != NULL, true);
        if (z) TEST_CHECK(strcmp(z, "\\u001f"), 0);
        free(z);
        ts_reset();
    }
    {
        char combo[] = "a\"\\\010\014\n\r\t\001\037end";
        TEST_CHECK(escape_case_parity(combo), 1);
    }
    {
        char mixed[] = "a\"b\\c\nd\te";
        TEST_CHECK(escape_case_parity(mixed), 1);
    }
    {
        char title[] = "line1\nline2";
        TEST_CHECK(escape_case_parity(title), 1);
    }
})

static int lifted_did_for(struct rule *r)
{
    return r->effects.did ? display_manager_display_id_arrangement(r->effects.did) : 0;
}

static int lifted_sid_for(struct rule *r)
{
    return r->effects.sid ? space_manager_mission_control_index(r->effects.sid) : 0;
}

static void free_serialize_rule_strings(struct rule *r)
{
    if (r->label) free(r->label);
    if (r->app) free(r->app);
    if (r->title) free(r->title);
    if (r->role) free(r->role);
    if (r->subrole) free(r->subrole);
    if (r->effects.scratchpad) free(r->effects.scratchpad);
    r->label = NULL;
    r->app = NULL;
    r->title = NULL;
    r->role = NULL;
    r->subrole = NULL;
    r->effects.scratchpad = NULL;
}

static int serialize_parity(struct rule *r, int index)
{
    ensure_ts_for_serialize();
    ts_reset();
    int did_idx = lifted_did_for(r);
    int sid_idx = lifted_sid_for(r);
    char *c_msg = NULL;
    size_t c_len = 0;
    char *z_msg = NULL;
    size_t z_len = 0;
    FILE *c_rsp = open_memstream(&c_msg, &c_len);
    FILE *z_rsp = open_memstream(&z_msg, &z_len);
    if (!c_rsp || !z_rsp) {
        if (c_rsp) fclose(c_rsp);
        if (z_rsp) fclose(z_rsp);
        free(c_msg);
        free(z_msg);
        return 0;
    }
    rule_serialize(c_rsp, r, index);
    yabai_rule_serialize(z_rsp, r, index, did_idx, sid_idx);
    fflush(c_rsp);
    fflush(z_rsp);
    fclose(c_rsp);
    fclose(z_rsp);
    int ok = 1;
    if (c_len != z_len) ok = 0;
    else if (c_len > 0 && memcmp(c_msg, z_msg, c_len) != 0) ok = 0;
    if (!ok) {
        printf("serialize mismatch index %d c_len %zu z_len %zu\n---C---\n%.*s\n---Z---\n%.*s\n",
               index, c_len, z_len,
               (int) c_len, c_msg ? c_msg : "(null)",
               (int) z_len, z_msg ? z_msg : "(null)");
    }
    free(c_msg);
    free(z_msg);
    ts_reset();
    return ok;
}

static void init_serialize_base(struct rule *r)
{
    memset(r, 0, sizeof(struct rule));
    r->effects.opacity = 1.0f;
}

TEST_FUNC(zig_rule_serialize_parity,
{
    ensure_ts_for_serialize();
    /* layer_str pins (runtime; indices pinned statically above). */
    TEST_CHECK(strcmp(layer_str[LAYER_AUTO], "auto"), 0);
    TEST_CHECK(strcmp(layer_str[LAYER_BELOW], "below"), 0);
    TEST_CHECK(strcmp(layer_str[LAYER_NORMAL], "normal"), 0);
    TEST_CHECK(strcmp(layer_str[LAYER_ABOVE], "above"), 0);

    /* All-NULL rule. */
    {
        struct rule r;
        init_serialize_base(&r);
        r.effects.opacity = 1.0f;
        TEST_CHECK(serialize_parity(&r, 0), 1);
        free_serialize_rule_strings(&r);
    }

    /* Fully populated plain strings. */
    {
        struct rule r;
        init_serialize_base(&r);
        r.label = string_copy("my-label");
        r.app = string_copy("Terminal");
        r.title = string_copy("bash");
        r.role = string_copy("AXWindow");
        r.subrole = string_copy("AXStandardWindow");
        r.effects.scratchpad = string_copy("pad1");
        r.effects.manage = RULE_PROP_ON;
        r.effects.sticky = RULE_PROP_OFF;
        r.effects.mff = RULE_PROP_ON;
        r.effects.fullscreen = RULE_PROP_OFF;
        r.effects.opacity = 0.5f;
        rule_effects_set_flag(&r.effects, RULE_OPACITY);
        rule_effects_set_flag(&r.effects, RULE_FOLLOW_SPACE);
        r.effects.grid[0] = 1;
        r.effects.grid[1] = 2;
        r.effects.grid[2] = 3;
        r.effects.grid[3] = 4;
        r.effects.grid[4] = 5;
        r.effects.grid[5] = 6;
        rule_set_flag(&r, RULE_ONE_SHOT);
        TEST_CHECK(serialize_parity(&r, 7), 1);
        free_serialize_rule_strings(&r);
    }

    /* Quotable fields: quotes, backslash, newline, controls. */
    {
        struct rule r;
        init_serialize_base(&r);
        r.app = string_copy("a\"b");
        r.title = string_copy("a\\b");
        r.role = string_copy("line1\nline2");
        r.subrole = string_copy("tab\there");
        TEST_CHECK(serialize_parity(&r, 1), 1);
        free_serialize_rule_strings(&r);
    }
    {
        struct rule r;
        init_serialize_base(&r);
        char ctrl_app[] = "x\001\002\037y";
        char ctrl_title[] = "q\"\\\010\014\r";
        r.app = string_copy(ctrl_app);
        r.title = string_copy(ctrl_title);
        r.role = string_copy("\b\f\n\r\t");
        TEST_CHECK(serialize_parity(&r, 2), 1);
        free_serialize_rule_strings(&r);
    }
    /* Label/scratchpad carry quotables raw (never escaped in C). */
    {
        struct rule r;
        init_serialize_base(&r);
        r.label = string_copy("lab\"el\\with\nnewline");
        r.effects.scratchpad = string_copy("pad\"\\x");
        TEST_CHECK(serialize_parity(&r, 3), 1);
        free_serialize_rule_strings(&r);
    }

    /* follow_space on/off. */
    {
        struct rule r;
        init_serialize_base(&r);
        TEST_CHECK(serialize_parity(&r, 0), 1);
        rule_effects_set_flag(&r.effects, RULE_FOLLOW_SPACE);
        TEST_CHECK(serialize_parity(&r, 0), 1);
        rule_effects_clear_flag(&r.effects, RULE_FOLLOW_SPACE);
        TEST_CHECK(serialize_parity(&r, 0), 1);
        free_serialize_rule_strings(&r);
    }

    /* manage/sticky/mff/fullscreen: UD(null)/ON(true)/OFF(false). */
    {
        int a = 0;
        int b = 0;
        for (a = 0; a < 3; ++a) {
            for (b = 0; b < 3; ++b) {
                struct rule r;
                init_serialize_base(&r);
                int va = a == 0 ? RULE_PROP_UD : (a == 1 ? RULE_PROP_ON : RULE_PROP_OFF);
                int vb = b == 0 ? RULE_PROP_UD : (b == 1 ? RULE_PROP_ON : RULE_PROP_OFF);
                r.effects.manage = va;
                r.effects.sticky = vb;
                r.effects.mff = va;
                r.effects.fullscreen = vb;
                TEST_CHECK(serialize_parity(&r, a * 3 + b), 1);
                free_serialize_rule_strings(&r);
            }
        }
    }

    /* Layer flag clear (empty) vs set with each valid layer. */
    {
        int i = 0;
        {
            struct rule r;
            init_serialize_base(&r);
            r.effects.layer = LAYER_ABOVE;
            TEST_CHECK(serialize_parity(&r, 0), 1);
            free_serialize_rule_strings(&r);
        }
        for (i = 0; i < 4; ++i) {
            struct rule r;
            init_serialize_base(&r);
            int lay = LAYER_AUTO;
            if (i == 1) lay = LAYER_BELOW;
            if (i == 2) lay = LAYER_NORMAL;
            if (i == 3) lay = LAYER_ABOVE;
            r.effects.layer = lay;
            rule_effects_set_flag(&r.effects, RULE_LAYER);
            TEST_CHECK(serialize_parity(&r, i), 1);
            free_serialize_rule_strings(&r);
        }
    }

    /* Grid: zero vs populated. */
    {
        struct rule r;
        init_serialize_base(&r);
        TEST_CHECK(serialize_parity(&r, 0), 1);
        r.effects.grid[0] = 1;
        r.effects.grid[1] = 1;
        r.effects.grid[2] = 10;
        r.effects.grid[3] = 20;
        r.effects.grid[4] = 30;
        r.effects.grid[5] = 40;
        TEST_CHECK(serialize_parity(&r, 0), 1);
        free_serialize_rule_strings(&r);
    }

    /* Opacity edge values (%.4f rounding). */
    {
        int i = 0;
        for (i = 0; i < 10; ++i) {
            struct rule r;
            init_serialize_base(&r);
            float op = 0.0f;
            if (i == 1) op = 1.0f;
            if (i == 2) op = 0.5f;
            if (i == 3) op = 0.123456f;
            if (i == 4) op = 2.0f;
            if (i == 5) op = -0.5f;
            if (i == 6) op = 0.00005f;
            if (i == 7) op = 0.99995f;
            if (i == 8) op = 0.1f;
            if (i == 9) op = 0.33333f;
            r.effects.opacity = op;
            TEST_CHECK(serialize_parity(&r, i), 1);
            free_serialize_rule_strings(&r);
        }
    }

    /* Flags word: (effects.flags << 16) | flags, 0x%08x lowercase. */
    {
        struct rule r;
        init_serialize_base(&r);
        TEST_CHECK(serialize_parity(&r, 0), 1);
        free_serialize_rule_strings(&r);
    }
    {
        struct rule r;
        init_serialize_base(&r);
        r.effects.flags = 0x07;
        r.flags = 0x3FF;
        TEST_CHECK(serialize_parity(&r, 0), 1);
        free_serialize_rule_strings(&r);
    }
    {
        struct rule r;
        init_serialize_base(&r);
        r.effects.flags = 0xABCD;
        r.flags = 0xEF01;
        TEST_CHECK(serialize_parity(&r, 0), 1);
        free_serialize_rule_strings(&r);
    }
    {
        struct rule r;
        init_serialize_base(&r);
        rule_set_flag(&r, RULE_ONE_SHOT);
        rule_effects_set_flag(&r.effects, RULE_FOLLOW_SPACE);
        rule_effects_set_flag(&r.effects, RULE_OPACITY);
        rule_effects_set_flag(&r.effects, RULE_LAYER);
        r.effects.layer = LAYER_NORMAL;
        TEST_CHECK(serialize_parity(&r, 5), 1);
        free_serialize_rule_strings(&r);
    }

    /* Non-zero did/sid with lifted indices (manager-derived oracle). */
    {
        struct rule r;
        init_serialize_base(&r);
        r.effects.did = 5;
        r.effects.sid = 0x1234;
        TEST_CHECK(serialize_parity(&r, 0), 1);
        free_serialize_rule_strings(&r);
    }
    /* Lifted-index seam: non-zero did/sid emit the lifted param;
     * zero did/sid emit 0 even with non-zero lifted params. */
    {
        struct rule r;
        init_serialize_base(&r);
        r.effects.did = 9;
        r.effects.sid = 99;
        char *z_msg = NULL;
        size_t z_len = 0;
        FILE *z_rsp = open_memstream(&z_msg, &z_len);
        yabai_rule_serialize(z_rsp, &r, 0, 2, 7);
        fflush(z_rsp);
        fclose(z_rsp);
        TEST_CHECK(z_msg != NULL, true);
        if (z_msg) {
            TEST_CHECK(strstr(z_msg, "\"display\":2") != NULL, true);
            TEST_CHECK(strstr(z_msg, "\"space\":7") != NULL, true);
        }
        free(z_msg);
        free_serialize_rule_strings(&r);
    }
    {
        struct rule r;
        init_serialize_base(&r);
        char *z_msg = NULL;
        size_t z_len = 0;
        FILE *z_rsp = open_memstream(&z_msg, &z_len);
        yabai_rule_serialize(z_rsp, &r, 0, 2, 7);
        fflush(z_rsp);
        fclose(z_rsp);
        TEST_CHECK(z_msg != NULL, true);
        if (z_msg) {
            TEST_CHECK(strstr(z_msg, "\"display\":0") != NULL, true);
            TEST_CHECK(strstr(z_msg, "\"space\":0") != NULL, true);
        }
        free(z_msg);
        free_serialize_rule_strings(&r);
    }
    /* NULL stream: Zig is a no-op that must not crash (C has no NULL
     * guard, so only the Zig side is exercised here). */
    {
        struct rule r;
        init_serialize_base(&r);
        r.app = string_copy("hi");
        yabai_rule_serialize(NULL, &r, 0, 0, 0);
        free_serialize_rule_strings(&r);
    }
})

/* parse_rule() (src/message.c) + yabai_parse_rule (zig/rule.zig) parity.
 *
 * The C static is the oracle: this file is #included into tests.m after
 * src/message.c, so parse_rule/get_token are directly callable. Message
 * buffers are NUL-joined token arrays plus a final NUL, matching the
 * daemon wire frame; each side gets its own copy because parsing writes
 * NULs in place. Rules start zeroed like handle_domain_rule's stack rule
 * and both sides are destroyed after every case (leaks gate). Token arrays
 * use parenthesized compound literals: TEST_FUNC/TEST_CHECK macro arguments
 * split on commas inside braces but not inside parens.
 *
 * display=/space= keys never reach the C oracle here: parse_rule evaluates
 * display_manager_active_display_id()/space_manager_active_space() eagerly
 * and the former asserts on a NULL SLS uuid in-process. Those branches run
 * Zig-only with the acting ids lifted in (same seam as
 * yabai_rule_serialize's did_index/sid_index) against hand-pinned bytes. */

bool yabai_parse_rule(FILE *rsp, char **message, struct rule *rule, const struct token *token, uint32_t acting_did, uint64_t acting_sid);

_Static_assert(offsetof(struct display_manager, last_display_id) == 4, "display_manager.last_display_id drift");
_Static_assert(offsetof(struct space_manager, last_space_id) == 48, "space_manager.last_space_id drift");
_Static_assert(offsetof(struct display_label, did) == 0, "display_label.did drift");
_Static_assert(offsetof(struct space_label, sid) == 0, "space_label.sid drift");
_Static_assert(RULE_PROP_UD == 0, "RULE_PROP_UD drift");
_Static_assert(RULE_PROP_ON == 1, "RULE_PROP_ON drift");
_Static_assert(RULE_PROP_OFF == 2, "RULE_PROP_OFF drift");
_Static_assert(TOKEN_TYPE_INVALID == 0, "TOKEN_TYPE_INVALID drift");
_Static_assert(TOKEN_TYPE_INT == 2, "TOKEN_TYPE_INT drift");
_Static_assert(TOKEN_TYPE_STRING == 5, "TOKEN_TYPE_STRING drift");
_Static_assert(REG_EXTENDED == 1, "REG_EXTENDED drift");

static const char *rsv_words[] = {
    ARGUMENT_WINDOW_TOGGLE_FLOAT, ARGUMENT_WINDOW_TOGGLE_STICKY,
    ARGUMENT_WINDOW_TOGGLE_SHADOW, ARGUMENT_WINDOW_TOGGLE_SPLIT,
    ARGUMENT_WINDOW_TOGGLE_PARENT, ARGUMENT_WINDOW_TOGGLE_FULLSC,
    ARGUMENT_WINDOW_TOGGLE_WINDOWED, ARGUMENT_WINDOW_TOGGLE_NATIVE,
    ARGUMENT_WINDOW_TOGGLE_EXPOSE, ARGUMENT_WINDOW_TOGGLE_PIP,
    ARGUMENT_WINDOW_SCRATCHPAD_RECOVER
};

static const char *onoff_keys[] = { "manage", "sticky", "mouse_follows_focus", "native-fullscreen" };
static const char *layer_vals[] = { "below", "normal", "above", "auto", "middle" };

static char *rule_msg_join(const char **toks, int n)
{
    size_t total = 1;
    int i = 0;
    for (i = 0; i < n; ++i) total += strlen(toks[i]) + 1;
    char *buf = malloc(total);
    char *p = buf;
    for (i = 0; i < n; ++i) {
        size_t l = strlen(toks[i]) + 1;
        memcpy(p, toks[i], l);
        p += l;
    }
    *p = 0;
    return buf;
}

static int rule_str_eq(char *a, char *b)
{
    if ((a == NULL) != (b == NULL)) return 0;
    if (a && strcmp(a, b) != 0) return 0;
    return 1;
}

static int rule_fx_eq(struct rule_effects *a, struct rule_effects *b)
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
    if (!rule_str_eq(a->scratchpad, b->scratchpad)) return 0;
    if (a->flags != b->flags) return 0;
    return 1;
}

static int rules_equal(struct rule *a, struct rule *b)
{
    if (!rule_str_eq(a->label, b->label)) return 0;
    if (!rule_str_eq(a->app, b->app)) return 0;
    if (!rule_str_eq(a->title, b->title)) return 0;
    if (!rule_str_eq(a->role, b->role)) return 0;
    if (!rule_str_eq(a->subrole, b->subrole)) return 0;
    if (!rule_fx_eq(&a->effects, &b->effects)) return 0;
    if (a->flags != b->flags) return 0;
    return 1;
}

/* Full oracle parity: return value, emitted bytes, resulting rule shape
 * (regex internals excluded, VALID flags compared), and cursor drain. */
static int rule_case_oracle(const char **toks, int n)
{
    char *c_buf = rule_msg_join(toks, n);
    char *z_buf = rule_msg_join(toks, n);
    char *c_cur = c_buf;
    char *z_cur = z_buf;
    struct token c_first = get_token(&c_cur);
    struct token z_first = get_token(&z_cur);
    struct rule c_rule;
    struct rule z_rule;
    memset(&c_rule, 0, sizeof(c_rule));
    memset(&z_rule, 0, sizeof(z_rule));
    char *c_msg = NULL;
    size_t c_len = 0;
    char *z_msg = NULL;
    size_t z_len = 0;
    FILE *c_rsp = open_memstream(&c_msg, &c_len);
    FILE *z_rsp = open_memstream(&z_msg, &z_len);
    bool c_ret = parse_rule(c_rsp, &c_cur, &c_rule, c_first);
    bool z_ret = yabai_parse_rule(z_rsp, &z_cur, &z_rule, &z_first, 0, 0);
    fflush(c_rsp);
    fflush(z_rsp);
    fclose(c_rsp);
    fclose(z_rsp);
    int ok = 1;
    if (c_ret != z_ret) {
        printf("rule ret mismatch c %d z %d toks:", c_ret, z_ret);
        int i = 0;
        for (i = 0; i < n; ++i) printf(" [%s]", toks[i]);
        printf("\n");
        ok = 0;
    }
    if (c_len != z_len || (c_len > 0 && memcmp(c_msg, z_msg, c_len) != 0)) {
        printf("rule output mismatch toks:");
        int i = 0;
        for (i = 0; i < n; ++i) printf(" [%s]", toks[i]);
        printf("\nc %zu: %.*s\nz %zu: %.*s\n", c_len, (int) c_len, c_msg ? c_msg : "", z_len, (int) z_len, z_msg ? z_msg : "");
        ok = 0;
    }
    if (!rules_equal(&c_rule, &z_rule)) {
        printf("rule shape mismatch toks:");
        int i = 0;
        for (i = 0; i < n; ++i) printf(" [%s]", toks[i]);
        printf("\nc flags 0x%x fx 0x%x did %u sid %llu app %s title %s\nz flags 0x%x fx 0x%x did %u sid %llu app %s title %s\n",
               c_rule.flags, c_rule.effects.flags, c_rule.effects.did, (unsigned long long) c_rule.effects.sid,
               c_rule.app ? c_rule.app : "(null)", c_rule.title ? c_rule.title : "(null)",
               z_rule.flags, z_rule.effects.flags, z_rule.effects.did, (unsigned long long) z_rule.effects.sid,
               z_rule.app ? z_rule.app : "(null)", z_rule.title ? z_rule.title : "(null)");
        ok = 0;
    }
    if (c_cur[0] != 0 || z_cur[0] != 0) {
        printf("rule cursor not drained\n");
        ok = 0;
    }
    rule_destroy(&c_rule);
    yabai_rule_destroy(&z_rule);
    free(c_buf);
    free(z_buf);
    free(c_msg);
    free(z_msg);
    return ok;
}

TEST_FUNC(zig_rule_parse_parity,
{
    TEST_CHECK(rule_case_oracle((const char *[]){"app=Terminal"}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"app!=Terminal"}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"title=bash"}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"title!=bash"}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"role=AXWindow"}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"role!=AXWindow"}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"subrole=AXStandardWindow"}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"subrole!=AXStandardWindow"}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"title=^(foo|bar).*$"}, 1), 1);
    /* Invalid regexes: string kept, VALID clear, regex-shape bytes. */
    TEST_CHECK(rule_case_oracle((const char *[]){"app=(["}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"title!=a{2,1}"}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"role=*bad"}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"subrole=\\"}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"app=[unclosed", "title=OK.*"}, 2), 1);
    /* Label / scratchpad. */
    TEST_CHECK(rule_case_oracle((const char *[]){"label=mylabel", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"label!=mylabel", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"scratchpad=pad1", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"scratchpad!=pad", "app=x"}, 2), 1);
    /* Every reserved window identifier is rejected as a scratchpad. */
    {
        int i = 0;
        for (i = 0; i < 11; ++i) {
            char tok[128];
            snprintf(tok, sizeof(tok), "scratchpad=%s", rsv_words[i]);
            TEST_CHECK(rule_case_oracle((const char *[]){tok, "app=x"}, 2), 1);
        }
    }
    /* Near-misses of reserved words are accepted. */
    TEST_CHECK(rule_case_oracle((const char *[]){"scratchpad=floating", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"scratchpad=Float", "app=x"}, 2), 1);
    /* Grid via sscanf %d:%d:%d:%d:%d:%d. */
    TEST_CHECK(rule_case_oracle((const char *[]){"grid=1:2:3:4:5:6", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"grid=1:2", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"grid=a:b:c:d:e:f", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"grid=1:2:3:4:5", "app=x"}, 2), 1);
    /* Opacity via sscanf %f + in_range_ii (sscanf prefix match counts). */
    TEST_CHECK(rule_case_oracle((const char *[]){"opacity=0.5", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"opacity=0", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"opacity=1", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"opacity=1.0", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"opacity=2", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"opacity=-0.1", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"opacity=abc", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"opacity=0.5abc", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"opacity=", "app=x"}, 2), 1);
    /* manage/sticky/mff/fullscreen on/off/bogus. */
    {
        int k = 0;
        for (k = 0; k < 4; ++k) {
            char t_on[64];
            char t_off[64];
            char t_bogus[64];
            snprintf(t_on, sizeof(t_on), "%s=on", onoff_keys[k]);
            snprintf(t_off, sizeof(t_off), "%s=off", onoff_keys[k]);
            snprintf(t_bogus, sizeof(t_bogus), "%s=maybe", onoff_keys[k]);
            TEST_CHECK(rule_case_oracle((const char *[]){t_on, "app=x"}, 2), 1);
            TEST_CHECK(rule_case_oracle((const char *[]){t_off, "app=x"}, 2), 1);
            TEST_CHECK(rule_case_oracle((const char *[]){t_bogus, "app=x"}, 2), 1);
        }
    }
    /* sub-layer values. */
    {
        int i = 0;
        for (i = 0; i < 5; ++i) {
            char tok[64];
            snprintf(tok, sizeof(tok), "sub-layer=%s", layer_vals[i]);
            TEST_CHECK(rule_case_oracle((const char *[]){tok, "app=x"}, 2), 1);
        }
    }
    /* Malformed pairs, unknown keys, missing filter, exclusions. */
    TEST_CHECK(rule_case_oracle((const char *[]){"noequals"}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"key="}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"!=", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"bogus=1", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"bogus!=1", "app=x"}, 2), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"label=only"}, 1), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"grid!=1:1:1:1:1:1", "label!=x", "app=y"}, 3), 1);
    TEST_CHECK(rule_case_oracle((const char *[]){"opacity!=0.5", "app=x"}, 2), 1);
    /* Full happy path across every pure key. */
    TEST_CHECK(rule_case_oracle((const char *[]){"label=L", "app=Term.*", "title!=bash", "role=AXWindow", "subrole=AXStandardWindow", "scratchpad=pad", "manage=on", "sticky=off", "mouse_follows_focus=on", "native-fullscreen=off", "sub-layer=above", "opacity=0.75", "grid=1:1:10:20:30:40"}, 13), 1);
})

/* Zig-only pins: hand-pinned daemon_fail bytes (0x07 marker + shape),
 * the manager-backed display/space branches with lifted acting ids, and
 * the NULL-stream case (C has no NULL guard; only Zig is exercised). */
static int z_rule_pinned(const char **toks, int n, uint32_t acting_did, uint64_t acting_sid, bool exp_ret, const char *exp_out)
{
    char *z_buf = rule_msg_join(toks, n);
    char *z_cur = z_buf;
    struct token z_first = get_token(&z_cur);
    struct rule z_rule;
    memset(&z_rule, 0, sizeof(z_rule));
    char *z_msg = NULL;
    size_t z_len = 0;
    FILE *z_rsp = open_memstream(&z_msg, &z_len);
    bool z_ret = yabai_parse_rule(z_rsp, &z_cur, &z_rule, &z_first, acting_did, acting_sid);
    fflush(z_rsp);
    fclose(z_rsp);
    int ok = 1;
    size_t exp_len = strlen(exp_out);
    if (z_ret != exp_ret) {
        printf("pinned ret mismatch got %d want %d\n", z_ret, exp_ret);
        ok = 0;
    }
    if (z_len != exp_len || (exp_len > 0 && memcmp(z_msg, exp_out, exp_len) != 0)) {
        printf("pinned output mismatch got %zu: %.*s want %zu: %s\n",
               z_len, (int) z_len, z_msg ? z_msg : "", exp_len, exp_out);
        ok = 0;
    }
    yabai_rule_destroy(&z_rule);
    free(z_buf);
    free(z_msg);
    return ok;
}

TEST_FUNC(zig_rule_parse_pinned,
{
    /* Hand-pinned error bytes for the invalid-pair / invalid-value /
     * invalid-regex shapes plus unknown-key / missing-filter /
     * unsupported-exclusion. */
    TEST_CHECK(z_rule_pinned((const char *[]){"noequals"}, 1, 0, 0, false, "\x07invalid key-value pair 'noequals'\n\x07missing required key-value pair 'app[!]=..' or 'title[!]=..'\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"grid=1:2", "app=x"}, 2, 0, 0, false, "\x07invalid value '1:2' for key 'grid'\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"app=(["}, 1, 0, 0, false, "\x07invalid regex pattern '([' for key 'app'\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"bogus=1", "app=x"}, 2, 0, 0, false, "\x07unknown key 'bogus'\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"label=only"}, 1, 0, 0, false, "\x07missing required key-value pair 'app[!]=..' or 'title[!]=..'\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"label!=x", "app=y"}, 2, 0, 0, false, "\x07unsupported token '!' (exclusion) given for key 'label'\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"app=x"}, 1, 0, 0, true, ""), 1);
    /* display=/space= with zeroed managers: recent resolves to id 0
     * (no emission, effect unset, parse fails); unknown labels miss;
     * space ints miss via the NULL-guarded SLS path; floats/hex take
     * the else path. Display int/first/last/mouse lookups dereference
     * NULL CoreFoundation state in-process, so those branches are
     * covered by inspection only (same reason the C oracle is
     * uncallable for every display/space key). */
    TEST_CHECK(z_rule_pinned((const char *[]){"display=recent", "app=x"}, 2, 0, 0, false, ""), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"space=recent", "app=x"}, 2, 0, 0, false, ""), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"display=bogus", "app=x"}, 2, 0, 0, false, "\x07value 'bogus' is not a valid option for DISPLAY_SEL\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"space=bogus", "app=x"}, 2, 0, 0, false, "\x07value 'bogus' is not a valid option for SPACE_SEL\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"space=2", "app=x"}, 2, 0, 0, false, "\007could not locate space with mission-control index '2'.\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"space=first", "app=x"}, 2, 0, 0, false, "\007could not locate the first space.\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"space=last", "app=x"}, 2, 0, 0, false, "\007could not locate the last space.\n"), 1);
    /* Direction/relative selectors with no acting id take the
     * could-not-locate-the-selected path without any manager call. */
    TEST_CHECK(z_rule_pinned((const char *[]){"display=north", "app=x"}, 2, 0, 0, false, "\007could not locate the selected display.\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"display=prev", "app=x"}, 2, 0, 0, false, "\007could not locate the selected display.\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"space=next", "app=x"}, 2, 0, 0, false, "\007could not locate the selected space.\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"space=prev", "app=x"}, 2, 0, 0, false, "\007could not locate the selected space.\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"display=1.5", "app=x"}, 2, 0, 0, false, "\x07value '1.5' is not a valid option for DISPLAY_SEL\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"space=0x10", "app=x"}, 2, 0, 0, false, "\x07value '0x10' is not a valid option for SPACE_SEL\n"), 1);
    TEST_CHECK(z_rule_pinned((const char *[]){"display=^", "app=x"}, 2, 0, 0, false, "\x07value '' is not a valid option for DISPLAY_SEL\n"), 1);
    /* Follow-space prefix sets the effect flag even when lookup fails. */
    {
        char *z_buf = rule_msg_join((const char *[]){"display=^recent", "app=x"}, 2);
        char *z_cur = z_buf;
        struct token z_first = get_token(&z_cur);
        struct rule z_rule;
        memset(&z_rule, 0, sizeof(z_rule));
        bool z_ret = yabai_parse_rule(NULL, &z_cur, &z_rule, &z_first, 0, 0);
        TEST_CHECK(z_ret, false);
        TEST_CHECK(rule_effects_check_flag(&z_rule.effects, RULE_FOLLOW_SPACE), true);
        TEST_CHECK(z_rule.effects.did, 0);
        TEST_CHECK(z_rule.app && strcmp(z_rule.app, "x") == 0, true);
        yabai_rule_destroy(&z_rule);
        free(z_buf);
    }
    /* NULL stream: no crash, same return, effects still applied. */
    {
        char *z_buf = rule_msg_join((const char *[]){"app=x", "manage=on"}, 2);
        char *z_cur = z_buf;
        struct token z_first = get_token(&z_cur);
        struct rule z_rule;
        memset(&z_rule, 0, sizeof(z_rule));
        bool z_ret = yabai_parse_rule(NULL, &z_cur, &z_rule, &z_first, 0, 0);
        TEST_CHECK(z_ret, true);
        TEST_CHECK(z_rule.effects.manage, RULE_PROP_ON);
        yabai_rule_destroy(&z_rule);
        free(z_buf);
    }
    {
        char *z_buf = rule_msg_join((const char *[]){"grid=bad", "app=x"}, 2);
        char *z_cur = z_buf;
        struct token z_first = get_token(&z_cur);
        struct rule z_rule;
        memset(&z_rule, 0, sizeof(z_rule));
        bool z_ret = yabai_parse_rule(NULL, &z_cur, &z_rule, &z_first, 0, 0);
        TEST_CHECK(z_ret, false);
        yabai_rule_destroy(&z_rule);
        free(z_buf);
    }
})
