/* CLI golden-fixture tests (headless-safe daemon invocations).
 *
 * The daemon binary under test is the freshly built ../bin/yabai (repo
 * root bin/ relative to the tests/ working directory). Only invocations
 * that exit without a display server or a running daemon are pinned:
 * --help/-h (stdout), --version/-v (stdout, shape only), and bare -m
 * (client-side usage error on stderr, captured via 2>&1). Message
 * commands that reach the socket (-m <args>) are daemon-state dependent
 * and are deliberately NOT pinned here.
 *
 * --version is shape-checked (yabai-vMajor.Minor.Patch newline) instead
 * of byte-compared so version bumps do not break the suite. The
 * tests/data/version.txt golden is a record-time reference only; the
 * suite asserts it is well-formed, never that live output equals it.
 *
 * Bump procedure: after a version change, re-run ./bin/yabai --help and
 * --version headless and overwrite tests/data/help.txt (it embeds the
 * version in the `man yabai` pointer line), tests/data/version.txt, and
 * tests/data/m_no_args.txt if the usage text changed.
 */

#define CLI_OUT_MAX 8192

static char cli_bin_path[256];
static char cli_data_dir[256];

static bool cli_paths_init(void)
{
    static bool done = false;
    static bool ok = false;
    if (done) return ok;
    done = true;

    static const char *roots[] = { "..", "../.." };
    int i = 0;
    for (i = 0; i < array_count(roots); ++i) {
        snprintf(cli_bin_path, sizeof(cli_bin_path), "%s/bin/yabai", roots[i]);
        FILE *probe = fopen(cli_bin_path, "rb");
        if (probe) {
            fclose(probe);
            snprintf(cli_data_dir, sizeof(cli_data_dir), "%s/tests/data", roots[i]);
            ok = true;
            return ok;
        }
    }

    cli_bin_path[0] = '\0';
    cli_data_dir[0] = '\0';
    return ok;
}

struct cli_output
{
    char bytes[CLI_OUT_MAX];
    int length;
    int exited_zero;
};

static bool cli_run(const char *args, struct cli_output *out)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s %s", cli_bin_path, args);

    FILE *pipe = popen(cmd, "r");
    if (!pipe) return false;

    out->length = 0;
    size_t n = fread(out->bytes, 1, sizeof(out->bytes) - 1, pipe);
    out->length = (int) n;
    out->bytes[n] = '\0';
    out->exited_zero = pclose(pipe) == 0;

    /* A full buffer with more output pending would silently truncate. */
    return n < sizeof(out->bytes) - 1;
}

static bool cli_read_golden(const char *name, struct cli_output *out)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", cli_data_dir, name);

    FILE *file = fopen(path, "rb");
    if (!file) return false;

    size_t n = fread(out->bytes, 1, sizeof(out->bytes) - 1, file);
    fclose(file);

    out->length = (int) n;
    out->bytes[n] = '\0';
    return n < sizeof(out->bytes) - 1;
}

/* Shape check for "yabai-v<major>.<minor>.<patch>\n"; components must be
 * non-negative decimal integers with no extra trailing bytes. */
static bool cli_version_shape_ok(const char *s, int n)
{
    int major = 0;
    int minor = 0;
    int patch = 0;
    int consumed = 0;

    if (n <= 0) return false;
    if (sscanf(s, "yabai-v%d.%d.%d%n", &major, &minor, &patch, &consumed) != 3) return false;
    if (major < 0 || minor < 0 || patch < 0) return false;

    return consumed == n - 1 && s[consumed] == '\n' && s[consumed + 1] == '\0';
}

TEST_FUNC(zig_cli_help_golden,
{
    TEST_CHECK(cli_paths_init(), true);

    struct cli_output live = {0};
    struct cli_output alias = {0};
    struct cli_output golden = {0};

    TEST_CHECK(cli_run("--help", &live), true);
    TEST_CHECK(live.exited_zero, 1);
    TEST_CHECK(cli_run("-h", &alias), true);
    TEST_CHECK(alias.exited_zero, 1);
    TEST_CHECK(cli_read_golden("help.txt", &golden), true);

    TEST_CHECK(live.length, golden.length);
    TEST_CHECK(alias.length, golden.length);
    if (live.length == golden.length) {
        TEST_CHECK(memcmp(live.bytes, golden.bytes, (size_t) golden.length), 0);
    }
    if (alias.length == golden.length) {
        TEST_CHECK(memcmp(alias.bytes, golden.bytes, (size_t) golden.length), 0);
    }
})

static const char *cli_help_domains[] = {
    "config", "display", "space", "window", "query", "rule", "signal",
};

TEST_FUNC(zig_cli_help_domains,
{
    TEST_CHECK(cli_paths_init(), true);

    struct cli_output golden = {0};
    TEST_CHECK(cli_read_golden("help.txt", &golden), true);

    int i = 0;
    for (i = 0; i < array_count(cli_help_domains); ++i) {
        char args[64];
        struct cli_output live = {0};
        snprintf(args, sizeof(args), "--help %s", cli_help_domains[i]);

        TEST_CHECK(cli_run(args, &live), true);
        TEST_CHECK(live.exited_zero, 1);
        TEST_CHECK(live.length, golden.length);
        if (live.length == golden.length) {
            TEST_CHECK(memcmp(live.bytes, golden.bytes, (size_t) golden.length), 0);
        }
    }
})

TEST_FUNC(zig_cli_version_shape,
{
    TEST_CHECK(cli_paths_init(), true);

    struct cli_output live = {0};
    struct cli_output alias = {0};
    struct cli_output golden = {0};

    TEST_CHECK(cli_run("--version", &live), true);
    TEST_CHECK(live.exited_zero, 1);
    TEST_CHECK(cli_version_shape_ok(live.bytes, live.length), true);

    TEST_CHECK(cli_run("-v", &alias), true);
    TEST_CHECK(alias.exited_zero, 1);
    TEST_CHECK(cli_version_shape_ok(alias.bytes, alias.length), true);

    /* The version golden is a record-time reference: it must itself be
     * well-formed, but live output is never byte-compared against it. */
    TEST_CHECK(cli_read_golden("version.txt", &golden), true);
    TEST_CHECK(cli_version_shape_ok(golden.bytes, golden.length), true);
})

TEST_FUNC(zig_cli_message_no_args,
{
    TEST_CHECK(cli_paths_init(), true);

    struct cli_output live = {0};
    struct cli_output alias = {0};
    struct cli_output golden = {0};

    /* Bare -m fails client-side before touching the socket, so the
     * usage error is deterministic with or without a running daemon. */
    TEST_CHECK(cli_run("-m 2>&1", &live), true);
    TEST_CHECK(live.exited_zero, 0);
    TEST_CHECK(cli_run("--message 2>&1", &alias), true);
    TEST_CHECK(alias.exited_zero, 0);
    TEST_CHECK(cli_read_golden("m_no_args.txt", &golden), true);

    TEST_CHECK(live.length, golden.length);
    TEST_CHECK(alias.length, golden.length);
    if (live.length == golden.length) {
        TEST_CHECK(memcmp(live.bytes, golden.bytes, (size_t) golden.length), 0);
    }
    if (alias.length == golden.length) {
        TEST_CHECK(memcmp(alias.bytes, golden.bytes, (size_t) golden.length), 0);
    }
})
