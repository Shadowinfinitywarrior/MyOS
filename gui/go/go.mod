module myos

go 1.21

// MyOS Go Shell - Bare metal desktop shell
// Uses TinyGo with custom runtime stubs (scheduler=none, gc=conservative)
// Target: x86_64-unknown-none (bare metal)

// Build with: tinygo build -target=x86_64-unknown-none -scheduler=none -gc=conservative -o shell.elf ./shell

// Toolchain requirements:
// - TinyGo 0.30+
// - LLVM 15+ (for x86_64 codegen)

require (
	// No external dependencies for bare metal
)

// Build tags for conditional compilation:
// - tinygo: for TinyGo-specific code
// - baremetal: for bare metal (no OS) code
// - myos: for MyOS-specific code