# MyOS — Complete Architectural Overview

## 1. HIGH-LEVEL LAYERED ARCHITECTURE

```text
┌─────────────────────────────────────────────────────┐
│                  USER APPLICATIONS                  │
│         (Native Apps · Web Apps · Terminals)        │
├─────────────────────────────────────────────────────┤
│              MyOS DESKTOP ENVIRONMENT               │
│    (Compositor · WM · Shell · Widget Toolkit)       │
├─────────────────────────────────────────────────────┤
│              SYSTEM LIBRARIES (libc/mylib)          │
│        (POSIX-compat · Graphics · Audio · Net)      │
├─────────────────────────────────────────────────────┤
│              SYSTEM DAEMONS & SERVICES              │
│     (Init · DevMgr · NetMgr · AudioSrv · LogD)      │
├─────────────────────────────────────────────────────┤
│               SYSTEM CALL INTERFACE                 │
│            (syscall · ioctl · mmap · IPC)           │
├═════════════════════════════════════════════════════┤
│ ╔═══════════════════════════════════════════════╗   │
│ ║            MyOS MONOLITHIC KERNEL             ║   │
│ ║  ┌─────────┬──────────┬──────────┬─────────┐  ║   │
│ ║  │ Process │  Memory  │   VFS    │ Network │  ║   │
│ ║  │  Mgr    │  Mgr     │          │  Stack  │  ║   │
│ ║  ├─────────┼──────────┼──────────┼─────────┤  ║   │
│ ║  │ Sched   │  VM/PMM  │  FS Drv  │ TCP/UDP │  ║   │
│ ║  ├─────────┴──────────┴──────────┴─────────┤  ║   │
│ ║  │       IPC · Signals · Timers · klog     │  ║   │
│ ║  ├─────────────────────────────────────────┤  ║   │
│ ║  │     HARDWARE ABSTRACTION LAYER (HAL)    │  ║   │
│ ║  │  IRQ · ACPI · PCI · USB · DMA · Timer   │  ║   │
│ ║  └─────────────────────────────────────────┘  ║   │
│ ╚═══════════════════════════════════════════════╝   │
├─────────────────────────────────────────────────────┤
│              LOADABLE KERNEL MODULES                │
│        (GPU · Audio · Net · Storage · Input)        │
├─────────────────────────────────────────────────────┤
│                  HARDWARE (x86_64/ARM)              │
└─────────────────────────────────────────────────────┘
```

## 2. KERNEL SUBSYSTEMS

| Subsystem | Role | Linux Equivalent |
|---|---|---|
| Process Manager | PID allocation, fork/exec/exit, thread mgmt | `kernel/fork.c`, `kernel/exit.c` |
| Scheduler | CFS-inspired fair scheduler, real-time queues | `kernel/sched/` |
| Memory Manager | PMM (bitmap/buddy), VMM (page tables, mmap, COW) | `mm/` |
| VFS | Unified inode/dentry/superblock abstraction | `fs/` |
| IPC | Shared memory, message queues, pipes, Unix sockets | `ipc/` |
| Signal Framework | POSIX signals + extended async events | `kernel/signal.c` |
| Timer Subsystem | High-res timers, tickless (NO_HZ) model | `kernel/time/` |
| Security Module | Capability-based + MAC (like SELinux lite) | `security/` |

## 3. HARDWARE ABSTRACTION LAYER (HAL)

```text
HAL
 ├── irq_controller    → APIC / GIC routing
 ├── acpi_parser       → Table walk, power states (S0–S5)
 ├── pci_bus           → Enumerate, BAR mapping, MSI/MSI-X
 ├── usb_host          → xHCI / EHCI controller abstraction
 ├── dma_engine        → IOMMU-aware DMA alloc/map
 ├── clock_source      → TSC / HPET / ARM Generic Timer
 └── firmware_if       → UEFI runtime services bridge
```
*Design Rule: No kernel subsystem touches hardware registers directly. All access flows through HAL.*

## 4. DRIVER MODEL (Modular, Linux-Inspired)

### 4.1 Driver Categories

| Class | Examples | Interface |
|---|---|---|
| Block | NVMe, AHCI/SATA, VirtIO-blk | `blk_request_queue` |
| Char | TTY, serial, input, framebuffer | `file_operations` |
| Network | e1000, virtio-net, WiFi | `net_device_ops` |
| GPU/Display | DRM/KMS-style, VirtIO-GPU | `drm_driver` |
| Audio | HDA, VirtIO-snd | `snd_card` |
| Input | PS/2, USB HID, touchpad | `input_dev` |
| Filesystem | MyFS, ext4-compat, FAT32, tmpfs | `file_system_type` |
| Bus | PCI, USB, I2C, SPI | `bus_type` |

### 4.2 Driver Lifecycle

```text
probe() → init() → register() → [runtime] → unregister() → remove()
   ↑                                                        ↓
   └──── devmgr hotplug event (udev-like daemon) ───────────┘
```

### 4.3 Module Format
- `.kmod` — ELF relocatable with metadata section
- Signed modules enforced at boot (Secure Boot chain)
- Dependency resolution via modprobe daemon

## 5. USER-SPACE ARCHITECTURE

```text
/sbin/myinit          ← PID 1 (systemd-like, but minimal)
 ├── /sbin/devmgr     ← Device node manager (udev-like)
 ├── /sbin/netmgr     ← Network config daemon
 ├── /sbin/audiosrv   ← Audio mixing daemon
 ├── /sbin/logd       ← Ring-buffer + file logger
 ├── /sbin/powersrv   ← ACPI event handler
 └── /sbin/mountd     ← Auto-mount daemon

/usr/bin/             ← User applications
/usr/lib/             ← Shared libraries (mylibc.so)
/etc/myos/            ← System config (TOML format)
/var/                 ← Runtime state
/tmp/                 ← tmpfs mount
```

## 6. GUI / DISPLAY STACK (Lightweight + Modern)

```text
┌──────────────────────────────────────┐
│          USER APPLICATIONS           │
│    (MyOS Toolkit / SDL / GTK-port)   │
├──────────────────────────────────────┤
│        MyOS WIDGET TOOLKIT           │
│   (Declarative UI · Animations ·     │
│    Theming · Accessibility)          │
├──────────────────────────────────────┤
│        WINDOW MANAGER (mywm)         │
│   (Tiling + Floating · Workspaces ·  │
│    Gestures · Blur/Shadow)           │
├──────────────────────────────────────┤
│        COMPOSITOR (mycomp)           │
│   (GPU-accelerated · Wayland-like    │
│    protocol · VSync · HDR)           │
├──────────────────────────────────────┤
│        DISPLAY SERVER PROTOCOL       │
│        (MyDP — Wayland-inspired)     │
├──────────────────────────────────────┤
│        DRM / KMS  (Kernel)           │
├──────────────────────────────────────┤
│        GPU DRIVER (Mesa-like)        │
└──────────────────────────────────────┘
```
*Philosophy: Compositor is the only thing that talks to DRM/KMS. Apps talk to compositor via MyDP sockets (shared-memory buffers). No X11 legacy.*

## 7. FILE SYSTEM LAYER

```text
VFS (Virtual File System)
 ├── MyFS    ← Default journaling FS (ext4-inspired, CoW optional)
 ├── tmpfs   ← RAM-backed
 ├── devfs   ← Device nodes (auto-populated by devmgr)
 ├── procfs  ← Kernel info (/proc)
 ├── sysfs   ← Device tree (/sys)
 └── fuse    ← User-space FS bridge
```

## 8. NETWORKING STACK

```text
Socket API (BSD-compatible)
 ├── TCP / UDP / ICMP
 ├── IPv4 / IPv6
 ├── Netfilter-lite (firewall rules)
 ├── virtio-net / e1000 / WiFi drivers
 └── Loopback
```

## 9. IPC MECHANISM

| Mechanism | Use Case |
|---|---|
| Pipes | Parent-child streams |
| Unix Sockets | Daemon ↔ Client (MyDP, audio) |
| Shared Memory | GPU buffers, large data |
| Message Queues | Async kernel↔user events |
| Signals | Process lifecycle |

## 10. SECURITY MODEL

- Ring 0 / Ring 3 strict separation (x86_64)
- Capability-based process permissions (no raw root)
- Sandboxing via namespaces (mount, PID, net, user)
- ASLR + NX + Stack Canaries enforced kernel-wide
- Module signing required for `.kmod` loading

## 11. BOOT SEQUENCE

```text
UEFI Firmware
  → MyOS Bootloader (custom, EFI stub)
    → Kernel ELF loaded into memory
      → HAL init (ACPI, APIC, PCI scan)
        → Memory manager init (PMM → VMM)
          → Scheduler init
            → VFS + MyFS root mount
              → Load essential .kmod (GPU, input, storage)
                → PID 1: /sbin/myinit
                  → Start daemons
                    → Launch compositor (mycomp)
                      → 🖥️ Desktop ready
```

## AT A GLANCE

| Attribute | MyOS Design Choice |
|---|---|
| Kernel Type | Modular Monolithic (like Linux) |
| Syscall ABI | syscall instruction, ~250 calls |
| Default FS | MyFS (journaling, optional CoW) |
| Init System | myinit (minimal, parallel start) |
| Display | MyDP protocol (Wayland-like) |
| GUI Feel | macOS smoothness + Windows familiarity |
| Target Arch | x86_64 primary, ARM64 planned |
| Package Format | `.mypkg` (compressed tar + TOML manifest) |
| Config Format | TOML everywhere |
