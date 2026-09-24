; Minimal stage2 loader for MyOS
; Loaded at 0x8000, loads stage3 to 0x9000 and jumps
org 0x8000
bits 16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x9000
    sti

    ; Load stage3 from LBA 2 to 0x9000
    mov byte [0x7e00], 16
    mov byte [0x7e00+1], 0
    mov word [0x7e00+2], 63
    mov dword [0x7e00+4], 0x9000
    mov dword [0x7e00+8], 2
    mov dword [0x7e00+12], 0
    mov dl, 0x80
    mov si, 0x7e00
    mov ah, 0x42
    int 0x13
    jc hang

    jmp 0x0000:0x9000

hang:
    cli
    hlt
    jmp hang

times 510-($-$$) db 0
dw 0xAA55
