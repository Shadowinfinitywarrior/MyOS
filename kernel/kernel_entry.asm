[bits 64]
[global _start]
[extern kernel_main_64]

section .text
_start:
    cli
    mov rax, 0x900000
    mov rsp, rax
    jmp kernel_main_64

section .bss
align 16
kernel_stack_bottom:
    resb 16384
kernel_stack_top:
