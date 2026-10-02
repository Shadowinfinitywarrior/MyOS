#include "libc.h"
int main(void) {
    puts("[mountd] Auto-mount daemon started.");
    while (1) { sleep_ms(7000); }
    return 0;
}
