bits 64

global CONTEXT_SWITCH

; CONTEXT_SWITCH(PTASK Old, PTASK New)
; RDI = old task (save current state here)
; RSI = new task (load state from here)
;
; TASK.Rsp is at offset 0 in the struct.

CONTEXT_SWITCH:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov [rdi], rsp
    mov rsp, [rsi]

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
