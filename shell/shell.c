#include "../lib/printf.h"
#include <string.h>
#include <stdint.h>
#include "../drivers/serial.h"
#include "../kernel/pmm.h"
#include "../kernel/process.h"
#include "../kernel/scheduler.h"
#include "../kernel/timer.h"
#include "../fs/vfs.h"

#define MAX_LINE 256
#define MAX_ARGS 16
#define HISTORY_SIZE 32

static char history[HISTORY_SIZE][MAX_LINE];
static int hist_len = 0;
static vfs_node_t *cwd = NULL;

static void shell_puts(const char *s) { kprintf("%s", s); }
static void shell_putc(char c) { kprintf("%c", c); }

static int atoi_simple(const char *s) {
    int n = 0, sign = 1;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    return sign * n;
}

static void trim_newline(char *s) {
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\n' || s[i] == '\r') { s[i] = '\0'; return; }
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

/* Builtins */
static void cmd_help(char *argv[]) {
    shell_puts("Kernel Shell builtins:\n");
    shell_puts("  help       - this help\n");
    shell_puts("  echo       - print args\n");
    shell_puts("  clear      - clear screen\n");
    shell_puts("  history    - show history\n");
    shell_puts("  meminfo    - show memory info\n");
    shell_puts("  ps         - list processes\n");
    shell_puts("  uptime     - system uptime\n");
    shell_puts("  kill       - kill process by pid\n");
    shell_puts("  pwd        - print working directory\n");
    shell_puts("  cd         - change directory\n");
    shell_puts("  ls         - list directory\n");
    shell_puts("  cat        - display file\n");
    shell_puts("  reboot     - reboot system\n");
    shell_puts("  halt       - halt system\n");
}

static void cmd_echo(char *argv[]) {
    for (int i = 1; argv[i]; i++) {
        shell_puts(argv[i]);
        if (argv[i+1]) shell_putc(' ');
    }
    shell_puts("\n");
}

static void cmd_clear(char *argv[]) {
    shell_puts("\033[2J\033[H");
}

static void cmd_history(char *argv[]) {
    for (int i = 0; i < hist_len; i++) {
        kprintf("%d: %s\n", i+1, history[i]);
    }
}

static void cmd_meminfo(char *argv[]) {
    uint64_t total = pmm_get_total_pages();
    uint64_t free = pmm_get_free_pages();
    uint64_t used = total > free ? total - free : 0;
    kprintf("Physical Memory:\n");
    kprintf("  Total pages : %llu\n", total);
    kprintf("  Free pages  : %llu\n", free);
    kprintf("  Used pages  : %llu\n", used);
    kprintf("  Page size   : 4096 bytes\n");
}

static void cmd_pwd(char *argv[]) {
    if (cwd) kprintf("%s\n", cwd->name);
    else kprintf("/\n");
}

static void cmd_cd(char *argv[]) {
    const char *path = argv[1] ? argv[1] : "/";
    vfs_node_t *node;
    if (path[0] == '/') node = vfs_resolve_path(path);
    else {
        // TODO: relative path resolution
        char full[512];
        strcpy(full, "/");
        // simplified
        node = vfs_resolve_path(path);
    }
    if (!node || !(node->flags & VFS_DIRECTORY)) {
        kprintf("cd: %s: No such directory\n", path);
        return;
    }
    cwd = node;
}

static void cmd_ls(char *argv[]) {
    const char *path = argv[1] ? argv[1] : (cwd ? cwd->name : "/");
    vfs_node_t *node;
    if (path[0] == '/') node = vfs_resolve_path(path);
    else {
        // simplified relative
        node = vfs_resolve_path(path);
    }
    if (!node) {
        kprintf("ls: cannot access '%s': No such file\n", path);
        return;
    }
    if (!(node->flags & VFS_DIRECTORY)) {
        kprintf("%s\n", node->name);
        return;
    }
    for (uint32_t i = 0; ; i++) {
        vfs_node_t *child = node->readdir ? node->readdir(node, i) : NULL;
        if (!child) break;
        kprintf("%s%c", child->name, (child->flags & VFS_DIRECTORY) ? '/' : ' ');
    }
    kprintf("\n");
}

static void cmd_cat(char *argv[]) {
    if (!argv[1]) {
        kprintf("cat: missing file operand\n");
        return;
    }
    vfs_node_t *node = vfs_resolve_path(argv[1]);
    if (!node) {
        kprintf("cat: cannot open '%s': No such file\n", argv[1]);
        return;
    }
    if (node->flags & VFS_DIRECTORY) {
        kprintf("cat: '%s' is a directory\n", argv[1]);
        return;
    }
    uint32_t len = node->length;
    if (len > 4096) len = 4096;
    char buf[4097];
    int r = vfs_read(node, 0, len, buf);
    if (r < 0) {
        kprintf("cat: read error\n");
        return;
    }
    buf[r] = '\0';
    kprintf("%s", buf);
}

static void cmd_ps(char *argv[]) {
    uint32_t count = process_count();
    kprintf("Processes: %u\n", count);
    kprintf("PID  NAME                           STATE\n");
    // process_dump_all() already prints details if available
    process_dump_all();
}

static void cmd_uptime(char *argv[]) {
    uint32_t secs = timer_get_seconds();
    uint32_t days = secs / 86400;
    uint32_t hours = (secs % 86400) / 3600;
    uint32_t mins = (secs % 3600) / 60;
    uint32_t s = secs % 60;
    kprintf("Uptime: %u days %02u:%02u:%02u\n", days, hours, mins, s);
}

static void cmd_kill(char *argv[]) {
    if (!argv[1]) {
        shell_puts("kill: missing pid\n");
        return;
    }
    pid_t pid = (pid_t)atoi_simple(argv[1]);
    int sig = 9;
    if (argv[2]) sig = atoi_simple(argv[2]);
    int ret = process_kill(pid, sig);
    if (ret == 0) kprintf("Killed pid %d\n", pid);
    else kprintf("kill: failed pid %d\n", pid);
}

static void cmd_reboot(char *argv[]) {
    shell_puts("Reboot requested\n");
    // TODO: trigger reboot
}

static void cmd_halt(char *argv[]) {
    shell_puts("Halt requested\n");
    // TODO: halt CPU
}

static void exec_cmd(char *argv[]) {
    if (!argv[0]) return;
    if (strcmp(argv[0], "help") == 0) cmd_help(argv);
    else if (strcmp(argv[0], "echo") == 0) cmd_echo(argv);
    else if (strcmp(argv[0], "clear") == 0) cmd_clear(argv);
    else if (strcmp(argv[0], "history") == 0) cmd_history(argv);
    else if (strcmp(argv[0], "meminfo") == 0) cmd_meminfo(argv);
    else if (strcmp(argv[0], "ps") == 0) cmd_ps(argv);
    else if (strcmp(argv[0], "uptime") == 0) cmd_uptime(argv);
    else if (strcmp(argv[0], "kill") == 0) cmd_kill(argv);
    else if (strcmp(argv[0], "pwd") == 0) cmd_pwd(argv);
    else if (strcmp(argv[0], "cd") == 0) cmd_cd(argv);
    else if (strcmp(argv[0], "ls") == 0) cmd_ls(argv);
    else if (strcmp(argv[0], "cat") == 0) cmd_cat(argv);
    else if (strcmp(argv[0], "reboot") == 0) cmd_reboot(argv);
    else if (strcmp(argv[0], "halt") == 0) cmd_halt(argv);
    else {
        kprintf("Unknown command: %s\n", argv[0]);
        kprintf("Type 'help' for commands\n");
    }
}

/* Read line from serial with echo and basic editing */
static int shell_read_line(char *buf, int max) {
    int pos = 0;
    buf[0] = '\0';
    for (;;) {
        int c = serial_read();
        if (c < 0) continue;
        if (c == '\r' || c == '\n') {
            serial_write('\r');
            serial_write('\n');
            buf[pos] = '\0';
            return pos;
        }
        if (c == 127 || c == '\b') { // backspace
            if (pos > 0) {
                pos--;
                buf[pos] = '\0';
                serial_write('\b');
                serial_write(' ');
                serial_write('\b');
            }
            continue;
        }
        if (c >= 32 && c <= 126 && pos < max - 1) {
            buf[pos++] = (char)c;
            buf[pos] = '\0';
            serial_write(c);
        }
    }
}

void shell_init(void) {
    hist_len = 0;
    cwd = vfs_resolve_path("/");
    if (!cwd) cwd = vfs_resolve_path("/");
    kprintf("[SHELL] Kernel CLI initialized\n");
    kprintf("Type 'help' for commands\n");
}

void shell_run(void) {
    char line[MAX_LINE];
    char *argv[MAX_ARGS];
    kprintf("Kernel CLI ready > ");
    for (;;) {
        int n = shell_read_line(line, MAX_LINE);
        if (n <= 0) {
            // idle or no input
            continue;
        }
        line[n] = '\0';
        trim_newline(line);
        if (!*line) {
            kprintf("\n> ");
            continue;
        }
        add_history(line);
        split_args(line, argv);
        exec_cmd(argv);
        kprintf("\n> ");
    }
}

