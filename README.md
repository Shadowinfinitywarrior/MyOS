# MyOS - Kernel From Scratch

Complete x86 kernel build from absolute scratch.

## Structure
myos/
├── boot/ mbr.asm, stage2.asm
├── kernel/ kernel_entry.asm, kernel.c, kernel.h
├── drivers/ screen.h, screen.c
├── include/ types.h, system.h
├── scripts/ linker.ld
├── build_toolchain.sh
└── Makefile

## Build
1. Build toolchain:
   chmod +x build_toolchain.sh
   ./build_toolchain.sh
   export PATH="$HOME/opt/cross/bin:$PATH"

2. Build OS:
   make all

3. Run:
   make run

Requires: nasm, qemu-system-i386
