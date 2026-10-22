/* C side of the Zig boundary (the zig/ modules; mirrors in zig/abi.zig).
 *
 * Included twice from src/manifest.m:
 *  1. after the other headers: every yabai_* prototype, the C structs
 *     that mirror Zig-only types, and the pins for every type and
 *     constant declared in a header;
 *  2. with ZIG_ABI_PRIVATE_PINS defined, after every .c file: the types
 *     and pins that need the structs and enums private to src/message.c.
 *
 * Every number below is a literal, and zig/abi.zig pins the same literals
 * on the Zig side, so drift on either side fails that side's build. The
 * daemon builds universal, so these run for both x86_64 and arm64.
 * Value pins are written one per line as
 *     _Static_assert(C_NAME == literal, "ZIG_NAME");
 * and zig/build.sh checks each literal against the zig/abi.zig constant
 * named in the message, so the two files cannot disagree either. */

#ifndef ZIG_ABI_H
#define ZIG_ABI_H

#define ZIG_ABI_SIZE(type, size) _Static_assert(sizeof(type) == (size), "sizeof(" #type ")")
#define ZIG_ABI_OFFSET(type, field, off) _Static_assert(offsetof(type, field) == (off), "offsetof(" #type ", " #field ")")

/* Types private to src/message.c, complete only in the second phase. */
struct token;
struct token_value;
struct properties;
struct yabai_selector;

/* zig/abi.zig ViewDefaults: the g_space_manager block yabai_view_create
 * copies in the user branch, in C assignment order. */
struct yabai_view_defaults
{
    int layout;
    int top_padding;
    int bottom_padding;
    int left_padding;
    int right_padding;
    int window_gap;
    uint32_t auto_balance;
    int split_type;
};

/* zig/abi.zig RuleLookup: the manager queries yabai_parse_rule needs for
 * display=/space= keys. The acting getters run only inside those
 * branches (upstream order); tests pass counting stubs. */
struct yabai_rule_lookup
{
    uint32_t (*active_display_id)(void);
    uint64_t (*active_space)(void);
    uint32_t (*display_in_direction)(uint32_t did, int direction);
    uint32_t (*prev_display_id)(uint32_t did);
    uint32_t (*next_display_id)(uint32_t did);
    uint64_t (*prev_space)(uint64_t sid);
    uint64_t (*next_space)(uint64_t sid);
};

/* zig/abi.zig FrameField: one field of a decoded socket frame. */
struct frame_field
{
    char *ptr;
    int len;
};

/* ------------------------------------------------------------------ */
/* Prototypes of every Zig export                                     */
/* ------------------------------------------------------------------ */

/* zig/geometry.zig */
void yabai_area_make_pair(int split, int gap, float ratio, const struct area *parent, struct area *left, struct area *right);
bool yabai_area_is_in_direction(const struct area *r1, const CGPoint *r1_max, const struct area *r2, const CGPoint *r2_max, int direction);
int yabai_area_distance_in_direction(const struct area *r1, const CGPoint *r1_max, const struct area *r2, const CGPoint *r2_max, int direction);

/* zig/view.zig */
bool yabai_window_node_is_leaf(struct window_node *node);
bool yabai_window_node_is_left_child(struct window_node *node);
bool yabai_window_node_is_right_child(struct window_node *node);
struct window_node *yabai_window_node_find_first_leaf(struct window_node *root);
struct window_node *yabai_window_node_find_last_leaf(struct window_node *root);
struct window_node *yabai_window_node_find_prev_leaf(struct window_node *node);
struct window_node *yabai_window_node_find_next_leaf(struct window_node *node);
bool yabai_window_node_contains_window(struct window_node *node, uint32_t window_id);
int yabai_window_node_index_of_window(struct window_node *node, uint32_t window_id);
void yabai_window_node_rotate(struct window_node *node, int degrees);
struct window_node *yabai_window_node_mirror(struct window_node *node, int axis);
struct window_node *yabai_window_node_fence(struct window_node *node, int dir);
bool yabai_window_node_is_occupied(struct window_node *node);
bool yabai_window_node_is_intermediate(struct window_node *node);
int yabai_window_node_get_child(struct window_node *node, int window_placement);
int yabai_window_node_get_split(struct view *view, struct window_node *node, int space_split_type);
float yabai_window_node_get_ratio(struct window_node *node, float space_split_ratio);
int yabai_window_node_get_gap(struct view *view);
void yabai_window_node_equalize(struct window_node *node, uint32_t axis_flag, float space_split_ratio);
void yabai_window_node_balance(struct window_node *node, uint32_t axis_flag, struct balance_node *out);
void yabai_window_node_split(struct view *view, struct window_node *node, uint32_t window_id, bool window_zoom_persist, int window_placement, int space_split_type, float space_split_ratio);
void yabai_window_node_swap_window_list(struct window_node *a_node, struct window_node *b_node);
void yabai_window_node_clear_zoom(struct window_node *node);
void yabai_area_from_cgrect(const CGRect *rect, struct area *out);
void yabai_area_max_point(const struct area *area, CGPoint *out);
void yabai_area_make_pair_for_node(struct view *view, struct window_node *node, int space_split_type, float space_split_ratio);
void yabai_view_stack_window(struct window_node *node, uint32_t window_id);
struct window_node *yabai_view_find_min_depth_leaf_node(struct window_node *node);
struct window_node *yabai_view_find_window_node(const struct view *view, uint32_t window_id);
struct window_node *yabai_view_find_window_node_in_direction(const struct view *view, struct window_node *source, int direction, const uint32_t *window_list, int window_count);
/* Returns libc malloc'd memory the caller frees. */
uint32_t *yabai_view_find_window_list(const struct view *view, int *window_count);
bool yabai_view_is_invalid(const struct view *view);
bool yabai_view_is_dirty(const struct view *view);
struct view *yabai_view_create(uint64_t sid, CFStringRef uuid, bool is_user, const struct yabai_view_defaults *defaults);
void yabai_view_clear(struct view *view);
void yabai_view_destroy(struct view *view);
void yabai_view_serialize(FILE *rsp, const struct view *view, uint64_t flags, const char *uuid_str, int mission_index, const char *space_label_str, int display_index, const uint32_t *window_list, int window_count, bool has_focus, bool is_visible, bool is_fullscreen);

/* zig/util.zig */
void yabai_balance_node_add(const struct balance_node *a, const struct balance_node *b, struct balance_node *out);
float yabai_clampf_range(float value, float min, float max);
const char *yabai_json_bool(bool value);
const char *yabai_json_optional_bool(int value);
bool yabai_in_range_ii_f(float a, float b, float c);
bool yabai_in_range_ii_i(int a, int b, int c);
bool yabai_in_range_ei_f(float a, float b, float c);
bool yabai_in_range_ie_i(int a, int b, int c);
int yabai_find_rank_in_list(uint32_t wid, const uint32_t *window_list, int window_count);
void yabai_rgba_color_from_hex(uint32_t color, struct rgba_color *out);
float yabai_ease_in_sine(float t);
float yabai_ease_out_sine(float t);
float yabai_ease_in_out_sine(float t);
float yabai_ease_in_quad(float t);
float yabai_ease_out_quad(float t);
float yabai_ease_in_out_quad(float t);
float yabai_ease_in_cubic(float t);
float yabai_ease_out_cubic(float t);
float yabai_ease_in_out_cubic(float t);
float yabai_ease_in_quart(float t);
float yabai_ease_out_quart(float t);
float yabai_ease_in_out_quart(float t);
float yabai_ease_in_quint(float t);
float yabai_ease_out_quint(float t);
float yabai_ease_in_out_quint(float t);
float yabai_ease_in_expo(float t);
float yabai_ease_out_expo(float t);
float yabai_ease_in_out_expo(float t);
float yabai_ease_in_circ(float t);
float yabai_ease_out_circ(float t);
float yabai_ease_in_out_circ(float t);

/* zig/message.zig */
void yabai_message_get_token(char **message, struct token *out);
bool yabai_token_prefix(const struct token *token, const char *match);
bool yabai_token_equals(const struct token *token, const char *match);
bool yabai_token_is_valid(const struct token *token);
bool yabai_token_is_positive_integer(const struct token *token, int *value);
bool yabai_token_is_hexadecimal(const struct token *token, uint32_t *value);
bool yabai_token_is_float(const struct token *token, float *value);
void yabai_token_to_value(const struct token *token, struct token_value *out);
void yabai_parse_key_value_pair(char *token, char **key, char **value, bool *exclusion);
uint8_t yabai_parse_value_type(const char *type);
uint8_t yabai_parse_resize_handle(const char *handle);
/* fields may be NULL to validate the frame without collecting fields. */
int yabai_socket_frame_decode(const char *frame, size_t frame_size, struct frame_field *fields, size_t max_fields, size_t *out_count);
bool yabai_parse_property(struct properties *properties, const char *property, const uint64_t *property_val, char **property_str, int property_count);
void yabai_parse_properties(FILE *rsp, const struct token *token, const uint64_t *property_val, char **property_str, int property_count, struct properties *out);
/* *label receives libc malloc'd memory the caller frees. */
bool yabai_parse_label(FILE *rsp, const struct token *token, int type, char **label);
void yabai_parse_insert_selector(FILE *rsp, char **message, struct yabai_selector *out);

/* zig/rule.zig */
void yabai_rule_combine_effects(struct rule_effects *effects, struct rule_effects *result);
bool yabai_rule_check_flag(const struct rule *r, uint16_t flag);
void yabai_rule_set_flag(struct rule *r, uint16_t flag);
void yabai_rule_clear_flag(struct rule *r, uint16_t flag);
void yabai_rule_destroy(struct rule *rule);
char *yabai_ts_string_escape(const char *s);
void yabai_rule_serialize(FILE *rsp, struct rule *rule, int index, int did_index, int sid_index);
bool yabai_parse_rule(FILE *rsp, char **message, struct rule *rule, const struct token *token, const struct yabai_rule_lookup *lookup);
int yabai_regex_match(bool valid, const regex_t *regex, const char *match);
bool yabai_rule_matches_window(const struct rule *rule, const char *app_name, const char *window_title, const char *window_role, const char *window_subrole);

/* zig/window.zig */
void yabai_move_frame_math(const CGRect *frame, int type, float dx, float dy, float *x, float *y, float *w, float *h);
void yabai_resize_frame_math(const CGRect *frame, int direction, float dx, float dy, float *fx, float *fy, float *fw, float *fh);
bool yabai_window_is_undersized(double w, double h);
bool yabai_window_can_move(uint8_t flags);
bool yabai_window_can_resize(uint8_t flags);
const char *yabai_window_layer(int level, int below, int normal, int above);

/* zig/display.zig */
void yabai_display_serialize(FILE *rsp, uint32_t did, uint64_t flags, const char *uuid_str, int arrange_index, const char *label_str, double frame_x, double frame_y, double frame_w, double frame_h, const int *space_list, int space_count, bool has_focus);

/* zig/mouse.zig */
bool yabai_triangle_contains_point(const CGPoint *t, const CGPoint *p);
uint8_t yabai_mouse_mod_from_cgflags(uint32_t cgflags);
int yabai_mouse_determine_drop_action(int drop_mode, int src_count, double fx, double fy, double fw, double fh, double px, double py);

/* ------------------------------------------------------------------ */
/* Layout pins (zig/abi.zig pins the same numbers)                    */
/* ------------------------------------------------------------------ */

ZIG_ABI_SIZE(struct area, 16);
ZIG_ABI_OFFSET(struct area, x, 0);
ZIG_ABI_OFFSET(struct area, y, 4);
ZIG_ABI_OFFSET(struct area, w, 8);
ZIG_ABI_OFFSET(struct area, h, 12);

ZIG_ABI_SIZE(CGPoint, 16);
ZIG_ABI_OFFSET(CGPoint, x, 0);
ZIG_ABI_OFFSET(CGPoint, y, 8);

ZIG_ABI_SIZE(CGRect, 32);
ZIG_ABI_OFFSET(CGRect, origin.x, 0);
ZIG_ABI_OFFSET(CGRect, origin.y, 8);
ZIG_ABI_OFFSET(CGRect, size.width, 16);
ZIG_ABI_OFFSET(CGRect, size.height, 24);

ZIG_ABI_SIZE(struct feedback_window, 16);
ZIG_ABI_OFFSET(struct feedback_window, id, 0);
ZIG_ABI_OFFSET(struct feedback_window, context, 8);

ZIG_ABI_SIZE(struct window_node, 344);
ZIG_ABI_OFFSET(struct window_node, area, 0);
ZIG_ABI_OFFSET(struct window_node, parent, 16);
ZIG_ABI_OFFSET(struct window_node, left, 24);
ZIG_ABI_OFFSET(struct window_node, right, 32);
ZIG_ABI_OFFSET(struct window_node, zoom, 40);
ZIG_ABI_OFFSET(struct window_node, window_list, 48);
ZIG_ABI_OFFSET(struct window_node, window_order, 176);
ZIG_ABI_OFFSET(struct window_node, window_count, 304);
ZIG_ABI_OFFSET(struct window_node, ratio, 308);
ZIG_ABI_OFFSET(struct window_node, split, 312);
ZIG_ABI_OFFSET(struct window_node, child, 316);
ZIG_ABI_OFFSET(struct window_node, insert_dir, 320);
ZIG_ABI_OFFSET(struct window_node, feedback_window, 328);

ZIG_ABI_SIZE(struct view, 72);
ZIG_ABI_OFFSET(struct view, uuid, 0);
ZIG_ABI_OFFSET(struct view, sid, 8);
ZIG_ABI_OFFSET(struct view, root, 16);
ZIG_ABI_OFFSET(struct view, insertion_point, 24);
ZIG_ABI_OFFSET(struct view, layout, 28);
ZIG_ABI_OFFSET(struct view, split_type, 32);
ZIG_ABI_OFFSET(struct view, top_padding, 36);
ZIG_ABI_OFFSET(struct view, bottom_padding, 40);
ZIG_ABI_OFFSET(struct view, left_padding, 44);
ZIG_ABI_OFFSET(struct view, right_padding, 48);
ZIG_ABI_OFFSET(struct view, window_gap, 52);
ZIG_ABI_OFFSET(struct view, auto_balance, 56);
ZIG_ABI_OFFSET(struct view, flags, 64);

ZIG_ABI_SIZE(struct yabai_view_defaults, 32);
ZIG_ABI_OFFSET(struct yabai_view_defaults, layout, 0);
ZIG_ABI_OFFSET(struct yabai_view_defaults, top_padding, 4);
ZIG_ABI_OFFSET(struct yabai_view_defaults, bottom_padding, 8);
ZIG_ABI_OFFSET(struct yabai_view_defaults, left_padding, 12);
ZIG_ABI_OFFSET(struct yabai_view_defaults, right_padding, 16);
ZIG_ABI_OFFSET(struct yabai_view_defaults, window_gap, 20);
ZIG_ABI_OFFSET(struct yabai_view_defaults, auto_balance, 24);
ZIG_ABI_OFFSET(struct yabai_view_defaults, split_type, 28);

ZIG_ABI_SIZE(struct yabai_rule_lookup, 56);
ZIG_ABI_OFFSET(struct yabai_rule_lookup, active_display_id, 0);
ZIG_ABI_OFFSET(struct yabai_rule_lookup, active_space, 8);
ZIG_ABI_OFFSET(struct yabai_rule_lookup, display_in_direction, 16);
ZIG_ABI_OFFSET(struct yabai_rule_lookup, prev_display_id, 24);
ZIG_ABI_OFFSET(struct yabai_rule_lookup, next_display_id, 32);
ZIG_ABI_OFFSET(struct yabai_rule_lookup, prev_space, 40);
ZIG_ABI_OFFSET(struct yabai_rule_lookup, next_space, 48);

ZIG_ABI_SIZE(struct balance_node, 8);
ZIG_ABI_OFFSET(struct balance_node, y_count, 0);
ZIG_ABI_OFFSET(struct balance_node, x_count, 4);

ZIG_ABI_SIZE(struct rgba_color, 20);
ZIG_ABI_OFFSET(struct rgba_color, p, 0);
ZIG_ABI_OFFSET(struct rgba_color, r, 4);
ZIG_ABI_OFFSET(struct rgba_color, g, 8);
ZIG_ABI_OFFSET(struct rgba_color, b, 12);
ZIG_ABI_OFFSET(struct rgba_color, a, 16);

/* zig/abi.zig DisplayManagerState / SpaceManagerState mirror only these
 * leading fields; the rest of each manager stays opaque to Zig. */
ZIG_ABI_OFFSET(struct display_manager, current_display_id, 0);
ZIG_ABI_OFFSET(struct display_manager, last_display_id, 4);
ZIG_ABI_SIZE(struct table, 40);
ZIG_ABI_OFFSET(struct space_manager, view, 0);
ZIG_ABI_OFFSET(struct space_manager, current_space_id, 40);
ZIG_ABI_OFFSET(struct space_manager, last_space_id, 48);

ZIG_ABI_SIZE(struct display_label, 16);
ZIG_ABI_OFFSET(struct display_label, did, 0);
ZIG_ABI_OFFSET(struct display_label, label, 8);

ZIG_ABI_SIZE(struct space_label, 16);
ZIG_ABI_OFFSET(struct space_label, sid, 0);
ZIG_ABI_OFFSET(struct space_label, label, 8);

ZIG_ABI_SIZE(struct frame_field, 16);
ZIG_ABI_OFFSET(struct frame_field, ptr, 0);
ZIG_ABI_OFFSET(struct frame_field, len, 8);

ZIG_ABI_SIZE(struct rule_effects, 80);
ZIG_ABI_OFFSET(struct rule_effects, did, 0);
ZIG_ABI_OFFSET(struct rule_effects, sid, 8);
ZIG_ABI_OFFSET(struct rule_effects, opacity, 16);
ZIG_ABI_OFFSET(struct rule_effects, manage, 20);
ZIG_ABI_OFFSET(struct rule_effects, sticky, 24);
ZIG_ABI_OFFSET(struct rule_effects, mff, 28);
ZIG_ABI_OFFSET(struct rule_effects, layer, 32);
ZIG_ABI_OFFSET(struct rule_effects, fullscreen, 36);
ZIG_ABI_OFFSET(struct rule_effects, grid, 40);
ZIG_ABI_OFFSET(struct rule_effects, scratchpad, 64);
ZIG_ABI_OFFSET(struct rule_effects, flags, 72);

ZIG_ABI_SIZE(regex_t, 32);
_Static_assert(_Alignof(regex_t) == 8, "_Alignof(regex_t)");

ZIG_ABI_SIZE(struct rule, 256);
ZIG_ABI_OFFSET(struct rule, label, 0);
ZIG_ABI_OFFSET(struct rule, app, 8);
ZIG_ABI_OFFSET(struct rule, title, 16);
ZIG_ABI_OFFSET(struct rule, role, 24);
ZIG_ABI_OFFSET(struct rule, subrole, 32);
ZIG_ABI_OFFSET(struct rule, app_regex, 40);
ZIG_ABI_OFFSET(struct rule, title_regex, 72);
ZIG_ABI_OFFSET(struct rule, role_regex, 104);
ZIG_ABI_OFFSET(struct rule, subrole_regex, 136);
ZIG_ABI_OFFSET(struct rule, effects, 168);
ZIG_ABI_OFFSET(struct rule, flags, 248);

/* ------------------------------------------------------------------ */
/* Value pins (checked against zig/abi.zig by zig/build.sh)           */
/* ------------------------------------------------------------------ */

/* src/view.h */
_Static_assert(SPLIT_NONE == 0, "SPLIT_NONE");
_Static_assert(SPLIT_Y == 1, "SPLIT_Y");
_Static_assert(SPLIT_X == 2, "SPLIT_X");
_Static_assert(SPLIT_AUTO == 3, "SPLIT_AUTO");
_Static_assert(CHILD_NONE == 0, "CHILD_NONE");
_Static_assert(CHILD_SECOND == 1, "CHILD_SECOND");
_Static_assert(CHILD_FIRST == 2, "CHILD_FIRST");
_Static_assert(NODE_MAX_WINDOW_COUNT == 32, "NODE_MAX_WINDOW_COUNT");
_Static_assert(VIEW_DEFAULT == 0, "VIEW_DEFAULT");
_Static_assert(VIEW_BSP == 1, "VIEW_BSP");
_Static_assert(VIEW_STACK == 2, "VIEW_STACK");
_Static_assert(VIEW_FLOAT == 3, "VIEW_FLOAT");
_Static_assert(VIEW_ENABLE_PADDING == 0x080, "VIEW_ENABLE_PADDING");
_Static_assert(VIEW_ENABLE_GAP == 0x100, "VIEW_ENABLE_GAP");
_Static_assert(VIEW_IS_VALID == 0x200, "VIEW_IS_VALID");
_Static_assert(VIEW_IS_DIRTY == 0x400, "VIEW_IS_DIRTY");
_Static_assert(SPACE_PROPERTY_ID == 0x001, "SPACE_PROPERTY_ID");
_Static_assert(SPACE_PROPERTY_UUID == 0x002, "SPACE_PROPERTY_UUID");
_Static_assert(SPACE_PROPERTY_INDEX == 0x004, "SPACE_PROPERTY_INDEX");
_Static_assert(SPACE_PROPERTY_LABEL == 0x008, "SPACE_PROPERTY_LABEL");
_Static_assert(SPACE_PROPERTY_TYPE == 0x010, "SPACE_PROPERTY_TYPE");
_Static_assert(SPACE_PROPERTY_DISPLAY == 0x020, "SPACE_PROPERTY_DISPLAY");
_Static_assert(SPACE_PROPERTY_WINDOWS == 0x040, "SPACE_PROPERTY_WINDOWS");
_Static_assert(SPACE_PROPERTY_FIRST_WINDOW == 0x080, "SPACE_PROPERTY_FIRST_WINDOW");
_Static_assert(SPACE_PROPERTY_LAST_WINDOW == 0x100, "SPACE_PROPERTY_LAST_WINDOW");
_Static_assert(SPACE_PROPERTY_HAS_FOCUS == 0x200, "SPACE_PROPERTY_HAS_FOCUS");
_Static_assert(SPACE_PROPERTY_IS_VISIBLE == 0x400, "SPACE_PROPERTY_IS_VISIBLE");
_Static_assert(SPACE_PROPERTY_IS_FULLSCREEN == 0x800, "SPACE_PROPERTY_IS_FULLSCREEN");

/* src/misc/macros.h */
_Static_assert(DIR_NORTH == 360, "DIR_NORTH");
_Static_assert(DIR_EAST == 90, "DIR_EAST");
_Static_assert(DIR_SOUTH == 180, "DIR_SOUTH");
_Static_assert(DIR_WEST == 270, "DIR_WEST");
_Static_assert(STACK == 111, "STACK");
_Static_assert(TYPE_ABS == 0x1, "TYPE_ABS");
_Static_assert(TYPE_REL == 0x2, "TYPE_REL");
_Static_assert(HANDLE_TOP == 0x01, "HANDLE_TOP");
_Static_assert(HANDLE_BOTTOM == 0x02, "HANDLE_BOTTOM");
_Static_assert(HANDLE_LEFT == 0x04, "HANDLE_LEFT");
_Static_assert(HANDLE_RIGHT == 0x08, "HANDLE_RIGHT");
_Static_assert(HANDLE_ABS == 0x10, "HANDLE_ABS");
_Static_assert(LAYER_AUTO == 0, "LAYER_AUTO");
_Static_assert(LAYER_BELOW == 3, "LAYER_BELOW");
_Static_assert(LAYER_NORMAL == 4, "LAYER_NORMAL");
_Static_assert(LAYER_ABOVE == 5, "LAYER_ABOVE");
_Static_assert(REGEX_MATCH_UD == 0, "REGEX_MATCH_UD");
_Static_assert(REGEX_MATCH_YES == 1, "REGEX_MATCH_YES");
_Static_assert(REGEX_MATCH_NO == 2, "REGEX_MATCH_NO");

/* src/window.h */
_Static_assert(WINDOW_SHADOW == 0x01, "WINDOW_SHADOW");
_Static_assert(WINDOW_FULLSCREEN == 0x02, "WINDOW_FULLSCREEN");
_Static_assert(WINDOW_MINIMIZE == 0x04, "WINDOW_MINIMIZE");
_Static_assert(WINDOW_FLOAT == 0x08, "WINDOW_FLOAT");
_Static_assert(WINDOW_STICKY == 0x10, "WINDOW_STICKY");
_Static_assert(WINDOW_WINDOWED == 0x20, "WINDOW_WINDOWED");
_Static_assert(WINDOW_MOVABLE == 0x40, "WINDOW_MOVABLE");
_Static_assert(WINDOW_RESIZABLE == 0x80, "WINDOW_RESIZABLE");

/* src/display.h */
_Static_assert(DISPLAY_PROPERTY_ID == 0x01, "DISPLAY_PROPERTY_ID");
_Static_assert(DISPLAY_PROPERTY_UUID == 0x02, "DISPLAY_PROPERTY_UUID");
_Static_assert(DISPLAY_PROPERTY_INDEX == 0x04, "DISPLAY_PROPERTY_INDEX");
_Static_assert(DISPLAY_PROPERTY_LABEL == 0x08, "DISPLAY_PROPERTY_LABEL");
_Static_assert(DISPLAY_PROPERTY_FRAME == 0x10, "DISPLAY_PROPERTY_FRAME");
_Static_assert(DISPLAY_PROPERTY_SPACES == 0x20, "DISPLAY_PROPERTY_SPACES");
_Static_assert(DISPLAY_PROPERTY_HAS_FOCUS == 0x40, "DISPLAY_PROPERTY_HAS_FOCUS");

/* src/mouse_handler.h */
_Static_assert(MOUSE_MOD_NONE == 0x01, "MOUSE_MOD_NONE");
_Static_assert(MOUSE_MOD_ALT == 0x02, "MOUSE_MOD_ALT");
_Static_assert(MOUSE_MOD_SHIFT == 0x04, "MOUSE_MOD_SHIFT");
_Static_assert(MOUSE_MOD_CMD == 0x08, "MOUSE_MOD_CMD");
_Static_assert(MOUSE_MOD_CTRL == 0x10, "MOUSE_MOD_CTRL");
_Static_assert(MOUSE_MOD_FN == 0x20, "MOUSE_MOD_FN");
_Static_assert(MOUSE_DROP_ACTION_NONE == 0, "MOUSE_DROP_ACTION_NONE");
_Static_assert(MOUSE_DROP_ACTION_STACK == 1, "MOUSE_DROP_ACTION_STACK");
_Static_assert(MOUSE_DROP_ACTION_SWAP == 2, "MOUSE_DROP_ACTION_SWAP");
_Static_assert(MOUSE_DROP_ACTION_WARP_TOP == 3, "MOUSE_DROP_ACTION_WARP_TOP");
_Static_assert(MOUSE_DROP_ACTION_WARP_RIGHT == 4, "MOUSE_DROP_ACTION_WARP_RIGHT");
_Static_assert(MOUSE_DROP_ACTION_WARP_BOTTOM == 5, "MOUSE_DROP_ACTION_WARP_BOTTOM");
_Static_assert(MOUSE_DROP_ACTION_WARP_LEFT == 6, "MOUSE_DROP_ACTION_WARP_LEFT");
_Static_assert(MOUSE_MODE_NONE == 0, "MOUSE_MODE_NONE");
_Static_assert(MOUSE_MODE_MOVE == 1, "MOUSE_MODE_MOVE");
_Static_assert(MOUSE_MODE_RESIZE == 2, "MOUSE_MODE_RESIZE");
_Static_assert(MOUSE_MODE_SWAP == 3, "MOUSE_MODE_SWAP");
_Static_assert(MOUSE_MODE_STACK == 4, "MOUSE_MODE_STACK");

/* CGEventTypes.h (NX_*MASK in IOLLEvent.h) */
_Static_assert(kCGEventFlagMaskShift == 0x00020000, "CG_FLAG_MASK_SHIFT");
_Static_assert(kCGEventFlagMaskControl == 0x00040000, "CG_FLAG_MASK_CONTROL");
_Static_assert(kCGEventFlagMaskAlternate == 0x00080000, "CG_FLAG_MASK_ALTERNATE");
_Static_assert(kCGEventFlagMaskCommand == 0x00100000, "CG_FLAG_MASK_COMMAND");
_Static_assert(kCGEventFlagMaskSecondaryFn == 0x00800000, "CG_FLAG_MASK_SECONDARY_FN");

/* src/rule.h, <regex.h> */
_Static_assert(RULE_PROP_UD == 0, "RULE_PROP_UD");
_Static_assert(RULE_PROP_ON == 1, "RULE_PROP_ON");
_Static_assert(RULE_PROP_OFF == 2, "RULE_PROP_OFF");
_Static_assert(RULE_FOLLOW_SPACE == 0x01, "RULE_FOLLOW_SPACE");
_Static_assert(RULE_OPACITY == 0x02, "RULE_OPACITY");
_Static_assert(RULE_LAYER == 0x04, "RULE_LAYER");
_Static_assert(RULE_APP_VALID == 0x001, "RULE_APP_VALID");
_Static_assert(RULE_TITLE_VALID == 0x002, "RULE_TITLE_VALID");
_Static_assert(RULE_ROLE_VALID == 0x004, "RULE_ROLE_VALID");
_Static_assert(RULE_SUBROLE_VALID == 0x008, "RULE_SUBROLE_VALID");
_Static_assert(RULE_APP_EXCLUDE == 0x010, "RULE_APP_EXCLUDE");
_Static_assert(RULE_TITLE_EXCLUDE == 0x020, "RULE_TITLE_EXCLUDE");
_Static_assert(RULE_ROLE_EXCLUDE == 0x040, "RULE_ROLE_EXCLUDE");
_Static_assert(RULE_SUBROLE_EXCLUDE == 0x080, "RULE_SUBROLE_EXCLUDE");
_Static_assert(RULE_ONE_SHOT == 0x100, "RULE_ONE_SHOT");
_Static_assert(RULE_ONE_SHOT_REMOVE == 0x200, "RULE_ONE_SHOT_REMOVE");
_Static_assert(REG_EXTENDED == 1, "REG_EXTENDED");

#endif

/* ------------------------------------------------------------------ */
/* Second phase: needs the types private to src/message.c             */
/* ------------------------------------------------------------------ */

#if defined(ZIG_ABI_PRIVATE_PINS) && !defined(ZIG_ABI_PRIVATE_PINS_DONE)
#define ZIG_ABI_PRIVATE_PINS_DONE

/* zig/abi.zig Selector: struct selector with its union as one raw u64
 * payload (Zig reads dir/did from the low bits; little-endian only). */
struct yabai_selector
{
    struct token token;
    bool did_parse;
    uint64_t payload;
};

ZIG_ABI_SIZE(struct token, 16);
ZIG_ABI_OFFSET(struct token, text, 0);
ZIG_ABI_OFFSET(struct token, length, 8);

ZIG_ABI_SIZE(struct token_value, 32);
ZIG_ABI_OFFSET(struct token_value, token, 0);
ZIG_ABI_OFFSET(struct token_value, type, 16);
ZIG_ABI_OFFSET(struct token_value, int_value, 24);
ZIG_ABI_OFFSET(struct token_value, float_value, 24);
ZIG_ABI_OFFSET(struct token_value, u32_value, 24);
ZIG_ABI_OFFSET(struct token_value, string_value, 24);

ZIG_ABI_SIZE(struct properties, 32);
ZIG_ABI_OFFSET(struct properties, token, 0);
ZIG_ABI_OFFSET(struct properties, did_parse, 16);
ZIG_ABI_OFFSET(struct properties, did_error, 17);
ZIG_ABI_OFFSET(struct properties, flags, 24);

ZIG_ABI_SIZE(struct selector, 32);
ZIG_ABI_OFFSET(struct selector, token, 0);
ZIG_ABI_OFFSET(struct selector, did_parse, 16);
ZIG_ABI_OFFSET(struct selector, dir, 24);
ZIG_ABI_OFFSET(struct selector, did, 24);
ZIG_ABI_OFFSET(struct selector, sid, 24);
ZIG_ABI_OFFSET(struct selector, window, 24);

ZIG_ABI_SIZE(struct yabai_selector, 32);
ZIG_ABI_OFFSET(struct yabai_selector, token, 0);
ZIG_ABI_OFFSET(struct yabai_selector, did_parse, 16);
ZIG_ABI_OFFSET(struct yabai_selector, payload, 24);

_Static_assert(TOKEN_TYPE_INVALID == 0, "TOKEN_TYPE_INVALID");
_Static_assert(TOKEN_TYPE_UNKNOWN == 1, "TOKEN_TYPE_UNKNOWN");
_Static_assert(TOKEN_TYPE_INT == 2, "TOKEN_TYPE_INT");
_Static_assert(TOKEN_TYPE_FLOAT == 3, "TOKEN_TYPE_FLOAT");
_Static_assert(TOKEN_TYPE_U32 == 4, "TOKEN_TYPE_U32");
_Static_assert(TOKEN_TYPE_STRING == 5, "TOKEN_TYPE_STRING");
_Static_assert(LABEL_DISPLAY == 0, "LABEL_DISPLAY");
_Static_assert(LABEL_SPACE == 1, "LABEL_SPACE");
_Static_assert(LABEL_WINDOW == 2, "LABEL_WINDOW");

#endif
