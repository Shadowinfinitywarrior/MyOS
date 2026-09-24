#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    puts("========================================");
    puts("  Hello from User Space!");
    puts("  This program runs in Ring 3.");
    puts("========================================");

    printf_simple("My PID is: %d\n", getpid());
    puts("Sleeping for 1 second...");
    sleep_ms(1000);
    puts("Done! Exiting.");

    return 0;
}

