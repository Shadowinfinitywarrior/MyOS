#include "string.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
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

int strncasecmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2 || (*s1|32) == (*s2|32))) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return (*s1|32) - (*s2|32);
}

int tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
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

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d < s) while (n--) *d++ = *s++;
    else { d += n-1; s += n-1; while (n--) *d-- = *s--; }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--) { if (*p1 != *p2) return *p1 - *p2; p1++; p2++; }
    return 0;
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
        int zero_pad = 0, width = 0;
        if (*format == '0') {
            zero_pad = 1;
            format++;
        }
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (*format - '0');
            format++;
        }
        while (*format == 'l' || *format == 'z' || *format == 't') {
            format++;
        }
        switch (*format) {
            case 'd': {
                int n = __builtin_va_arg(args, int);
                char buf[32];
                int i = 0, neg = 0;
                if (n < 0) { neg = 1; n = -n; }
                if (n == 0) { buf[i++] = '0'; }
                while (n > 0 && i < 30) { buf[i++] = '0' + (n % 10); n /= 10; }
                while (zero_pad && i < width && i < 30) { buf[i++] = '0'; }
                if (neg && written < (int)size - 1) str[written++] = '-';
                while (i > 0 && written < (int)size - 1) str[written++] = buf[--i];
                break;
            }
            case 'u': {
                unsigned int n = __builtin_va_arg(args, unsigned int);
                char buf[32];
                int i = 0;
                if (n == 0) { buf[i++] = '0'; }
                while (n > 0 && i < 30) { buf[i++] = '0' + (n % 10); n /= 10; }
                while (zero_pad && i < width && i < 30) { buf[i++] = '0'; }
                while (i > 0 && written < (int)size - 1) str[written++] = buf[--i];
                break;
            }
            case 'x':
            case 'X': {
                unsigned int n = __builtin_va_arg(args, unsigned int);
                const char *hex = (*format == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                char buf[32];
                int i = 0;
                if (n == 0) { buf[i++] = '0'; }
                while (n > 0 && i < 30) { buf[i++] = hex[n & 0xF]; n >>= 4; }
                while (zero_pad && i < width && i < 30) { buf[i++] = '0'; }
                while (i > 0 && written < (int)size - 1) str[written++] = buf[--i];
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
            default:
                break;
        }
        if (*format) format++;
    }
    if (size > 0) str[written] = '\0';
    __builtin_va_end(args);
    return written;
}
