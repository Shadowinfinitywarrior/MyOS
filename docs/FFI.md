# MyOS FFI Contract & Cross-Language Boundary Specification

## Overview
This specification formally defines the Foreign Function Interface (FFI) and cross-language boundaries between C (kernel core & drivers), Rust (GUI core & window manager), Go/TinyGo (desktop shell), Java (GraalVM AOT apps), and userspace ELF binaries.

## Architectural Boundary Invariants
1. **ABI Conventions**: All cross-language functions adhere strictly to the System V AMD64 ABI (`extern "C"`).
2. **Memory Layout**: Cross-language structs must be declared `#[repr(C)]` in Rust, `__attribute__((packed))` or standard C alignment in C/C++, and cgo-compatible in Go.
3. **Boundary Crossing**: Ring 0 / Ring 3 boundary is strictly crossed via `SYSCALL` / `SYSRET` with versioned ABI validation.
4. **Ownership & Pointers**:
   - Memory ownership transfers ONLY at syscall boundaries.
   - The kernel never holds pointers to userspace memory across syscall return.
   - Userspace never holds or dereferences pointers to kernel memory.
   - Opaque handles (`uint64_t`) are used for all cross-boundary object references (windows, surfaces, IPC channels).
5. **Exception Safety**: No C++ or language runtime exceptions across the C boundary.
6. **Concurrency**: Go userspace uses `-scheduler=none` and does not interleave preemptive goroutines across blocking kernel calls without synchronization.
7. **Rust in Kernel**: Any Rust kernel components must be compiled `#![no_std]` without panics, unwinding, or uncontrolled allocations.
8. **Header Synchronization**: All FFI functions are declared in `include/rust_gui.h` and Rust `ffi.rs` with byte-identical signatures.
