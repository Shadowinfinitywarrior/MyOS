# MyOS — Transformation Roadmap

Phase plan driven by the findings in `docs/audit/AUDIT.md`. Every phase must
end with: build clean (`-Werror`) + QEMU boot regression + the phase's tests
passing, all recorded in `docs/qa/progress.log`.

Legend: ✔ done · ▶ in progress · □ planned

---

## Phase 0 — Audit & Baseline (✔)

- [x] Full eight-subsystem audit → `docs/audit/AUDIT.md`
- [x] Formalized architecture → `docs/architecture/ARCHITECTURE.md`
- [x] Baseline: build clean, QEMU boots to shell
- [x] Immediate criticals (2026-09-24): PMM heap/boot-stack reservation (M1),
      APIC EOI before scheduler tick (P1), PIT-mode APIC-id guard (P10)
- [x] Roadmap (this doc)

## Phase 1 — Kernel Stabilization

Correctness and memory/concurrency soundness of the existing ring-0 kernel.

- [✔] M1, P1, P10 (above)
- [▶] M3  heap lock IRQ-safety; M2 PMM+paging concurrency (lock, IRQ-save)
- [ ] P4  scheduler full dequeue/re-enqueue discipline
- [ ] P3  process_fork NULL-frame guard + child context init
- [ ] P8  context_switch: clear IF before RSP switch
- [ ] P9  timer_list mutation discipline
- [ ] S8  syscall gaps: implement WAIT/EXEC/KILL + formal numbers doc
- [ ] S9  sys_read/sys_write fd/node lifetime (cache resolve, free on close)
- [ ] devfs per-lookup leak; ramfs_finddir search; vfs_resolve_path walk safety
- [ ] Bounded in/out polling in serial/screen/framebuffer/acpi/apic
- [ ] `lib/divmod.c` correct mul64 on -m64; stdarg follow ABI
- [ ] Correct `irq.c` plumbing or remove the stub; shared-IRQ support
- Validated ring-0 test suite under QEMU (first passing `run_qa.sh`)

Out: kernel is provably sound at ring 0 with documented invariants.

## Phase 2 — ABI Freeze

- Single 64-bit syscall ABI; install SYSCALL kernel stack + TSS rsp0 per task
- Full register save/restore; negative-errno returns; `docs/ABI/SYSCALLS.md`
- Userspace ELF actually loads (64-bit) and returns via SYSRET to ring 3
- Fork/Cofork, exec, wait, kill against processes; scheduler preempts ring 3
- NX + W^X now (security entry point for the new ring-3 boundary)
- Userspace test image in CI: open/read/write/exec/spawn/sleep all pass

Out: a documented, frozen ABI with real ring-3 coverage in CI.

## Phase 3 — Security Hardening

- Guard pages everywhere; NULL page unmapped; kernel higher-half layout
- SMEP/SMAP; frame refcounts/CoW for fork; per-process VMA instead of globals
- Fault-tolerant copy helpers through untrusted pointers; brk/mmap ASLR
- Heap hardening (poison, freelist checks, size header validation and tests)
- malformed-input robustness harness (devices/network/ELF under fuzz)

Out: exploit/robustness test suite green; no "security = absent" rows in ARCH.

## Phase 4 — Hardware & Devices

- PCI_COMMAND enable, BAR allocation, MSI-X, MMCONFIG
- Virtio over PCI (real transport), feature negotiation, ring reclaim fix,
  IRQ-driven (de-bond from polling hacks)
- xHCI: MaxPacketSize clamp, short-transfer handling, unify USB device match
- Storage: honest block-IO layer; retire lie-success stub drivers
- IOAPIC programming from MADT; spurious/shared IRQ handling; timer calibration
- Verified boot matrix (QEMU/OVMF, -cpu variants, extra devices); real HW report

## Phase 5 — Userspace

- libc (memory, strings, stdio-via-fd, time), dynamic linker, ELF64 loader
- `/init`, shell as a real ELF, coreutils (ls/cat/echo/uptime/ps/kill)
- Man pages-grade manual pages for every syscall and every libc function
- Package/init/IPC groundwork (sockets/pipe beyond stubs)

## Phase 6 — Graphics (build `gui/` from zero)

- Foundation: rect/point/color/surface/bitmap, clipping, double-buffer + flush
- Fonts: vector raster stack (stb_truetype wired via fontbake), text layout
- Compositor: window tree, damage tracking, 60 fps test vs. BGA
- WM: focus/stack/input routing; WM protocol header (`wm2.h`)
- Apps: terminal, settings/file-explorer/browser against real APIs
- Mouse ring drained through input pump → motion/jelly tests

## Phase 7 — Networking

- htons/ntohs normalization end-to-end; receive demux; validation passes
- Real TCP state machine (LISTEN/CONNECT/CLOSE/RST/FIN, window, RTT, cwnd)
- Sockets wired into syscall table and libc `socket/bind/listen/accept`
- DHCP/DNS run from init; link-local fallback; loopback echo test in CI
- Ethernet frame fuzz harness; MTU/offload/rx-vec sustain tests

## Phase 8 — Reliability & Engineering

- Crash dumps (PMI + serial), registry/scaffold, SW vars in a version `S-VAR`
- Panic/KASAN-style fault tracking; leak detector collared to QA gates
- CI (QEMU matrix + zero-warning policy + coverage); regression suite
- Formal model of concurrency invariants reflected in code comments (header "contracts")

## Phase 9 — Release

- Installer, ISO/USB + UEFI verification, signing/checksums, versioning
  (kernel, ABI, userspace, FS format, driver model, module ABI)
- Vulnerability-disclosure + licensing (LICENSE) + third-party registry
  (stb_truetype provenance), SLSA-grade build provenance
- Hardware compatibility matrix; performance baselines (`tests/qa/bench.md`)

---

### Progress Log
- 2026-09-24: Phase 0 complete. Phase 1 batch 1 (M1, P1, P10) done + verified.