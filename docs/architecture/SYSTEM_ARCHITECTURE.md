# MyOS System Architecture Specification

## 1. Executive Summary

**MyOS** is a preemptive, multi-tasking, 64-bit operating system developed from scratch for the x86-64 (AMD64) architecture. It features a hybrid multi-language design:
- **Ring 0 Kernel Core**: Written in freestanding C and NASM assembly for hardware initialization, interrupt routing, virtual memory management, process scheduling, virtual filesystems, and device drivers.
- **Ring 0 GUI Core**: Written in `no_std` Rust (`gui/rust/`), providing a memory-safe 2D rendering pipeline, window compositor, input event queue, and DejaVu font engine.
- **Ring 3 Userspace Shell**: Written in Go / TinyGo (`gui/go/shell/`), providing a modern desktop shell with glassmorphism taskbars and application menus.
- **Ring 3 Desktop Applications**: Written in Java (`user/java/`) and compiled ahead-of-time (AOT) to standalone native ELFs via GraalVM Native Image, alongside rapid utility apps written in embedded MicroPython (`user/python/`).

---

## 2. Boot & Initialization Chain

MyOS supports both standard BIOS MBR booting and modern UEFI firmware environments:

```
                  ┌───────────────────────────────┐
                  │ BIOS Master Boot Record (MBR) │
                  │  (boot/boot_sector_min.asm)   │
                  └───────────────┬───────────────┘
                                  │ LBA 1
                  ┌───────────────▼───────────────┐
                  │ Stage 2: Real Mode Transition │
                  │      (boot/stage2.asm)        │
                  └───────────────┬───────────────┘
                                  │ LBA 2
                  ┌───────────────▼───────────────┐
                  │ Stage 3: Protected/Long Mode  │
                  │      (boot/stage3.asm)        │
                  │  - Reads E820 memory map      │
                  │  - Enables A20 gate           │
                  │  - Sets up PAE & Long Mode    │
                  │  - Establishes PML4 paging    │
                  │  - Relocates kernel to 1MB    │
                  └───────────────┬───────────────┘
                                  │ Jumps to 0x100000
                  ┌───────────────▼───────────────┐
                  │ Kernel Entry & Initialization │
                  │   (kernel/kernel_entry.asm)   │
                  │   -> kernel_main_64()         │
                  └───────────────────────────────┘
```

### UEFI Boot Alternative
- Implemented in `boot/efi_main.c` (`BOOTX64.EFI`).
- Allocates memory at `0x100000`, loads `kernel.bin`, queries EFI memory map, passes metadata at `0x4FF8/0x4FFC`, exits Boot Services, and jumps to `kernel_entry.asm`.

### Kernel Initialization Phasing
1. **Phase 1: Early BSS & Console**: Zeroes BSS, initializes COM1 UART serial (`serial_init`) and VGA text/framebuffer screen (`screen_init`).
2. **Phase 2: CPU Descriptors**: Initializes 64-bit Global Descriptor Table (`gdt_init`) and Task State Segment (`tss_init`) with kernel stack pointers.
3. **Phase 3: Physical Memory**: Parses E820 memory map from bootloader and initializes bitmap-based Physical Memory Manager (`pmm_init`).
4. **Phase 4: Virtual Memory & Heap**: Allocates kernel heap at `0x4000000` (`heap_init`) and sets up 4-level paging (`paging_init`).
5. **Phase 5: Interrupts & System Calls**: Configures IDT, PIC/APIC, timer (1000 Hz PIT), and sets up `SYSCALL`/`SYSRET` MSR registers (`syscall_init_64`).
6. **Phase 6: Multi-Tasking**: Initializes Process Control Block pool (`process_init`) and round-robin scheduler (`scheduler_init`).
7. **Phase 7: Hardware & Storage**: Probes PCI bus, initializes VirtIO, ATA/AHCI/NVMe storage controllers, and USB xHCI controllers.
8. **Phase 8: Higher-Level Subsystems (`kernel/init_phase8.c`)**:
   - Mounts VFS root with RamFS and DevFS (`/dev/console`, `/dev/null`, `/dev/zero`, `/dev/random`).
   - Detects GPT partition tables and mounts ext4 partitions if present.
   - Populates in-memory `/bin` with 24 embedded userspace executables.
   - Spawns PID 1 (`init`), interactive shell (`sh`), and desktop compositor.
   - Launches Rust GUI desktop environment (`rust_gui_init_desktop`, `rust_gui_compose_desktop`).

---

## 3. Memory Architecture

```
0x00000000_00000000 ──────────────────────────────────────────
                    │ Unmapped Guard Page (Catches NULL ptrs) │
0x00000000_40000000 ────────────────────────────────────────── ELF_USER_VMA_MIN
                    │ User Code, Data, BSS (.text / .data)    │
                    │ User Heap (via sys_mmap / sys_brk)      │
                    │ Shared Memory Graphics Buffers (SHM)    │
0x00000000_BFFDF000 ────────────────────────────────────────── USER_STACK_BOTTOM
                    │ User Stack (32 KiB, grows downward)     │
0x00000000_BFFFF000 ────────────────────────────────────────── USER_STACK_TOP
                    │ Unmapped Guard Page                     │
0xFFFF8000_00000000 ────────────────────────────────────────── Higher-Half Kernel
                    │ Identity Map & Kernel Code (at 1MB)     │
                    │ Kernel Heap (at 0x4000000)              │
                    │ Framebuffer Memory & MMIO Spaces        │
```

### Physical Memory Manager (PMM)
- **Implementation**: `kernel/pmm.c`, `kernel/pmm.h`
- **Algorithm**: Bitmap tracking 4KB frames across physical memory regions up to 1GB (`MAX_PAGES = 262144`).
- **Features**: Single-page and contiguous page allocation, frame reservation for boot stacks and kernel text, thread-safe with interrupt state preservation.

### 4-Level Paging
- **Implementation**: `kernel/paging.c`, `kernel/paging.h`
- **Structure**: Page Map Level 4 (PML4) → Page Directory Pointer Table (PDPT) → Page Directory (PD) → Page Table (PT).
- **Isolation**: Each process possesses an independent PML4 page directory (`proc->page_dir`). Address space switching occurs via `CR3` reloading during context switches.
- **Flags**: `PAGE_PRESENT`, `PAGE_WRITE`, `PAGE_USER`, `PAGE_NOCACHE`, `PAGE_NX` (Execute-Disable bit enforced via EFER.NXE).

### Virtual Memory Subsystem
- **Implementation**: `kernel/mmap.c`, `kernel/mmap.h`
- **System Calls**: `mmap`, `munmap`, `mprotect`.
- **Flags**: `MAP_ANONYMOUS`, `MAP_PRIVATE`, `MAP_SHARED`, `MAP_FIXED`.
- **Shared Memory (SHM)**: Named shared memory segments (`kernel/shm.c`) allowing zero-copy framebuffer and surface sharing between the kernel compositor and userland GUI applications.

---

## 4. Process Management & Scheduling

### Process Control Block (`process_t`)
Each process is tracked in `kernel/process.h`:
- **Identity**: `pid`, `ppid`, `name`, `state` (`PROC_READY`, `PROC_RUNNING`, `PROC_BLOCKED`, `PROC_SLEEPING`, `PROC_ZOMBIE`).
- **CPU Context**: `cpu_context_t` saving callee-saved registers (`r15`, `r14`, `r13`, `r12`, `rbx`, `rbp`, `rdi`, `rsi`, `rsp`, `rip`, `rflags`) plus 512-byte aligned FPU/SSE state.
- **Trap Frame**: `trap_frame_t` containing complete user-mode register state on interrupt or syscall entry.
- **Kernel Stack**: Dedicated 64 KiB kernel stack (`kernel_stack`) loaded into `TSS.RSP0` on every task switch.
- **Address Space**: Pointer to private `page_directory_t`.
- **File Descriptors**: Process file table with up to 16 descriptors (`fd_table`).
- **Controlling Terminal**: Pointer to active `vtty` (boot console or GUI terminal window).
- **Signal Infrastructure**: Signal masks, pending signal bitmask, and 32 `sigaction` handlers.

### Scheduler
- **Implementation**: `kernel/scheduler.c`, `kernel/scheduler.h`
- **Model**: Preemptive round-robin scheduler driven by the 1000 Hz PIT timer interrupt.
- **Queueing**: Circular doubly-linked ready queue with priority-weighted time slices.
- **Context Switch Assembly**: `kernel/context_switch.asm` (`context_switch` and `switch_to_user`).

---

## 5. System Call Interface (ABI)

MyOS conforms to the **System V AMD64 System Call ABI**:
- **Transition**: `SYSCALL` / `SYSRET` instructions configured via `MSR_STAR`, `MSR_LSTAR`, and `MSR_FMASK`.
- **Register Convention**:
  - Syscall Number: `RAX`
  - Arguments 1–6: `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9`
  - Return Value: `RAX` (negative values `[-4095, -1]` represent `-errno`)
  - Clobbered by hardware: `RCX` (saved user RIP), `R11` (saved user RFLAGS)
  - Preserved across call: `RBX`, `RBP`, `R12`, `R13`, `R14`, `R15`, `RSP`

### Syscall Categories
- **POSIX Core (1–32)**: `SYS_EXIT`, `SYS_FORK`, `SYS_READ`, `SYS_WRITE`, `SYS_OPEN`, `SYS_CLOSE`, `SYS_WAIT`, `SYS_EXEC`, `SYS_GETPID`, `SYS_SLEEP`, `SYS_YIELD`, `SYS_KILL`, `SYS_BRK`, `SYS_MMAP`, `SYS_MUNMAP`, `SYS_MPROTECT`, `SYS_SHMGET`, `SYS_SHMCTL`, `SYS_TIME`, `SYS_REBOOT`, `SYS_SHUTDOWN`.
- **GUI Engine (40–62)**: `SYS_GUI_CREATE_SURFACE`, `SYS_GUI_BLIT_SURFACE`, `SYS_GUI_INVALIDATE`, `SYS_GUI_GET_FB_INFO`, `SYS_GUI_INIT`, `SYS_GUI_CREATE_WINDOW`, `SYS_GUI_DESTROY_WINDOW`, `SYS_GUI_RENDER_FRAME`, `SYS_GUI_PUSH_KEY_EVENT`, `SYS_GUI_PUSH_MOUSE_EVENT`, `SYS_GUI_FOCUS_WINDOW`, `SYS_GUI_SET_WINDOW_TITLE`, `SYS_GUI_SET_WINDOW_RECT`.
- **IPC & Message Ports (70–82)**: `SYS_IPC_PORT_CREATE`, `SYS_IPC_PORT_DESTROY`, `SYS_IPC_PORT_SEND`, `SYS_IPC_PORT_RECV`, `SYS_IPC_CAP_GRANT`, `SYS_IPC_SHM_CREATE`, `SYS_IPC_EVENT_PUBLISH`.
- **MYDP Display Protocol (90–95)**: `SYS_MYDP_CREATE_SURFACE`, `SYS_MYDP_ATTACH_BUFFER`, `SYS_MYDP_COMMIT`, `SYS_MYDP_DAMAGE`, `SYS_MYDP_CLOSE_SURFACE`.

---

## 6. Virtual Filesystem (VFS)

- **VFS Abstraction**: Defined in `fs/vfs.h`, `fs/vfs.c`. Inode abstraction with function pointers (`read`, `write`, `finddir`, `readdir`).
- **Directory Cache (dcache)**: 256-entry LRU cache for rapid path resolution.
- **Filesystem Implementations**:
  - **RamFS** (`fs/ramfs.c`): In-memory filesystem storing embedded userland binaries and configuration files.
  - **DevFS** (`fs/devfs.c`): Device filesystem exposing `/dev/console`, `/dev/null`, `/dev/zero`, `/dev/random`, `/dev/rtc`.
  - **ext2** (`fs/ext2.c`): Full read-write filesystem driver for standard ext2 storage images.
  - **ext4** (`fs/ext4.c`): Extent-tree based reader for modern Linux partitions.
  - **FAT16** (`fs/fat16.c`): Support for legacy DOS and FAT partitions.
  - **ProcFS** (`fs/procfs.c`): Dynamic process and system status reporting.
  - **GPT Support** (`kernel/gpt.c`, `gpt_ext4_mount.c`): GUID Partition Table parsing with automated root partition mounting.

---

## 7. Networking Stack

A complete TCP/IP stack implemented from scratch in `net/`:
- **Data Link**: Ethernet II frame construction and parsing (`net/eth.c`).
- **Address Resolution**: ARP protocol with dynamic cache and request/reply processing (`net/arp.c`).
- **Network Layer**: IPv4 protocol with checksumming, header validation, and routing (`net/ip.c`).
- **Transport Layer**:
  - ICMP echo request/reply (ping) (`net/icmp.c`).
  - UDP connectionless datagram transport (`net/udp.c`).
  - TCP connection-oriented stream transport with handshake and sliding window (`net/tcp.c`).
- **Application Services**: DHCP client for automatic network configuration (`net/dhcp.c`), DNS resolver (`net/dns.c`), embedded HTTP client and web utility (`net/http.c`, `net/web.c`).
- **Socket API**: BSD socket interface (`socket`, `bind`, `connect`, `send`, `recv`, `close`) in `net/socket.c`.
- **Supported Network Interfaces**: VirtIO Net (`drivers/virtio_net.c`), NE2000 PCI (`drivers/ne2k.c`), Intel E1000 PCI.

---

## 8. Multi-Language GUI Subsystem

The graphical subsystem combines three language tiers into a unified desktop:
1. **Rust GUI Core (`gui/rust/`)**: Bare-metal software rasterizer supporting clipping, anti-aliased curves, gradients, window chrome decorations, and pre-baked DejaVu bitmap font rendering.
2. **Go Desktop Shell (`gui/go/shell/`)**: TinyGo-based taskbar, start menu, desktop icon grid, and notification daemon.
3. **Java & Python Application Suite**:
   - `user/java/files/`: Graphical File Manager.
   - `user/java/terminal/`: Graphical Terminal Emulator with full ANSI colors.
   - `user/python/apps/`: Calculator, text editor, and system settings panels.

---

## 9. Verification & Quality Assurance

The system is validated by an automated headless regression pipeline:
- **Test Harness**: `tests/qa/run_qa.sh`
- **Validation Steps**:
  1. Complete build verification (`make all`).
  2. Creates a scratch data disk image.
  3. Boots QEMU in headless mode (`-display none -serial stdio`).
  4. Validates boot banner: `[PHASE8] Init complete`.
  5. Confirms PMM initialization, 4-level paging, and NX protection.
  6. Confirms userland processes spawn, execute, and exit cleanly.
  7. Confirms child processes exit with expected status codes (`forkdemo`).
  8. Ensures zero panics, zero unhandled page faults, and zero triple faults.
