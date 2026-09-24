# MyLang Toolchain Requirements

## Overview
MyLang is a minimal stack-based language for MyOS.
Compiler → Bytecode → Composer → ELF → Kernel load
Interpreter → Bytecode execution in kernel space

## Components

### 1. Compiler
Input: .myl source
Output: .myc bytecode + symbol table
Requirements:
- Lexer: identifiers, numbers, strings, keywords
- Parser: recursive descent
- Codegen: bytecode emission
- Dependencies: lib/string.c, lib/printf.c

### 2. Composer
Input: .myc bytecode + libs
Output: ELF object for kernel
Requirements:
- Bytecode → x86 codegen
- Relocation
- Link with kernel heap/runtime

### 3. Interpreter
Input: .myc bytecode
Output: execution in kernel
Requirements:
- Stack machine VM
- Builtins: print, input, malloc, syscall
- Memory safety via kernel allocator

### 4. Runtime Requirements
- Kernel heap
- VFS for .myl files
- Serial/VGA output
- Interrupt safe

## Build Targets
Host build: compile toolchain on host with gcc
Target build: cross-compile for i686-elf

## File Layout
languages/
  mylang.h
  compiler.c
  interpreter.c
  composer.c
  runtime.c
  tests/
