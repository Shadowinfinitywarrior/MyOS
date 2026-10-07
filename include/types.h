#ifndef TYPES_H
#define TYPES_H

typedef unsigned char       uint8_t;
typedef signed char         int8_t;
typedef unsigned short      uint16_t;
typedef signed short        int16_t;
typedef unsigned int        uint32_t;
typedef signed int          int32_t;
typedef unsigned long long  uint64_t;
typedef signed long long    int64_t;

typedef uint64_t            size_t;
typedef int64_t             ssize_t;
typedef int64_t             ptrdiff_t;
typedef uint64_t            uintptr_t;
typedef int64_t             intptr_t;
typedef int32_t             pid_t;
typedef int32_t             off_t;
typedef uint32_t            mode_t;
typedef uint32_t            ino_t;
typedef int32_t             dev_t;
typedef uint32_t            uid_t;
typedef uint32_t            gid_t;

typedef enum { false = 0, true = 1 } bool;

#define NULL ((void *)0)

#define PACKED          __attribute__((packed))
#define ALIGNED(x)      __attribute__((aligned(x)))
#define UNUSED          __attribute__((unused))
#define NORETURN        __attribute__((noreturn))
#define ALWAYS_INLINE   __attribute__((always_inline)) inline
#define SECTION(x)      __attribute__((section(x)))

#define BIT(x)              (1U << (x))
#define SET_BIT(val, bit)   ((val) | BIT(bit))
#define CLEAR_BIT(val, bit) ((val) & ~BIT(bit))
#define TEST_BIT(val, bit)  ((val) & BIT(bit))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ALIGN_UP(val, align)   (((val) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(val, align) ((val) & ~((align) - 1))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define PAGE_SIZE 4096

#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

/* Signal constants (shared between process.h and signal.h to avoid circular dependency) */
#define NSIGNALS   32
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
#define SIGTSTP    20
#define SIGTTIN    21
#define SIGTTOU    22
#define SIGURG     23
#define SIGWINCH   24

#define SIG_DFL    ((void *)0)
#define SIG_IGN    ((void *)1)

/* sigaction flags */
#define SA_NOCLDSTOP  0x00000001
#define SA_NOCLDWAIT  0x00000002
#define SA_SIGINFO    0x00000004
#define SA_ONSTACK    0x00000008
#define SA_RESTART    0x00000010
#define SA_NODEFER    0x00000020
#define SA_RESETHAND  0x00000040
#define SA_NOMASK     SA_NODEFER
#define SA_ONESHOT    SA_RESETHAND

/* Signal handler types */
typedef void (*sighandler_t)(int);

/* Signal structures - complete definitions to avoid circular dependency */
typedef struct siginfo {
    int      si_signo;
    int      si_code;
    int      si_errno;
    pid_t    si_pid;
    uid_t    si_uid;
    void    *si_addr;
    int      si_status;
    long     si_band;
} siginfo_t;

struct sigaction {
    union {
        sighandler_t       sa_handler;
        void (*sa_sigaction)(int, siginfo_t *, void *);
    } sa_handler;
    uint32_t         sa_flags;
    void            (*sa_restorer)(void);
    uint32_t         sa_mask;
};

/* sigprocmask how values */
#define SIG_BLOCK     0
#define SIG_UNBLOCK   1
#define SIG_SETMASK   2

#define barrier() __asm__ __volatile__("" ::: "memory")

#endif