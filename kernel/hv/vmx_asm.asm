bits 64

global ASM_VMXON
global ASM_VMCLEAR
global ASM_VMPTRLD
global ASM_VMXOFF
global ASM_VMREAD
global ASM_VMWRITE
global VMX_LAUNCH

extern VMEXIT_DISPATCH

section .text

; UINT8 ASM_VMXON(UINT64 *PhysAddr)
ASM_VMXON:
    vmxon [rdi]
    jc .vmxon_fail
    jz .vmxon_fail
    xor eax, eax
    ret
.vmxon_fail:
    mov eax, 1
    ret

; UINT8 ASM_VMCLEAR(UINT64 *PhysAddr)
ASM_VMCLEAR:
    vmclear [rdi]
    jc .vmclear_fail
    jz .vmclear_fail
    xor eax, eax
    ret
.vmclear_fail:
    mov eax, 1
    ret

; UINT8 ASM_VMPTRLD(UINT64 *PhysAddr)
ASM_VMPTRLD:
    vmptrld [rdi]
    jc .vmptrld_fail
    jz .vmptrld_fail
    xor eax, eax
    ret
.vmptrld_fail:
    mov eax, 1
    ret

; VOID ASM_VMXOFF(VOID)
ASM_VMXOFF:
    vmxoff
    ret

; UINT64 ASM_VMREAD(UINT64 Field)
ASM_VMREAD:
    xor rax, rax
    vmread rax, rdi
    ret

; VOID ASM_VMWRITE(UINT64 Field, UINT64 Value)
ASM_VMWRITE:
    vmwrite rdi, rsi
    ret

; UINT8 VMX_LAUNCH(VOID)
; saves current state, writes GUEST_RSP/RIP into VMCS, does VMLAUNCH.
; returns 0 on success (now VMX guest), 1 on failure.
VMX_LAUNCH:
    pushfq
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; write GUEST_RSP = current RSP
    mov rsi, rsp
    mov rdi, 0x681C         ; VMCS_GUEST_RSP
    vmwrite rdi, rsi

    ; write GUEST_RIP = .guest_resume
    lea rsi, [rel .guest_resume]
    mov rdi, 0x681E         ; VMCS_GUEST_RIP
    vmwrite rdi, rsi

    vmlaunch

    ; VMLAUNCH failed
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    popfq
    mov eax, 1
    ret

.guest_resume:
    ; VMLAUNCH succeeded, now running as VMX guest
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    popfq
    xor eax, eax
    ret

; VMEXIT handler entry point. HOST_RIP points here.
; CPU loaded HOST_RSP, we're in VMX root.
global VMEXIT_STUB
VMEXIT_STUB:
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call VMEXIT_DISPATCH

    test rax, rax
    jnz .vmexit_fatal

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    vmresume

    ; vmresume failed
    hlt
    jmp $-1

.vmexit_fatal:
    cli
    hlt
    jmp .vmexit_fatal

section .note.GNU-stack noalloc noexec nowrite progbits
