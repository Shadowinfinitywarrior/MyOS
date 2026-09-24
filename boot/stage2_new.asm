; Stage2 bootloader for MyOS
org 0x8000
bits 16

start:
    cli
    xor ax,ax
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov sp,0x9000
    sti
    ; debug S
    mov al,'S'
    mov dx,0x3F8
    out dx,al

    ; DAP setup
    mov byte [0x7e00],16
    mov byte [0x7e00+1],0
    mov dl,0x80
    mov si,0x7e00
    mov ah,0x42

    ; read1
    mov word [0x7e00+2],63
    mov dword [0x7e00+4],0xA0000
    mov dword [0x7e00+8],2
    mov dword [0x7e00+12],0
    int 0x13
    jc hang

    ; read2
    mov word [0x7e00+2],63
    mov dword [0x7e00+4],0xA7E00
    mov dword [0x7e00+8],65
    mov dword [0x7e00+12],0
    int 0x13
    jc hang

    ; read3
    mov word [0x7e00+2],63
    mov dword [0x7e00+4],0xAFE00
    mov dword [0x7e00+8],128
    mov dword [0x7e00+12],0
    int 0x13
    jc hang

    ; read4
    mov word [0x7e00+2],4
    mov dword [0x7e00+4],0xB7A00
    mov dword [0x7e00+8],191
    mov dword [0x7e00+12],0
    int 0x13
    jc hang

    ; debug L
    mov al,'L'
    mov dx,0x3F8
    out dx,al

    ; enable A20
    in al,0x92
    or al,2
    out 0x92,al

    ; GDT
    lgdt [gdt_desc]
    mov eax,cr0
    or eax,1
    mov cr0,eax
    jmp 0x08:pm

hang:
    cli
    hlt
    jmp hang

bits 32
pm:
    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,0x90000

    ; enable PAE
    mov eax,cr4
    or eax,1<<5
    mov cr4,eax

    ; clear page tables
    xor eax,eax
    mov edi,0x1000
    mov ecx,4096/4
    rep stosd
    mov edi,0x2000
    mov ecx,4096/4
    rep stosd
    mov edi,0x3000
    mov ecx,4096/4
    rep stosd

    ; PML4[0]=PDPT
    mov dword [0x1000],0x2003
    ; PDPT[0]=PD
    mov dword [0x2000],0x3003
    ; map 2MB pages
    xor ecx,ecx
.map:
    mov eax,ecx
    shl eax,21
    or eax,0x83
    mov [0x3000+ecx*8],eax
    inc ecx
    cmp ecx,512
    jl .map

    mov eax,0x1000
    mov cr3,eax
    mov ecx,0xC0000080
    rdmsr
    or eax,1<<8
    wrmsr
    mov eax,cr0
    or eax,0x80000000
    mov cr0,eax
    jmp 0x08:long

bits 64
long:
    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov rsp,0x900000

    ; copy kernel from 0xA0000 to 0x100000
    mov rsi,0xA0000
    mov rdi,0x100000
    mov rcx,0x18400
    rep movsb

    ; debug M
    mov al,'M'
    mov dx,0x3F8
    out dx,al

    jmp 0x100000

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

times 512-($-$$) db 0
