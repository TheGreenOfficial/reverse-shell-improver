static struct termios saved_term;
static int raw_mode = 0;

void term_save(void) {
    tcgetattr(STDIN_FILENO, &saved_term);
}

void term_raw(void) {
    if (raw_mode) return;

    struct termios t = saved_term;
    t.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    t.c_oflag &= ~OPOST;
    t.c_cflag |= CS8;
    t.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    t.c_cc[VMIN]  = 0;
    t.c_cc[VTIME] = 1;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
    raw_mode = 1;
}

void term_restore(void) {
    if (!raw_mode) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_term);
    raw_mode = 0;
}

void term_get_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0 || ws.ws_col == 0) {
        *rows = 24;
        *cols = 80;
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    }
}

static void term_on_signal(int sig) {
    (void)sig;
    term_restore();
    _exit(1);
}

void term_install_safety(void) {
    signal(SIGINT,  term_on_signal);
    signal(SIGTERM, term_on_signal);
    signal(SIGHUP,  term_on_signal);
    signal(SIGQUIT, term_on_signal);
    signal(SIGPIPE, SIG_IGN);
}
