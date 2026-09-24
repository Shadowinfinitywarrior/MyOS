CROSS = $(CURDIR)/opt/cross/bin
CC = gcc
LD = ld
AS = nasm
OBJCOPY = objcopy

CFLAGS = -ffreestanding -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Werror -nostdlib -nostdinc -m64 -mno-red-zone -fcf-protection=none -Iinclude -Idrivers -Ikernel -Ifs -Inet -Iuser -Ilib -DMYOS_SSE2=1 -DMYOS_PERF=1
USER_CFLAGS = -ffreestanding -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Werror -nostdlib -nostdinc -m64 -mno-red-zone -fcf-protection=none -fno-pie -fno-stack-check -Iinclude -Iuser
LDFLAGS = -T scripts/linker.ld -nostdlib
ASFLAGS = -f elf64

KERNEL_SRCS = kernel/kernel.c kernel/idt.c kernel/isr.c kernel/irq.c kernel/pic.c kernel/timer.c kernel/pmm.c kernel/paging.c kernel/heap.c kernel/process.c kernel/scheduler.c kernel/syscall.c kernel/elf.c kernel/signal.c kernel/apic.c kernel/acpi.c kernel/smp.c kernel/mmap.c kernel/shm.c kernel/pipe.c kernel/select.c kernel/init.c kernel/exec.c kernel/slab.c kernel/module.c kernel/tty.c kernel/pthread.c kernel/mutex.c kernel/rwlock.c kernel/dynlink.c kernel/init_phase8.c kernel/gpt_detect.c kernel/gpt_ext4_mount.c kernel/socket.c kernel/gpt.c kernel/gdt.c kernel/tss.c kernel/syscall64.c
DRIVER_SRCS = drivers/serial.c drivers/keyboard.c drivers/ata.c drivers/pci.c drivers/rtc.c drivers/screen.c drivers/mouse.c drivers/speaker.c drivers/vga_gfx.c drivers/framebuffer.c drivers/ne2k.c drivers/ahci.c drivers/ac97.c drivers/fbcon.c drivers/virtio.c drivers/virtio_blk.c drivers/virtio_net.c drivers/nvme.c drivers/xhci.c drivers/usb.c drivers/usb_hid.c drivers/fbterm.c drivers/virtio_gpu.c
FS_SRCS = fs/vfs.c fs/ramfs.c fs/devfs.c fs/fat16.c fs/ext2.c fs/ext4.c fs/procfs.c
NET_SRCS = net/net.c net/eth.c net/arp.c net/ip.c net/icmp.c net/udp.c net/tcp.c net/dhcp.c net/dns.c

BUILD = build

all: $(BUILD)/myos.img

$(BUILD)/kernel_entry.o: kernel/kernel_entry.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/kernel_stub.o: kernel/kernel_stub.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: drivers/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: fs/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: net/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: user/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: lib/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# ---- User-space programs (built separately, embedded into the kernel) ----
USER_PROGS = hello forkdemo

$(BUILD)/user/crt0.o: user/crt0.asm | $(BUILD)
	@mkdir -p $(dir $@)
	$(AS) -f elf64 $< -o $@

$(BUILD)/user/%.o: user/%.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD)/user/%.elf: $(BUILD)/user/crt0.o $(BUILD)/user/%.o $(BUILD)/user/libc.o
	$(LD) -m elf_x86_64 -T scripts/linker_user64.ld -nostdlib -o $@ $^

$(BUILD)/user_hello_embed.o: $(BUILD)/user/hello.elf
	$(LD) -r -b binary $< -o $@

$(BUILD)/user_forkdemo_embed.o: $(BUILD)/user/forkdemo.elf
	$(LD) -r -b binary $< -o $@

$(BUILD)/smp_trampoline_stub.o: kernel/smp_trampoline_stub.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/context_switch.o: kernel/context_switch.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/isr_stubs.o: kernel/isr64.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

KERNEL_OBJS = $(KERNEL_SRCS:kernel/%.c=$(BUILD)/%.o)
DRIVER_OBJS = $(DRIVER_SRCS:drivers/%.c=$(BUILD)/%.o)
FS_OBJS = $(FS_SRCS:fs/%.c=$(BUILD)/%.o)
NET_OBJS = $(NET_SRCS:net/%.c=$(BUILD)/%.o)
LIB_SRCS = $(wildcard lib/*.c)
LIB_OBJS = $(LIB_SRCS:lib/%.c=$(BUILD)/%.o)

TERMINAL_SRCS =
FILE_EXPLORER_SRCS =
SETTINGS_SRCS =
BROWSER_SRCS =
TERMINAL_OBJS = $(TERMINAL_SRCS:user/%.c=$(BUILD)/%.o)
FILE_EXPLORER_OBJS = $(FILE_EXPLORER_SRCS:user/%.c=$(BUILD)/%.o)
SETTINGS_OBJS = $(SETTINGS_SRCS:user/%.c=$(BUILD)/%.o)
BROWSER_OBJS = $(BROWSER_SRCS:user/%.c=$(BUILD)/%.o)
ALL_OBJS = $(KERNEL_OBJS) $(DRIVER_OBJS) $(FS_OBJS) $(NET_OBJS) $(LIB_OBJS) $(TERMINAL_OBJS) $(FILE_EXPLORER_OBJS) $(SETTINGS_OBJS) $(BROWSER_OBJS) $(BUILD)/smp_trampoline_stub.o $(BUILD)/context_switch.o $(BUILD)/isr_stubs.o

$(BUILD)/kernel.elf: $(BUILD)/kernel_entry.o $(ALL_OBJS) $(BUILD)/user_hello_embed.o $(BUILD)/user_forkdemo_embed.o
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/efi/efi_main.o: boot/efi_main.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fno-ident -c $< -o $@

$(BUILD)/efi/boot.elf: $(BUILD)/efi/efi_main.o
	$(LD) -nostdlib -e efi_main -o $@ $^

$(BUILD)/efi/boot.bin: $(BUILD)/efi/boot.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/efi/boot.pe: $(BUILD)/efi/efi_main.o
	$(LD) -m x86_64pe --subsystem 10 -nostdlib -e efi_main --image-base 0x100000 --disable-dynamicbase -o $@ $<

$(BUILD)/boot_sector.bin: boot/boot_sector_min.asm | $(BUILD)
	nasm -f bin $< -o $@

$(BUILD)/stage2.bin: boot/stage2.asm | $(BUILD)
	nasm -f bin $< -o $@

$(BUILD)/stage3.bin: boot/stage3.asm | $(BUILD)
	nasm -f bin $< -o $@

$(BUILD)/myos.img: $(BUILD)/kernel.bin $(BUILD)/boot_sector.bin $(BUILD)/stage2.bin $(BUILD)/stage3.bin
	dd if=/dev/zero of=$@ bs=512 count=20480 2>/dev/null
	dd if=$(BUILD)/boot_sector.bin of=$@ bs=512 count=1 conv=notrunc 2>/dev/null
	dd if=$(BUILD)/stage2.bin of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=$(BUILD)/stage3.bin of=$@ bs=512 seek=2 conv=notrunc 2>/dev/null
	dd if=$(BUILD)/kernel.bin of=$@ bs=512 seek=66 conv=notrunc 2>/dev/null

$(BUILD)/esp.img: $(BUILD)/efi/boot.pe $(BUILD)/kernel.bin $(BUILD)/kernel.elf | $(BUILD)
	dd if=/dev/zero of=$@ bs=1M count=50 2>/dev/null
	mkfs.fat -F 32 -n ESP $@
	mmd -i $@ ::/EFI
	mmd -i $@ ::/EFI/BOOT
	mcopy -i $@ $(BUILD)/efi/boot.pe ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $@ $(BUILD)/kernel.bin ::/kernel.bin
	mcopy -i $@ $(BUILD)/kernel.elf ::/kernel.elf

$(BUILD)/uefi.img: $(BUILD)/esp.img | $(BUILD)
	dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null
	sgdisk -o $@
	sgdisk -n 1:2048:0 -t 1:EF00 $@
	dd if=$(BUILD)/esp.img of=$@ seek=2048 conv=notrunc bs=512

$(BUILD):
	mkdir -p $(BUILD)

run: $(BUILD)/myos.img
	qemu-system-x86_64 -drive file=$<,format=raw,if=ide -m 1G -smp 1 -netdev user,id=n0 -device virtio-net-pci,netdev=n0 -vga std -display gtk,gl=off -no-reboot

run-usb: $(BUILD)/myos.img
	qemu-system-x86_64 -drive file=$<,format=raw,if=ide -m 1G -smp 1 -netdev user,id=n0,hostfwd=tcp::8080-:80 -device virtio-net-pci,netdev=n0 -device qemu-xhci -device usb-kbd -device usb-mouse -vga std -nographic -no-reboot

run-headless: $(BUILD)/myos.img
	qemu-system-x86_64 -drive file=$<,format=raw,if=ide -m 1G -smp 1 -serial stdio -debugcon file:debug.log -netdev user,id=n0 -device virtio-net-pci,netdev=n0 -vga std -display none -no-reboot

clean:
	rm -rf $(BUILD)

qa:
	@bash tests/qa/run_qa.sh
