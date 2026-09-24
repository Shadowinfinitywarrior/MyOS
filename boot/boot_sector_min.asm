org 0x7c00
bits 16
cli
xor ax,ax
mov ds,ax
mov es,ax
mov ss,ax
mov sp,0x7c00
sti
mov [0x7c00+510-1],dl   ; store drive

; Load stage2 from LBA 1 to 0x8000
mov word [0x7e00+2],1
mov byte [0x7e00],16
mov dword [0x7e00+4],0x8000
mov dword [0x7e00+8],1
mov dword [0x7e00+12],0
mov dl,[0x7c00+510-1]
mov si,0x7e00
mov ah,0x42
int 0x13
jc hang

jmp 0x0000:0x8000

hang:
cli
jmp hang

gdt:
dq 0
dw 0xFFFF
dw 0
db 0
db 0x9A
db 0xCF
db 0
dw 0xFFFF
dw 0
db 0
db 0x92
db 0xCF
db 0
gdt_desc:
dw $ - gdt -1
dd gdt

times 510-($-$$) db 0
dw 0xAA55
