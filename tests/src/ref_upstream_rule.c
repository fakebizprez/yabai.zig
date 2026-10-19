/* FROZEN ORIGINAL C ORACLES -- verbatim copies of upstream yabai dd84572.
 *
 * The daemon's rule/display functions now delegate to Zig, so comparing
 * them against their yabai_* exports would run Zig on both sides. These
 * are the original C bodies from commit dd84572, renamed ref_* and made
 * static; the only edits are those renames (definitions and calls between
 * copied functions), so the whole reference chain stays original C.
 *
 * NEVER edit these bodies to match Zig. A parity failure against them is
 * a Zig bug (or a deliberate, documented behaviour change that belongs in
 * the test, not here).
 *
 * Sources: src/misc/helpers.h (ts_string_escape, regex_match),
 * src/rule.c (rule_serialize, rule_combine_effects),
 * src/window_manager.c (window_manager_rule_matches_window),
 * src/display.c (display_serialize), src/message.c (parse_rule; dead in
 * the daemon since both handle_domain_rule sites call yabai_parse_rule,
 * so it lives only here now). */

static char *ref_ts_string_escape(char *s)
{
    char *cursor = s;
    int num_replacements = 0;

    while (*cursor) {
        if ((*cursor == '"') ||
            (*cursor == '\\') ||
            (*cursor == '\b') ||
            (*cursor == '\f') ||
            (*cursor == '\n') ||
            (*cursor == '\r') ||
            (*cursor == '\t')) {
            ++num_replacements;
        } else if (*cursor >= 0x00 && *cursor <= 0x1f) {
            num_replacements += 5;
        }

        ++cursor;
    }

    if (num_replacements == 0) return NULL;

    int size_in_bytes = (int)(cursor - s) + num_replacements;
    char *result = ts_alloc_unaligned(sizeof(char) * (size_in_bytes+1));
    result[size_in_bytes] = '\0';

    for (char *dst = result, *cursor = s; *cursor; ++cursor) {
        if (*cursor == '"') {
            *dst++ = '\\';
            *dst++ = '"';
        } else if (*cursor == '\\') {
            *dst++ = '\\';
            *dst++ = '\\';
        } else if (*cursor == '\b') {
            *dst++ = '\\';
            *dst++ = 'b';
        } else if (*cursor == '\f') {
            *dst++ = '\\';
            *dst++ = 'f';
        } else if (*cursor == '\n') {
            *dst++ = '\\';
            *dst++ = 'n';
        } else if (*cursor == '\r') {
            *dst++ = '\\';
            *dst++ = 'r';
        } else if (*cursor == '\t') {
            *dst++ = '\\';
            *dst++ = 't';
        } else if (*cursor >= 0x00 && *cursor <= 0x1f) {
            *dst++ = '\\';
            *dst++ = 'u';
            sprintf(dst, "%04x", (int)*cursor);
            dst += 4;
        } else {
            *dst++ = *cursor;
        }
    }

    return result;
}

static int ref_regex_match(bool valid, regex_t *regex, const char *match)
{
    if (!valid) return REGEX_MATCH_UD;

    int result = regexec(regex, match, 0, NULL, 0);
    return result == 0 ? REGEX_MATCH_YES : REGEX_MATCH_NO;
}

static void ref_rule_serialize(FILE *rsp, struct rule *rule, int index)
{
    TIME_FUNCTION;

    char *app   = rule->app;
    char *title = rule->title;
    char *role  = rule->role;
    char *srole = rule->subrole;

    char *escaped_app   = app   ? ref_ts_string_escape(app)   : NULL;
    char *escaped_title = title ? ref_ts_string_escape(title) : NULL;
    char *escaped_role  = role  ? ref_ts_string_escape(role)  : NULL;
    char *escaped_srole = srole ? ref_ts_string_escape(srole) : NULL;

    fprintf(rsp,
            "{\n"
            "\t\"index\":%d,\n"
            "\t\"label\":\"%s\",\n"
            "\t\"app\":\"%s\",\n"
            "\t\"title\":\"%s\",\n"
            "\t\"role\":\"%s\",\n"
            "\t\"subrole\":\"%s\",\n"
            "\t\"display\":%d,\n"
            "\t\"space\":%d,\n"
            "\t\"follow_space\":%s,\n"
            "\t\"opacity\":%.4f,\n"
            "\t\"manage\":%s,\n"
            "\t\"sticky\":%s,\n"
            "\t\"mouse_follows_focus\":%s,\n"
            "\t\"sub-layer\":\"%s\",\n"
            "\t\"native-fullscreen\":%s,\n"
            "\t\"grid\":\"%d:%d:%d:%d:%d:%d\",\n"
            "\t\"scratchpad\":\"%s\",\n"
            "\t\"one-shot\":%s,\n"
            "\t\"flags\":\"0x%08x\"\n"
            "}",
            index,
            rule->label ? rule->label : "",
            escaped_app ? escaped_app : app ? app : "",
            escaped_title ? escaped_title : title ? title : "",
            escaped_role ? escaped_role : role ? role : "",
            escaped_srole ? escaped_srole : srole ? srole : "",
            rule->effects.did ? display_manager_display_id_arrangement(rule->effects.did) : 0,
            rule->effects.sid ? space_manager_mission_control_index(rule->effects.sid) : 0,
            json_bool(rule_effects_check_flag(&rule->effects, RULE_FOLLOW_SPACE)),
            rule->effects.opacity,
            json_optional_bool(rule->effects.manage),
            json_optional_bool(rule->effects.sticky),
            json_optional_bool(rule->effects.mff),
            rule_effects_check_flag(&rule->effects, RULE_LAYER) ? layer_str[rule->effects.layer] : "",
            json_optional_bool(rule->effects.fullscreen),
            rule->effects.grid[0], rule->effects.grid[1],
            rule->effects.grid[2], rule->effects.grid[3],
            rule->effects.grid[4], rule->effects.grid[5],
            rule->effects.scratchpad ? rule->effects.scratchpad : "",
            json_bool(rule_check_flag(rule, RULE_ONE_SHOT)),
            (uint32_t)(rule->effects.flags << 16) | (uint32_t)rule->flags);
}

static void ref_rule_combine_effects(struct rule_effects *effects, struct rule_effects *result)
{
    if (effects->did) {
        result->did = effects->did;
        if (rule_effects_check_flag(effects, RULE_FOLLOW_SPACE)) {
            rule_effects_set_flag(result, RULE_FOLLOW_SPACE);
        } else {
            rule_effects_clear_flag(result, RULE_FOLLOW_SPACE);
        }
    }

    if (effects->sid) {
        result->sid = effects->sid;
        if (rule_effects_check_flag(effects, RULE_FOLLOW_SPACE)) {
            rule_effects_set_flag(result, RULE_FOLLOW_SPACE);
        } else {
            rule_effects_clear_flag(result, RULE_FOLLOW_SPACE);
        }
    }

    if (rule_effects_check_flag(effects, RULE_OPACITY) && in_range_ii(effects->opacity, 0.0f, 1.0f)) {
        result->opacity = effects->opacity;
        rule_effects_set_flag(result, RULE_OPACITY);
    }

    if (rule_effects_check_flag(effects, RULE_LAYER)) {
        result->layer = effects->layer;
        rule_effects_set_flag(result, RULE_LAYER);
    }

    if (effects->scratchpad) {
        if (result->scratchpad) free(result->scratchpad);
        result->scratchpad = string_copy(effects->scratchpad);
    }

    if (effects->manage     != RULE_PROP_UD) result->manage     = effects->manage;
    if (effects->sticky     != RULE_PROP_UD) result->sticky     = effects->sticky;
    if (effects->mff        != RULE_PROP_UD) result->mff        = effects->mff;
    if (effects->fullscreen != RULE_PROP_UD) result->fullscreen = effects->fullscreen;

    if (effects->grid[0] != 0 && effects->grid[1] != 0) {
        result->grid[0] = effects->grid[0];
        result->grid[1] = effects->grid[1];
        result->grid[2] = effects->grid[2];
        result->grid[3] = effects->grid[3];
        result->grid[4] = effects->grid[4];
        result->grid[5] = effects->grid[5];
    }
}

static bool ref_window_manager_rule_matches_window(struct rule *rule, struct window *window, char *window_title, char *window_role, char *window_subrole)
{
    int regex_match_app = rule_check_flag(rule, RULE_APP_EXCLUDE) ? REGEX_MATCH_YES : REGEX_MATCH_NO;
    if (ref_regex_match(rule_check_flag(rule, RULE_APP_VALID), &rule->app_regex, window->application->name) == regex_match_app) return false;

    int regex_match_title = rule_check_flag(rule, RULE_TITLE_EXCLUDE) ? REGEX_MATCH_YES : REGEX_MATCH_NO;
    if (ref_regex_match(rule_check_flag(rule, RULE_TITLE_VALID), &rule->title_regex, window_title) == regex_match_title) return false;

    int regex_match_role = rule_check_flag(rule, RULE_ROLE_EXCLUDE) ? REGEX_MATCH_YES : REGEX_MATCH_NO;
    if (ref_regex_match(rule_check_flag(rule, RULE_ROLE_VALID), &rule->role_regex, window_role) == regex_match_role) return false;

    int regex_match_subrole = rule_check_flag(rule, RULE_SUBROLE_EXCLUDE) ? REGEX_MATCH_YES : REGEX_MATCH_NO;
    if (ref_regex_match(rule_check_flag(rule, RULE_SUBROLE_VALID), &rule->subrole_regex, window_subrole) == regex_match_subrole) return false;

    return true;
}

static void ref_display_serialize(FILE *rsp, uint32_t did, uint64_t flags)
{
    TIME_FUNCTION;

    if (flags == 0x0) flags |= ~flags;

    bool did_output = false;
    fprintf(rsp, "{\n");

    if (flags & DISPLAY_PROPERTY_ID) {
        fprintf(rsp, "\t\"id\":%d", did);
        did_output = true;
    }

    if (flags & DISPLAY_PROPERTY_UUID) {
        if (did_output) fprintf(rsp, ",\n");

        char *uuid = NULL;
        CFStringRef uuid_ref = display_uuid(did);
        if (uuid_ref) {
            uuid = ts_cfstring_copy(uuid_ref);
            CFRelease(uuid_ref);
        }

        fprintf(rsp, "\t\"uuid\":\"%s\"", uuid ? uuid : "<unknown>");
        did_output = true;
    }

    if (flags & DISPLAY_PROPERTY_INDEX) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"index\":%d", display_manager_display_id_arrangement(did));
        did_output = true;
    }

    if (flags & DISPLAY_PROPERTY_LABEL) {
        if (did_output) fprintf(rsp, ",\n");

        struct display_label *display_label = display_manager_get_label_for_display(&g_display_manager, did);
        fprintf(rsp, "\t\"label\":\"%s\"", display_label ? display_label->label : "");
        did_output = true;
    }

    if (flags & DISPLAY_PROPERTY_FRAME) {
        if (did_output) fprintf(rsp, ",\n");

        CGRect frame = CGDisplayBounds(did);
        fprintf(rsp, "\t\"frame\":{\n\t\t\"x\":%.4f,\n\t\t\"y\":%.4f,\n\t\t\"w\":%.4f,\n\t\t\"h\":%.4f\n\t}", frame.origin.x, frame.origin.y, frame.size.width, frame.size.height);
        did_output = true;
    }

    if (flags & DISPLAY_PROPERTY_SPACES) {
        if (did_output) fprintf(rsp, ",\n");

        int count;
        uint64_t *space_list = display_space_list(did, &count);

        fprintf(rsp, "\t\"spaces\":[");
        if (space_list) {
            int first_mci = space_manager_mission_control_index(space_list[0]);
            for (int i = 0; i < count; ++i) {
                if (i < count - 1) {
                    fprintf(rsp, "%d, ", first_mci + i);
                } else {
                    fprintf(rsp, "%d", first_mci + i);
                }
            }
        }
        fprintf(rsp, "]");
        did_output = true;
    }

    if (flags & DISPLAY_PROPERTY_HAS_FOCUS) {
        if (did_output) fprintf(rsp, ",\n");

        fprintf(rsp, "\t\"has-focus\":%s", json_bool(did == g_display_manager.current_display_id));
    }

    fprintf(rsp, "\n}");
}

static bool ref_parse_rule(FILE *rsp, char **message, struct rule *rule, struct token token)
{
    TIME_FUNCTION;

    char *unsupported_exclusion = NULL;
    bool did_parse = true;
    bool has_filter = false;

    for (; token_is_valid(token); token = get_token(message)) {
        char *key = NULL;
        char *value = NULL;
        bool exclusion = false;
        parse_key_value_pair(token.text, &key, &value, &exclusion);

        if (!key || !value) {
            daemon_fail(rsp, "invalid key-value pair '%s'\n", token.text);
            did_parse = false;
            continue;
        }

        if (string_equals(key, ARGUMENT_RULE_KEY_LABEL)) {
            if (exclusion) unsupported_exclusion = key;
            rule->label = string_copy(value);
        } else if (string_equals(key, ARGUMENT_RULE_KEY_SCRATCHPAD)) {
            if (exclusion) unsupported_exclusion = key;

            bool valid = true;
            for (int i = 0; i < array_count(reserved_window_identifiers); ++i) {
                if (string_equals(value, reserved_window_identifiers[i])) {
                    valid = false;
                    break;
                }
            }

            if (valid) {
                rule->effects.scratchpad = string_copy(value);
                rule->effects.manage = RULE_PROP_OFF;
            } else {
                daemon_fail(rsp, "invalid value '%s' for key '%s'\n", value, key);
                did_parse = false;
            }
        } else if (string_equals(key, ARGUMENT_RULE_KEY_APP)) {
            has_filter = true;
            rule->app = string_copy(value);
            if (exclusion) rule_set_flag(rule, RULE_APP_EXCLUDE);
            if (regcomp(&rule->app_regex, value, REG_EXTENDED) == 0) {
                rule_set_flag(rule, RULE_APP_VALID);
            } else {
                daemon_fail(rsp, "invalid regex pattern '%s' for key '%s'\n", value, key);
                did_parse = false;
            }
        } else if (string_equals(key, ARGUMENT_RULE_KEY_TITLE)) {
            has_filter = true;
            rule->title = string_copy(value);
            if (exclusion) rule_set_flag(rule, RULE_TITLE_EXCLUDE);
            if (regcomp(&rule->title_regex, value, REG_EXTENDED) == 0) {
                rule_set_flag(rule, RULE_TITLE_VALID);
            } else {
                daemon_fail(rsp, "invalid regex pattern '%s' for key '%s'\n", value, key);
                did_parse = false;
            }
        } else if (string_equals(key, ARGUMENT_RULE_KEY_ROLE)) {
            has_filter = true;
            rule->role = string_copy(value);
            if (exclusion) rule_set_flag(rule, RULE_ROLE_EXCLUDE);
            if (regcomp(&rule->role_regex, value, REG_EXTENDED) == 0) {
                rule_set_flag(rule, RULE_ROLE_VALID);
            } else {
                daemon_fail(rsp, "invalid regex pattern '%s' for key '%s'\n", value, key);
                did_parse = false;
            }
        } else if (string_equals(key, ARGUMENT_RULE_KEY_SUBROLE)) {
            has_filter = true;
            rule->subrole = string_copy(value);
            if (exclusion) rule_set_flag(rule, RULE_SUBROLE_EXCLUDE);
            if (regcomp(&rule->subrole_regex, value, REG_EXTENDED) == 0) {
                rule_set_flag(rule, RULE_SUBROLE_VALID);
            } else {
                daemon_fail(rsp, "invalid regex pattern '%s' for key '%s'\n", value, key);
                did_parse = false;
            }
        } else if (string_equals(key, ARGUMENT_RULE_KEY_DISPLAY)) {
            if (exclusion) unsupported_exclusion = key;

            if (value[0] == ARGUMENT_RULE_VALUE_SPACE) {
                ++value;
                rule_effects_set_flag(&rule->effects, RULE_FOLLOW_SPACE);
            }

            struct selector selector = parse_display_selector(rsp, &value, display_manager_active_display_id(), false);
            if (selector.did_parse && selector.did) {
                rule->effects.did = selector.did;
            } else {
                did_parse = false;
            }
        } else if (string_equals(key, ARGUMENT_RULE_KEY_SPACE)) {
            if (exclusion) unsupported_exclusion = key;

            if (value[0] == ARGUMENT_RULE_VALUE_SPACE) {
                ++value;
                rule_effects_set_flag(&rule->effects, RULE_FOLLOW_SPACE);
            }

            struct selector selector = parse_space_selector(rsp, &value, space_manager_active_space(), false);
            if (selector.did_parse && selector.sid) {
                rule->effects.sid = selector.sid;
            } else {
                did_parse = false;
            }
        } else if (string_equals(key, ARGUMENT_RULE_KEY_GRID)) {
            if (exclusion) unsupported_exclusion = key;

            if ((sscanf(value, ARGUMENT_RULE_VALUE_GRID,
                        &rule->effects.grid[0], &rule->effects.grid[1],
                        &rule->effects.grid[2], &rule->effects.grid[3],
                        &rule->effects.grid[4], &rule->effects.grid[5]) != 6)) {
                daemon_fail(rsp, "invalid value '%s' for key '%s'\n", value, key);
                did_parse = false;
            }
        } else if (string_equals(key, ARGUMENT_RULE_KEY_OPACITY)) {
            if (exclusion) unsupported_exclusion = key;

            if ((sscanf(value, "%f", &rule->effects.opacity) == 1) && (in_range_ii(rule->effects.opacity, 0.0f, 1.0f))) {
                rule_effects_set_flag(&rule->effects, RULE_OPACITY);
            } else {
                daemon_fail(rsp, "invalid value '%s' for key '%s'\n", value, key);
                did_parse = false;
            }
        } else if (string_equals(key, ARGUMENT_RULE_KEY_MANAGE)) {
            if (exclusion) unsupported_exclusion = key;

            if (string_equals(value, ARGUMENT_COMMON_VAL_ON)) {
                rule->effects.manage = RULE_PROP_ON;
            } else if (string_equals(value, ARGUMENT_COMMON_VAL_OFF)) {
                rule->effects.manage = RULE_PROP_OFF;
            } else {
                daemon_fail(rsp, "invalid value '%s' for key '%s'\n", value, key);
                did_parse = false;
            }
        } else if (string_equals(key, ARGUMENT_RULE_KEY_STICKY)) {
            if (exclusion) unsupported_exclusion = key;

            if (string_equals(value, ARGUMENT_COMMON_VAL_ON)) {
                rule->effects.sticky = RULE_PROP_ON;
            } else if (string_equals(value, ARGUMENT_COMMON_VAL_OFF)) {
                rule->effects.sticky = RULE_PROP_OFF;
            } else {
                daemon_fail(rsp, "invalid value '%s' for key '%s'\n", value, key);
                did_parse = false;
            }
        } else if (string_equals(key, ARGUMENT_RULE_KEY_MFF)) {
            if (exclusion) unsupported_exclusion = key;

            if (string_equals(value, ARGUMENT_COMMON_VAL_ON)) {
                rule->effects.mff = RULE_PROP_ON;
            } else if (string_equals(value, ARGUMENT_COMMON_VAL_OFF)) {
                rule->effects.mff = RULE_PROP_OFF;
            } else {
                daemon_fail(rsp, "invalid value '%s' for key '%s'\n", value, key);
                did_parse = false;
            }
        } else if (string_equals(key, ARGUMENT_RULE_KEY_SUB_LAYER)) {
            if (exclusion) unsupported_exclusion = key;

            if (string_equals(value, ARGUMENT_WINDOW_LAYER_BELOW)) {
                rule->effects.layer = LAYER_BELOW;
                rule_effects_set_flag(&rule->effects, RULE_LAYER);
            } else if (string_equals(value, ARGUMENT_WINDOW_LAYER_NORMAL)) {
                rule->effects.layer = LAYER_NORMAL;
                rule_effects_set_flag(&rule->effects, RULE_LAYER);
            } else if (string_equals(value, ARGUMENT_WINDOW_LAYER_ABOVE)) {
                rule->effects.layer = LAYER_ABOVE;
                rule_effects_set_flag(&rule->effects, RULE_LAYER);
            } else if (string_equals(value, ARGUMENT_WINDOW_LAYER_AUTO)) {
                rule->effects.layer = LAYER_AUTO;
                rule_effects_set_flag(&rule->effects, RULE_LAYER);
            } else {
                daemon_fail(rsp, "invalid value '%s' for key '%s'\n", value, key);
                did_parse = false;
            }
        } else if (string_equals(key, ARGUMENT_RULE_KEY_FULLSCR)) {
            if (exclusion) unsupported_exclusion = key;

            if (string_equals(value, ARGUMENT_COMMON_VAL_ON)) {
                rule->effects.fullscreen = RULE_PROP_ON;
            } else if (string_equals(value, ARGUMENT_COMMON_VAL_OFF)) {
                rule->effects.fullscreen = RULE_PROP_OFF;
            } else {
                daemon_fail(rsp, "invalid value '%s' for key '%s'\n", value, key);
                did_parse = false;
            }
        } else {
            daemon_fail(rsp, "unknown key '%s'\n", key);
            did_parse = false;
        }
    }

    if (!has_filter) {
        daemon_fail(rsp, "missing required key-value pair 'app[!]=..' or 'title[!]=..'\n");
        did_parse = false;
    }

    if (unsupported_exclusion) {
        daemon_fail(rsp, "unsupported token '!' (exclusion) given for key '%s'\n", unsupported_exclusion);
        did_parse = false;
    }

    return did_parse;
}
