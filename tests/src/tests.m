unsigned char __src_osax_payload[1];
unsigned int __src_osax_payload_len;
unsigned char __src_osax_loader[1];
unsigned int __src_osax_loader_len;

#include "../../src/manifest.m"

#define TEST_SIG(name) bool test_##name(void)
typedef TEST_SIG(function);

#define TEST_FUNC(name, code) static TEST_SIG(name) { char *test_name = #name; bool result = true; {code} return result; }
#define TEST_CHECK(r, e) if ((r) != (e)) { printf("                   \e[1;33m%s\e[m\e[1;31m#%d %s == %s\e[m \e[1;31m(%d == %d)\e[m\n", test_name, __LINE__, #r, #e, r, e); result = false; }

#include "area.c"
#include "socket_frame.c"
#include "zig_geometry.c"
#include "zig_message.c"
#include "zig_view.c"
#include "zig_util.c"
#include "zig_rule.c"

#define TEST_ENTRY(name) { #name, test_##name },
#define TEST_LIST                                              \
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
    TEST_ENTRY(zig_message_token_stream)                         \
    TEST_ENTRY(zig_message_token_compare)                        \
    TEST_ENTRY(zig_message_token_numbers)                        \
    TEST_ENTRY(zig_message_token_to_value)                       \
    TEST_ENTRY(zig_message_frame_decode)                         \
    TEST_ENTRY(zig_message_parse_property)                       \
    TEST_ENTRY(zig_message_key_value)                            \
    TEST_ENTRY(zig_message_named_consts)                         \
    TEST_ENTRY(zig_view_leaf_walk)                               \
    TEST_ENTRY(zig_view_window_lookup)                           \
    TEST_ENTRY(zig_view_tree_mutate)                             \
    TEST_ENTRY(zig_view_fence)                                   \
    TEST_ENTRY(zig_view_stack_window)                                   \
    TEST_ENTRY(zig_util_balance_clamp)                           \
    TEST_ENTRY(zig_util_json_bool)                               \
    TEST_ENTRY(zig_util_in_range)                                \
    TEST_ENTRY(zig_util_rgba)                                    \
    TEST_ENTRY(zig_util_find_rank)                               \
    TEST_ENTRY(zig_rule_combine_display_space)                   \
    TEST_ENTRY(zig_rule_combine_flags_cleared)                   \
    TEST_ENTRY(zig_rule_combine_scratchpad_props_grid)

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
