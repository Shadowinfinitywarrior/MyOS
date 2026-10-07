// gc.go - Bare-metal garbage collector for MyOS Go shell
// Implements a "leaking" allocator that never frees memory, using kernel mmap syscall.
// This is suitable for a desktop shell that runs for the session lifetime.

package runtime

import (
	"unsafe"
)

// Syscall numbers (must match kernel/syscall.h)
const (
	SYS_MMAP     = 14
	SYS_MUNMAP   = 15
	SYS_MPROTECT = 16
	SYS_EXIT     = 1
)

// mmap flags
const (
	PROT_READ  = 0x1
	PROT_WRITE = 0x2
	PROT_EXEC  = 0x4
	MAP_SHARED = 0x01
	MAP_PRIVATE = 0x02
	MAP_ANONYMOUS = 0x20
	MAP_FIXED  = 0x10
	MAP_FAILED = ^uintptr(0)
)

//go:linkname syscall Syscall
//go:noescape
func syscall(num, a1, a2, a3, a4, a5, a6 uintptr) (r1, r2 uintptr)

// allocm allocates memory using the kernel mmap syscall.
// This is called by the TinyGo runtime for heap allocations.
// The allocator never frees memory (leaking allocator).
func allocm(size uintptr) unsafe.Pointer {
	if size == 0 {
		return unsafe.Pointer(uintptr(0x1000)) // Non-null for zero size
	}

	// Align to page size (4KB)
	const pageSize = 4096
	alignedSize := (size + pageSize - 1) &^ (pageSize - 1)

	// mmap(addr=0, length, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, fd=-1, offset=0)
	r1, _ := syscall(SYS_MMAP,
		0,                              // addr (0 = kernel chooses)
		alignedSize,                    // length
		PROT_READ|PROT_WRITE,           // prot
		MAP_PRIVATE|MAP_ANONYMOUS,      // flags
		^uintptr(0),                    // fd (-1)
		0,                              // offset
		0)

	if r1 == MAP_FAILED || r1 == 0 {
		// Out of memory - halt
		asm_hlt()
		unreachable()
	}

	return unsafe.Pointer(r1)
}

// freem is a no-op for the leaking allocator.
// Memory is never returned to the kernel.
func freem(ptr unsafe.Pointer, size uintptr) {
	// Intentionally leak - memory will be reclaimed when process exits
	// For a desktop shell session, this is acceptable
	_ = ptr
	_ = size
}

// reallocm reallocates memory by allocating new and copying.
// The old memory is leaked.
func reallocm(oldPtr unsafe.Pointer, oldSize, newSize uintptr) unsafe.Pointer {
	if newSize == 0 {
		return unsafe.Pointer(uintptr(0x1000))
	}

	newPtr := allocm(newSize)
	if oldPtr != nil && oldSize > 0 {
		// Copy old data to new allocation
		copy((*[1 << 30]byte)(newPtr))[:newSize], (*[1 << 30]byte)(oldPtr))[:oldSize])
	}
	// Old memory is leaked intentionally
	return newPtr
}

// memclrNoHeapPointers clears memory without write barriers.
//go:nosplit
func memclrNoHeapPointers(ptr unsafe.Pointer, n uintptr) {
	// Use memclr for zeroing
	memclr(ptr, n)
}

// memmove copies memory.
//go:nosplit
func memmove(dst, src unsafe.Pointer, n uintptr) {
	// Standard memory copy
	if dst == src || n == 0 {
		return
	}
	dstPtr := (*[1 << 30]byte)(dst)
	srcPtr := (*[1 << 30]byte)(src)
	copy(dstPtr[:n], srcPtr[:n])
}

// mallocgc is the main allocation entry point for TinyGo.
//go:nosplit
func mallocgc(size uintptr, typ *uint8, needzero bool) unsafe.Pointer {
	if size == 0 {
		return unsafe.Pointer(uintptr(0x1000))
	}

	ptr := allocm(size)
	if needzero {
		memclrNoHeapPointers(ptr, size)
	}
	return ptr
}

// newobject allocates a new object of the given type.
//go:nosplit
func newobject(typ *uint8) unsafe.Pointer {
	// For TinyGo, we don't have full type info, so use a default size
	return mallocgc(64, typ, true)
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

// MemStats returns memory statistics (stub for runtime/debug)
type MemStats struct {
	Alloc       uint64
	TotalAlloc  uint64
	Sys         uint64
	Mallocs     uint64
	Frees       uint64
	HeapAlloc   uint64
	HeapSys     uint64
	HeapIdle    uint64
	HeapInuse   uint64
	HeapReleased uint64
	HeapObjects uint64
}

// ReadMemStats fills in memory statistics (always zero for leaking allocator)
func ReadMemStats(m *MemStats) {
	// Leaking allocator doesn't track stats
	*m = MemStats{}
}