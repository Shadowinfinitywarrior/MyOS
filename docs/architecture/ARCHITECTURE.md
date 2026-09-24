# MyOS — Architecture

Status: **Draft 0.2** — reflects the audited reality plus the two Phase 1 fixes
applied on 2026-09-24. Supersedes the aspirational sections of
`docs/current_architecture_assessment.md` and `docs/current-gui-architecture.md`
where they conflict with code.

---

## 1. Design Tenets

1. Modular monolithic kernel in C, x86_64, GNU toolchain + NASM + GNU ld.
2. Retain and harden the existing boot chain (BIOS MBR→stage2→stage3 and UEFI
   entry); do not rewrite working subsystems without a written justification.
3. Subsystems communicate through explicit headers under `kernel/`, `drivers/`,
   `fs/`, `net/`, `lib/`. No uncontrolled cross-subsystem globals.
4. Architecture-specific code stays in `kernel/`/`drivers/`; generic logic in
   `fs/`, `net/`, `lib/` depends on abstracts, not registers.
5. Correctness > security > maintainability > compatibility > performance.
6. Nothing is "implemented" until it has an automated test that passes under
   QEMU; nothing is "secure" until there is a passing exploit/robustness test.

---

## 2. Boot Flow

```
Firmware (BIOS/SeaBIOS or UEFI OVMF)
  │
  ├─ BIOS: boot_sector_min.asm (MBR, loads stage2)
  │        stage2.asm → stage3.asm
  │        stage3: A20, E820→0x5000 (cnt@0x4FFC, ptr@0x4FF8), PAE, 2 MiB
  │        identity map (0x1000-0x3000), long mode, copy kernel 0x70000→0x100000
  ├─ UEFI: efi_main.c → loads kernel.bin/elf → (needs EVAL: memory-map handoff)
  │
  ▼
kernel_entry.asm            RSP=0x900000  → kernel_main_64
kernel_main_64 (kernel.c:37)
  1. zero BSS
  2. serial_init, screen_init
  3. gdt_init, tss_init            (64-bit GDT, single static TSS)
  4. read E820  → pmm_init         (bitmap allocator)
     → pmm_reserve_range(boot stack)      [FIX P1-2026-09-24]
  5. heap_init                    (page heap @ VA==PA 0x4000000, reserves its
                                    frames in PMM)              [FIX P1]
  6. paging_init                  (4-level; identity 0-1 GiB + high alias)
  7. isr_init, pic_init, idt_init (ISR stubs, PIC remap, 256 gates)
  8. syscall_init, syscall_init_64 (SYSCALL/SYSRET MSRs)
  9. timer_init(1000)             (APIC LVT timer; EOI-before-schedule [FIX])
 10. process_init, scheduler_init (PID 0 idle)
 11. init_phase8                  ramfs→devfs→virtio→sockets→input→FB→shell
 12. sti(); first schedule; idle hlt loop
```

---

## 3. Virtual Address Layout (formalized)

All regions are VA==PA unless noted (identity-mapped kernel).

| Range | Owner | Mapping | Notes |
|---|---|---|---|
| 0x0000000000001000 | stage3 | PAE 2 MiB pages | transient boot page tables (0x1000-0x3000) |
| 0x0000000000005000 | stage3 | E820 buffer | later: SMP trampoline 0x8000 |
| 0x0000000000100000 | kernel | identity text/data/BSS | `_start` at 0x100000 (`scripts/linker.ld`) |
| 0x0000000000297xxx | kernel | `__kernel_end` | PMM free-list starts here (page-aligned) |
| 0x0000000000400000 | heap | identity, 16 MiB | kmalloc region, **reserved in PMM** |
| 0x00000000008fe000 | boot | boot stack | RSP=0x900000, **reserved in PMM** |
| 0x000000000FE* | APIC | identity uncached | FEE00000 LAPIC; IOAPIC via MADT (not wired) |
| 0x00000000D0000000 | virtio | MMIO probe | dead in practice (QEMU uses PCI transport) |
| 0xFFFF800000000000 (≈pml4[511]) | kernel | higher-half alias | created by paging_init; not actually used for linking |
| 0x00000000BFFFF000 | user | user stack top | `USER_STACK_TOP` (process.h:13) — unused, no ring 3 |
| 0x0000000040000000–0x80000000 | mmap | user anon/file | dormant (`mmap.c`) |

Design note for later phases: the current design is **identity-linked**, not a
true higher-half kernel. A formal higher-half split (kernel ≥ 0xFFFF80000…,
user < 0x00000FFFFFFFFFFFFF) plus NX/SMEP/SMAP is a Phase 2/3 prerequisite for
the isolation story, and every driver that currently passes kernel VAs to DMA
must then use an explicit virt→phys translation.

---

## 4. Subsystem Map

```
                kernel_main_64
                     │
   ┌─────────┬───────┼────────┬──────────┬─────────┐
   │  arch   │  mm   │  sched │  syscall │  driver │
   │ gdt tss │ pmm   │ process│  ABI     │  pci    │
   │ idt isr │ paging│scheduler│ elf exec│  virtio │
   │ apic    │ heap  │ signal │ socket   │ xhci/usb│
   │ acpi    │ mmap  │ sync   │ pipe     │ fb/serial│
   │         │ shm   │        │          │  kbd/mouse│
   └────┬────┴───┬──┴───┬────┴───┬──────┴────┬────┘
        │        │      │        │           │
        ▼        ▼      ▼        ▼           ▼
    fs/vfs→ramfs/devfs/fat16/ext2/ext4/procfs
    net/eth→arp→ip→(icmp,udp,tcp,dhcp,dns)
    lib/printf,string,list,ring_buffer,bitmap,ipc
```

### 4.1 Public interfaces (de-facto, frozen pending ABI work)

- `kernel/pmm.h` — `pmm_init(mmap,count)`, `pmm_reserve_range(start,end)*`,
  `pmm_alloc_page`, `pmm_free_page`, `pmm_get_free_pages`, `pmm_get_total_pages`
- `kernel/paging.h` — `paging_init`, `paging_map(virt,phys,flags)`,
  `paging_unmap`, `paging_get_physical`, `paging_get_directory`,
  `paging_clone_directory`, `paging_switch_directory`
- `kernel/heap.h` — `heap_init`, `kmalloc`, `kzalloc`, `kfree`
- `kernel/scheduler.h` — `scheduler_init/start/add/remove/schedule/tick/wake_sleepers`
- `kernel/process.h` — `process_init/create_kernel/create_user/destroy/exit/
  yield/sleep/block/unblock/get_current/get_by_pid/fork/alloc_fd/free_fd/count/dump_all`
  (+ declared-but-missing: `process_exec/wait/kill`)
- `kernel/isr.h` — `isr_init/register_handler`; `kernel/pic.h` — `pic_init/clear_mask/send_eoi`
- `kernel/timer.h` — `timer_init/get_ticks/get_seconds/sleep/get_ms/get_ms64/add/del`
- `kernel/syscall.h` — syscall numbers 0-25 (see §5)
- `drivers/*.h`, `fs/vfs.h`, `net/{net,eth,arp,ip,icmp,udp,tcp,dhcp,dns}.h`

Allocation/locking context rules exist only informally today: functions that
may sleep/free/allocate/lock are not annotated. A Phase 1 deliverable is a
documented rule on `kmalloc`, interrupt-safety, and lock ownership per module.

---

## 5. Syscall ABI — Current State

Entry: `syscall`/`sysret` (LSTAR `syscall_entry64`), 64-bit.
Numbers: `include`d via `kernel/syscall.h`; **15 of 256 slots wired**, gaps at
WAIT/EXEC/KILL/BRK/MMAP/MUNMAP/GETCWD/CHDIR/MKDIR/UNLINK and all socket ops.

Current (broken) convention:
`rax=num, arg in rbx/rdx/rsi/rdi per asm comment; dispatch reads rbx,rcx,rdx,rsi,rdi`
→ **arg2 is the user return RIP (hardware rcx)**; only rax/rcx/r11 restored on
return; errors are literal `-1` (no errno); int 0x80 legacy path is unreachable
(DPL0).

Target convention (Phase 2 — ABI freeze):
`rax=num, args rdi,rsi,rdx,r10,r8,r9 (SysV-compatible)`, full register
preservation, negative-errno returns, documented per-call contracts in
`docs/ABI/SYSCALLS.md`, syscall-number versioning baked into the ABI.

---

## 6. Driver Model — Current State

- PCI: 0xCF8/0xCFC config space only; `pci_find_device(vendor,device)` literal
  matching; no MMCONFIG/MSI/MSI-X/BAR allocation; **no PCI_COMMAND enable**.
- Virtio: legacy MMIO probe at 0xD0000000 with **PCI-addressable QEMU devices
  the probe cannot see**; feature negotiation skipped; ring free-list broken;
  polling (no IRQ).
- Storage fakes to remove or retire: `ata/ide/nvme/ahci` lie-success stubs.
- USB: real xHCI skeleton; hard-coded QEMU device ID in `usb.c:8-9`;
  **MaxPacketSize→TRB-length overflow pending clamp** (security-critical).
- Input: PS/2 (real) + USB-HID feeding the same rings; mouse ring undrained.
- Display: BGA framebuffer + fbcon; virtio-gpu stub.
- Interrupt delivery: legacy PIC→LINT0 ExtINT with local APIC enabled; IOAPIC
  unprogrammed; isr.c auto-EOIs PIC vectors before dispatch; spurious IRQ7/15
  and shared IRQs unhandled.

---

## 7. Filesystem & Storage — Current State

Root = ramfs, `devfs` mounted manually; single-level `vfs_resolve_path`
(`fs/vfs.c:9-24`); no mount table/permissions/ownership/block layer. ext2 read
partial (write/mkdir present but disk-block access is stubbed behind fake
drivers → disk path is dead in QEMU today); ext4/procfs claims fake; FAT16
partial with leak; no persistence.

---

## 8. Networking — Current State

Layered headers exist (eth/arp/ip/icmp/udp/tcp/dhcp/dns) with correct structs
and checksum algorithms, but: no byte-order conversion, no receive demux, no
connection table, virtio ring reclaim broken, socket layer disconnected, DHCP
and DNS real sequences but zero call sites. Not operational end-to-end.

---

## 9. Security Posture (honest)

| Mechanism | Status |
|---|---|
| Ring 0/3 isolation | **Not in effect** (no ring-3 entry path) |
| NX | Not enabled (no EFER.NXE) |
| W^X | Not enforced (text mapped writable) |
| SMEP / SMAP | Not enabled (no CR4 writes to these bits) |
| ASLR / KASLR | Absent (identity-linked, fixed addresses) |
| Stack canaries | Absent in kernel (`-fno-stack-protector`) |
| Guard pages | Absent |
| Kernel heap hardening | Partial (magic+state checks; no poisoning, no IRQ-safe lock until P1 fix note) |
| User-pointer validation | Absent (single ineffective range check) |
| Capabilities / privilege separation | Absent |
| Secure boot / module signatures | Absent |
| Malformed-hardware resilience | Partial (some bounded waits; several unbounded) |

None of these are claimable until tested.

---

## 10. Versioning & ABI Policy

- Kernel: `KERNEL_VERSION`/`KERNEL_NAME` (`kernel/kernel.h:6-7`).
- Introduce `MAJOR.MINOR.PATCH` + separate ABI version constants for: syscall
  ABI, userspace ABI, filesystem format, driver/device model, module ABI.
- ABI freeze target: Phase 2. Until then, nothing user-facing depends on the
  current (broken) ABI — this is the cheapest moment to get it right.

---

## 11. Definition of Done (project standard)

A feature is done only when all of: implementation, error handling, security
analysis, concurrency analysis, automated test, QEMU validation, regression
test, API documentation, measured performance where relevant, and defined
failure behavior exist. See `docs/audit/AUDIT.md` §5 for the phase plan.