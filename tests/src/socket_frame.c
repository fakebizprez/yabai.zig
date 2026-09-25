/* Socket wire-format characterization for the Zig migration.
 *
 * The encoder below mirrors client_send_message() in src/yabai.c byte for
 * byte: a native int byte length, followed by NUL-separated arguments and
 * a final NUL. The daemon side (DAEMON_MESSAGE handler in src/event_loop.c)
 * reads the int prefix and then exactly that many payload bytes.
 *
 * socket_frame_decode_checked() is the contract a Zig replacement decoder
 * must satisfy: validate lengths before allocating or parsing, while
 * accepting every valid frame unchanged.
 */

#define SOCKET_FRAME_MAX (1024 * 1024)

static int socket_frame_encode(int argc, char **argv, char *out, int out_size)
{
    int message_length = argc;
    for (int i = 1; i < argc; ++i) message_length += strlen(argv[i]);
    if (sizeof(int) + message_length > out_size) return -1;

    memcpy(out, &message_length, sizeof(int));
    char *temp = out + sizeof(int);
    for (int i = 1; i < argc; ++i) {
        size_t len = strlen(argv[i]);
        memcpy(temp, argv[i], len);
        temp += len;
        *temp++ = '\0';
    }
    *temp++ = '\0';

    return sizeof(int) + message_length;
}

/* Returns the field count (including the trailing empty field) on success,
 * or -1 when the frame is malformed. Never allocates. */
static int socket_frame_decode_checked(char *frame, int frame_size, char **fields, int max_fields)
{
    if (frame_size < (int) sizeof(int)) return -1;

    int declared;
    memcpy(&declared, frame, sizeof(int));
    if (declared <= 0 || declared > SOCKET_FRAME_MAX) return -1;
    if (frame_size != (int) sizeof(int) + declared) return -1;

    char *payload = frame + sizeof(int);
    if (payload[declared - 1] != '\0') return -1;

    int count = 0;
    char *p = payload;
    while (p < payload + declared) {
        if (count >= max_fields) return -1;
        fields[count++] = p;
        p += strlen(p) + 1;
    }

    if (count < 1 || fields[count - 1][0] != '\0') return -1;
    return count;
}

static const char expected_two_arg_payload[] = { 'a', '\0', 'b', 'b', '\0', '\0' };

TEST_FUNC(socket_frame_valid_args,
{
    char *argv[3];
    argv[0] = "yabai";
    argv[1] = "query";
    argv[2] = "--windows";
    char frame[256];
    char *fields[16];

    int total = socket_frame_encode(3, argv, frame, sizeof(frame));
    /* argc(3) + "query"(5) + "--windows"(9) = 17 payload bytes */
    TEST_CHECK(total, (int) sizeof(int) + 17);

    int declared;
    memcpy(&declared, frame, sizeof(int));
    TEST_CHECK(declared, 17);

    int count = socket_frame_decode_checked(frame, total, fields, 16);
    TEST_CHECK(count, 3);
    TEST_CHECK(strcmp(fields[0], "query"), 0);
    TEST_CHECK(strcmp(fields[1], "--windows"), 0);
    TEST_CHECK(fields[2][0], '\0');
})

TEST_FUNC(socket_frame_byte_exact,
{
    char *argv[3];
    argv[0] = "yabai";
    argv[1] = "a";
    argv[2] = "bb";
    char frame[256];
    char *fields[16];

    int total = socket_frame_encode(3, argv, frame, sizeof(frame));
    /* argc(3) + 1 + 2 = 6 payload bytes: "a\0bb\0\0" */
    TEST_CHECK(total, (int) sizeof(int) + 6);

    TEST_CHECK(memcmp(frame + sizeof(int), expected_two_arg_payload, sizeof(expected_two_arg_payload)), 0);

    int count = socket_frame_decode_checked(frame, total, fields, 16);
    TEST_CHECK(count, 3);
    TEST_CHECK(strcmp(fields[0], "a"), 0);
    TEST_CHECK(strcmp(fields[1], "bb"), 0);
})

TEST_FUNC(socket_frame_malformed,
{
    char *argv[3];
    argv[0] = "yabai";
    argv[1] = "query";
    argv[2] = "--windows";
    char frame[256];
    char *fields[16];

    int total = socket_frame_encode(3, argv, frame, sizeof(frame));

    /* Truncated length prefix. */
    TEST_CHECK(socket_frame_decode_checked(frame, (int) sizeof(int) - 1, fields, 16), -1);
    /* Truncated payload. */
    TEST_CHECK(socket_frame_decode_checked(frame, total - 1, fields, 16), -1);
    /* Trailing bytes beyond the declared length. */
    TEST_CHECK(socket_frame_decode_checked(frame, total + 1, fields, 16), -1);

    /* Declared length of zero. */
    char zero[sizeof(int) + 1];
    memset(zero, 0, sizeof(zero));
    TEST_CHECK(socket_frame_decode_checked(zero, sizeof(zero), fields, 16), -1);

    /* Negative declared length. */
    char neg[sizeof(int)];
    int neg_len = -5;
    memcpy(neg, &neg_len, sizeof(int));
    TEST_CHECK(socket_frame_decode_checked(neg, sizeof(neg), fields, 16), -1);

    /* Absurd declared length rejected without allocating. */
    char huge[sizeof(int)];
    int huge_len = 0x7fffffff;
    memcpy(huge, &huge_len, sizeof(int));
    TEST_CHECK(socket_frame_decode_checked(huge, sizeof(huge), fields, 16), -1);

    /* Payload not NUL-terminated at the end. */
    char unterminated[256];
    memcpy(unterminated, frame, total);
    unterminated[total - 1] = 'x';
    TEST_CHECK(socket_frame_decode_checked(unterminated, total, fields, 16), -1);
})

/* Feed a wire-format payload through the real handle_message in
 * src/message.c and capture the reply via memstream. */
static void socket_frame_handle_live(char **tokens, int count, char **out_buf, size_t *out_len)
{
    char *argv[16];
    char frame[2048];
    argv[0] = "yabai";
    for (int i = 0; i < count; ++i) argv[i + 1] = tokens[i];
    int total = socket_frame_encode(count + 1, argv, frame, sizeof(frame));

    char *message = malloc(total - sizeof(int));
    memcpy(message, frame + sizeof(int), total - sizeof(int));

    FILE *rsp = open_memstream(out_buf, out_len);
    handle_message(rsp, message);
    fflush(rsp);
    fclose(rsp);
    free(message);
}

TEST_FUNC(socket_frame_unknown_domain_fails,
{
    char *tokens[1];
    tokens[0] = "nosuchdomain";
    char *buf = NULL;
    size_t len = 0;
    socket_frame_handle_live(tokens, 1, &buf, &len);

    TEST_CHECK((len > 0), true);
    TEST_CHECK(buf[0], '\x07');
    free(buf);
})

TEST_FUNC(socket_frame_unknown_command_fails,
{
    char *tokens[2];
    tokens[0] = "query";
    tokens[1] = "--bogus";
    char *buf = NULL;
    size_t len = 0;
    socket_frame_handle_live(tokens, 2, &buf, &len);

    TEST_CHECK((len > 0), true);
    TEST_CHECK(buf[0], '\x07');
    free(buf);
})

TEST_FUNC(socket_frame_rule_list_ok,
{
    /* Empty rule set serializes as "[]\n" with no failure prefix. */
    char *tokens[2];
    tokens[0] = "rule";
    tokens[1] = "--list";
    char *buf = NULL;
    size_t len = 0;
    socket_frame_handle_live(tokens, 2, &buf, &len);

    TEST_CHECK((len > 0), true);
    TEST_CHECK((buf[0] == '\x07'), false);
    TEST_CHECK(strcmp(buf, "[]\n"), 0);
    free(buf);
})

TEST_FUNC(socket_frame_signal_list_ok,
{
    /* Empty signal set serializes as a JSON list with no failure prefix. */
    char *tokens[2];
    tokens[0] = "signal";
    tokens[1] = "--list";
    char *buf = NULL;
    size_t len = 0;
    socket_frame_handle_live(tokens, 2, &buf, &len);

    TEST_CHECK((len > 0), true);
    TEST_CHECK((buf[0] == '\x07'), false);
    TEST_CHECK(buf[0], '[');
    free(buf);
})

TEST_FUNC(socket_frame_failure_marker,
{
    /* The daemon prefixes error responses with FAILURE_MESSAGE
     * (src/misc/macros.h); the CLI routes those to stderr and exits
     * non-zero (client_send_message in src/yabai.c). */
    TEST_CHECK(FAILURE_MESSAGE[0], '\x07');
})
