# MyLang Toolchain

## Build
Host build:
```
gcc -std=c99 languages/compiler.c languages/interpreter.c -o mylang_tool
```

Cross build for MyOS:
```
i686-elf-gcc -ffreestanding -O2 -Iinclude languages/compiler.c -c -o build/compiler.o
```

## Usage
Source example test.myl:
```
push 42
push 8
add
print
halt
```

Compile + interpret:
```
mylang_compile("push 42\npush 8\nadd\nprint\nhalt", &prog);
mylang_interpret(&prog);   // prints 50
```

## Requirements
- Kernel heap
- VGA/serial output
- VFS for source files
- Cross-compiler i686-elf-gcc

Composer currently stub; full version will translate bytecode to x86 and link via linker.ld.
