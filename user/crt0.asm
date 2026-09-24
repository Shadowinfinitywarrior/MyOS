; 64-bit user-space CRT0
; Entry delivered by the kernel trampoline with RSP % 16 == 8.
[bits 64]

extern main
extern exit

global _start
_start:
    sub rsp, 8                 ; align to 16 for the upcoming call
    xor rdi, rdi               ; argc = 0
    xor rsi, rsi               ; argv = NULL
    call main
    mov rdi, rax               ; exit(code)
    call exit
    hlt