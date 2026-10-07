// thread.go - Bare-metal thread/runtime support for MyOS Go shell
// Disables goroutines and uses kernel threads directly via syscalls.
// This is a "scheduler=none" runtime for TinyGo.

package runtime

import (
	"unsafe"
)

// Syscall numbers
const (
	SYS_FORK    = 2
	SYS_EXIT    = 1
	SYS_YIELD   = 11
	SYS_SLEEP   = 10
	SYS_GETPID  = 9
	SYS_KILL    = 12
	SYS_MMAP    = 14
	SYS_MUNMAP  = 15
)

// Maximum number of kernel threads we can track
const maxThreads = 32

// thread represents a kernel thread
type thread struct {
	id     int32
	stack  unsafe.Pointer
	stackSize uintptr
	entry  func()
	arg    unsafe.Pointer
	active bool
}

// Thread table
var threads [maxThreads]thread
var threadCount int32
var currentThread *thread

// threadEntry is the assembly entry point for new threads
//go:noescape
func threadEntry()

// initThreading initializes the threading subsystem
func initThreading() {
	// Main thread is already running
	threads[0].id = 1 // PID 1 for main thread
	threads[0].active = true
	currentThread = &threads[0]
	threadCount = 1
}

// Gosched yields the CPU to another thread/process
//go:nosplit
func Gosched() {
	// Yield to kernel scheduler
	syscall(SYS_YIELD, 0, 0, 0, 0, 0, 0, 0)
}

// ExitThread exits the current thread
func ExitThread(code int) {
	syscall(SYS_EXIT, uintptr(code), 0, 0, 0, 0, 0, 0)
	unreachable()
}

// GetThreadID returns the current thread ID
func GetThreadID() int32 {
	if currentThread != nil {
		return currentThread.id
	}
	return 0
}

// CreateThread creates a new kernel thread
// The entry function runs in the new thread context
func CreateThread(entry func(), stackSize uintptr) int32 {
	if threadCount >= maxThreads {
		return -1
	}

	// Allocate stack for the new thread
	if stackSize == 0 {
		stackSize = 64 * 1024 // 64KB default
	}
	stack := allocm(stackSize)
	if stack == nil {
		return -1
	}

	// Find free slot
	var idx int32 = -1
	for i := int32(1); i < maxThreads; i++ {
		if !threads[i].active {
			idx = i
			break
		}
	}
	if idx == -1 {
		freem(stack, stackSize)
		return -1
	}

	// Create thread via fork syscall
	// The child will run threadEntry which calls the entry function
	pid, _ := syscall(SYS_FORK, 0, 0, 0, 0, 0, 0, 0)
	if pid == 0 {
		// Child process - this is the new thread
		// Set up thread state and call entry
		threads[idx].id = int32(syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0, 0))
		threads[idx].stack = stack
		threads[idx].stackSize = stackSize
		threads[idx].entry = entry
		threads[idx].active = true
		currentThread = &threads[idx]

		// Call the user entry function
		entry()

		// Thread function returned - exit
		ExitThread(0)
	}

	// Parent process - track the thread
	threads[idx].id = int32(pid)
	threads[idx].stack = stack
	threads[idx].stackSize = stackSize
	threads[idx].entry = entry
	threads[idx].active = true
	threadCount++

	return threads[idx].id
}

// KillThread terminates a thread by ID
func KillThread(tid int32) int32 {
	for i := int32(0); i < threadCount; i++ {
		if threads[i].active && threads[i].id == tid {
			_, err := syscall(SYS_KILL, uintptr(tid), 9, 0, 0, 0, 0, 0) // SIGKILL = 9
			if err == 0 {
				freem(threads[i].stack, threads[i].stackSize)
				threads[i].active = false
				threadCount--
				return 0
			}
			return int32(err)
		}
	}
	return -1
}

// Sleep puts the current thread to sleep for the given milliseconds
func Sleep(ms uint32) {
	syscall(SYS_SLEEP, uintptr(ms), 0, 0, 0, 0, 0, 0)
}

// Yield voluntarily yields the CPU
func Yield() {
	Gosched()
}

// LockOSThread locks the current goroutine to its OS thread (no-op for scheduler=none)
//go:nosplit
func LockOSThread() {
	// No-op: we have 1:1 mapping of goroutines to kernel threads
}

// UnlockOSThread unlocks the current goroutine from its OS thread (no-op)
//go:nosplit
func UnlockOSThread() {
	// No-op
}

// GOMAXPROCS returns the maximum number of CPUs that can execute simultaneously
func GOMAXPROCS(n int) int {
	// We're single-threaded in the Go sense, but kernel handles SMP
	return 1
}

// NumCPU returns the number of logical CPUs
func NumCPU() int {
	// Query from kernel or return 1
	return 1
}

// SetMaxStack sets the maximum stack size (no-op)
func SetMaxStack(in int) int {
	return 0
}

// StackGuard returns the stack guard value
func StackGuard() uintptr {
	return 0
}

// asm_hlt executes the HLT instruction
//go:noescape
func asm_hlt()

// unreachable marks code that should never be reached
func unreachable() {
	for {
		asm_hlt()
	}
}

// syscall is the low-level syscall wrapper
//go:linkname syscall Syscall
//go:noescape
func syscall(num, a1, a2, a3, a4, a5, a6 uintptr) (r1, r2 uintptr)