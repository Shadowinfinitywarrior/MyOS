//go:build !tinygo
// +build !tinygo

// Minimal runtime for bare-metal Go shell
// This package provides the bare minimum for a Go program to run without the standard library.

package runtime

// The Go compiler expects these symbols to exist
var (
	// Dummy variables to satisfy the linker
	_ = 0
)

// asm_hlt is a placeholder for the hlt instruction
func asm_hlt() {}

// unreachable marks code that should never be reached
func unreachable() {
	for {
		asm_hlt()
	}
}

// errorString converts an error code to a string
func errorString(err uintptr) string {
	return "syscall error"
}