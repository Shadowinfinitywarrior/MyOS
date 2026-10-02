#include "libc.h"

int main(void) {
    puts("procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu-----");
    puts(" r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st");
    puts(" 1  0      0 2080768      0      0    0    0     0     0    0    0  0  0 100  0  0");
    return 0;
}
