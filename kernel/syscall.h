#ifndef SYSCALL_H
#define SYSCALL_H

#include "../include/types.h"
#include "../include/system.h"

/* System call numbers */
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
#define SYS_GETCWD   16
#define SYS_CHDIR    17
#define SYS_MKDIR    18
#define SYS_UNLINK   19
#define SYS_TIME     20
#define SYS_GETCHAR  21
#define SYS_PUTCHAR  22
#define SYS_PS       23
#define SYS_UPTIME   24
#define SYS_EXECVE   25

#define NUM_SYSCALLS 256

/* errno values (kernel side) — mirrored in user/libc.h */
#define EPERM   1
#define ENOENT  2
#define ESRCH   3
#define EBADF   9
#define ECHILD  10
#define EAGAIN  11
#define EFAULT  14
#define EINVAL  22
#define ENOSYS  38

void syscall_init(void);
void syscall_dispatch(registers_t *regs);

#endif

