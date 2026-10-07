#include "libc.h"

int errno;

/* ==========================================
 * Syscall wrapper (SYSCALL / SysV args) - 6 args matching kernel
 * ========================================== */
long _syscall(long num, long a1, long a2, long a3, long a4, long a5, long a6) {
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    long ret;
    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    if (ret < 0 && ret >= -4096) {
        errno = (int)(-ret);
        return -1;
    }
    return ret;
}

long os_control(long cmd, long a1, long a2, long a3) {
    return _syscall(SYS_OS_CONTROL, cmd, a1, a2, a3, 0, 0);
}

/* ==========================================
 * Process control
 * ========================================== */
void exit(int code) {
    _syscall(SYS_EXIT, code, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

pid_t fork(void) {
    return _syscall(SYS_FORK, 0, 0, 0, 0, 0, 0);
}

pid_t exec(const char *path) {
    return _syscall(SYS_EXECVE, (long)path, 0, 0, 0, 0, 0);
}

pid_t getpid(void) {
    return _syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0);
}

void ps(void) {
    _syscall(SYS_PS, 0, 0, 0, 0, 0, 0);
}

int uptime(void) {
    return _syscall(SYS_UPTIME, 0, 0, 0, 0, 0, 0);
}

int reboot(void) {
    return _syscall(SYS_REBOOT, 0, 0, 0, 0, 0, 0);
}

int shutdown(void) {
    return _syscall(SYS_SHUTDOWN, 0, 0, 0, 0, 0, 0);
}

void meminfo(void) {
    _syscall(SYS_MEMINFO, 0, 0, 0, 0, 0, 0);
}

pid_t wait(pid_t pid, int *status) {
    long ret = _syscall(SYS_WAIT, pid, status ? (long)status : 0, 0, 0, 0, 0);
    return (pid_t)ret;
}

int kill(pid_t pid, int sig) {
    long ret = _syscall(SYS_KILL, pid, sig, 0, 0, 0, 0);
    return (int)ret;
}

/* ==========================================
 * File I/O
 * ========================================== */
int read(int fd, void *buf, size_t count) {
    long ret = _syscall(SYS_READ, fd, (long)buf, (long)count, 0, 0, 0);
    return (int)ret;
}

int write(int fd, const void *buf, size_t count) {
    long ret = _syscall(SYS_WRITE, fd, (long)buf, (long)count, 0, 0, 0);
    return (int)ret;
}

int open(const char *path, int flags) {
    long ret = _syscall(SYS_OPEN, (long)path, flags, 0, 0, 0, 0);
    return (int)ret;
}

int close(int fd) {
    long ret = _syscall(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    return (int)ret;
}

/* ==========================================
 * Misc
 * ========================================== */
void sleep_ms(unsigned int ms) {
    _syscall(SYS_SLEEP, (long)ms, 0, 0, 0, 0, 0);
}

void yield(void) {
    _syscall(SYS_YIELD, 0, 0, 0, 0, 0, 0);
}

void putchar(char c) {
    _syscall(SYS_PUTCHAR, c, 0, 0, 0, 0, 0);
}

char getchar(void) {
    long ret = _syscall(SYS_GETCHAR, 0, 0, 0, 0, 0, 0);
    return (char)ret;
}

/* ==========================================
 * String functions
 * ========================================== */
size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) while (n--) *d++ = *s++;
    else { d += n-1; s += n-1; while (n--) *d-- = *s--; }
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s) { if (*s == c) return (char *)s; s++; }
    return NULL;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--) { if (*p1 != *p2) return *p1 - *p2; p1++; p2++; }
    return 0;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *strrchr(const char *s, int c) {
    char *last = NULL;
    do {
        if (*s == (char)c) last = (char *)s;
    } while (*s++);
    return last;
}

char *strtok(char *s, const char *delim) {
    static char *next = NULL;
    if (s) next = s;
    if (!next) return NULL;
    
    while (*next && strchr(delim, *next)) next++;
    if (!*next) return NULL;
    
    char *start = next;
    while (*next && !strchr(delim, *next)) next++;
    if (*next) *next++ = '\0';
    return start;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, format);
    
    int written = 0;
    while (*format && written < (int)size - 1) {
        if (*format != '%') {
            str[written++] = *format++;
            continue;
        }
        format++;
        switch (*format) {
            case 'd': {
                int n = __builtin_va_arg(args, int);
                char buf[12];
                int i = 0, neg = 0;
                if (n < 0) { neg = 1; n = -n; }
                if (n == 0) { buf[i++] = '0'; }
                while (n > 0 && i < 11) { buf[i++] = '0' + (n % 10); n /= 10; }
                if (neg) buf[i++] = '-';
                while (i > 0 && written < (int)size - 1) str[written++] = buf[--i];
                break;
            }
            case 'x': {
                unsigned int n = __builtin_va_arg(args, unsigned int);
                const char *hex = "0123456789abcdef";
                if (written + 2 < (int)size) {
                    str[written++] = '0'; str[written++] = 'x';
                }
                for (int i = 28; i >= 0 && written < (int)size - 1; i -= 4)
                    str[written++] = hex[(n >> i) & 0xF];
                break;
            }
            case 's': {
                const char *s = __builtin_va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s && written < (int)size - 1) str[written++] = *s++;
                break;
            }
            case 'c': {
                char c = (char)__builtin_va_arg(args, int);
                if (written < (int)size - 1) str[written++] = c;
                break;
            }
            case '%':
                if (written < (int)size - 1) str[written++] = '%';
                break;
            case 'u': {
                unsigned int n = __builtin_va_arg(args, unsigned int);
                char buf[12];
                int i = 0;
                if (n == 0) { buf[i++] = '0'; }
                while (n > 0 && i < 11) { buf[i++] = '0' + (n % 10); n /= 10; }
                while (i > 0 && written < (int)size - 1) str[written++] = buf[--i];
                break;
            }
            default:
                if (written < (int)size - 1) { str[written++] = '%'; str[written++] = *format; }
                break;
        }
        format++;
    }
    str[written] = '\0';
    __builtin_va_end(args);
    return written;
}

/* ==========================================
 * I/O helpers
 * ========================================== */
void puts(const char *s) {
    write(1, s, strlen(s));
    putchar('\n');
}

void print_int(int n) {
    char buf[12];
    int i = 0;
    int neg = 0;

    if (n < 0) { neg = 1; n = -n; }
    if (n == 0) { putchar('0'); return; }

    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    if (neg) buf[i++] = '-';

    while (i > 0) putchar(buf[--i]);
}

void print_hex(unsigned int n) {
    const char *hex = "0123456789abcdef";
    putchar('0'); putchar('x');
    for (int i = 28; i >= 0; i -= 4)
        putchar(hex[(n >> i) & 0xF]);
}

/* Minimal printf (supports %d, %x, %s, %c, %%) */
void printf_simple(const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    int written = 0;

    while (*fmt) {
        if (*fmt != '%') {
            putchar(*fmt++);
            written++;
            continue;
        }
        fmt++;
        switch (*fmt) {
            case 'd':
                print_int(__builtin_va_arg(args, int));
                break;
            case 'x':
                print_hex(__builtin_va_arg(args, unsigned int));
                break;
            case 's': {
                const char *s = __builtin_va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s) { putchar(*s++); written++; }
                break;
            }
            case 'c':
                putchar((char)__builtin_va_arg(args, int));
                written++;
                break;
            case '%':
                putchar('%');
                written++;
                break;
            default:
                putchar('%');
                putchar(*fmt);
                written += 2;
                break;
        }
        fmt++;
    }

    __builtin_va_end(args);
}

int printf(const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    int written = 0;

    while (*fmt) {
        if (*fmt != '%') {
            putchar(*fmt++);
            written++;
            continue;
        }
        fmt++;
        switch (*fmt) {
            case 'd':
                print_int(__builtin_va_arg(args, int));
                break;
            case 'x':
                print_hex(__builtin_va_arg(args, unsigned int));
                break;
            case 's': {
                const char *s = __builtin_va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s) { putchar(*s++); written++; }
                break;
            }
            case 'c':
                putchar((char)__builtin_va_arg(args, int));
                written++;
                break;
            case '%':
                putchar('%');
                written++;
                break;
            default:
                putchar('%');
                putchar(*fmt);
                written += 2;
                break;
        }
        fmt++;
    }

    __builtin_va_end(args);
    return written;
}

/* ==========================================
 * Memory mapping
 * ========================================== */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return (void *)_syscall(SYS_MMAP, (long)addr, (long)length, (long)prot,
                            (long)flags, (long)fd, (long)offset);
}

int munmap(void *addr, size_t length) {
    return (int)_syscall(SYS_MUNMAP, (long)addr, (long)length, 0, 0, 0, 0);
}

int mprotect(void *addr, size_t length, int prot) {
    return (int)_syscall(SYS_MPROTECT, (long)addr, (long)length, (long)prot, 0, 0, 0);
}

/* ==========================================
 * Shared memory
 * ========================================== */
int shmget(const char *name, size_t size, int flags) {
    return (int)_syscall(SYS_SHMGET, (long)name, (long)size, (long)flags, 0, 0, 0);
}

int shmctl(int fd, int cmd, void *arg) {
    return (int)_syscall(SYS_SHMCTL, (long)fd, (long)cmd, (long)arg, 0, 0, 0);
}

/* ==========================================
 * Signal handling
 * ========================================== */
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    return (int)_syscall(SYS_SIGACTION, (long)signum, (long)act, (long)oldact, 0, 0, 0);
}

int sigprocmask(int how, const uint32_t *set, uint32_t *oldset) {
    return (int)_syscall(SYS_SIGPROCMASK, (long)how, (long)set, (long)oldset, 0, 0, 0);
}

int sigreturn(void *saved_regs) {
    return (int)_syscall(SYS_SIGRETURN, (long)saved_regs, 0, 0, 0, 0, 0);
}

