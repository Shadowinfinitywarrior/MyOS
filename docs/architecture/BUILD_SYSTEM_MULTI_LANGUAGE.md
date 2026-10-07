# Multi-Language Build System Guide

## Overview

This document describes the MyOS multi-language build system, which coordinates the compilation of **C**, **x86-64 Assembly**, **Rust**, **Go (TinyGo)**, **Java (GraalVM Native Image)**, and **Python (MicroPython)** into a single bootable operating system disk image (`myos.img`).

---

## 1. Toolchain Prerequisites & Configuration

| Language | Toolchain Component | Version / Target | Target Flag | Output Artifact |
|----------|---------------------|------------------|-------------|-----------------|
| **C / C++** | GCC (Cross or Host) | GCC 11+ / freestanding | `-m64 -ffreestanding -mno-red-zone -nostdlib` | `build/*.o` |
| **Assembly** | NASM | 2.15+ | `-f elf64` (kernel) / `-f bin` (boot) | `build/*.o`, `build/*.bin` |
| **Rust** | Rustc & Cargo | Nightly (1.75+) | `--target x86_64-unknown-none` | `build/gui/rust/libmyos_gui.a` |
| **Go** | TinyGo | 0.30+ | `-target x86_64-unknown-none -scheduler=none` | `build/gui/go/shell.bin` |
| **Java** | GraalVM & native-image | Java 17 / 22.3+ | `--static --no-fallback` | `build/user/files`, `build/user/terminal` |
| **Python** | GCC (MicroPython port) | In-tree MicroPython | `-ffreestanding -nostdlib` | `build/python/libpymyos.a` |

---

## 2. Makefile Architecture & Flow

The build dependency graph enforces clean stage sequencing:

```
                  ┌────────────────────────┐
                  │ DejaVu TTF Font Files  │
                  └───────────┬────────────┘
                              │ tools/mkbake
                  ┌───────────▼────────────┐
                  │ Baked Font C Headers   │
                  │   (gui/fonts/*.h)      │
                  └───────────┬────────────┘
                              │
     ┌────────────────────────┼────────────────────────┐
     │                        │                        │
┌────▼─────────────────┐ ┌────▼─────────────────┐ ┌────▼─────────────────┐
│ C Kernel & Drivers   │ │ Rust GUI Core        │ │ Embedded Userland    │
│ (kernel/*.c, drv/ )  │ │ (gui/rust/src/*.rs)  │ │ (24 C User ELFs)     │
└────────────┬─────────┘ └────────────┬─────────┘ └────────────┬─────────┘
             │                        │                        │
             │           ┌────────────┴───────────┐            │
             │           │ Multi-Language Apps:   │            │
             │           │ - TinyGo Shell         │            │
             │           │ - GraalVM Java Apps    │            │
             │           │ - MicroPython Apps     │            │
             │           └────────────┬───────────┘            │
             │                        │                        │
             └────────────────┬───────┴────────────────────────┘
                              │
                    ┌─────────▼──────────┐
                    │  scripts/linker.ld │
                    └─────────┬──────────┘
                              │ ld
                    ┌─────────▼──────────┐
                    │  build/kernel.elf  │
                    └─────────┬──────────┘
                              │ objcopy
                    ┌─────────▼──────────┐
                    │  build/kernel.bin  │
                    └─────────┬──────────┘
                              │ dd with boot sectors (MBR, stage2, stage3)
                    ┌─────────▼──────────┐
                    │   build/myos.img   │
                    │   (Bootable Disk)  │
                    └────────────────────┘
```

---

## 3. Toolchain Definitions in Makefile

The root `Makefile` establishes all multi-language toolchain variables at lines 1–25:

```makefile
CROSS ?= $(CURDIR)/opt/cross/bin
CC = gcc
LD = ld
AS = nasm
OBJCOPY = objcopy

# Parallel builds enabled across all available CPU cores
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

# Freestanding C compilation flags
CFLAGS = -ffreestanding -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Werror \
         -nostdlib -nostdinc -m64 -mno-red-zone -fcf-protection=none -MMD -MP \
         -Iinclude -Idrivers -Ikernel -Ifs -Inet -Iuser -Ilib -Igui \
         -DMYOS_SSE2=1 -DMYOS_PERF=1

USER_CFLAGS = -ffreestanding -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Werror \
              -nostdlib -nostdinc -m64 -mno-red-zone -fcf-protection=none -fno-pie \
              -fno-stack-check -MMD -MP -Iinclude -Iuser
LDFLAGS = -T scripts/linker.ld -nostdlib
ASFLAGS = -f elf64
```

---

## 4. Multi-Language Build Rules

### 4.1 Rust GUI Core Rules
Located at `gui/rust/`:

```makefile
GUI_RUST_DIR = gui/rust
GUI_RUST_SRC = $(wildcard $(GUI_RUST_DIR)/src/*.rs)
GUI_RUST_LIB = $(BUILD)/gui/rust/libmyos_gui.a

# Release build with link-time optimization (LTO)
$(GUI_RUST_LIB): $(GUI_RUST_SRC) $(GUI_RUST_DIR)/Cargo.toml
	@mkdir -p $(dir $@)
	cd $(GUI_RUST_DIR) && \
		$(CARGO) build --release --target $(RUST_TARGET)
	cp $(GUI_RUST_DIR)/target/$(RUST_TARGET)/release/libmyos_gui.a $@

# Fast debug build for rapid local testing
$(BUILD)/gui/rust/libmyos_gui_dev.a: $(GUI_RUST_SRC) $(GUI_RUST_DIR)/Cargo.toml
	@mkdir -p $(dir $@)
	cd $(GUI_RUST_DIR) && \
		$(CARGO) build --target $(RUST_TARGET)
	cp $(GUI_RUST_DIR)/target/$(RUST_TARGET)/debug/libmyos_gui.a $@

rust-clean:
	cd $(GUI_RUST_DIR) && $(CARGO) clean
	rm -rf $(BUILD)/gui/rust
```

### 4.2 Go / TinyGo Desktop Shell Rules
Located at `gui/go/`:

```makefile
GUI_GO_DIR = gui/go
GUI_GO_SHELL_SRC = $(wildcard $(GUI_GO_DIR)/shell/*.go) \
                   $(wildcard $(GUI_GO_DIR)/shell/graphics/*.go) \
                   $(wildcard $(GUI_GO_DIR)/runtime/*.go)
GUI_GO_SHELL_BIN = $(BUILD)/gui/go/shell.bin
GUI_GO_SHELL_ELF = $(BUILD)/gui/go/shell.elf
GUI_GO_SHELL_EMBED = $(BUILD)/user_goshell_embed.o

TINYGO_AVAILABLE := $(shell which tinygo 2>/dev/null || echo "")

ifeq ($(TINYGO_AVAILABLE),)
$(GUI_GO_SHELL_BIN): $(GUI_GO_SHELL_SRC) | $(BUILD)/gui/go
	@echo "TinyGo not found, skipping bare-metal build. Install TinyGo to enable."
	@touch $@
else
$(GUI_GO_SHELL_BIN): $(GUI_GO_SHELL_SRC) | $(BUILD)/gui/go
	$(TINYGO) build -target $(GO_TARGET) -scheduler=none -gc=conservative -o $@ $(GUI_GO_DIR)/shell/main.go
endif

# Embedding mechanism with fallback hierarchy: TinyGo binary -> Go ELF -> C shell stub
$(GUI_GO_SHELL_EMBED): $(GUI_GO_SHELL_BIN)
	@if [ -s $(GUI_GO_SHELL_BIN) ] && [ $$(stat -c%s $(GUI_GO_SHELL_BIN)) -gt 100 ]; then \
		$(LD) -r -b binary $(GUI_GO_SHELL_BIN) -o $@; \
	else \
		$(LD) -r -b binary $(GUI_GO_SHELL_ELF) -o $@; \
	fi
```

### 4.3 Java / GraalVM Native Image Rules
Located at `user/java/`:

```makefile
JAVA_SRC_DIR = user/java
JAVA_FILES_DIR = $(JAVA_SRC_DIR)/files
JAVA_TERMINAL_DIR = $(JAVA_SRC_DIR)/terminal
JAVA_TOOLKIT_DIR = $(JAVA_SRC_DIR)/toolkit
JAVA_BINDING_DIR = $(JAVA_SRC_DIR)/myos/binding
JAVA_CLASSES_DIR = $(BUILD)/java/classes
JAVA_FILES_JAR = $(BUILD)/java/files.jar
JAVA_TERMINAL_JAR = $(BUILD)/java/terminal.jar
JAVA_FILES_NATIVE = $(BUILD)/user/files
JAVA_TERMINAL_NATIVE = $(BUILD)/user/terminal

GRAALVM_AVAILABLE := $(shell which native-image 2>/dev/null || echo "")

# Native image compilation with static linking and reflection descriptors
$(JAVA_FILES_NATIVE): $(JAVA_FILES_JAR) $(JAVA_FILES_DIR)/META-INF/native-image/files/reflect-config.json
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

# Embedded objects linked into kernel RamFS
$(BUILD)/user_files_embed.o: $(JAVA_FILES_NATIVE)
	$(LD) -r -b binary $< -o $@

$(BUILD)/user_terminal_embed.o: $(JAVA_TERMINAL_NATIVE)
	$(LD) -r -b binary $< -o $@
```

### 4.4 Python / MicroPython Rules
Located at `user/python/`:

```makefile
PYTHON_RUNTIME_DIR = user/python/runtime
PYTHON_QT_DIR = user/python/qt
PYTHON_APPS_DIR = user/python/apps
PYTHON_SRCS = $(wildcard $(PYTHON_RUNTIME_DIR)/*.c)
PYTHON_RUNTIME_LIB = $(BUILD)/python/libpymyos.a
PYTHON_EMBED = $(BUILD)/user_python_embed.o

$(PYTHON_RUNTIME_LIB): $(PYTHON_SRCS) | $(BUILD)/python
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-const-variable -I$(PYTHON_RUNTIME_DIR) -Iinclude -c $(PYTHON_RUNTIME_DIR)/pymyos.c -o $(BUILD)/python/pymyos.o
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-const-variable -I$(PYTHON_RUNTIME_DIR) -Iinclude -c $(PYTHON_RUNTIME_DIR)/mphalport.c -o $(BUILD)/python/mphalport.o
	$(AR) rcs $@ $(BUILD)/python/pymyos.o $(BUILD)/python/mphalport.o

$(PYTHON_EMBED): $(PYTHON_RUNTIME_LIB) $(PYTHON_APPS_BIN)
	$(LD) -r -b binary $(PYTHON_APPS_BIN) -o $@
```

---

## 5. Kernel Linking & Binary Embedding

In `scripts/linker.ld`, the kernel is positioned at physical `0x100000` (1MB). All C objects, the Rust GUI static library (`libmyos_gui.a`), and the embedded userland binaries link together:

```makefile
ALL_OBJS = $(KERNEL_OBJS) $(DRIVER_OBJS) $(FS_OBJS) $(NET_OBJS) $(LIB_OBJS) \
           $(GUI_OBJS) $(GUI_RUST_LIB) \
           $(BUILD)/smp_trampoline_stub.o $(BUILD)/context_switch.o $(BUILD)/isr_stubs.o

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
```

Each embedded object exposes linker symbols:
- `_binary_build_user_hello_elf_start` / `_binary_build_user_hello_elf_end`
- `_binary_build_user_files_start` / `_binary_build_user_files_end`
- `_binary_build_python_apps_bin_start` / `_binary_build_python_apps_bin_end`

During `kernel/init_phase8.c`, these symbols are read to populate the in-memory RamFS (`/bin/hello`, `/bin/sh`, `/bin/files`, etc.).

---

## 6. Phased Targets for Developer Workflows

The build system provides modular targets for rapid local development:

```bash
# Phased language builds
make rust-gui        # Compiles release Rust GUI core
make rust-dev        # Compiles debug Rust GUI core
make go-shell        # Compiles Go desktop shell
make java-apps       # Compiles Java applications
make python-all      # Compiles MicroPython runtime & apps
make gui-all         # Compiles all multi-language GUI components

# Language-specific rebuilds
make rebuild-rust    # Rebuilds Rust core and re-links kernel
make rebuild-go      # Rebuilds Go shell and re-links kernel
make rebuild-java    # Rebuilds Java apps and re-links kernel

# Disk image generation & testing
make all             # Builds complete bootable myos.img
make run             # Boots in QEMU with GTK display
make run-console     # Boots in QEMU nographic mode
make run-headless    # Boots in headless mode for CI/CD
make qa              # Executes tests/qa/run_qa.sh test suite
```

---

## 7. Migration Checklist (Verified Complete)

- [x] Install Rust nightly toolchain with `x86_64-unknown-none` target
- [x] Configure TinyGo toolchain with bare-metal target profile
- [x] Configure GraalVM Native Image toolchain with reflection configs
- [x] Update Makefile with Rust build and clean rules
- [x] Update Makefile with TinyGo build and embedding rules
- [x] Update Makefile with Java javac and native-image rules
- [x] Update Makefile with MicroPython runtime and app bundling rules
- [x] Create Rust `Cargo.toml` and `.cargo/config.toml`
- [x] Create Go `go.mod`
- [x] Create Java `reflect-config.json` and `jni-config.json`
- [x] Verify Rust standalone staticlib build (`build/gui/rust/libmyos_gui.a`)
- [x] Verify TinyGo shell compilation and embedding
- [x] Verify Java compilation and native-image pipeline
- [x] Verify full multi-language kernel link into `kernel.elf`
- [x] Verify automated headless QEMU QA test suite passes (`tests/qa/run_qa.sh`)
- [x] Document complete multi-language build system
