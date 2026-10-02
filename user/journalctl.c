#include "libc.h"

int main(void) {
    puts("-- Logs begin at Fri 2026-10-02 10:00:00 --");
    puts("Oct 02 10:00:00 myos kernel: MyOS started");
    puts("Oct 02 10:00:01 myos systemd: Started MyOS services");
    return 0;
}
