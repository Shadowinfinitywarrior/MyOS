#include "libc.h"

int main(void) {
    puts("Filesystem     1K-blocks  Used Available Use% Mounted on");
    puts("ramfs           262144      10240    251904   4% /");
    puts("devfs              1024         0       1024   0% /dev");
    return 0;
}
