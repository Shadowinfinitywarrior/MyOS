#include "printf.h"
#include "string.h"
#include "../drivers/screen.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

/* Renders an unsigned value into the output using kputchar.
 * Supports zero-padding to a given width (e.g. %08X). */
static void kprint_u64(unsigned long long n, int base, int uppercase,
                       int zero_pad, int width) {
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[32];
    int i = 0;
    if (n == 0) {
        buf[i++] = '0';
    }
    while (n > 0 && i < (int)sizeof(buf) - 1) {
        buf[i++] = digits[n % (unsigned)base];
        n /= (unsigned)base;
    }
    while (zero_pad && i < (int)sizeof(buf) - 1 && i < width) {
        buf[i++] = '0';
    }
    while (i > 0) kputchar(buf[--i]);
}

static void kprint_s64(long long v, int zero_pad, int width) {
    if (v < 0) {
        kputchar('-');
        unsigned long long uv = (unsigned long long)(-(v + 1)) + 1ULL;
        kprint_u64(uv, 10, 0, zero_pad, width);
    } else {
        kprint_u64((unsigned long long)v, 10, 0, zero_pad, width);
    }
}

void kprintf(const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            int zero_pad = 0, width = 0;
            if (*fmt == '0') { zero_pad = 1; fmt++; }
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
            int is_long = 0;
            while (*fmt == 'l' || *fmt == 'z' || *fmt == 't') { is_long = 1; fmt++; }
            char spec = *fmt;
            switch (spec) {
                case 's': {
                    char *s = __builtin_va_arg(args, char *);
                    if (!s) s = "(null)";
                    kputs(s); break;
                }
                case 'd':
                case 'i': {
                    if (is_long) {
                        long long v = __builtin_va_arg(args, long long);
                        kprint_s64(v, zero_pad, width);
                    } else {
                        int v = __builtin_va_arg(args, int);
                        kprint_s64(v, zero_pad, width);
                    }
                    break;
                }
                case 'u': {
                    unsigned long long v = is_long
                        ? (unsigned long long)__builtin_va_arg(args, unsigned long long)
                        : (unsigned long long)(unsigned int)__builtin_va_arg(args, unsigned int);
                    kprint_u64(v, 10, 0, zero_pad, width);
                    break;
                }
                case 'x': {
                    unsigned long long v = is_long
                        ? (unsigned long long)__builtin_va_arg(args, unsigned long long)
                        : (unsigned long long)(unsigned int)__builtin_va_arg(args, unsigned int);
                    kprint_u64(v, 16, 0, zero_pad, width);
                    break;
                }
                case 'X': {
                    unsigned long long v = is_long
                        ? (unsigned long long)__builtin_va_arg(args, unsigned long long)
                        : (unsigned long long)(unsigned int)__builtin_va_arg(args, unsigned int);
                    kprint_u64(v, 16, 1, zero_pad, width);
                    break;
                }
                case 'p': {
                    void *p = __builtin_va_arg(args, void *);
                    kprint_u64((unsigned long long)(uintptr_t)p, 16, 1,
                               zero_pad, width);
                    break;
                }
                case 'c': kputchar(__builtin_va_arg(args, int)); break;
                case '%': kputchar('%'); break;
                default: kputchar('%'); kputchar(*fmt); break;
            }
        } else {
            kputchar(*fmt);
        }
        fmt++;
    }
    __builtin_va_end(args);
}

int kputchar(int c) {
    screen_putchar((char)c);
    return c;
}

int kputs(const char *s) {
    while (*s) kputchar(*s++);
    return 0;
}

/* Same number rendering as above, but writing into a string buffer. */
static void sprint_u64(char **p, char *end, unsigned long long n, int base,
                       int uppercase, int zero_pad, int width) {
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[32];
    int i = 0;
    if (n == 0) {
        buf[i++] = '0';
    }
    while (n > 0 && i < (int)sizeof(buf) - 1) {
        buf[i++] = digits[n % (unsigned)base];
        n /= (unsigned)base;
    }
    while (zero_pad && i < (int)sizeof(buf) - 1 && i < width) {
        buf[i++] = '0';
    }
    while (i > 0 && *p < end) {
        *(*p)++ = buf[--i];
    }
}

static void sprint_s64(char **p, char *end, long long v, int zero_pad, int width) {
    if (v < 0) {
        if (*p < end) *(*p)++ = '-';
        unsigned long long uv = (unsigned long long)(-(v + 1)) + 1ULL;
        sprint_u64(p, end, uv, 10, 0, zero_pad, width);
    } else {
        sprint_u64(p, end, (unsigned long long)v, 10, 0, zero_pad, width);
    }
}

int ksprintf(char *buf, const char *fmt, ...) {
    char *p = buf;
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            int zero_pad = 0, width = 0;
            if (*fmt == '0') { zero_pad = 1; fmt++; }
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
            int is_long = 0;
            while (*fmt == 'l' || *fmt == 'z' || *fmt == 't') { is_long = 1; fmt++; }
            char spec = *fmt;
            switch (spec) {
                case 's': {
                    char *s = __builtin_va_arg(args, char *);
                    if (!s) s = "(null)";
                    while (*s) *p++ = *s++;
                    break;
                }
                case 'd':
                case 'i': {
                    if (is_long) {
                        long long v = __builtin_va_arg(args, long long);
                        sprint_s64(&p, buf + 1000, v, zero_pad, width);
                    } else {
                        int v = __builtin_va_arg(args, int);
                        sprint_s64(&p, buf + 1000, v, zero_pad, width);
                    }
                    break;
                }
                case 'u': {
                    unsigned long long v = is_long
                        ? (unsigned long long)__builtin_va_arg(args, unsigned long long)
                        : (unsigned long long)(unsigned int)__builtin_va_arg(args, unsigned int);
                    sprint_u64(&p, buf + 1000, v, 10, 0, zero_pad, width);
                    break;
                }
                case 'x': {
                    unsigned long long v = is_long
                        ? (unsigned long long)__builtin_va_arg(args, unsigned long long)
                        : (unsigned long long)(unsigned int)__builtin_va_arg(args, unsigned int);
                    sprint_u64(&p, buf + 1000, v, 16, 0, zero_pad, width);
                    break;
                }
                case 'X': {
                    unsigned long long v = is_long
                        ? (unsigned long long)__builtin_va_arg(args, unsigned long long)
                        : (unsigned long long)(unsigned int)__builtin_va_arg(args, unsigned int);
                    sprint_u64(&p, buf + 1000, v, 16, 1, zero_pad, width);
                    break;
                }
                case 'p': {
                    void *pv = __builtin_va_arg(args, void *);
                    sprint_u64(&p, buf + 1000, (unsigned long long)(uintptr_t)pv,
                               16, 1, zero_pad, width);
                    break;
                }
                case 'c': *p++ = __builtin_va_arg(args, int); break;
                case '%': *p++ = '%'; break;
                default: *p++ = '%'; *p++ = *fmt; break;
            }
        } else {
            *p++ = *fmt;
        }
        fmt++;
    }
    __builtin_va_end(args);
    *p = '\0';
    return (int)(p - buf);
}