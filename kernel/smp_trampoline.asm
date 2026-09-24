;=====================================================
; SMP AP Trampoline Code
; Loaded at physical address 0x8000 (below 1MB)
; APs start in real mode, switch to protected mode
;=====================================================

[bits 16]
[org 0x8000]

TRAMPOLINE_START:
    cli
    cld

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    lgdt [TRAMPOLINE_GDT_PTR - TRAMPOLINE_START + 0x8000]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected_ap_entry

[bits 32]
protected_ap_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, [AP_STACK_PTR - TRAMPOLINE_START + 0x8000]

    mov eax, [AP_CR3 - TRAMPOLINE_START + 0x8000]
    mov cr3, eax

    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    lidt [AP_IDT_PTR - TRAMPOLINE_START + 0x8000]

    mov ecx, 0x1B
    rdmsr
    or eax, 0x800
    wrmsr

    mov eax, [AP_READY_FLAG - TRAMPOLINE_START + 0x8000]
    mov dword [eax], 1

    mov eax, [AP_ENTRY - TRAMPOLINE_START + 0x8000]
    jmp eax

    cli
    hlt
    jmp $ - 2

align 4
AP_STACK_PTR:   dd 0
AP_CR3:         dd 0
AP_IDT_PTR:     dq 0
AP_ENTRY:       dd 0
AP_READY_FLAG:  dd 0

TRAMPOLINE_GDT:
    dq 0
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
TRAMPOLINE_GDT_END:

TRAMPOLINE_GDT_PTR:
    dw TRAMPOLINE_GDT_END - TRAMPOLINE_GDT - 1
    dd TRAMPOLINE_GDT - TRAMPOLINE_START + 0x8000

TRAMPOLINE_END:
