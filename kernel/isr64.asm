; 64-bit ISR stubs
[bits 64]

%macro ISR_NOERR 1
global isr_stub_%1
isr_stub_%1:
    push qword 0
    push qword %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERR 1
global isr_stub_%1
isr_stub_%1:
    push qword %1
    jmp isr_common_stub
%endmacro

%assign i 0
%rep 256
    %if (i == 8) || (i >=10 && i <=14) || (i ==17) || (i ==21)
        ISR_ERR i
    %else
        ISR_NOERR i
    %endif
%assign i i+1
%endrep

global isr_common_stub
extern isr_handler
isr_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov rdi, rsp
    push rdi           ; save original RSP; also aligns RSP to 16 bytes for call
    call isr_handler
    pop rsp            ; restore original RSP

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iretq

global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    dq isr_stub_%+i
%assign i i+1
%endrep
