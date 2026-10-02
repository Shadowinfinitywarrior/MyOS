#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: wget <url>");
        return 1;
    }
    char buf[4096];
    // Simple stub - would normally use web API
    snprintf(buf, sizeof(buf), "Fetching %s...\n", argv[1]);
    puts(buf);
    puts("wget: Web access not fully implemented in kernel API yet");
    return 0;
}
