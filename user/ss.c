#include "libc.h"

int main(void) {
    puts("State    Recv-Q Send-Q  Local Address:Port  Peer Address:Port");
    puts("LISTEN   0      128     *:80               *:*");
    puts("LISTEN   0      128     *:22               *:*");
    return 0;
}
