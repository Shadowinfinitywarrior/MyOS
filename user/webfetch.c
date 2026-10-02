#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: webfetch <url>");
        return 1;
    }
    printf("Fetching: %s\n", argv[1]);
    puts("Content-Type: text/html");
    puts("");
    puts("<html><body><h1>MyOS Web Fetch</h1><p>Stub content</p></body></html>");
    return 0;
}
