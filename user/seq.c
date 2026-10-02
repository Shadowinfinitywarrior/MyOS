#include "libc.h"

int main(int argc, char **argv) {
    int start = argc > 1 ? atoi(argv[1]) : 1;
    int end = argc > 2 ? atoi(argv[2]) : start;
    for (int i = start; i <= end; i++) {
        printf("%d\n", i);
    }
    return 0;
}
