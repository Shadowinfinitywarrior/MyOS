#include "libc.h"

int main(void) {
    puts(" 10:00:00 up 0 min, 1 user, load average: 0.00, 0.00, 0.00");
    puts("USER     TTY      FROM             LOGIN@   IDLE   JCPU   PCPU WHAT");
    puts("root     tty1     -                10:00    0.00s  0.01s  0.00s shell");
    return 0;
}
