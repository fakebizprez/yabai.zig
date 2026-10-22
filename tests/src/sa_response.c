#include <errno.h>

/* Exercise the real client against a private socket, without contacting the Dock payload. */
TEST_FUNC(sa_response_status,
{
    char directory[] = "/tmp/yabai-sa-response-XXXXXX";
    char previous_path[sizeof(g_sa_socket_file)];
    memcpy(previous_path, g_sa_socket_file, sizeof(previous_path));
    struct sockaddr_un address = {0};
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
    int listening = listen(listener, 3);
    TEST_CHECK(listening, 0);
    if (listening != 0) goto cleanup;

    pid_t server = fork();
    TEST_CHECK(server != -1, true);
    if (server == -1) goto cleanup;
    if (server == 0) {
        alarm(5);
        for (int mode = 0; mode < 3; ++mode) {
            int client = accept(listener, NULL, NULL);
            if (client == -1) _exit(1);
            char request[5];
            ssize_t received = recv(client, request, sizeof(request), MSG_WAITALL);
            if (received != sizeof(request) || memcmp(request, "test", sizeof(request)) != 0) _exit(2);
            if (mode > 0) {
                char reply = mode == 1 ? 0 : SA_RESPONSE_FAILURE;
                if (send(client, &reply, 1, 0) != 1) _exit(3);
            }
            close(client);
        }
        close(listener);
        _exit(0);
    }

    close(listener);
    listener = -1;
    snprintf(g_sa_socket_file, sizeof(g_sa_socket_file), "%s", address.sun_path);
    for (int mode = 0; mode < 3; ++mode) {
        char request[] = "test";
        bool sent = scripting_addition_send_bytes(request, sizeof(request));
        TEST_CHECK(sent, mode != 2);
    }
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
