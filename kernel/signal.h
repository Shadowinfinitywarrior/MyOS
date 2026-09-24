#ifndef SIGNAL_H
#define SIGNAL_H

#include "../include/types.h"

/* Standard signals */
#define SIGHUP     1
#define SIGINT     2
#define SIGQUIT    3
#define SIGILL     4
#define SIGTRAP    5
#define SIGABRT    6
#define SIGBUS     7
#define SIGFPE     8
#define SIGKILL    9
#define SIGUSR1    10
#define SIGSEGV    11
#define SIGUSR2    12
#define SIGPIPE    13
#define SIGALRM    14
#define SIGTERM    15
#define SIGCHLD    17
#define SIGCONT    18
#define SIGSTOP    19

#define NSIGNALS   32

#define SIG_DFL    ((void *)0)
#define SIG_IGN    ((void *)1)

typedef void (*sighandler_t)(int);

int  signal_send(pid_t pid, int signum);
void signal_init(void);

#endif

