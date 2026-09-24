org 0x7c00
bits 16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov [drive_num], dl

    ; DAP at 0x7e00
    mov word [0x7e00 + 2], 256
    mov byte [0x7e00], 16
    mov dword [0x7e00 + 4], 0x100000
    mov dword [0x7e00 + 8], 1
    mov dword [0x7e00 + 12], 0

    mov dl, [drive_num]
    mov si, 0x7e00
    mov ah, 0x42
    int 0x13
    jc error

    ; enable A20
    in al, 0x92
    or al, 2
    out 0x92, al

    ; GDT
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:pm_start

bits 32
pm_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x900000
    jmp 0x100000

error:
    cli
.hang:
    hlt
    jmp .hang

drive_num: db 0

; GDT
gdt:
    dq 0
    ; code 0x08
    dw 0xFFFF
    dw 0
    db 0
    db 0x9A
    db 0xCF
    db 0
    ; data 0x10
    dw 0xFFFF
    dw 0
    db 0
    db 0x92
    db 0xCF
    db 0

gdt_descriptor:
    dw $ - gdt - 1
    dd gdt

times 510 - ($ - $$) db 0
dw 0xAA55
