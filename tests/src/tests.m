unsigned char __src_osax_payload[1];
unsigned int __src_osax_payload_len;
unsigned char __src_osax_loader[1];
unsigned int __src_osax_loader_len;

#include <CoreGraphics/CoreGraphics.h>
static void test_capture_event_post(CGEventTapLocation tap, CGEventRef event);
static CGEventRef test_create_event(CGEventSourceRef source);
#define CGEventPost test_capture_event_post
#define CGEventCreate test_create_event
#include "../../src/manifest.m"
#undef CGEventPost
#undef CGEventCreate

#define TEST_SIG(name) bool test_##name(void)
typedef TEST_SIG(function);

#define TEST_FUNC(name, code) static TEST_SIG(name) { char *test_name = #name; bool result = true; {code} return result; }
#define TEST_CHECK(r, e) if ((r) != (e)) { printf("                   \e[1;33m%s\e[m\e[1;31m#%d %s == %s\e[m \e[1;31m(%d == %d)\e[m\n", test_name, __LINE__, #r, #e, r, e); result = false; }

#include "ref_upstream_rule.c"
#include "area.c"
#include "socket_frame.c"
#include "ref_upstream_view.c"
#include "zig_geometry.c"
#include "zig_message.c"
#include "zig_view.c"
#include "zig_util.c"
#include "zig_rule.c"
#include "zig_window.c"
#include "zig_display.c"
#include "zig_mouse.c"
#include "zig_regress.c"
#include "zig_cli.c"
#include "macos27.c"
#include "event_serialize.c"
#include "sa_response.c"

#define TEST_ENTRY(name) { #name, test_##name },
#define TEST_LIST                                              \
    TEST_ENTRY(macos_swipe_failure)                            \
    TEST_ENTRY(macos_swipe_timing)                             \
    TEST_ENTRY(sa_response_status)                             \
    TEST_ENTRY(macos_space_dictionary)                         \
    TEST_ENTRY(macos_swipe_events)                             \
    TEST_ENTRY(event_serialize_roundtrip)                      \
    TEST_ENTRY(event_serialize_payload_replacement)             \
    TEST_ENTRY(event_serialize_malformed)                      \
    TEST_ENTRY(macos_version_detection)                        \
    TEST_ENTRY(display_area_is_in_direction)                   \
    TEST_ENTRY(closest_display_in_direction)                   \
    TEST_ENTRY(socket_frame_valid_args)                        \
    TEST_ENTRY(socket_frame_byte_exact)                        \
    TEST_ENTRY(socket_frame_malformed)                         \
    TEST_ENTRY(socket_frame_unknown_domain_fails)                \
    TEST_ENTRY(socket_frame_unknown_command_fails)               \
    TEST_ENTRY(socket_frame_rule_list_ok)                        \
    TEST_ENTRY(socket_frame_signal_list_ok)                      \
    TEST_ENTRY(socket_frame_failure_marker)                      \
    TEST_ENTRY(zig_area_make_pair_parity)                        \
    TEST_ENTRY(zig_area_direction_parity)                      \
    TEST_ENTRY(zig_area_make_pair_rounding)                      \
    TEST_ENTRY(zig_area_extreme_values)                          \
    TEST_ENTRY(zig_message_token_stream)                         \
    TEST_ENTRY(zig_message_token_compare)                        \
    TEST_ENTRY(zig_message_token_numbers)                        \
    TEST_ENTRY(zig_message_token_to_value)                       \
    TEST_ENTRY(zig_message_frame_decode)                         \
    TEST_ENTRY(zig_message_parse_property)                       \
    TEST_ENTRY(zig_message_parse_properties)                     \
    TEST_ENTRY(zig_message_parse_label)                          \
    TEST_ENTRY(zig_message_parse_insert_selector)                \
    TEST_ENTRY(zig_message_key_value)                            \
    TEST_ENTRY(zig_message_named_consts)                         \
    TEST_ENTRY(zig_view_leaf_walk)                               \
    TEST_ENTRY(zig_view_window_lookup)                           \
    TEST_ENTRY(zig_view_tree_mutate)                             \
    TEST_ENTRY(zig_view_fence)                                   \
    TEST_ENTRY(zig_view_stack_window)                            \
    TEST_ENTRY(zig_view_find_window_node)                                   \
    TEST_ENTRY(zig_view_occupancy_intermediate)                     \
    TEST_ENTRY(zig_view_child_split_ratio_gap)                      \
    TEST_ENTRY(zig_view_area_convert)                               \
    TEST_ENTRY(zig_view_equalize)                                   \
    TEST_ENTRY(zig_view_balance)                                   \
    TEST_ENTRY(zig_view_area_pair_for_node)                      \
    TEST_ENTRY(zig_view_node_split)                              \
    TEST_ENTRY(zig_view_swap_window_list)                        \
    TEST_ENTRY(zig_view_min_depth_leaf)                          \
    TEST_ENTRY(zig_view_invalid_dirty)                           \
    TEST_ENTRY(zig_view_clear_zoom)                              \
    TEST_ENTRY(zig_view_find_window_list)                        \
    TEST_ENTRY(zig_view_lifecycle_create)                        \
    TEST_ENTRY(zig_view_lifecycle_clear)                         \
    TEST_ENTRY(zig_view_lifecycle_destroy)                       \
    TEST_ENTRY(zig_view_serialize)                               \
    TEST_ENTRY(zig_view_find_in_direction)                       \
    TEST_ENTRY(zig_view_deep_spine)                              \
    TEST_ENTRY(zig_display_serialize)                            \
    TEST_ENTRY(zig_regex_match_parity)                           \
    TEST_ENTRY(zig_rule_matches_window_parity)                   \
    TEST_ENTRY(zig_mouse_mod_parity)                             \
    TEST_ENTRY(zig_mouse_triangle_parity)                        \
    TEST_ENTRY(zig_mouse_drop_action_parity)                     \
    TEST_ENTRY(zig_cli_help_golden)                              \
    TEST_ENTRY(zig_cli_help_domains)                             \
    TEST_ENTRY(zig_cli_version_shape)                            \
    TEST_ENTRY(zig_cli_message_no_args)                          \
    TEST_ENTRY(zig_util_balance_clamp)                           \
    TEST_ENTRY(zig_util_json_bool)                               \
    TEST_ENTRY(zig_util_in_range)                                \
    TEST_ENTRY(zig_util_rgba)                                    \
    TEST_ENTRY(zig_util_find_rank)                               \
    TEST_ENTRY(zig_util_easing)                               \
    TEST_ENTRY(zig_rule_combine_display_space)                   \
    TEST_ENTRY(zig_rule_combine_flags_cleared)                   \
    TEST_ENTRY(zig_rule_combine_scratchpad_props_grid)           \
    TEST_ENTRY(zig_rule_flag_parity)                             \
    TEST_ENTRY(zig_rule_destroy_parity)                          \
    TEST_ENTRY(zig_ts_string_escape_parity)                      \
    TEST_ENTRY(zig_rule_serialize_parity)                        \
    TEST_ENTRY(zig_rule_parse_parity)                            \
    TEST_ENTRY(zig_rule_parse_pinned)                            \
    TEST_ENTRY(zig_rule_parse_relative_parity)                   \
    TEST_ENTRY(zig_rule_parse_lazy_lookup)                       \
    TEST_ENTRY(zig_window_resize_math)                           \
    TEST_ENTRY(zig_window_move_math)                             \
    TEST_ENTRY(zig_window_undersized_parity)                     \
    TEST_ENTRY(zig_window_flag_parity)                             \
    TEST_ENTRY(zig_window_layer_parity)                          \
    TEST_ENTRY(zig_regress_window_list_single_leaf_31)           \
    TEST_ENTRY(zig_regress_window_list_two_leaves_12_31)         \
    TEST_ENTRY(zig_regress_rule_negative_grid)

static struct {
    char *name;
    test_function *func;
} tests[] = {
    TEST_LIST
};

int main(int argc, char **argv)
{
    int succeeded = 0;
    int failed = 0;
    int total = array_count(tests);
    printf("\e[1;34m -- Running %d tests\e[m\n\n", total);

    uint64_t cpu_freq  = read_cpu_freq();
    uint64_t begin_tsc = read_cpu_timer();

    for (int i = 0; i < total; ++i) {
        uint64_t tsc = read_cpu_timer();
        bool result = tests[i].func();
        double ms_elapsed = 1000.0 * (double)(read_cpu_timer() - tsc) / (double)cpu_freq;

        printf("(%0.4fms) %s \e[1;33m%s\e[m\n", ms_elapsed, result ? "\e[1;32msuccess\e[m" : " \e[1;31mfailed\e[m", tests[i].name);
        if (result) ++succeeded; else ++failed;
    }

    double ms_elapsed = 1000.0 * (double)(read_cpu_timer() - begin_tsc) / (double)cpu_freq;
    printf("\n\e[1;34m -- Completed (%0.4fms)\e[m\n", ms_elapsed);
    printf("\t%d \e[1;32msucceeded\e[m\n", succeeded);
    printf("\t%d \e[1;31mfailed\e[m\n", failed);
    printf("\t%d \e[1;33mtotal\e[m\n", total);

    return total == succeeded ? EXIT_SUCCESS : EXIT_FAILURE;
}
