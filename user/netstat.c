#include "libc.h"

int main(void) {
    puts("Active Internet connections");
    puts("Proto Recv-Q Send-Q Local Address           Foreign Address         State");
    puts("tcp        0      0 0.0.0.0:80              0.0.0.0:*               LISTEN");
    return 0;
}
