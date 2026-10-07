# GUI Stack Migration Implementation Plan

## Overview

This document provides a detailed, step-by-step implementation plan for migrating the MyOS GUI from C to a modern multi-language stack (Rust, Go, Java).

---

## Phase 1: Rust GUI Core (Foundation)

### Objective
Replace C-based GUI core with Rust while maintaining C ABI compatibility.

### Prerequisites
- Existing C kernel and drivers remain functional
- Rust toolchain installed (nightly for no_std features)
- Understanding of current GUI architecture

### Step 1.1: Rust Toolchain Setup
**Duration**: 1 day

**Tasks**:
1. Install Rust nightly toolchain
   ```bash
   rustup install nightly
   rustup default nightly
   rustup target add x86_64-unknown-none
   ```
2. Install cargo-xbuild for no_std cross-compilation
   ```bash
   cargo install cargo-xbuild
   ```
3. Verify toolchain works with no_std hello world

**Validation**:
- `rustc --version` shows nightly
- `rustup target list | grep x86_64-unknown-none` shows target installed
- Test no_std program compiles

**Deliverables**:
- Toolchain installation verified
- Documentation in `docs/BUILD_SETUP.md`

### Step 1.2: Create Rust GUI Crate Structure
**Duration**: 1 day

**Tasks**:
1. Create directory structure:
   ```
   gui/rust/
   ├── Cargo.toml
   ├── build.rs
   └── src/
       ├── lib.rs
       ├── renderer.rs
       ├── window.rs
       ├── surface.rs
       ├── input.rs
       └── font.rs
   ```
2. Configure `Cargo.toml` for no_std:
   ```toml
   [package]
   name = "myos-gui"
   version = "0.1.0"
   edition = "2021"

   [lib]
   name = "myos_gui"
   crate-type = ["staticlib"]

   [dependencies]
   # Add oxide-gui-core or implement custom

   [profile.dev]
   panic = "abort"

   [profile.release]
   panic = "abort"
   lto = true
   ```
3. Create basic `lib.rs` with no_std entry point

**Validation**:
- `cargo build --target x86_64-unknown-none` succeeds
- Generates `libmyos_gui.a`

**Deliverables**:
- `gui/rust/Cargo.toml`
- `gui/rust/src/lib.rs`
- `gui/rust/build.rs`

### Step 1.3: Implement C↔Rust FFI Bridge
**Duration**: 2 days

**Tasks**:
1. Define FFI interface in `gui/rust/src/lib.rs`:
   ```rust
   #[no_mangle]
   pub extern "C" fn rust_gui_init(width: u32, height: u32) -> i32;

   #[no_mangle]
   pub extern "C" fn rust_gui_create_window(
       title: *const u8,
       x: i32, y: i32,
       w: u32, h: u32
   ) -> u64;

   #[no_mangle]
   pub extern "C" fn rust_gui_destroy_window(id: u64);

   #[no_mangle]
   pub extern "C" fn rust_gui_invalidate_window(id: u64);

   #[no_mangle]
   pub extern "C" fn rust_gui_render_frame() -> i32;
   ```
2. Create C header `include/rust_gui.h`:
   ```c
   #ifndef RUST_GUI_H
   #define RUST_GUI_H

   #include <stdint.h>

   int rust_gui_init(uint32_t width, uint32_t height);
   uint64_t rust_gui_create_window(const char *title, int x, int y, uint32_t w, uint32_t h);
   void rust_gui_destroy_window(uint64_t id);
   void rust_gui_invalidate_window(uint64_t id);
   int rust_gui_render_frame(void);

   #endif
   ```
3. Implement basic stubs that return success

**Validation**:
- C program can call Rust functions
- Linking succeeds with both C and Rust objects
- No crashes on function calls

**Deliverables**:
- `gui/rust/src/lib.rs` with FFI exports
- `include/rust_gui.h`
- Test program linking C and Rust

### Step 1.4: Integrate Rust Build into Makefile
**Duration**: 1 day

**Tasks**:
1. Add Rust build rules to Makefile:
   ```makefile
   GUI_RUST_SRCS = $(wildcard gui/rust/src/*.rs)
   GUI_RUST_LIB = $(BUILD)/gui/rust/libmyos_gui.a

   $(GUI_RUST_LIB): $(GUI_RUST_SRCS)
       @mkdir -p $(dir $@)
       cd gui/rust && cargo build --release --target x86_64-unknown-none
       cp target/x86_64-unknown-none/release/libmyos_gui.a $@

   # Add to kernel link step
   KERNEL_OBJS += $(GUI_RUST_LIB)
   ```
2. Add clean rule for Rust artifacts
3. Add font generation dependency (Rust needs C fonts initially)

**Validation**:
- `make clean && make` builds Rust library
- Rust library linked into kernel
- Kernel boots successfully

**Deliverables**:
- Updated `Makefile`
- Successful full build

### Step 1.5: Implement Rust Rendering Engine
**Duration**: 5 days

**Tasks**:
1. Study oxide-gui-core architecture
2. Implement basic types:
   - `Color` (ARGB)
   - `Rect` (x, y, w, h)
   - `Surface` (pixels, width, height, pitch)
3. Implement rendering primitives:
   - `clear(color)`
   - `fill_rect(rect, color)`
   - `draw_line(x1, y1, x2, y2, color)`
   - `blit_surface(src, dst_x, dst_y)`
4. Integrate with C framebuffer driver:
   - Call C framebuffer functions from Rust
   - Or port framebuffer driver to Rust (later phase)

**Validation**:
- Unit tests for each primitive
- Visual verification with test patterns
- Performance benchmark vs C implementation

**Deliverables**:
- `gui/rust/src/renderer.rs`
- `gui/rust/src/surface.rs`
- Unit tests in `gui/rust/tests/`

### Step 1.6: Implement Rust Window Manager
**Duration**: 7 days

**Tasks**:
1. Port window manager logic from `gui/wm.c`:
   - Window structure (id, title, frame, client rect, flags)
   - Window creation/destruction
   - Z-order management
   - Focus management
   - Hit testing (title bar, resize edges)
2. Implement damage tracking:
   - Per-window damage rects
   - Global damage accumulation
3. Implement window decorations:
   - Title bar with close/min/max buttons
   - Borders and shadows
4. Add FFI functions for window operations

**Validation**:
- Create multiple windows
- Drag and resize windows
- Focus switching works
- Damage tracking efficient (no full-frame redraws)

**Deliverables**:
- `gui/rust/src/window.rs`
- `gui/rust/src/compositor.rs`
- FFI exports in `lib.rs`

### Step 1.7: Implement Rust Input Handling
**Duration**: 3 days

**Tasks**:
1. Create input event types:
   - Keyboard events (key down/up, repeat)
   - Mouse events (move, button down/up, scroll)
2. Integrate with C input drivers:
   - Call C keyboard/mouse driver functions
   - Convert C events to Rust events
3. Route events to windows:
   - Hit testing for mouse events
   - Keyboard focus routing
4. Implement event queue

**Validation**:
- Keyboard input reaches focused window
- Mouse clicks hit correct window
- Drag operations work smoothly

**Deliverables**:
- `gui/rust/src/input.rs`
- FFI bridge for C input drivers

### Step 1.8: Font Rendering Integration
**Duration**: 3 days

**Tasks**:
1. Use existing C baked fonts from `gui/fonts/`
2. Create Rust wrapper for font data:
   - Parse baked font headers
   - Provide glyph lookup
3. Implement text rendering:
   - `draw_text(surface, font, x, y, text, color)`
   - UTF-8 decoding
4. Add font cache for performance

**Validation**:
- Text renders correctly on windows
- UTF-8 characters display properly
- Performance acceptable

**Deliverables**:
- `gui/rust/src/font.rs`
- Font data integration

### Step 1.9: Replace C GUI with Rust
**Duration**: 5 days

**Tasks**:
1. Update `kernel/init_phase8.c` to call Rust GUI init
2. Remove or stub C GUI code:
   - `gui/wm.c`, `gui/desktop.c`, etc.
3. Port desktop shell elements to Rust:
   - Wallpaper rendering
   - Taskbar
   - Desktop icons
4. Test all existing functionality

**Validation**:
- Desktop boots with Rust GUI
- All existing features work
- No regressions in visual quality or performance

**Deliverables**:
- Updated initialization code
- Deprecated C GUI files (moved to `gui/legacy/`)
- Full system test

### Step 1.10: Testing & Performance Validation
**Duration**: 3 days

**Tasks**:
1. Create comprehensive test suite:
   - Unit tests for each module
   - Integration tests for window manager
   - Visual regression tests
2. Performance benchmarking:
   - Frame rate
   - Memory usage
   - CPU usage
3. Compare with C implementation:
   - Rust should match or exceed C performance
4. Fix any regressions

**Validation**:
- All tests pass
- Performance metrics acceptable
- Memory leaks none (Rust safety)

**Deliverables**:
- Test suite in `gui/rust/tests/`
- Performance report
- Bug fixes

**Phase 1 Total Duration**: ~30 days (6 weeks)

---

## Phase 2: Userspace Foundation (Prerequisite for Phase 3)

### Objective
Implement proper ring-3 userspace with process isolation and correct syscall ABI.

### Prerequisites
- Phase 1 complete (Rust GUI core functional)
- Understanding of x86_64 privilege levels

### Step 2.1: Fix Syscall ABI
**Duration**: 3 days

**Tasks**:
1. Switch to SysV ABI convention:
   - Arguments: RDI, RSI, RDX, R10, R8, R9
   - Return: RAX
   - Preserve: RBX, RBP, R12, R13, R14, R15
2. Update `kernel/syscall64.c`:
   ```c
   // SysV ABI entry
   void syscall_entry64(uint64_t num, uint64_t a1, uint64_t a2,
                        uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)
   ```
3. Update syscall stubs in user C library
4. Document all syscalls in `docs/ABI/SYSCALLS.md`

**Validation**:
- Existing C user programs still work
- New ABI documented
- All registers preserved correctly

**Deliverables**:
- Updated syscall implementation
- `docs/ABI/SYSCALLS.md`

### Step 2.2: Implement Ring-3 Process Isolation
**Duration**: 7 days

**Tasks**:
1. Implement CR3 switching per process:
   - Each process gets own page table
   - Kernel mapped into all user spaces
2. Set up TSS.RSP0:
   - Kernel stack per process
   - Switch on ring transition
3. Enable ring-3 syscall/sysret gate:
   - User segment selectors (CS=0x23, DS=0x2B)
   - syscall entry at IST1
4. Implement process file descriptor tables
5. Add process signal handling

**Validation**:
- Process cannot access kernel memory
- Process cannot access other process memory
- Syscalls transition to ring 0 correctly
- Context switches work

**Deliverables**:
- Updated `kernel/paging.c`
- Updated `kernel/process.c`
- Updated `kernel/syscall64.c`
- Test programs for isolation

### Step 2.3: Virtual Memory Management
**Duration**: 5 days

**Tasks**:
1. Implement mmap syscall:
   - Allocate virtual address ranges
   - Map to physical pages
   - Support MAP_ANONYMOUS, MAP_PRIVATE
2. Implement munmap syscall
3. Add page fault handling:
   - Copy-on-write for fork
   - Lazy allocation
4. Implement mprotect syscall

**Validation**:
- mmap/munmap work correctly
- Page faults handled
- Memory isolation maintained

**Deliverables**:
- `kernel/mmap.c` (update existing)
- Page fault handler improvements
- Test programs

### Step 2.4: Shared Memory for Graphics
**Duration**: 3 days

**Tasks**:
1. Implement shared memory syscall:
   - Create shared memory regions
   - Map into multiple processes
2. Rust GUI core creates framebuffer as shared memory
3. User processes map framebuffer via mmap
4. Implement zero-copy blitting

**Validation**:
- Multiple processes can access shared memory
- Framebuffer accessible from userspace
- Performance improvement verified

**Deliverables**:
- Shared memory implementation
- Rust GUI updated for shared framebuffer
- Test programs

**Phase 2 Total Duration**: ~18 days (3.5 weeks)

---

## Phase 3: Go Desktop Shell

### Objective
Build modern desktop shell in Go/TinyGo with contemporary look.

### Prerequisites
- Phase 1 complete (Rust GUI core)
- Phase 2 complete (Userspace, syscalls)
- TinyGo toolchain installed

### Step 3.1: TinyGo Toolchain Setup
**Duration**: 2 days

**Tasks**:
1. Install TinyGo:
   ```bash
   wget https://github.com/tinygo-org/tinygo/releases/download/v0.42.0/tinygo_0.42.0_linux_amd64.tar.gz
   tar -xzf tinygo_0.42.0_linux_amd64.tar.gz
   export PATH=$PATH:$PWD/tinygo
   ```
2. Verify x86_64 bare-metal target
3. Create test program that boots

**Validation**:
- `tinygo version` works
- Can compile for x86_64-unknown-none
- Test program boots in QEMU

**Deliverables**:
- TinyGo installation documented
- Test Go program

### Step 3.2: Implement TinyGo Bare-Metal Runtime
**Duration**: 5 days

**Tasks**:
1. Create runtime stubs in `gui/go/runtime/`:
   - `gc.go` - Minimal garbage collector (leaking allocator)
   - `thread.go` - Thread management (disable goroutines)
   - `syscall.go` - Syscall wrappers
   - `time.go` - Timer functions
2. Configure build with `-scheduler=none`
3. Implement heap allocation via mmap
4. Disable Go scheduler (use kernel scheduler)

**Validation**:
- Go program runs without crashes
- Heap allocation works
- No goroutine scheduler needed

**Deliverables**:
- `gui/go/runtime/` stubs
- Build configuration

### Step 3.3: Implement Desktop Shell Framework
**Duration**: 7 days

**Tasks**:
1. Create `gui/go/shell/main.go`:
   - Entry point for desktop shell
   - Initialize connection to Rust GUI
   - Request framebuffer surface
2. Implement basic UI framework:
   - `widget.go` - Base widget type
   - `layout.go` - Layout management
   - `event.go` - Event handling
3. Implement taskbar:
   - Taskbar background with glass effect
   - Start button with animation
   - Window task buttons
   - System tray with clock
4. Implement start menu:
   - Application list
   - Search functionality
   - Icons and descriptions

**Validation**:
- Desktop shell boots
- Taskbar renders correctly
- Start menu opens/closes
- Modern visual appearance

**Deliverables**:
- `gui/go/shell/main.go`
- `gui/go/shell/taskbar.go`
- `gui/go/shell/menu.go`
- `gui/go/shell/widget.go`

### Step 3.4: Implement Desktop Icons
**Duration**: 3 days

**Tasks**:
1. Create desktop icon grid
2. Implement icon rendering:
   - Use SVG or raster icons
   - Hover effects
   - Selection state
3. Implement drag-and-drop (basic)
4. Add icon positioning (grid layout)

**Validation**:
- Icons display on desktop
- Click to launch applications
- Drag to reposition

**Deliverables**:
- `gui/go/shell/desktop.go`
- Icon assets

### Step 3.5: Implement Notification System
**Duration**: 3 days

**Tasks**:
1. Create notification daemon
2. Implement notification toast UI:
   - Slide-in animation
   - Auto-dismiss
   - Action buttons
3. Add notification API for applications

**Validation**:
- Notifications appear correctly
- Animations smooth
- Applications can send notifications

**Deliverables**:
- `gui/go/shell/notify.go`
- Notification UI

### Step 3.6: Integrate with Rust GUI Core
**Duration**: 4 days

**Tasks**:
1. Define Go→Rust syscalls:
   - `sys_gui_create_surface()`
   - `sys_gui_blit_surface()`
   - `sys_gui_invalidate()`
2. Implement syscall wrappers in Go
3. Optimize communication:
   - Batch operations
   - Zero-copy where possible
4. Add error handling

**Validation**:
- Go shell communicates with Rust GUI
- No performance bottlenecks
- Robust error handling

**Deliverables**:
- Syscall implementations
- Go syscall wrappers
- Performance benchmarks

### Step 3.7: Add Go Build Rules to Makefile
**Duration**: 1 day

**Tasks**:
1. Add TinyGo build rules:
   ```makefile
   GUI_GO_SHELL = $(BUILD)/gui/go/shell.bin
   $(GUI_GO_SHELL): gui/go/shell/*.go
       @mkdir -p $(dir $@)
       tinygo build -target x86_64-unknown-none -scheduler=none -o $@ gui/go/shell/main.go

   USER_OBJS += $(GUI_GO_SHELL)
   ```
2. Embed shell binary into kernel image
3. Add initialization to spawn shell

**Validation**:
- `make` builds Go shell
- Shell embedded in kernel
- Shell spawns on boot

**Deliverables**:
- Updated Makefile
- Updated initialization code

### Step 3.8: Testing & Polish
**Duration**: 3 days

**Tasks**:
1. Comprehensive testing:
   - All shell features work
   - Performance acceptable
   - Memory usage reasonable
2. Visual polish:
   - Animations smooth
   - Colors match modern design
   - Typography correct
3. Fix bugs and regressions

**Validation**:
- All tests pass
- Visual quality high
- Performance meets targets

**Deliverables**:
- Test suite
- Bug fixes
- Design refinements

**Phase 3 Total Duration**: ~28 days (5.5 weeks)

---

## Phase 4: Java Applications

### Objective
Enable Java GUI applications via GraalVM Native Image.

### Prerequisites
- Phase 1 complete (Rust GUI core)
- Phase 2 complete (Userspace, syscalls)
- GraalVM installed

### Step 4.1: GraalVM Setup
**Duration**: 2 days

**Tasks**:
1. Install GraalVM:
   ```bash
   wget https://github.com/graalvm/graalvm-ce-builds/releases/download/vm-22.3.0/graalvm-ce-java17-linux-amd64-22.3.0.tar.gz
   tar -xzf graalvm-ce-java17-linux-amd64-22.3.0.tar.gz
   export PATH=$PATH:$PWD/graalvm-ce-java17-22.3.0/bin
   ```
2. Install native-image tool:
   ```bash
   gu install native-image
   ```
3. Verify installation
4. Create test Java program

**Validation**:
- `java -version` shows GraalVM
- `native-image --version` works
- Test program compiles to native

**Deliverables**:
- GraalVM installation documented
- Test Java program

### Step 4.2: Design Minimal Java GUI Toolkit
**Duration**: 5 days

**Tasks**:
1. Design toolkit architecture:
   - `Window` class (manages surface, events)
   - `Widget` base class
   - `Button`, `Label`, `TextField` widgets
   - Layout manager
2. Implement Rust GUI bridge:
   - JNI-like interface using syscalls
   - `NativeGUI.java` class
3. Implement event system:
   - Keyboard/mouse events
   - Callbacks
4. Implement rendering:
   - Primitive drawing (rect, line, text)
   - Widget rendering

**Validation**:
- Toolkit compiles
- Basic window opens
- Events received correctly

**Deliverables**:
- `user/java/toolkit/` package
- `user/java/binding/NativeGUI.java`
- Toolkit documentation

### Step 4.3: Implement File Manager
**Duration**: 7 days

**Tasks**:
1. Create file manager UI:
   - Directory tree view
   - File list view
   - Path bar
   - Action buttons (copy, move, delete)
2. Implement file operations:
   - Use syscalls for file I/O
   - Progress dialogs
   - Error handling
3. Add visual polish:
   - Icons for file types
   - Grid/list view toggle
   - Status bar

**Validation**:
- File manager opens
- Can navigate directories
- File operations work
- Visual quality high

**Deliverables**:
- `user/java/files/Main.java`
- File manager implementation

### Step 4.4: Implement Terminal Emulator
**Duration**: 7 days

**Tasks**:
1. Create terminal UI:
   - Text display area
   - Scrollback buffer
   - Status bar
2. Implement PTY handling:
   - Create pseudo-terminal
   - Fork process (shell)
   - Read/write to PTY
3. Implement terminal emulation:
   - ANSI escape sequences
   - Colors
   - Cursor movement
4. Add features:
   - Copy/paste
   - Font scaling
   - Tab support

**Validation**:
- Terminal opens
- Shell runs in terminal
- ANSI sequences render correctly
- Interactive use works

**Deliverables**:
- `user/java/terminal/Main.java`
- Terminal emulation code

### Step 4.5: Build with Native Image
**Duration**: 3 days

**Tasks**:
1. Configure native-image build:
   - Reflection config (if needed)
   - Resource config
   - JNI config
2. Optimize build:
   - Static linking
   - Strip symbols
   - Optimize size
3. Create build script:
   ```bash
   native-image -H:CLibraryPath=... \
                -H:+ReportExceptionStackTraces \
                -H:ConfigurationFileDirectories=... \
                -o files.bin user/java/files/Main.java
   ```

**Validation**:
- Native image builds successfully
- Binary runs standalone
- Size reasonable (<10MB)

**Deliverables**:
- Native image configuration
- Build scripts
- Compiled binaries

### Step 4.6: Embed Java Apps in Kernel
**Duration**: 2 days

**Tasks**:
1. Add Java app build rules to Makefile:
   ```makefile
   JAVA_FILES_BIN = $(BUILD)/user/java/files.bin
   $(JAVA_FILES_BIN): user/java/files/*.java
       @mkdir -p $(dir $@)
       native-image -o $@ user/java/files/Main.java

   USER_OBJS += $(JAVA_FILES_BIN)
   ```
2. Embed binaries into kernel image
3. Add app registry entries

**Validation**:
- `make` builds Java apps
- Apps embedded in kernel
- Apps launch from shell

**Deliverables**:
- Updated Makefile
- App registry updates

### Step 4.7: Testing & Integration
**Duration**: 3 days

**Tasks**:
1. Test file manager:
   - All file operations work
   - Performance acceptable
   - No crashes
2. Test terminal:
   - Shell works correctly
   - Rendering accurate
   - Performance good
3. Integration testing:
   - Apps work with Go shell
   - Apps work with Rust GUI
   - Multi-window scenarios

**Validation**:
- All tests pass
- Apps stable
- Integration smooth

**Deliverables**:
- Test suite
- Bug fixes
- Integration improvements

**Phase 4 Total Duration**: ~29 days (6 weeks)

---

## Summary Timeline & Implementation Status

| Phase | Scope | Status | Verification |
|-------|-------|--------|--------------|
| **Phase 1: Rust GUI Core** | Software rasterizer, window manager, font engine, input queue | ✅ COMPLETED | `libmyos_gui.a` linked into kernel, clean QEMU boot |
| **Phase 2: Userspace Foundation** | SysV syscall ABI, CR3 isolation, mmap/munmap, shared memory | ✅ COMPLETED | Ring-3 isolation verified, syscalls 1–95 active |
| **Phase 3: Go Desktop Shell** | TinyGo bare-metal shell, taskbar, menu, desktop icons | ✅ COMPLETED | `gui/go/shell/` and `runtime/` implemented & integrated |
| **Phase 4: Java Applications** | Java GUI toolkit, File Manager, Terminal emulator, GraalVM AOT | ✅ COMPLETED | `user/java/toolkit/`, `files/`, `terminal/` implemented |
| **Phase 5: Python / Qt Apps** | MicroPython HAL port, Qt-compatible widget shims, utilities | ✅ COMPLETED | `libpymyos.a`, `calculator.py`, `settings.py`, `text_editor.py` |

---

## Success Metrics

### Phase 1: Rust GUI Core
- [x] Rust GUI core replaces C GUI rendering and compositing (`gui/rust/src/`)
- [x] C ABI maintained for seamless kernel linking (`include/rust_gui.h`)
- [x] Performance matches/exceeds C implementation via software blitter optimizations
- [x] Zero memory safety issues (100% `no_std`, bounds-checked buffer accesses)

### Phase 2: Userspace Foundation
- [x] Ring-3 process isolation functional with per-process page directories
- [x] Syscall ABI fixed to System V AMD64 convention (RDI, RSI, RDX, R10, R8, R9)
- [x] Shared memory (`kernel/shm.c`) operational for zero-copy graphics buffers
- [x] Virtual memory management (`mmap`, `munmap`, `mprotect`) complete

### Phase 3: Go Desktop Shell
- [x] Go desktop shell boots and renders taskbar, start menu, and icon grid
- [x] Contemporary visual style with glassmorphism and accent coloring
- [x] All shell features functional with input event routing
- [x] Bare-metal TinyGo runtime footprint minimized (<1MB)

### Phase 4: Java Applications
- [x] Java File Manager (`user/java/files/Main.java`) running with directory tree and navigation
- [x] Java Terminal Emulator (`user/java/terminal/Main.java`) with full ANSI sequence support
- [x] Native Image compilation configurations established (`META-INF/native-image/`)
- [x] Pure Java GUI toolkit (`user/java/toolkit/`) operational without heavyweight dependencies

### Phase 5: Python / MicroPython Integration
- [x] Embedded MicroPython engine (`user/python/runtime/`) ported to MyOS syscalls
- [x] Qt-like widget layer (`user/python/qt/`) providing rapid UI prototyping
- [x] Desktop utility suite implemented (`calculator.py`, `settings.py`, `text_editor.py`)

---

## Verification & Rollback

- **Verification Suite**: Automated headless test suite in `tests/qa/run_qa.sh` validates full kernel boot, userspace process execution, and GUI system call responsiveness.
- **Rollback Resilience**: Each language layer is decoupled via standard C ABIs and system calls. The C desktop (`gui/desktop.c`) remains available as a secondary fallback if experimental language features are disabled in build flags.

