 void print_usage(){
        fprintf(stderr,
        "\n"
        C_GREEN
        "  ┌──────────────────────────────────────────────────────┐\n"
        "  │    rsi  —  Reverse Shell Improver                    │\n"
        "  │                                                      │\n"
        "  │  Usage:  rsi <ANY-LISTNER> [OPTIONS]                 │\n"
        "  │                                                      │\n"
        "  │  Options:                                            │\n"
        "  │    --ssh    Probe port 22, upload SSH key,           │\n"
        "  │    --help   Show this help screen                    │\n"
        "  │                                                      │\n"
        "  │  Examples:                                           │\n"
        "  │    rsi nc -nlvp 4444                                 │\n"
        "  │    rsi nc -nlvp 1337 --ssh                           │\n"
        "  │    rsi ncat --listen -p 9001                         │\n"
        "  │                                                      │\n"
        "  │  HOW IT WORKS                                        │\n"
        "  │    • Starts the supplied listener with its arguments │\n"
        "  │    • Waits for an incoming connection                │\n"
        "  │    • Improves the resulting shell automatically      │\n"
        "  │    • Configures the terminal for interactive use     │\n"
        "  │                                                      │\n"
        "  │  SSH MODE                                            │\n"
        "  │    --ssh checks whether SSH is available and, when   │\n"
        "  │    possible, attempts to establish a persistent      │\n"
        "  │    SSH session instead of relying on the listener.   │\n"
        "  │                                                      │\n"
        "  │  Without --ssh, rsi performs the shell-improvement   │\n"
        "  │  steps and returns control of stdin/stdout to the    │\n"
        "  │  resulting interactive shell.                        │\n"
        "  └──────────────────────────────────────────────────────┘\n"
        C_RST "\n");
}

void print_banner(){
    fprintf(stderr,
        "\n"
        C_GREEN
        "  ┌────────────────────────────────────┐\n"
        "  │    rsi  —  Reverse Shell Improver  │\n"
        "  └────────────────────────────────────┘\n"
        "\n"
        C_RST);
}

