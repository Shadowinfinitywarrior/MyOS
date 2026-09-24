#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define MAX_LINE 256
#define MAX_ARGS 16
#define HISTORY_SIZE 32

static char history[HISTORY_SIZE][MAX_LINE];
static int hist_len = 0;
static int hist_idx = -1;

static void trim_newline(char *s) {
    int i = 0;
    while (s[i]) {
        if (s[i] == '\n' || s[i] == '\r') { s[i] = '\0'; break; }
        i++;
    }
}

static int split_args(char *line, char *argv[]) {
    int argc = 0;
    char *p = line;
    while (*p && argc < MAX_ARGS - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) { *p = '\0'; p++; }
    }
    argv[argc] = NULL;
    return argc;
}

static void add_history(const char *line) {
    if (!line || !*line) return;
    if (hist_len < HISTORY_SIZE) {
        strcpy(history[hist_len++], line);
    } else {
        for (int i = 1; i < HISTORY_SIZE; i++) strcpy(history[i-1], history[i]);
        strcpy(history[HISTORY_SIZE-1], line);
    }
}

static void builtin_help(char *argv[]) {
    puts("MyOS Shell");
    puts("Builtins:");
    puts("  help      - show this help");
    puts("  echo      - print args");
    puts("  clear     - clear screen");
    puts("  history   - show command history");
    puts("  pwd       - print working directory");
    puts("  cd        - change directory");
    puts("  ls        - list files");
    puts("  cat       - display file");
    puts("  exit      - exit shell");
}

static void builtin_echo(char *argv[]) {
    for (int i = 1; argv[i]; i++) {
        puts(argv[i]);
        if (argv[i+1]) putchar(' ');
    }
    putchar('\n');
}

static void builtin_clear(char *argv[]) {
    puts("\033[2J\033[H");
}

static void builtin_history(char *argv[]) {
    for (int i = 0; i < hist_len; i++) {
        print_int(i + 1);
        puts(": ");
        puts(history[i]);
    }
}

static void builtin_pwd(char *argv[]) {
    puts("/"); // TODO: implement cwd
}

static void builtin_cd(char *argv[]) {
    if (argv[1]) {
        puts("cd: TODO: change directory to ");
        puts(argv[1]);
        putchar('\n');
    } else {
        puts("cd: missing argument");
    }
}

static void builtin_ls(char *argv[]) {
    puts("ls: TODO: list files");
}

static void builtin_cat(char *argv[]) {
    if (!argv[1]) { puts("cat: missing file"); return; }
    puts("cat: TODO: display ");
    puts(argv[1]);
    putchar('\n');
}

static void exec_builtin(char *argv[]) {
    if (!argv[0]) return;
    if (strcmp(argv[0], "help") == 0) { builtin_help(argv); return; }
    if (strcmp(argv[0], "echo") == 0) { builtin_echo(argv); return; }
    if (strcmp(argv[0], "clear") == 0) { builtin_clear(argv); return; }
    if (strcmp(argv[0], "history") == 0) { builtin_history(argv); return; }
    if (strcmp(argv[0], "pwd") == 0) { builtin_pwd(argv); return; }
    if (strcmp(argv[0], "cd") == 0) { builtin_cd(argv); return; }
    if (strcmp(argv[0], "ls") == 0) { builtin_ls(argv); return; }
    if (strcmp(argv[0], "cat") == 0) { builtin_cat(argv); return; }
    if (strcmp(argv[0], "exit") == 0) { exit(0); }
    puts("Unknown command: ");
    puts(argv[0]);
    putchar('\n');
}

int main() {
    puts("MyOS Shell v1.0");
    puts("Type 'help' for commands");
    char line[MAX_LINE];
    char *argv[MAX_ARGS];
    while (1) {
        putchar('$');
        putchar(' ');
        int n = read(0, line, MAX_LINE - 1);
        if (n <= 0) continue;
        line[n] = '\0';
        trim_newline(line);
        if (!*line) continue;
        add_history(line);
        split_args(line, argv);
        exec_builtin(argv);
    }
    return 0;
}
