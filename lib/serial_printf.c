#include "../include/system.h"
#include "printf.h"
#include "../drivers/serial.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static void put_num(char **p, unsigned long long n, int base, int uppercase,
                    int zero_pad, int width, char *end) {
    static const char digits_l[] = "0123456789abcdef";
    static const char digits_u[] = "0123456789ABCDEF";
    const char *digits = uppercase ? digits_u : digits_l;
    char buf[32];
    int i = 0;
    if (n == 0) {
        buf[i++] = '0';
    }
    while (n && i < (int)sizeof(buf) - 1) {
        buf[i++] = digits[n % (unsigned long long)base];
        n /= (unsigned long long)base;
    }
    while (zero_pad && i < (int)sizeof(buf) - 1 && i < width) {
        buf[i++] = '0';
    }
    while (i > 0 && *p < end) {
        *(*p)++ = buf[--i];
    }
}

void serial_printf(const char *fmt, ...) {
    char out[512] __attribute__((aligned(16)));
    char *p = out;
    char *end = out + 511;
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    while (*fmt && p < end) {
        if (*fmt == '%') {
            fmt++;
            if (!*fmt) break;
            int zero_pad = 0, width = 0;
            if (*fmt == '0') { zero_pad = 1; fmt++; }
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
            int is_long = 0;
            while (*fmt == 'l' || *fmt == 'z' || *fmt == 't') { is_long = 1; fmt++; }
            switch (*fmt) {
                case 's': {
                    char *s = __builtin_va_arg(args, char *);
                    if (!s) s = "(null)";
                    while (*s && p < end) *p++ = *s++;
                    break;
                }
                case 'c': {
                    int c = __builtin_va_arg(args, int);
                    if (p < end) *p++ = (char)c;
                    break;
                }
                case 'd':
                case 'i': {
                    long long v = is_long
                        ? __builtin_va_arg(args, long long)
                        : (long long)__builtin_va_arg(args, int);
                    if (v < 0) {
                        if (p < end) *p++ = '-';
                        unsigned long long uv = (unsigned long long)(-(v + 1)) + 1ULL;
                        put_num(&p, uv, 10, 0, 0, 0, end);
                    } else {
                        put_num(&p, (unsigned long long)v, 10, 0, 0, 0, end);
                    }
                    break;
                }
                case 'u': {
                    unsigned long long v = is_long
                        ? __builtin_va_arg(args, unsigned long long)
                        : (unsigned long long)(unsigned int)__builtin_va_arg(args, unsigned int);
                    put_num(&p, v, 10, 0, zero_pad, width, end);
                    break;
                }
                case 'x': {
                    unsigned long long v = is_long
                        ? __builtin_va_arg(args, unsigned long long)
                        : (unsigned long long)(unsigned int)__builtin_va_arg(args, unsigned int);
                    put_num(&p, v, 16, 0, zero_pad, width, end);
                    break;
                }
                case 'X': {
                    unsigned long long v = is_long
                        ? __builtin_va_arg(args, unsigned long long)
                        : (unsigned long long)(unsigned int)__builtin_va_arg(args, unsigned int);
                    put_num(&p, v, 16, 1, zero_pad, width, end);
                    break;
                }
                case 'p': {
                    void *ptr = __builtin_va_arg(args, void *);
                    put_num(&p, (unsigned long long)(uintptr_t)ptr, 16, 0,
                            zero_pad, width, end);
                    break;
                }
                case '%': {
                    if (p < end) *p++ = '%';
                    break;
                }
                default: {
                    if (p < end) *p++ = '%';
                    if (p < end) *p++ = *fmt;
                    break;
                }
            }
        } else {
            *p++ = *fmt;
        }
        fmt++;
    }
    *p = '\0';
    for (char *q = out; *q; q++) {
        serial_write(*q);
    }
    __builtin_va_end(args);
}
