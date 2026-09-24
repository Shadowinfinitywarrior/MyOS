#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    printf_simple("[TEST] Process %d starting multitasking demo\n", getpid());

    pid_t child = fork();

    if (child == 0) {
        /* Child process */
        for (int i = 0; i < 5; i++) {
            printf_simple("[CHILD PID %d] iteration %d\n", getpid(), i);
            sleep_ms(500);
        }
        puts("[CHILD] Done, exiting.");
        return 0;
    } else {
        /* Parent process */
        printf_simple("[PARENT PID %d] forked child PID %d\n", getpid(), child);
        for (int i = 0; i < 3; i++) {
            printf_simple("[PARENT PID %d] iteration %d\n", getpid(), i);
            sleep_ms(700);
        }
        puts("[PARENT] Done, exiting.");
        return 0;
    }
}
