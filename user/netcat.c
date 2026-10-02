#include "libc.h"

int main(int argc, char **argv) {
    puts("netcat (nc) - stub implementation");
    if (argc >= 3) {
        printf("Connecting to %s:%s\n", argv[1], argv[2]);
    }
    return 0;
}
