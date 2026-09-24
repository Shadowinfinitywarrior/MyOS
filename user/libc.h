#ifndef USER_LIBC_H
#define USER_LIBC_H
#include "../include/system.h"

typedef int pid_t;

/* errno values (user side) — must match kernel/syscall.h */
#define EPERM   1
#define ENOENT  2
#define ESRCH   3
#define EBADF   9
#define ECHILD  10
#define EAGAIN  11
#define EFAULT  14
#define EINVAL  22
#define ENOSYS  38

extern int errno;

/* System call numbers (must match kernel) */
#define SYS_EXIT     1
#define SYS_FORK     2
#define SYS_READ     3
#define SYS_WRITE    4
#define SYS_OPEN     5
#define SYS_CLOSE    6
#define SYS_WAIT     7
#define SYS_KILL     12
#define SYS_GETPID   9
#define SYS_SLEEP    10
#define SYS_YIELD    11
#define SYS_PUTCHAR  22
#define SYS_GETCHAR  21

/* Syscall wrapper (SYSCALL instruction, SysV argument slots).
 * Returns -1 and sets errno on negative-errno results. */
long _syscall(long num, long a1, long a2, long a3, long a4, long a5);

/* Standard functions */
void exit(int code);
pid_t fork(void);
pid_t wait(pid_t pid, int *status);
int kill(pid_t pid, int sig);
pid_t getpid(void);
int read(int fd, void *buf, size_t count);
int write(int fd, const void *buf, size_t count);
int open(const char *path, int flags);
int close(int fd);
void sleep_ms(unsigned int ms);
void yield(void);
void putchar(char c);
char getchar(void);

/* String functions (self-contained) */
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
char *strchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

/* I/O helpers */
void puts(const char *s);
void print_int(int n);
void print_hex(unsigned int n);
void printf_simple(const char *fmt, ...);

#endif

