# MyOS — Agent Task List: Demo OS → Real 64-bit GUI OS

**Goal:** Remove every simulation/mock/stub and turn this into a genuinely working x86_64 GUI OS with a Windows 11-style desktop, built from scratch.

**Estimated effort:** Multi-month, phase-ordered. Each phase has a hard acceptance gate — do not proceed until the gate passes.

---

## Directive 0 — Ground Rules (apply to every task)

- [ ] **Delete, don't hide.** Remove all simulated/fake code paths (fake acrylic, fake battery, fake network status, RTC returning fixed `2026-01-01`, `fb_wait_vsync` timer stub, print-only DHCP/DNS). No `DEMO`/`MOCK`/`TODO` fallback branches. Unsupported hardware returns an error — never a plausible lie.
- [ ] **No floats in hot paths.** Remove `float dt_sec` in `desktop.c:153`; use fixed-point (`Q16.16`) for animations and timing.
- [ ] **Fail loudly.** Every stub (`return 0`, `return -1`, empty body) is either implemented or deleted. `grep -rn "TODO\|FIXME\|stub" kernel/ drivers/ fs/ net/ gui/` returns zero hits by Phase 5.
- [x] **Fix the build.** Remove `-Wno-error` from `Makefile:7`. Build with `-Wall -Wextra -Werror`. Add CI.
- [ ] **One window manager.** Delete `gui/wm.c` (legacy, 8000+ lines, immediate-mode) or `gui/wm2.c`. Keep exactly one compositor-based WM. Delete the `MYOS_WM2` switch and the dead legacy desktop path.
- [ ] **Userspace shell in final state.** During bring-up the desktop may run as a kernel thread, but by Phase 6 the shell is a userspace ELF spawned via `execve`. Delete the `desktop_run` kernel-thread spawn in `init_phase8` and `init.c:8`'s "Initialize GUI for demo" comment.
- [ ] **Architecture target:** x86_64 Long Mode. Port the kernel to 64-bit **before** any feature work. UEFI boot (real `efi_main`) that loads the kernel properly. BIOS MBR/CHS boot files still present (`boot/boot_sector.asm`, `boot/stage2.asm`); remove legacy real-mode boot or retain for SeaBIOS compatibility only.
- [ ] **QEMU is dev hardware, not the platform.** Primary targets: virtio-gpu, virtio-blk, virtio-net per spec. No hard-coded PCI vendor/device IDs (fix `usb.c:6` QEMU IDs, BGA 256×32 scan). Enumerate properly.

---

## Phase 1 — x86_64 Port & Boot (blocks everything)

- [ ] 1.1 Replace boot chain: UEFI application (`efi_main.c`) → load kernel ELF → parse memory map → exit boot services → jump to 64-bit entry. BIOS boot files (`boot/boot_sector.asm`, `boot/stage2.asm`, `boot/stage3.asm`) still present and used for `myos.img`; UEFI image builds but boot not verified.
- [x] 1.2 Set up 64-bit GDT, per-CPU TSS, IDT with full ISR table, SYSCALL/SYSRET.
- [x] 1.3 Port `kernel/timer.c`: replace non-atomic `volatile uint64_t ticks` with per-CPU tick counter using proper atomic read (or HPET/APIC timer). Timer tick must invoke the scheduler, not just update `total_time`.
- [ ] 1.4 Port `kernel/context_switch.asm`: file still 32-bit (`[bits 32]`), uses 32-bit registers and `iret`. ISR stubs in `kernel/isr.asm` also 32-bit. 64-bit `isr64.asm` exists but not used by Makefile. Context switch and ISR need 64-bit rewrite.
- [x] 1.5 Serial output (8250/16550 + QEMU debugcon) as early logging backend — keep for the whole project.
- [ ] 1.6 **Acceptance:** Boots to a 64-bit kernel shell on serial under both OVMF and SeaBIOS; zero warnings; paging enabled with higher-half kernel. Not yet verified — QEMU UEFI boot hits #UD at RIP 0xB0000; 32-bit ISR/context switch likely cause.

## Phase 2 — Memory & Paging (blocks processes/FS/GUI buffers)

- [x] 2.1 Real 4-level page tables: `paging_switch_directory` writes CR3; per-process address spaces; kernel half mapped into every directory. Remove the NULL-page-table bail at `kernel/paging.c:16-19`.
- [x] 2.2 `paging_clone_directory` → proper deep copy with CoW anon pages or ref-counted frames. No `memcpy` into index 1.
- [x] 2.3 Replace bump allocator (`kernel/pmm.c:3`, `pmm_next_phys = 0x200000`) with a free-list/bitmap allocator over the E820/UEFI memory map. Implement `pmm_free_page` for real.
- [ ] 2.4 Replace linear `kernel/heap.c` with a real kmalloc (freelist + slabs or bin allocator). `kfree` reclaims. `kmalloc(0)` → NULL; frees validated.
- [ ] 2.5 `process_create_user` maps the user stack in the new page table **before** the CR3 switch (currently calls the no-op switch). `process_destroy` walks and frees user pages (kill the `/* TODO: Free user-space pages */` at `kernel/process.c:184`). ELF loader maps at `p_vaddr` — no identity-mapping assumption (`elf.c:75-83`).
- [ ] 2.6 **Acceptance:** `mmap`/`munmap` syscalls work; two processes fault-independently (one SIGSEGVs, the other survives).

## Phase 3 — Scheduler, Processes, Syscalls

- [x] 3.1 Pre-emptive scheduler (round-robin first, then MLFQ or CFS-lite): timer tick triggers context switch. Remove the pre-emption disable at `kernel/scheduler.c:126-133`. Implement `scheduler_wake_sleepers` as a real sleep queue (sorted/priority on wake tick) — currently empty.
- [ ] 3.2 Process table: PIDs, parent/child, `fork`/`execve`/`wait`/`exit`, zombie reaping.
- [ ] 3.3 Syscall layer (~40): read/write/open/close, mmap, fork, execve, exit, wait, sleep, futex-lite. Every pointer argument through `copy_from_user`/`copy_to_user` with page-fault-safe access.
- [ ] 3.4 Kernel `kernel/init.c:14` stops looping and `execve("/sbin/init")` a userspace ELF. Rewrite `user/init.c` as a real init that spawns `/bin/desktop` (or reads `/etc/init.rc`).
- [ ] 3.5 `user/hello.c` runs as a userspace ELF in its own address space — proves Phase 2/3.
- [ ] 3.6 **Acceptance:** `fork()` stress with 100 processes; sleepers don't burn CPU; init is userspace.

## Phase 4 — Drivers (storage, display, net, input)

- [ ] 4.1 **Virtio-blk**: full virtqueue implementation (split queues OK), IRQ-driven completion, real block read/write. Delete `ahci.c`/`ata.c`/`nvme.c` stubs entirely (re-add AHCI only in stretch phase) behind a clean `block_device_t` interface.
- [ ] 4.2 **Virtio-net**: rx/tx virtqueues, MAC from config space, IRQ integration. Wire into `net_current_driver` (currently `NULL`, `net/net.c:4`).
- [ ] 4.3 **Virtio-gpu** (currently empty): 2D mode, scanout from allocated framebuffer, resource flush. Replaces the `framebuffer.c` BAR hack (kill the "write straight through to the BAR" comment at `framebuffer.c:138`). Keep linear-FB fallback for `bochs-display`/`stdvga` only.
- [ ] 4.4 **Vsync/damage:** real vblank IRQ (virtio-gpu or bochs-display). Delete the timer-stub `fb_wait_vsync` (`framebuffer.c:213`). Double/triple buffering in the compositor.
- [ ] 4.5 **Input:** PS/2 (mouse+kbd) IRQ1/IRQ12 and/or virtio-input. Rewrite `gui/input.c:31-41`: lock-free SPSC ring (1024 entries), drain motion accumulation **every frame**, no silent drops (backpressure coalesces moves, never loses buttons/keys). Fix `wm2.c:215-219` — remove the `ev.type==4` mouse-only filter so keys and clicks reach apps.
- [ ] 4.6 **RTC/ACPI:** real `rtc_get_time` (CMOS/ACPI) — delete the fixed `2026-01-01` return. Parse ACPI tables for shutdown/reboot. Battery/network status queries real sources or shows "unavailable" — never simulated (`wm.c:1256-1276,1708-1716,1803-1824` are the simulated versions to delete).
- [ ] 4.7 **SMP decision:** either real bring-up (per-CPU stacks, per-CPU run queues, IPI TLB shootdown + reschedule, locks everywhere) **or** strict single-CPU and remove `-smp 4` from the Makefile. Delete the dead AP code in `kernel/smp.c:29-33` (APs currently print + hlt). Recommendation: single-CPU first; SMP is Phase 8.
- [ ] 4.8 **Acceptance:** Boot self-tests: read known virtio-blk sectors; receive ARP reply or loopback ping; mouse moves cursor with zero event loss; display flips at 60 Hz on a tearing test.

## Phase 5 — Filesystem & VFS

- [ ] 5.1 VFS: full path resolution (`vfs_resolve_path` currently walks one level), dentry/inode caches, per-process fd table, mount table. `ramfs_finddir` must actually search (currently always NULL).
- [ ] 5.2 Implement **one** real read-write FS: **ext2 first** (simple, documented). Fix `ext2_get_inode` sector-boundary bug (`fs/ext2.c:39-40`), honor block size (currently 1 sector/block regardless), implement `write`, `truncate`, `mkdir`, `unlink`, inode + block bitmap allocation. **Delete the fake ext4 mount** (`fs/ext4.c:12-13` — prints "Magic check passed" without reading the superblock).
- [ ] 5.3 Either fix FAT16 sub-dir traversal + `dir_buf` leak, or drop FAT entirely. Fix `devfs_finddir` allocating a new node per lookup (leak); make `/dev` a real device FS (block devices, null/zero/tty). Remove the unreachable `wm_pipe` override.
- [ ] 5.4 procfs: real dynamic listing — `/proc/<pid>/status`, `mem`, `fd` from live kernel data. Delete the print-and-return-NULL `procfs_init`.
- [ ] 5.5 Locking: per-inode rwlocks at minimum; no global FS lock. No permissions enforcement required yet, but design for it.
- [ ] 5.6 **Acceptance:** Boot from ext2 root on virtio-blk. Userspace shell: `ls`, `cat`, `echo > file`, mkdir/rmdir — files survive reboot. `devfs` exposes the block device. Concurrent file ops don't corrupt.

## Phase 6 — Network Stack (real)

- [ ] 6.1 ARP: real table with aging, reply handling, entry install. `arp_resolve` returns real MACs; queue packets pending resolution (currently always -1).
- [ ] 6.2 IPv4: fix `ip_send` (`net/ip.c:33` — currently sends `pkt + sizeof(ip_hdr_t)` dropping the header and mis-sizing). Checksum validation, reassembly, routing table with default gateway.
- [ ] 6.3 ICMP: echo request/reply that actually transmits (current handler rewrites the header in memory but never sends).
- [ ] 6.4 UDP: full socket layer.
- [ ] 6.5 TCP: real state machine (SYN/SYN-ACK, seq/ack tracking, retransmit timer, RTT estimation, windowing, FIN teardown). Currently no state machine/retransmit/ACK validation. **Hardest item in the project — budget accordingly.**
- [ ] 6.6 DHCP: real discover/offer/request/ack client with lease renewal (currently print-only, `dhcp.c:6`). DNS: real resolver over UDP (currently hard-coded, `dns.c:13`).
- [ ] 6.7 **Acceptance:** From inside MyOS: ping the gateway, DHCP an address, fetch a file over TCP from a host, resolve a DNS name.

## Phase 7 — GUI: Windows 11-style Compositing Desktop

- [ ] 7.1 Keep one compositor (wm2). Fix documented bugs in `gui/compositor.c`:
  - [ ] Damage tracking: per-surface damage rects accumulated during the frame, unioned at frame end. Remove the `frame_damage_count==0 → full-rect auto-collect` fallback (`compositor.c:91-98`) and the `frame_damage_count = -1` overflow hack in `compositor_request_frame`.
  - [ ] `surface_fill` must add a damage rect, not set `full_damage=1` (`surface.c:58`). `surface_swap_dirty_cursor` must use cursor-rect damage only (`surface.c:81`).
  - [ ] Implement `anim_invalidate_surface` for real (currently no-op, `anim.c:147-152`). Scene-graph invalidation must mark regions dirty — replace the log stubs (`scene.c:60-65`, `paint.c`).
  - [ ] Per-rect single copy; eliminate the 3 MB/frame full backbuffer copy and the per-rect double copy. Damage generated during a frame must not be lost.
  - [ ] Remove `serial_printf` from the frame path.
- [ ] 7.2 Real Mica: per-frame downscaled blur of the desktop backdrop behind each window + precomputed noise texture + luminosity tint. Cache per-window backdrop; invalidate on window move. Replaces the fake blend (`wm.c:1303-1318` — being deleted, reimplement properly in compositor).
- [ ] 7.3 Windows 11 visual spec: rounded corners (real rounded-rect raster path), centered taskbar with centered icons, start menu (pinned + all-apps grid), system tray with real RTC clock, snap layouts (Win+Arrow + hover on maximize), virtual desktops (real, compositor-backed), Fluent animations (Q16.16 fixed-point dt, 60 fps budget — compositor ≤ 8 ms/frame).
- [ ] 7.4 Fonts: replace hard-coded 8×8 bitmap with a real renderer — bundle a compact TTF + rasterizer (stb_truetype-style). Full Unicode text layout; subpixel rendering optional.
- [ ] 7.5 Hit-testing & input routing: compositor dispatches mouse/keyboard to focused **userspace** windows via shared-memory surfaces + event queues (Wayland-lite protocol over shm + unix sockets).
- [ ] 7.6 **Acceptance (perf):** Drag a window smoothly at 60 fps with 10 windows open; taskbar clock tracks RTC; battery icon shows real ACPI status or hides; memory stable over 1 hour idle (kmalloc accounting shows no leaks).

## Phase 8 — Userspace, Shell, Apps, Tooling

- [ ] 8.1 Minimal libc: malloc/free, stdio, string, printf; pthread-lite over your futex. Static linking; dynamic linker optional/stretch.
- [ ] 8.2 `/bin/desktop` shell: taskbar, start menu, window-management IPC server.
- [ ] 8.3 Apps — each a separate userspace ELF: terminal (spawning `/bin/sh`), file manager (real FS), notepad (real save), settings, calculator.
- [ ] 8.4 Rewrite `tests/qa/run_qa.sh`: scripted QEMU runs (QMP/monitor + serial expect) with **assertions** — boot time, measured 60 fps drag test, ping test, file persistence, fork stress. Fill `bench.md` with real numbers; CI fails on regression. Replace `tests/test_stub.c` (currently blocks forever in `wm2_run`) with real tests.
- [ ] 8.5 **Acceptance:** Full boot → login-free desktop → launch apps → edit/save file → reboot → file persists. All QA scripted.

## Phase 9 — Stretch (in order)

- [ ] 9.1 SMP bring-up (if skipped in 4.7)
- [ ] 9.2 ext4 + journaling
- [ ] 9.3 AHCI/NVMe drivers
- [ ] 9.4 USB (xHCI, real enumeration — no QEMU hard-coded IDs)
- [ ] 9.5 Audio (real AC97 PCM — delete the `playing=true` fake at `ac97_play`)
- [ ] 9.6 Power management / suspend

---

## Execution Protocol

1. Work strictly phase-ordered; each acceptance gate is hard. No GUI polish before Phase 5 passes.
2. After every phase: update `plan.md` to reflect *completed* work, delete obsolete stub notes, make CI green.
3. Commit checkpoint per phase with tags: `p1-64bit`, `p2-mem`, `p3-sched`, `p4-drivers`, `p5-fs`, `p6-net`, `p7-gui`, `p8-userspace`.
4. If a phase is infeasible, **stop and report the blocker** — never fabricate an acceptance test.
5. `memory/stage*.md` are design notes — fold anything still valid into real docs; delete the rest.

## First Three Tasks (start here)

- [ ] **Task A:** Port boot to UEFI 64-bit; delete BIOS boot files; enable `-Werror`. UEFI loader builds, BIOS files still present, boot not verified.
- [x] **Task B:** Rewrite `pmm.c` + `paging.c` with real 4-level page tables and a free-list allocator.
- [ ] **Task C:** Rewrite `context_switch.asm` + scheduler pre-emption + sleep queue. `context_switch.asm` still 32-bit; scheduler pre-emption may work but needs 64-bit context switch.
