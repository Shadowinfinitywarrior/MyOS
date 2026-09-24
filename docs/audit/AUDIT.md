# MyOS — Phase 0 System Audit

Date: 2026-09-24
Status: Current state of the repository after a full eight-subsystem audit.

This document consolidates the audit of the entire codebase. It is the factual
baseline for the commercial-grade transformation effort. Line references are
against the repository as of this date.

---

## 1. Executive Summary

MyOS is a real, from-scratch x86_64 kernel that **builds clean** (`-Wall
-Wextra -Werror`, host gcc, `-nostdlib -nostdinc -m64`) and **boots reliably
under QEMU** (BIOS SeaBIOS → stage2 → stage3 → long mode → kernel → headless
serial shell). ~29.5 kLOC of C/asm across ~240 files.

What genuinely works today:

- BIOS boot chain with real E820 memory-map handoff to the kernel
  (`boot/stage3.asm:68-88` → `kernel/kernel.c:63-74` → `kernel/pmm.c:22-85`)
- 4-level paging with identity map + higher-half alias
  (`kernel/paging.c:31-77`)
- Bitmap physical memory allocator + page-granular heap with split/coalesce
  (`kernel/pmm.c`, `kernel/heap.c:52-146`)
- Cooperative kernel-thread scheduler, context switch (FPU state saved),
  IRQ-driven APIC timer
- Real PS/2 keyboard and mouse drivers, real Bochs BGA framebuffer + text
  console, real CMOS RTC
- Substantial driver/FS/net code (much of it dormant or approximated — see §7)

What is NOT present or NOT functional (the gap vs. the project docs):

- **The entire `gui/` subsystem described by `docs/current-gui-architecture.md`
  and `docs/INTERFACE.md` does not exist in the code.** No compositor, no
  window manager (`wm.c`/`wm2.c`), no surfaces, no fonts, no input pump, no
  apps. The kernel boots to a text shell (`shell_dummy`). The docs are
  aspirational; the graphify-out/ graph was derived from those docs, not the
  code. `kernel.c:100` logs "Desktop init complete" even though no desktop runs.
- **No user mode is ever entered.** `process_create_user` sets
  `context.rip = 0` (`process.c:192`), `enter_usermode` is never called, TSS
  `rsp0` and the SYSCALL kernel stack are never installed. Ring 3 exists only
  on paper. User ELF loading is 32-bit-only and would panic if scheduled.
- **No working syscall path exists end-to-end.** Userspace is written against
  `int 0x80` (DPL0 gate → #GP), the kernel's only real entry is SYSCALL/SYSRET,
  and the dispatch passes `regs->rcx` (the user return RIP) as syscall arg 2.
- **The network stack writes reversed byte order** (no htons/ntohs anywhere),
  has no receive demux, and its virtio ring self-destructs after ~16 buffers.
- **SMP is dormant and would corrupt instantly if enabled.** `smp_init` is
  never called; the AP trampoline never sets PAE/EFER.LME; there is no per-CPU
  anything (run queue, current, CR3, TSS, IDT, LAPIC programming, TLB IPIs).

Bottom line: this is a dependable single-CPU, ring-0, kernel-thread OS with a
real memory/driver/FS foundation and an aspirational GUI/process/net layer.

---

## 2. Build & Boot Verification (done by this audit)

- `make clean && make all` → success, zero warnings/errors.
- QEMU boot (1 CPU, 1 GiB, virtio-net, BGA): reaches serial shell in <2 s.
  Boot log shows correct E820 parse (`mmap_ptr=0x5000 cnt=6`), PAGING init,
  ISR/PIC/IDT, SYSCALL setup, APIC timer, PHASE8 init, shell spawn.
- `make run-usb` (xhci + usb-kbd/usb-mouse) and UEFI/OVMF paths are NOT yet
  verified.

---

## 3. Critical Findings (top issues, all subsystems)

### 3.1 Memory management

| # | Sev | Issue | Loc | Fix |
|---|-----|-------|-----|-----|
| M1 | CRIT | Heap lives at VA==PA `0x4000000` inside the allocatable E820 region and is never reserved in the PMM → PMM can hand out the heap's own frames (page tables/DMA clobber heap, heap clobbers page tables). **FIXED in this session** (`pmm_reserve_range`, `heap_init`, plus boot-stack reservation). | heap.c:28-30, pmm.c:50-53 | reserve range at init ✔ |
| M2 | CRIT | PMM and paging updates are unsynchronized; `active_dir` is a single global. `pmm_alloc_page` is test-then-set across non-atomic ops → two CPUs can allocate the same frame. Latent (APs never started) but fatal once SMP is wired. | pmm.c:87-124, paging.c:20-22, 286-291 | spinlock (+ IRQ-save) around allocator; per-CPU address-space state |
| M3 | CRIT | Heap spinlock is not IRQ-safe. Timer ISR frees allocations (`timer_tick_process → kfree`, live via TCP retransmit timers) while task context may hold `heap_lock` → deadlock with IF=0. | heap.c:34-37, 61, 121; timer.c:117-130 | IRQ-safe lock (cli + flags save) |
| M4 | HIGH | No NX, no W^X, no SMEP/SMAP. `EFER.NXE` never set; `elf_load` maps every segment `P|USER|W` (`elf.c:28-31`). | syscall64.c:37, elf.c:28 | set NXE, honor p_flags, CR4.SMEP/SMAP |
| M5 | HIGH | Null page (phys 0-2 MiB) is supervisor-mapped in *every* address space → kernel NULL deref silently touches BIOS memory instead of faulting. | paging.c:57 | leave PDE[0] unmapped |
| M6 | HIGH | `paging_get_directory()` always returns the kernel dir (`paging.c:205-207`) → any mmap/elf path that switches and "restores" ends up in the wrong CR3. | paging.c:205-207, mmap.c:44/61/81/87, elf.c:21/40 | return active_dir |
| M7 | HIGH | Fork clones PTEs verbatim (shared writable frames, no refcount/CoW) while `process_destroy` frees the exiting stack frames → child use-after-free; exits leak PT/ELF pages. | paging.c:272-275, process.c:252-267 | frame refcounts or CoW + full destroy |
| M8 | HIGH | `copy_from_user` checks only `src < 0xC0000000` which is meaningless here (kernel is identity-mapped at 0x100000, below the boundary). No fault-safe copies. | syscall.c:17-28 | real access_ok + fault-tolerant copy helpers |
| M9 | MED | mmap/shm dormant bugs: global (not per-process) region list, MAP_FIXED without overlap checks, uint32 overflow, shm leaks pages, detach never unmaps, refcount underflow. | mmap.c:14/37-40/83-91, shm.c:35-38/60-74 | per-process VMA tree, range validation, refcounted segments |
| M10 | MED | `physical_directory[8]` cap → max 7 cloned address spaces, slots never freed; `paging_map` kprintf per call. | paging.c:20, 211-213, 92 | dynamic slot allocator |

### 3.2 Process / scheduler / concurrency

| # | Sev | Issue | Loc | Fix |
|---|-----|-------|-----|-----|
| P1 | CRIT | Timer ISR can context-switch **before** `apic_eoi()`; the preempted task's EOI never runs until it is rescheduled → LAPIC IS bit stays set, periodic timer stalls. **FIXED in this session.** | timer.c:21-24 | EOI first ✔ |
| P2 | CRIT | User processes scheduled with `context.rip = 0` → `jmp 0` → #PF → halt. Execve is therefore a panic, not a feature. | process.c:192, context_switch.asm:74 | ring-3 trampoline / disable user spawn until real |
| P3 | CRIT | `process_fork` returns a child whose `context` is all zeros and `memcpy`'s from `current->trap_frame` which is NULL for kernel threads. | process.c:359-425 | init child context; guard NULL frame |
| P4 | HIGH | Ready queue never fully unlinks dispatched processes → double-reference / starvation; a sleeping "current" can be re-added while running. | scheduler.c:80-98, process.c:318-322 | full dequeue + re-insert discipline |
| P5 | HIGH | Zombies never reaped; `process_wait/process_kill/process_exec` declared but undefined; SIGKILL kills the *caller*, not the target. | process.h:131-133, signal.c:12-34 | implement wait/kill/exec; fix signal_send target |
| P6 | MED | `scheduler_wake_sleepers` scans all 256 entries per tick with uint32 wrap (~49.7 d at 1 kHz). | scheduler.c:110-118 | sorted sleep queue on 64-bit tick |
| P7 | MED | SMP: trampoline never sets CR4.PAE/EFER.LME; no per-CPU GDT/TSS/IDT/stack/runqueue/CR3; global `current_process`/`active_dir`; no TLB shootdown/IPI reschedule. Never called (safe only by omission). | smp.c, smp_trampoline.asm:21-43 | strict single-CPU until per-CPU infra exists (per plan.md §4.7) |
| P8 | MED | Context-switch `popfq` (IF=1) before `rsp` switch → reentrant scheduler window. | context_switch.asm:64-70 | disable IF across restore |
| P9 | MED | `timer_list` mutated from ISR and task context with no cli; `heap_lock` used from both (see M3). | timer.c:92-130 | serialize timer ops |
| P10 | LOW | `apic_get_id()` dereferences NULL `apic_base` in PIT mode (fixed by guarding on `use_apic` in this session). | timer.c:19/29/64 | ✔ |

### 3.3 Syscall / ELF / ABI

| # | Sev | Issue | Loc | Fix |
|---|-----|-------|-----|-----|
| S1 | CRIT | No int 0x80 DPL3 gate and no 0x80 handler; userspace (never run) targets int 0x80, kernel hosts SYSCALL. No interoperable path. | idt.c:47, user/crt0.asm:49 | one ABI + gate |
| S2 | CRIT | SYSCALL arg 2 is the user's return RIP: dispatch passes `rbx, rcx, rdx, rsi, rdi` but hardware overwrote rcx. Every ≥2-arg syscall (read/write/open) is broken. | context_switch.asm:107/135, syscall.c:249-255 | pass `rbx, rdx, rsi, rdi, r10` |
| S3 | CRIT | SYSCALL kernel stack (`gs:0` = `syscall_cpu[0]`) never initialized; TSS `rsp0` is a fixed placeholder and never updated per process. | syscall64.c:17-21, tss.c:37, process.c:16 | install per-process kernel stack |
| S4 | CRIT | SYSCALL restores only rax/rcx/r11/rsp → destroys all caller-saved registers vs. the standard convention. | context_switch.asm:150-155 | reload from frame |
| S5 | CRIT | User pointers dereferenced directly in ring 0 (sys_read/sys_write store/load through untrusted `buf_ptr`; raw pointer into VFS). A user program gets kernel read/write. | syscall.c:17-28, 60-96 | copy_from/to_user everywhere |
| S6 | HIGH | ELF loader: no bounds checks on `e_phoff/e_phentsize/e_phnum`, no `p_offset+p_filesz ≤ size`, no range check on `p_vaddr+p_memsz`, OOM maps phys 0, all segments W+X, ELF32-only. | elf.c:18-42 | full validation + NX + atomic rollback |
| S7 | HIGH | No SMEP/SMAP/NXE; kernel identity-mapped under 4 GiB user space. | — | CR4/EFER enable (later phase) |
| S8 | MED | 15 of 256 syscall slots wired; WAIT/EXEC/KILL/BRK/MMAP/MUNMAP/GETCWD/CHDIR/MKDIR/UNLINK numbered but unimplemented; socket/select/pipe dead code. | syscall.c:214-237 | define formal ABI; implement incrementally |
| S9 | MED | `sys_read`/`sys_write` re-resolve `/dev/console` per call → ~8 KiB leak per I/O syscall; `process_free_fd` doesn't free node. | syscall.c:59/86, process.c:437-440 | cache; drop ref |

### 3.4 VFS / filesystem / storage (agent verification; condensed)

- VFS root is ramfs + devfs; no mount table, single-level path resolution.
- ext2/ext4/fat16 partial; ext4's "magic check" does not read a superblock
  (`fs/ext4.c:12-13`); procfs is a print-and-return-NULL stub; devfs allocates
  a fresh node per lookup (leak); `ramfs_finddir` doesn't search.
- Storage: virtio-blk/net exist but probe MMIO `0xD0000000` while QEMU is told
  `-device virtio-*-pci` → PCI transport never found → **virtio is dead in
  every provided run target**; tsymmetric with `-netdev user`.
- `ata/ide/nvme/ahci` read/write are lie-success stubs (`ahci.c:63-64`,
  `nvme.c:7-9`) — any FS on them silently reads zeros; GPT/ext4 mount path is
  dead (depends on `ata_devices[]` that is never populated).
- No block-device abstraction, no DMA helpers (virt→phys), no PAT/uncached
  MMIO (PAGE_NOCACHE silently dropped by `paging.c:147-150`).
- No PCI_COMMAND programming (no bus-master/decode enable) anywhere; relies on
  QEMU defaults. Real hardware would see no DMA, no device IRQs (IOAPIC never
  initialized; interrupts flow PIC→LINT0 ExtINT; spurious IRQ7/15 unhandled).

### 3.5 Drivers / interrupts / USB

- `irq.c` is an empty stub (callers get nothing). Shared IRQs unsupported
  (one handler slot per vector, `isr.c:15-17`).
- **xHCI EP1-IN buffer overflow:** device `wMaxPacketSize` used as TRB length
  into a fixed 16-byte `xhci_inbuf` → malicious/full-speed device corrupts
  kernel statics (`xhci.c:402/728/791`). Must clamp.
- Virtqueue free-list corruption (`virtio.c:51-54`) + single-page ring that
  overflows for larger queues + unbounded completion spins
  (`virtio_blk.c:43/67`) + TX ring never reaped (`virtio_net`).
- APIC timer uncalibrated (initial count = `frequency`, timer.c:57-58);
  PIT keeps firing IRQ0 in APIC mode → ~2× tick.
- Keyboard LED command written to wrong port (0x64 instead of 0x60,
  `keyboard.c:350-353`).
- `serial.c:26` / `screen.c:21` / `framebuffer.c:127-130` / `acpi.c:94` /
  `apic.c:77` unbounded inb/outb polls = machine hang on stuck hardware.
- ACPI: RSDT length underflow, single-page RSDT map, MADT `length==0` infinite
  loop; `acpi_init` never called. RTC real but no UIP sync/century.
- `vga_gfx.c:212-215` OOB read on `font_8x8[uc-32]` for unprintable chars.

### 3.6 Networking

- **No byte-order conversion anywhere** (`htons/ntohs/htonl` absent) → ARP
  dispatch compares `0x0608` vs `0x0806` (self-traffic misframes); checksums
  computed over wrong layout.
- No receive demux: `ip_recv` passes ANY frame up; `tcp_recv`/`udp_recv` treat
  the next frame as theirs; ICMP echo handler computes a reply but never sends
  and is never called; frag handling none.
- Stack overflows: `udp_send` allows 1472 → 1514 B frame into `frame[1500]`;
  2048 B virtio frame copied into `frame[1500]`/`pkt[1500]`; DHCP/DNS option
  walks unbounded (DNS `pos += rdlen` OOB; DHCP ≤255 B overrun).
- TCP is a non-functional approximation (no state machine/LISTEN/FIN/RST/
  window/RTT/cwnd; SYN seq never consumed; RX data never ACKed; one-shot
  retransmit timer holds a caller-owned `tcp_sock_t*` → use-after-free).
- Socket layer is a disconnected stub; not in the syscall table; fd numbers
  2000+i collide with nothing but are unusable via read/write.
- virtio_net: single static rx buffer re-posted without settling; TX posts
  caller stack frames to DMA; ring never reclaimed (see §3.5).
- `my_ip`/`my_mac` hardcoded and duplicated across 4+ files; DHCP/DNS are real
  sequences but dead code (zero call sites).

### 3.7 GUI / input / graphics

- `gui/` does not exist (see §1). New-ground architecture, not refactor.
- Display: real BGA LFB + fbcon text; `virtio_gpu` empty stub (unused);
  `fbterm_draw()` empty; no double buffering, no flush, no clipping.
- Input: real PS/2 keyboard/mouse + USB HID rings exist and fill; **mouse
  events are read by nothing**; keyboard events go to `shell_dummy` debug
  prints. No event pump to any GUI.
- Fonts: fixed 8×8 bitmap; stb_truetype vendored twice but unused; fontbake
  tool not wired into the build; no image/FS font files.
- Apps (`user/settings.c`, `terminal.c`, `file_explorer.c`, `browser.c`)
  include missing `gui/*.h` and call absent `wm_*` APIs; nothing in `user/` is
  compiled (empty SRCS lists in Makefile).

### 3.8 Build / tooling / QA / admin

- CFLAGS missing hardening (`-fstack-protector`, `-fpie`) — deliberate for a
  freestanding kernel, but to be revisited with userspace separately.
- Linker: identity-linked at `0x100000`, single RWE LOAD segment, GNU_STACK
  RWE; user linker is `elf32-i386` at 0x08048000 (32-bit userspace vs 64-bit
  kernel — incompatible).
- `opt/cross` contains only i686 binutils (no x86_64 gcc); build uses host
  gcc. `build_toolchain.sh` targets i686 too. `kernel_stub.asm` referenced by
  Makefile but does not exist (latent, never triggered).
- `lib/divmod.c` implements `__udivdi3/__umoddi3` on 32-bit halves — wrong math
  on -m64 builds.
- `lib/string.c` `strcpy/strcat` unbounded; `stdarg.h` va_list incompatible
  with x86_64 SysV; `assert.h` no-op; `include/stddef.h size_t` conflicts with
  `types.h`.
- Tests: `tests/test_stub.c` includes `../gui/*.h` → cannot compile; `run_qa.sh`
  greps for logs that the current boot never emits (e.g. `[COMP]`, `fps=`) →
  QA can never pass; `bench.md` TBD; no CI; no .github.
- Versioning: only `kernel/kernel.h` `"0.1.0"`; no LICENSE/third-party registry
  (stb_truetype vendored without headers); no SOFTWARE-BOM.
- TODO=38, FIXME=48, stub=238, "not implemented"=10 occurrences across sources.

---

## 4. What Is Genuinely Reusable (do not throw away)

- E820 handoff and PMM bitmap (correct math; needs a lock + reservations ✔).
- 4-level paging framework (needs locks, NX, refcounts, current-dir fix).
- Heap with split/coalesce (needs IRQ-safe lock; kfree alignment validation).
- context_switch.asm 64-bit callee-saved+FPU save (fix popfq window).
- isr64.asm frame layout matches `registers_t`; IRETQ ring-0 semantics correct.
- init_phase8 structured boot + serial logging discipline.
- Real PS/2 keyboard/mouse, BGA framebuffer, CMOS RTC, 16550 serial.
- xHCI structure is genuinely substantial (rings, EP0, HID) — needs the
  MaxPacketSize clamp and short-transfer handling.
- Drivers' config-space PCI access, virtio mmio queue skeleton.
- Header structs in net/ are correct; checksum algorithms correct; DHCP DORA
  and DNS query/resolve logic real.
- Variable: `docs/` describe an intended architecture; `plan.md` is a good
  roadmap with accurate incomplete-work markers where it references code
  (several of its file:line cites are stale — e.g., fb_wait_vsync is real).

---

## 5. Recommended Precedence (aligned to the master directive phases)

| Phase | Focus | Primary items from this audit | Validation |
|---|---|---|---|
| P0 | Audit (this doc) | ✔ done | build+boot clean |
| P1 | Kernel stabilization | M2/M3/M4/M5/M6, P1/P2/P3/P4/P8/P9, S8 partial | QEMU boot + regressions |
| P2 | ABI freeze | S1-S4: single syscall ABI, arg convention, stacks, reg save/restore | userspace coverage |
| P3 | Security | M7/M8, S5/S6/S7, NX/W^X/SMEP/SMAP, guard pages, ASLR | exploit tests |
| P4 | Devices | xHCI clamp, virtio ring reclaim + PCI transport, PCI_COMMAND, IOAPIC, timer calibration, bounded polls | real-ish HW matrix |
| P5 | Userspace | libc, dynamic linker, /init, shell as userspace ELF, coreutils | boot-to-ELF tests |
| P6 | Graphics | Build gui/ from zero per INTERFACE.md (rect→surface→blit→compositor→wm→input→font) | 60 fps compositor test |
| P7 | Networking | endianness, demux, validation, real TCP, sockets wired to table | loopback + host tests |
| P8 | Reliability | crash dumps, tracing, fuzzing, CI, regression suite | automated QEMU CI |
| P9 | Release | installer, ISO/UEFI verification, packages, signing, docs | hardware matrix |

---

## 6. Session Progress Log

- 2026-09-24 FIXED M1 (reserve heap + boot stack frames in PMM) and P1
  (APIC EOI before scheduler tick) + timer PIT-mode APIC-id guard.
  Build: clean (-Werror). Boot: verified to shell under QEMU.