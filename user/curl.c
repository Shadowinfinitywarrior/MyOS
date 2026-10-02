#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: curl <url>");
        return 1;
    }
    char buf[4096];
    snprintf(buf, sizeof(buf), "curl: fetching %s", argv[1]);
    puts(buf);
    puts("curl: HTTP client stub - not fully connected to network yet");
    return 0;
}
