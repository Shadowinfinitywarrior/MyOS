#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    puts("MyOS Terminal Commands:");
    puts("  Built-in: help, clear, ps, uptime, exit, cd, pwd");
    puts("  File ops: ls, cat, touch, cp, mv, rm, mkdir, rmdir");
    puts("  System:   ps, kill, free, df, date, uptime, version");
    puts("  Info:     whoami, uname, env, calc");
    puts("  Fun:      hello, fortune, cowsay, banner, toilet");
    puts("");
    puts("Type 'help' in shell for built-in commands");
    puts("Type '<command> --help' for command-specific help");
    return 0;
}