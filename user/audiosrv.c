#include "libc.h"
int main(void) {
    puts("[audiosrv] Audio mixing daemon started.");
    while (1) { sleep_ms(5000); }
    return 0;
}
