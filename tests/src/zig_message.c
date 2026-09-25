/* Message tokenizer parity tests (doc/zig-refactor-plan.md step 3).
 *
 * The Zig exports in zig/message.zig must match the token helpers in
 * src/message.c exactly. Layout contract: C struct token is 16 bytes and
 * C struct token_value is 32 bytes; the Zig MsgToken/TokenValue mirrors
 * assert the same sizes at compile time.
 */

void yabai_message_get_token(char **message, struct token *out);
bool yabai_token_prefix(const struct token *token, char *match);
bool yabai_token_equals(const struct token *token, char *match);
bool yabai_token_is_valid(const struct token *token);
bool yabai_token_is_positive_integer(const struct token *token, int *value);
bool yabai_token_is_hexadecimal(const struct token *token, uint32_t *value);
bool yabai_token_is_float(const struct token *token, float *value);
void yabai_token_to_value(const struct token *token, struct token_value *out);

_Static_assert(sizeof(struct token) == 16, "struct token must stay 16 bytes");
_Static_assert(sizeof(struct token_value) == 32, "struct token_value must stay 32 bytes");
_Static_assert(offsetof(struct token_value, type) == 16, "token_value.type must sit at offset 16");

static int token_stream_check(char *frame, int total)
{
    char *base = frame + sizeof(int);
    char *c_cursor = base;
    char *z_cursor = base;
    int done = 0;
    int i = 0;

    for (i = 0; i < 16; ++i) {
        struct token ct = get_token(&c_cursor);
        struct token zt;
        yabai_message_get_token(&z_cursor, &zt);

        if (ct.length != zt.length) return 0;
        if (memcmp(ct.text, zt.text, ct.length) != 0) return 0;
        if (c_cursor - base != z_cursor - base) return 0;
        if (ct.length == 0) {
            done = 1;
            break;
        }
    }

    (void) total;
    return done;
}

TEST_FUNC(zig_message_token_stream,
{
    char *argv1[4];
    argv1[0] = "yabai";
    argv1[1] = "window";
    argv1[2] = "--focus";
    argv1[3] = "west";
    char frame1[256];
    int total1 = socket_frame_encode(4, argv1, frame1, sizeof(frame1));
    TEST_CHECK(token_stream_check(frame1, total1), 1);

    char *argv2[3];
    argv2[0] = "yabai";
    argv2[1] = "query";
    argv2[2] = "--windows";
    char frame2[256];
    int total2 = socket_frame_encode(3, argv2, frame2, sizeof(frame2));
    TEST_CHECK(token_stream_check(frame2, total2), 1);

    char *argv3[6];
    argv3[0] = "yabai";
    argv3[1] = "config";
    argv3[2] = "--space";
    argv3[3] = "3";
    argv3[4] = "--layout";
    argv3[5] = "bsp";
    char frame3[256];
    int total3 = socket_frame_encode(6, argv3, frame3, sizeof(frame3));
    TEST_CHECK(token_stream_check(frame3, total3), 1);
})

static char *compare_token_texts[] = { "query", "qu", "", "queryx", "0x1F", "3.5", "--windows" };
static char *compare_matches[] = { "query", "qu", "", "queryx", "q" };

TEST_FUNC(zig_message_token_compare,
{
    int text_count = array_count(compare_token_texts);
    int match_count = array_count(compare_matches);
    int i = 0;
    int j = 0;

    for (i = 0; i < text_count; ++i) {
        struct token t;
        t.text = compare_token_texts[i];
        t.length = strlen(compare_token_texts[i]);

        bool c_valid = token_is_valid(t);
        bool z_valid = yabai_token_is_valid(&t);
        TEST_CHECK(c_valid != z_valid, 0);

        for (j = 0; j < match_count; ++j) {
            bool c_pre = token_prefix(t, compare_matches[j]);
            bool z_pre = yabai_token_prefix(&t, compare_matches[j]);
            TEST_CHECK(c_pre != z_pre, 0);

            bool c_eq = token_equals(t, compare_matches[j]);
            bool z_eq = yabai_token_equals(&t, compare_matches[j]);
            TEST_CHECK(c_eq != z_eq, 0);
        }
    }

    struct token nt;
    nt.text = NULL;
    nt.length = 5;
    TEST_CHECK(token_is_valid(nt) != yabai_token_is_valid(&nt), 0);
    nt.length = 0;
    TEST_CHECK(token_is_valid(nt) != yabai_token_is_valid(&nt), 0);
})

static char *int_texts[] = { "0", "42", "007", "4294967295", "", "12a", "-5", " 12", "0x10" };
static char *hex_texts[] = { "0x1F", "0XdeadBEEF", "0x0", "0x", "0xG1", "123", "", "0x123456789" };
static char *float_texts[] = { "3.5", "-0.25", "1e3", "nan", "inf", "0x1p3", "", "abc", "12px" };

TEST_FUNC(zig_message_token_numbers,
{
    int i = 0;

    int int_count = array_count(int_texts);
    for (i = 0; i < int_count; ++i) {
        struct token t;
        t.text = int_texts[i];
        t.length = strlen(int_texts[i]);
        int c_value = 0;
        int z_value = 0;
        bool c_ok = token_is_positive_integer(t, &c_value);
        bool z_ok = yabai_token_is_positive_integer(&t, &z_value);
        TEST_CHECK(c_ok != z_ok, 0);
        if (c_ok && z_ok) TEST_CHECK(c_value != z_value, 0);
    }

    int hex_count = array_count(hex_texts);
    for (i = 0; i < hex_count; ++i) {
        struct token t;
        t.text = hex_texts[i];
        t.length = strlen(hex_texts[i]);
        uint32_t c_value = 0;
        uint32_t z_value = 0;
        bool c_ok = token_is_hexadecimal(t, &c_value);
        bool z_ok = yabai_token_is_hexadecimal(&t, &z_value);
        TEST_CHECK(c_ok != z_ok, 0);
        if (c_ok && z_ok) TEST_CHECK(c_value != z_value, 0);
    }

    int float_count = array_count(float_texts);
    for (i = 0; i < float_count; ++i) {
        struct token t;
        t.text = float_texts[i];
        t.length = strlen(float_texts[i]);
        float c_value = 0.0f;
        float z_value = 0.0f;
        bool c_ok = token_is_float(t, &c_value);
        bool z_ok = yabai_token_is_float(&t, &z_value);
        TEST_CHECK(c_ok != z_ok, 0);
        if (c_ok && z_ok) TEST_CHECK(memcmp(&c_value, &z_value, sizeof(float)), 0);
    }

    struct token known;
    known.text = "3.5";
    known.length = 3;
    float known_value = 0.0f;
    bool known_ok = yabai_token_is_float(&known, &known_value);
    TEST_CHECK(known_ok, 1);
    TEST_CHECK(known_value == 3.5f, 1);
})

struct to_value_case
{
    char *text;
    int expected_type;
};

static struct to_value_case to_value_cases[] =
{
    { "42", TOKEN_TYPE_INT },
    { "0x2A", TOKEN_TYPE_U32 },
    { "4.5", TOKEN_TYPE_FLOAT },
    { "hello", TOKEN_TYPE_STRING },
    { "", TOKEN_TYPE_INVALID },
};

TEST_FUNC(zig_message_token_to_value,
{
    int count = array_count(to_value_cases);
    int i = 0;

    for (i = 0; i < count; ++i) {
        struct token t;
        t.text = to_value_cases[i].text;
        t.length = strlen(to_value_cases[i].text);

        struct token_value cv = token_to_value(t);
        struct token_value zv;
        yabai_token_to_value(&t, &zv);

        TEST_CHECK(cv.type != to_value_cases[i].expected_type, 0);
        TEST_CHECK(cv.type != zv.type, 0);

        if (cv.type == TOKEN_TYPE_INT) TEST_CHECK(cv.int_value != zv.int_value, 0);
        if (cv.type == TOKEN_TYPE_U32) TEST_CHECK(cv.u32_value != zv.u32_value, 0);
        if (cv.type == TOKEN_TYPE_FLOAT) TEST_CHECK(memcmp(&cv.float_value, &zv.float_value, sizeof(float)), 0);
        if (cv.type == TOKEN_TYPE_STRING) TEST_CHECK(cv.string_value != zv.string_value, 0);
    }
})
