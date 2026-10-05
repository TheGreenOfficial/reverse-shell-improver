#include "idd.h"
#include "tui.c"
#include "worker.c"

int main(int argc, char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0) {
        print_usage();
        exit(EXIT_SUCCESS);
    }else{
    char  ssh_flag       = 0;
    char *listener_argv[argc];
    int   listener_argc  = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ssh") == 0) {
            ssh_flag = 1;
            continue;
        }
        if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            exit(EXIT_SUCCESS);
        }
        listener_argv[listener_argc++] = argv[i];
    }

        listener_argv[listener_argc] = NULL; 
        
    
    print_banner();

    start_session(listener_argv, listener_argc, ssh_flag);

    }


    return 0;
}