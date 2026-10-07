//go:build tinygo
// +build tinygo

// Assembly implementations of GUI syscall wrappers for Go
// This file provides the actual syscall implementations that the Go code calls.
// Only used when building with TinyGo for bare metal.

#include "textflag.h"

// func CreateSurface(width, height int) uintptr
TEXT ·CreateSurface(SB), $0-24
	MOVQ width+0(FP), AX
	MOVQ height+8(FP), BX
	MOVL $40, CX        // SYS_GUI_CREATE_SURFACE
	SYSCALL
	MOVQ AX, ret+16(FP)
	RET

// func BlitSurface(surface uintptr, x, y int) int32
TEXT ·BlitSurface(SB), $0-32
	MOVQ surface+0(FP), AX
	MOVQ x+8(FP), BX
	MOVQ y+16(FP), CX
	MOVL $41, DX        // SYS_GUI_BLIT_SURFACE
	SYSCALL
	MOVL AX, ret+24(FP)
	RET

// func InvalidateWindow(windowID uint64, x, y, w, h int) int32
TEXT ·InvalidateWindow(SB), $0-48
	MOVQ windowID+0(FP), AX
	MOVQ x+8(FP), BX
	MOVQ y+16(FP), CX
	MOVQ w+24(FP), DX
	MOVQ h+32(FP), SI
	MOVL $42, DI        // SYS_GUI_INVALIDATE
	SYSCALL
	MOVL AX, ret+40(FP)
	RET

// func GetFramebufferInfo(width, height, pitch *int) int32
TEXT ·GetFramebufferInfo(SB), $0-32
	MOVQ width+0(FP), AX
	MOVQ height+8(FP), BX
	MOVQ pitch+16(FP), CX
	MOVL $43, DX        // SYS_GUI_GET_FB_INFO
	SYSCALL
	MOVL AX, ret+24(FP)
	RET

// func asm_hlt()
TEXT ·asm_hlt(SB), $0-0
	HLT
	RET

// syscall_errno for error handling
DATA ·syscall_errno(SB)/8, $0
GLOBL ·syscall_errno(SB), NOPTR, $8