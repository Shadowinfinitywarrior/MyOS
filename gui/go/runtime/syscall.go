//go:build !tinygo
// +build !tinygo

// Syscall wrappers for Go shell to communicate with MyOS kernel/Rust GUI
// This file provides implementations for Linux (testing)

package runtime

import (
	"unsafe"
)

// Syscall numbers (must match kernel/syscall.h)
const (
	SYS_GUI_CREATE_SURFACE = 40
	SYS_GUI_BLIT_SURFACE   = 41
	SYS_GUI_INVALIDATE     = 42
	SYS_GUI_GET_FB_INFO    = 43
	SYS_YIELD              = 11
)

// Surface represents a drawing surface
type Surface struct {
	Pixels unsafe.Pointer
	Width  int
	Height int
	Pitch  int
}

// CreateSurface creates a new drawing surface via the kernel/Rust GUI.
// Returns a surface handle (pointer) or 0 on error.
func CreateSurface(width, height int) uintptr {
	// For Linux testing, allocate memory directly
	// In bare metal (TinyGo), this would be implemented in syscall_amd64.s
	size := width * height * 4
	pixels := unsafe.Pointer(&make([]byte, size)[0])
	return uintptr(unsafe.Pointer(&Surface{
		Pixels: pixels,
		Width:  width,
		Height: height,
		Pitch:  width * 4,
	}))
}

// BlitSurface blits a surface to the framebuffer at the given position.
func BlitSurface(surface uintptr, x, y int) int32 {
	// For Linux testing, this is a no-op
	// In bare metal, this would call the kernel syscall
	_ = surface
	_ = x
	_ = y
	return 0
}

// InvalidateWindow marks a window region for redraw.
func InvalidateWindow(windowID uint64, x, y, w, h int) int32 {
	// For Linux testing, this is a no-op
	_ = windowID
	_ = x
	_ = y
	_ = w
	_ = h
	return 0
}

// GetFramebufferInfo returns framebuffer dimensions and pitch.
func GetFramebufferInfo(width, height, pitch *int) int32 {
	// Default to 1024x768 for testing
	*width = 1024
	*height = 768
	*pitch = 1024 * 4
	return 0
}

// Yield yields the CPU to other processes
func Yield() {
	// For Linux testing, use Gosched
	// In bare metal, this would be a syscall
}

// Gosched yields the CPU (alias for Yield)
func Gosched() {
	Yield()
}

// GetSurface returns the surface from a handle
func GetSurface(handle uintptr) *Surface {
	return (*Surface)(unsafe.Pointer(handle))
}

// SetSurfacePixels sets the pixel data for a surface
func SetSurfacePixels(handle uintptr, pixels []byte) {
	surf := GetSurface(handle)
	if surf != nil && surf.Pixels != nil {
		dst := unsafe.Slice((*byte)(surf.Pixels), surf.Width*surf.Height*4)
		copy(dst, pixels)
	}
}

// GetMousePosition returns the current mouse position
// For testing, returns center of screen
func GetMousePosition() (int, int) {
	return 512, 384
}