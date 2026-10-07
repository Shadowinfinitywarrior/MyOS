#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int main(int argc, char **argv) {
    if (argc < 4) {
        puts("Usage: calc <num1> <op> <num2>");
        puts("Ops: + - * / %");
        return 1;
    }

    int a = 0, b = 0;
    for (int i = 0; argv[1][i]; i++) a = a * 10 + (argv[1][i] - '0');
    for (int i = 0; argv[3][i]; i++) b = b * 10 + (argv[3][i] - '0');

    char op = argv[2][0];
    int result = 0;

    switch (op) {
        case '+': result = a + b; break;
        case '-': result = a - b; break;
        case '*': result = a * b; break;
        case '/': 
            if (b == 0) { puts("Error: division by zero"); return 1; }
            result = a / b; break;
        case '%': 
            if (b == 0) { puts("Error: modulo by zero"); return 1; }
            result = a % b; break;
        default:
            puts("Error: invalid operator");
            return 1;
    }

    printf("%d %c %d = %d\n", a, op, b, result);
    return 0;
}