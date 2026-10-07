#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    
    int uptime_sec = uptime();
    int hours = uptime_sec / 3600;
    int mins = (uptime_sec % 3600) / 60;
    int secs = uptime_sec % 60;
    
    printf("System uptime: %02d:%02d:%02d\n", hours, mins, secs);
    return 0;
}