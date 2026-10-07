#ifndef USER_LIBC_H
#define USER_LIBC_H
#include "../include/system.h"

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
#define SYS_EXEC     8
#define SYS_GETPID   9
#define SYS_SLEEP    10
#define SYS_YIELD    11
#define SYS_KILL     12
#define SYS_BRK      13
#define SYS_MMAP     14
#define SYS_MUNMAP   15
#define SYS_MPROTECT 16
#define SYS_GETCWD   17
#define SYS_CHDIR    18
#define SYS_MKDIR    19
#define SYS_UNLINK   20
#define SYS_SHMGET   21
#define SYS_SHMCTL   22
#define SYS_TIME     23
#define SYS_GETCHAR  24
#define SYS_PUTCHAR  25
#define SYS_PS       26
#define SYS_UPTIME   27
#define SYS_EXECVE   28
#define SYS_REBOOT   29
#define SYS_SHUTDOWN 30
#define SYS_MEMINFO  31
#define SYS_READDIR  32
#define SYS_GUI_CREATE_SURFACE 40
#define SYS_GUI_BLIT_SURFACE   41
#define SYS_GUI_INVALIDATE     42
#define SYS_GUI_GET_FB_INFO    43
#define SYS_OS_CONTROL         65

/* OS control command opcodes */
#define OS_CMD_AUTH_LOGIN     1
#define OS_CMD_AUTH_ADD_USER  2
#define OS_CMD_AUTH_PASSWD    3
#define OS_CMD_AUTH_WHOAMI    4
#define OS_CMD_AUTH_USERS     5
#define OS_CMD_AUTH_LOCK      6
#define OS_CMD_AUTH_LOGOUT    7
#define OS_CMD_WM_LIST        10
#define OS_CMD_WM_CLOSE       11
#define OS_CMD_WM_FOCUS       12
#define OS_CMD_WM_TILE        13
#define OS_CMD_APP_LAUNCH     14
#define OS_CMD_SET_THEME      15
#define OS_CMD_SET_MOUSE      16
#define OS_CMD_GET_MOUSE      17
#define OS_CMD_SET_DPI        18
#define OS_CMD_GET_DPI        19
#define OS_CMD_STORAGE_INFO   20
#define OS_CMD_STORAGE_SYNC   21
#define OS_CMD_PORTABLE_LIST  22
#define OS_CMD_PLAY_SOUND     23
#define OS_CMD_DRIVER_LIST    24
#define OS_CMD_PCI_LIST       25

/* Signal handling syscalls */
#define SYS_SIGACTION      100
#define SYS_SIGRETURN      101
#define SYS_SIGPROCMASK    102

/* Syscall wrapper (SYSCALL instruction, SysV argument slots).
 * Returns -1 and sets errno on negative-errno results. */
long _syscall(long num, long a1, long a2, long a3, long a4, long a5, long a6);
long os_control(long cmd, long a1, long a2, long a3);

/* Standard functions */
void exit(int code);
pid_t fork(void);
pid_t exec(const char *path);
pid_t wait(pid_t pid, int *status);
int kill(pid_t pid, int sig);
pid_t getpid(void);
void ps(void);
int uptime(void);
int reboot(void);
int shutdown(void);
void meminfo(void);
int readdir(int fd, int index, char *name_buf);
int read(int fd, void *buf, size_t count);
int write(int fd, const void *buf, size_t count);
int open(const char *path, int flags);
int close(int fd);
void sleep_ms(unsigned int ms);
void yield(void);
void putchar(char c);
char getchar(void);

/* Memory mapping */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
int mprotect(void *addr, size_t length, int prot);

/* Shared memory */
#define SHM_RDONLY 0x01
#define SHM_RND    0x02
int shmget(const char *name, size_t size, int flags);
int shmctl(int fd, int cmd, void *arg);

/* Signal handling - types and constants from kernel types.h */
#define SIGHUP     1
#define SIGINT     2
#define SIGQUIT    3
#define SIGILL     4
#define SIGTRAP    5
#define SIGABRT    6
#define SIGBUS     7
#define SIGFPE     8
#define SIGKILL    9
#define SIGUSR1    10
#define SIGSEGV    11
#define SIGUSR2    12
#define SIGPIPE    13
#define SIGALRM    14
#define SIGTERM    15
#define SIGCHLD    17
#define SIGCONT    18
#define SIGSTOP    19

#define NSIGNALS   32

#define SIG_DFL    ((void *)0)
#define SIG_IGN    ((void *)1)

#define SA_NOCLDSTOP  0x00000001
#define SA_NOCLDWAIT  0x00000002
#define SA_SIGINFO    0x00000004
#define SA_ONSTACK    0x00000008
#define SA_RESTART    0x00000010
#define SA_NODEFER    0x00000020
#define SA_RESETHAND  0x00000040
#define SA_NOMASK     SA_NODEFER
#define SA_ONESHOT    SA_RESETHAND

#define SIG_BLOCK     0
#define SIG_UNBLOCK   1
#define SIG_SETMASK   2

typedef void (*sighandler_t)(int);

/* siginfo_t, sigaction structures defined in kernel types.h */
typedef void (*sigaction_handler_t)(int, siginfo_t *, void *);

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int sigprocmask(int how, const uint32_t *set, uint32_t *oldset);

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
int printf(const char *fmt, ...);
int snprintf(char *str, size_t size, const char *format, ...);

#endif