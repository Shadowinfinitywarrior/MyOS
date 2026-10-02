; Stage2 bootloader for MyOS
; Loaded at 0x8000 in real mode
; Loads kernel from disk to 0x100000, enables long mode, jumps to kernel

org 0x9000
bits 16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0xA000
    sti

    ; Reset the disk controller before the first read.
    mov ah, 0
    mov dl, 0x80
    int 0x13

    ; Setup DAP for 8-sector reads to fill kernel
    mov byte [0x7e20], 16
    mov byte [0x7e20+1], 0
    mov word [0x7e20+2], 8
    mov word [0x7e20+4], 0x0000 ; Offset always 0
    mov dword [0x7e20+12], 0
    ; Stage from 0x20000 instead of 0x77E0 and widen the window. The old
    ; values capped the kernel at 400 sectors (200 KB); once the image grew
    ; past that the tail (banner + embedded user ELFs) was never loaded and
    ; the boot silently degraded. 0x20000 stays clear of the boot sector
    ; (0x7C00), stage2/3 (0x8000/0x9000), the DAP (0x7e20) and the E820 map
    ; (0x5000), and 120*8 sectors = 480 KB keeps the top at 0x98000, still
    ; clear of the EBDA at 0x9FC00. Keep generous headroom here: this window
    ; silently truncating the kernel is a nasty failure mode, because the boot
    ; still succeeds and only the tail of the image is missing.
    mov ax, 0x2000 ; Starting Segment = 0x2000:0 = 0x20000
    mov ebx, 66
    mov ecx, 120   ; 120 loops * 8 sectors = 960 sectors (480 KB)
.read_loop:
    mov word [0x7e20+6], ax
    mov dword [0x7e20+8], ebx
    pusha
    mov dl, 0x80
    mov si, 0x7e20
    mov ah, 0x42
    int 0x13
    popa
    jc hang
    add ax, 0x0100 ; Increment segment by 4096 bytes
    add ebx, 8
    dec ecx
    jnz .read_loop

    jmp a20

a20:
    ; Debug entering a20
    ; Enable A20
    in al, 0x92
    or al, 2
    out 0x92, al

    ; --- E820 memory map ---
    xor ebx, ebx
    mov edx, 0x534D4150      ; 'SMAP'
    mov ecx, 24
    mov edi, 0x5000
    xor esi, esi             ; entry count
.e820_loop:
    mov eax, 0x0000E820
    int 0x15
    jc .e820_done
    cmp eax, edx
    jne .e820_done
    inc esi
    add edi, ecx
    test ebx, ebx
    jnz .e820_loop
.e820_done:
    ; store entry count at 0x4FFC
    mov dword [0x4FFC], esi
    ; store pointer at 0x4FF8 for kernel to read
    mov dword [0x4FF8], 0x5000

    ; Setup GDT for 64-bit
    lgdt [gdt_descriptor]

    ; Debug after lgdt

    ; Enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Debug before jump

    jmp 0x08:pm_start

hang:
    cli
    hlt
    jmp hang

bits 32
pm_start:
    ; Debug entering protected mode
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Enable PAE and SSE (OSFXSR=bit9, OSXMMEXCPT=bit10)
    mov eax, cr4
    or eax, 0x20
    or eax, 0x200
    or eax, 0x400
    mov cr4, eax

    ; Enable FPU (set MP=bit1, clear EM=bit2)
    mov eax, cr0
    and eax, 0xFFFFFFFB
    or eax, 0x2
    mov cr0, eax

    ; Setup page tables
    ; PML4 at 0x1000
    ; PDPT at 0x2000
    ; PD at 0x3000
    ; Zero them
    xor eax, eax
    mov edi, 0x1000
    mov ecx, 4096/4
    rep stosd
    mov edi, 0x2000
    mov ecx, 4096/4
    rep stosd
    mov edi, 0x3000
    mov ecx, 4096/4
    rep stosd

    ; Debug after zeroing

    ; PML4[0] = PDPT | present|write
    mov dword [0x1000], 0x2003
    mov dword [0x1004], 0

    ; PDPT[0] = PD | present|write
    mov dword [0x2000], 0x3003
    mov dword [0x2004], 0

    ; Map first 1GB with 2MB pages? Let's use 2MB pages for simplicity.
    ; PD entries: each entry maps 2MB
    ; For i = 0..511, map 2MB*i
    xor ecx, ecx
.map_loop:
    mov eax, ecx
    shl eax, 21           ; 2MB * i
    or eax, 0x83          ; present, writable, huge
    mov [0x3000 + ecx*8], eax
    inc ecx
    cmp ecx, 512
    jl .map_loop

    ; Debug after map loop

    ; Load CR3
    mov eax, 0x1000
    mov cr3, eax

    ; Debug after CR3

    ; Enable long mode via EFER
    mov ecx, 0xC0000080
    rdmsr
    or eax, 256          ; LME
    wrmsr

    ; Debug after LME

    ; Enable paging
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; Debug before jump

    ; Far jump to 64-bit code
    jmp 0x18:long_mode

bits 64
long_mode:
    ; Early serial debug
    ; Reload segments
    cli                          ; No IDT is loaded yet: one stray IRQ = #GP -> triple fault
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x900000
    ; Verify source loaded
    ; Copy kernel from low memory 0x20000 to 0x100000
    mov rsi, 0x20000
    mov rdi, 0x100000
    mov rcx, 0x78000     ; 480 KB, matching the read loop above
    cld
    rep movsb
    ; Verify first byte copied

    ; Call kernel at 0x100000 (call pushes 8 bytes, aligning stack for ABI)
    mov rax, 0x100000
    call rax
    
    cli
    hlt

gdt:
    dq 0
    ; 32-bit code descriptor for protected mode
    dw 0xFFFF
    dw 0
    db 0
    db 0x9A
    db 0xCF
    db 0
    ; 32-bit data descriptor
    dw 0xFFFF
    dw 0
    db 0
    db 0x92
    db 0xCF
    db 0
    ; 64-bit code descriptor
    dw 0xFFFF
    dw 0
    db 0
    db 0x9A
    db 0xAF
    db 0
    ; 64-bit data descriptor
    dw 0xFFFF
    dw 0
    db 0
    db 0x92
    db 0xCF
    db 0

gdt_descriptor:
    dw $ - gdt - 1
    dd gdt

