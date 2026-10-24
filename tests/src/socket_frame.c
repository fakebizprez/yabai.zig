/* Socket wire format, exercised through production code only: frames come
 * from socket_frame_encode() (the client send path in src/yabai-zig.c) and go
 * over a socketpair into daemon_read_message() / the DAEMON_MESSAGE
 * handler in src/event_loop.c. A rejected frame is closed without a reply
 * and handle_message is never called, so the client sees 0 bytes.
 */

/* Reference splitter for the Zig decoder parity test (zig_message.c). */
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

static void socket_frame_ensure_ts(void)
{
    static bool ts_ready = false;
    if (!ts_ready) {
        ts_init(MEGABYTES(8));
        ts_ready = true;
    }
    ts_reset();
}

struct socket_frame_writer
{
    int fd;
    const char *data;
    size_t size;
};

/* Client side on its own thread so frames larger than the socket buffer
 * can be written while the daemon side reads. */
static void *socket_frame_write_all(void *context)
{
    struct socket_frame_writer *writer = context;
    size_t sent = 0;
    while (sent < writer->size) {
        ssize_t n = write(writer->fd, writer->data + sent, writer->size - sent);
        if (n <= 0) break;
        sent += n;
    }
    shutdown(writer->fd, SHUT_WR);
    return NULL;
}

/* Runs daemon_read_message on the daemon end of a socketpair. */
static char *socket_frame_read(const char *data, size_t size)
{
    socket_frame_ensure_ts();

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return NULL;
    int on = 1;
    setsockopt(sv[0], SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));

    pthread_t thread;
    struct socket_frame_writer writer = { sv[0], data, size };
    pthread_create(&thread, NULL, &socket_frame_write_all, &writer);
    char *message = daemon_read_message(sv[1]);

    /* Closing first unblocks a writer whose frame was rejected early. */
    close(sv[1]);
    pthread_join(thread, NULL);
    close(sv[0]);
    return message;
}

/* Runs the whole DAEMON_MESSAGE handler and returns what it wrote back
 * (0 bytes = closed without handle_message). *ts_used is the temporary
 * storage the handler consumed. */
static int socket_frame_daemon(const void *data, size_t size, char *rsp, int rsp_size, uint64_t *ts_used)
{
    socket_frame_ensure_ts();

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return -1;
    if (write(sv[0], data, size) != (ssize_t) size) return -1;
    shutdown(sv[0], SHUT_WR);

    EVENT_HANDLER_DAEMON_MESSAGE(NULL, sv[1]);
    *ts_used = g_temp_storage.used;

    int total = 0;
    ssize_t n;
    while (total < rsp_size - 1 && (n = read(sv[0], rsp + total, rsp_size - 1 - total)) > 0) total += n;
    rsp[total] = '\0';
    close(sv[0]);
    return total;
}

static int socket_frame_daemon_header(int declared, const char *payload, int payload_size, char *rsp, int rsp_size, uint64_t *ts_used)
{
    char frame[256];
    memcpy(frame, &declared, sizeof(int));
    memcpy(frame + sizeof(int), payload, payload_size);
    return socket_frame_daemon(frame, sizeof(int) + payload_size, rsp, rsp_size, ts_used);
}

static const char expected_two_arg_payload[] = { 'a', '\0', 'b', 'b', '\0', '\0' };

TEST_FUNC(socket_frame_valid_args,
{
    char *argv[3];
    argv[0] = "-m";
    argv[1] = "query";
    argv[2] = "--windows";
    char frame[256];

    int total = socket_frame_encode(3, argv, frame, sizeof(frame));
    /* argc(3) + "query"(5) + "--windows"(9) = 17 payload bytes */
    TEST_CHECK(total, (int) sizeof(int) + 17);
    TEST_CHECK(socket_frame_encode(3, argv, NULL, 0), total);

    int declared;
    memcpy(&declared, frame, sizeof(int));
    TEST_CHECK(declared, 17);

    char *message = socket_frame_read(frame, total);
    TEST_CHECK((message != NULL), true);
    if (message) TEST_CHECK(memcmp(message, "query\0--windows\0", 17), 0);
})

TEST_FUNC(socket_frame_byte_exact,
{
    char *argv[3];
    argv[0] = "-m";
    argv[1] = "a";
    argv[2] = "bb";
    char frame[256];

    int total = socket_frame_encode(3, argv, frame, sizeof(frame));
    /* argc(3) + 1 + 2 = 6 payload bytes: "a\0bb\0\0" */
    TEST_CHECK(total, (int) sizeof(int) + 6);
    TEST_CHECK(memcmp(frame + sizeof(int), expected_two_arg_payload, sizeof(expected_two_arg_payload)), 0);

    /* Too small a buffer is measured, not written. */
    char small[4];
    memset(small, 'x', sizeof(small));
    TEST_CHECK(socket_frame_encode(3, argv, small, sizeof(small)), total);
    TEST_CHECK(small[0], 'x');
})

TEST_FUNC(socket_frame_many_args,
{
    /* No cap on the argument count: 300 args survive the read intact. */
    char *argv[301];
    argv[0] = "-m";
    for (int i = 1; i <= 300; ++i) argv[i] = "a";
    char frame[1024];

    int total = socket_frame_encode(301, argv, frame, sizeof(frame));
    TEST_CHECK(total, (int) sizeof(int) + 601);

    char *message = socket_frame_read(frame, total);
    TEST_CHECK((message != NULL), true);
    if (message) TEST_CHECK(memcmp(message, frame + sizeof(int), 601), 0);
})

TEST_FUNC(socket_frame_max_boundary,
{
    /* The largest frame (SOCKET_FRAME_MAX payload bytes) is accepted by
     * the daemon read and the Zig decoder; one byte more is rejected by
     * both, which pins the C and Zig limits together. */
    char *arg = malloc(SOCKET_FRAME_MAX);
    memset(arg, 'a', SOCKET_FRAME_MAX - 1);
    arg[SOCKET_FRAME_MAX - 1] = '\0';
    char *frame = malloc(sizeof(int) + SOCKET_FRAME_MAX + 1);
    char *argv[2];
    argv[0] = "-m";
    argv[1] = arg + 1;

    int total = socket_frame_encode(2, argv, frame, sizeof(int) + SOCKET_FRAME_MAX + 1);
    TEST_CHECK(total, (int) sizeof(int) + SOCKET_FRAME_MAX);
    size_t count = 0;
    TEST_CHECK(yabai_socket_frame_decode(frame, total, NULL, 0, &count), 0);
    TEST_CHECK((int) count, 2);
    char *message = socket_frame_read(frame, total);
    TEST_CHECK((message != NULL), true);
    if (message) TEST_CHECK(memcmp(message, frame + sizeof(int), SOCKET_FRAME_MAX), 0);

    argv[1] = arg;
    total = socket_frame_encode(2, argv, frame, sizeof(int) + SOCKET_FRAME_MAX + 1);
    TEST_CHECK(total, (int) sizeof(int) + SOCKET_FRAME_MAX + 1);
    TEST_CHECK(yabai_socket_frame_decode(frame, total, NULL, 0, &count), -1);
    TEST_CHECK((socket_frame_read(frame, total) == NULL), true);

    free(frame);
    free(arg);
})

TEST_FUNC(socket_frame_empty_arg,
{
    /* An empty argument still yields a well-formed frame, "\0\0", which
     * reaches handle_message (unknown domain ''). */
    char *argv[2];
    argv[0] = "-m";
    argv[1] = "";
    char frame[256];
    char rsp[256];
    uint64_t ts_used;
    int total = socket_frame_encode(2, argv, frame, sizeof(frame));
    TEST_CHECK(total, (int) sizeof(int) + 2);
    TEST_CHECK((socket_frame_daemon(frame, total, rsp, sizeof(rsp), &ts_used) > 0), true);
    TEST_CHECK(rsp[0], '\x07');
})

TEST_FUNC(socket_frame_daemon_rejects_zero,
{
    char rsp[256];
    uint64_t ts_used;
    TEST_CHECK(socket_frame_daemon_header(0, "", 0, rsp, sizeof(rsp), &ts_used), 0);
    TEST_CHECK((int) ts_used, 0);
})

TEST_FUNC(socket_frame_daemon_rejects_negative,
{
    char rsp[256];
    uint64_t ts_used;
    TEST_CHECK(socket_frame_daemon_header(-1, "", 0, rsp, sizeof(rsp), &ts_used), 0);
    TEST_CHECK((int) ts_used, 0);
})

TEST_FUNC(socket_frame_daemon_rejects_short_read,
{
    char rsp[256];
    uint64_t ts_used;
    /* Declares 20 payload bytes, sends 7, then EOF. */
    TEST_CHECK(socket_frame_daemon_header(20, "rule\0--", 7, rsp, sizeof(rsp), &ts_used), 0);
    /* Truncated length prefix. */
    TEST_CHECK(socket_frame_daemon("\x06\0", 2, rsp, sizeof(rsp), &ts_used), 0);
})

TEST_FUNC(socket_frame_daemon_rejects_missing_nul,
{
    char rsp[256];
    uint64_t ts_used;
    /* Last byte is not NUL. */
    TEST_CHECK(socket_frame_daemon_header(11, "rule\0--list", 11, rsp, sizeof(rsp), &ts_used), 0);
    /* Ends in NUL but lacks the final empty field the client always sends. */
    TEST_CHECK(socket_frame_daemon_header(12, "rule\0--list\0", 12, rsp, sizeof(rsp), &ts_used), 0);
})

TEST_FUNC(socket_frame_daemon_rejects_oversized,
{
    char rsp[256];
    uint64_t ts_used;
    TEST_CHECK(socket_frame_daemon_header(SOCKET_FRAME_MAX + 1, "", 0, rsp, sizeof(rsp), &ts_used), 0);
    TEST_CHECK((int) ts_used, 0);
    TEST_CHECK(socket_frame_daemon_header(0x7fffffff, "", 0, rsp, sizeof(rsp), &ts_used), 0);
    TEST_CHECK((int) ts_used, 0);
})

/* Encode tokens, run them through the real DAEMON_MESSAGE handler, and
 * return the reply. */
static int socket_frame_handle_live(char **tokens, int count, char *rsp, int rsp_size)
{
    char *argv[16];
    char frame[2048];
    uint64_t ts_used;
    argv[0] = "-m";
    for (int i = 0; i < count; ++i) argv[i + 1] = tokens[i];
    int total = socket_frame_encode(count + 1, argv, frame, sizeof(frame));
    return socket_frame_daemon(frame, total, rsp, rsp_size, &ts_used);
}

TEST_FUNC(socket_frame_unknown_domain_fails,
{
    /* Failures start with FAILURE_MESSAGE (0x07); the CLI sends those to
     * stderr and exits non-zero. */
    char *tokens[1];
    tokens[0] = "nosuchdomain";
    char rsp[4096];
    TEST_CHECK((socket_frame_handle_live(tokens, 1, rsp, sizeof(rsp)) > 0), true);
    TEST_CHECK(rsp[0], FAILURE_MESSAGE[0]);
    TEST_CHECK(rsp[0], '\x07');
})

TEST_FUNC(socket_frame_unknown_command_fails,
{
    char *tokens[2];
    tokens[0] = "query";
    tokens[1] = "--bogus";
    char rsp[4096];
    TEST_CHECK((socket_frame_handle_live(tokens, 2, rsp, sizeof(rsp)) > 0), true);
    TEST_CHECK(rsp[0], '\x07');
})

TEST_FUNC(socket_frame_rule_list_ok,
{
    /* Empty rule set serializes as "[]\n" with no failure prefix. */
    char *tokens[2];
    tokens[0] = "rule";
    tokens[1] = "--list";
    char rsp[4096];
    TEST_CHECK(socket_frame_handle_live(tokens, 2, rsp, sizeof(rsp)), 3);
    TEST_CHECK(strcmp(rsp, "[]\n"), 0);
})

TEST_FUNC(socket_frame_signal_list_ok,
{
    /* Empty signal set serializes as a JSON list with no failure prefix. */
    char *tokens[2];
    tokens[0] = "signal";
    tokens[1] = "--list";
    char rsp[4096];
    TEST_CHECK((socket_frame_handle_live(tokens, 2, rsp, sizeof(rsp)) > 0), true);
    TEST_CHECK(rsp[0], '[');
})
