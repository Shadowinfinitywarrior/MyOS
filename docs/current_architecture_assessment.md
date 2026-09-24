# MYOS Current Architecture Assessment

Date: 2026-09-21

## Build and Boot Status
- Build: `make clean && make all` succeeds with `-Werror`. Produces `build/myos.img` ~10 MiB.
- Boot: BIOS SeaBIOS → stage2 → stage3 → kernel_entry.asm → kernel_main_64. Serial output shows:
  ```
  K
  S
  MyOS starting...
  1
  Kernel loaded at 1MB
  2
  [GDT] 64-bit GDT loaded ...
  [ISR] Initialized
  [PMM] init: 1 region(s), total_pages=..., free_pages=...
  [PMM] using fallback region after __kernel_end
  paging enabled
  ```
  Boot reaches desktop_init. Scheduler starts. Stable boot in QEMU.

## Working Components

### Boot
- BIOS boot chain: boot_sector_min.asm → stage2.asm → stage3.asm
- stage3 enables A20, sets up GDT, PAE, 2 MiB identity mapping for first 1 GiB, long mode, copies kernel from 0x70000 to 0x100000, calls kernel at 0x100000.
- UEFI loader exists in boot/efi_main.c but not actively used in BIOS image; it loads kernel.bin to 0x100000, obtains memory map, exits boot services, jumps. Memory map not passed to kernel.

### Kernel Core
- kernel/kernel.c: kernel_main_64, BSS zero, serial/screen init, GDT/TSS/IDT/PIC/timer init.
- GDT 64-bit, TSS loaded, IDT 256 entries via isr64.asm stubs.
- PMM: bitmap over MAX_PAGES 262144, pmm_init() fallback to 256 MiB region after __kernel_end when mmap_addr==0. Real E820 map not ingested.
- Paging: paging_init() identity maps; stage3 provides 2 MiB identity mapping.
- Heap: kernel heap at 0x4000000 (kmalloc/kzalloc/kfree). Init succeeds.
- Scheduler: round-robin, process_create_kernel, process_create_user stub. Idle PID0.
- Syscalls: syscall_dispatch + syscall_entry64 via SYSCALL/SYSRET, also int0x80 path mixed.
- Context switch: kernel/context_switch.asm 64-bit, saves callee-saved regs, fxsave/fxrstor, proper rsp/rip restore. enter_usermode via IRETQ.
- ISRs: isr64.asm pushes all regs, switches DS/ES/FS/GS to kernel data selector 0x10, calls isr_handler. Timer IRQ0 handled.

### Drivers
- Serial COM1, screen text mode, framebuffer driver fb_init.
- Keyboard PS/2 IRQ1, mouse PS/2 IRQ12, input ring buffer.
- PCI, ATA/AHCI, virtio_blk/net/gpu, USB/XHCI stub.
- Framebuffer: BGA mode, 32-bit RGBA.

### Filesystem / VFS
- VFS abstraction with ramfs + devfs. Root is ramfs, /dev populated.
- ext2/ext4 read-only partial, FAT16.
- No persistent root; ramfs is volatile.

### Graphics / Compositor
- Renderer abstraction: gui/renderer.c with fb_renderer_*.
- Surface system, damage tracking rects, COMPOSITOR_MAX_FRAME_DAMAGE.
- Compositor: compositor_t with layer linked list, z-order, dirty flags, damage snapshot per frame, renderer_clear + blit per damage rect.
- WM2: wm2_init/run, wallpaper, taskbar, windows with drag/resize/min/max/close, stacking.
- Font: stb_truetype style renderer in gui/font.c, font_load("/boot/fonts/DejaVuSans.ttf",16). Fallback to 8x8 bitmap if missing.
- Theme system initialized.
- UI toolkit widgets: Widget, Container, Label, Button, TextBox, CheckBox, Slider.

### Applications / Desktop Shell
- Desktop init → wm2_init → taskbar + desktop icons.
- App registry exists gui/app_registry.c.
- Stub apps: settings_app, files_app, taskmanager_app, sysinfo_app. Created as kernel threads, not userspace ELF.

## Incomplete / Broken

- Memory map handoff: pmm_init called with 0,0 → uses fallback region. No E820/UEFI map passed from bootloader.
- Paging: identity only, no higher-half kernel, no user address space isolation.
- Page fault handler: vector 14 unhandled → kernel_panic with CR2 print.
- Context switch pre-emption: timer tick may cause issues; scheduler_schedule called after first switch.
- Syscall ABI inconsistent: int0x80 vs SYSCALL.
- VFS root ramfs not persistent; no real disk mount for root.
- ext2 write incomplete.
- Font atlas loading depends on /boot/fonts/DejaVuSans.ttf present in image? Likely missing → fallback.
- Userspace ELF exec not working: process_create_user exists but no working ELF loader for apps.
- Apps run as kernel threads, no isolation.
- Vsync stub, no double buffering guarantee.
- Compositor damage overflow: compositor_request_frame returns on overflow, damage lost.
- GPU backend not implemented.
- UEFI boot not verified in QEMU OVMF.

## Hard-coded / Demo Elements

- Desktop icons and taskbar layout hard-coded in wm2/desktop code.
- Wallpaper color hard-coded.
- Font path hard-coded.
- Kernel heap size fixed.
- Fallback PMM region fixed to 256 MiB.

## Reusable Components

- Context switch 64-bit, ISR stubs, serial logging.
- PMM bitmap, heap.
- Renderer abstraction and surface system.
- Compositor damage tracking design.
- WM2 window basics.
- Font rasterizer code.
- VFS interface.

## Migration Plan – Phase 0→1

1. Stabilize boot memory map
   - Add E820 query in stage3.asm before long mode, pass map pointer/address to kernel via e.g. EBX or dedicated memory region.
   - Update kernel_entry to preserve map pointer and pass to kernel_main_64.
   - Update pmm_init to parse real map.
2. Harden paging
   - Implement page fault handler that logs and optionally recovers.
   - Move kernel to higher half.
3. Heap / PMM safety
   - Log real map vs fallback, assert heap not overlapping kernel.
4. Compositor damage
   - Merge overflow damage into full damage rect instead of dropping.
   - Add serial log of damage count per frame.
5. Font
   - Verify font file in image or embed fallback TTF.
6. Build/test loop
   - After each change: make clean && make all, qemu -nographic -serial mon:stdio, confirm logs.

## Dependency Graph Summary
boot → kernel_entry → kernel_main_64
kernel_main_64 → GDT/TSS/IDT → PMM → paging → heap → scheduler → init_phase8 → VFS → desktop_init → wm2 → compositor → renderer → framebuffer
Input: keyboard/mouse → input subsystem → wm2 events
Apps: kernel threads → wm2 windows

## First Milestone Target
Kernel & framebuffer stabilization:
- Clean build with -Werror
- Reliable boot to desktop in QEMU
- PMM uses real memory map or documented fallback with logging
- Heap initialized and functional
- Damage tracking works without overflow loss
- Serial logs for GDT/PMM/PAGING/COMPOSITOR visible

Status: Build clean ✓, boot reliable ✓, PMM fallback documented ✓, damage tracking present but overflow not handled. Next: implement E820 map handoff and page fault handler.
