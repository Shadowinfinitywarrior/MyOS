# MyOS Modern Edition (x86_64)

> **Freedom · Privacy · Performance**  
> *A clean, modular, multi-language 64-bit operating system with an acrylic GUI desktop, functional hardware drivers, and real-time terminal control.*

---

## Overview

**MyOS** is a from-scratch 64-bit (x86_64 / AMD64) long mode operating system built for high performance, modular architecture, and modern desktop usability. It features a preemptive multitasking kernel, comprehensive hardware drivers, a multi-user authentication system, persistent and portable storage layers, and an anti-aliased graphical window manager running at native display resolutions with HiDPI support.

---

## Key Features

### 🖥️ Modern Acrylic GUI & Window Manager
- **1024×768 32bpp Framebuffer Engine**: Crisp, double-buffered linear framebuffer with damage-rectangle tracking and subpixel antialiasing.
- **Ultra-Smooth 200Hz Mouse Engine**: PS/2 and USB HID mouse pointer with 9-bit two's complement delta decoding, software cursor background save/restore (`under_cursor`), drop shadows, and zero-stutter rendering.
- **Dynamic HiDPI Scaling**: Real-time desktop scaling supporting 96 DPI (1.0x), 120 DPI (1.25x), and 144 DPI (1.5x) via the `dpi` command.
- **Theming Engine**: Dynamic color palettes (Tokyo Night, Emerald Forest, Amber Glow, Cyberpunk Neon).
- **Desktop Shell**: Acrylic top status bar, dock, quick-launch navigation bar, and start menu with keyboard navigation.

### 🔐 Security & Multi-User Authentication
- **Secure Login & Lockscreen**: Dedicated lock screen masking passwords with bullet dots and brute-force protection.
- **Default Credentials**: `username: myos` / `password: myos`.
- **Runtime User Management**: Add users (`useradd`), change passwords (`passwd`), list accounts (`users`), and switch active sessions (`login`).
- **Instant Locking**: Lock immediately via terminal command (`lock`) or taskbar lock control.

### 📦 Inbuilt Desktop Applications
- **MyOS Terminal**: Hardware-accelerated terminal emulator with VT100 support, scrollback, canonical line editing, and immediate focus.
- **Calculator**: Clean arithmetic calculator with quick calculation and mouse/keyboard entry.
- **Text Editor**: Multiline text notepad for editing and inspecting configuration and notes.
- **Sound Studio / Music Player**: Synthesizer and audio player utilizing the kernel AC'97 and PC Speaker sound drivers.
- **Settings & Control Center**: System overview, theme switcher, DPI scaler, and mouse sensitivity tuning.
- **Tor Onion Browser**: Inbuilt privacy-focused browser with onion routing emulation and web page rendering.
- **File Explorer & About Dialog**: Visual storage and architecture inspection windows.

### ⚙️ Operating System Kernel & Hardware Drivers
- **Kernel Architecture**: x86-64 higher-half long mode kernel, SMP (multi-core APIC) support, preemptive round-robin scheduler.
- **Memory Management**: Physical Frame Allocator (bitmap-based), 4-level paging (PML4) with copy-on-write (COW), kernel slab heap allocator.
- **Storage Subsystem**:
  - IDE/ATA, AHCI (SATA), NVMe, and VirtIO-Block drivers.
  - GPT partition table parser with EXT4, EXT2, and FAT16 filesystem mounting.
  - Persistent disk data synchronization (`sync`) and portable USB drive detection (`portable` / `usb`).
- **Audio Drivers**: AC'97 sound controller and PC Speaker tone synthesizer (`sound` / `beep`).
- **Networking Stack**: Realtek RTL8139, Intel E1000 Gigabit, and VirtIO-Net drivers with full Ethernet, ARP, IPv4, ICMP, UDP, TCP, and DHCP stack.
- **Bus & System Drivers**: PCI bus scanner and device registry (`lspci`), APIC, ACPI power management, HPET/PIT timers, and CMOS RTC clock.

### 💻 Multi-Language Userspace
- **C / C++**: Core kernel, driver primitives, and user utilities.
- **Rust**: Embedded `no_std` GUI core (`gui/rust/libmyos_gui.a`).
- **Go / TinyGo**: Modern desktop shell components (`gui/go/shell`).
- **Java**: Bytecode runtime native GUI binding (`gui/java_binding.c`).
- **Python**: Embedded MicroPython scripting runtime.

---

## Directory Structure

```
myos/
├── boot/                   # BIOS MBR (stage1, stage2, stage3) and UEFI bootloaders
├── kernel/                 # x86-64 long mode kernel (scheduler, memory, syscalls, SMP)
├── drivers/                # Hardware device drivers (storage, video, net, sound, input)
├── fs/                     # Virtual Filesystem (VFS, devfs, procfs, ext2/ext4, fat16)
├── gui/                    # Modern desktop GUI, window manager, apps, compositor, themes
│   ├── rust/               # Rust no_std GUI static library
│   ├── go/                 # Go/TinyGo desktop shell components
│   └── fonts/              # Antialiased baked bitmap fonts
├── net/                    # TCP/IP network stack (Ethernet, ARP, IP, ICMP, UDP, TCP)
├── user/                   # Userspace programs (sh, calc, cat, ls, ps, kill, libc)
├── include/                # Kernel and userland C header files
├── lib/                    # Standard utilities (string, printf, ring buffers)
├── docs/                   # Architectural specifications and syscall ABI references
│   ├── architecture/       # System and GUI architecture specifications
│   └── ABI/                # 64-bit SysV syscall conventions
├── scripts/                # Disk creation, Java build, and linker scripts
└── Makefile                # Master unified build system
```

---

## Building & Running

### Prerequisites

On Ubuntu / Debian:
```bash
sudo apt update
sudo apt install build-essential gcc nasm qemu-system-x86 genisoimage imagemagick
```

Optional for multi-language components:
- **Rust**: `rustup target add x86_64-unknown-none`
- **Go**: `tinygo` (version 0.30+)
- **Java**: OpenJDK 17+

### Compilation

Build the complete operating system, embedded userland, fonts, and bootable disk image:
```bash
make all
```

This generates `build/myos.img` containing the MBR bootloader, 64-bit kernel, drivers, and user binaries.

### Running in QEMU

- **Graphical Desktop Mode** (recommended):
  ```bash
  make run
  ```
- **Console / Serial Mode**:
  ```bash
  make run-console
  ```
- **Headless Mode** (automated testing):
  ```bash
  make run-headless
  ```

---

## Terminal Commands Quick Reference

Once booted, log in with `myos` / `myos`. Type `help` in the terminal to view available commands:

| Command | Description |
|---------|-------------|
| `help` | Display terminal command center manual |
| `whoami` | Show current active logged-in user |
| `users` | List all users in OS database |
| `useradd <user> <pass>` | Register a new user account |
| `passwd <user> <pass>` | Update password for user |
| `lock` / `logout` | Lock screen and present login card |
| `storage` / `df` | Inspect active storage mounts and filesystems |
| `sync` | Flush and commit system data to persistent storage |
| `portable` / `usb` | Inspect removable USB devices |
| `drivers` | List active kernel hardware drivers |
| `lspci` | Scan and list all PCI hardware devices |
| `calc` | Launch arithmetic Calculator app |
| `editor` | Launch multiline Text Editor |
| `music` | Launch Sound Studio & Player |
| `settings` | Launch Control Center & Display Settings |
| `tor` / `browser [url]` | Launch inbuilt Tor Onion Browser |
| `wm list` | List open GUI windows |
| `wm tile` | Auto-tile all windows on desktop |
| `wm focus <title>` | Bring window to front and focus |
| `wm close <title>` | Close window by title |
| `dpi [96\|120\|144]` | Query or set HiDPI desktop scale factor |
| `sound [freq] [ms]` | Synthesize audio tone via AC'97 / Speaker |
| `theme <name>` | Switch theme (`tokyo`, `emerald`, `amber`, `cyberpunk`) |
| `mouse [1-10]` | Query or set mouse pointer sensitivity |
| `ps` | Display active processes and threads |
| `free` / `mem` | Display physical memory allocation |
| `clear` | Clear terminal display |
| `reboot` / `shutdown` | Restart or power down machine |

---

## License

MyOS is open-source software provided under the MIT License.
