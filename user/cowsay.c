#include "libc.h"

int main(int argc, char **argv) {
    const char *msg = argc > 1 ? argv[1] : "Hello from MyOS!";
    printf(" %s\n", msg);
    puts("  \\   ^__^");
    puts("   \\  (oo)\\_______");
    puts("      (__)\\       )\\/\\");
    puts("          ||----w |");
    puts("          ||     ||");
    return 0;
}
