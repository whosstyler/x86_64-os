bits 64

global KERNEL_ENTRY
extern KERNEL_MAIN

section .bss
align 16
kernel_stack:
    resb 16384
kernel_stack_top:

section .text
KERNEL_ENTRY:
    ; UEFI loader is compiled with MS ABI (mingw), so first arg is in RCX.
    ; Kernel is compiled with System V ABI (gcc), so KERNEL_MAIN expects RDI.
    mov rdi, rcx

    lea rsp, [rel kernel_stack_top]
    mov rbp, rsp

    call KERNEL_MAIN

.hang:
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
