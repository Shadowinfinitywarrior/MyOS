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

/* Extended GUI syscalls for window management (Phase 4: Java/GraalVM) */
#define SYS_GUI_INIT                  50
#define SYS_GUI_CREATE_WINDOW         51
#define SYS_GUI_DESTROY_WINDOW        52
#define SYS_GUI_RENDER_FRAME          53
#define SYS_GUI_SET_FRAMEBUFFER       54
#define SYS_GUI_WINDOW_COUNT          55
#define SYS_GUI_GET_WINDOW            56
#define SYS_GUI_PUSH_KEY_EVENT        57
#define SYS_GUI_PUSH_MOUSE_EVENT      58
#define SYS_GUI_FOCUS_WINDOW          59
#define SYS_GUI_SET_WINDOW_TITLE      60
#define SYS_GUI_GET_WINDOW_RECT       61
#define SYS_GUI_SET_WINDOW_RECT       62
#define SYS_OS_CONTROL                65

/* OS control command opcodes for SYS_OS_CONTROL */
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
#define OS_CMD_POWER_STATUS   26
#define OS_CMD_POWER_STANDBY  27
#define OS_CMD_POWER_CHARGING 28
#define OS_CMD_NET_STATUS     29
#define OS_CMD_NET_WIFI       30
#define OS_CMD_NET_BT         31
#define OS_CMD_NET_ETH        32
#define OS_CMD_GET_TIME       33

/* IPC/MYDP syscalls for cross-language integration */
#define SYS_IPC_PORT_CREATE       70
#define SYS_IPC_PORT_DESTROY      71
#define SYS_IPC_PORT_SEND         72
#define SYS_IPC_PORT_RECV         73
#define SYS_IPC_PORT_FIND         74
#define SYS_IPC_CAP_GRANT         75
#define SYS_IPC_CAP_REVOKE        76
#define SYS_IPC_SHM_CREATE        77
#define SYS_IPC_SHM_ATTACH        78
#define SYS_IPC_SHM_DETACH        79
#define SYS_IPC_SHM_DESTROY       80
#define SYS_IPC_EVENT_SUBSCRIBE   81
#define SYS_IPC_EVENT_PUBLISH     82

/* MYDP protocol syscalls */
#define SYS_MYDP_CREATE_SURFACE   90
#define SYS_MYDP_ATTACH_BUFFER    91
#define SYS_MYDP_COMMIT           92
#define SYS_MYDP_DAMAGE           93
#define SYS_MYDP_CLOSE_SURFACE    94
#define SYS_MYDP_GET_FB_INFO      95

/* Signal handling syscalls */
#define SYS_SIGACTION      100
#define SYS_SIGRETURN      101
#define SYS_SIGPROCMASK    102

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
extern registers_t *g_current_regs;

#endif

