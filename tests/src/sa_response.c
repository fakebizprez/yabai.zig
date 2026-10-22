#include <errno.h>
#include <signal.h>
#include <sys/time.h>

static void sa_response_interrupt(int signo) { (void) signo; }

/* reply == -1: close without replying; reply == -2: hold the connection open so the
 * client's recv is interrupted and returns -1; otherwise send that one status byte. */
static const struct { int reply; bool expected; } sa_response_cases[] = {
    { -1,   false },
    { -2,   false },
    { 0x00, true  },
    { 0x01, false },
    { 0x02, false },
};
#define case_count ((int) (sizeof(sa_response_cases) / sizeof(sa_response_cases[0])))
#define cases sa_response_cases

/* Exercise the real client against a private socket, without contacting the Dock payload. */
TEST_FUNC(sa_response_status,
{
    char directory[] = "/tmp/yabai-sa-response-XXXXXX";
    char previous_path[sizeof(g_sa_socket_file)];
    memcpy(previous_path, g_sa_socket_file, sizeof(previous_path));
    struct sockaddr_un address = {0};
    struct sigaction interrupt = {0};
    struct sigaction previous_action = {0};
    int listener = -1;
    bool created = mkdtemp(directory) != NULL;
    TEST_CHECK(created, true);
    if (!created) goto cleanup;

    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, sizeof(address.sun_path), "%s/socket", directory);
    listener = socket(AF_UNIX, SOCK_STREAM, 0);
    TEST_CHECK(listener != -1, true);
    if (listener == -1) goto cleanup;
    int bound = bind(listener, (struct sockaddr *)&address, sizeof(address));
    TEST_CHECK(bound, 0);
    if (bound != 0) goto cleanup;
    int listening = listen(listener, case_count);
    TEST_CHECK(listening, 0);
    if (listening != 0) goto cleanup;

    pid_t server = fork();
    TEST_CHECK(server != -1, true);
    if (server == -1) goto cleanup;
    if (server == 0) {
        alarm(5);
        for (int i = 0; i < case_count; ++i) {
            int client = accept(listener, NULL, NULL);
            if (client == -1) _exit(1);
            char request[5];
            ssize_t received = recv(client, request, sizeof(request), MSG_WAITALL);
            if (received != sizeof(request) || memcmp(request, "test", sizeof(request)) != 0) _exit(2);
            if (cases[i].reply == -2) {
                char eof;
                if (recv(client, &eof, 1, 0) != 0) _exit(4);
            } else if (cases[i].reply >= 0) {
                char reply = (char) cases[i].reply;
                if (send(client, &reply, 1, 0) != 1) _exit(3);
            }
            close(client);
        }
        close(listener);
        _exit(0);
    }

    close(listener);
    listener = -1;
    interrupt.sa_handler = sa_response_interrupt;
    sigaction(SIGALRM, &interrupt, &previous_action);
    snprintf(g_sa_socket_file, sizeof(g_sa_socket_file), "%s", address.sun_path);
    for (int i = 0; i < case_count; ++i) {
        struct itimerval timer = { .it_value = { .tv_usec = cases[i].reply == -2 ? 100000 : 0 } };
        setitimer(ITIMER_REAL, &timer, NULL);
        char request[] = "test";
        bool sent = scripting_addition_send_bytes(request, sizeof(request));
        struct itimerval off = {0};
        setitimer(ITIMER_REAL, &off, NULL);
        TEST_CHECK(sent, cases[i].expected);
    }
    sigaction(SIGALRM, &previous_action, NULL);
    int status = 0;
    pid_t waited;
    do { waited = waitpid(server, &status, 0); } while (waited == -1 && errno == EINTR);
    TEST_CHECK(waited == server, true);
    TEST_CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, true);

cleanup:
    memcpy(g_sa_socket_file, previous_path, sizeof(previous_path));
    if (listener != -1) close(listener);
    if (created) {
        unlink(address.sun_path);
        rmdir(directory);
    }
})

#undef cases
#undef case_count
