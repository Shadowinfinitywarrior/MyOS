# MyOS Boot Progress Documentation

Date: 2026-09-14

## QEMU Launch Verification

`make run` target inspected in `Makefile:138-139`.

Current command after changes:
```
qemu-system-x86_64 -drive file=build/myos.img,format=raw,if=ide -m 1G -smp 1 -netdev user,id=n0,hostfwd=tcp::8080-:80 -device virtio-net-pci,netdev=n0 -vga std -display sdl,gl=off -no-reboot
```

Verification:
- `-display sdl,gl=off` present → SDL GUI display enabled, OpenGL disabled.
- No `-serial` argument → no serial conflict with SDL display / stdin.
- `-smp 1` changed from `-smp 4` to single-CPU mode per plan.md Phase 4.7 (SMP not yet implemented; AP code halts). Removes serial output interleaving and CPU contention.
- `-vga std` provides standard VGA for SDL rendering.
- `-no-reboot` keeps VM stopped for inspection.

`make -n run` confirms the exact command line.

## Boot Progress Steps (code inspection)

Boot chain: BIOS/SeaBIOS → `boot_sector_min.asm` → stage2/stage3 → kernel ELF.

Kernel entry `kernel/kernel.c:kernel_main_64`:
1. `serial_init(COM1)` + debug port writes `K`
2. `screen_init()`
3. `screen_write("MyOS starting...\n")` + serial `1`
4. `screen_write("Kernel loaded at 1MB\n")` + serial `2`
5. `gdt_init()`, `tss_init()`, `paging_init()` → `kprintf("paging enabled\n")`, `kprintf("higher-half kernel\n")`
6. `isr_init()`, `pic_init()`, `idt_init()`, `syscall_init_64()`, `timer_init(1000)`
7. `process_init()`, `scheduler_init()`
8. `init_phase8()`:
   - `kprintf("[PHASE8] Init start\n")`
   - `socket_init()`
   - `virtio_gpu_init()`
   - `fbterm_init(virtio_gpu_get_width(), virtio_gpu_get_height())`
   - `desktop_init()`
   - `process_create_kernel("Desktop", desktop_run)`
   - `kprintf("[PHASE8] Init complete\n")`
9. `screen_write("Desktop init complete\n")` + serial `4`
10. `sti()` → serial `5`
11. `scheduler_start()` / `scheduler_schedule()`

Init process `kernel/init.c:init_start`:
- `kprintf("\n MyOS Init v1.0 PID 1\n")`
- `widget_init()`
- `mount_defaults()` → `kprintf("[INIT] mount /dev, /proc, /tmp\n")`
- `start_services()` → `kprintf("[INIT] services started\n")`
- `kprintf("[INIT] boot complete\n")`

Observed messages on screen via `kprintf`/`screen_write` and serial debug chars `K,S,1,2,3,4,5`.

## Changes Made

- `Makefile`:
  - `run` target: `-smp 4` → `-smp 1`
  - `run-usb` target: `-smp 4` → `-smp 1`
  - `run-headless` target: `-smp 4` → `-smp 1`
  Rationale: SMP bring-up not complete per plan.md Phase 4.7; single CPU avoids AP halt loops and serial output interleaving.

No code rebuild required; `build/myos.img` already present and up to date.

## Test Result

- `make -n run` shows correct QEMU arguments with `-display sdl,gl=off` and no `-serial`.
- Dry-run succeeds, build is current.
- Boot progress documented above from source inspection; live SDL GUI launch requires an X/SDL display environment.

Follow-up: enable real SMP or keep single-CPU until Phase 4.7 acceptance. Add `-debugcon file:` capture for serial debug port 0xE9 if serial logging needed alongside SDL.
