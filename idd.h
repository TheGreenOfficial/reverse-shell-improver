#ifndef INC_DEF_DEC_H
#define INC_DEF_DEC_H

/*
 * HOW WILL IT WORKS UNDER THE HOOD (PLAN w)
 *
 * Forks a passed listener with it's arguments
 * and waits until socket conn is established
 * after that hijacks stdin and stdout of that
 * socket and sends commands to improve shell
 * that we do manually and also a feature it
 * has is if that machine has port 22 open and
 * that user's home dir exists then we it will
 * put our rsa key in .ssh/authorized_keys and
 * runs ssh to connect on another fork and if
 * no errors and got shell via ssh kills nc one
 * and hands us the stdin and stdout for ssh or
 * if ssh option unused or unavailable then does
 * its shell improving automations and give us
 * stdin and stdout back of the listener that got
 * shell.
 *
 */

// 4 preprocessor..

// ====================================includes====================================

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>


// =====================================defines=====================================

#define C_GREEN "\033[1;32m"
#define C_YLW   "\033[1;33m"
#define C_RST   "\033[0m"


// ==================================declerations===================================

void print_usage();
void print_banner();
void start_session(char *listener_argv[], int listener_argc, char ssh_flag);

void term_save(void);
void term_raw(void);
void term_restore(void);
void term_get_size(int *rows, int *cols);
void term_install_safety(void);

#endif
