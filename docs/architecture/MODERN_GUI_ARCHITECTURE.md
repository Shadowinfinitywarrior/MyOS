# Modern GUI Stack Architecture - Multi-Language Design

## Executive Summary

This document specifies the multi-language GUI architecture for MyOS. The system transitions from a monolithic C graphics implementation to a modular, multi-language stack leveraging **Rust**, **Go (TinyGo)**, **Java (GraalVM Native Image)**, and **Python (MicroPython)** to achieve memory safety, rapid user interface development, and rich desktop applications.

---

## 1. Language Roles

### 1.1 Rust: Kernel-Level GUI & Compositor
**Role**: Core 2D rendering engine, window management, compositor, input handling, and font rasterization.
**Execution Level**: Ring 0 (bare-metal `no_std` static library linked into `kernel.elf`).
**Key Capabilities**:
- Zero-cost memory safety in graphics processing and pointer manipulation.
- High-efficiency software rasterizer with clipping, gradients, and alpha blending.
- Window hierarchy management with z-ordering, damage tracking, and decorations.
- Pre-baked DejaVu font rasterization across 4 faces (UI, Mono, Bold, Blocks).
- Direct framebuffer access via `gui/rust/src/renderer.rs`.

**Components (`gui/rust/src/`)**:
- `lib.rs`: Module re-exports and C FFI entry points (`rust_gui_*`).
- `renderer.rs`: Primitive drawing, shapes, gradients, and surface blitting.
- `window.rs`: Window manager, decorations, focus, and hit testing.
- `input.rs`: Atomic circular event queue (keyboard scancodes and mouse events).
- `font.rs`: Glyph parsing and text rendering with clipping.
- `desktop.rs`: Integrated desktop environment (wallpaper, taskbar, start menu).

### 1.2 Go / TinyGo: Desktop Shell & System Services
**Role**: Userspace desktop shell, application launcher, taskbar widgets, and system indicators.
**Execution Level**: Ring 3 userspace process or bare-metal embedded service.
**Key Capabilities**:
- TinyGo compiles with `-scheduler=none -gc=conservative` for minimalist resource footprint.
- Clean high-level syntax for desktop layouts and widget logic.
- Communicates with the kernel and Rust GUI via system calls (`sys_gui_create_surface`, `sys_gui_blit_surface`, `sys_gui_invalidate`).

**Components (`gui/go/`)**:
- `shell/main.go`: Entry point, screen sizing, shell state loop.
- `shell/taskbar.go`: Glass-effect bottom taskbar with window buttons and clock.
- `shell/menu.go`: Animated start menu with application categorization.
- `shell/desktop.go`: Desktop icon grid with drag-and-drop calculation.
- `shell/widget.go`: Toast notification daemon and system indicators.
- `shell/graphics/`: Surface abstraction wrapping MyOS GUI syscalls.
- `runtime/`: Bare-metal TinyGo runtime shims (`gc.go`, `syscall.go`, `thread.go`, `time_tinygo.go`).

### 1.3 Java / GraalVM: Complex Desktop Applications
**Role**: Enterprise desktop applications including File Manager, Terminal Emulator, and Developer Tools.
**Execution Level**: Ring 3 userspace process compiled Ahead-Of-Time (AOT) to native ELF binaries via GraalVM Native Image.
**Key Capabilities**:
- Instant startup time and deterministic memory footprint without JVM startup overhead.
- Standalone static native binaries embedded into the kernel RamFS (`/bin/files`, `/bin/terminal`).
- Lightweight Java GUI toolkit (`user/java/toolkit/`) providing an AWT/Swing-like component hierarchy without heavyweight dependencies.

**Components (`user/java/`)**:
- `toolkit/`: Core GUI widget toolkit (`Component`, `Container`, `Window`, `Button`, `Label`, `Graphics`, `Color`, `Font`, `BorderLayout`, `FlowLayout`).
- `myos/binding/`: JNI/Syscall bridge (`NativeGUI.java`, `Syscall.java`) interfacing with MyOS kernel graphics syscalls.
- `files/Main.java`: Graphical File Manager with directory tree, file list, and path navigation.
- `terminal/`: Advanced Terminal Emulator with ANSI escape sequence parser (`ANSIParser.java`, `TerminalColors.java`).

### 1.4 Python / MicroPython: Scripting & Rapid Utility Applications
**Role**: System utilities, settings panels, calculators, and automation scripts.
**Execution Level**: Embedded MicroPython engine with custom MyOS HAL port (`libpymyos.a`).
**Key Capabilities**:
- Lightweight embedded Python runtime (<100KB footprint) executing with direct kernel syscall shims.
- Qt-compatible API bindings mapping Python widget classes to MyOS Rust GUI rendering primitives.
- Fast iteration for lightweight desktop accessories.

**Components (`user/python/`)**:
- `runtime/`: MicroPython HAL (`mphalport.c`, `pymyos.c`, `mpconfigport.h`).
- `qt/`: PyQt-like widget compatibility wrapper.
- `apps/`: Bundled Python desktop apps (`calculator.py`, `settings.py`, `text_editor.py`).

---

## 2. Architecture Diagram

```
┌────────────────────────────────────────────────────────────────────────┐
│                           Hardware Layer                               │
│  Framebuffer (BGA / VirtIO-GPU / VESA) │ PS/2 & USB HID │ ACPI / APIC  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│                    Ring 0: Rust Kernel GUI Core                        │
│ ┌───────────────┐ ┌───────────────┐ ┌───────────────┐ ┌──────────────┐ │
│ │   Renderer    │ │Window Manager │ │ Input Engine  │ │ Font Engine  │ │
│ │ (Primitives & │ │  (Z-Order &   │ │ (Event Queue  │ │ (DejaVu TTF  │ │
│ │  Gradients)   │ │ Decorations)  │ │  & Routing)   │ │   Bitmaps)   │ │
│ └───────┬───────┘ └───────┬───────┘ └───────┬───────┘ └───────┬──────┘ │
│         └─────────────────┼─────────────────┼─────────────────┘        │
│                           ▼                 ▼                          │
│               Desktop Compositor (Wallpaper, Taskbar, Menu)            │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                    ┌───────────────┴───────────────┐
                    │ System Call ABI (SysV AMD64)  │
                    │  SYS_GUI_* / SYS_IPC_* / MYDP │
                    └───────────────┬───────────────┘
                                    │
        ┌───────────────────────────┼───────────────────────────┐
        │                           │                           │
┌───────▼───────────────┐ ┌─────────▼─────────────┐ ┌───────────▼──────────┐
│  Go Desktop Shell     │ │  Java Applications    │ │ Python / Qt Apps     │
│  (TinyGo Userspace)   │ │  (GraalVM AOT Native) │ │ (MicroPython Engine) │
│ ┌───────────────────┐ │ │ ┌───────────────────┐ │ │ ┌──────────────────┐ │
│ │ Taskbar & Tray    │ │ │ │ File Manager      │ │ │ │ Calculator       │ │
│ │ Start Menu        │ │ │ │ Terminal Emulator │ │ │ │ System Settings  │ │
│ │ Desktop Icons     │ │ │ │ Java GUI Toolkit  │ │ │ │ Text Editor      │ │
│ │ Notifications     │ │ │ └───────────────────┘ │ │ └──────────────────┘ │
│ └───────────────────┘ │ └───────────────────────┘ └──────────────────────┘
└───────────────────────┘
```

---

## 3. Inter-Language Communication & Protocols

### 3.1 C↔Rust FFI Bridge
The kernel invokes Rust GUI services through static C bindings defined in `include/rust_gui.h`:
- `rust_gui_init(width, height)`: Allocates window buffers and sets display geometry.
- `rust_gui_set_framebuffer(fb, w, h, pitch)`: Configures hardware backbuffer address.
- `rust_gui_create_window(title, x, y, w, h)`: Creates a managed window instance.
- `rust_gui_render_frame()`: Composites active windows in z-order onto the backbuffer.
- `rust_gui_push_key_event()` / `rust_gui_push_mouse_event()`: Injects hardware events.

### 3.2 System Call ABI (Ring 3 to Ring 0)
Userspace processes interact with the display subsystem via standard `syscall` instructions using the System V AMD64 ABI:
- Arguments: `RAX` (syscall number), `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9`.
- Key GUI System Calls:
  - `SYS_GUI_CREATE_SURFACE` (40): Allocates an offscreen drawing surface.
  - `SYS_GUI_BLIT_SURFACE` (41): Transfers surface contents to the target screen location.
  - `SYS_GUI_INVALIDATE` (42): Schedules dirty regions for recompositing.
  - `SYS_GUI_GET_FB_INFO` (43): Queries screen width, height, and pitch.
  - `SYS_GUI_CREATE_WINDOW` (51): Creates a userspace-owned window.
  - `SYS_GUI_RENDER_FRAME` (53): Requests full compositor frame render.

### 3.3 MYDP (MyOS Display Protocol)
For advanced client-compositor interaction, MyOS defines MYDP (`include/mydp/protocol.h`):
- Magic Header: `MYDP_MAGIC = 0x4D5950`, Version 1.
- Event Model: Asynchronous request/response structure over named message ports or domain sockets (`/tmp/mydp`).
- Message Types:
  - `MYDP_HELLO` / `MYDP_HELLO_REPLY`
  - `MYDP_CREATE_SURFACE` / `MYDP_SURFACE_CREATED`
  - `MYDP_ATTACH_BUFFER` / `MYDP_COMMIT` / `MYDP_DAMAGE`
  - `MYDP_KEYBOARD_EVENT` / `MYDP_POINTER_EVENT`
  - `MYDP_FRAME_DONE` / `MYDP_CLOSE_SURFACE`

---

## 4. File Structure

```
myos/
├── boot/                        # BIOS & UEFI bootloaders
├── kernel/                      # Ring 0 C kernel core
│   ├── kernel.c                 # Early kernel initialization
│   ├── init_phase8.c            # Phase 8 subsystem & desktop boot
│   ├── syscall64.c              # SYSCALL/SYSRET MSR configuration
│   ├── syscall.c                # System call dispatcher and handlers
│   ├── ipc.c                    # Named ports, message queues, capabilities
│   └── shm.c                    # Shared memory for zero-copy graphics
├── gui/
│   ├── rust/                    # Rust GUI Core (no_std static library)
│   │   ├── Cargo.toml           # Package manifest
│   │   ├── .cargo/config.toml   # Bare-metal linker flags
│   │   └── src/
│   │       ├── lib.rs           # FFI exports & API re-exports
│   │       ├── renderer.rs      # Drawing primitives & software rasterizer
│   │       ├── window.rs        # Window manager & decorations
│   │       ├── input.rs         # Event queue & hardware event routing
│   │       ├── font.rs          # Glyph parser & text rendering
│   │       └── desktop.rs       # Wallpaper, taskbar, start menu
│   ├── go/                      # Go Desktop Shell (TinyGo)
│   │   ├── go.mod               # Go module definition
│   │   ├── shell/
│   │   │   ├── main.go          # Shell lifecycle entry point
│   │   │   ├── taskbar.go       # Glass taskbar & system tray
│   │   │   ├── menu.go          # Start menu with app launch list
│   │   │   ├── desktop.go       # Desktop icon grid
│   │   │   ├── widget.go        # Toast notification system
│   │   │   └── graphics/        # Surface drawing helper wrappers
│   │   └── runtime/             # TinyGo bare-metal shims
│   └── fonts/                   # Pre-baked DejaVu bitmap headers
├── user/
│   ├── java/                    # Java / GraalVM Native Applications
│   │   ├── toolkit/             # Custom lightweight GUI toolkit
│   │   │   ├── Component.java   # Base UI widget
│   │   │   ├── Container.java   # Composite container
│   │   │   ├── Window.java      # Top-level window
│   │   │   ├── Button.java      # Interactive button
│   │   │   ├── Label.java       # Text label
│   │   │   ├── Graphics.java    # 2D graphics context
│   │   │   └── border/          # Border managers
│   │   ├── myos/binding/        # Kernel syscall & GUI bindings
│   │   │   ├── NativeGUI.java   # JNI/Syscall bridge
│   │   │   └── Syscall.java     # Raw syscall invoker
│   │   ├── files/               # Graphical File Manager app
│   │   │   ├── Main.java        # File manager entry point
│   │   │   └── META-INF/        # GraalVM reflection & native image config
│   │   └── terminal/            # Graphical Terminal Emulator app
│   │       ├── Main.java        # Terminal entry point
│   │       ├── Terminal.java    # Terminal canvas & state
│   │       └── ANSIParser.java  # ANSI escape sequence parser
│   ├── python/                  # Python Applications & MicroPython Port
│   │   ├── runtime/             # MicroPython HAL port & pymyos bindings
│   │   ├── qt/                  # PyQt compatibility layer
│   │   └── apps/                # Calculator, Settings, Text Editor
│   └── libc.c                   # Userspace C standard library
├── include/
│   ├── rust_gui.h               # C header for Rust GUI functions
│   └── mydp/protocol.h          # MyOS Display Protocol specification
└── Makefile                     # Unified multi-language build system
```

---

## 5. Build System Multi-Language Pipeline

The root `Makefile` orchestrates builds across all toolchains:
1. **Rust**: Compiled via `cargo build --release --target x86_64-unknown-none`, producing `build/gui/rust/libmyos_gui.a`.
2. **C Kernel & Drivers**: Compiled via GCC (`-ffreestanding -nostdlib -mno-red-zone -O2`).
3. **Userspace Programs**: Compiled into standalone ELFs, linked with `crt0.o` and `libc.o`.
4. **Go Shell**: Compiled via TinyGo into `shell.bin` / `shell.elf` and embedded into the kernel.
5. **Java Applications**: Compiled via `javac` and GraalVM `native-image --static --no-fallback` into standalone native ELFs.
6. **Python**: MicroPython core compiled into `libpymyos.a` with embedded app scripts.
7. **Kernel Image**: `ld` links all C objects, `libmyos_gui.a`, embedded binaries, and font tables into `kernel.elf`, which is written to the bootable disk image `myos.img`.

---

## 6. Success Verification

| Criteria | Status | Verification Method |
|----------|--------|---------------------|
| **Rust Core Stability** | ✅ Passed | Bare-metal compile clean, no panics, QEMU headless QA pass |
| **Window Compositing** | ✅ Passed | Z-order composition, focus toggling, damage propagation |
| **Input Event Pipeline** | ✅ Passed | PS/2 & USB keyboard/mouse events routed to active window |
| **Font Rendering** | ✅ Passed | DejaVu Sans, Mono, Bold, and Blocks glyphs render cleanly |
| **Desktop Shell** | ✅ Passed | Taskbar, clock, start menu, and icon grid active |
| **Multi-Language Syscalls** | ✅ Passed | SysV ABI compliant, syscall numbers 40–95 operational |
| **Unified Build** | ✅ Passed | Single `make all` produces complete bootable image |
