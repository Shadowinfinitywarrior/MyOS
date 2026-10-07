# MyOS Complete Multi-Language Implementation Plan

## Overview
Complete the 4-phase migration from C-only to multi-language OS (C/C++/Rust/Go/Java/Python) with parallel agent execution.

---

## Phase 2: Userspace Foundation (Kernel Core)
**Agent: kernel-specialist**

### 2.1 Syscall ABI Fix (SysV Convention)
- [ ] Update `kernel/syscall64.c` to use SysV ABI (RDI, RSI, RDX, R10, R8, R9)
- [ ] Fix `kernel/syscall.c` argument handling
- [ ] Update all syscall entry points in assembly
- [ ] Document all syscalls in `docs/ABI/SYSCALLS.md`

### 2.2 Ring-3 Process Isolation
- [ ] Implement per-process CR3 switching in `kernel/paging.c`
- [ ] Set up TSS.RSP0 for kernel stack per process
- [ ] Enable ring-3 syscall/sysret gate with user segments (CS=0x23, DS=0x2B)
- [ ] Implement process file descriptor tables in `kernel/process.c`
- [ ] Add process signal handling infrastructure

### 2.3 Virtual Memory Management
- [ ] Complete `mmap` syscall (MAP_ANONYMOUS, MAP_PRIVATE, MAP_SHARED)
- [ ] Implement `munmap` syscall
- [ ] Add page fault handler with copy-on-write for fork
- [ ] Implement lazy allocation and `mprotect` syscall
- [ ] User-space heap management via mmap

### 2.4 Shared Memory for Graphics
- [ ] Implement `shm_open`/`shm_unlink` syscalls
- [ ] Create framebuffer as shared memory region
- [ ] Map framebuffer into user processes via mmap
- [ ] Zero-copy blitting between surfaces

### 2.5 Userspace C Library
- [ ] Implement libc syscall wrappers in `user/libc/`
- [ ] Add pthread support with kernel threads
- [ ] Implement malloc/free using mmap
- [ ] Port existing user programs to new ABI

---

## Phase 3: Go/TinyGo Desktop Shell
**Agent: go-specialist**

### 3.1 TinyGo Toolchain Setup
- [ ] Install TinyGo 0.42+ with `x86_64-unknown-none` target
- [ ] Create `gui/go/shell/go.mod`
- [ ] Configure build with `-scheduler=none -gc=conservative`
- [ ] Add TinyGo rules to Makefile

### 3.2 Bare-Metal Runtime Stubs (`gui/go/runtime/`)
- [ ] `gc.go` - Leaking allocator using kernel mmap
- [ ] `thread.go` - Disable goroutines, use kernel threads
- [ ] `syscall.go` - Syscall wrappers for graphics/input
- [ ] `time.go` - Timer functions using kernel timer

### 3.3 Desktop Shell Framework (`gui/go/shell/`)
- [ ] `main.go` - Entry point, initialize Rust GUI connection
- [ ] `widget.go` - Base widget type with layout/events
- [ ] `layout.go` - Flex/grid layout managers
- [ ] `event.go` - Input event handling

### 3.4 Shell Components
- [ ] `taskbar.go` - Glass effect, start button, window buttons, system tray
- [ ] `menu.go` - Start menu with search, app list, icons
- [ ] `desktop.go` - Icon grid, drag-drop, positioning
- [ ] `notifications.go` - Toast system with animations

### 3.5 Rust GUI Integration
- [ ] Define syscalls: `sys_gui_create_surface`, `sys_gui_blit_surface`, `sys_gui_invalidate`
- [ ] Implement Go syscall wrappers
- [ ] Batch operations for performance
- [ ] Zero-copy surface sharing

---

## Phase 4: Java/GraalVM Applications
**Agent: java-specialist**

### 4.1 GraalVM Setup
- [ ] Install GraalVM CE 22.3+ with Native Image
- [ ] Create `user/java/toolkit/` minimal GUI toolkit
- [ ] Design `NativeGUI.java` bridge to Rust GUI via syscalls

### 4.2 Java GUI Toolkit (`user/java/toolkit/`)
- [ ] `Window.java` - Surface management, event loop
- [ ] `Widget.java` - Base class with layout
- [ ] `Button.java`, `Label.java`, `TextField.java` - Core widgets
- [ ] `LayoutManager.java` - Flow/Grid/Border layouts
- [ ] Event system with callbacks

### 4.3 File Manager (`user/java/files/`)
- [ ] Directory tree view with icons
- [ ] File list/grid view with thumbnails
- [ ] Path bar, action buttons (copy/move/delete)
- [ ] Progress dialogs for operations
- [ ] File type icons (using Blocks font)

### 4.4 Terminal Emulator (`user/java/terminal/`)
- [ ] PTY handling via syscalls
- [ ] ANSI escape sequence parser
- [ ] Color support (16/256/true color)
- [ ] Scrollback buffer, copy/paste
- [ ] Font scaling, tab support

### 4.5 Native Image Build
- [ ] Configure reflection/resource configs
- [ ] Static linking, symbol stripping
- [ ] Embed binaries in kernel via Makefile
- [ ] App registry integration

---

## Phase 5: Python/PyQt Integration
**Agent: python-specialist**

### 5.1 Python Runtime
- [ ] Embed MicroPython or CPython 3.11+ (no_std compatible)
- [ ] Implement Python C API shim for kernel syscalls
- [ ] Create `user/python/` directory structure

### 5.2 PyQt Bindings
- [ ] Minimal Qt-like API mapping to Rust GUI
- [ ] `QApplication`, `QWidget`, `QLayout` equivalents
- [ ] Signal/slot mechanism via Rust callbacks
- [ ] Paint events using Rust renderer

### 5.3 Python Applications
- [ ] `user/python/apps/` - Settings, calculator, text editor
- [ ] Package management via embedded pip
- [ ] Script execution from shell

---

## Cross-Language Integration
**Agent: integration-specialist**

### IPC & Shared Memory
- [ ] Unified surface protocol (MYDP) for all languages
- [ ] Shared memory regions for framebuffers
- [ ] Event bus for cross-process communication
- [ ] Capability-based security model

### Build System
- [ ] Complete multi-language Makefile
- [ ] Parallel build support (`MAKEFLAGS += -j$(nproc)`)
- [ ] Incremental rebuild per language
- [ ] CI/CD pipeline with all toolchains

### Testing
- [ ] Unit tests per language
- [ ] Integration tests (Rust↔Go↔Java↔Python)
- [ ] Visual regression tests in QEMU
- [ ] Performance benchmarks

---

## Agent Assignments

| Agent | Responsibility | Key Files |
|-------|---------------|-----------|
| kernel-specialist | Phase 2: Kernel userspace | kernel/*.c, include/ |
| go-specialist | Phase 3: Go shell | gui/go/*, Makefile |
| java-specialist | Phase 4: Java apps | user/java/*, Makefile |
| python-specialist | Phase 5: Python/PyQt | user/python/*, Makefile |
| integration-specialist | Cross-cutting | Makefile, docs/, tests/ |

---

## Success Criteria

1. ✅ Phase 1: Rust GUI core (COMPLETED)
2. 🔄 Phase 2: Ring-3 userspace with fixed ABI
3. 🔄 Phase 3: Go desktop shell boots and renders
4. 🔄 Phase 4: Java file manager + terminal run
5. 🔄 Phase 5: Python apps launch from shell
6. 🔄 All languages share graphics surfaces
7. 🔄 Single `make` builds everything
8. 🔄 Boots in QEMU with full desktop