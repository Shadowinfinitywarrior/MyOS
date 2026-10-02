#include "libc.h"

#define MAX_ARGS 16
#define MAX_INPUT 256

void parse_args(char *line, char **argv) {
    int i = 0;
    while (*line && i < MAX_ARGS - 1) {
        while (*line == ' ') line++;
        if (!*line) break;
        argv[i++] = line;
        while (*line && *line != ' ') line++;
        if (*line) {
            *line = '\0';
            line++;
        }
    }
    argv[i] = NULL;
}

int main(void) {
    char input[MAX_INPUT];
    char *argv[MAX_ARGS];

    /* Banner: the same art the boot splash prints from ascii.txt, so the shell
     * and the serial console greet identically. Copied verbatim from that file
     * (keep the two in sync when it changes). These are UTF-8 block elements,
     * U+2588/U+2591, so they need a cell to hold a codepoint rather than a
     * byte - see vtty_cell_t.ch and font_blocks(). */
    puts("░███     ░███              ░██████     ░██████");
    puts("░████   ░████             ░██   ░██   ░██   ░██");
    puts("░██░██ ░██░██ ░██    ░██ ░██     ░██ ░██");
    puts("░██ ░████ ░██ ░██    ░██ ░██     ░██  ░████████");
    puts("░██  ░██  ░██ ░██    ░██ ░██     ░██         ░██");
    puts("░██       ░██ ░██   ░███  ░██   ░██   ░██   ░██");
    puts("░██       ░██  ░█████░██   ░██████     ░██████");
    puts("                     ░██");
    puts("               ░███████");
    puts("");

    while (1) {
        write(1, "myos> ", 6);
        
        int n = read(0, input, MAX_INPUT - 1);
        if (n <= 0) continue;
        /* Strip trailing newline / carriage return */
        while (n > 0 && (input[n-1] == '\n' || input[n-1] == '\r')) {
            input[--n] = '\0';
        }
        input[n] = '\0';
        if (n == 0) continue;

        parse_args(input, argv);
        if (!argv[0]) continue;

        if (strcmp(argv[0], "help") == 0) {
            puts("Available commands:");
            puts("  help    - Show this help message");
            puts("  clear   - Clear screen");
            puts("  ps      - Process status");
            puts("  uptime  - Show system uptime");
            puts("  exit    - Exit shell");
        } else if (strcmp(argv[0], "clear") == 0) {
            for (int k = 0; k < 30; k++) {
                putchar('\n');
            }
        } else if (strcmp(argv[0], "ps") == 0) {
            ps();
        } else if (strcmp(argv[0], "uptime") == 0) {
            printf("Uptime: %d seconds\n", uptime());
        } else if (strcmp(argv[0], "exit") == 0) {
            break;
        } else {
            /* Try to run it via fork/exec */
            char path[128];
            if (argv[0][0] == '/') {
                strcpy(path, argv[0]);
            } else {
                snprintf(path, sizeof(path), "/bin/%s", argv[0]);
            }
            
            int pid = exec(path);
            if (pid < 0) {
                printf("Unknown command or failed to execute: %s\n", argv[0]);
            } else {
                int status;
                wait(pid, &status);
            }
        }
    }

    return 0;
}
