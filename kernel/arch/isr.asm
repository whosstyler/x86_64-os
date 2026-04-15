bits 64

extern ISR_HANDLER
extern IRQ_HANDLER

; macro for ISR stubs that push a dummy error code
%macro ISR_NO_ERR 1
global ISR_STUB_%1
ISR_STUB_%1:
    push 0              ; dummy error code
    push %1             ; interrupt number
    jmp isr_common
%endmacro

; macro for ISR stubs where CPU pushes an error code
%macro ISR_ERR 1
global ISR_STUB_%1
ISR_STUB_%1:
    push %1             ; interrupt number (error code already on stack)
    jmp isr_common
%endmacro

; macro for IRQ stubs
%macro IRQ 2
global IRQ_STUB_%1
IRQ_STUB_%1:
    push 0              ; dummy error code
    push %2             ; IRQ number (remapped vector)
    jmp irq_common
%endmacro

; --- CPU exception stubs ---
ISR_NO_ERR 0            ; #DE divide error
ISR_NO_ERR 1            ; #DB debug
ISR_NO_ERR 2            ; NMI
ISR_NO_ERR 3            ; #BP breakpoint
ISR_NO_ERR 4            ; #OF overflow
ISR_NO_ERR 5            ; #BR bound range
ISR_NO_ERR 6            ; #UD invalid opcode
ISR_NO_ERR 7            ; #NM device not available
ISR_ERR    8            ; #DF double fault
ISR_NO_ERR 9            ; coprocessor segment overrun
ISR_ERR    10           ; #TS invalid TSS
ISR_ERR    11           ; #NP segment not present
ISR_ERR    12           ; #SS stack-segment fault
ISR_ERR    13           ; #GP general protection
ISR_ERR    14           ; #PF page fault
ISR_NO_ERR 15           ; reserved
ISR_NO_ERR 16           ; #MF x87 FP exception

; --- hardware IRQ stubs ---
IRQ 0, 0                ; PIT timer (IRQ0)
IRQ 1, 1                ; keyboard  (IRQ1)
IRQ 12, 12              ; PS/2 mouse (IRQ12)

; ---- common ISR path ----
; stack at this point: SS, RSP, RFLAGS, CS, RIP, [err_code], int_number
isr_common:
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

    mov rdi, rsp        ; arg0 = pointer to saved state
    call ISR_HANDLER

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

    add rsp, 16         ; pop error code + interrupt number
    iretq

; ---- common IRQ path ----
irq_common:
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

    mov rdi, rsp        ; arg0 = pointer to saved state
    call IRQ_HANDLER

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

    add rsp, 16         ; pop error code + irq number
    iretq

section .note.GNU-stack noalloc noexec nowrite progbits
