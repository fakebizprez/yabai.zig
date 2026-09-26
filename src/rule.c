extern struct space_manager g_space_manager;
extern struct window_manager g_window_manager;

/* Zig delegates (zig/rule.zig): parity-proven equivalents of the JSON
 * emission and rule_combine_effects below. Signatures in src/rule.h are
 * unchanged. The combine export reuses the shared yabai_in_range_ii_f
 * across objects (defined in zig/util.zig). */
void yabai_rule_serialize(FILE *rsp, struct rule *rule, int index, int did_index, int sid_index);
void yabai_rule_combine_effects(struct rule_effects *effects, struct rule_effects *result);

void rule_serialize(FILE *rsp, struct rule *rule, int index)
{
    TIME_FUNCTION;

    yabai_rule_serialize(rsp, rule, index,
                         rule->effects.did ? display_manager_display_id_arrangement(rule->effects.did) : 0,
                         rule->effects.sid ? space_manager_mission_control_index(rule->effects.sid) : 0);
}

void rule_combine_effects(struct rule_effects *effects, struct rule_effects *result)
{
    yabai_rule_combine_effects(effects, result);
}

void rule_reapply_all(void)
{
    table_for (struct window *window, g_window_manager.window, {
        if (window->is_root) {
            char *window_title = window_title_ts(window);
            char *window_role = window_role_ts(window);
            char *window_subrole = window_subrole_ts(window);

            window_manager_apply_manage_rules_to_window(&g_space_manager, &g_window_manager, window, window_title, window_role, window_subrole, false);

            if (window_manager_is_window_eligible(window)) {
                window->is_eligible = true;
                window_manager_apply_rules_to_window(&g_space_manager, &g_window_manager, window, window_title, window_role, window_subrole, false);
            }
        }
    })
}

bool rule_reapply_by_index(int index)
{
    for (int i = 0; i < buf_len(g_window_manager.rules); ++i) {
        if (i == index) {
            if (!rule_check_flag(&g_window_manager.rules[i], RULE_ONE_SHOT)) {
                rule_apply(&g_window_manager.rules[i]);
            }
            return true;
        }
    }

    return false;
}

bool rule_reapply_by_label(char *label)
{
    for (int i = 0; i < buf_len(g_window_manager.rules); ++i) {
        if (string_equals(g_window_manager.rules[i].label, label)) {
            if (!rule_check_flag(&g_window_manager.rules[i], RULE_ONE_SHOT)) {
                rule_apply(&g_window_manager.rules[i]);
            }
            return true;
        }
    }

    return false;
}

void rule_apply(struct rule *rule)
{
    table_for (struct window *window, g_window_manager.window, {
        if (window->is_root) {
            if (window_manager_rule_matches_window(rule, window, window_title_ts(window), window_role_ts(window), window_subrole_ts(window))) {
                window_manager_apply_manage_rule_effects_to_window(&g_space_manager, &g_window_manager, window, &rule->effects);

                if (window_manager_is_window_eligible(window)) {
                    window->is_eligible = true;
                    window_manager_apply_rule_effects_to_window(&g_space_manager, &g_window_manager, window, &rule->effects);
                }
            }
        }
    })
}

void rule_add(struct rule *rule)
{
    if (rule->label) rule_remove_by_label(rule->label);
    buf_push(g_window_manager.rules, *rule);
}

bool rule_remove_by_index(int index)
{
    for (int i = 0; i < buf_len(g_window_manager.rules); ++i) {
        if (i == index) {
            rule_destroy(&g_window_manager.rules[i]);
            buf_del(g_window_manager.rules, i);
            return true;
        }
    }

    return false;
}

bool rule_remove_by_label(char *label)
{
    for (int i = 0; i < buf_len(g_window_manager.rules); ++i) {
        if (string_equals(g_window_manager.rules[i].label, label)) {
            rule_destroy(&g_window_manager.rules[i]);
            buf_del(g_window_manager.rules, i);
            return true;
        }
    }

    return false;
}

void rule_destroy(struct rule *rule)
{
    if (rule_check_flag(rule, RULE_APP_VALID))     regfree(&rule->app_regex);
    if (rule_check_flag(rule, RULE_TITLE_VALID))   regfree(&rule->title_regex);
    if (rule_check_flag(rule, RULE_ROLE_VALID))    regfree(&rule->role_regex);
    if (rule_check_flag(rule, RULE_SUBROLE_VALID)) regfree(&rule->subrole_regex);

    if (rule->label)   free(rule->label);
    if (rule->app)     free(rule->app);
    if (rule->title)   free(rule->title);
    if (rule->role)    free(rule->role);
    if (rule->subrole) free(rule->subrole);

    if (rule->effects.scratchpad) free(rule->effects.scratchpad);
}
