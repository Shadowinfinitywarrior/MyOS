# MYOS Architecture Assessment — 2026-09-19

## Executive summary
MYOS is a from-scratch x86_64 hobby OS with a working BIOS/UEFI boot chain, a 64-bit kernel, basic memory management, scheduler, VFS skeleton, framebuffer driver, PS/2+USB HID input, and a damage-driven compositor with a WM2 desktop demo. The desktop boots to a gradient wallpaper, taskbar with clock, 4 hard-coded desktop icons and a demo welcome window. Core subsystems exist but many are stubs, demos, or hard-coded; userspace is largely absent and the VFS root is never installed.

## Repository structure
- `boot/` – MBR stage1/2/3 variants, UEFI `efi_main.c`, stage assembly
- `kernel/` – 70+ source files: gdt/idt/isr, pmm/paging/heap/slab, scheduler/process, syscalls, timer/apic, acpi, smp, elf, exec, mmap/shm/pipe
- `drivers/` – framebuffer/BGA, fbcon/fbterm, screen, keyboard/mouse PS/2, USB/xhci/hid, pci, virtio-blk/net/gpu, ata/ahci/nvme stubs, ac97, rtc, serial, speaker
- `fs/` – vfs/ramfs/devfs/procfs stubs, ext2 partial, ext4 fake, fat16 read-only
- `gui/` – compositor, surface, blit, wm2, desktop, anim, input, scene/widget stubs, font SDF + font8 bitmap, rect, cursor, perf, debuglog
- `net/` – arp/ip/icmp/udp/tcp/dhcp/dns, mostly stubs
- `lib/` – printf, string, bitmap, ring_buffer, ipc, list, divmod
- `user/` – libc stub, crt0, demo apps: hello, browser, file_explorer, httpd, init, gui_hello
- `scripts/` – linker.ld, linker_user.ld, create_disk.sh
- `Makefile` – monolithic build, host gcc/ld/nasm, no cross-toolchain used

## What works
**Boot**
- BIOS MBR → stage2 → stage3 real→protected→long mode, identity 2 MiB pages, kernel copied to 0x100000 and jumped. Serial debug bytes emitted.
- UEFI loader builds BOOTX64.EFI, loads kernel.bin to 0x100000, calls ExitBootServices. Boot not verified per plan.md.

**Kernel core**
- 64-bit GDT with kernel/user code/data + TSS loaded, 256-entry IDT from `isr64.asm`, PIC remapped, timer PIT/APIC ticking, SYSCALL/SYSRET configured.
- Physical page manager bitmap with fallback 256 MiB region; 4-level paging with 2 MiB bootstrap, paging_map/unmap/clone work for identity mapped area.
- Kernel heap kmalloc/kzalloc/kfree with free-list + coalesce; used by kernel.
- Round-robin pre-emptive scheduler, process_create_kernel/user, fork, sleep/yield, context_switch with fxsave/fxrstor.
- 15 syscalls implemented: exit/fork/read/write/open/close/getpid/sleep/yield/time/putchar/getchar/ps/uptime/execve. User ABI uses int 0x80 vs syscall.

**Drivers**
- BGA linear framebuffer 1024×768×32, software backbuffer + dirty-rect blit, fb_flush_rects.
- PS/2 keyboard with scancode table, repeat, USB HID injection into same queue.
- PS/2 mouse with IntelliMouse detection, USB HID injection.
- PCI config scan, xHCI init + boot HID enumeration in QEMU.
- Virtio-blk skeleton read/write, virtio device discovery.

**Graphics/Compositor**
- Surface abstraction ARGB8888 with damage rects, compositor with damage coalescing, Z-list, direct LFB composition.
- WM2: wallpaper gradient, taskbar 48px, clock, desktop icons painted, cursor 12×20, window chrome, open/close animations.
- Blit with alpha blend, optional SSE2, rect math.
- Animation pool with easing, fixed-size.
- Input ring buffer 1024 entries.

**Filesystem/VFS**
- VFS node API exists, syscalls call vfs_resolve_path but root never set → all lookups fail.
- devfs provides /dev/null/zero/random/console/rtc/uptime, wm_pipe IPC.
- ext2 can mount virtio-blk, read inodes and files with direct/indirect blocks; write incomplete.
- fat16 read-only on ATA, ext4 fake mount prints only.

**Desktop shell**
- `desktop_init` → `wm2_init` → demo welcome window. Boot chain calls `process_create_kernel("desktop",desktop_run)`. Kernel-threaded, not userspace.

## Incomplete / broken / demo-only
- Boot: hard-coded LBA 1/2/66, drive 0x80, load addresses, no ELF parsing, multiple stage variants, UEFI handoff no memory map.
- Memory: pmm_init called with 0/0 → fallback 256 MiB; no E820/UEFI map ingestion; MAX_PAGES 262144 hard limit; heap bump leaks physical pages, assumes identity mapping; slab stub.
- Paging: identity-mapped 1 GiB only, no higher-half kernel mapping, page fault handler panics, huge-page conversion fragile, per-process isolation incomplete.
- GDT/TSS: static 7-entry GDT, single global TSS rsp0 placeholder, no per-process rsp0 update, no IST.
- Scheduler: no priority scheduling, global ready queue, O(N) wake-sleepers scan, scheduler_start empty, no SMP.
- Threads/mutex: pthread/mutex stubs.
- Syscalls: many numbers defined but not wired, copy_from_user only high-half check, user ABI mismatch int 0x80 vs SYSCALL.
- IPC: single global pipe wm_pipe, busy-wait reads, no message queues.
- VFS: root NULL, ramfs_finddir always allocates new node, devfs leaks, no mount table, no dentry cache, procfs print-only.
- FS: ext2 write limited, delete/list stub, sector-boundary bug, ext4 fake, fat16 leak.
- Drivers: fbcon never draws glyphs, fbterm draw empty, framebuffer mode fixed 1024×768, backbuffer 1920×1080 hard-coded, vsync stub, ATA/AHCI/NVMe stubs, USB HID only.
- GUI: widgets only tree, layout is pass-through, paint empty, window manager no drag/resize/hit-test, input pump discards mouse moves, font SDF code exists but UI uses 8×8 bitmap, hard-coded positions/colors/sizes, cursor clamp 1023×767.
- Desktop: kernel-threaded, icons not interactive, start menu missing, no app launcher, no system tray.
- Build: uses host gcc/ld, cross-toolchain built but unused, user linker ELF32 i386 never used, no dependency tracking.

## Dependency graph (high level)
Boot → Kernel entry → GDT/IDT/TSS → PMM → Paging → Heap → Scheduler/Process → Syscall → VFS → Drivers → Framebuffer → Compositor → WM2 → Desktop
Drivers → IRQ → ISR → Scheduler tick
FS → VFS → Syscall → Process
GUI → Surface/Blit → Framebuffer → PMM/Paging
Input → Keyboard/Mouse → Input ring → WM2

## Migration plan aligned to plan.md
Phases 0-20 as documented. Immediate gates:
1. Stabilize boot to UEFI 64-bit with proper memory map handoff.
2. Real PMM from E820/UEFI, proper heap init, page fault handler.
3. Userspace init via execve, process isolation, syscall ABI fix.
4. Real ext2 root, VFS mount, /proc.
5. Renderer abstraction, SDF font, real window manager interaction.
6. Userspace desktop shell and apps.

## First implementation milestone
**Milestone 1 – Kernel & framebuffer stabilization**
Goals:
- Kernel builds with -Werror, boots reliably to desktop on QEMU SeaBIOS.
- PMM uses real memory map or documented fallback, heap initialized at heap_init with size limit, no leaks.
- Framebuffer mode selectable, damage tracking works, compositor does not fallback to full-screen on overflow.

Acceptance:
- `make` succeeds zero warnings.
- `make run` boots to wallpaper, taskbar clock updates, cursor moves.
- Serial log shows `[GDT]`, `[PMM]`, `[PAGING]`, `[COMPOSITOR]` init.
Known limitations:
- VFS root unset, apps are kernel-threaded, fonts are bitmap.

Next step: implement proper heap init and PMM fallback documentation, then move to UEFI boot handoff.
