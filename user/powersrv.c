#include "libc.h"
int main(void) {
    puts("[powersrv] ACPI power event daemon started.");
    while (1) { sleep_ms(6000); }
    return 0;
}
