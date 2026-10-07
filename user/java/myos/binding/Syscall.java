package myos.binding;

/**
 * Low-level syscall wrapper for MyOS.
 * This class provides direct access to kernel syscalls via native methods.
 * When compiled with GraalVM native-image, these native methods are implemented
 * in C and linked into the native executable.
 */
public final class Syscall {

    // System call numbers (must match kernel/syscall.h)
    public static final int SYS_EXIT      = 1;
    public static final int SYS_FORK      = 2;
    public static final int SYS_READ      = 3;
    public static final int SYS_WRITE     = 4;
    public static final int SYS_OPEN      = 5;
    public static final int SYS_CLOSE     = 6;
    public static final int SYS_WAIT      = 7;
    public static final int SYS_EXECVE    = 8;
    public static final int SYS_GETPID    = 9;
    public static final int SYS_SLEEP     = 10;
    public static final int SYS_YIELD     = 11;
    public static final int SYS_KILL      = 12;
    public static final int SYS_BRK       = 13;
    public static final int SYS_MMAP      = 14;
    public static final int SYS_MUNMAP    = 15;
    public static final int SYS_GETCWD    = 16;
    public static final int SYS_CHDIR     = 17;
    public static final int SYS_MKDIR     = 18;
    public static final int SYS_UNLINK    = 19;
    public static final int SYS_TIME      = 20;
    public static final int SYS_GETCHAR   = 21;
    public static final int SYS_PUTCHAR   = 22;
    public static final int SYS_PS        = 23;
    public static final int SYS_UPTIME    = 24;
    public static final int SYS_REBOOT    = 26;
    public static final int SYS_SHUTDOWN  = 27;
    public static final int SYS_MEMINFO   = 28;
    public static final int SYS_READDIR   = 29;

    // GUI-related syscalls (must match kernel/syscall.h)
    public static final int SYS_GUI_CREATE_SURFACE   = 40;
    public static final int SYS_GUI_BLIT_SURFACE     = 41;
    public static final int SYS_GUI_INVALIDATE       = 42;
    public static final int SYS_GUI_GET_FB_INFO      = 43;
    public static final int SYS_GUI_INIT             = 50;
    public static final int SYS_GUI_CREATE_WINDOW    = 51;
    public static final int SYS_GUI_DESTROY_WINDOW   = 52;
    public static final int SYS_GUI_RENDER_FRAME     = 53;
    public static final int SYS_GUI_SET_FRAMEBUFFER  = 54;
    public static final int SYS_GUI_WINDOW_COUNT     = 55;
    public static final int SYS_GUI_GET_WINDOW       = 56;
    public static final int SYS_GUI_PUSH_KEY_EVENT   = 57;
    public static final int SYS_GUI_PUSH_MOUSE_EVENT = 58;
    public static final int SYS_GUI_FOCUS_WINDOW     = 59;
    public static final int SYS_GUI_SET_WINDOW_TITLE = 60;
    public static final int SYS_GUI_GET_WINDOW_RECT  = 61;
    public static final int SYS_GUI_SET_WINDOW_RECT  = 62;

    // PTY-related syscalls
    public static final int SYS_PTY_ALLOC    = 200;
    public static final int SYS_PTY_FREE     = 201;
    public static final int SYS_PTY_READ     = 202;
    public static final int SYS_PTY_WRITE    = 203;
    public static final int SYS_PTY_GET_SIZE = 204;
    public static final int SYS_PTY_SET_SIZE = 205;
    public static final int SYS_PTY_PUSH_INPUT = 206;

    private static int errno = 0;

    private Syscall() {}

    public static int getErrno() {
        return errno;
    }

    /**
     * Raw syscall invocation. Implemented in native code (C) that executes
     * the SYSCALL instruction with SysV argument passing convention.
     *
     * @param num Syscall number
     * @param a1 First argument
     * @param a2 Second argument
     * @param a3 Third argument
     * @param a4 Fourth argument
     * @param a5 Fifth argument
     * @param a6 Sixth argument
     * @return Syscall return value, or -1 on error (errno set)
     */
    public static native long syscall(int num, long a1, long a2, long a3, long a4, long a5, long a6);

    // Convenience wrappers for common syscalls
    public static void exit(int code) {
        syscall(SYS_EXIT, code, 0, 0, 0, 0, 0);
    }

    public static long fork() {
        return syscall(SYS_FORK, 0, 0, 0, 0, 0, 0);
    }

    public static long read(int fd, long bufPtr, long count) {
        return syscall(SYS_READ, fd, bufPtr, count, 0, 0, 0);
    }

    public static long write(int fd, long bufPtr, long count) {
        return syscall(SYS_WRITE, fd, bufPtr, count, 0, 0, 0);
    }

    public static int open(String path, int flags) {
        return (int) syscall(SYS_OPEN, toCString(path), flags, 0, 0, 0, 0);
    }

    public static int close(int fd) {
        return (int) syscall(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    }

    public static void sleepMs(long ms) {
        syscall(SYS_SLEEP, ms, 0, 0, 0, 0, 0);
    }

    public static void yield() {
        syscall(SYS_YIELD, 0, 0, 0, 0, 0, 0);
    }

    public static int getPid() {
        return (int) syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0);
    }

    public static int uptime() {
        return (int) syscall(SYS_UPTIME, 0, 0, 0, 0, 0, 0);
    }

    public static void putchar(char c) {
        syscall(SYS_PUTCHAR, c, 0, 0, 0, 0, 0);
    }

    public static char getchar() {
        return (char) syscall(SYS_GETCHAR, 0, 0, 0, 0, 0, 0);
    }

    // Helper to convert Java string to C-style null-terminated string pointer
    // In native-image, this would use CCharPointer or similar
    private static native long toCString(String s);

    // PTY wrappers
    public static int ptyAlloc(String name) {
        return (int) syscall(SYS_PTY_ALLOC, toCString(name), 0, 0, 0, 0, 0);
    }

    public static void ptyFree(int ptyId) {
        syscall(SYS_PTY_FREE, ptyId, 0, 0, 0, 0, 0);
    }

    public static int ptyRead(int ptyId, long bufPtr, long count) {
        return (int) syscall(SYS_PTY_READ, ptyId, bufPtr, count, 0, 0, 0);
    }

    public static int ptyWrite(int ptyId, long bufPtr, long count) {
        return (int) syscall(SYS_PTY_WRITE, ptyId, bufPtr, count, 0, 0, 0);
    }

    public static int ptyPushInput(int ptyId, long bufPtr, long count) {
        return (int) syscall(SYS_PTY_PUSH_INPUT, ptyId, bufPtr, count, 0, 0, 0);
    }
}