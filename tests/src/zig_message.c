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
void yabai_parse_key_value_pair(char *token, char **key, char **value, bool *exclusion);
uint8_t yabai_parse_value_type(char *type);
uint8_t yabai_parse_resize_handle(char *handle);

struct frame_field
{
    char *ptr;
    int len;
};

int yabai_socket_frame_decode(const char *frame, size_t frame_size, struct frame_field *fields, size_t max_fields, size_t *out_count);

_Static_assert(sizeof(struct frame_field) == 16, "frame_field size drift");

struct yabai_properties
{
    struct token token;
    bool did_parse;
    bool did_error;
    uint64_t flags;
};

bool yabai_parse_property(struct yabai_properties *properties, char *property, const uint64_t *property_val, char **property_str, int property_count);

_Static_assert(sizeof(struct properties) == 32, "C properties size drift");
_Static_assert(sizeof(struct yabai_properties) == 32, "properties size drift");
_Static_assert(offsetof(struct properties, flags) == 24, "C properties.flags drift");

static char *prop_names[] = { "id", "uuid", "index", "label" };
static uint64_t prop_vals[] = { 0x001, 0x002, 0x004, 0x008 };

static int prop_case(char *name, bool expected_found, uint64_t expected_flags)
{
    struct properties c_props;
    struct yabai_properties z_props;
    memset(&c_props, 0, sizeof(c_props));
    memset(&z_props, 0, sizeof(z_props));

    bool c_found = parse_property(&c_props, name, prop_vals, prop_names, array_count(prop_names));
    bool z_found = yabai_parse_property(&z_props, name, prop_vals, prop_names, array_count(prop_names));

    if (c_found != expected_found) return 0;
    if (z_found != expected_found) return 0;
    if (c_props.flags != expected_flags) return 0;
    if (z_props.flags != expected_flags) return 0;
    return 1;
}

TEST_FUNC(zig_message_parse_property,
{
    TEST_CHECK(prop_case("id", true, 0x001), 1);
    TEST_CHECK(prop_case("uuid", true, 0x002), 1);
    TEST_CHECK(prop_case("index", true, 0x004), 1);
    TEST_CHECK(prop_case("label", true, 0x008), 1);
    TEST_CHECK(prop_case("bogus", false, 0), 1);
    TEST_CHECK(prop_case("", false, 0), 1);
    TEST_CHECK(prop_case("ID", false, 0), 1);
    TEST_CHECK(prop_case("idx", false, 0), 1);

    /* Flags accumulate across calls like the real query loop. */
    struct yabai_properties acc;
    memset(&acc, 0, sizeof(acc));
    bool f1 = yabai_parse_property(&acc, "id", prop_vals, prop_names, array_count(prop_names));
    bool f2 = yabai_parse_property(&acc, "index", prop_vals, prop_names, array_count(prop_names));
    bool f3 = yabai_parse_property(&acc, "bogus", prop_vals, prop_names, array_count(prop_names));
    TEST_CHECK(f1 && f2 && !f3, 1);
    TEST_CHECK(acc.flags == 0x005, 1);
})

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

static char *kv_inputs[] = { "app=Terminal", "title!=foo", "space=", "manage", "=floating", "!=", "a!=b=c", "x==", "==", "!", "label=one=two" };

TEST_FUNC(zig_message_key_value,
{
    int count = array_count(kv_inputs);
    int i = 0;
    for (i = 0; i < count; ++i) {
        int len = strlen(kv_inputs[i]);
        char c_buf[64];
        char z_buf[64];
        memcpy(c_buf, kv_inputs[i], len + 1);
        memcpy(z_buf, kv_inputs[i], len + 1);

        char *c_key;
        char *c_value;
        bool c_exc = true;
        char *z_key;
        char *z_value;
        bool z_exc = true;
        parse_key_value_pair(c_buf, &c_key, &c_value, &c_exc);
        yabai_parse_key_value_pair(z_buf, &z_key, &z_value, &z_exc);

        TEST_CHECK((c_key == NULL) != (z_key == NULL), 0);
        if (c_key && z_key) TEST_CHECK(strcmp(c_key, z_key), 0);
        TEST_CHECK((c_value == NULL) != (z_value == NULL), 0);
        if (c_value && z_value) TEST_CHECK(strcmp(c_value, z_value), 0);
        TEST_CHECK(c_exc != z_exc, 0);
        TEST_CHECK(memcmp(c_buf, z_buf, len + 1), 0);
    }

    char hand[] = "app=Terminal";
    char *hkey = NULL;
    char *hval = NULL;
    bool hexc = true;
    yabai_parse_key_value_pair(hand, &hkey, &hval, &hexc);
    TEST_CHECK(hkey == NULL, 0);
    TEST_CHECK(hval == NULL, 0);
    if (hkey) TEST_CHECK(strcmp(hkey, "app"), 0);
    if (hval) TEST_CHECK(strcmp(hval, "Terminal"), 0);
    TEST_CHECK(hexc, false);
})

struct named_const_case
{
    char *name;
    uint8_t expected;
};

static struct named_const_case value_type_cases[] =
{
    { "abs", 0x1 },
    { "rel", 0x2 },
    { "", 0 },
    { "ABS", 0 },
    { "absolute", 0 },
};

static struct named_const_case resize_handle_cases[] =
{
    { "top", 0x01 },
    { "bottom", 0x02 },
    { "left", 0x04 },
    { "right", 0x08 },
    { "top_left", 0x05 },
    { "top_right", 0x09 },
    { "bottom_left", 0x06 },
    { "bottom_right", 0x0A },
    { "abs", 0x10 },
    { "", 0 },
    { "center", 0 },
    { "TOP", 0 },
};

static int frame_decode_parity(char *frame, int total)
{
    char *fields_c[16];
    char *fields_z[16];
    struct frame_field zf[16];
    size_t z_count = 0;

    /* Reference split with the characterization helper. */
    int c_count = socket_frame_decode_checked(frame, total, fields_c, 16);
    int z_rc = yabai_socket_frame_decode(frame, total, zf, 16, &z_count);

    if ((c_count < 0) != (z_rc != 0)) return 0;
    if (c_count < 0) return 1;
    if (z_count != (size_t) c_count) return 0;

    int i = 0;
    for (i = 0; i < c_count; ++i) {
        fields_z[i] = zf[i].ptr;
        if (zf[i].len != (int) strlen(fields_c[i])) return 0;
        if (memcmp(fields_z[i], fields_c[i], zf[i].len) != 0) return 0;
    }
    return 1;
}

TEST_FUNC(zig_message_frame_decode,
{
    char *argv[3];
    argv[0] = "yabai";
    argv[1] = "query";
    argv[2] = "--windows";
    char frame[256];
    int total = socket_frame_encode(3, argv, frame, sizeof(frame));
    TEST_CHECK(frame_decode_parity(frame, total), 1);

    /* Hand-pinned valid decode: 2 args + trailing empty field. */
    struct frame_field zf[16];
    memset(zf, 0, sizeof(zf));
    size_t z_count = 0;
    int rc = yabai_socket_frame_decode(frame, total, zf, 16, &z_count);
    TEST_CHECK(rc, 0);
    TEST_CHECK(z_count, 3);
    TEST_CHECK(zf[0].ptr == NULL, 0);
    TEST_CHECK(zf[1].ptr == NULL, 0);
    if (zf[0].ptr) TEST_CHECK(strcmp(zf[0].ptr, "query"), 0);
    if (zf[1].ptr) TEST_CHECK(strcmp(zf[1].ptr, "--windows"), 0);
    TEST_CHECK(zf[2].len, 0);

    /* Truncated prefix. */
    TEST_CHECK(yabai_socket_frame_decode(frame, 3, zf, 16, &z_count), -1);
    /* Truncated payload. */
    TEST_CHECK(yabai_socket_frame_decode(frame, total - 1, zf, 16, &z_count), -1);
    /* Trailing garbage. */
    TEST_CHECK(yabai_socket_frame_decode(frame, total + 1, zf, 16, &z_count), -1);

    /* Zero declared length. */
    char zero[8];
    memset(zero, 0, sizeof(zero));
    TEST_CHECK(yabai_socket_frame_decode(zero, sizeof(zero), zf, 16, &z_count), -1);
    TEST_CHECK(frame_decode_parity(zero, sizeof(zero)), 1);

    /* Negative declared length. */
    char neg[4];
    int neg_len = -5;
    memcpy(neg, &neg_len, sizeof(int));
    TEST_CHECK(yabai_socket_frame_decode(neg, sizeof(neg), zf, 16, &z_count), -1);
    TEST_CHECK(frame_decode_parity(neg, sizeof(neg)), 1);

    /* Absurd declared length rejected without allocating. */
    char huge[4];
    int huge_len = 0x7fffffff;
    memcpy(huge, &huge_len, sizeof(int));
    TEST_CHECK(yabai_socket_frame_decode(huge, sizeof(huge), zf, 16, &z_count), -1);
    TEST_CHECK(frame_decode_parity(huge, sizeof(huge)), 1);

    /* Missing trailing NUL. */
    char unterminated[256];
    memcpy(unterminated, frame, total);
    unterminated[total - 1] = 'x';
    TEST_CHECK(yabai_socket_frame_decode(unterminated, total, zf, 16, &z_count), -1);
    TEST_CHECK(frame_decode_parity(unterminated, total), 1);

    /* Field buffer too small. */
    TEST_CHECK(yabai_socket_frame_decode(frame, total, zf, 1, &z_count), -1);
})

TEST_FUNC(zig_message_named_consts,
{
    int count = array_count(value_type_cases);
    int i = 0;
    for (i = 0; i < count; ++i) {
        uint8_t c = parse_value_type(value_type_cases[i].name);
        uint8_t z = yabai_parse_value_type(value_type_cases[i].name);
        TEST_CHECK(c != value_type_cases[i].expected, 0);
        TEST_CHECK(c != z, 0);
    }

    count = array_count(resize_handle_cases);
    for (i = 0; i < count; ++i) {
        uint8_t c = parse_resize_handle(resize_handle_cases[i].name);
        uint8_t z = yabai_parse_resize_handle(resize_handle_cases[i].name);
        TEST_CHECK(c != resize_handle_cases[i].expected, 0);
        TEST_CHECK(c != z, 0);
    }
})
