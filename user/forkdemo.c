#include "libc.h"

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    printf_simple("forkdemo: parent PID %d before fork\n", getpid());

    pid_t pid = fork();
    if (pid < 0) {
        puts("forkdemo: fork failed!");
        return 1;
    }

    if (pid == 0) {
        /* Child */
        printf_simple("  [child] PID %d, my fork() returned 0\n", getpid());
        sleep_ms(500);
        return 7;
    }

    /* Parent */
    printf_simple("forkdemo: parent PID %d, child PID %d, waiting...\n",
                  getpid(), pid);
    int status = -1;
    pid_t reaped = wait(pid, &status);
    printf_simple("forkdemo: wait() reaped PID %d with status %d\n",
                  reaped, status);

    /* fork(-1, any child) on a childless parent must fail with ECHILD */
    int rc = wait(-1, &status);
    printf_simple("forkdemo: wait(-1) with no children -> %d (errno %d)\n",
                  rc, errno);

    return 0;
}