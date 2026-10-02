#include "libc.h"

int main(void) {
    puts("MyOS Shell Help");
    puts("---------------");
    puts("Common commands:");
    puts("  ls, cd, pwd, cat, echo, clear");
    puts("  ps, kill, free, uptime, date");
    puts("  mkdir, rm, cp, mv, touch");
    puts("  ifconfig, ipconfig, ping, nslookup");
    puts("  wget, curl, browser, websearch");
    puts("  audioctl, aplay, beep");
    puts("  python3, node, lua");
    puts("  reboot, poweroff, halt");
    puts("Type 'help <command>' for more info");
    return 0;
}
