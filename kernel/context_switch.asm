;=====================================================
; Context Switching for Multitasking - 64-bit
;=====================================================

[bits 64]

global context_switch
global enter_usermode

extern syscall_dispatch

; ==========================================
; context_switch(old_ctx, new_ctx)
;   Saves callee-saved registers to old context
;   Restores from new context
;   old_ctx = pointer to cpu_context_t (can be NULL)
;   new_ctx = pointer to cpu_context_t
;   SysV AMD64: rdi = old_ctx, rsi = new_ctx
; ==========================================
context_switch:
    mov r10, rdi          ; old_ctx
    mov r11, rsi          ; new_ctx
    test r10, r10
    jz .load_new

    ; Save current context to old_ctx
    mov [r10 + 0],  r15
    mov [r10 + 8],  r14
    mov [r10 + 16], r13
    mov [r10 + 24], r12
    mov [r10 + 32], rbx
    mov [r10 + 40], rbp
    mov [r10 + 48], rdi
    mov [r10 + 56], rsi
    ; Save the stack pointer as if the call had already returned
    ; (skip the return-address slot) so that when the task resumes
    ; at rip, its function epilogue pops correctly aligned slots.
    lea rax, [rsp + 8]
    mov [r10 + 64], rax
    mov rax, [rsp]        ; return address as rip
    mov [r10 + 72], rax
    pushfq
    pop rax
    mov [r10 + 80], rax
    ; Save FPU state (512 bytes)
    lea rax, [r10 + 96]
    fxsave [rax]

.load_new:
    ; Load new context
    mov r15, [r11 + 0]
    mov r14, [r11 + 8]
    mov r13, [r11 + 16]
    mov r12, [r11 + 24]
    mov rbx, [r11 + 32]
    mov rbp, [r11 + 40]
    mov rdi, [r11 + 48]
    mov rsi, [r11 + 56]

    ; Restore FPU state
    lea rax, [r11 + 96]
    fxrstor [rax]

    ; Restore stack pointer BEFORE rflags: rflags carries IF, and re-enabling
    ; interrupts before switching stacks would let an IRQ run the new task's
    ; registers on the old task's (soon-to-be-unmapped/freed) stack.
    mov rsp, [r11 + 64]

    ; Restore rflags
    mov rax, [r11 + 80]
    push rax
    popfq

    ; Jump to saved rip
    mov rax, [r11 + 72]
    jmp rax

; ==========================================
; enter_usermode(eip, esp)
;   Switches to Ring 3 (user mode)
;   Uses IRETQ to set CS, SS to user segments
;   SysV AMD64: rdi = user RIP, rsi = user RSP
; ==========================================
enter_usermode:
    mov rax, rdi            ; user RIP
    mov rcx, rsi            ; user RSP

    ; Set user data segments (user data selector = 0x1B).
    ; NB: must not clobber rax (holds RIP for the IRETQ frame).
    mov rdx, 0x1B
    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx

    ; Build IRETQ frame on current stack:
    ; SS, RSP, RFLAGS, CS, RIP
    push qword 0x1B
    push rcx
    push qword 0x202
    push qword 0x23
    push rax
    iretq

; ==========================================
; usermode_enter_trampoline
;   Ring-0 -> Ring-3 transition used on the FIRST switch into a fresh
;   user process. Arrives via context_switch with:
;     rdi = user entry point, rsi = user RSP.
;   Builds the IRETQ frame on the current kernel stack and drops to Ring 3.
; ==========================================
global usermode_enter_trampoline
usermode_enter_trampoline:
    cli
    push qword 0x1B          ; ss
    push rsi                 ; user rsp
    push qword 0x202         ; rflags
    push qword 0x23          ; cs
    push rdi                 ; user rip
    iretq

; ==========================================
; fork_child_iret
;   Resume path for a freshly forked Ring-3 child. context_switch enters here
;   with RSP pointing at a copy of the parent's registers_t frame on the
;   child's kernel stack. Pops every GPR (child rax already 0), skips the
;   int_no/err slots, and iretq back to the user instruction following the
;   original fork() SYSCALL. Mirrors the isr_common_stub tail layout.
; ==========================================
global fork_child_iret
fork_child_iret:
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
    add rsp, 16     ; skip int_no + err_code
    iretq

; ==========================================
; sigreturn_trampoline
;   User-mode trampoline for sigreturn syscall.
;   Called with RSP pointing to saved registers_t frame.
;   Invokes SYS_SIGRETURN and restores user context.
; ==========================================
global sigreturn_trampoline
sigreturn_trampoline:
    mov rax, 101        ; SYS_SIGRETURN
    syscall
    ; Should not return
    hlt

; ==========================================
; syscall_entry64
;   Fast path for the SYSCALL instruction.
;   On entry (from Ring 3):
;     rax = syscall number, rdi/rsi/rdx/r10/r8 = args (SysV)
;     rcx = user return RIP, r11 = user RFLAGS
;     rsp  = user stack, IF cleared by FMASK
;   Builds a registers_t frame on the process kernel stack (held in the
;   absolute syscall_cpu[0] cell — this kernel does not rely on GS-based
;   addressing), runs syscall_dispatch with full SysV argument slots, then
;   restores every GPR and sysretq back to the user.
; ==========================================
global syscall_entry64
extern syscall_cpu
syscall_entry64:
    ; Stash the four values this stub clobbers into syscall_cpu[1..4]
    ; so no user register is ever lost.
    mov  [syscall_cpu + 8],  r9
    mov  [syscall_cpu + 16], rcx         ; user RIP (clobbered by SYSCALL)
    mov  [syscall_cpu + 24], r11         ; user RFLAGS (clobbered by SYSCALL)
    mov  [syscall_cpu + 32], rax         ; syscall number
    mov  r9, rsp                         ; save user rsp
    mov  rcx, [syscall_cpu]              ; kernel stack top
    sub  rcx, 0xB0                       ; room for the 176-byte registers_t frame
    mov  [rcx + 0x00], r15
    mov  [rcx + 0x08], r14
    mov  [rcx + 0x10], r13
    mov  [rcx + 0x18], r12
    mov  [rcx + 0x20], r11     ; r11 slot = user RFLAGS
    mov  [rcx + 0x28], r10     ; r10 slot = user r10 (arg4)
    mov  [rcx + 0x38], r8      ; r8 slot  = user r8  (arg5)
    mov  [rcx + 0x40], rbp
    mov  [rcx + 0x48], rdi     ; arg1
    mov  [rcx + 0x50], rsi     ; arg2
    mov  [rcx + 0x58], rdx     ; arg3
    mov  [rcx + 0x68], rbx
    mov  qword [rcx + 0x78], 0 ; int_no
    mov  qword [rcx + 0x80], 0 ; err_code
    mov  qword [rcx + 0x90], 0x23 ; cs = user code
    mov  qword [rcx + 0xA8], 0x1B ; ss = user data
    ; Fill in the trampled slots (and the explicit frame tail) from stash:
    mov  rax, [syscall_cpu + 8]
    mov  [rcx + 0x30], rax     ; r9 slot = user r9
    mov  rax, [syscall_cpu + 16]
    mov  [rcx + 0x60], rax     ; rcx slot = user RIP
    mov  [rcx + 0x88], rax     ; rip = user RIP
    mov  rax, [syscall_cpu + 24]
    mov  [rcx + 0x98], rax     ; rflags = user RFLAGS
    mov  [rcx + 0xA0], r9      ; rsp = user RSP
    mov  rax, [syscall_cpu + 32]
    mov  [rcx + 0x70], rax     ; rax slot = syscall number
    mov  rsp, rcx
    mov  rdi, rsp              ; registers_t *regs
    call syscall_dispatch

    ; Return path: result in regs->rax (rsp+0x70); restore every GPR.
    ; rcx/r11/rsp are repurposed for sysret per the hardware contract.
    mov  rax, [rsp + 0x70]     ; syscall result
    mov  rcx, [rsp + 0x88]     ; user RIP for SYSRET
    mov  r11, [rsp + 0x98]     ; user RFLAGS for SYSRET
    mov  r15, [rsp + 0x00]
    mov  r14, [rsp + 0x08]
    mov  r13, [rsp + 0x10]
    mov  r12, [rsp + 0x18]
    mov  r10, [rsp + 0x28]
    mov  r9,  [rsp + 0x30]
    mov  r8,  [rsp + 0x38]
    mov  rbp, [rsp + 0x40]
    mov  rdi, [rsp + 0x48]
    mov  rsi, [rsp + 0x50]
    mov  rdx, [rsp + 0x58]
    mov  rbx, [rsp + 0x68]
    mov  rsp, [rsp + 0xA0]     ; user RSP
    o64 sysret
