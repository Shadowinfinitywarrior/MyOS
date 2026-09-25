# MyOS — Agent Task List: Demo OS → Real 64-bit GUI OS

**Goal:** Remove every simulation/mock/stub and turn this into a genuinely working x86_64 GUI OS with a Windows 11-style desktop, built from scratch.

**Estimated effort:** Multi-month, phase-ordered. Each phase has a hard acceptance gate — do not proceed until the gate passes.

> **AUDIT 2026-09-25 — read this first.**
> Every checkbox below was re-verified against the source. The previous version of this
> plan was stale in two directions and understated reality in one, overstated it in another:
>
> - **Marked incomplete but actually done:** 1.4 (64-bit context switch + `isr64.asm`),
>   2.4 (real kmalloc with coalescing `kfree`), 1.3, 1.5, 2.1, 3.5, 4.7 half
>   (`-smp 1`), 4.6 RTC (real CMOS reader — the fixed `2026-01-01` is gone),
>   4.4 `fb_wait_vsync` (real bounded port poll, not a timer stub), 9.4 xHCI body (887
>   lines, genuinely real). **The old 1.4 text and its `gui/…`/`isr.asm` citations were fiction.**
> - **Presented as existing but does not exist at all:** the **entire `gui/` directory**
>   (no `wm.c`, `wm2.c`, `compositor.c`, `surface.c`, `anim.c`, `scene.c`, `input.c`).
>   Phase 7 is therefore a **greenfield build spec, not a bug list** — every
>   `compositor.c:91-98`-style citation in the old plan pointed at nothing. Similarly 8 of
>   the 9 Phase 8 "apps" are uncompilable design references, not work.
> - **The true critical path is blocked by two small, surgical defects** (see
>   "True Critical Path" below) — neither is a month-long project; both are a day.
>
> Legend: `[x]` verified done · `[~]` partially done (state the gap inline) · `[ ]` not done.

---

## True Critical Path (unblocks every later phase; do these first)

- [ ] **CP-1 — virtio is dead in every run target.** `drivers/virtio.c:10-11,67` probes
  legacy MMIO `0xD0000000` only, while `Makefile:148,151,154` request `-device
  virtio-blk-pci`/`virtio-net-pci`. Live log: `[VIRTIO] 0 devices`. **No block device ⇒
  no persistent FS ⇒ Phase 5/8.5 unreachable.** Add PCI transport enumeration.
- [ ] **CP-2 — no userspace entry point.** `kernel/init.c:6-13` `init_start()` (which would
  `process_create_user("/sbin/init")`) is **never called** (only its decl in `init.h:4`).
  The live shell is a kernel thread (`init_phase8.c:180 shell_dummy`).
- [ ] **CP-3 — address spaces are capped at 7.** `kernel/paging.c:18-20` has 8 static
  directory slots with a monotonic `next_idx` (`paging.c:261-265,334-335`) that is **never
  reclaimed**; `kernel/process.c:251-280` frees user frames but not cloned page-table
  frames or slots. `process.h:8` allows 256 processes — the 100-fork gate (3.6) is
  impossible until slots are recycled.
- [ ] **CP-4 — the user-pointer ABI is unsafe under SMEP.** `kernel/syscall.c:34-45,80-120`
  `memcpy`s from/to user pointers after only a range check, while
  `kernel/syscall64.c:67-73` enables SMEP → those accesses fault. Separately
  `user/libc.c:54-65` **truncates 64-bit user pointers to `int`** in `read`/`write`.
  Route all through `copy_from_user`/`copy_to_user` (or `stac`/`clac`).
- [ ] **CP-5 — QA harness cannot fail.** `tests/qa/run_qa.sh:18-21` greps for four log
  strings (`[COMP]`, `[INPUT] ring overflow`, `[RTFIX]`, `fps=`) that **no code emits**, and
  its `grep -c … && echo FAIL || echo PASS` inverts the result. Every phase gate is
  currently a no-op. Replace with QMP-driven assertions + CI. Highest leverage per line
  in the repo: it is what makes any "done" claim trustworthy.

---

## Directive 0 — Ground Rules (apply to every task)

- [~] **Delete, don't hide.** Real RTC now exists (`drivers/rtc.c:15-36` CMOS reader).
  Remaining fakes: `drivers/virtio_gpu.c:7-9` empty init/flush, `net/net.c:6-11,27` no
  active driver + empty poll, `net/dhcp.c:5-8` hard-coded MAC/address state,
  `drivers/framebuffer.c:75-76` fixed-physical-address fallback.
- [ ] **No floats in hot paths.** Floats live in `user/terminal.c:165,1447-1595` and
  `user/browser.c:83-149,316-327` — but both are **excluded from the build** and depend on
  the nonexistent `gui/`. Blocked on Phase 7 existing; enforce Q16.16 when it does.
- [ ] **Fail loudly.** 41 raw `TODO|FIXME|stub` hits; ~21 are real (e.g.
  `kernel/dynlink.c:5-8`, `kernel/pthread.c:6-12`); the rest are legitimate `isr_stub_*`
  symbol names. Zero real stubs is the Phase 5 gate.
- [~] **Fix the build.** `-Wall -Wextra -Werror` active, no `-Wno-error` (`Makefile:7-8`).
  **No CI** (no `.github/`). Only automated target is `make qa` (`Makefile:159-160`).
- [ ] **One window manager.** Moot until one exists: `gui/` is absent; `Makefile:84-92` has
  empty GUI/app source lists.
- [ ] **Userspace shell in final state.** Shell is a **kernel thread**:
  `kernel/init_phase8.c:66-125,179-180` (`shell_dummy`). `kernel/init.c:6-13` defines an
  uncalled userspace-init path. `kernel/syscall.c:221-252` `execve` *spawns* and returns a
  PID instead of replacing the current image.
- [~] **Architecture target.** 64-bit reached via BIOS stage 3
  (`boot/stage3.asm:198-221`). UEFI loads a raw `kernel.bin`, **not an ELF**, and passes no
  memory map (`boot/efi_main.c:46-75`); `ExitBootServices` result ignored.
  `Makefile:19` builds the BIOS image; UEFI is a separate target.
- [ ] **QEMU is dev hardware, not the platform.** Hard-coded IDs: `drivers/usb.c:8-16` and
  `drivers/xhci.c:228` match `0x1B36:0x000D`; `drivers/framebuffer.c:48-59,75-76` accepts
  fixed device IDs + fixed fallback; `drivers/virtio.c:10-11,65-73` assumes fixed MMIO
  (see CP-1).

---

## Phase 1 — x86_64 Port & Boot (blocks everything)

- [~] 1.1 Boot chain: BIOS path is real. UEFI (`boot/efi_main.c`) loads `kernel.bin` into
  UEFI pool and copies to `0x100000` (`:46-75`); calls `GetMemoryMap` but never parses the
  descriptors nor passes them to the kernel; ignores the `ExitBootServices` result and jumps
  (`:79-83`). Needs: ELF loader + memory-map handover + checked boot-services exit.
- [~] 1.2 64-bit GDT (`kernel/gdt.c:45-68`), 256-gate IDT (`kernel/idt.c:43-50`), SYSCALL
  (`kernel/syscall64.c:45-97`) all real. Gap: **single static TSS with placeholder `rsp0`**
  (`kernel/tss.c:29-47`) and a **global** `syscall_cpu` array
  (`kernel/syscall64.c:23-31`) — not per-CPU as specified.
- [x] 1.3 Per-CPU tick counter: `kernel/timer.c:11-29` uses `per_cpu_ticks[MAX_CPUS]`,
  resolves the APIC CPU id, and calls `scheduler_tick()`; preemption at
  `kernel/scheduler.c:110-117`.
- [x] 1.4 **64-bit context switch and ISR stubs.** `kernel/context_switch.asm:5,20-76,96-120,130-148,162-223`
  is `[bits 64]` with `iretq`/`sysretq`; `kernel/isr64.asm` generates the 64-bit stubs and
  **is** the wired source (`Makefile:71-75`; `isr_stubs.o` built from it, in `ALL_OBJS`).
  *(The old note claiming this was still 32-bit and unused was stale.)*
- [x] 1.5 Serial backend: `drivers/serial.c:6-29` configures COM1, bounded UART polling,
  plus QEMU debug port.
- [ ] 1.6 **Acceptance:** not met. No evidence of a serial userspace shell under **both**
  OVMF and SeaBIOS. Old blocker "UEFI #UD at RIP 0xB0000, 32-bit ISR likely cause" — the
  32-bit-ISR theory is now disproven (1.4 is done); the UEFI loader is the live suspect.

## Phase 2 — Memory & Paging (blocks processes/FS/GUI buffers)

- [x] 2.1 Real 4-level page tables: `kernel/paging.c:29-75` allocates PML4/PDPT/PD, maps
  the low 1 GiB, adds the high-half alias, writes CR3; NULL-bail removed. Per-process
  directories + active-dir switching at `kernel/paging.c:240-249,338-342`.
- [~] 2.2 `paging_clone_directory` deep-copies and does not blindly copy index 1
  (`kernel/paging.c:261-335`), but there is **no CoW bit and no frame refcount** —
  `kernel/process.c:434-447` eagerly copies user pages instead.
- [~] 2.3 PMM consumes BIOS E820 and allocates/frees (`kernel/pmm.c:23-85,114-162`), but
  caps at 1 GiB with a 256 MiB fallback (`kernel/pmm.c:7,57-74`) and the UEFI memory map
  is never integrated (see 1.1).
- [x] 2.4 **Real `kmalloc`/`kfree`.** `kernel/heap.c:15-33,70-124` is a block allocator
  with free-list splitting; `kernel/heap.c:133-164` validates blocks, returns them to the
  free list, and coalesces adjacent blocks; `kmalloc(0)` returns `NULL`. *(The old "replace
  the linear heap" note was stale.)*
- [~] 2.5 ELF loads at `p_vaddr` (`kernel/elf.c:102-143`) and `process_destroy` frees user
  pages (`kernel/process.c:264-277`). **Needs re-verification:** the CR3-switch ordering
  vs. user-stack mapping in `kernel/process.c:200-216` was reported both as already-fixed
  and as inverted across two audit passes — confirm directly before changing it.
- [ ] 2.6 **Acceptance:** not met. `kernel/mmap.c` is incomplete **and unwired** —
  `mmap_init()` is never called, `SYS_MMAP`/`SYS_MUNMAP` are absent from the table
  (`kernel/syscall.c:254-277`), regions are global not per-process, file-backed maps never
  read the file, and `munmap` lacks VMA-ownership checks. `kernel/isr.c:58-93` halts on
  page fault rather than demonstrating independent recovery. See CP-3/CP-4.

## Phase 3 — Scheduler, Processes, Syscalls

- [~] 3.1 Pre-emption works (`kernel/scheduler.c:110-117` on slice expiry) but
  `scheduler_wake_sleepers` is still an **empty stub** (no real sleep queue).
- [~] 3.2 `fork`/`wait`/`exit`/zombie-reap are real and boot-verified. Gaps:
  `execve` spawns-and-returns a PID instead of replacing the image
  (`kernel/syscall.c:221-252`), and `kernel/process.c:566-571` still returns `-ENOSYS` for
  a path the plan expects to work.
- [~] 3.3 Syscall layer is real (`kernel/syscall.c`, `kernel/syscall64.c`) but **not
  pointer-safe**: `kernel/syscall.c:34-45,80-120,164-165` direct-`memcpy`s user buffers
  after a bare range check (faults under the SMEP we enabled), and `user/libc.c:54-65`
  truncates 64-bit pointers to `int`. No `futex`. Full ~40-syscall coverage not met. See CP-4.
- [ ] 3.4 `kernel/init.c:6-13` `init_start()` — the path that would `execve("/sbin/init")` —
  is **never called**. `user/init.c:9` issues a raw `_syscall(25, "/bin/shell", …)`, but no
  `/sbin/init` or `/bin/shell` exists in ramfs. See CP-2.
- [x] 3.5 `user/hello.c` runs as a userspace ELF in its own ring-3 address space
  (boot-spawned, exit 0). Phase 2.5 adds an on-demand launcher for the sibling ELFs.
- [ ] 3.6 **Acceptance:** not met and currently **impossible** — 100 simultaneous forks
  exceed the 8 static directory slots (CP-3). No sleepers-vs-CPU test, no userspace init.

## Phase 4 — Drivers (storage, display, net, input)

- [~] 4.1 virtio-blk transport is incomplete: legacy-MMIO-only probe (CP-1), so no device
  is found. `drivers/ahci.c:63-64` and `drivers/nvme.c:7-8` are lie-success stubs
  (`return 0`, no probe/DMA) and must be deleted per the "delete, don't hide" rule.
- [ ] 4.2 virtio-net: `net/net.c:4 net_current_driver` is `NULL`; `net/net.c:27` poll is
  empty. No rx/tx virtqueues, no MAC-from-config-space, no IRQ integration.
- [ ] 4.3 virtio-gpu is an empty stub (`drivers/virtio_gpu.c:7-9`). No 2D mode, no scanout,
  no resource flush. `drivers/framebuffer.c:138` still writes straight to the BAR.
- [~] 4.4 `fb_wait_vsync` is a **real bounded port poll** (`drivers/framebuffer.c:127-132`),
  not the timer stub the old plan claimed. Still missing: any backbuffer, damage rects,
  double/triple buffering. `fb_fill()` writes straight to the LFB (`:109-113`).
- [~] 4.5 PS/2 keyboard + mouse are real and fill event queues
  (`drivers/keyboard.c`, `drivers/mouse.c`), but **nothing in userspace consumes them** —
  events die in the `shell_dummy` kernel thread (`kernel/init_phase8.c:180`). No SPSC ring
  and no per-frame drain exists (there is no frame).
- [~] 4.6 `rtc_get_time` is a **real CMOS reader** (`drivers/rtc.c:15-36`) — the fixed
  `2026-01-01` is gone *(old note stale)*. `kernel/acpi.c:23-80` is a real RSDP/RSDT/MADT
  parser with working `acpi_shutdown` (`:82-89`) / `acpi_reboot` (`:91-97`), but
  **`acpi_init()` is never called**, so even that is dormant. No battery source at all
  (`grep -i battery` → 0 hits). The old `wm.c:…` simulated-status citations are stale —
  `wm.c` does not exist.
- [~] 4.7 Safe half done: `Makefile:148,151,154` already use `-smp 1`. Dead AP code still
  present: `kernel/smp.c:31-35 ap_main()` prints, `sti()`, then `while(1) hlt()`;
  `smp_init()` is never called; `cpu_info_t` (`kernel/smp.h:8-12`) has no per-CPU
  run-queue/CR3/TSS/stack. Recommendation (single-CPU-first) is being followed.
- [ ] 4.8 **Acceptance:** not met — no self-test reads virtio-blk sectors, no ARP/ping, no
  cursor-move test, no 60 Hz flip test. Note: with CP-1 unfixed the virtio-based tests
  cannot even run.

## Phase 5 — Filesystem & VFS

- [ ] 5.1 VFS: `vfs_resolve_path` walks one level only; no dentry/inode caches, no
  per-process fd table, no mount table. `ramfs_finddir` always returns `NULL`.
- [ ] 5.2 **No real read-write FS.** ext2 has the sector-boundary bug (`fs/ext2.c:39-40`)
  and assumes 1 sector/block regardless of superblock; no `write`/`truncate`/`mkdir`/
  `unlink`/bitmap allocation. **Delete the fake ext4 mount** — `fs/ext4.c` is 19 lines that
  hard-code `block_size=4096; inode_size=256; has_extents=true; has_journal=true` (`:11-12`)
  and print `"Magic check passed"` (`:15`) without reading a byte. Blocked on CP-1.
- [ ] 5.3 FAT16 sub-dir traversal + `dir_buf` leak unresolved or FAT not dropped;
  `devfs_finddir` allocates a node per lookup (leak). `/dev` is not yet a real device FS.
- [ ] 5.4 procfs: `procfs_init` is print-and-return-`NULL`; no `/proc/<pid>/status|mem|fd`.
- [ ] 5.5 No per-inode rwlocks; global FS locking.
- [ ] 5.6 **Acceptance:** not met and unreachable until CP-1 gives a block device.

## Phase 6 — Network Stack (real)

- [ ] 6.1 `arp_resolve` always returns `-1`; no aging table, reply handling, or pending-packet
  queue.
- [ ] 6.2 `net/ip.c:33 ip_send` sends `pkt + sizeof(ip_hdr_t)` — **drops the header** and
  mis-sizes. No checksum validation, reassembly, or routing table.
- [ ] 6.3 ICMP echo handler rewrites the header in memory but never transmits.
- [ ] 6.4 No real UDP socket layer.
- [ ] 6.5 No TCP state machine (no SYN/SYN-ACK, seq/ack, retransmit, RTT, windowing, FIN).
  Hardest item in the project.
- [ ] 6.6 `net/dhcp.c:6` is print-only with hard-coded MAC/address state; `net/dns.c:13`
  is hard-coded. No lease renewal, no real resolver.
- [ ] 6.7 **Acceptance:** not met. Blocked on CP-1/4.2 (no driver, no ARP, no IP send).

## Phase 7 — GUI: Windows 11-style Compositing Desktop

> **⚠ GREENFIELD — `gui/` does not exist.** There is no `wm.c`, `wm2.c`, `compositor.c`,
> `surface.c`, `anim.c`, `scene.c`, `paint.c`, `input.c`, or `font.c`. `tests/test_stub.c:1-9`
> includes `../gui/*.h` and **cannot compile**. The substrate is BGA linear-FB with no
> backbuffer (`drivers/framebuffer.c`) + 8×8 fbcon text. The old per-line bug list cited
> files that were never in the tree. Treat this phase as a from-scratch build, in this order.

- [ ] 7.0 **Foundations** (new, must precede 7.1): backbuffer + damage rects on
  `framebuffer.c`; a real virtio-gpu or retained BGA path; one compositor owning surfaces,
  z-order, and the frame loop.
- [ ] 7.1 Compositor correctness: per-surface damage accumulated during the frame and
  unioned at frame end; `surface_fill` adds a damage rect (never `full_damage=1`);
  `anim_invalidate_surface` implemented for real; per-rect single copy with no 3 MB/frame
  full-backbuffer copy; `serial_printf` out of the frame path.
- [ ] 7.2 Real Mica: per-frame downscaled backdrop blur + noise + luminosity tint, cached
  per window, invalidated on move.
- [ ] 7.3 Win11 visual spec: real rounded-rect raster, centered taskbar, start menu, system
  tray with a **real RTC clock** (`drivers/rtc.c` already works — just needs a consumer),
  snap layouts, virtual desktops, Q16.16 fixed-point dt, 60 fps budget.
- [ ] 7.4 Fonts: replace the 8×8 bitmap (`drivers/font8x8_basic.h:23`, used by
  `drivers/fbcon.c:5,29`). `lib/stb_truetype.h` is vendored but included by no translation
  unit; `tools/fontbake/main.c:38-71` is a real `stbtt_GetCodepointSDF` atlas baker that is
  **not in the Makefile and never runs**. Hook it up, then Unicode layout.
- [ ] 7.5 Input routing: compositor dispatches mouse/keyboard to focused **userspace**
  windows via shared-memory surfaces + event queues (Wayland-lite over shm + unix sockets).
  `kernel/shm.c` exists but is dormant/buggy.
- [ ] 7.6 **Acceptance (perf):** 60 fps drag with 10 windows; taskbar clock tracks RTC;
  battery shows real ACPI status or hides; memory stable over 1 h with kmalloc accounting.

## Phase 8 — Userspace, Shell, Apps, Tooling

> **Reality check: 3 of the "apps" are real ring-3 ELFs** — `hello.c`, `forkdemo.c`,
> `stacktrip.c` (`Makefile:46`); `user/crt0.asm` is correctly `[bits 64]`.
> `user/terminal.c` (63 KB), `browser.c` (56 KB), `settings.c` (44 KB),
> `file_explorer.c` (20 KB) `#include "../gui/wm.h"` (missing) and are **excluded from
> every build list** (`Makefile:84-91` are empty) — ~180 KB of design reference, not work.
> `notepad` and `calculator` do not exist at all.

- [~] 8.1 libc: syscall wrapper (`user/libc.c:10-25`), string suite (`:92-191`), `snprintf`
  (`:193-258`) are real. Missing: **`malloc`/`free` entirely** (0 hits), no `stdio`/`FILE`,
  no `printf` (only `printf_simple` at `:293`), no futex/pthread (0 hits in `kernel/`).
- [ ] 8.2 `/bin/desktop` does not exist. `user/shell.c` is a text-mode line editor that is
  **not in the build** and has literal TODO stubs for `pwd` (`:80`), `cd` (`:85`), `ls`
  (`:94`), `cat` (`:99`). The live shell is the `shell_dummy` kernel thread. See CP-2.
- [ ] 8.3 Apps: **0 of 5 real.** See the reality check above. Rebuild each as a ring-3 ELF
  against the real libc once it has `malloc`.
- [ ] 8.4 QA harness: live one is **`tests/qa/run_qa.sh`** (`Makefile:160`), not
  `tools/qa/run_qa.sh` (a stale duplicate referenced by nothing). It has **no assertions**
  and cannot fail (CP-5). `tests/qa/bench.md` is 5× `"TBD"`. `tests/test_stub.c` cannot
  compile and its `wm2_run()` (`:61`) blocks forever. No CI.
- [ ] 8.5 **Acceptance:** not met — no desktop, no login, no app launch, no persistence.
  Blocked on CP-1 (no block device ⇒ nothing survives reboot) and CP-2.

## Phase 9 — Stretch (in order)

- [ ] 9.1 SMP: dead AP code remains (`kernel/smp.c:31-35`), `smp_init()` never called, no
  per-CPU run queues / TLB shootdown. `-smp 1` is set, so the safe half is done. Partial
  groundwork: `kernel/timer.c:12` `per_cpu_ticks`.
- [ ] 9.2 ext4 + journaling: `fs/ext4.c` is 19 lines of print statements pretending to
  mount. Either implement the superblock read or delete it and its `gpt_ext4_mount.c`
  call path.
- [ ] 9.3 AHCI/NVMe: `drivers/ahci.c:63-64` and `drivers/nvme.c:7-8` are lie-success
  stubs — no BAR probe, no command list, no DMA. Delete now (Directive 0) or implement.
- [~] 9.4 USB xHCI: **best Phase 9 item.** `drivers/xhci.c` (887 lines) is genuinely real
  — HCRST, command ring + CRCR, EnableSlot, AddressDevice, `GET_DESCRIPTOR`,
  `usb_hid_parse_cfgdesc`, SET_CONFIGURATION, ConfigureEndpoint, EP1-IN IOC ring, ISR.
  Gaps: QEMU IDs hard-coded (`drivers/usb.c:8-9`, re-matched `xhci.c:228`) and no
  `-device qemu-xhci` in `make run`, so the live log reads `[XHCI] no controller found`;
  also unfixed EP1 `wMaxPacketSize` overflow into the fixed 16-byte `xhci_inbuf`
  (`xhci.c:791` → `:728`).
- [ ] 9.5 Audio AC97: `drivers/ac97.c:14-17 ac97_play` discards `samples`,
  `num_samples`, `sample_rate` and just sets `playing = true` — the "playing=true" fake is
  verbatim still present. `ac97_init` discards its args; `ac97_beep` is `kprintf` +
  `timer_sleep`. Zero call sites.
- [~] 9.6 Power management: `kernel/acpi.c` has a real RSDP/RSDT/MADT parser and working
  shutdown/reboot, but **no S1–S4**: `pm1a_control_block` is declared (`kernel/acpi.h:49`)
  and never touched, and `acpi_init()` is never called.

---

## Execution Protocol

1. Work strictly phase-ordered; each acceptance gate is hard. No GUI polish before Phase 5 passes.
   *Exception authorised this round:* CP-1…CP-5 first, because they are cross-phase blockers
   that no single phase owns.
2. After every phase: update `plan.md` to reflect *completed* work, delete obsolete stub notes, make CI green.
3. Commit checkpoint per phase with tags: `p1-64bit`, `p2-mem`, `p3-sched`, `p4-drivers`, `p5-fs`, `p6-net`, `p7-gui`, `p8-userspace`.
4. If a phase is infeasible, **stop and report the blocker** — never fabricate an acceptance test.
5. `memory/stage*.md` are design notes — fold anything still valid into real docs; delete the rest.

## First Three Tasks (superseded — kept for history)

Original seeds, now resolved: Task A (UEFI 64-bit) → split into 1.1 + 1.6; Task B (pmm/paging)
→ **done** (2.1, 2.3-partial, plus 2.4 kmalloc now verified done); Task C (context switch)
→ **done** (1.4 verified 64-bit with `isr64.asm` wired). The live starting point is
**True Critical Path** above.
