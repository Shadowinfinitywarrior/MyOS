#include "libc.h"

int main(void) {
    puts("Software synthesizer (stub)");
    puts("Playing simple melody...");
    int notes[] = {440, 494, 523, 587, 659, 698, 784};
    for (int i = 0; i < 7; i++) {
        sleep_ms(100);
    }
    puts("Done");
    return 0;
}
