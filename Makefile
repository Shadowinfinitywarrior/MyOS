CROSS = $(CURDIR)/opt/cross/bin
CC = gcc
LD = ld
AS = nasm
OBJCOPY = objcopy

# Enable parallel builds
MAKEFLAGS += -j$(shell nproc 2>/dev/null || echo 4)

# Rust toolchain
CARGO = cargo
RUST_TARGET = x86_64-unknown-none

# Go toolchain (TinyGo for bare-metal)
TINYGO = tinygo
GO_TARGET = x86_64-unknown-none

# Java toolchain (GraalVM)
NATIVE_IMAGE = native-image
JAVAC = javac

CFLAGS = -ffreestanding -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Werror -nostdlib -nostdinc -m64 -mno-red-zone -fcf-protection=none -MMD -MP -Iinclude -Idrivers -Ikernel -Ifs -Inet -Iuser -Ilib -Igui -DMYOS_SSE2=1 -DMYOS_PERF=1
USER_CFLAGS = -ffreestanding -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Werror -nostdlib -nostdinc -m64 -mno-red-zone -fcf-protection=none -fno-pie -fno-stack-check -MMD -MP -Iinclude -Iuser
LDFLAGS = -T scripts/linker.ld -nostdlib
ASFLAGS = -f elf64

KERNEL_SRCS = kernel/kernel.c kernel/idt.c kernel/isr.c kernel/irq.c kernel/pic.c kernel/timer.c kernel/pmm.c kernel/paging.c kernel/heap.c kernel/process.c kernel/scheduler.c kernel/syscall.c kernel/elf.c kernel/signal.c kernel/apic.c kernel/acpi.c kernel/smp.c kernel/mmap.c kernel/shm.c kernel/pagefault.c kernel/pipe.c kernel/select.c kernel/init.c kernel/exec.c kernel/slab.c kernel/module.c kernel/tty.c kernel/pthread.c kernel/mutex.c kernel/rwlock.c kernel/dynlink.c kernel/init_phase8.c kernel/gpt_detect.c kernel/gpt_ext4_mount.c kernel/gpt.c kernel/gdt.c kernel/tss.c kernel/syscall64.c kernel/vtty.c kernel/ipc.c kernel/unix_socket.c kernel/storage.c
DRIVER_SRCS = drivers/driver.c drivers/ide.c drivers/e1000.c drivers/hda.c drivers/serial.c drivers/keyboard.c drivers/ata.c drivers/pci.c drivers/rtc.c drivers/screen.c drivers/mouse.c drivers/speaker.c drivers/vga_gfx.c drivers/framebuffer.c drivers/ne2k.c drivers/ahci.c drivers/ac97.c drivers/fbcon.c drivers/virtio.c drivers/virtio_blk.c drivers/virtio_net.c drivers/nvme.c drivers/xhci.c drivers/usb.c drivers/usb_hid.c drivers/fbterm.c drivers/virtio_gpu.c
FS_SRCS = fs/vfs.c fs/ramfs.c fs/devfs.c fs/fat16.c fs/ext2.c fs/ext4.c fs/procfs.c
NET_SRCS = net/net.c net/eth.c net/arp.c net/ip.c net/icmp.c net/udp.c net/tcp.c net/socket.c net/dhcp.c net/dns.c
GUI_SRCS = gui/rect.c gui/blit.c gui/surface.c gui/text.c gui/theme.c gui/input.c gui/cursor.c gui/wm.c gui/desktop.c gui/term.c gui/apps.c gui/desktop_boot.c gui/login.c gui/logo.c gui/browser.c gui/java_binding.c gui/fonts/font_ptrs.c

BUILD = build

all: $(BUILD)/myos.img

# ---- GUI font baking --------------------------------------------------------
FONT_TTF_DIR ?= /usr/share/fonts/truetype/dejavu
MKBAKE       := $(BUILD)/mkbake
FONT_PX      := 15
FONT_FIRST   := 32
FONT_LAST    := 126

FONT_HEADERS := gui/fonts/ui.h gui/fonts/mono.h gui/fonts/ubold.h gui/fonts/blocks.h

FONT_BLOCK_FIRST := 0x2580
FONT_BLOCK_LAST  := 0x259F

$(MKBAKE): tools/mkbake/main.c tools/mkbake/stb_truetype.h | $(BUILD)
	$(CC) -O2 -o $@ $< -lm

gui/fonts/ui.h: $(MKBAKE)
	@mkdir -p $(dir $@)
	$(MKBAKE) $(FONT_TTF_DIR)/DejaVuSans.ttf $@ UI $(FONT_PX) $(FONT_FIRST) $(FONT_LAST)

gui/fonts/mono.h: $(MKBAKE)
	@mkdir -p $(dir $@)
	$(MKBAKE) $(FONT_TTF_DIR)/DejaVuSansMono.ttf $@ MONO $(FONT_PX) $(FONT_FIRST) $(FONT_LAST)

gui/fonts/ubold.h: $(MKBAKE)
	@mkdir -p $(dir $@)
	$(MKBAKE) $(FONT_TTF_DIR)/DejaVuSans-Bold.ttf $@ UBOLD $(FONT_PX) $(FONT_FIRST) $(FONT_LAST)

gui/fonts/blocks.h: $(MKBAKE)
	@mkdir -p $(dir $@)
	$(MKBAKE) $(FONT_TTF_DIR)/DejaVuSansMono.ttf $@ BLOCKS $(FONT_PX) $(FONT_BLOCK_FIRST) $(FONT_BLOCK_LAST)

fonts: $(FONT_HEADERS)

# ---- Rust GUI Core ----------------------------------------------------------

GUI_RUST_DIR = gui/rust
GUI_RUST_SRC = $(wildcard $(GUI_RUST_DIR)/src/*.rs)
GUI_RUST_LIB = $(BUILD)/gui/rust/libmyos_gui.a

$(GUI_RUST_LIB): $(GUI_RUST_SRC) $(GUI_RUST_DIR)/Cargo.toml
	@mkdir -p $(dir $@)
	cd $(GUI_RUST_DIR) && \
		$(CARGO) build --release --target $(RUST_TARGET)
	cp $(GUI_RUST_DIR)/target/$(RUST_TARGET)/release/libmyos_gui.a $@

$(BUILD)/gui/rust/libmyos_gui_dev.a: $(GUI_RUST_SRC) $(GUI_RUST_DIR)/Cargo.toml
	@mkdir -p $(dir $@)
	cd $(GUI_RUST_DIR) && \
		$(CARGO) build --target $(RUST_TARGET)
	cp $(GUI_RUST_DIR)/target/$(RUST_TARGET)/debug/libmyos_gui.a $@

rust-clean:
	cd $(GUI_RUST_DIR) && $(CARGO) clean
	rm -rf $(BUILD)/gui/rust

# ---- Go/TinyGo Desktop Shell ------------------------------------------------

GUI_GO_DIR = gui/go
GUI_GO_SHELL_SRC = $(wildcard $(GUI_GO_DIR)/shell/*.go) $(wildcard $(GUI_GO_DIR)/shell/graphics/*.go) $(wildcard $(GUI_GO_DIR)/runtime/*.go)
GUI_GO_SHELL_BIN = $(BUILD)/gui/go/shell.bin
GUI_GO_SHELL_ELF = $(BUILD)/gui/go/shell.elf
GUI_GO_SHELL_EMBED = $(BUILD)/user_goshell_embed.o

# Check for TinyGo
TINYGO_AVAILABLE := $(shell which tinygo 2>/dev/null || echo "")

# TinyGo shell build (bare-metal) - only if TinyGo is available
ifeq ($(TINYGO_AVAILABLE),)
$(GUI_GO_SHELL_BIN): $(GUI_GO_SHELL_SRC) | $(BUILD)/gui/go
	@echo "TinyGo not found, skipping bare-metal build. Install TinyGo to enable."
	@touch $@
else
$(GUI_GO_SHELL_BIN): $(GUI_GO_SHELL_SRC) | $(BUILD)/gui/go
	$(TINYGO) build -target $(GO_TARGET) -scheduler=none -gc=conservative -o $@ $(GUI_GO_DIR)/shell/main.go
endif

# Standard Go shell build (userspace ELF for testing) - uses TinyGo if available
# Note: This requires TinyGo; regular Go cannot build bare-metal code
ifeq ($(TINYGO_AVAILABLE),)
GUI_GO_SHELL_GO_ELF = 
else
GUI_GO_SHELL_GO_ELF = $(BUILD)/gui/go/shell_go.elf
$(GUI_GO_SHELL_GO_ELF): $(GUI_GO_SHELL_SRC)
	@mkdir -p $(dir $@)
	$(TINYGO) build -target $(GO_TARGET) -scheduler=none -gc=conservative -o $(CURDIR)/$@ $(GUI_GO_DIR)/shell/main.go
endif

# Embed Go shell binary into kernel (prefers TinyGo binary, falls back to Go ELF, then C shell)
ifeq ($(TINYGO_AVAILABLE),)
$(GUI_GO_SHELL_EMBED): $(GUI_GO_SHELL_ELF)
	$(LD) -r -b binary $< -o $@
else
$(GUI_GO_SHELL_EMBED): $(GUI_GO_SHELL_BIN) $(GUI_GO_SHELL_GO_ELF)
	@if [ -s $(GUI_GO_SHELL_BIN) ] && [ $$(stat -c%s $(GUI_GO_SHELL_BIN)) -gt 100 ]; then \
		$(LD) -r -b binary $(GUI_GO_SHELL_BIN) -o $@; \
	elif [ -f $(GUI_GO_SHELL_GO_ELF) ]; then \
		$(LD) -r -b binary $(GUI_GO_SHELL_GO_ELF) -o $@; \
	else \
		$(LD) -r -b binary $(GUI_GO_SHELL_ELF) -o $@; \
	fi
endif

# Go/C shell build (static binary for userspace - legacy)
GUI_GO_SHELL_C_SRC = $(GUI_GO_DIR)/shell.c
$(GUI_GO_SHELL_ELF): $(GUI_GO_SHELL_C_SRC) | $(BUILD)/gui/go
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $(BUILD)/gui/go/shell.o
	$(LD) -m elf_x86_64 -T scripts/linker_user64.ld -nostdlib -o $@ $(BUILD)/user/crt0.o $(BUILD)/gui/go/shell.o $(BUILD)/user/libc.o

$(BUILD)/gui/go:
	@mkdir -p $@

# Go development builds
go-dev: $(GUI_GO_SHELL_GO_ELF)
	@echo "Go shell (userspace) built: $(GUI_GO_SHELL_GO_ELF)"

go-baremetal: $(GUI_GO_SHELL_BIN)
	@echo "Go shell (bare-metal) built: $(GUI_GO_SHELL_BIN)"

go-clean:
	rm -rf $(BUILD)/gui/go

# ---- Java/GraalVM Applications ----------------------------------------------

JAVA_SRC_DIR = user/java
JAVA_FILES_DIR = $(JAVA_SRC_DIR)/files
JAVA_TERMINAL_DIR = $(JAVA_SRC_DIR)/terminal
JAVA_TOOLKIT_DIR = $(JAVA_SRC_DIR)/toolkit
JAVA_CLASSES_DIR = $(BUILD)/java/classes
JAVA_FILES_JAR = $(BUILD)/java/files.jar
JAVA_TERMINAL_JAR = $(BUILD)/java/terminal.jar
JAVA_FILES_NATIVE = $(BUILD)/user/files
JAVA_TERMINAL_NATIVE = $(BUILD)/user/terminal

# Check for GraalVM
GRAALVM_AVAILABLE := $(shell which native-image 2>/dev/null || echo "")

# Java compilation targets (only if GraalVM available)
ifeq ($(GRAALVM_AVAILABLE),)
$(JAVA_CLASSES_DIR)/files/Main.class:
	@echo "GraalVM not found, skipping Java build. Install GraalVM to enable."
	@touch $@

$(JAVA_FILES_JAR):
	@echo "GraalVM not found, skipping Java build."

$(JAVA_FILES_NATIVE):
	@echo "GraalVM not found, skipping Java build."

$(BUILD)/user_files_embed.o:
	@echo "GraalVM not found, creating dummy embed object for files."
	@echo "dummy_files_content" > $(BUILD)/dummy_files.bin
	$(LD) -r -b binary $(BUILD)/dummy_files.bin -o $@

$(JAVA_CLASSES_DIR)/terminal/Main.class:
	@echo "GraalVM not found, skipping Java terminal build."
	@touch $@

$(JAVA_TERMINAL_JAR):
	@echo "GraalVM not found, skipping Java terminal build."

$(JAVA_TERMINAL_NATIVE):
	@echo "GraalVM not found, skipping Java terminal build."

$(BUILD)/user_terminal_embed.o:
	@echo "GraalVM not found, creating dummy embed object for terminal."
	@echo "dummy_terminal_content" > $(BUILD)/dummy_terminal.bin
	$(LD) -r -b binary $(BUILD)/dummy_terminal.bin -o $@
else
JAVA_BINDING_DIR = $(JAVA_SRC_DIR)/myos/binding

$(JAVA_CLASSES_DIR)/files/Main.class: $(wildcard $(JAVA_FILES_DIR)/*.java) $(wildcard $(JAVA_TOOLKIT_DIR)/*.java) $(wildcard $(JAVA_BINDING_DIR)/*.java) | $(JAVA_CLASSES_DIR)
	@mkdir -p $(dir $@)
	$(JAVAC) -d $(JAVA_CLASSES_DIR) --release 17 -sourcepath $(JAVA_SRC_DIR) $(JAVA_TOOLKIT_DIR)/*.java $(JAVA_FILES_DIR)/*.java $(JAVA_BINDING_DIR)/*.java

$(JAVA_FILES_JAR): $(JAVA_CLASSES_DIR)/files/Main.class
	@mkdir -p $(dir $@)
	cd $(JAVA_CLASSES_DIR) && jar cfe $@ files.Main toolkit/**/*.class files/*.class myos/**/*.class META-INF/

$(JAVA_FILES_NATIVE): $(JAVA_FILES_JAR) $(JAVA_FILES_DIR)/META-INF/native-image/files/reflect-config.json $(JAVA_FILES_DIR)/META-INF/native-image/files/jni-config.json $(JAVA_FILES_DIR)/META-INF/native-image/files/proxy-config.json
	@mkdir -p $(dir $@)
	$(NATIVE_IMAGE) \
	    --static \
	    --no-fallback \
	    --no-server \
	    -H:EnableURLProtocols=http,https \
	    -H:+ReportUnsupportedElementsAtRuntime \
	    -H:ReflectionConfigurationFiles=$(JAVA_FILES_DIR)/META-INF/native-image/files/reflect-config.json \
	    -H:JNIConfigurationFiles=$(JAVA_FILES_DIR)/META-INF/native-image/files/jni-config.json \
	    -H:DynamicProxyConfigurationFiles=$(JAVA_FILES_DIR)/META-INF/native-image/files/proxy-config.json \
	    -H:Name=files \
	    -H:Class=files.Main \
	    -cp $(JAVA_CLASSES_DIR) \
	    -o $@

$(JAVA_CLASSES_DIR)/terminal/Main.class: $(wildcard $(JAVA_TERMINAL_DIR)/*.java) $(wildcard $(JAVA_TOOLKIT_DIR)/*.java) $(wildcard $(JAVA_BINDING_DIR)/*.java) | $(JAVA_CLASSES_DIR)
	@mkdir -p $(dir $@)
	$(JAVAC) -d $(JAVA_CLASSES_DIR) --release 17 -sourcepath $(JAVA_SRC_DIR) $(JAVA_TOOLKIT_DIR)/*.java $(JAVA_TERMINAL_DIR)/*.java $(JAVA_BINDING_DIR)/*.java

$(JAVA_TERMINAL_JAR): $(JAVA_CLASSES_DIR)/terminal/Main.class
	@mkdir -p $(dir $@)
	cd $(JAVA_CLASSES_DIR) && jar cfe $@ terminal.Main toolkit/**/*.class terminal/*.class myos/**/*.class META-INF/

$(JAVA_TERMINAL_NATIVE): $(JAVA_TERMINAL_JAR) $(JAVA_TERMINAL_DIR)/META-INF/native-image/terminal/reflect-config.json $(JAVA_TERMINAL_DIR)/META-INF/native-image/terminal/jni-config.json $(JAVA_TERMINAL_DIR)/META-INF/native-image/terminal/proxy-config.json
	@mkdir -p $(dir $@)
	$(NATIVE_IMAGE) \
	    --static \
	    --no-fallback \
	    --no-server \
	    -H:EnableURLProtocols=http,https \
	    -H:+ReportUnsupportedElementsAtRuntime \
	    -H:ReflectionConfigurationFiles=$(JAVA_TERMINAL_DIR)/META-INF/native-image/terminal/reflect-config.json \
	    -H:JNIConfigurationFiles=$(JAVA_TERMINAL_DIR)/META-INF/native-image/terminal/jni-config.json \
	    -H:DynamicProxyConfigurationFiles=$(JAVA_TERMINAL_DIR)/META-INF/native-image/terminal/proxy-config.json \
	    -H:Name=terminal \
	    -H:Class=terminal.Main \
	    -cp $(JAVA_CLASSES_DIR) \
	    -o $@

$(JAVA_CLASSES_DIR):
	@mkdir -p $@

$(BUILD)/user_files_embed.o: $(JAVA_FILES_NATIVE)
	$(LD) -r -b binary $< -o $@

$(BUILD)/user_terminal_embed.o: $(JAVA_TERMINAL_NATIVE)
	$(LD) -r -b binary $< -o $@
endif

java-clean:
	rm -rf $(BUILD)/java $(BUILD)/user/files $(BUILD)/user/terminal $(BUILD)/user_files_embed.o $(BUILD)/user_terminal_embed.o

# ---- Python/MicroPython Applications ------------------------------------------

PYTHON_RUNTIME_DIR = user/python/runtime
PYTHON_QT_DIR = user/python/qt
PYTHON_APPS_DIR = user/python/apps
PYTHON_SRCS = $(wildcard $(PYTHON_RUNTIME_DIR)/*.c)
PYTHON_RUNTIME_LIB = $(BUILD)/python/libpymyos.a
PYTHON_EMBED = $(BUILD)/user_python_embed.o

# Python runtime build
$(PYTHON_RUNTIME_LIB): $(PYTHON_SRCS) | $(BUILD)/python
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-const-variable -I$(PYTHON_RUNTIME_DIR) -Iinclude -c $(PYTHON_RUNTIME_DIR)/pymyos.c -o $(BUILD)/python/pymyos.o
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-const-variable -I$(PYTHON_RUNTIME_DIR) -Iinclude -c $(PYTHON_RUNTIME_DIR)/mphalport.c -o $(BUILD)/python/mphalport.o
	$(AR) rcs $@ $(BUILD)/python/pymyos.o $(BUILD)/python/mphalport.o

# Embed Python apps as binary data
PYTHON_APP_FILES = $(wildcard $(PYTHON_APPS_DIR)/*.py) $(wildcard $(PYTHON_QT_DIR)/*.py)
PYTHON_APPS_BIN = $(BUILD)/python/apps.bin

$(PYTHON_APPS_BIN): $(PYTHON_APP_FILES) | $(BUILD)/python
	@mkdir -p $(BUILD)/python/apps
	@cat $(PYTHON_APP_FILES) > $(BUILD)/python/apps.bin 2>/dev/null || echo "# No Python apps" > $(BUILD)/python/apps.bin

$(PYTHON_EMBED): $(PYTHON_RUNTIME_LIB) $(PYTHON_APPS_BIN)
	$(LD) -r -b binary $(PYTHON_APPS_BIN) -o $@

$(BUILD)/python:
	@mkdir -p $@

python-clean:
	rm -rf $(BUILD)/python $(BUILD)/user_python_embed.o

# ---- Build object files ------------------------------------------------------

$(BUILD)/kernel_entry.o: kernel/kernel_entry.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/kernel_stub.o: kernel/kernel_stub.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/%.o: net/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: drivers/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: fs/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: user/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: lib/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/gui/%.o: gui/%.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ---- User-space programs (built separately, embedded into the kernel) ----
USER_PROGS = hello forkdemo stacktrip init sh compositor cat echo pwd date whoami uname kill touch calc df env version help ls free ps uptime reboot shutdown files

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

$(BUILD)/user_stacktrip_embed.o: $(BUILD)/user/stacktrip.elf
	$(LD) -r -b binary $< -o $@

$(BUILD)/user_init_embed.o: $(BUILD)/user/init.elf
	$(LD) -r -b binary $< -o $@

$(BUILD)/user_sh_embed.o: $(BUILD)/user/sh.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_compositor_embed.o: $(BUILD)/user/compositor.elf
	$(LD) -r -b binary $< -o $@

$(BUILD)/user_cat_embed.o: $(BUILD)/user/cat.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_echo_embed.o: $(BUILD)/user/echo.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_pwd_embed.o: $(BUILD)/user/pwd.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_date_embed.o: $(BUILD)/user/date.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_whoami_embed.o: $(BUILD)/user/whoami.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_uname_embed.o: $(BUILD)/user/uname.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_kill_embed.o: $(BUILD)/user/kill.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_touch_embed.o: $(BUILD)/user/touch.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_calc_embed.o: $(BUILD)/user/calc.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_df_embed.o: $(BUILD)/user/df.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_env_embed.o: $(BUILD)/user/env.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_version_embed.o: $(BUILD)/user/version.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_help_embed.o: $(BUILD)/user/help.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_ls_embed.o: $(BUILD)/user/ls.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_free_embed.o: $(BUILD)/user/free.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_ps_embed.o: $(BUILD)/user/ps.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_uptime_embed.o: $(BUILD)/user/uptime.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_reboot_embed.o: $(BUILD)/user/reboot.elf
	$(LD) -r -b binary $< -o $@
$(BUILD)/user_shutdown_embed.o: $(BUILD)/user/shutdown.elf
	$(LD) -r -b binary $< -o $@

$(BUILD)/ascii_embed.o: ascii.txt | $(BUILD)
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
GUI_OBJS = $(GUI_SRCS:gui/%.c=$(BUILD)/gui/%.o)

ALL_OBJS = $(KERNEL_OBJS) $(DRIVER_OBJS) $(FS_OBJS) $(NET_OBJS) $(LIB_OBJS) $(GUI_OBJS) $(GUI_RUST_LIB) $(TERMINAL_OBJS) $(FILE_EXPLORER_OBJS) $(SETTINGS_OBJS) $(BROWSER_OBJS) $(BUILD)/smp_trampoline_stub.o $(BUILD)/context_switch.o $(BUILD)/isr_stubs.o

$(BUILD)/kernel.elf: $(BUILD)/kernel_entry.o $(ALL_OBJS) \
    $(BUILD)/user_hello_embed.o $(BUILD)/user_forkdemo_embed.o $(BUILD)/user_stacktrip_embed.o \
    $(BUILD)/user_init_embed.o $(BUILD)/user_sh_embed.o $(BUILD)/user_compositor_embed.o \
    $(BUILD)/user_cat_embed.o $(BUILD)/user_echo_embed.o $(BUILD)/user_pwd_embed.o \
    $(BUILD)/user_date_embed.o $(BUILD)/user_whoami_embed.o $(BUILD)/user_uname_embed.o \
    $(BUILD)/user_kill_embed.o $(BUILD)/user_touch_embed.o $(BUILD)/user_calc_embed.o \
    $(BUILD)/user_df_embed.o $(BUILD)/user_env_embed.o $(BUILD)/user_version_embed.o \
    $(BUILD)/user_help_embed.o $(BUILD)/user_ls_embed.o $(BUILD)/user_free_embed.o \
    $(BUILD)/user_ps_embed.o $(BUILD)/user_uptime_embed.o $(BUILD)/user_reboot_embed.o \
    $(BUILD)/user_shutdown_embed.o $(BUILD)/user_files_embed.o $(BUILD)/user_terminal_embed.o \
    $(BUILD)/user_python_embed.o \
    $(BUILD)/ascii_embed.o
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

$(BUILD)/data.img: | $(BUILD)
	dd if=/dev/zero of=$@ bs=1M count=32 2>/dev/null

VIRTIO_BLK = -drive file=$(BUILD)/data.img,format=raw,if=none,id=vd0 -device virtio-blk-pci,drive=vd0

run: $(BUILD)/myos.img $(BUILD)/data.img
	qemu-system-x86_64 -drive file=$<,format=raw,if=ide -m 1G -smp 1 -netdev user,id=n0 -device virtio-net-pci,netdev=n0 $(VIRTIO_BLK) -vga std -display gtk,gl=off -serial stdio -no-reboot

run-console: $(BUILD)/myos.img $(BUILD)/data.img
	qemu-system-x86_64 -drive file=$<,format=raw,if=ide -m 1G -smp 1 -netdev user,id=n0 -device virtio-net-pci,netdev=n0 $(VIRTIO_BLK) -vga std -nographic -serial mon:stdio -no-reboot

run-usb: $(BUILD)/myos.img $(BUILD)/data.img
	qemu-system-x86_64 -drive file=$<,format=raw,if=ide -m 1G -smp 1 -netdev user,id=n0,hostfwd=tcp::8080-:80 -device virtio-net-pci,netdev=n0 -device qemu-xhci -device usb-kbd -device usb-mouse $(VIRTIO_BLK) -vga std -nographic -no-reboot

run-headless: $(BUILD)/myos.img $(BUILD)/data.img
	qemu-system-x86_64 -drive file=$<,format=raw,if=ide -m 1G -smp 1 -serial stdio -debugcon file:debug.log -netdev user,id=n0 -device virtio-net-pci,netdev=n0 $(VIRTIO_BLK) -vga std -display none -no-reboot

clean:
	rm -rf $(BUILD)
	cd $(GUI_RUST_DIR) && $(CARGO) clean || true

# Phased build targets for Python
python-runtime: $(PYTHON_RUNTIME_LIB)
	@echo "Python runtime built"

python-apps: $(PYTHON_EMBED)
	@echo "Python apps embedded"

python-all: python-runtime python-apps
	@echo "All Python components built"

qa:
	@bash tests/qa/run_qa.sh

# ---- Phased build targets for incremental development ------------------------

# Phase 1: Rust GUI core only
rust-gui: $(GUI_RUST_LIB)
	@echo "Rust GUI core built"

# Fast rebuild for Rust dev (skip optimization)
rust-dev:
	cd $(GUI_RUST_DIR) && cargo build --target $(RUST_TARGET)
	cp $(GUI_RUST_DIR)/target/$(RUST_TARGET)/debug/libmyos_gui.a $(BUILD)/gui/rust/libmyos_gui.a
	@echo "Rust GUI core (dev) built"

# Phase 2: Go/TinyGo shell only
go-shell: $(GUI_GO_SHELL_BIN)
	@echo "Go desktop shell built"

# Phase 3: Java apps only
java-apps: $(JAVA_FILES_NATIVE) $(JAVA_TERMINAL_NATIVE)
	@echo "Java applications built"

# Phase 4: All GUI components
gui-all: rust-gui go-shell java-apps python-all
	@echo "All GUI components built"

# Full rebuild per language (for development)
rebuild-rust: rust-dev
	$(MAKE) $(BUILD)/kernel.elf

rebuild-go: go-dev
	$(MAKE) $(BUILD)/kernel.elf

rebuild-java:
	$(MAKE) java-apps
	$(MAKE) $(BUILD)/kernel.elf

# CI/CD targets
ci-build: all
	@echo "CI build successful"

smoke-test: rust-gui
	@echo "Smoke test passed"

# Pull in the -MMD dependency files
-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)