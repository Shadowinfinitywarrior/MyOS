# MyOS Architecture Documentation Index

Welcome to the MyOS Architecture Documentation. This directory contains detailed specifications, implementation plans, and reports for the system architecture, multi-language GUI stack, and build infrastructure.

---

## Architecture Documents Index

| Document | Description | Scope |
|----------|-------------|-------|
| [SYSTEM_ARCHITECTURE.md](SYSTEM_ARCHITECTURE.md) | **Master System Architecture Specification** | Complete overview of kernel, bootloader, memory management, process scheduling, syscalls, VFS, networking, drivers, and GUI stack |
| [MODERN_GUI_ARCHITECTURE.md](MODERN_GUI_ARCHITECTURE.md) | **Modern Multi-Language GUI Design** | Detailed design of the Rust GUI core, Go desktop shell, Java GraalVM apps, and MicroPython utilities |
| [PHASE1_PROGRESS_REPORT.md](PHASE1_PROGRESS_REPORT.md) | **Phase 1: Rust GUI Core Completion Report** | Verification of all 10 steps of the Rust GUI implementation (`libmyos_gui.a`) |
| [GUI_MIGRATION_IMPLEMENTATION_PLAN.md](GUI_MIGRATION_IMPLEMENTATION_PLAN.md) | **GUI Migration Implementation Plan** | Multi-phase roadmap, milestones, success criteria, and verification metrics |
| [GUI_MODERNIZATION_SUMMARY.md](GUI_MODERNIZATION_SUMMARY.md) | **GUI Modernization Executive Summary** | High-level architectural overview, language roles, file structures, and build commands |
| [BUILD_SYSTEM_MULTI_LANGUAGE.md](BUILD_SYSTEM_MULTI_LANGUAGE.md) | **Multi-Language Build System Guide** | Complete toolchain configuration, Makefile rules, and binary embedding architecture |
| [GUI_STACK_COMPLETION_REPORT.md](GUI_STACK_COMPLETION_REPORT.md) | **GUI Stack Completion & Evolution Report** | Technical report on MYDP display protocol, scene graph, input gestures, and multi-language convergence |

---

## Quick Navigation

- **Kernel & OS Core**: See [SYSTEM_ARCHITECTURE.md](SYSTEM_ARCHITECTURE.md)
- **GUI & Graphics Stack**: See [MODERN_GUI_ARCHITECTURE.md](MODERN_GUI_ARCHITECTURE.md)
- **Build System & Toolchains**: See [BUILD_SYSTEM_MULTI_LANGUAGE.md](BUILD_SYSTEM_MULTI_LANGUAGE.md)
- **System Call ABI Reference**: See [../ABI/SYSCALLS.md](../ABI/SYSCALLS.md)
