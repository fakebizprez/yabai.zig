/* Message tokenizer parity tests (doc/zig-refactor-plan.md step 3).
 *
 * The Zig exports in zig/message.zig must match the token helpers in
 * src/message.c exactly. Layout contract: src/zig_abi.h pins
 * struct token/token_value/properties/selector against zig/abi.zig.
 */

static char *prop_names[] = { "id", "uuid", "index", "label" };
static uint64_t prop_vals[] = { 0x001, 0x002, 0x004, 0x008 };

static int prop_case(char *name, bool expected_found, uint64_t expected_flags)
{
    struct properties c_props;
    struct properties z_props;
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
    struct properties acc;
    memset(&acc, 0, sizeof(acc));
    bool f1 = yabai_parse_property(&acc, "id", prop_vals, prop_names, array_count(prop_names));
    bool f2 = yabai_parse_property(&acc, "index", prop_vals, prop_names, array_count(prop_names));
    bool f3 = yabai_parse_property(&acc, "bogus", prop_vals, prop_names, array_count(prop_names));
    TEST_CHECK(f1 && f2 && !f3, 1);
    TEST_CHECK(acc.flags == 0x005, 1);
})

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
    { "abs", TYPE_ABS },
    { "rel", TYPE_REL },
    { "", 0 },
    { "ABS", 0 },
    { "absolute", 0 },
};

static struct named_const_case resize_handle_cases[] =
{
    { "top", HANDLE_TOP },
    { "bottom", HANDLE_BOTTOM },
    { "left", HANDLE_LEFT },
    { "right", HANDLE_RIGHT },
    { "top_left", HANDLE_TOP | HANDLE_LEFT },
    { "top_right", HANDLE_TOP | HANDLE_RIGHT },
    { "bottom_left", HANDLE_BOTTOM | HANDLE_LEFT },
    { "bottom_right", HANDLE_BOTTOM | HANDLE_RIGHT },
    { "abs", HANDLE_ABS },
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

/* C-vs-Zig parity for parse_properties() (src/message.c): struct fields,
 * in-place comma splitting, and byte-identical daemon_fail emission
 * captured via open_memstream. Must FAIL against the stubbed Zig export
 * (TDD red) and pass on the real one (green). */
static int props_parity_case(char *input)
{
    char c_buf[128];
    char z_buf[128];
    size_t len = strlen(input);
    if (len + 1 > sizeof(c_buf)) return 0;
    memcpy(c_buf, input, len + 1);
    memcpy(z_buf, input, len + 1);

    struct token c_tok = { c_buf, (int) len };
    struct token z_tok = { z_buf, (int) len };

    char *c_msg = NULL;
    char *z_msg = NULL;
    size_t c_mlen = 0;
    size_t z_mlen = 0;
    FILE *c_rsp = open_memstream(&c_msg, &c_mlen);
    FILE *z_rsp = open_memstream(&z_msg, &z_mlen);
    if (!c_rsp || !z_rsp) return 0;

    struct properties c_props = parse_properties(c_rsp, c_tok, prop_vals, prop_names, array_count(prop_names));
    struct properties z_props;
    memset(&z_props, 0, sizeof(z_props));
    yabai_parse_properties(z_rsp, &z_tok, prop_vals, prop_names, array_count(prop_names), &z_props);

    fflush(c_rsp);
    fflush(z_rsp);
    fclose(c_rsp);
    fclose(z_rsp);

    int ok = 1;
    if (c_props.did_parse != z_props.did_parse) ok = 0;
    if (c_props.did_error != z_props.did_error) ok = 0;
    if (c_props.flags != z_props.flags) ok = 0;
    if (c_props.token.length != z_props.token.length) ok = 0;
    if (memcmp(c_buf, z_buf, len + 1) != 0) ok = 0;
    if (c_mlen != z_mlen) ok = 0;
    else if (c_mlen > 0 && memcmp(c_msg, z_msg, c_mlen) != 0) ok = 0;

    free(c_msg);
    free(z_msg);
    return ok;
}

TEST_FUNC(zig_message_parse_properties,
{
    TEST_CHECK(props_parity_case("id"), 1);
    TEST_CHECK(props_parity_case("id,index"), 1);
    TEST_CHECK(props_parity_case("id,bogus"), 1);
    TEST_CHECK(props_parity_case("bogus"), 1);
    TEST_CHECK(props_parity_case("bogus,id"), 1);
    TEST_CHECK(props_parity_case(""), 1);
    TEST_CHECK(props_parity_case("--query"), 1);
    TEST_CHECK(props_parity_case("--"), 1);
    TEST_CHECK(props_parity_case("id,"), 1);
    TEST_CHECK(props_parity_case(",id"), 1);
    TEST_CHECK(props_parity_case("id,,index"), 1);
    TEST_CHECK(props_parity_case("label,uuid,zzz,label"), 1);
    TEST_CHECK(props_parity_case("ID"), 1);

    /* NULL stream: same struct result as C, no emission, no crash. */
    char c_null[64];
    char z_null[64];
    memcpy(c_null, "id,bogus", 9);
    memcpy(z_null, "id,bogus", 9);
    struct token c_ntok;
    c_ntok.text = c_null;
    c_ntok.length = 8;
    struct token z_ntok;
    z_ntok.text = z_null;
    z_ntok.length = 8;
    struct properties c_np = parse_properties(NULL, c_ntok, prop_vals, prop_names, array_count(prop_names));
    struct properties z_np;
    memset(&z_np, 0, sizeof(z_np));
    yabai_parse_properties(NULL, &z_ntok, prop_vals, prop_names, array_count(prop_names), &z_np);
    TEST_CHECK(c_np.did_parse != z_np.did_parse, 0);
    TEST_CHECK(c_np.did_error != z_np.did_error, 0);
    TEST_CHECK(c_np.flags != z_np.flags, 0);
    TEST_CHECK(memcmp(c_null, z_null, 9), 0);

    /* Hand-pinned error bytes: 0x07 failure marker plus formatting. */
    char hbuf[64];
    memcpy(hbuf, "bogus", 6);
    struct token htok;
    htok.text = hbuf;
    htok.length = 5;
    char *hmsg = NULL;
    size_t hlen = 0;
    FILE *hrsp = open_memstream(&hmsg, &hlen);
    struct properties hz;
    memset(&hz, 0, sizeof(hz));
    yabai_parse_properties(hrsp, &htok, prop_vals, prop_names, array_count(prop_names), &hz);
    fflush(hrsp);
    fclose(hrsp);
    TEST_CHECK(hz.did_parse, true);
    TEST_CHECK(hz.did_error, true);
    TEST_CHECK(hz.flags, 0);
    TEST_CHECK(hlen, strlen("\x07'bogus' is not a valid property.\n"));
    if (hlen > 0) TEST_CHECK(memcmp(hmsg, "\x07'bogus' is not a valid property.\n", hlen), 0);
    free(hmsg);
})

/* C-vs-Zig parity for parse_label() (src/message.c): return value,
 * malloc'd label bytes plus NUL, untouched label on error paths, and
 * byte-identical daemon_fail emission captured via open_memstream.
 * Must FAIL against the stubbed Zig export (TDD red) and pass on the
 * real one (green). Every allocated label is freed by the caller. */
static int label_parity_n(char *input, int length, int type)
{
    char c_buf[128];
    char z_buf[128];
    if (length < 0 || length + 1 > (int) sizeof(c_buf)) return 0;
    memcpy(c_buf, input, length + 1);
    memcpy(z_buf, input, length + 1);

    struct token c_tok = { c_buf, length };
    struct token z_tok = { z_buf, length };

    char *c_msg = NULL;
    char *z_msg = NULL;
    size_t c_mlen = 0;
    size_t z_mlen = 0;
    FILE *c_rsp = open_memstream(&c_msg, &c_mlen);
    FILE *z_rsp = open_memstream(&z_msg, &z_mlen);
    if (!c_rsp || !z_rsp) return 0;

    char *c_label = NULL;
    char *z_label = NULL;
    bool c_ok = parse_label(c_rsp, c_tok, type, &c_label);
    bool z_ok = yabai_parse_label(z_rsp, &z_tok, type, &z_label);

    fflush(c_rsp);
    fflush(z_rsp);
    fclose(c_rsp);
    fclose(z_rsp);

    int ok = 1;
    if (c_ok != z_ok) ok = 0;
    if ((c_label == NULL) != (z_label == NULL)) ok = 0;
    if (c_label && z_label) {
        if (strcmp(c_label, z_label) != 0) ok = 0;
        if (c_label[length] != '\0' || z_label[length] != '\0') ok = 0;
        if (memcmp(c_label, z_label, length + 1) != 0) ok = 0;
    }
    if (c_mlen != z_mlen) ok = 0;
    else if (c_mlen > 0 && memcmp(c_msg, z_msg, c_mlen) != 0) ok = 0;

    free(c_label);
    free(z_label);
    free(c_msg);
    free(z_msg);
    return ok;
}

static int label_parity_case(char *input, int type)
{
    return label_parity_n(input, strlen(input), type);
}

static char *label_reserved_display[] = { "north", "east", "south", "west", "prev", "next", "first", "last", "recent", "mouse" };
static char *label_reserved_space[] = { "prev", "next", "first", "last", "recent", "mouse" };
static char *label_reserved_window[] = { "float", "sticky", "shadow", "split", "zoom-parent", "zoom-fullscreen", "windowed-fullscreen", "native-fullscreen", "expose", "pip", "recover" };

struct label_hand_case
{
    char *input;
    int type;
    char *expected;
};

static struct label_hand_case label_hand_cases[] =
{
    { "42", LABEL_DISPLAY, "\x07'42' cannot be used as a label.\n" },
    { "prev", LABEL_SPACE, "\x07'prev' is a reserved keyword and cannot be used as a label.\n" },
    { "north", LABEL_DISPLAY, "\x07'north' is a reserved keyword and cannot be used as a label.\n" },
    { "float", LABEL_WINDOW, "\x07'float' is a reserved keyword and cannot be used as a scratchpad.\n" },
};

TEST_FUNC(zig_message_parse_label,
{
    int i = 0;

    /* Invalid tokens yield NULL with success and no emission. */
    TEST_CHECK(label_parity_case("", LABEL_DISPLAY), 1);
    TEST_CHECK(label_parity_case("", LABEL_SPACE), 1);
    TEST_CHECK(label_parity_case("", LABEL_WINDOW), 1);

    /* Non-string tokens fail with the label emission. */
    TEST_CHECK(label_parity_case("42", LABEL_DISPLAY), 1);
    TEST_CHECK(label_parity_case("0x2A", LABEL_SPACE), 1);
    TEST_CHECK(label_parity_case("4.5", LABEL_WINDOW), 1);

    /* Every reserved keyword fails for its own label type. */
    for (i = 0; i < array_count(label_reserved_display); ++i) {
        TEST_CHECK(label_parity_case(label_reserved_display[i], LABEL_DISPLAY), 1);
    }
    for (i = 0; i < array_count(label_reserved_space); ++i) {
        TEST_CHECK(label_parity_case(label_reserved_space[i], LABEL_SPACE), 1);
    }
    for (i = 0; i < array_count(label_reserved_window); ++i) {
        TEST_CHECK(label_parity_case(label_reserved_window[i], LABEL_WINDOW), 1);
    }

    /* Reserved for one type is a valid label for another. */
    TEST_CHECK(label_parity_case("north", LABEL_SPACE), 1);
    TEST_CHECK(label_parity_case("north", LABEL_WINDOW), 1);
    TEST_CHECK(label_parity_case("float", LABEL_DISPLAY), 1);
    TEST_CHECK(label_parity_case("float", LABEL_SPACE), 1);
    TEST_CHECK(label_parity_case("prev", LABEL_WINDOW), 1);

    /* Valid labels incl. malloc'd bytes and NUL. */
    TEST_CHECK(label_parity_case("my-label_01", LABEL_DISPLAY), 1);
    TEST_CHECK(label_parity_case("my-label_01", LABEL_SPACE), 1);
    TEST_CHECK(label_parity_case("my-label_01", LABEL_WINDOW), 1);
    TEST_CHECK(label_parity_case("a", LABEL_DISPLAY), 1);

    /* Length-bounded segment: trailing bytes past the token are
     * neither matched nor emitted. */
    TEST_CHECK(label_parity_n("prevTRAILING", 4, LABEL_SPACE), 1);
    TEST_CHECK(label_parity_n("42TRAILING", 2, LABEL_DISPLAY), 1);

    /* NULL stream: same result as C, no emission, no crash. */
    char c_null[64];
    char z_null[64];
    memcpy(c_null, "prev", 5);
    memcpy(z_null, "prev", 5);
    struct token c_ntok;
    c_ntok.text = c_null;
    c_ntok.length = 4;
    struct token z_ntok;
    z_ntok.text = z_null;
    z_ntok.length = 4;
    char *c_nlab = NULL;
    char *z_nlab = NULL;
    bool c_nok = parse_label(NULL, c_ntok, LABEL_SPACE, &c_nlab);
    bool z_nok = yabai_parse_label(NULL, &z_ntok, LABEL_SPACE, &z_nlab);
    TEST_CHECK(c_nok != z_nok, 0);
    TEST_CHECK((c_nlab == NULL) != (z_nlab == NULL), 0);
    free(c_nlab);
    free(z_nlab);

    memcpy(c_null, "fine", 5);
    memcpy(z_null, "fine", 5);
    c_ntok.length = 4;
    z_ntok.length = 4;
    c_nlab = NULL;
    z_nlab = NULL;
    c_nok = parse_label(NULL, c_ntok, LABEL_DISPLAY, &c_nlab);
    z_nok = yabai_parse_label(NULL, &z_ntok, LABEL_DISPLAY, &z_nlab);
    TEST_CHECK(c_nok != z_nok, 0);
    TEST_CHECK((c_nlab == NULL) != (z_nlab == NULL), 0);
    if (c_nlab && z_nlab) TEST_CHECK(strcmp(c_nlab, z_nlab), 0);
    free(c_nlab);
    free(z_nlab);

    /* Hand-pinned error bytes, Zig side only. */
    for (i = 0; i < array_count(label_hand_cases); ++i) {
        char hbuf[64];
        size_t hlen_in = strlen(label_hand_cases[i].input);
        memcpy(hbuf, label_hand_cases[i].input, hlen_in + 1);
        struct token htok;
        htok.text = hbuf;
        htok.length = hlen_in;
        char *hmsg = NULL;
        size_t hlen = 0;
        FILE *hrsp = open_memstream(&hmsg, &hlen);
        char *hlab = NULL;
        bool hok = yabai_parse_label(hrsp, &htok, label_hand_cases[i].type, &hlab);
        fflush(hrsp);
        fclose(hrsp);
        TEST_CHECK(hok, false);
        TEST_CHECK(hlab == NULL, true);
        TEST_CHECK(hlen, strlen(label_hand_cases[i].expected));
        if (hlen > 0) TEST_CHECK(memcmp(hmsg, label_hand_cases[i].expected, hlen), 0);
        free(hmsg);
    }

    /* Hand-pinned valid label: malloc'd copy with NUL. */
    char vbuf[64];
    memcpy(vbuf, "desk-1", 7);
    struct token vtok;
    vtok.text = vbuf;
    vtok.length = 6;
    char *vlab = NULL;
    TEST_CHECK(yabai_parse_label(NULL, &vtok, LABEL_DISPLAY, &vlab), true);
    TEST_CHECK(vlab == NULL, false);
    if (vlab) {
        TEST_CHECK(strcmp(vlab, "desk-1"), 0);
        TEST_CHECK(vlab[6], '\0');
    }
    free(vlab);
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

/* C-vs-Zig parity for parse_insert_selector() (src/message.c:1130):
 * struct fields, message-cursor advance, and byte-identical
 * daemon_fail emission captured via open_memstream. Must FAIL against
 * the stubbed Zig export (TDD red) and pass on the real one (green). */
static int insert_parity_case(char *input)
{
    char c_buf[128];
    char z_buf[128];
    size_t len = strlen(input);
    if (len + 2 > sizeof(c_buf)) return 0;
    memcpy(c_buf, input, len + 1);
    c_buf[len + 1] = '\0';
    memcpy(z_buf, input, len + 1);
    z_buf[len + 1] = '\0';

    char *c_cur = c_buf;
    char *z_cur = z_buf;

    char *c_msg = NULL;
    char *z_msg = NULL;
    size_t c_mlen = 0;
    size_t z_mlen = 0;
    FILE *c_rsp = open_memstream(&c_msg, &c_mlen);
    FILE *z_rsp = open_memstream(&z_msg, &z_mlen);
    if (!c_rsp || !z_rsp) return 0;

    struct selector c_sel = parse_insert_selector(c_rsp, &c_cur);
    struct yabai_selector z_sel;
    memset(&z_sel, 0, sizeof(z_sel));
    yabai_parse_insert_selector(z_rsp, &z_cur, &z_sel);

    fflush(c_rsp);
    fflush(z_rsp);
    fclose(c_rsp);
    fclose(z_rsp);

    int ok = 1;
    if (c_sel.did_parse != z_sel.did_parse) ok = 0;
    if (c_sel.token.length != z_sel.token.length) ok = 0;
    if (c_sel.token.length > 0 && c_sel.token.length == z_sel.token.length) {
        if (!c_sel.token.text || !z_sel.token.text) ok = 0;
        else if (memcmp(c_sel.token.text, z_sel.token.text, c_sel.token.length) != 0) ok = 0;
    }
    if ((c_cur - c_buf) != (z_cur - z_buf)) ok = 0;
    if (c_sel.did_parse) {
        if (c_sel.dir != (int) z_sel.payload) ok = 0;
        if ((z_sel.payload >> 32) != 0) ok = 0;
    } else {
        if (z_sel.payload != 0) ok = 0;
    }
    if (c_mlen != z_mlen) ok = 0;
    else if (c_mlen > 0 && memcmp(c_msg, z_msg, c_mlen) != 0) ok = 0;

    free(c_msg);
    free(z_msg);
    return ok;
}

TEST_FUNC(zig_message_parse_insert_selector,
{
    TEST_CHECK(insert_parity_case("north"), 1);
    TEST_CHECK(insert_parity_case("east"), 1);
    TEST_CHECK(insert_parity_case("south"), 1);
    TEST_CHECK(insert_parity_case("west"), 1);
    TEST_CHECK(insert_parity_case("stack"), 1);

    TEST_CHECK(insert_parity_case("bogus"), 1);
    TEST_CHECK(insert_parity_case("NORTH"), 1);
    TEST_CHECK(insert_parity_case("northx"), 1);
    TEST_CHECK(insert_parity_case("stack."), 1);
    TEST_CHECK(insert_parity_case("0"), 1);
    TEST_CHECK(insert_parity_case(""), 1);

    /* NULL stream: same struct result as C, no emission, no crash. */
    char c_null[64];
    char z_null[64];
    memcpy(c_null, "north", 6);
    c_null[6] = '\0';
    memcpy(z_null, "north", 6);
    z_null[6] = '\0';
    char *c_ncur = c_null;
    char *z_ncur = z_null;
    struct selector c_np = parse_insert_selector(NULL, &c_ncur);
    struct yabai_selector z_np;
    memset(&z_np, 0, sizeof(z_np));
    yabai_parse_insert_selector(NULL, &z_ncur, &z_np);
    TEST_CHECK(c_np.did_parse != z_np.did_parse, 0);
    TEST_CHECK(c_np.dir != (int) z_np.payload, 0);
    TEST_CHECK((c_ncur - c_null) != (z_ncur - z_null), 0);

    memcpy(c_null, "bogus", 6);
    c_null[6] = '\0';
    memcpy(z_null, "bogus", 6);
    z_null[6] = '\0';
    c_ncur = c_null;
    z_ncur = z_null;
    c_np = parse_insert_selector(NULL, &c_ncur);
    memset(&z_np, 0, sizeof(z_np));
    yabai_parse_insert_selector(NULL, &z_ncur, &z_np);
    TEST_CHECK(c_np.did_parse != z_np.did_parse, 0);
    TEST_CHECK(z_np.payload, 0);
    TEST_CHECK((c_ncur - c_null) != (z_ncur - z_null), 0);

    /* Hand-pinned dir values. */
    char vbuf[64];
    memcpy(vbuf, "north", 6);
    vbuf[6] = '\0';
    char *vcur = vbuf;
    struct yabai_selector vsel;
    memset(&vsel, 0, sizeof(vsel));
    yabai_parse_insert_selector(NULL, &vcur, &vsel);
    TEST_CHECK(vsel.did_parse, true);
    TEST_CHECK(vsel.payload, (uint64_t) DIR_NORTH);

    memcpy(vbuf, "stack", 6);
    vbuf[6] = '\0';
    vcur = vbuf;
    memset(&vsel, 0, sizeof(vsel));
    yabai_parse_insert_selector(NULL, &vcur, &vsel);
    TEST_CHECK(vsel.did_parse, true);
    TEST_CHECK(vsel.payload, (uint64_t) STACK);

    /* Hand-pinned error bytes, Zig side only. */
    char hbuf[64];
    memcpy(hbuf, "bogus", 6);
    hbuf[6] = '\0';
    char *hcur = hbuf;
    char *hmsg = NULL;
    size_t hlen = 0;
    FILE *hrsp = open_memstream(&hmsg, &hlen);
    struct yabai_selector hz;
    memset(&hz, 0, sizeof(hz));
    yabai_parse_insert_selector(hrsp, &hcur, &hz);
    fflush(hrsp);
    fclose(hrsp);
    TEST_CHECK(hz.did_parse, false);
    TEST_CHECK(hz.payload, 0);
    TEST_CHECK(hlen, strlen("\x07value 'bogus' is not a valid option for DIR_SEL\n"));
    if (hlen > 0) TEST_CHECK(memcmp(hmsg, "\x07value 'bogus' is not a valid option for DIR_SEL\n", hlen), 0);
    free(hmsg);

    memcpy(hbuf, "", 1);
    hbuf[1] = '\0';
    hcur = hbuf;
    hmsg = NULL;
    hlen = 0;
    hrsp = open_memstream(&hmsg, &hlen);
    memset(&hz, 0, sizeof(hz));
    yabai_parse_insert_selector(hrsp, &hcur, &hz);
    fflush(hrsp);
    fclose(hrsp);
    TEST_CHECK(hz.did_parse, false);
    TEST_CHECK(hz.payload, 0);
    TEST_CHECK(hlen, strlen("\x07value '' is not a valid option for DIR_SEL\n"));
    if (hlen > 0) TEST_CHECK(memcmp(hmsg, "\x07value '' is not a valid option for DIR_SEL\n", hlen), 0);
    free(hmsg);
})
