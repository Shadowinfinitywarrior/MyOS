//go:build tinygo
// +build tinygo

// syscall_tinygo.go - Syscall wrappers for MyOS Go shell (TinyGo bare metal)
// Provides high-level syscall wrappers for graphics, input, file I/O, and process management.

package runtime

import (
	"unsafe"
)

// Syscall numbers (must match kernel/syscall.h)
const (
	SYS_EXIT           = 1
	SYS_FORK           = 2
	SYS_READ           = 3
	SYS_WRITE          = 4
	SYS_OPEN           = 5
	SYS_CLOSE          = 6
	SYS_WAIT           = 7
	SYS_EXEC           = 8
	SYS_GETPID         = 9
	SYS_SLEEP          = 10
	SYS_YIELD          = 11
	SYS_KILL           = 12
	SYS_BRK            = 13
	SYS_MMAP           = 14
	SYS_MUNMAP         = 15
	SYS_MPROTECT       = 16
	SYS_GETCWD         = 17
	SYS_CHDIR          = 18
	SYS_MKDIR          = 19
	SYS_UNLINK         = 20
	SYS_SHMGET         = 21
	SYS_SHMCTL         = 22
	SYS_TIME           = 23
	SYS_GETCHAR        = 24
	SYS_PUTCHAR        = 25
	SYS_PS             = 26
	SYS_UPTIME         = 27
	SYS_EXECVE         = 28
	SYS_REBOOT         = 29
	SYS_SHUTDOWN       = 30
	SYS_MEMINFO        = 31
	SYS_READDIR        = 32

	// GUI syscalls (Phase 3)
	SYS_GUI_CREATE_SURFACE = 40
	SYS_GUI_BLIT_SURFACE   = 41
	SYS_GUI_INVALIDATE     = 42
	SYS_GUI_GET_FB_INFO    = 43

	// Extended GUI syscalls
	SYS_GUI_CREATE_WINDOW  = 44
	SYS_GUI_DESTROY_WINDOW = 45
	SYS_GUI_SET_TITLE      = 46
	SYS_GUI_GET_EVENT      = 47
	SYS_GUI_POLL_EVENT     = 48

	// Input syscalls
	SYS_INPUT_GET_MOUSE    = 50
	SYS_INPUT_GET_KEYBOARD = 51
	SYS_INPUT_SET_CURSOR   = 52
)

// mmap protection flags
const (
	PROT_READ  = 0x1
	PROT_WRITE = 0x2
	PROT_EXEC  = 0x4
	PROT_NONE  = 0x0
)

// mmap flags
const (
	MAP_SHARED      = 0x01
	MAP_PRIVATE     = 0x02
	MAP_ANONYMOUS   = 0x20
	MAP_FIXED       = 0x10
	MAP_ANON        = MAP_ANONYMOUS
	MAP_FAILED      = ^uintptr(0)
	MAP_STACK       = 0x20000
)

// open flags
const (
	O_RDONLY   = 0x0000
	O_WRONLY   = 0x0001
	O_RDWR     = 0x0002
	O_CREAT    = 0x0040
	O_TRUNC    = 0x0200
	O_APPEND   = 0x0400
	O_EXCL     = 0x0080
	O_DIRECTORY = 0x10000
)

// seek whence
const (
	SEEK_SET = 0
	SEEK_CUR = 1
	SEEK_END = 2
)

// Error codes
const (
	EPERM   = 1
	ENOENT  = 2
	ESRCH   = 3
	EINTR   = 4
	EIO     = 5
	ENXIO   = 6
	E2BIG   = 7
	ENOEXEC = 8
	EBADF   = 9
	ECHILD  = 10
	EAGAIN  = 11
	ENOMEM  = 12
	EACCES  = 13
	EFAULT  = 14
	ENOTBLK = 15
	EBUSY   = 16
	EEXIST  = 17
	EXDEV   = 18
	ENODEV  = 19
	ENOTDIR = 20
	EISDIR  = 21
	EINVAL  = 22
	ENFILE  = 23
	EMFILE  = 24
	ENOTTY  = 25
	ETXTBSY = 26
	EFBIG   = 27
	ENOSPC  = 28
	ESPIPE  = 29
	EROFS   = 30
	EMLINK  = 31
	EPIPE   = 32
	EDOM    = 33
	ERANGE  = 34
	ENOSYS  = 38
)

//go:linkname syscall Syscall
//go:noescape
func syscall(num, a1, a2, a3, a4, a5, a6 uintptr) (r1, r2 uintptr)

//go:noescape
func asm_hlt()

func unreachable() {
	for {
		asm_hlt()
	}

// =============================================================================
// Process Management
// =============================================================================

// Exit terminates the current process
func Exit(code int) {
	syscall(SYS_EXIT, uintptr(code), 0, 0, 0, 0, 0, 0)
	unreachable()
}

// Fork creates a new process
func Fork() (int, error) {
	r1, _ := syscall(SYS_FORK, 0, 0, 0, 0, 0, 0, 0)
	if r1 == ^uintptr(0) {
		return 0, Errno(EAGAIN)
	}
	return int(r1), nil
}

// GetPID returns the current process ID
func GetPID() int {
	r1, _ := syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0, 0)
	return int(r1)
}

// Sleep sleeps for the given milliseconds
func Sleep(ms int) {
	syscall(SYS_SLEEP, uintptr(ms), 0, 0, 0, 0, 0, 0)
}

// Yield yields the CPU
func Yield() {
	syscall(SYS_YIELD, 0, 0, 0, 0, 0, 0, 0)
}

// Kill sends a signal to a process
func Kill(pid int, sig int) error {
	_, err := syscall(SYS_KILL, uintptr(pid), uintptr(sig), 0, 0, 0, 0, 0)
	if err != 0 {
		return Errno(err)
	}
	return nil
}

// Execve executes a new program
func Execve(path string, argv []string, envv []string) error {
	r1, _ := syscall(SYS_EXEC, uintptr(unsafe.Pointer(&path)), 0, 0, 0, 0, 0, 0)
	if r1 != 0 {
		return Errno(r1)
	}
	return nil
}

// Reboot reboots the system
func Reboot() error {
	_, err := syscall(SYS_REBOOT, 0, 0, 0, 0, 0, 0, 0)
	if err != 0 {
		return Errno(err)
	}
	return nil
}

// Shutdown shuts down the system
func Shutdown() error {
	_, err := syscall(SYS_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0)
	if err != 0 {
		return Errno(err)
	}
	return nil
}

// Uptime returns system uptime in seconds
func Uptime() int {
	r1, _ := syscall(SYS_UPTIME, 0, 0, 0, 0, 0, 0, 0)
	return int(r1)
}

// Time returns current time in seconds since epoch
func Time() int64 {
	r1, _ := syscall(SYS_TIME, 0, 0, 0, 0, 0, 0, 0)
	return int64(r1)
}

// =============================================================================
// File I/O
// =============================================================================

// Open opens a file
func Open(path string, flags int, mode uint32) (int, error) {
	r1, err := syscall(SYS_OPEN, uintptr(unsafe.Pointer(&path)), uintptr(flags), uintptr(mode), 0, 0, 0, 0)
	if err != 0 {
		return -1, Errno(err)
	}
	if int(r1) < 0 {
		return -1, Errno(^r1 + 1)
	}
	return int(r1), nil
}

// Close closes a file descriptor
func Close(fd int) error {
	_, err := syscall(SYS_CLOSE, uintptr(fd), 0, 0, 0, 0, 0, 0)
	if err != 0 {
		return Errno(err)
	}
	return nil
}

// Read reads from a file descriptor
func Read(fd int, buf []byte) (int, error) {
	if len(buf) == 0 {
		return 0, nil
	}
	r1, err := syscall(SYS_READ, uintptr(fd), uintptr(unsafe.Pointer(&buf[0])), uintptr(len(buf)), 0, 0, 0, 0)
	if err != 0 {
		return 0, Errno(err)
	}
	if int(r1) < 0 {
		return 0, Errno(^r1 + 1)
	}
	return int(r1), nil
}

// Write writes to a file descriptor
func Write(fd int, buf []byte) (int, error) {
	if len(buf) == 0 {
		return 0, nil
	}
	r1, err := syscall(SYS_WRITE, uintptr(fd), uintptr(unsafe.Pointer(&buf[0])), uintptr(len(buf)), 0, 0, 0, 0)
	if err != 0 {
		return 0, Errno(err)
	}
	if int(r1) < 0 {
		return 0, Errno(^r1 + 1)
	}
	return int(r1), nil
}

// =============================================================================
// Memory Management
// =============================================================================

// Mmap maps memory
func Mmap(addr uintptr, length uintptr, prot int, flags int, fd int, offset int64) (uintptr, error) {
	r1, err := syscall(SYS_MMAP, addr, length, uintptr(prot), uintptr(flags), uintptr(fd), uintptr(offset), 0)
	if err != 0 {
		return 0, Errno(err)
	}
	if r1 == MAP_FAILED {
		return 0, Errno(ENOMEM)
	}
	return r1, nil
}

// Munmap unmaps memory
func Munmap(addr uintptr, length uintptr) error {
	_, err := syscall(SYS_MUNMAP, addr, length, 0, 0, 0, 0, 0)
	if err != 0 {
		return Errno(err)
	}
	return nil
}

// Mprotect changes memory protection
func Mprotect(addr uintptr, length uintptr, prot int) error {
	_, err := syscall(SYS_MPROTECT, addr, length, uintptr(prot), 0, 0, 0, 0)
	if err != 0 {
		return Errno(err)
	}
	return nil
}

// =============================================================================
// GUI Syscalls (Phase 3: Go Shell -> Rust GUI)
// =============================================================================

// CreateSurface creates a new drawing surface via the kernel/Rust GUI.
// Returns a surface handle (pointer) or 0 on error.
// Implemented in syscall_amd64_tinygo.s
//go:noescape
func CreateSurface(width, height int) uintptr

// BlitSurface blits a surface to the framebuffer at the given position.
// Returns 0 on success, negative on error.
// Implemented in syscall_amd64_tinygo.s
//go:noescape
func BlitSurface(surface uintptr, x, y int) int32

// InvalidateWindow marks a window region for redraw.
// Returns 0 on success, negative on error.
// Implemented in syscall_amd64_tinygo.s
//go:noescape
func InvalidateWindow(windowID uint64, x, y, w, h int) int32

// GetFramebufferInfo returns framebuffer dimensions and pitch.
// Returns 0 on success, negative on error.
// Implemented in syscall_amd64_tinygo.s
//go:noescape
func GetFramebufferInfo(width, height, pitch *int) int32

// CreateWindow creates a new window
func CreateWindow(title string, x, y, w, h int) uint64 {
	r1, _ := syscall(SYS_GUI_CREATE_WINDOW, uintptr(unsafe.Pointer(&title)), uintptr(x), uintptr(y), uintptr(w), uintptr(h), 0, 0)
	return uint64(r1)
}

// DestroyWindow destroys a window
func DestroyWindow(windowID uint64) int32 {
	r1, _ := syscall(SYS_GUI_DESTROY_WINDOW, uintptr(windowID), 0, 0, 0, 0, 0, 0)
	return int32(r1)
}

// SetWindowTitle sets the window title
func SetWindowTitle(windowID uint64, title string) int32 {
	r1, _ := syscall(SYS_GUI_SET_TITLE, uintptr(windowID), uintptr(unsafe.Pointer(&title)), 0, 0, 0, 0, 0)
	return int32(r1)
}

// =============================================================================
// Input Syscalls
// =============================================================================

// MouseEvent represents a mouse event
type MouseEvent struct {
	Type      uint8  // 4=move, 5=down, 6=up, 7=scroll_v, 8=scroll_h
	X, Y      int32
	Button    uint8
	Delta     int32
	Timestamp uint64
}

// KeyEvent represents a keyboard event
type KeyEvent struct {
	Type      uint8  // 1=down, 2=up, 3=repeat
	Scancode  uint8
	ASCII     uint8
	Modifiers uint32
	Timestamp uint64
}

// GetMouseEvent gets the next mouse event (non-blocking)
// Returns true if an event was retrieved
func GetMouseEvent(ev *MouseEvent) bool {
	// This would need a syscall to get mouse events from the kernel
	// For now, return false (no events)
	return false
}

// GetKeyEvent gets the next keyboard event (non-blocking)
// Returns true if an event was retrieved
func GetKeyEvent(ev *KeyEvent) bool {
	// This would need a syscall to get keyboard events from the kernel
	// For now, return false (no events)
	return false
}

// PollEvents polls for input events
func PollEvents() int {
	r1, _ := syscall(SYS_GUI_POLL_EVENT, 0, 0, 0, 0, 0, 0, 0)
	return int(r1)
}

// GetMousePosition returns the current mouse position
func GetMousePosition() (int, int) {
	r1, r2 := syscall(SYS_INPUT_GET_MOUSE, 0, 0, 0, 0, 0, 0, 0)
	return int(r1), int(r2)
}

// SetCursor sets the mouse cursor
func SetCursor(cursorType int) error {
	_, err := syscall(SYS_INPUT_SET_CURSOR, uintptr(cursorType), 0, 0, 0, 0, 0, 0)
	if err != 0 {
		return Errno(err)
	}
	return nil
}

// =============================================================================
// Shared Memory
// =============================================================================

// ShmGet creates or opens a shared memory segment
func ShmGet(name string, size int) (int, error) {
	r1, err := syscall(SYS_SHMGET, uintptr(unsafe.Pointer(&name)), uintptr(size), 0, 0, 0, 0, 0)
	if err != 0 {
		return -1, Errno(err)
	}
	if int(r1) < 0 {
		return -1, Errno(^r1 + 1)
	}
	return int(r1), nil
}

// ShmCtl controls a shared memory segment
func ShmCtl(fd int, cmd int, arg uintptr) (int, error) {
	r1, err := syscall(SYS_SHMCTL, uintptr(fd), uintptr(cmd), arg, 0, 0, 0, 0)
	if err != 0 {
		return -1, Errno(err)
	}
	return int(r1), nil
}

// =============================================================================
// Console I/O
// =============================================================================

// PutChar writes a character to console
func PutChar(c byte) {
	syscall(SYS_PUTCHAR, uintptr(c), 0, 0, 0, 0, 0, 0)
}

// GetChar reads a character from console
func GetChar() byte {
	r1, _ := syscall(SYS_GETCHAR, 0, 0, 0, 0, 0, 0, 0)
	return byte(r1)
}

// WriteString writes a string to stdout
func WriteString(s string) (int, error) {
	return Write(1, unsafe.Slice(unsafe.StringData(s), len(s)))
}

// =============================================================================
// Printf-style output (minimal)
// =============================================================================

// Print prints to console
func Print(args ...interface{}) {
	for _, arg := range args {
		switch v := arg.(type) {
		case string:
			WriteString(v)
		case int:
			WriteString(itoa(v))
		case byte:
			PutChar(v)
		}
	}
}

// Println prints to console with newline
func Println(args ...interface{}) {
	Print(args...)
	PutChar('\n')
}

// Printf prints formatted string (minimal implementation)
func Printf(format string, args ...interface{}) {
	i := 0
	for i < len(format) {
		if format[i] == '%' && i+1 < len(format) {
			i++
			switch format[i] {
			case 's':
				if len(args) > 0 {
					if s, ok := args[0].(string); ok {
						WriteString(s)
					}
					args = args[1:]
				}
			case 'd':
				if len(args) > 0 {
					if n, ok := args[0].(int); ok {
						WriteString(itoa(n))
					}
					args = args[1:]
				}
			case 'c':
				if len(args) > 0 {
					if c, ok := args[0].(byte); ok {
						PutChar(c)
					}
					args = args[1:]
				}
			case 'x':
				if len(args) > 0 {
					if n, ok := args[0].(int); ok {
						WriteString(itox(n))
					}
					args = args[1:]
				}
			default:
				PutChar('%')
				PutChar(format[i])
			}
		} else {
			PutChar(format[i])
		}
		i++
	}
}

// itoa converts an integer to a string
func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	buf := make([]byte, 20)
	i := len(buf)
	neg := n < 0
	if neg {
		n = -n
	}
	for n > 0 {
		i--
		buf[i] = byte('0' + n%10)
		n /= 10
	}
	if neg {
		i--
		buf[i] = '-'
	}
	return string(buf[i:])
}

// itox converts an integer to hex string
func itox(n int) string {
	if n == 0 {
		return "0"
	}
	buf := make([]byte, 16)
	i := len(buf)
	for n > 0 {
		i--
		d := n & 0xf
		if d < 10 {
			buf[i] = byte('0' + d)
		} else {
			buf[i] = byte('a' + d - 10)
		}
		n >>= 4
	}
	return string(buf[i:])
}

// =============================================================================
// Error handling
// =============================================================================

// Errno represents a system call error
type Errno uintptr

func (e Errno) Error() string {
	return "syscall error " + itoa(int(e))
}